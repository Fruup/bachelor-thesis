#include <engine/hzpch.h>

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>
#include <engine/events/Event.h>
#include <engine/utils/PerformanceTimer.h>

#include "app/Kernel.h"

#include <fstream>
#include <thread>

// ------------------------------------------------------------------------

static int s_MaxSteps = 16;
static float s_StepSize = 0.01f;
static float s_IsoDensity = 1.0f;

static float s_ProcessTimer = 0.0f;
constexpr const float s_ProcessTimerMax = 1.0f;

std::thread RayMarchThread;
std::atomic_bool RayMarchFinished = true;
std::condition_variable ConditionVariable;
std::mutex Mutex;
bool ShouldTerminateThread = true;

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
	GaussRenderPass(*this),
	ShowImageRenderPass(*this),
	CameraController(Camera)
{
}

void AdvancedRenderer::Init(::Dataset* dataset)
{
	Dataset = dataset;

	DepthBuffer.Init(BilateralBuffer::Depth);
	SmoothedDepthBuffer.Init(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
							 vk::Format::eR32Sfloat,
							 vk::ImageAspectFlagBits::eColor);
	PositionsBuffer.Init(BilateralBuffer::Color);
	NormalsBuffer.Init(BilateralBuffer::Color);

	DepthRenderPass.Init();
	CompositionRenderPass.Init();
	GaussRenderPass.Init();
	ShowImageRenderPass.Init();

	VertexBuffer.Create(6 * Dataset->MaxParticles * sizeof(Vertex));

	// init camera
	Camera = Camera3D(
		glm::radians(60.0f),
		Vulkan.GetAspect(),
		0.1f,
		1000.0f
	);

	
	//CameraController.SetPosition({ 0, 3, -5 });
	CameraController.SetPosition({ 10.0f, 2.5f, -16.0f });
	//CameraController.SetOrientation(glm::quatLookAtLH(-glm::normalize(CameraController.Position), { 0, 1, 0 }));
	CameraController.SetOrientation(glm::quatLookAtLH(-glm::normalize(glm::vec3(0, 3, -5)), { 0, 1, 0 }));
	CameraController.ComputeMatrices();

	// start worker thread
	RayMarchThread = std::thread(&AdvancedRenderer::RayMarch, this);
}

void AdvancedRenderer::Exit()
{
	ShouldTerminateThread = true;
	ConditionVariable.notify_all();
	if (RayMarchThread.joinable())
		RayMarchThread.join();

	delete Dataset;
	VertexBuffer.Destroy();

	ShowImageRenderPass.Exit();
	GaussRenderPass.Exit();
	CompositionRenderPass.Exit();
	DepthRenderPass.Exit();

	NormalsBuffer.Exit();
	PositionsBuffer.Exit();
	SmoothedDepthBuffer.Exit();
	DepthBuffer.Exit();
}

void AdvancedRenderer::Update(float time)
{
	s_ProcessTimer += time;

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
	// - begin gauss pass
	// - render gauss pass
	// - end gauss pass
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
	
	{
		PROFILE_SCOPE("Pre-marching");

		CollectRenderData();

		DepthBuffer.TransitionLayout(vk::ImageLayout::eDepthAttachmentOptimal,
									 vk::AccessFlagBits::eDepthStencilAttachmentRead,
									 // vk::AccessFlagBits::eDepthStencilAttachmentWrite,
									 vk::PipelineStageFlagBits::eEarlyFragmentTests |
									 vk::PipelineStageFlagBits::eLateFragmentTests);

		DepthRenderPass.Begin();
		DrawDepthPass();
		DepthRenderPass.End();

		DepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									 vk::AccessFlagBits::eColorAttachmentRead,
									 vk::PipelineStageFlagBits::eColorAttachmentOutput);

		SmoothedDepthBuffer.TransitionLayout(vk::ImageLayout::eColorAttachmentOptimal,
									 vk::AccessFlagBits::eColorAttachmentWrite,
									 vk::PipelineStageFlagBits::eColorAttachmentOutput);

		GaussRenderPass.Begin();
		DrawFullscreenQuad();
		GaussRenderPass.End();

		/*DepthBuffer.TransitionLayout(vk::ImageLayout::eTransferSrcOptimal,
									 vk::AccessFlagBits::eTransferRead,
									 vk::PipelineStageFlagBits::eTransfer);

		SmoothedDepthBuffer.TransitionLayout(vk::ImageLayout::eTransferSrcOptimal,
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
		
		SmoothedDepthBuffer.CopyFromGPU();

		float* data = reinterpret_cast<float*>(Vulkan.Device.mapMemory(SmoothedDepthBuffer.CPU.Memory, 0, VK_WHOLE_SIZE));

		DumpDataToFile("OUT", data, SmoothedDepthBuffer.Size);*/

		/*if (RayMarchFinished)
			DepthBuffer.CopyFromGPU();*/
	}

	/*if (RayMarchFinished && s_ProcessTimer >= s_ProcessTimerMax)
	{
		RayMarchFinished = false;

		ConditionVariable.notify_all();
	}*/

	{
		PROFILE_SCOPE("Post-marching");

#if 0
		if (RayMarchFinished)
		{
			PositionsBuffer.CopyToGPU();
			NormalsBuffer.CopyToGPU();
		}

		Vulkan.CommandBuffer.reset();
		vk::CommandBufferBeginInfo beginInfo;
		Vulkan.CommandBuffer.begin(beginInfo);

		PositionsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
										 vk::AccessFlagBits::eColorAttachmentRead,
										 vk::PipelineStageFlagBits::eColorAttachmentOutput);

		NormalsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									   vk::AccessFlagBits::eColorAttachmentRead,
									   vk::PipelineStageFlagBits::eColorAttachmentOutput);

		DepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									 vk::AccessFlagBits::eColorAttachmentRead,
									 vk::PipelineStageFlagBits::eColorAttachmentOutput);
