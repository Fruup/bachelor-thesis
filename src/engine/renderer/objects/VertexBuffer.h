#pragma once

#include "engine/renderer/objects/Buffer.h"

class VertexBuffer
{
public:
	bool Create(vk::DeviceSize size);
	void Destroy()
	{
		BufferGPU.Destroy();
		BufferCPU.Destroy();
	}

	void Copy(vk::CommandBuffer& cmd);
	void Stage(void* source, size_t size, size_t offset = 0);

	Buffer BufferGPU, BufferCPU;
};
