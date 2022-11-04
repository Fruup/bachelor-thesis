#include <engine/hzpch.h>

#include "ExternalImage.h"
#include "engine/renderer/Renderer.h"

#include "WindowsSecurityAttributes.h"

#include "engine/renderer/objects/Command.h"
#include "app/Utils.h"

#include <dxgi1_2.h>

// ---------------------------------------------------------------

DWORD g_DXResourceAccess = DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE;

class
{
public:
	operator VkExternalMemoryHandleTypeFlags() { return VkExternalMemoryHandleTypeFlags(_f); }
	operator VkExternalMemoryHandleTypeFlagBits() { return VkExternalMemoryHandleTypeFlagBits(_f.operator unsigned int()); }
	operator vk::ExternalMemoryHandleTypeFlags() { return vk::ExternalMemoryHandleTypeFlags(_f); }

private:
	const vk::ExternalMemoryHandleTypeFlags _f = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
} static g_HandleType;

// ---------------------------------------------------------------

bool ExternalImage::Init(vk::Format format,
						 vk::ImageUsageFlags usage,
						 uint32_t width, uint32_t height,
						 vk::ImageLayout initialLayout)
{
	vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;

	m_Width = width;
	m_Height = height;

	m_Usage = usage;
	m_Format = format;

	m_Layout = initialLayout;

	uint32_t size = 0;
	vk::ImageAspectFlags aspectMask;

	switch (format)
	{
		case vk::Format::eR32G32B32A32Sfloat: {
			m_NumChannels = 4;
			m_BitsPerChannel = 32;
			size = width * height * m_NumChannels * m_BitsPerChannel / 8;
			aspectMask = vk::ImageAspectFlagBits::eColor;
		} break;

		case vk::Format::eD32Sfloat: {
			m_NumChannels = 1;
			m_BitsPerChannel = 32;
			size = width * height * m_NumChannels * m_BitsPerChannel / 8;
			aspectMask = vk::ImageAspectFlagBits::eDepth;
		} break;

		default:
			HZ_ASSERT(false, "Unsupported external image format!");
	}

	// create external memory
	vk::ExternalMemoryImageCreateInfo externalCreateInfo(g_HandleType);

	vk::ImageCreateInfo createInfo;
	createInfo
		.setArrayLayers(1)
		.setExtent({ width, height, 1 })
		.setFormat(format)
		.setImageType(vk::ImageType::e2D)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setMipLevels(1)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(usage)
		.setPNext(&externalCreateInfo);

	// create buffer
	m_Image = Vulkan.Device.createImage(createInfo);
	if (!m_Image)
	{
		SPDLOG_ERROR("Failed to create external image!");
		return false;
	}

	// get memory requirements
	vk::MemoryRequirements memRequirements
		= Vulkan.Device.getImageMemoryRequirements(m_Image);

	m_MemorySize = memRequirements.size;

	// create memory
	WindowsSecurityAttributes winSecurityAttributes;

	VkExportMemoryWin32HandleInfoKHR vulkanExportMemoryWin32HandleInfoKHR = {};
	vulkanExportMemoryWin32HandleInfoKHR.sType =
		VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
	vulkanExportMemoryWin32HandleInfoKHR.pNext = NULL;
	vulkanExportMemoryWin32HandleInfoKHR.pAttributes = &winSecurityAttributes;
	vulkanExportMemoryWin32HandleInfoKHR.dwAccess = g_DXResourceAccess;
	vulkanExportMemoryWin32HandleInfoKHR.name = 0;

	vk::ExportMemoryAllocateInfoKHR vulkanExportMemoryAllocateInfoKHR;
	vulkanExportMemoryAllocateInfoKHR
		.setPNext(&vulkanExportMemoryWin32HandleInfoKHR)
		.setHandleTypes(g_HandleType);

	vk::MemoryAllocateInfo allocInfo;
	allocInfo
		.setAllocationSize(memRequirements.size)
		.setMemoryTypeIndex(Vulkan.FindMemoryType(memRequirements.memoryTypeBits, properties))
		.setPNext(&vulkanExportMemoryAllocateInfoKHR);

	m_Memory = Vulkan.Device.allocateMemory(allocInfo);

	if (!m_Memory)
	{
		SPDLOG_ERROR("Failed to create external image memory!");
		return false;
	}

	Vulkan.Device.bindImageMemory(m_Image, m_Memory, 0);

	// create image view
	vk::ImageViewCreateInfo viewCreateInfo;
	viewCreateInfo
		.setComponents(vk::ComponentMapping{})
		.setFormat(m_Format)
		.setImage(m_Image)
		.setSubresourceRange(vk::ImageSubresourceRange(
			aspectMask,
			0, 1, 0, 1
		))
		.setViewType(vk::ImageViewType::e2D);

	m_ImageView = Vulkan.Device.createImageView(viewCreateInfo);

	if (!m_ImageView)
	{
		SPDLOG_ERROR("Failed to create external image view!");
		return false;
	}

	// create sampler
	vk::SamplerCreateInfo samplerCreateInfo;
	samplerCreateInfo
		.setAnisotropyEnable(false)
		.setCompareEnable(false)
		.setMagFilter(vk::Filter::eNearest)
		.setMinFilter(vk::Filter::eNearest)
		.setMaxLod(1);

	m_Sampler = Vulkan.Device.createSampler(samplerCreateInfo);
	if (!m_Sampler)
	{
		SPDLOG_ERROR("Image sampler creation failed!");
		return false;
	}

	// transition layout to initial layout
	if (initialLayout != vk::ImageLayout::eUndefined)
	{
		vk::AccessFlags dstAccess;
		vk::PipelineStageFlags dstStage;

		switch (initialLayout)
		{
			case vk::ImageLayout::eShaderReadOnlyOptimal: {
				dstAccess = vk::AccessFlagBits::eShaderRead;
				dstStage = vk::PipelineStageFlagBits::eFragmentShader;
			} break;

			case vk::ImageLayout::eTransferDstOptimal: {
				dstAccess = vk::AccessFlagBits::eNone;
				dstStage = vk::PipelineStageFlagBits::eNone;
			} break;

			case vk::ImageLayout::eColorAttachmentOptimal: {
				dstAccess = vk::AccessFlagBits::eColorAttachmentWrite;
				dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			} break;

			default:
				HZ_ASSERT(false, "Unsupported initial layout!");
		}

		OneTimeCommand cmd;
		TransitionImageLayout(cmd,
							  m_Image,
							  aspectMask,
							  vk::ImageLayout::eUndefined,
							  initialLayout,
							  vk::AccessFlagBits::eNone,
							  dstAccess,
							  vk::PipelineStageFlagBits::eTopOfPipe,
							  dstStage);
	}
	
	return true;
}

