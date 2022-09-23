#include "engine/hzpch.h"
#include "engine/renderer/objects/Buffer.h"
#include "engine/renderer/Renderer.h"

bool Buffer::Create(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	vk::BufferCreateInfo info;
	info.setSharingMode(vk::SharingMode::eExclusive)
		.setUsage(usage)
		.setSize(size);

	Buffer = Renderer::GetInstance().Device.createBuffer(info);
	if (!Buffer)
	{
		SPDLOG_ERROR("Vertex buffer creation failed!");
		return false;
	}

	// allocate buffer memory
	auto requirements = Renderer::GetInstance().Device.getBufferMemoryRequirements(Buffer);
	vk::MemoryAllocateInfo allocInfo;
	allocInfo
		.setAllocationSize(requirements.size)
		.setMemoryTypeIndex(Renderer::FindMemoryType(requirements.memoryTypeBits, properties));

	Memory = Renderer::GetInstance().Device.allocateMemory(allocInfo);
	if (!Memory)
	{
		SPDLOG_ERROR("Buffer memory creation failed!");
		return false;
	}

	Renderer::GetInstance().Device.bindBufferMemory(Buffer, Memory, 0);

	Size = size;

	return true;
}

void Buffer::Destroy()
{
	if (Memory)
	{
		Renderer::GetInstance().Device.freeMemory(Memory);
		Memory = nullptr;
	}

	if (Buffer)
	{
		Renderer::GetInstance().Device.destroyBuffer(Buffer);
		Buffer = nullptr;
	}
}

void Buffer::Map(void* source, vk::DeviceSize size, vk::DeviceSize offset)
{
	void* target = Renderer::GetInstance().Device.mapMemory(Memory, offset, size);
	memcpy(target, source, size);
	Renderer::GetInstance().Device.unmapMemory(Memory);
}
