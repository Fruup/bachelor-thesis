#pragma once

#include <optional>
#include <glm/glm.hpp>
#include <glfw/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#include "objects/Image.h"
#include "ImGuiRenderPass.h"

struct QueueFamilyIndices
{
	std::optional<uint32_t> GraphicsFamily;
	std::optional<uint32_t> PresentFamily;

	bool IsComplete()
	{
		return (
			GraphicsFamily.has_value() &&
			PresentFamily.has_value()
		);
	}
};

struct SwapChainSupportDetails
{
	vk::SurfaceCapabilitiesKHR Caps;
	std::vector<vk::SurfaceFormatKHR> Formats;
	std::vector<vk::PresentModeKHR> PresentModes;
};

class Renderer
{
public:
	static Renderer& GetInstance();

	bool Init(int width, int height, int pixelSize, const char* title);
	void Exit();

	void Begin();
	void End();

	void RenderImGui();

	void Submit(bool wait = false, bool signal = false);
	void Submit(const std::vector<vk::Semaphore>& waitSemaphores,
				const std::vector<vk::PipelineStageFlags>& waitStages,
				const std::vector<vk::Semaphore>& signalSemaphores);

	void WaitForRenderingFinished();

	static uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
	float GetAspect() { return float(SwapchainExtent.width) / float(SwapchainExtent.height); }

	void Screenshot() { m_ShouldScreenshot = true; }

private:
	bool InitVulkan();
	void ExitVulkan();

	bool CreateInstance();
	bool PickPhysicalDevice();
	bool CreateLogicalDevice();
	bool CreateSwapChain();
	bool CreateSwapChainImageViews();

	bool CreateDescriptorPool();
	bool CreateCommandPool();
	bool CreateCommandBuffer();

	bool CreateSemaphores();
	bool CreateFences();

	bool IsDeviceSuitable(vk::PhysicalDevice device);
	QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device);

public:
	GLFWwindow* Window = nullptr;

	std::vector<const char*> ValidationLayers;
	std::vector<const char*> DeviceExtensions;

	vk::Instance Instance;
	vk::PhysicalDevice PhysicalDevice;
	vk::Device Device;

	vk::DebugUtilsMessengerEXT m_DebugMessenger;

	QueueFamilyIndices QueueIndices;
	vk::Queue GraphicsQueue, PresentationQueue;

	vk::SurfaceKHR Surface;
	vk::SwapchainKHR Swapchain;
	std::vector<vk::Image> SwapchainImages;
	std::vector<vk::Framebuffer> SwapchainFramebuffers;
	std::vector<vk::ImageView> SwapchainImageViews;
	vk::Format SwapchainFormat;
	vk::Extent2D SwapchainExtent;
	uint32_t CurrentImageIndex = UINT32_MAX;

	vk::CommandPool CommandPool;
	vk::DescriptorPool DescriptorPool;

	vk::CommandBuffer CommandBuffer;

	vk::Semaphore ImageAvailableSemaphore;
	vk::Semaphore RenderFinishedSemaphore;
	vk::Fence RenderFinishedFence;

	ImGuiRenderPass ImGuiRenderPass;

private:
	bool m_ShouldScreenshot = false;

	vk::Format m_ScreenshotFormat;
	vk::Image m_ScreenshotImage;
	vk::DeviceMemory m_ScreenshotMemory;

	void _Screenshot();
};

//static Renderer& Vulkan = Renderer::GetInstance();
#define Vulkan Renderer::GetInstance()
