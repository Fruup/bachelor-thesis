#include "engine/hzpch.h"

#include "engine/renderer/Renderer.h"
#include "engine/assets/AssetManager.h"

#include "engine/renderer/objects/Shader.h"
#include "engine/renderer/objects/Command.h"

bool Renderer::CreateSwapChainImageViews()
{
	SwapchainImageViews.resize(SwapchainImages.size());

	for (size_t i = 0; i < SwapchainImages.size(); i++)
	{
		vk::ImageViewCreateInfo info;
		info.setImage(SwapchainImages[i])
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(SwapchainFormat)
			.setSubresourceRange(vk::ImageSubresourceRange{
				vk::ImageAspectFlagBits::eColor,
				0, 1, 0, 1
			});

		SwapchainImageViews[i] = Device.createImageView(info);
	}

	return true;
}

bool Renderer::CreateDescriptorPool()
{
	std::array<vk::DescriptorPoolSize, 3> poolSize{
		vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBuffer, 8 },
		vk::DescriptorPoolSize{ vk::DescriptorType::eInputAttachment, 8 },
		vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, 32 },
	};

	vk::DescriptorPoolCreateInfo info;
	info.setPoolSizes(poolSize)
		.setMaxSets(4);

	DescriptorPool = Device.createDescriptorPool(info);
	if (!DescriptorPool)
	{
		SPDLOG_ERROR("Descriptor pool creation failed!");
		return false;
	}

	return true;
}

bool CreateFramebufferAttachment(
	vk::Format format,
	vk::ImageUsageFlags usage,
	vk::Image& image,
	vk::ImageView& view,
	vk::DeviceMemory& memory
)
{
	auto& Device = Renderer::GetInstance().Device;

	// image
	{
		vk::ImageCreateInfo info;
		info.setExtent({
				Renderer::GetInstance().SwapchainExtent.width,
				Renderer::GetInstance().SwapchainExtent.height,
				1
			})
			.setFormat(format)
			.setQueueFamilyIndices(Renderer::GetInstance().QueueIndices.GraphicsFamily.value())
			.setImageType(vk::ImageType::e2D)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setMipLevels(1)
			.setArrayLayers(1)
			.setTiling(vk::ImageTiling::eOptimal)
			.setUsage(usage)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1);

		image = Renderer::GetInstance().Device.createImage(info);
		if (!image)
		{
			SPDLOG_ERROR("Image creation failed!");
			return false;
		}
	}

	// memory
	{
		auto requirements = Device.getImageMemoryRequirements(image);

		vk::MemoryAllocateInfo allocInfo{
			requirements.size,
			Renderer::FindMemoryType(requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
		};

		memory = Device.allocateMemory(allocInfo);
		if (!memory)
		{
			SPDLOG_ERROR("Memory creation failed!");
			return false;
		}

		Device.bindImageMemory(image, memory, 0);
	}

	// create image view
	{
		vk::ComponentMapping mapping{};
		vk::ImageViewCreateInfo viewInfo;
		viewInfo
			.setImage(image)
			.setFormat(format)
			.setComponents(mapping)
			.setViewType(vk::ImageViewType::e2D)
			.setSubresourceRange(vk::ImageSubresourceRange{
				vk::ImageAspectFlagBits::eColor,
				0, 1, 0, 1
			});

		view = Device.createImageView(viewInfo);
		if (!view)
		{
			SPDLOG_ERROR("View creation failed!");
			return false;
		}
	}

	return true;
}

bool Renderer::CreateCommandPool()
{
	CommandPool = Device.createCommandPool({
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		QueueIndices.GraphicsFamily.value()
	});

	if (!CommandPool)
	{
		SPDLOG_ERROR("Command pool creation failed!");
		return false;
	}

	return true;
}

bool Renderer::CreateCommandBuffer()
{
	vk::CommandBufferAllocateInfo info;
	info.setCommandBufferCount(1)
		.setCommandPool(CommandPool)
		.setLevel(vk::CommandBufferLevel::ePrimary);

	CommandBuffer = Device.allocateCommandBuffers(info).front();

	if (!CommandBuffer)
	{
		SPDLOG_ERROR("Failed to allocate primary command buffer!");
		return false;
	}

	return true;
}

bool Renderer::CreateSemaphores()
{
	ImageAvailableSemaphore = Device.createSemaphore({});
	RenderFinishedSemaphore = Device.createSemaphore({});

	if (!RenderFinishedSemaphore || !ImageAvailableSemaphore)
	{
		SPDLOG_ERROR("Semaphore creation failed!");
		return false;
	}

	return true;
}

bool Renderer::CreateFences()
{
	vk::FenceCreateInfo info;
	RenderFinishedFence = Device.createFence(info);

	return RenderFinishedFence;
}

// ich habe den besten freund aus hu .

bool Renderer::InitVulkan()
{
	DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,

		VK_KHR_MAINTENANCE2_EXTENSION_NAME,
		VK_KHR_MULTIVIEW_EXTENSION_NAME,
	};

	if (!CreateInstance()) return false;

	// create window surface
	VkSurfaceKHR surface;
	glfwCreateWindowSurface(Instance, Window, nullptr, &surface);
	Surface = surface;

	if (!Surface)
	{
		SPDLOG_ERROR("Failed to create window surface!");
		return false;
	}

	if (!PickPhysicalDevice()) return false;
	if (!CreateLogicalDevice()) return false;
	if (!Command::Init()) return false;
	if (!CreateSwapChain()) return false;
	if (!CreateSwapChainImageViews()) return false;
	if (!CreateDescriptorPool()) return false;
	if (!CreateCommandPool()) return false;
	if (!CreateCommandBuffer()) return false;
	if (!CreateSemaphores()) return false;
	if (!CreateFences()) return false;

	return true;
}

void Renderer::ExitVulkan()
{
	Device.destroyFence(RenderFinishedFence);
	Device.destroySemaphore(RenderFinishedSemaphore);
	Device.destroySemaphore(ImageAvailableSemaphore);
	Device.destroyCommandPool(CommandPool);
	Device.destroyDescriptorPool(DescriptorPool);
	for (auto& framebuffer : SwapchainFramebuffers)
		Device.destroyFramebuffer(framebuffer);
	for (auto& view : SwapchainImageViews)
		Device.destroyImageView(view);
	Command::Exit();
	Device.destroySwapchainKHR(Swapchain);
	Device.destroy();
	Instance.destroySurfaceKHR(Surface);
	Instance.destroy();
}
