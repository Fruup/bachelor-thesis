#pragma once

#include "engine/renderer/objects/Buffer.h"

struct ImageCreateOptions
{
	bool sRGB{ true };
	vk::Filter Filter{ vk::Filter::eNearest };
};

class Image
{
public:
	~Image();

	bool Create(const std::string& filename, const ImageCreateOptions& options = {});
	bool Create(void* data, uint32_t width, uint32_t height, const ImageCreateOptions& options = {});
	bool Create(vk::Extent2D dim, uint32_t numChannels, vk::Format format, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

	void Stage();

	void CopyBufferToImage();
	void TransitionLayout(vk::ImageLayout newLayout);

	void Destroy();

	operator bool()
	{
		return TextureImage && StagingBuffer.Buffer && Memory && View && Sampler;
	}

	vk::Extent2D Dimensions;
	uint32_t NumBytes;
	vk::Format Format;
	vk::ImageLayout Layout;

	vk::Image TextureImage;
	Buffer StagingBuffer;
	vk::DeviceMemory Memory;
	vk::ImageView View;
	vk::Sampler Sampler;
	vk::Filter Filter;
};
