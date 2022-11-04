#include <engine/hzpch.h>

#include "DiskRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>
#include <engine/events/Event.h>
#include <engine/utils/PerformanceTimer.h>

#include "app/Kernel.h"
#include "app/Utils.h"

#include <fstream>
#include <thread>

// ------------------------------------------------------------------------

decltype(DiskRenderer::Vertex::Attributes) DiskRenderer::Vertex::Attributes = {
	// uint32_t location
	// uint32_t binding
	// Format format = Format::eUndefined
	// uint32_t offset
	vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position) },
	vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, UV) },
};

const int bla = offsetof(DiskRenderer::Vertex, UV);

decltype(DiskRenderer::Vertex::Binding) DiskRenderer::Vertex::Binding = vk::VertexInputBindingDescription{
	0, // uint32_t binding
	sizeof(DiskRenderer::Vertex), // uint32_t stride
	vk::VertexInputRate::eVertex, // VertexInputRate inputRate
};

// ------------------------------------------------------------------------
// PUBLIC FUNCTIONS

DiskRenderer::DiskRenderer() :
	//CoordinateSystemRenderPass(*this),
	DiskRenderPass(*this),
	CameraController(Camera)
{
}

void DiskRenderer::Init(::Dataset* dataset)
{
	Dataset = dataset;

	DepthBuffer.Init(BilateralBuffer::Depth);

	DiskRenderPass.Init();
	//CoordinateSystemRenderPass.Init();

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

void DiskRenderer::Exit()
{
	delete Dataset;
	VertexBuffer.Destroy();

	//CoordinateSystemRenderPass.Exit();
	DiskRenderPass.Exit();

	DepthBuffer.Exit();
}

void DiskRenderer::Update(float time)
{
	CameraController.Update(time);
}

void DiskRenderer::HandleEvent(Event& e)
{
	CameraController.HandleEvent(e);
}

void DiskRenderer::Render()
{
	CollectRenderData();

	DepthBuffer.TransitionLayout(vk::ImageLayout::eDepthAttachmentOptimal,
									vk::AccessFlagBits::eDepthStencilAttachmentRead |
									vk::AccessFlagBits::eDepthStencilAttachmentWrite,
									vk::PipelineStageFlagBits::eEarlyFragmentTests |
									vk::PipelineStageFlagBits::eLateFragmentTests);

	TransitionImageLayout(Vulkan.CommandBuffer,
						  Vulkan.SwapchainImages[Vulkan.CurrentImageIndex],
						  vk::ImageAspectFlagBits::eColor,
						  vk::ImageLayout::eUndefined,
						  vk::ImageLayout::eColorAttachmentOptimal,
						  {},
						  vk::AccessFlagBits::eColorAttachmentWrite,
						  vk::PipelineStageFlagBits::eTopOfPipe,
						  vk::PipelineStageFlagBits::eColorAttachmentOutput);

	DiskRenderPass.Begin();
	DrawParticles();
	DiskRenderPass.End();

	/*CoordinateSystemRenderPass.Begin();
	CoordinateSystemRenderPass.End();*/
}

void DiskRenderer::RenderUI()
{
	ImGui::Begin("Disk Renderer");

	ImGui::SliderInt("Frame", &CurrentFrame, 0, Dataset->Frames.size() - 1);

	ImGui::InputFloat3("Camera", (float*)&CameraController.Position, "%.3f", ImGuiInputTextFlags_ReadOnly);

	ImGui::End();
}

// ------------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void DiskRenderer::CollectRenderData()
{
	PROFILE_FUNCTION();

	NumVertices = 0;

	Vertex* target = reinterpret_cast<Vertex*>(
		Vulkan.Device.mapMemory(VertexBuffer.BufferCPU.Memory, 0, VertexBuffer.BufferCPU.Size));

	for (auto& particle : Dataset->Frames[CurrentFrame].m_Particles)
	{
		glm::vec3 x = CameraController.System[0];
		glm::vec3 y = CameraController.System[1];

		glm::vec3 p = particle;
		const float R = Dataset->ParticleRadius;

		glm::vec3 topLeft     = p + R * (-x + y);
		glm::vec3 topRight    = p + R * (+x + y);
		glm::vec3 bottomLeft  = p + R * (-x - y);
		glm::vec3 bottomRight = p + R * (+x - y);

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

void DiskRenderer::DrawParticles()
{
	vk::DeviceSize offset = 0;
	Vulkan.CommandBuffer.bindVertexBuffers(
		0,
		VertexBuffer.BufferGPU.Buffer,
		offset);

	Vulkan.CommandBuffer.draw(NumVertices, 1, 0, 0);
}

float DiskRenderer::ComputeDensity(const glm::vec3& x)
{
	CubicSplineKernel K(Dataset->ParticleRadius);
	const auto& neighbors = Dataset->GetNeighbors(x, CurrentFrame);

	float density = 0.0f;
	for (const auto& index : neighbors)
	{
		const auto& p = Dataset->Frames[CurrentFrame].m_Particles[index];
		density += K.W(x - glm::vec3(p[0], p[1], p[2]));
	}

	return density;
}

// ------------------------------------------------------------------------
