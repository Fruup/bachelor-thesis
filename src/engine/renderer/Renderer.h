#pragma once

#include <optional>
#include <glm/glm.hpp>
#include <glfw/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "engine/renderer/objects/Buffer.h"
#include "engine/renderer/objects/Shader.h"
#include "engine/renderer/objects/Image.h"
#include "engine/renderer/objects/DepthBuffer.h"

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>

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

struct RendererData
{
	GLFWwindow* Window = nullptr;

	std::vector<const char*> ValidationLayers;
	std::vector<const char*> DeviceExtensions;

	vk::Instance Instance;
	vk::PhysicalDevice PhysicalDevice;
	vk::Device Device;

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
	std::vector<vk::CommandBuffer> SecondaryCommandBuffers;

	vk::RenderPass RenderPass;

	vk::Semaphore ImageAvailableSemaphore;
	vk::Semaphore RenderFinishedSemaphore;

	DepthBuffer DepthBuffer;

	Buffer DepthImageCPU;

	struct Texture
	{
		vk::Image Image;
		vk::ImageView ImageView;
		vk::DeviceMemory Memory;
	} NormalsAttachment, PositionsAttachment;

	// ImGui
	uint32_t ImGuiSubpass = 3;
};

class Renderer
{
public:
	static bool Init(int width, int height, const char* title);
	static void Exit();

	static void Begin();
	static void End();

	static uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
	static GLFWwindow* GetWindow() { return Data.Window; }
	static uint32_t GetCurrentSwapchainIndex() { return Data.CurrentImageIndex; }
	static vk::Extent2D GetSwapchainExtent() { return Data.SwapchainExtent; }

	static vk::CommandBuffer CreateSecondaryCommandBuffer();

	static RendererData Data;

private:
	static bool InitVulkan();
	static void ExitVulkan();

	static void RenderImGui();

	static bool CreateInstance();
	static bool PickPhysicalDevice();
	static bool CreateLogicalDevice();
	static bool CreateSwapChain();
	static bool CreateImageViews();
	static bool CreateDepthBuffer();
	static bool CreateRenderPass();
	static bool CreateDescriptorPool();
	static bool CreateFrambuffers();
	static bool CreateCommandPool();
	static bool CreateCommandBuffer();
	static bool CreateSemaphores();

	static bool IsDeviceSuitable(vk::PhysicalDevice device);
	static QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device);
	static SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device);
};
