#include <engine/hzpch.h>

#include "BilateralBuffer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>

// -------------------------------------------------------------------------

//auto& Vulkan = Renderer::GetInstance();

// -------------------------------------------------------------------------
// PUBLIC FUNCTIONS

void BilateralBuffer::Init(_Type type)
{
	Type = type;

	switch (Type)
	{
		case Color:
		{
			Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
			Format = vk::Format::eR32G32B32A32Sfloat;
			AspectFlags = vk::ImageAspectFlagBits::eColor;
			//GPU.Layout = vk::ImageLayout::eColorAttachmentOptimal;
			Size =
				4 * sizeof(float) *
				Vulkan.SwapchainExtent.width *
				Vulkan.SwapchainExtent.height;
		} break;

		case Depth:
		{
			Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
			Format = vk::Format::eD32Sfloat;
			AspectFlags = vk::ImageAspectFlagBits::eDepth;
			//GPU.Layout = vk::ImageLayout::eDepthAttachmentOptimal;
			Size =
				sizeof(float) *
				Vulkan.SwapchainExtent.width *
				Vulkan.SwapchainExtent.height;
		} break;

		default:
			HZ_ASSERT(false, "");
	}

	GPU.Layout = vk::ImageLayout::eUndefined;

	CreateCPUSide();
	CreateGPUSide();
}

void BilateralBuffer::Exit()
{
	Vulkan.Device.destroyBuffer(CPU.Buffer);
	Vulkan.Device.freeMemory(CPU.Memory);

	Vulkan.Device.destroyImage(GPU.Image);
	Vulkan.Device.destroyImageView(GPU.ImageView);
	Vulkan.Device.freeMemory(GPU.Memory);
	Vulkan.Device.destroySampler(GPU.Sampler);
}

void BilateralBuffer::CopyToGPU()
{
	OneTimeCommand cmd;
	vk::BufferImageCopy region;

	region
		.setBufferImageHeight(0)
		.setBufferOffset(0)
		.setBufferRowLength(0)
		.setImageOffset(0)
		.setImageSubresource(vk::ImageSubresourceLayers(
			AspectFlags, // aspect mask
			0, // mip map level
			0, // base layer
			1 // layer count
		))
		.setImageExtent(vk::Extent3D(
			Vulkan.SwapchainExtent.width,
			Vulkan.SwapchainExtent.height,
			1
		));

	cmd->copyBufferToImage(
		CPU.Buffer,
		GPU.Image,
		vk::ImageLayout::eTransferDstOptimal,
		region);
}

void BilateralBuffer::CopyFromGPU()
{
	OneTimeCommand cmd;
	vk::BufferImageCopy region;

	region
		.setBufferImageHeight(0)
		.setBufferOffset(0)
		.setBufferRowLength(0)
		.setImageOffset(0)
		.setImageSubresource(vk::ImageSubresourceLayers(
			AspectFlags, // aspect mask
			0, // mip map level
			0, // base layer
			1 // layer count
		))
		.setImageExtent(vk::Extent3D(
			Vulkan.SwapchainExtent.width,
			Vulkan.SwapchainExtent.height,
			1
		));

	cmd->copyImageToBuffer(
		GPU.Image,
		vk::ImageLayout::eTransferSrcOptimal,
		CPU.Buffer,
		region);
}

void* BilateralBuffer::MapCPUMemory(vk::DeviceSize offset, vk::DeviceSize size)
{
	return Vulkan.Device.mapMemory(CPU.Memory, offset, size);
}

void BilateralBuffer::UnmapCPUMemory()
{
	Vulkan.Device.unmapMemory(CPU.Memory);
}

// -------------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void BilateralBuffer::CreateCPUSide()
{
	// buffer

	vk::BufferCreateInfo bufferCreateInfo;
	bufferCreateInfo
		.setSharingMode(vk::SharingMode::eExclusive)
		.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc)
		.setSize(Size);

	CPU.Buffer = Vulkan.Device.createBuffer(bufferCreateInfo);

	// memory

	auto requirements = Vulkan.Device.getBufferMemoryRequirements(CPU.Buffer);

	vk::MemoryAllocateInfo allocInfo;
	allocInfo.setAllocationSize(requirements.size)
		.setMemoryTypeIndex(Vulkan.FindMemoryType(
			requirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eHostCoherent |
			vk::MemoryPropertyFlagBits::eHostVisible
		));

	CPU.Memory = Vulkan.Device.allocateMemory(allocInfo);

	// bind memory

	Vulkan.Device.bindBufferMemory(CPU.Buffer, CPU.Memory, 0);
}