#endif

		PositionsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
										 vk::AccessFlagBits::eColorAttachmentRead,
										 vk::PipelineStageFlagBits::eColorAttachmentOutput);

		SmoothedDepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
											 vk::AccessFlagBits::eColorAttachmentRead,
											 vk::PipelineStageFlagBits::eColorAttachmentOutput);

		TransitionImageLayout(Vulkan.SwapchainImages[Vulkan.CurrentImageIndex],
							  vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eColorAttachmentOptimal,
							  {},
							  vk::AccessFlagBits::eColorAttachmentWrite,
							  vk::PipelineStageFlagBits::eTopOfPipe,
							  vk::PipelineStageFlagBits::eColorAttachmentOutput);

		/*ShowImageRenderPass.Begin();
		DrawFullscreenQuad();
		ShowImageRenderPass.End();*/

		CompositionRenderPass.Begin();
		DrawFullscreenQuad();
		CompositionRenderPass.End();
	}

	//   Done in Renderer:
	// Transition swapchain image layout
	// Render ImGui pass
	// end command buffer
}

void AdvancedRenderer::RenderUI()
{
	ImGui::Begin("Renderer");

	ImGui::SliderInt("Frame", &CurrentFrame, 0, Dataset->Snapshots.size() - 1);

	ImGui::SliderInt("# steps", &s_MaxSteps, 0, 64);
	ImGui::DragFloat("Step size", &s_StepSize, 0.1f, 0.000001f, 10.0f, "%f", ImGuiSliderFlags_Logarithmic);
	ImGui::DragFloat("Iso", &s_IsoDensity, 0.1f, 0.001f, 1000.0f, "%f", ImGuiSliderFlags_Logarithmic);

	//s_ShouldProcess = ImGui::Button("Process");

	float density = ComputeDensity({ 0, 0, 0 });
	ImGui::InputFloat("D", &density, 0.0f, 0.0f, "%f");

	ImGui::InputFloat3("Camera", CameraController.Position.data.data, "%.3f", ImGuiInputTextFlags_ReadOnly);

	ImGui::End();

	GaussRenderPass.RenderUI();
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
	while (!ShouldTerminateThread)
	{
		{
			std::unique_lock lock(Mutex);
			ConditionVariable.wait(lock);
		}

		PROFILE_FUNCTION();

		const int   MAX_STEPS   = s_MaxSteps;
		const float STEP_SIZE   = s_StepSize;
		const float ISO_DENSITY = s_IsoDensity;

		const int width = Vulkan.SwapchainExtent.width;
		const int height = Vulkan.SwapchainExtent.height;
		const float widthF = float(width);
		const float heightF = float(height);

		const float* depth = reinterpret_cast<float*>(DepthBuffer.MapCPUMemory());
		glm::vec4* positions = reinterpret_cast<glm::vec4*>(PositionsBuffer.MapCPUMemory());
		glm::vec4* normals = reinterpret_cast<glm::vec4*>(NormalsBuffer.MapCPUMemory());

		#pragma omp parallel for
		for (int y = 0; y < height; y++)
		{
			#pragma omp parallel for
			for (int x = 0; x < width; x++)
			{
				uint32_t index = y * width + x;
				const float z = depth[index];

				positions[index] = glm::vec4(0, 0, 0, 1);
				if (z == 1.0f) continue;

				glm::vec3 clip(2.0f * float(x) / widthF - 1.0f,
							   2.0f * float(y) / heightF - 1.0f,
							   z);

				glm::vec4 worldH = Camera.GetInvProjectionView() * glm::vec4(clip, 1);
				glm::vec3 world = glm::vec3(worldH) / worldH.w;

				glm::vec3 position = glm::vec3(worldH) / worldH.w;
				glm::vec3 step = STEP_SIZE * glm::normalize(position - CameraController.Position);

				for (int i = 0; i < MAX_STEPS; i++)
				{
					position += step;
					float density = ComputeDensity(position);
					if (density >= ISO_DENSITY)
					{
						//positions[index] = glm::vec4(density / ISO_DENSITY);
						positions[index] = glm::vec4(position, 0);
						
						//normals[index] = glm::vec4(0);

						i = MAX_STEPS;
					}
				}
			}
		}

		NormalsBuffer.UnmapCPUMemory();
		PositionsBuffer.UnmapCPUMemory();
		DepthBuffer.UnmapCPUMemory();

		s_ProcessTimer = 0.0f;
		RayMarchFinished = true;
	}
}

void AdvancedRenderer::DrawFullscreenQuad()
{
	vk::DeviceSize offset = 0;
	Vulkan.CommandBuffer.bindVertexBuffers(
		0,
		VertexBuffer.BufferGPU.Buffer,
		offset);

	Vulkan.CommandBuffer.draw(3, 1, 0, 0);
}

float AdvancedRenderer::ComputeDensity(const glm::vec3& x)
{
	CubicSplineKernel K(Dataset->ParticleRadius);
	const auto& neighbors = Dataset->GetNeighbors(x, CurrentFrame);

	float density = 0.0f;
	for (const auto& index : neighbors)
	{
		const auto& p = Dataset->Snapshots[CurrentFrame][index];
		density += K.W(x - glm::vec3(p[0], p[1], p[2]));
	}

	return density;
}

// ------------------------------------------------------------------------
