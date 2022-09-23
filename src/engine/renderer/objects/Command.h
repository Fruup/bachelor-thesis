#pragma once

#include <vulkan/vulkan.hpp>

class Command
{
public:
	static bool Init();
	static void Exit();

	static vk::CommandBuffer BeginOneTimeCommand();
	static void EndOneTimeCommand(const vk::CommandBuffer& cmd);

private:
	static vk::CommandPool Pool;
};
