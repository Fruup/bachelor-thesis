#pragma once

#include <optional>
#include <glm/glm.hpp>
#include <glfw/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "engine/renderer/objects/Buffer.h"
#include "engine/renderer/objects/Shader.h"
#include "engine/renderer/objects/Image.h"
#include "engine/renderer/objects/DepthBuffer.h"
#include "engine/camera/EditorCameraController3D.h"

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

class Renderer
{
public:
	static Renderer& GetInstance();

	bool Init(int width, int height, const char* title);
	void Exit();

	virtual bool VInit() { return true; }
	virtual void VExit() {}

	virtual void Begin();
	virtual void End();

	virtual void Render() {}
	virtual void RenderUI() {}

	virtual void Update(float time)
	{
		CameraController.Update(time);
	}

	virtual void HandleEvent(Event& e)
	{
		CameraController.HandleEvent(e);
	}

	static uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

	GLFWwindow* GetWindow() { return Window; }
	uint32_t GetCurrentSwapchainIndex() { return CurrentImageIndex; }
	vk::Extent2D GetSwapchainExtent() { return SwapchainExtent; }

	vk::CommandBuffer CreateSecondaryCommandBuffer();

private:
	bool InitVulkan();
	void ExitVulkan();

	void RenderImGui();

	bool CreateInstance();
	bool PickPhysicalDevice();
	bool CreateLogicalDevice();
	bool CreateSwapChain();
	bool CreateSwapChainImageViews();

	virtual bool CreateDepthBuffer();
	virtual bool CreateRenderPass();
	virtual bool CreateDescriptorPool();
	virtual bool CreateFramebuffers();
	virtual bool CreateCommandPool();
	virtual bool CreateCommandBuffer();

	bool CreateSemaphores();

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

	// camera
	Camera3D Camera;
	EditorCameraController3D CameraController;

	// ImGui
	uint32_t ImGuiSubpass = uint32_t(-1);

protected:
	Renderer();
};
