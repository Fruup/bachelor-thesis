#pragma once

#include <vulkan/vulkan.hpp>

#include <engine/assets/AssetPath.h>

class AssetPathRel;

class Shader
{
public:
	Shader() = default;
	Shader(const Shader& rhs) = default;

	bool Create(const AssetPathRel& path, vk::ShaderStageFlagBits stage);
	void Destroy();

	operator bool()
	{
		return Module.operator bool();
	}

	vk::ShaderModule Module;
	vk::ShaderStageFlagBits Stage;
	vk::PipelineShaderStageCreateInfo CreateInfo;
};
