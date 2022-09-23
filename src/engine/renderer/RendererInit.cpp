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

bool Renderer::CreateDepthBuffer()
{
	return DepthBuffer.Create();
}

bool Renderer::CreateRenderPass()
{
	const uint32_t screenIndex = 0;
	const uint32_t depthIndex = 1;
	const uint32_t positionsIndex = 2;
	const uint32_t normalsIndex = 3;

	// attachments
	vk::AttachmentDescription screenAttachment;
	screenAttachment
		.setFormat(SwapchainFormat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	vk::AttachmentDescription depthAttachment;
	depthAttachment
		.setFormat(vk::Format::eD32Sfloat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		//.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
		.setFinalLayout(vk::ImageLayout::eTransferSrcOptimal);

	vk::AttachmentDescription positionsAttachment;
	positionsAttachment
		.setFormat(vk::Format::eR32G32B32A32Sfloat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

	vk::AttachmentDescription normalsAttachment;
	normalsAttachment
		.setFormat(vk::Format::eR32G32B32A32Sfloat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

	// subpasses

	vk::SubpassDescription depthSubpass;
	{
		vk::AttachmentReference depth(depthIndex, vk::ImageLayout::eDepthStencilAttachmentOptimal);

		depthSubpass
			.setPDepthStencilAttachment(&depth)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	}

	vk::SubpassDescription rayMarchSubpass;
	{
		vk::AttachmentReference input(depthIndex, vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal);

		vk::AttachmentReference color[] = {
			vk::AttachmentReference(positionsIndex, vk::ImageLayout::eColorAttachmentOptimal),
			vk::AttachmentReference(normalsIndex, vk::ImageLayout::eColorAttachmentOptimal),
		};

		rayMarchSubpass
			.setInputAttachments(input)
			.setColorAttachments(color)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	}

	vk::SubpassDescription compositionSubpass;
	{
		vk::AttachmentReference input[] = {
			vk::AttachmentReference(positionsIndex, vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::AttachmentReference(normalsIndex, vk::ImageLayout::eShaderReadOnlyOptimal),
		};

		vk::AttachmentReference color[] = {
			vk::AttachmentReference(screenIndex, vk::ImageLayout::eColorAttachmentOptimal),
		};

		compositionSubpass
			.setInputAttachments(input)
			.setColorAttachments(color)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	}

	vk::SubpassDescription imguiSubpass;
	{
		vk::AttachmentReference color[] = {
			vk::AttachmentReference(screenIndex, vk::ImageLayout::eColorAttachmentOptimal),
		};

		imguiSubpass
			.setColorAttachments(color)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	}

	std::array<vk::SubpassDescription, 4> subpasses = {
		depthSubpass,
		rayMarchSubpass,
		compositionSubpass,
		imguiSubpass,
	};

	// dependencies
	vk::SubpassDependency depthDependency;
	depthDependency
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setDstSubpass(0)
		.setSrcStageMask(vk::PipelineStageFlagBits::eAllGraphics)
		.setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
		.setSrcAccessMask(vk::AccessFlagBits::eNone)
		.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
		.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

	vk::SubpassDependency rayMarchDependency;
	rayMarchDependency
		.setSrcSubpass(0)
		.setDstSubpass(1)
		.setSrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
		.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

	vk::SubpassDependency compositionDependency;
	compositionDependency
		.setSrcSubpass(1)
		.setDstSubpass(2)
		.setSrcStageMask(vk::PipelineStageFlagBits::eAllGraphics)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eColorAttachmentWrite)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
		.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

	vk::SubpassDependency imguiDependency;
	imguiDependency
		.setSrcSubpass(2)
		.setDstSubpass(3)
		.setSrcStageMask(vk::PipelineStageFlagBits::eAllGraphics)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eColorAttachmentWrite)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
		.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

	std::array<vk::SubpassDependency, 4> dependencies = {
		depthDependency,
		rayMarchDependency,
		compositionDependency,
		imguiDependency,
	};

	// create render pass
	{
		std::vector<vk::AttachmentDescription> attachments = {
			screenAttachment,
			depthAttachment,
			positionsAttachment,
			normalsAttachment,
		};

		vk::RenderPassCreateInfo info;
		info.setAttachments(attachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		RenderPass = Device.createRenderPass(info);
		if (!RenderPass)
		{
			SPDLOG_ERROR("Failed to create render pass!");
			return false;
		}
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
				Renderer::GetInstance().GetSwapchainExtent().width,
				Renderer::GetInstance().GetSwapchainExtent().height,
				1 })
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

bool Renderer::CreateFramebuffers()
{
	// create framebuffer images
	/*{
		CreateFramebufferAttachment(
			vk::Format::eR32G32B32A32Sfloat,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			Data.PositionsAttachment.Image,
			Data.PositionsAttachment.ImageView,
			Data.PositionsAttachment.Memory
		);
		
		CreateFramebufferAttachment(
			vk::Format::eR32G32B32A32Sfloat,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			Data.NormalsAttachment.Image,
			Data.NormalsAttachment.ImageView,
			Data.NormalsAttachment.Memory
		);
	}*/

	SwapchainFramebuffers.resize(SwapchainImages.size());

	for (size_t i = 0; i < SwapchainFramebuffers.size(); i++)
	{
		std::vector<vk::ImageView> attachments = {
			SwapchainImageViews[i],
			DepthBuffer.GetView(),
			/*PositionsAttachment.ImageView,
			NormalsAttachment.ImageView,*/
		};

		vk::FramebufferCreateInfo info;
		info.setAttachments(attachments)
			.setRenderPass(RenderPass)
			.setWidth(SwapchainExtent.width)
			.setHeight(SwapchainExtent.height)
			.setLayers(1);

		SwapchainFramebuffers[i] = Device.createFramebuffer(info);
		if (!SwapchainFramebuffers[i])
		{
			SPDLOG_ERROR("Swapchain framebuffer creation failed!");
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

// ich habe den besten freund aus hu .

bool Renderer::InitVulkan()
{
	DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
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
	if (!CreateDepthBuffer()) return false;
	if (!CreateRenderPass()) return false;
	if (!CreateDescriptorPool()) return false;
	if (!CreateFramebuffers()) return false;
	if (!CreateCommandPool()) return false;
	if (!CreateCommandBuffer()) return false;
	if (!CreateSemaphores()) return false;

	return true;
}

void Renderer::ExitVulkan()
{
	Device.destroySemaphore(RenderFinishedSemaphore);
	Device.destroySemaphore(ImageAvailableSemaphore);
	Device.destroyCommandPool(CommandPool);
	Device.destroyDescriptorPool(DescriptorPool);
	for (auto& framebuffer : SwapchainFramebuffers)
		Device.destroyFramebuffer(framebuffer);
	DepthBuffer.Destroy();
	Device.destroyRenderPass(RenderPass);
	for (auto& view : SwapchainImageViews)
		Device.destroyImageView(view);
	Command::Exit();
	Device.destroySwapchainKHR(Swapchain);
	Device.destroy();
	Instance.destroySurfaceKHR(Surface);
	Instance.destroy();
}
