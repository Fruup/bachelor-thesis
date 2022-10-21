#include <engine/hzpch.h>

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>
#include <engine/events/Event.h>
#include <engine/utils/PerformanceTimer.h>
#include <engine/input/Input.h>

#include "app/Kernel.h"
#include "app/Utils.h"

#include <fstream>

// ------------------------------------------------------------------------

VisualizationSettings g_VisualizationSettings = {
	.MaxSteps = 64,
	.StepSize = 0.006f,
	.IsoDensity = 1.0f,

	.EnableAnisotropy = false,
	.k_n = 0.5f,
	.k_r = 3.0f,
	.k_s = 2000.0f,
	.N_eps = 25,
};

static float s_ProcessTimer = 0.0f;
constexpr const float s_ProcessTimerMax = 1.0f;

bool g_Autoplay = false;

std::atomic_bool RayMarchFinished = true;

bool s_EnableDepthPass = true;
bool s_EnableRayMarch = false;
bool s_EnableGaussPass = true;
bool s_EnableCoordinateSystem = true;
bool s_EnableComposition = true;
bool s_EnableShowDepth = false;

// ------------------------------------------------------------------------

decltype(AdvancedRenderer::Vertex::Attributes) AdvancedRenderer::Vertex::Attributes = {
	// uint32_t location
	// uint32_t binding
	// Format format = Format::eUndefined
	// uint32_t offset
	vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position) },
	vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, UV) },
};

const int bla = offsetof(AdvancedRenderer::Vertex, UV);

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

// ------------------------------------------------------------------------
// PUBLIC FUNCTIONS

AdvancedRenderer::AdvancedRenderer() :
	DepthRenderPass(*this),
	CompositionRenderPass(*this),
	GaussRenderPass(*this),
	ShowImageRenderPass(*this),
	CoordinateSystemRenderPass(*this),
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
	CoordinateSystemRenderPass.Init();

	ShowImageRenderPass.ImageView = DepthBuffer.GPU.ImageView;
	ShowImageRenderPass.Sampler = DepthBuffer.GPU.Sampler;
	ShowImageRenderPass.Init();

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
	m_RayMarcher.Exit();

	delete Dataset;
	VertexBuffer.Destroy();

	CoordinateSystemRenderPass.Exit();
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
	if (s_EnableRayMarch)
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

		if (s_EnableDepthPass)
		{
			DepthBuffer.TransitionLayout(vk::ImageLayout::eDepthAttachmentOptimal,
										 vk::AccessFlagBits::eDepthStencilAttachmentRead |
										 vk::AccessFlagBits::eDepthStencilAttachmentWrite,
										 vk::PipelineStageFlagBits::eEarlyFragmentTests |
										 vk::PipelineStageFlagBits::eLateFragmentTests);

			DepthRenderPass.Begin();
			DrawDepthPass();
			DepthRenderPass.End();
		}

		if (s_EnableGaussPass)
		{
			DepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
										 vk::AccessFlagBits::eColorAttachmentRead,
										 vk::PipelineStageFlagBits::eColorAttachmentOutput);

			SmoothedDepthBuffer.TransitionLayout(vk::ImageLayout::eColorAttachmentOptimal,
												 vk::AccessFlagBits::eColorAttachmentWrite,
												 vk::PipelineStageFlagBits::eColorAttachmentOutput);

			GaussRenderPass.Begin();
			DrawFullscreenQuad();
			GaussRenderPass.End();
		}

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

		{
			PROFILE_SCOPE("Submit and wait");

			Vulkan.Submit();
			Vulkan.WaitForRenderingFinished();
		}

		if (RayMarchFinished)
			DepthBuffer.CopyFromGPU();
	}

	if (RayMarchFinished && s_ProcessTimer >= s_ProcessTimerMax)
	{
		RayMarchFinished = false;

		m_Positions = reinterpret_cast<glm::vec4*>(PositionsBuffer.MapCPUMemory());
		m_Normals = reinterpret_cast<glm::vec4*>(NormalsBuffer.MapCPUMemory());
		m_Depth = reinterpret_cast<float*>(DepthBuffer.MapCPUMemory());

		if (g_Autoplay)
			g_VisualizationSettings.Frame =
				std::min(g_VisualizationSettings.Frame + 1, int(Dataset->Frames.size()) - 1);

		m_RayMarcher.Prepare(g_VisualizationSettings,
							 CameraController,
							 Dataset,
							 m_Positions,
							 m_Normals,
							 m_Depth);

		m_RayMarcher.Start();
	}

	if (!RayMarchFinished && m_RayMarcher.IsDone())
	{
		RayMarchFinished = true;

		PositionsBuffer.UnmapCPUMemory();
		NormalsBuffer.UnmapCPUMemory();
		DepthBuffer.UnmapCPUMemory();

		s_ProcessTimer = 0.0f;
	}

	{
		PROFILE_SCOPE("Post-marching");

		PositionsBuffer.CopyToGPU();
		NormalsBuffer.CopyToGPU();

		Vulkan.CommandBuffer.reset();
		vk::CommandBufferBeginInfo beginInfo;
		Vulkan.CommandBuffer.begin(beginInfo);

		PositionsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
										 vk::AccessFlagBits::eShaderRead,
										 vk::PipelineStageFlagBits::eFragmentShader);

		NormalsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									   vk::AccessFlagBits::eShaderRead,
									   vk::PipelineStageFlagBits::eFragmentShader);

		DepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									 vk::AccessFlagBits::eShaderRead,
									 vk::PipelineStageFlagBits::eFragmentShader);

		SmoothedDepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
											 vk::AccessFlagBits::eShaderRead,
											 vk::PipelineStageFlagBits::eFragmentShader);

		TransitionImageLayout(Vulkan.SwapchainImages[Vulkan.CurrentImageIndex],
							  vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eColorAttachmentOptimal,
							  {},
							  vk::AccessFlagBits::eColorAttachmentWrite,
							  vk::PipelineStageFlagBits::eTopOfPipe,
							  vk::PipelineStageFlagBits::eColorAttachmentOutput);

		if (s_EnableShowDepth)
		{
			ShowImageRenderPass.Begin();
			DrawFullscreenQuad();
			ShowImageRenderPass.End();
		}

		if (s_EnableComposition)
		{
			CompositionRenderPass.Begin();
			DrawFullscreenQuad();
			CompositionRenderPass.End();
		}

		if (s_EnableCoordinateSystem)
		{
			CoordinateSystemRenderPass.Begin();
			CoordinateSystemRenderPass.End();
		}
	}

	//   Done in Renderer:
	// Transition swapchain image layout
	// Render ImGui pass
	// end command buffer
}