void ExternalImage::Exit()
{
	if (m_Image)
	{
		Vulkan.Device.destroyImage(m_Image);
		m_Image = nullptr;
	}

	if (m_ImageView)
	{
		Vulkan.Device.destroyImageView(m_ImageView);
		m_ImageView = nullptr;
	}

	if (m_Memory)
	{
		Vulkan.Device.freeMemory(m_Memory);
		m_Memory = nullptr;
	}

	if (m_Sampler)
	{
		Vulkan.Device.destroySampler(m_Sampler);
		m_Sampler = nullptr;
	}
}

HANDLE ExternalImage::GetMemoryHandle() const
{
	HANDLE handle;

	VkMemoryGetWin32HandleInfoKHR vkMemoryGetWin32HandleInfoKHR = {};
	vkMemoryGetWin32HandleInfoKHR.sType =
		VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
	vkMemoryGetWin32HandleInfoKHR.pNext = NULL;
	vkMemoryGetWin32HandleInfoKHR.memory = m_Memory;
	vkMemoryGetWin32HandleInfoKHR.handleType = g_HandleType;

	auto getMemoryHandle =
		PFN_vkGetMemoryWin32HandleKHR(vkGetDeviceProcAddr(Vulkan.Device, "vkGetMemoryWin32HandleKHR"));

	HZ_ASSERT(getMemoryHandle != nullptr,
			  "Failed to load vulkan function 'vkGetMemoryWin32HandleKHR'!");

	getMemoryHandle(Vulkan.Device, &vkMemoryGetWin32HandleInfoKHR, &handle);

	return handle;
}