void BilateralBuffer::CreateGPUSide()
{
	// image

	vk::ImageCreateInfo imageCreateInfo;
	imageCreateInfo
		.setExtent(vk::Extent3D(
			Vulkan.SwapchainExtent.width,
			Vulkan.SwapchainExtent.height,
			1
		))
		.setFormat(Format)
		.setQueueFamilyIndices(Vulkan.QueueIndices.GraphicsFamily.value())
		.setImageType(vk::ImageType::e2D)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setMipLevels(1)
		.setArrayLayers(1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(
			Usage |
			vk::ImageUsageFlagBits::eTransferSrc |
			vk::ImageUsageFlagBits::eTransferDst)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);

	GPU.Image = Vulkan.Device.createImage(imageCreateInfo);

	// memory

	auto requirements = Vulkan.Device.getImageMemoryRequirements(GPU.Image);

	vk::MemoryAllocateInfo allocInfo;
	allocInfo.setAllocationSize(requirements.size)
		.setMemoryTypeIndex(Vulkan.FindMemoryType(
			requirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		));

	GPU.Memory = Vulkan.Device.allocateMemory(allocInfo);

	// bind memory

	Vulkan.Device.bindImageMemory(GPU.Image, GPU.Memory, 0);

	// view

	vk::ImageViewCreateInfo viewCreateInfo;
	viewCreateInfo
		.setComponents(vk::ComponentMapping())
		.setFormat(Format)
		.setImage(GPU.Image)
		.setViewType(vk::ImageViewType::e2D)
		.setSubresourceRange(vk::ImageSubresourceRange(
			AspectFlags, // aspect mask
			0, // mip map level
			1, // level count
			0, // base array layer
			1  // layer count
		));

	GPU.ImageView = Vulkan.Device.createImageView(viewCreateInfo);

	// sampler

	vk::SamplerCreateInfo samplerCreateInfo;
	samplerCreateInfo
		.setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
		.setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
		.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
		.setAnisotropyEnable(false)
		.setMaxAnisotropy(0)
		.setCompareEnable(false)
		.setMagFilter(vk::Filter::eLinear)
		.setMinFilter(vk::Filter::eLinear)
		.setMaxLod(1.0f)
		.setMinLod(1.0f)
		.setMipLodBias(0.0f)
		.setMipmapMode(vk::SamplerMipmapMode::eLinear)
		.setUnnormalizedCoordinates(false);

	GPU.Sampler = Vulkan.Device.createSampler(samplerCreateInfo);
}

void BilateralBuffer::TransitionLayout(vk::ImageLayout newLayout,
									   vk::AccessFlags accessMask,
									   vk::PipelineStageFlags stage)
{
	if (GPU.Layout == newLayout) return;

	vk::ImageLayout oldLayout = GPU.Layout;
	GPU.Layout = newLayout;

	/*vk::AccessFlagBits accessMask;
	switch (newLayout)
	{
		case vk::ImageLayout::eTransferDstOptimal:
			accessMask = vk::AccessFlagBits::eTransferWrite;
			break;

		case vk::ImageLayout::eTransferSrcOptimal:
			accessMask = vk::AccessFlagBits::eTransferRead;
			break;

		default:
			HZ_ASSERT(false, "");
	}*/

	vk::ImageMemoryBarrier barrier;
	barrier
		.setImage(GPU.Image)
		.setOldLayout(oldLayout)
		.setNewLayout(GPU.Layout)
		.setDstAccessMask(accessMask)
		.setSubresourceRange(vk::ImageSubresourceRange(
			AspectFlags, // aspect mask
			0, // mip map level
			1, // level count
			0, // base array layer
			1  // layer count
		));

	Vulkan.CommandBuffer.pipelineBarrier(
		stage,
		stage,
		{}, // dependency flags
		{}, // memory barriers
		{}, // buffer memory barriers
		barrier // image memory barrier
	);
}
