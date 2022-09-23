#pragma once

#include <vulkan/vulkan.hpp>

class AssetPathRel;

struct DescriptorSetLayoutBinding
{
	uint32_t Binding;
	uint32_t Count;
	vk::DescriptorType Type;
	vk::ShaderStageFlags StageFlags{};
};

class Shader
{
public:
	Shader() = default;
	Shader(const Shader& rhs) = default;

	bool Create(
		const AssetPathRel& path,
		vk::ShaderStageFlagBits stage,
		const std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings);

	void Destroy();

	operator bool()
	{
		return Module.operator bool();
	}

	vk::ShaderModule Module;
	vk::ShaderStageFlagBits Stage;
	vk::PipelineShaderStageCreateInfo ShaderStageCreateInfo;
	std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayoutBindings;
};
