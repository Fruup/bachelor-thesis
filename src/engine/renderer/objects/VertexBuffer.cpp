#include "engine/hzpch.h"
#include "engine/renderer/objects/VertexBuffer.h"
#include "engine/renderer/Renderer.h"

bool VertexBuffer::Create(vk::DeviceSize size)
{
	if (!BufferCPU.Create(
		size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
	{
		SPDLOG_ERROR("CPU staging buffer creation failed!");
		return false;
	}

	if (!BufferGPU.Create(
		size,
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal))
	{
		SPDLOG_ERROR("GPU buffer creation failed!");
		return false;
	}

	return true;
}

void VertexBuffer::Copy(vk::CommandBuffer& cmd)
{
	vk::BufferCopy region{ 0, 0, BufferGPU.Size };
	cmd.copyBuffer(BufferCPU, BufferGPU, region);
}

void VertexBuffer::Stage(void* source, size_t size, size_t offset)
{
	assert(size <= BufferCPU.Size);

	void* target = Renderer::Data.Device.mapMemory(BufferCPU.Memory, offset, size);
	memcpy(target, source, size);
	Renderer::Data.Device.unmapMemory(BufferCPU.Memory);
}
