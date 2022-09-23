#include "engine/hzpch.h"
#include "engine/renderer/objects/Command.h"
#include "engine/renderer/Renderer.h"

vk::CommandPool Command::Pool;

bool Command::Init()
{
	// create pool
	vk::CommandPoolCreateInfo poolInfo;
	poolInfo
		.setFlags(vk::CommandPoolCreateFlagBits::eTransient)
		.setQueueFamilyIndex(Renderer::Data.QueueIndices.GraphicsFamily.value());
	Pool = Renderer::Data.Device.createCommandPool(poolInfo);

	return Pool;
}

void Command::Exit()
{
	Renderer::Data.Device.destroyCommandPool(Pool);
}

vk::CommandBuffer Command::BeginOneTimeCommand()
{
	// create command buffer
	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo
		.setCommandBufferCount(1)
		.setCommandPool(Pool)
		.setLevel(vk::CommandBufferLevel::ePrimary);
	vk::CommandBuffer cmd = Renderer::Data.Device.allocateCommandBuffers(allocInfo).front();

	// begin buffer
	vk::CommandBufferBeginInfo beginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
	cmd.begin(beginInfo);

	return cmd;
}

void Command::EndOneTimeCommand(const vk::CommandBuffer& cmd)
{
	cmd.end();

	// submit
	vk::SubmitInfo submitInfo;
	submitInfo.setCommandBuffers(cmd);
	Renderer::Data.GraphicsQueue.submit(submitInfo);
	Renderer::Data.GraphicsQueue.waitIdle();

	// free
	Renderer::Data.Device.freeCommandBuffers(Pool, cmd);
}
