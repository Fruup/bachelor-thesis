#include <engine/hzpch.h>
#include <engine/renderer/objects/DepthBuffer.h>
#include <engine/renderer/Renderer.h>

bool DepthBuffer::Create()
{
	// image
	{
		vk::ImageCreateInfo info;
		info.setExtent({
				Renderer::GetInstance().GetSwapchainExtent().width,
				Renderer::GetInstance().GetSwapchainExtent().height,
				1 })
			.setFormat(vk::Format::eD32Sfloat)
			.setQueueFamilyIndices(Renderer::GetInstance().QueueIndices.GraphicsFamily.value())
			.setImageType(vk::ImageType::e2D)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setMipLevels(1)
			.setArrayLayers(1)
			.setTiling(vk::ImageTiling::eOptimal)
			.setUsage(
				vk::ImageUsageFlagBits::eDepthStencilAttachment |
				vk::ImageUsageFlagBits::eInputAttachment |
				vk::ImageUsageFlagBits::eTransferSrc
			)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1);

		m_Image = Renderer::GetInstance().Device.createImage(info);
		if (!m_Image)
		{
			SPDLOG_ERROR("Depth buffer image creation failed!");
			return false;
		}
	}

	// memory
	{
		auto requirements = Renderer::GetInstance().Device.getImageMemoryRequirements(m_Image);

		vk::MemoryAllocateInfo allocInfo{
			requirements.size,
			Renderer::FindMemoryType(requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
		};

		m_Memory = Renderer::GetInstance().Device.allocateMemory(allocInfo);
		if (!m_Memory)
		{
			SPDLOG_ERROR("Depth buffer memory creation failed!");
			return false;
		}

		Renderer::GetInstance().Device.bindImageMemory(m_Image, m_Memory, 0);
	}

	// create image view
	{
		vk::ComponentMapping mapping{};
		vk::ImageViewCreateInfo viewInfo;
		viewInfo
			.setImage(m_Image)
			.setFormat(vk::Format::eD32Sfloat)
			.setComponents(mapping)
			.setViewType(vk::ImageViewType::e2D)
			.setSubresourceRange(vk::ImageSubresourceRange{
				vk::ImageAspectFlagBits::eDepth,
				0, 1, 0, 1
			});

		m_View = Renderer::GetInstance().Device.createImageView(viewInfo);
		if (!m_View)
		{
			SPDLOG_ERROR("Depth buffer view creation failed!");
			return false;
		}
	}

	return true;
}

void DepthBuffer::Destroy()
{
	if (m_Image)
	{
		Renderer::GetInstance().Device.destroyImage(m_Image);
		m_Image = nullptr;
	}

	if (m_View)
	{
		Renderer::GetInstance().Device.destroyImageView(m_View);
		m_View = nullptr;
	}

	if (m_Memory)
	{
		Renderer::GetInstance().Device.freeMemory(m_Memory);
		m_Memory = nullptr;
	}
}
