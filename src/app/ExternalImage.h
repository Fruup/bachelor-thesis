#pragma once

// This is a vulkan image that can be imported and manipulated by CUDA.

class ExternalImage
{
public:
	bool Init(vk::Format format,
			  vk::ImageUsageFlags usage,
			  uint32_t width, uint32_t height,
			  vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined);

	void Exit();

	HANDLE GetMemoryHandle() const;

	vk::Image m_Image;
	vk::ImageView m_ImageView;
	vk::DeviceMemory m_Memory;
	vk::Sampler m_Sampler;

	vk::ImageUsageFlags m_Usage;
	vk::Format m_Format;
	vk::ImageLayout m_Layout;

	uint32_t m_Width, m_Height;
	uint32_t m_MemorySize;

	uint32_t m_NumChannels;
	uint32_t m_BitsPerChannel;
};
