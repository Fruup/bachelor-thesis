#include <engine/hzpch.h>

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>
#include <engine/events/Event.h>

#include <fstream>

// ------------------------------------------------------------------------

decltype(AdvancedRenderer::Vertex::Attributes) AdvancedRenderer::Vertex::Attributes = {
	// uint32_t location
	// uint32_t binding
	// Format format = Format::eUndefined
	// uint32_t offset
	vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position) },
	vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, UV) },
};

decltype(AdvancedRenderer::Vertex::Binding) AdvancedRenderer::Vertex::Binding = vk::VertexInputBindingDescription{
	0, // uint32_t binding
	sizeof(AdvancedRenderer::Vertex), // uint32_t stride
	vk::VertexInputRate::eVertex, // VertexInputRate inputRate
};

// ------------------------------------------------------------------------

template <typename T>
void DumpDataToFile(const char* filename, const std::vector<T>& data)
{
	std::ofstream file(filename, std::ios::binary | std::ios::trunc);
	file.write((char*)data.data(), sizeof(T) * data.size());
	file.close();
}

void DumpDataToFile(const char* filename, const void* data, size_t size)
{
	std::ofstream file(filename, std::ios::binary | std::ios::trunc);
	file.write((char*)data, size);
	file.close();
}

void TransitionImageLayout(vk::Image image,
						   vk::ImageLayout oldLayout,
						   vk::ImageLayout newLayout,
						   vk::AccessFlags srcAccessMask,
						   vk::AccessFlags dstAccessMask,
						   vk::PipelineStageFlags srcStageMask,
						   vk::PipelineStageFlags dstStageMask)
{
	vk::ImageMemoryBarrier barrier;
	barrier
		.setOldLayout(oldLayout)
		.setNewLayout(newLayout)
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,
													   0, /* baseMipLevel */
													   1, /* levelCount */
													   0, /* baseArrayLayer */
													   1  /* layerCount */ ));

	Vulkan.CommandBuffer.pipelineBarrier(srcStageMask,
										 dstStageMask,
										 {},
										 {},
										 {},
										 barrier);
}

// ------------------------------------------------------------------------
// PUBLIC FUNCTIONS

AdvancedRenderer::AdvancedRenderer() :
	DepthRenderPass(*this),
	CompositionRenderPass(*this),
	CameraController(Camera)
{
}

void AdvancedRenderer::Init(::Dataset* dataset)
{
	Dataset = dataset;

	DepthBuffer.Init(BilateralBuffer::Depth);
	PositionsBuffer.Init(BilateralBuffer::Color);
	NormalsBuffer.Init(BilateralBuffer::Color);

	DepthRenderPass.Init();
	CompositionRenderPass.Init();

	VertexBuffer.Create(6 * Dataset->MaxParticles * sizeof(Vertex));

	// init camera
	Camera = Camera3D(
		glm::radians(60.0f),
		Vulkan.GetAspect(),
		0.1f,
		1000.0f
	);

	CameraController.ComputeMatrices();
}

void AdvancedRenderer::Exit()
{
	delete Dataset;
	VertexBuffer.Destroy();

	CompositionRenderPass.Exit();
	DepthRenderPass.Exit();

	NormalsBuffer.Exit();
	PositionsBuffer.Exit();
	DepthBuffer.Exit();
}

void AdvancedRenderer::Update(float time)
{
	CameraController.Update(time);
}

void AdvancedRenderer::HandleEvent(Event& e)
{
	CameraController.HandleEvent(e);
}

