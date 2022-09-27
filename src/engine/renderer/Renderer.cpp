#include "engine/hzpch.h"
#include "engine/renderer/Renderer.h"
#include "engine/renderer/objects/Command.h"

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

	ImGuiDescriptorPool = Vulkan.Device.createDescriptorPool(info);
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
	ImGui_ImplGlfw_InitForVulkan(Vulkan.Window, false);

	// vulkan
	ImGui_ImplVulkan_InitInfo initInfo{
		Vulkan.Instance,
		Vulkan.PhysicalDevice,
		Vulkan.Device,
		Vulkan.QueueIndices.GraphicsFamily.value(),
		Vulkan.GraphicsQueue,
		nullptr,
		ImGuiDescriptorPool,
		/* subpass */ 0,
		uint32_t(Vulkan.SwapchainImages.size()),
		uint32_t(Vulkan.SwapchainImages.size()),
		VK_SAMPLE_COUNT_1_BIT,
		nullptr,
		nullptr
	};

	if (!ImGui_ImplVulkan_Init(&initInfo, Vulkan.ImGuiRenderPass.RenderPass))
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

	// styling
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	return true;
}

void ExitImGui()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (ImGuiDescriptorPool)
	{
		Vulkan.Device.destroyDescriptorPool(ImGuiDescriptorPool);
		ImGuiDescriptorPool = nullptr;
	}
}

bool CenterGlfwWindow()
{
	GLFWwindow* window = Vulkan.Window;

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

bool Renderer::Init(int width, int height, int pixelSize, const char* title)
{
	SwapchainExtent.setWidth(width);
	SwapchainExtent.setHeight(height);

	// glfw
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwSetErrorCallback(GlfwErrorCallback);

	Window = glfwCreateWindow(
		pixelSize * width, pixelSize * height, title,
		nullptr, nullptr
	);

	CenterGlfwWindow();

	if (!Window)
	{
		SPDLOG_ERROR("Failed to create window!");
		return false;
	}

	if (!InitVulkan())
	{
		SPDLOG_ERROR("Failed to initialize vulkan");
		return false;
	}

	ImGuiRenderPass.Init();

	if (!InitImGui())
	{
		SPDLOG_ERROR("Failed to initialize ImGui!");
		return false;
	}

	return true;
}

void Renderer::Exit()
{
	Device.waitIdle();

	ImGuiRenderPass.Exit();

	ExitImGui();
	ExitVulkan();

	if (Window)
		glfwDestroyWindow(Window);
	Window = nullptr;
	glfwTerminate();
}

void Renderer::Begin()
{
	// acquire image from swap chain
	CurrentImageIndex = Device.acquireNextImageKHR(Swapchain, UINT64_MAX, ImageAvailableSemaphore).value;

	// begin imgui
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// begin command buffer
	CommandBuffer.reset();

	vk::CommandBufferBeginInfo beginInfo;
	CommandBuffer.begin(beginInfo);
}

void Renderer::End()
{
	// render imgui pass
	RenderImGui();

	// end command buffer
	CommandBuffer.end();

	// submit
	Submit(true, true);

	WaitForRenderingFinished();
	GraphicsQueue.waitIdle();

	// PRESENT!
	vk::PresentInfoKHR presentInfo;
	presentInfo
		.setWaitSemaphores(RenderFinishedSemaphore)
		.setSwapchains(Swapchain)
		.setImageIndices(CurrentImageIndex);

	if (PresentationQueue.presentKHR(presentInfo) != vk::Result::eSuccess)
		SPDLOG_WARN("Presentation failed!");

	// ...and wait
	PresentationQueue.waitIdle();

	CurrentImageIndex = UINT32_MAX;
}

void Renderer::Submit(bool wait, bool signal)
{
	vk::SubmitInfo info;
	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	info
		.setWaitSemaphores({})
		.setCommandBuffers(CommandBuffer);

	if (wait)
	{
		info.setWaitSemaphores(ImageAvailableSemaphore)
			.setWaitDstStageMask(waitStage);
	}

	if (signal)
		info.setSignalSemaphores(RenderFinishedSemaphore);

	GraphicsQueue.submit(info, RenderFinishedFence);
}

void Renderer::WaitForRenderingFinished()
{
	// GraphicsQueue.waitIdle();

	Device.waitForFences(RenderFinishedFence, true, UINT64_MAX);
	Device.resetFences(RenderFinishedFence);
}

void Renderer::RenderImGui()
{
	// start render pass
	ImGuiRenderPass.Begin();

	// collect render data
	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();
	bool minimized = drawData->DisplaySize.x <= 0 || drawData->DisplaySize.y <= 0;

	// render
	if (!minimized)
		ImGui_ImplVulkan_RenderDrawData(drawData, CommandBuffer);

	// end render pass
	ImGuiRenderPass.End();
}
