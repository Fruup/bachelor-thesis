#include <engine/hzpch.h>

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>
#include <engine/events/Event.h>
#include <engine/utils/PerformanceTimer.h>
#include <engine/input/Input.h>

#include "app/Kernel.h"
#include "app/Utils.h"
#include "app/VisualizationSettings.h"

#include "cuda/CudaContext.cuh"

#include "app/WindowsSecurityAttributes.h"

#include <fstream>

//#include <dxgi1_2.h>

// ------------------------------------------------------------------------

VisualizationSettings g_VisualizationSettings = {
	.MaxSteps = 128,
	.StepSize = 0.009f,
	.IsoDensity = 1.0f,

	.EnableAnisotropy = true,
	.k_n = 0.5f,
	.k_r = 2.0f,
	.k_s = 2000.0f,
	.N_eps = 1,
};

static float s_ProcessTimer = 0.0f;
constexpr const float s_ProcessTimerMax = 1.0f;

bool g_Autoplay = false;
bool g_Recording = false;

std::atomic_bool RayMarchFinished = true;

bool s_EnableDepthPass = true;
bool s_EnableRayMarch = true;
bool s_EnableGaussPass = true;
bool s_EnableCoordinateSystem = true;
bool s_EnableComposition = true;
bool s_EnableShowDepth = false;

// ------------------------------------------------------------------------

HANDLE GetSemaphoreHandle(const vk::Semaphore& semaphore)
{
	HANDLE handle;

	VkSemaphoreGetWin32HandleInfoKHR vulkanSemaphoreGetWin32HandleInfoKHR = {};
	vulkanSemaphoreGetWin32HandleInfoKHR.sType =
		VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
	vulkanSemaphoreGetWin32HandleInfoKHR.pNext = NULL;
	vulkanSemaphoreGetWin32HandleInfoKHR.semaphore = semaphore;
	vulkanSemaphoreGetWin32HandleInfoKHR.handleType =
		VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	auto getHandle =
		PFN_vkGetSemaphoreWin32HandleKHR(
			vkGetDeviceProcAddr(Vulkan.Device, "vkGetSemaphoreWin32HandleKHR"));

	HZ_ASSERT(getHandle != nullptr,
			  "Failed to load vulkan function 'vkGetSemaphoreWin32HandleKHR'!");

	getHandle(Vulkan.Device, &vulkanSemaphoreGetWin32HandleInfoKHR, &handle);

	HZ_ASSERT(handle != nullptr,
			  "Failed to get semaphore handle!");

	return handle;
}

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
	//GaussRenderPass(*this),
	ShowImageRenderPass(*this),
	CoordinateSystemRenderPass(*this),
	CameraController(Camera)
{
}

void AdvancedRenderer::Init(::Dataset* dataset)
{
	Dataset = dataset;

	uint32_t width = Vulkan.SwapchainExtent.width;
	uint32_t height = Vulkan.SwapchainExtent.height;

	/*DepthBuffer.Init(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
					 vk::Format::eD32Sfloat,
					 vk::ImageAspectFlagBits::eDepth);*/

	DepthBuffer.Init(vk::Format::eD32Sfloat,
					 vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
					 width, height);

	SmoothedDepthBuffer.Init(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
							 vk::Format::eD32Sfloat,
							 vk::ImageAspectFlagBits::eDepth);

	PositionsBuffer.Init(vk::Format::eR32G32B32A32Sfloat,
						 vk::ImageUsageFlagBits::eStorage |
							 vk::ImageUsageFlagBits::eSampled |
							 vk::ImageUsageFlagBits::eTransferDst |
							 vk::ImageUsageFlagBits::eTransferSrc,
						 width, height,
						 vk::ImageLayout::eTransferDstOptimal);

	NormalsBuffer.Init(vk::Format::eR32G32B32A32Sfloat,
					   vk::ImageUsageFlagBits::eStorage |
						 vk::ImageUsageFlagBits::eSampled |
						 vk::ImageUsageFlagBits::eTransferDst |
						 vk::ImageUsageFlagBits::eTransferSrc,
					   width, height,
					   vk::ImageLayout::eShaderReadOnlyOptimal);

	DepthRenderPass.Init();
	CompositionRenderPass.Init();
	//GaussRenderPass.Init();
	CoordinateSystemRenderPass.Init();

	ShowImageRenderPass.ImageView = DepthBuffer.m_ImageView;
	ShowImageRenderPass.Sampler = DepthBuffer.m_Sampler;
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

	// init cuda
	vk::PhysicalDeviceIDProperties deviceIDProps;
	vk::PhysicalDeviceProperties2 props;
	props.pNext = &deviceIDProps;

	Vulkan.PhysicalDevice.getProperties2(&props);

	Cuda.Init(Vulkan.Device, deviceIDProps.deviceUUID, sizeof(deviceIDProps.deviceUUID));

	// import image memory
	auto importedDepthBuffer = Cuda.ImportImageMemory(DepthBuffer);
	auto importedPositionsBuffer = Cuda.ImportImageMemory(PositionsBuffer);
	auto importedNormalsBuffer = Cuda.ImportImageMemory(NormalsBuffer);

	m_RayMarcher.Setup(g_VisualizationSettings,
					   importedDepthBuffer.surfaceObject,
					   {},
					   importedPositionsBuffer.deviceSurfaceObject,
					   importedNormalsBuffer.surfaceObject,
					   width,
					   height);

	CreateSemaphores();

	// import semaphores
	Cuda.ImportSemaphores(GetSemaphoreHandle(m_SemaphoreVK), GetSemaphoreHandle(m_SemaphoreCU));
}

