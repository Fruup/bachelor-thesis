#include "engine/hzpch.h"
#include "engine/renderer/objects/Image.h"
#include "engine/renderer/Renderer.h"
#include "engine/utils/ImageCPU.h"
#include "engine/renderer/objects/Command.h"

bool Image::Create(const std::string& filename, const ImageCreateOptions& options)
{
	Filter = options.Filter;

	ImageCPU img(filename);
	vk::Format format;

	switch (img.NumChannels)
	{
	case 4:
	{
		if (options.sRGB) format = vk::Format::eR8G8B8A8Srgb;
		else format = vk::Format::eR8G8B8A8Unorm;
	} break;

	default:
		HZ_ASSERT(false, "Invalid number of channels on image '{}'!", filename.c_str());
	}

	Create(
		vk::Extent2D{ img.Width, img.Height },
		img.NumChannels,
		format,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	// map buffer
	StagingBuffer.Map(img.Data.data(), NumBytes);

	Stage();

	return true;
}

bool Image::Create(void* data, uint32_t width, uint32_t height, const ImageCreateOptions& options)
{
	Filter = options.Filter;

	// create
	Create(
		{ width, height },
		4,
		options.sRGB ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	// map and stage
	StagingBuffer.Map(data, NumBytes);
	Stage();

	return true;
}

bool Image::Create(vk::Extent2D dim, uint32_t numChannels, vk::Format format, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	Destroy();

	Dimensions = dim;
	Format = format;
	NumBytes = numChannels * dim.width * dim.height;
	Layout = vk::ImageLayout::eUndefined;

	// buffer
	StagingBuffer.Create(
		NumBytes,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// image
	vk::ImageCreateInfo info;
	info.setExtent({ Dimensions.width, Dimensions.height, 1 })
		.setFormat(format)
		.setQueueFamilyIndices(Renderer::Data.QueueIndices.GraphicsFamily.value())
		.setImageType(vk::ImageType::e2D)
		.setInitialLayout(Layout)
		.setMipLevels(1)
		.setArrayLayers(1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);

	TextureImage = Renderer::Data.Device.createImage(info);
	if (!TextureImage)
	{
		SPDLOG_ERROR("Texture image creation failed!");
		return false;
	}

	// memory
	auto requirements = Renderer::Data.Device.getImageMemoryRequirements(TextureImage);

	vk::MemoryAllocateInfo allocInfo{
		requirements.size,
		Renderer::FindMemoryType(requirements.memoryTypeBits, properties)
	};
	
	Memory = Renderer::Data.Device.allocateMemory(allocInfo);
	if (!Memory)
	{
		SPDLOG_ERROR("Texture memory creation failed!");
		return false;
	}

	Renderer::Data.Device.bindImageMemory(TextureImage, Memory, 0);

	// create image view
	vk::ComponentMapping mapping{};
	vk::ImageViewCreateInfo viewInfo;
	viewInfo
		.setImage(TextureImage)
		.setFormat(Format)
		.setComponents(mapping)
		.setViewType(vk::ImageViewType::e2D)
		.setSubresourceRange(vk::ImageSubresourceRange{
			vk::ImageAspectFlagBits::eColor,
			0,
			1,
			0,
			1
		});

	View = Renderer::Data.Device.createImageView(viewInfo);
	if (!View)
	{
		SPDLOG_ERROR("Image view creation failed!");
		return false;
	}

	// sampler
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo
		//.setAnisotropyEnable(false)
		.setAnisotropyEnable(true)
		.setMaxAnisotropy(Renderer::Data.PhysicalDevice.getProperties().limits.maxSamplerAnisotropy)
		.setCompareEnable(false)
		.setMagFilter(Filter)
		.setMinFilter(Filter);

	Sampler = Renderer::Data.Device.createSampler(samplerInfo);
	if (!Sampler)
	{
		SPDLOG_ERROR("Image sampler creation failed!");
		return false;
	}

	return true;
}

void Image::Stage()
{
	TransitionLayout(vk::ImageLayout::eTransferDstOptimal);
	CopyBufferToImage();
	TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
}

void Image::CopyBufferToImage()
{
	auto cmd = Command::BeginOneTimeCommand();

	// fill command buffer
	vk::BufferImageCopy region;
	region
		.setBufferImageHeight(0)
		.setBufferRowLength(0)
		.setImageExtent({ Dimensions.width, Dimensions.height, 1 })
		.setImageSubresource(vk::ImageSubresourceLayers{
			vk::ImageAspectFlagBits::eColor,
			0,
			0,
			1
		});

	cmd.copyBufferToImage(StagingBuffer.Buffer, TextureImage, vk::ImageLayout::eTransferDstOptimal, region);

	// end
	Command::EndOneTimeCommand(cmd);
}

void Image::TransitionLayout(vk::ImageLayout newLayout)
{
	auto cmd = Command::BeginOneTimeCommand();

	// fill command buffer
	vk::ImageMemoryBarrier barrier;
	barrier
		.setOldLayout(Layout)
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(TextureImage)
		.setSubresourceRange(vk::ImageSubresourceRange{
			vk::ImageAspectFlagBits::eColor,
			0,
			1,
			0,
			1
		});

	vk::PipelineStageFlags srcStage, dstStage;
	if (Layout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier
			.setSrcAccessMask(vk::AccessFlagBits::eNoneKHR)
			.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (
		Layout == vk::ImageLayout::eTransferDstOptimal &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier
			.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else
	{
		SPDLOG_CRITICAL("Image layout transition not supported!");
		throw std::runtime_error("Image layout transition not supported!");
	}

	cmd.pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);

	// end
	Command::EndOneTimeCommand(cmd);

	// update layout
	Layout = newLayout;
}

Image::~Image()
{
	//Destroy();
}

void Image::Destroy()
{
	StagingBuffer.Destroy();

	if (Sampler)
	{
		Renderer::Data.Device.destroySampler(Sampler);
		Sampler = nullptr;
	}

	if (TextureImage)
	{
		Renderer::Data.Device.destroyImage(TextureImage);
		TextureImage = nullptr;
	}

	if (Memory)
	{
		Renderer::Data.Device.freeMemory(Memory);
		Memory = nullptr;
	}

	if (View)
	{
		Renderer::Data.Device.destroyImageView(View);
		View = nullptr;
	}
}
