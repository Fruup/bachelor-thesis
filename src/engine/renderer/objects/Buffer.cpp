#include "engine/hzpch.h"
#include "engine/renderer/objects/Buffer.h"
#include "engine/renderer/Renderer.h"

bool Buffer::Create(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	vk::BufferCreateInfo info;
	info.setSharingMode(vk::SharingMode::eExclusive)
		.setUsage(usage)
		.setSize(size);

	Buffer = Renderer::Data.Device.createBuffer(info);
	if (!Buffer)
	{
		SPDLOG_ERROR("Vertex buffer creation failed!");
		return false;
	}

	// allocate buffer memory
	auto requirements = Renderer::Data.Device.getBufferMemoryRequirements(Buffer);
	vk::MemoryAllocateInfo allocInfo;
	allocInfo
		.setAllocationSize(requirements.size)
		.setMemoryTypeIndex(Renderer::FindMemoryType(requirements.memoryTypeBits, properties));

	Memory = Renderer::Data.Device.allocateMemory(allocInfo);
	if (!Memory)
	{
		SPDLOG_ERROR("Buffer memory creation failed!");
		return false;
	}

	Renderer::Data.Device.bindBufferMemory(Buffer, Memory, 0);

	Size = size;

	return true;
}

void Buffer::Destroy()
{
	if (Memory)
	{
		Renderer::Data.Device.freeMemory(Memory);
		Memory = nullptr;
	}

	if (Buffer)
	{
		Renderer::Data.Device.destroyBuffer(Buffer);
		Buffer = nullptr;
	}
}

void Buffer::Map(void* source, vk::DeviceSize size, vk::DeviceSize offset)
{
	void* target = Renderer::Data.Device.mapMemory(Memory, offset, size);
	memcpy(target, source, size);
	Renderer::Data.Device.unmapMemory(Memory);
}
