#include "engine/hzpch.h"
#include "engine/renderer/Renderer.h"
#include "engine/renderer/objects/Command.h"

#include <fstream>

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

	ImGuiDescriptorPool = Renderer::GetInstance().Device.createDescriptorPool(info);
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
	ImGui_ImplGlfw_InitForVulkan(Renderer::GetInstance().GetWindow(), false);

	// vulkan
	ImGui_ImplVulkan_InitInfo initInfo{
		Renderer::GetInstance().Instance,
		Renderer::GetInstance().PhysicalDevice,
		Renderer::GetInstance().Device,
		Renderer::GetInstance().QueueIndices.GraphicsFamily.value(),
		Renderer::GetInstance().GraphicsQueue,
		nullptr,
		ImGuiDescriptorPool,
		/* subpass */ Renderer::GetInstance().ImGuiSubpass,
		uint32_t(Renderer::GetInstance().SwapchainImages.size()),
		uint32_t(Renderer::GetInstance().SwapchainImages.size()),
		VK_SAMPLE_COUNT_1_BIT,
		nullptr,
		nullptr
	};

	if (!ImGui_ImplVulkan_Init(&initInfo, Renderer::GetInstance().RenderPass))
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
		Renderer::GetInstance().Device.destroyDescriptorPool(ImGuiDescriptorPool);
		ImGuiDescriptorPool = nullptr;
	}
}

bool CenterGlfwWindow()
{
	GLFWwindow* window = Renderer::GetInstance().GetWindow();

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

	Window = glfwCreateWindow(
		width, height, title,
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

	if (!VInit())
		return false;

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

	VExit();

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

	// begin command buffer
	{
		CommandBuffer.reset();

		vk::CommandBufferBeginInfo info;
		CommandBuffer.begin(info);
	}

	// begin render pass
	std::array<vk::ClearValue, 2> clearValues = {
		// screen
		vk::ClearColorValue(std::array<float, 4>{ .1f, .1f, .1f, 1 }),

		// depth
		vk::ClearDepthStencilValue{ 1.0f },

		// positions
		//vk::ClearColorValue(std::array<float, 4>{ 0, 0, 0, 0 }),

		// normals
		//vk::ClearColorValue(std::array<float, 4>{ 0, 0, 0, 0 }),
	};

	vk::RenderPassBeginInfo renderPassInfo;
	renderPassInfo
		.setClearValues(clearValues)
		.setFramebuffer(SwapchainFramebuffers[GetCurrentSwapchainIndex()])
		.setRenderArea({ { 0, 0 }, SwapchainExtent })
		.setRenderPass(RenderPass);

	CommandBuffer.beginRenderPass(
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
	// end imgui
	RenderImGui();

	// end render pass and command buffer
	CommandBuffer.endRenderPass();

	CommandBuffer.end();

	// submit
	{
		vk::SubmitInfo info;
		vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		info.setWaitSemaphores(ImageAvailableSemaphore)
			.setWaitDstStageMask(waitStage)
			.setCommandBuffers(CommandBuffer)
			.setSignalSemaphores(RenderFinishedSemaphore);

		GraphicsQueue.submit(info);
		GraphicsQueue.waitIdle();
	}

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

void Renderer::RenderImGui()
{
	// next subpass
	CommandBuffer.nextSubpass(vk::SubpassContents::eInline);

	// collect render data
	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();
	bool minimized = drawData->DisplaySize.x <= 0 || drawData->DisplaySize.y <= 0;

	// render
	if (!minimized)
		ImGui_ImplVulkan_RenderDrawData(drawData, CommandBuffer);
}

vk::CommandBuffer Renderer::CreateSecondaryCommandBuffer()
{
	vk::CommandBufferAllocateInfo info;
	info.setCommandBufferCount(1)
		.setCommandPool(CommandPool)
		.setLevel(vk::CommandBufferLevel::eSecondary);

	auto buffer = Device.allocateCommandBuffers(info).front();

	if (!buffer)
	{
		SPDLOG_ERROR("Failed to allocate secondary command buffer!");
		return {};
	}

	SecondaryCommandBuffers.push_back(buffer);

	return buffer;
}
