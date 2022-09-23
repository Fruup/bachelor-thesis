#include "engine/hzpch.h"
#include "engine/renderer/Renderer.h"
#include "engine/renderer/objects/Command.h"

#include <fstream>

RendererData Renderer::Data;

void GlfwErrorCallback(int error_code, const char* description)
{
	SPDLOG_ERROR("GLFW ERROR {}: {}", error_code, description);
}

static vk::DescriptorPool ImGuiDescriptorPool;

bool InitImGui()
{
	// create descriptor pool
	std::array<vk::DescriptorPoolSize, 2> poolSize{
		vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBuffer, 1 },
		vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, 1 },
	};

	vk::DescriptorPoolCreateInfo info;
	info.setPoolSizes(poolSize).setMaxSets(1);

	ImGuiDescriptorPool = Renderer::Data.Device.createDescriptorPool(info);
	if (!ImGuiDescriptorPool)
	{
		SPDLOG_ERROR("ImGui descriptor pool creation failed!");
		return false;
	}

	// imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	// glfw
	ImGui_ImplGlfw_InitForVulkan(Renderer::GetWindow(), false);

	// vulkan
	ImGui_ImplVulkan_InitInfo initInfo{
		Renderer::Data.Instance,
		Renderer::Data.PhysicalDevice,
		Renderer::Data.Device,
		Renderer::Data.QueueIndices.GraphicsFamily.value(),
		Renderer::Data.GraphicsQueue,
		nullptr,
		ImGuiDescriptorPool,
		/* subpass */ Renderer::Data.ImGuiSubpass,
		uint32_t(Renderer::Data.SwapchainImages.size()),
		uint32_t(Renderer::Data.SwapchainImages.size()),
		VK_SAMPLE_COUNT_1_BIT,
		nullptr,
		nullptr
	};

	if (!ImGui_ImplVulkan_Init(&initInfo, Renderer::Data.RenderPass))
	{
		SPDLOG_ERROR("ImGui Vulkan initialization failed!");
		return false;
	}

	// upload fonts
	auto cmd = Command::BeginOneTimeCommand();

	if (!ImGui_ImplVulkan_CreateFontsTexture(cmd))
	{
		SPDLOG_ERROR("ImGui Vulkan font texture creation failed!");
		return false;
	}

	Command::EndOneTimeCommand(cmd);

	ImGui_ImplVulkan_DestroyFontUploadObjects();

	return true;
}

void ExitImGui()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (ImGuiDescriptorPool)
	{
		Renderer::Data.Device.destroyDescriptorPool(ImGuiDescriptorPool);
		ImGuiDescriptorPool = nullptr;
	}
}

bool CenterGlfwWindow()
{
	GLFWwindow* window = Renderer::GetWindow();

	int sx = 0, sy = 0;
	int px = 0, py = 0;
	int mx = 0, my = 0;
	int monitor_count = 0;
	int best_area = 0;
	int final_x = 0, final_y = 0;

	glfwGetWindowSize(window, &sx, &sy);
	glfwGetWindowPos(window, &px, &py);

	// Iterate through all monitors
	GLFWmonitor** m = glfwGetMonitors(&monitor_count);
	if (!m)
		return false;

	for (int j = 0; j < monitor_count; ++j)
	{

		glfwGetMonitorPos(m[j], &mx, &my);
		const GLFWvidmode* mode = glfwGetVideoMode(m[j]);
		if (!mode)
			continue;

		// Get intersection of two rectangles - screen and window
		int minX = std::max(mx, px);
		int minY = std::max(my, py);

		int maxX = std::min(mx + mode->width, px + sx);
		int maxY = std::min(my + mode->height, py + sy);

		// Calculate area of the intersection
		int area = std::max(maxX - minX, 0) * std::max(maxY - minY, 0);

		// If its bigger than actual (window covers more space on this monitor)
		if (area > best_area)
		{
			// Calculate proper position in this monitor
			final_x = mx + (mode->width - sx) / 2;
			final_y = my + (mode->height - sy) / 2;

			best_area = area;
		}

	}

	// We found something
	if (best_area)
		glfwSetWindowPos(window, final_x, final_y);

	// Something is wrong - current window has NOT any intersection with any monitors. Move it to the default one.
	else
	{
		GLFWmonitor* primary = glfwGetPrimaryMonitor();
		if (primary)
		{
			const GLFWvidmode* desktop = glfwGetVideoMode(primary);

			if (desktop)
				glfwSetWindowPos(window, (desktop->width - sx) / 2, (desktop->height - sy) / 2);
			else
				return false;
		}
		else
			return false;
	}

	return true;
}

bool Renderer::Init(int width, int height, const char* title)
{
	// glfw
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwSetErrorCallback(GlfwErrorCallback);

	Data.Window = glfwCreateWindow(
		width, height, title,
		nullptr, nullptr
	);

	CenterGlfwWindow();

	if (!Data.Window)
	{
		SPDLOG_ERROR("Failed to create window!");
		return false;
	}

	if (!InitVulkan())
	{
		SPDLOG_ERROR("Failed to initialize vulkan");
		return false;
	}

	if (!InitImGui())
	{
		SPDLOG_ERROR("Failed to initialize ImGui!");
		return false;
	}

	{
		Data.DepthImageCPU.Create(
			4 * GetSwapchainExtent().width * GetSwapchainExtent().height,
			vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eHostCoherent |
			vk::MemoryPropertyFlagBits::eHostVisible
		);
	}

	return true;
}