void AdvancedRenderer::RenderUI()
{
	ImGui::Begin("Renderer");
	
	ImGui::Checkbox("Autoplay", &g_Autoplay);
	ImGui::DragInt("Frame", &g_VisualizationSettings.Frame, 0.125f, 0, Dataset->Frames.size() - 1, "%d", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SliderInt("# steps", &g_VisualizationSettings.MaxSteps, 0, 128);
	ImGui::DragFloat("Step size", &g_VisualizationSettings.StepSize, 0.0001f, 0.001f, 0.1f, "%f");
	ImGui::DragFloat("Iso", &g_VisualizationSettings.IsoDensity, 0.1f, 0.001f, 1000.0f, "%f", ImGuiSliderFlags_Logarithmic);

	ImGui::SliderFloat("Blur spread", &GaussRenderPass.Spread, 1.0f, 32.0f);
	
	// does not make sense because hash grid is not recomputed
	// ImGui::DragFloat("Particle radius", &Dataset->ParticleRadius, 0.01f, 0.1f, 3.0f);

	{
		ImGui::Separator();
		ImGui::Text("Anisotropy");

		ImGui::Checkbox("Enable", &g_VisualizationSettings.EnableAnisotropy);

		ImGui::DragFloat("k_n", &g_VisualizationSettings.k_n, 0.001f, 0.0f, 2.0f);
		ImGui::DragFloat("k_r", &g_VisualizationSettings.k_r, 0.001f, 0.0f, 8.0f);
		ImGui::DragFloat("k_s", &g_VisualizationSettings.k_s, 1.0f, 0.0f, 6000.0f);
		ImGui::DragInt("N_eps", &g_VisualizationSettings.N_eps, 1.0f, 1, 500);
	}

	{
		ImGui::Separator();
		ImGui::Text("Enable render passes");

		ImGui::Checkbox("Depth", &s_EnableDepthPass);
		ImGui::Checkbox("Coordinate System", &s_EnableCoordinateSystem);
		ImGui::Checkbox("Gaussian blur", &s_EnableGaussPass);
		ImGui::Checkbox("Ray march", &s_EnableRayMarch);
	}

	{
		ImGui::Separator();
		ImGui::Text("What to show");

		ImGui::TreePush("last_render_pass");

		static int radio = 0;
		ImGui::RadioButton("Composition", &radio, 0);
		
		if (ImGui::RadioButton("Depth", &radio, 1))
		{
			ShowImageRenderPass.ImageView = DepthBuffer.GPU.ImageView;
			ShowImageRenderPass.Sampler = DepthBuffer.GPU.Sampler;
		}

		if (ImGui::RadioButton("Blurred depth", &radio, 2))
		{
			ShowImageRenderPass.ImageView = SmoothedDepthBuffer.GPU.ImageView;
			ShowImageRenderPass.Sampler = SmoothedDepthBuffer.GPU.Sampler;
		}

		ImGui::TreePop();

		if (radio == 0)
		{
			s_EnableComposition = true;
			s_EnableShowDepth = false;
		}
		else if (radio == 1 || radio == 2)
		{
			s_EnableComposition = false;
			s_EnableShowDepth = true;
		}
	}

	ImGui::End();

	GaussRenderPass.RenderUI();
}

// ------------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void AdvancedRenderer::CollectRenderData()
{
	PROFILE_FUNCTION();

	NumVertices = 0;

	Vertex* target = reinterpret_cast<Vertex*>(
		Vulkan.Device.mapMemory(VertexBuffer.BufferCPU.Memory, 0, VertexBuffer.BufferCPU.Size));

	for (auto& particle : Dataset->Frames[g_VisualizationSettings.Frame])
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

void AdvancedRenderer::DrawDepthPass()
{
	vk::DeviceSize offset = 0;
	Vulkan.CommandBuffer.bindVertexBuffers(
		0,
		VertexBuffer.BufferGPU.Buffer,
		offset);

	Vulkan.CommandBuffer.draw(NumVertices, 1, 0, 0);
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

// ------------------------------------------------------------------------
