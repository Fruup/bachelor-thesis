#pragma once

class Buffer
{
public:
	bool Create(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
	void Destroy();

	void Map(void* source, vk::DeviceSize size, vk::DeviceSize offset = 0);

	operator vk::Buffer() { return Buffer; }

	vk::DeviceSize Size;

	vk::Buffer Buffer;
	vk::DeviceMemory Memory;
};