void Renderer::Exit()
{
	Data.Device.waitIdle();

	ExitImGui();
	ExitVulkan();

	if (Data.Window)
		glfwDestroyWindow(Data.Window);
	Data.Window = nullptr;
	glfwTerminate();
}

void Renderer::Begin()
{
	// acquire image from swap chain
	Data.CurrentImageIndex = Data.Device.acquireNextImageKHR(Data.Swapchain, UINT64_MAX, Data.ImageAvailableSemaphore).value;

	// begin command buffer
	{
		Data.CommandBuffer.reset();

		vk::CommandBufferBeginInfo info;
		Data.CommandBuffer.begin(info);
	}

	// begin render pass
	std::array<vk::ClearValue, 4> clearValues = {
		// screen
		vk::ClearColorValue(std::array<float, 4>{ 1, 0, 0, 1 }),

		// depth
		vk::ClearDepthStencilValue{ 1.0f },

		// positions
		vk::ClearColorValue(std::array<float, 4>{ 0, 0, 0, 0 }),

		// normals
		vk::ClearColorValue(std::array<float, 4>{ 0, 0, 0, 0 }),
	};

	vk::RenderPassBeginInfo renderPassInfo;
	renderPassInfo
		.setClearValues(clearValues)
		.setFramebuffer(Data.SwapchainFramebuffers[GetCurrentSwapchainIndex()])
		.setRenderArea({ { 0, 0 }, Data.SwapchainExtent })
		.setRenderPass(Data.RenderPass);

	Data.CommandBuffer.beginRenderPass(
		renderPassInfo,
		vk::SubpassContents::eInline
	);

	// begin imgui
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}
}

void Renderer::End()
{
	// execute secondary command buffers
	//Data.CommandBuffer.executeCommands(Data.SecondaryCommandBuffers);

	// end imgui
	RenderImGui();

	// end render pass and command buffer
	Data.CommandBuffer.endRenderPass();


	{
		vk::BufferImageCopy region;
		region
			.setBufferImageHeight(0)
			.setBufferRowLength(0)
			.setImageExtent({ GetSwapchainExtent().width, GetSwapchainExtent().height, 1 })
			.setImageSubresource(vk::ImageSubresourceLayers(
				vk::ImageAspectFlagBits::eDepth, 0, 0, 1
			));

		Data.CommandBuffer.copyImageToBuffer(
			Data.DepthBuffer.GetImage(),
			vk::ImageLayout::eTransferSrcOptimal,
			Data.DepthImageCPU.Buffer,
			region
		);
	}


	Data.CommandBuffer.end();

	// submit
	{
		vk::SubmitInfo info;
		vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		info.setWaitSemaphores(Data.ImageAvailableSemaphore)
			.setWaitDstStageMask(waitStage)
			.setCommandBuffers(Data.CommandBuffer)
			.setSignalSemaphores(Data.RenderFinishedSemaphore);

		Renderer::Data.GraphicsQueue.submit(info);
		Renderer::Data.GraphicsQueue.waitIdle();
	}

	// PRESENT!
	vk::PresentInfoKHR presentInfo;
	presentInfo
		.setWaitSemaphores(Data.RenderFinishedSemaphore)
		.setSwapchains(Data.Swapchain)
		.setImageIndices(Data.CurrentImageIndex);

	if (Data.PresentationQueue.presentKHR(presentInfo) != vk::Result::eSuccess)
		SPDLOG_WARN("Presentation failed!");

	// ...and wait
	Data.PresentationQueue.waitIdle();

	Data.CurrentImageIndex = UINT32_MAX;
}

void Renderer::RenderImGui()
{
	// next subpass
	Data.CommandBuffer.nextSubpass(vk::SubpassContents::eInline);
	//Data.CommandBuffer.reset();

	//{
	//	vk::CommandBufferInheritanceInfo inheritanceInfo;
	//	inheritanceInfo
	//		.setFramebuffer(Renderer::Data.SwapchainFramebuffers[Renderer::GetCurrentSwapchainIndex()])
	//		.setRenderPass(Renderer::Data.RenderPass)
	//		.setSubpass(Data.ImGuiSubpass);

	//	vk::CommandBufferBeginInfo info;
	//	info.setPInheritanceInfo(&inheritanceInfo)
	//		.setFlags(
	//			vk::CommandBufferUsageFlagBits::eRenderPassContinue //|
	//			//vk::CommandBufferUsageFlagBits::eSimultaneousUse
	//		);

	//	Data.CommandBuffer.begin(info);
	//}

	// collect render data
	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();
	bool minimized = drawData->DisplaySize.x <= 0 || drawData->DisplaySize.y <= 0;

	// render
	if (!minimized)
		ImGui_ImplVulkan_RenderDrawData(drawData, Data.CommandBuffer);
}

vk::CommandBuffer Renderer::CreateSecondaryCommandBuffer()
{
	vk::CommandBufferAllocateInfo info;
	info.setCommandBufferCount(1)
		.setCommandPool(Data.CommandPool)
		.setLevel(vk::CommandBufferLevel::eSecondary);

	auto buffer = Data.Device.allocateCommandBuffers(info).front();

	if (!buffer)
	{
		SPDLOG_ERROR("Failed to allocate secondary command buffer!");
		return {};
	}

	Data.SecondaryCommandBuffers.push_back(buffer);

	return buffer;
}