void AdvancedRenderer::Exit()
{
	//Cuda.Exit();

	//m_RayMarcher.Exit();

	if (m_SemaphoreCU)
	{
		Vulkan.Device.destroySemaphore(m_SemaphoreCU);
		m_SemaphoreCU = nullptr;
	}

	if (m_SemaphoreVK)
	{
		Vulkan.Device.destroySemaphore(m_SemaphoreVK);
		m_SemaphoreVK = nullptr;
	}

	delete Dataset;
	VertexBuffer.Destroy();

	CoordinateSystemRenderPass.Exit();
	ShowImageRenderPass.Exit();
	//GaussRenderPass.Exit();
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
			TransitionImageLayout(Vulkan.CommandBuffer,
								  DepthBuffer.m_Image,
								  vk::ImageAspectFlagBits::eDepth,
								  vk::ImageLayout::eUndefined,
								  vk::ImageLayout::eDepthAttachmentOptimal,
								  {},
								  vk::AccessFlagBits::eDepthStencilAttachmentRead |
									vk::AccessFlagBits::eDepthStencilAttachmentWrite,
								  vk::PipelineStageFlagBits::eTopOfPipe,
								  vk::PipelineStageFlagBits::eEarlyFragmentTests |
									vk::PipelineStageFlagBits::eLateFragmentTests);

			/*DepthBuffer.TransitionLayout(vk::ImageLayout::eDepthAttachmentOptimal,
										 vk::AccessFlagBits::eDepthStencilAttachmentRead |
										 vk::AccessFlagBits::eDepthStencilAttachmentWrite,
										 vk::PipelineStageFlagBits::eEarlyFragmentTests |
										 vk::PipelineStageFlagBits::eLateFragmentTests);*/

			DepthRenderPass.Begin();
			DrawDepthPass();
			DepthRenderPass.End();
		}

		TransitionImageLayout(Vulkan.CommandBuffer,
							  PositionsBuffer.m_Image,
							  vk::ImageAspectFlagBits::eColor,
							  vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eTransferDstOptimal,
							  vk::AccessFlagBits::eNone,
							  vk::AccessFlagBits::eNone,
							  vk::PipelineStageFlagBits::eAllGraphics,
							  vk::PipelineStageFlagBits::eAllGraphics);

		/*TransitionImageLayout(Vulkan.CommandBuffer,
							  NormalsBuffer.m_Image,
							  vk::ImageAspectFlagBits::eColor,
							  vk::ImageLayout::eShaderReadOnlyOptimal,
							  vk::ImageLayout::eColorAttachmentOptimal,
							  vk::AccessFlagBits::eMemoryWrite,
							  vk::AccessFlagBits::eMemoryRead,
							  vk::PipelineStageFlagBits::eBottomOfPipe,
							  vk::PipelineStageFlagBits::eTopOfPipe);*/

		/*TransitionImageLayout(Vulkan.CommandBuffer,
							  PositionsBuffer.m_Image,
							  vk::ImageAspectFlagBits::eColor,
							  vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eColorAttachmentOptimal,
							  {},
							  vk::AccessFlagBits::eShaderWrite,
							  vk::PipelineStageFlagBits::eBottomOfPipe,
							  vk::PipelineStageFlagBits::eAllCommands);*/

		/*if (s_EnableGaussPass)
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
		}*/

		//DepthBuffer.TransitionLayout(vk::ImageLayout::eTransferSrcOptimal,
		//							 vk::AccessFlagBits::eTransferRead,
		//							 vk::PipelineStageFlagBits::eTransfer);

		/*PositionsBuffer.TransitionLayout(vk::ImageLayout::eTransferDstOptimal,
										 vk::AccessFlagBits::eTransferWrite,
										 vk::PipelineStageFlagBits::eTransfer);

		NormalsBuffer.TransitionLayout(vk::ImageLayout::eTransferDstOptimal,
									   vk::AccessFlagBits::eTransferWrite,
									   vk::PipelineStageFlagBits::eTransfer);*/

		Vulkan.CommandBuffer.end();

		{
			PROFILE_SCOPE("Submit and wait");

			Vulkan.Submit({},
						  {},
						  { m_SemaphoreVK });
			Vulkan.WaitForRenderingFinished();
		}
	}

	// launch cuda
	{
		PROFILE_SCOPE("Cuda");

		Cuda.WaitForVulkan();
		m_RayMarcher.Run();
		Cuda.SignalVulkan();
	}

	{
		PROFILE_SCOPE("Post-marching");

		Vulkan.CommandBuffer.reset();
		vk::CommandBufferBeginInfo beginInfo;
		Vulkan.CommandBuffer.begin(beginInfo);

		/*PositionsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
										 vk::AccessFlagBits::eShaderRead,
										 vk::PipelineStageFlagBits::eFragmentShader);


		NormalsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									   vk::AccessFlagBits::eShaderRead,
									   vk::PipelineStageFlagBits::eFragmentShader);*/

		TransitionImageLayout(Vulkan.CommandBuffer,
							  DepthBuffer.m_Image,
							  vk::ImageAspectFlagBits::eDepth,
							  vk::ImageLayout::eDepthAttachmentOptimal,
							  vk::ImageLayout::eShaderReadOnlyOptimal,
							  vk::AccessFlagBits::eDepthStencilAttachmentRead |
								vk::AccessFlagBits::eDepthStencilAttachmentWrite,
							  vk::AccessFlagBits::eShaderRead,
							  vk::PipelineStageFlagBits::eEarlyFragmentTests |
								vk::PipelineStageFlagBits::eLateFragmentTests,
							  vk::PipelineStageFlagBits::eFragmentShader);

		TransitionImageLayout(Vulkan.CommandBuffer,
							  PositionsBuffer.m_Image,
							  vk::ImageAspectFlagBits::eColor,
							  vk::ImageLayout::eTransferDstOptimal,
							  vk::ImageLayout::eShaderReadOnlyOptimal,
							  vk::AccessFlagBits::eNone,
							  vk::AccessFlagBits::eNone,
							  vk::PipelineStageFlagBits::eAllGraphics,
							  vk::PipelineStageFlagBits::eAllGraphics);

		/*TransitionImageLayout(Vulkan.CommandBuffer,
							  PositionsBuffer.m_Image,
							  vk::ImageAspectFlagBits::eColor,
							  vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eShaderReadOnlyOptimal,
							  vk::AccessFlagBits::eMemoryWrite,
							  vk::AccessFlagBits::eMemoryRead,
							  vk::PipelineStageFlagBits::eBottomOfPipe,
							  vk::PipelineStageFlagBits::eTopOfPipe);

		TransitionImageLayout(Vulkan.CommandBuffer,
							  NormalsBuffer.m_Image,
							  vk::ImageAspectFlagBits::eColor,
							  vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eShaderReadOnlyOptimal,
							  vk::AccessFlagBits::eMemoryWrite,
							  vk::AccessFlagBits::eMemoryRead,
							  vk::PipelineStageFlagBits::eBottomOfPipe,
							  vk::PipelineStageFlagBits::eTopOfPipe);*/

		/*DepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									 vk::AccessFlagBits::eShaderRead,
									 vk::PipelineStageFlagBits::eFragmentShader);*/

		/*SmoothedDepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
											 vk::AccessFlagBits::eShaderRead,
											 vk::PipelineStageFlagBits::eFragmentShader);*/

		TransitionImageLayout(Vulkan.CommandBuffer,
							  Vulkan.SwapchainImages[Vulkan.CurrentImageIndex],
							  vk::ImageAspectFlagBits::eColor,
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

		/*if (s_EnableCoordinateSystem)
		{
			CoordinateSystemRenderPass.Begin();
			CoordinateSystemRenderPass.End();
		}*/
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

	if (ImGui::Checkbox("Recording", &g_Recording) && g_Recording)
		g_Autoplay = true;

	ImGui::DragInt("Frame", &g_VisualizationSettings.Frame, 0.125f, 0, Dataset->Frames.size() - 1, "%d", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SliderInt("# steps", &g_VisualizationSettings.MaxSteps, 0, 128);
	ImGui::DragFloat("Step size", &g_VisualizationSettings.StepSize, 0.0001f, 0.001f, 0.1f, "%f");
	ImGui::DragFloat("Iso", &g_VisualizationSettings.IsoDensity, 0.1f, 0.001f, 1000.0f, "%f", ImGuiSliderFlags_Logarithmic);

	//ImGui::SliderFloat("Blur spread", &GaussRenderPass.Spread, 1.0f, 32.0f);
	
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
			ShowImageRenderPass.ImageView = DepthBuffer.m_ImageView;
			ShowImageRenderPass.Sampler = DepthBuffer.m_Sampler;
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

	//GaussRenderPass.RenderUI();
}

// ------------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void AdvancedRenderer::CollectRenderData()
{
	PROFILE_FUNCTION();

	NumVertices = 0;

	Vertex* target = reinterpret_cast<Vertex*>(
		Vulkan.Device.mapMemory(VertexBuffer.BufferCPU.Memory, 0, VertexBuffer.BufferCPU.Size));

	for (auto& particle : Dataset->Frames[g_VisualizationSettings.Frame].m_Particles)
	{
		glm::vec3 x = CameraController.System[0];
		glm::vec3 y = CameraController.System[1];

		glm::vec3 p = particle;
		const float R = Dataset->ParticleRadius;

		glm::vec3 topLeft     = p + R * (-x + y);
		glm::vec3 topRight    = p + R * (+x + y);
		glm::vec3 bottomLeft  = p + R * (-x - y);
		glm::vec3 bottomRight = p + R * (+x - y);

		target[NumVertices++] = { topLeft, { -1, -1 } };
		target[NumVertices++] = { bottomLeft, { -1, 1 } };
		target[NumVertices++] = { topRight, { 1, -1 } };
		target[NumVertices++] = { topRight, { 1, -1 } };
		target[NumVertices++] = { bottomLeft, { -1, 1 } };
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

void AdvancedRenderer::CreateSemaphores()
{
	extern DWORD g_DXResourceAccess;

	// export semaphores
	WindowsSecurityAttributes winSecurityAttributes;

	VkExportSemaphoreWin32HandleInfoKHR
		vulkanExportSemaphoreWin32HandleInfoKHR = {};
	vulkanExportSemaphoreWin32HandleInfoKHR.sType =
		VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR;
	vulkanExportSemaphoreWin32HandleInfoKHR.pNext = NULL;
	vulkanExportSemaphoreWin32HandleInfoKHR.pAttributes =
		&winSecurityAttributes;
	vulkanExportSemaphoreWin32HandleInfoKHR.dwAccess = g_DXResourceAccess;
	vulkanExportSemaphoreWin32HandleInfoKHR.name = 0;
	
	vk::ExportSemaphoreCreateInfo vulkanExportSemaphoreCreateInfo;
	vulkanExportSemaphoreCreateInfo
		.setPNext(&vulkanExportSemaphoreWin32HandleInfoKHR)
		.setHandleTypes(vk::ExternalSemaphoreHandleTypeFlagBits::eOpaqueWin32);
	
	// create semaphores
	vk::SemaphoreCreateInfo semaphoreInfo;
	semaphoreInfo.setPNext(&vulkanExportSemaphoreCreateInfo);

	m_SemaphoreVK = Vulkan.Device.createSemaphore(semaphoreInfo);
	m_SemaphoreCU = Vulkan.Device.createSemaphore(semaphoreInfo);

	if (!m_SemaphoreVK || !m_SemaphoreCU)
		SPDLOG_ERROR("Failed to create semaphores!");
}

// ------------------------------------------------------------------------
