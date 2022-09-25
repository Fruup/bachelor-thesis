#pragma once

#include <vulkan/vulkan.hpp>

class Command
{
	friend class OneTimeCommand;

public:
	static bool Init();
	static void Exit();

	static vk::CommandBuffer BeginOneTimeCommand();
	static void EndOneTimeCommand(const vk::CommandBuffer& cmd);

private:
	static vk::CommandPool Pool;
};

class OneTimeCommand
{
public:
	OneTimeCommand();
	~OneTimeCommand();

	vk::CommandBuffer* operator -> () { return &CommandBuffer; }
	operator vk::CommandBuffer& () { return CommandBuffer; }

private:
	vk::CommandBuffer CommandBuffer;
};
