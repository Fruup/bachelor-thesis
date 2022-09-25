#include <engine/hzpch.h>

#include "BilateralBuffer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>

// -------------------------------------------------------------------------

auto& Vulkan = Renderer::GetInstance();

// -------------------------------------------------------------------------
// PUBLIC FUNCTIONS

void BilateralBuffer::Init(_Type type)
{
	Type = type;

	switch (Type)
	{
		case Color:
		{
			Usage = vk::ImageUsageFlagBits::eColorAttachment;
			Format = vk::Format::eR32G32B32A32Sfloat;
			AspectFlags = vk::ImageAspectFlagBits::eColor;
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
			Size =
				sizeof(float) *
				Vulkan.SwapchainExtent.width *
				Vulkan.SwapchainExtent.height;
		} break;
	}

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
}

void BilateralBuffer::PrepareGPULayoutForRendering(vk::CommandBuffer& cmd)
{
	vk::ImageLayout oldLayout = GPU.Layout;
	switch (Type)
	{
		case Color: GPU.Layout = vk::ImageLayout::eColorAttachmentOptimal; break;
		case Depth: GPU.Layout = vk::ImageLayout::eDepthAttachmentOptimal; break;
	}

	vk::AccessFlags accessMask;
	switch (Type)
	{
		case Color: accessMask = vk::AccessFlagBits::eColorAttachmentWrite; break;
		case Depth: accessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite; break;
	}

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

	cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
		vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
		{},
		{}, // memory barriers
		{}, // buffer memory barriers
		barrier // image memory barrier
	);
}

void BilateralBuffer::PrepareGPULayoutForCopying(vk::CommandBuffer& cmd)
{
	vk::ImageLayout oldLayout = GPU.Layout;
	GPU.Layout = vk::ImageLayout::eTransferSrcOptimal;

	vk::ImageMemoryBarrier barrier;
	barrier
		.setImage(GPU.Image)
		.setOldLayout(oldLayout)
		.setNewLayout(GPU.Layout)
		.setDstAccessMask(vk::AccessFlagBits::eTransferRead)
		.setSubresourceRange(vk::ImageSubresourceRange(
			AspectFlags, // aspect mask
			0, // mip map level
			1, // level count
			0, // base array layer
			1  // layer count
		));

	cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
		vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
		{},
		{}, // memory barriers
		{}, // buffer memory barriers
		barrier // image memory barrier
	);
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
			1 // layer count
		))
		.setImageExtent(vk::Extent3D(
			Vulkan.SwapchainExtent.width,
			Vulkan.SwapchainExtent.height,
			1
		));

	cmd->copyImageToBuffer(
		GPU.Image,
		vk::ImageLayout::eTransferDstOptimal,
		CPU.Buffer,
		region);
}

void* BilateralBuffer::MapCPUMemory(vk::DeviceSize offset, vk::DeviceSize size)
{
	if (!size) size = Size;

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
}
