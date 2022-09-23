#pragma once

#include <vulkan/vulkan.hpp>

#include "engine/renderer/objects/Shader.h"

class Pipeline
{
public:
	Pipeline() = default;

	bool Create(
		const std::vector<Shader>& shaders,
		uint32_t subpass,
		const vk::ArrayProxyNoTemporaries<const vk::VertexInputAttributeDescription>& vertexAttributeDescriptions,
		const vk::VertexInputBindingDescription& vertexBindingDescription,
		vk::PrimitiveTopology Topology = vk::PrimitiveTopology::eTriangleList,
		bool depthTesting = true,
		bool enableBlending = true,
		int numBlendAttachments = 1);

	void Destroy();

	const vk::PipelineLayout& GetLayout() const { return m_Layout; }
	const vk::DescriptorSetLayout& GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }

	operator vk::Pipeline() const { return m_Pipeline; }
	operator bool() const { return m_Pipeline.operator bool(); }

private:
	vk::Pipeline m_Pipeline;
	vk::PipelineLayout m_Layout;

	vk::PipelineLayoutCreateInfo m_LayoutInfo;
	vk::GraphicsPipelineCreateInfo m_PipelineInfo;

	vk::DescriptorSetLayout m_DescriptorSetLayout;
};