void AdvancedRenderer::Render()
{
	// - begin command buffer
	// 
	// - transition depth buffer
	// - collect render data
	// 
	// - begin depth pass
	// - render scene
	// - end depth pass
	// 
	// - end command buffer
	// - submit command buffer
	// 
	// - transition depth buffer
	// - copy depth buffer to CPU
	// - do ray marching and fill positions and normals buffer
	// - (transition positions and normals buffer)
	// - copy positions and normals buffer to GPU
	// - (transition positions and normals buffer)
	// - copy positions and normals buffer to GPU
	// 
	// - begin command buffer
	// 
	// - begin composition pass
	// - render fullscreen quad
	// - end composition pass
	// 
	// - render ImGui pass
	// 
	// - end command buffer
	// - submit command buffer

	//   Done in Renderer:
	// begin command buffer

	vk::CommandBufferBeginInfo beginInfo;

	//DepthBuffer.PrepareGPULayoutForRendering(Vulkan.CommandBuffer);

	CollectRenderData();

	DepthBuffer.TransitionLayout(vk::ImageLayout::eDepthAttachmentOptimal,
								 vk::AccessFlagBits::eDepthStencilAttachmentRead,
								 vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests);

	DepthRenderPass.Begin();
	DrawDepthPass();
	DepthRenderPass.End();

	DepthBuffer.TransitionLayout(vk::ImageLayout::eTransferSrcOptimal,
								 vk::AccessFlagBits::eTransferRead,
								 vk::PipelineStageFlagBits::eTransfer);

	PositionsBuffer.TransitionLayout(vk::ImageLayout::eTransferDstOptimal,
									 vk::AccessFlagBits::eTransferWrite,
									 vk::PipelineStageFlagBits::eTransfer);

	NormalsBuffer.TransitionLayout(vk::ImageLayout::eTransferDstOptimal,
								   vk::AccessFlagBits::eTransferWrite,
								   vk::PipelineStageFlagBits::eTransfer);

	Vulkan.CommandBuffer.end();
	Vulkan.Submit();
	Vulkan.WaitForRenderingFinished();

	DepthBuffer.CopyFromGPU();

	RayMarch();

	PositionsBuffer.CopyToGPU();
	NormalsBuffer.CopyToGPU();

	Vulkan.CommandBuffer.reset();
	Vulkan.CommandBuffer.begin(beginInfo);

	PositionsBuffer.TransitionLayout(vk::ImageLayout::eColorAttachmentOptimal,
									 vk::AccessFlagBits::eColorAttachmentRead,
									 vk::PipelineStageFlagBits::eColorAttachmentOutput);

	NormalsBuffer.TransitionLayout(vk::ImageLayout::eColorAttachmentOptimal,
								   vk::AccessFlagBits::eColorAttachmentRead,
								   vk::PipelineStageFlagBits::eColorAttachmentOutput);

	NormalsBuffer.TransitionLayout(vk::ImageLayout::eColorAttachmentOptimal,
								   vk::AccessFlagBits::eColorAttachmentRead,
								   vk::PipelineStageFlagBits::eColorAttachmentOutput);

	TransitionImageLayout(Vulkan.SwapchainImages[Vulkan.CurrentImageIndex],
						  vk::ImageLayout::eUndefined,
						  vk::ImageLayout::eColorAttachmentOptimal,
						  {},
						  vk::AccessFlagBits::eColorAttachmentWrite,
						  vk::PipelineStageFlagBits::eTopOfPipe,
						  vk::PipelineStageFlagBits::eColorAttachmentOutput);

	CompositionRenderPass.Begin();
	DrawCompositionPass();
	CompositionRenderPass.End();

	/*TransitionImageLayout(Vulkan.SwapchainImages[Vulkan.CurrentImageIndex],
						  vk::ImageLayout::eColorAttachmentOptimal,
						  vk::ImageLayout::ePresentSrcKHR,
						  vk::AccessFlagBits::eColorAttachmentWrite,
						  {},
						  vk::PipelineStageFlagBits::eColorAttachmentOutput,
						  vk::PipelineStageFlagBits::eBottomOfPipe);*/

	//   Done in Renderer:
	// Render ImGui pass
	// end command buffer
}

void AdvancedRenderer::RenderUI()
{
}

// ------------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void AdvancedRenderer::CollectRenderData()
{
	NumVertices = 0;

	Vertex* target = reinterpret_cast<Vertex*>(
		Vulkan.Device.mapMemory(VertexBuffer.BufferCPU.Memory, 0, VertexBuffer.BufferCPU.Size));

	const glm::vec3 up(0, 1, 0);

	for (auto& particle : Dataset->Snapshots[CurrentFrame])
	{
		glm::vec3 z = CameraController.System[2];
		glm::vec3 x = glm::normalize(glm::cross(up, z));
		glm::vec3 y = glm::normalize(glm::cross(x, z));

		glm::vec3 p(particle[0], particle[1], particle[2]);

		glm::vec3 topLeft     = p + Dataset->ParticleRadius * (-x + y);
		glm::vec3 topRight    = p + Dataset->ParticleRadius * (+x + y);
		glm::vec3 bottomLeft  = p + Dataset->ParticleRadius * (-x - y);
		glm::vec3 bottomRight = p + Dataset->ParticleRadius * (+x - y);

		target[NumVertices++] = { topLeft, { 0, 0 } };
		target[NumVertices++] = { bottomLeft, { 0, 1 } };
		target[NumVertices++] = { topRight, { 1, 0 } };
		target[NumVertices++] = { topRight, { 1, 0 } };
		target[NumVertices++] = { bottomLeft, { 0, 1 } };
		target[NumVertices++] = { bottomRight, { 1, 1 } };
	}

	Vulkan.Device.unmapMemory(VertexBuffer.BufferCPU.Memory);

	// copy
	if (NumVertices > 0)
	{
		OneTimeCommand cmd;
		VertexBuffer.Copy(cmd);
	}
}

void AdvancedRenderer::DrawDepthPass()
{
	vk::DeviceSize offset = 0;
	Vulkan.CommandBuffer.bindVertexBuffers(
		0,
		VertexBuffer.BufferGPU.Buffer,
		offset);

	Vulkan.CommandBuffer.draw(NumVertices, 1, 0, 0);
}

void AdvancedRenderer::RayMarch()
{
	// ...

	/*void* mapped = DepthBuffer.MapCPUMemory();
	DumpDataToFile("DEPTH", mapped, DepthBuffer.Size);
	DepthBuffer.UnmapCPUMemory();*/
}

void AdvancedRenderer::DrawCompositionPass()
{
	vk::DeviceSize offset = 0;
	Vulkan.CommandBuffer.bindVertexBuffers(
		0,
		VertexBuffer.BufferGPU.Buffer,
		offset);

	Vulkan.CommandBuffer.draw(3, 1, 0, 0);
}

// ------------------------------------------------------------------------
