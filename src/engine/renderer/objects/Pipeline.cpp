#include "engine/hzpch.h"
#include "engine/renderer/objects/Pipeline.h"
#include "engine/renderer/Renderer.h"

bool Pipeline::Create(
	const std::vector<Shader>& shaders,
	uint32_t subpass,
	const vk::ArrayProxyNoTemporaries<const vk::VertexInputAttributeDescription>& vertexAttributeDescriptions,
	const vk::VertexInputBindingDescription& vertexBindingDescription,
	vk::PrimitiveTopology Topology,
	bool depthTesting,
	bool enableBlending,
	int numBlendAttachments)
{
	// shader stages
	std::vector<vk::PipelineShaderStageCreateInfo> stages(shaders.size());
	for (size_t i = 0; i < stages.size(); i++)
		stages[i] = shaders[i].ShaderStageCreateInfo;

	// vertex input
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo
		.setVertexAttributeDescriptions(vertexAttributeDescriptions)
		.setVertexBindingDescriptions(vertexBindingDescription);

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo;
	assemblyInfo
		.setPrimitiveRestartEnable(false)
		.setTopology(Topology);

	// viewport and scissor
	vk::Viewport viewport(
		.0f, .0f,
		float(Renderer::Data.SwapchainExtent.width), float(Renderer::Data.SwapchainExtent.height),
		.0f, 1.0f
	);

	vk::Rect2D scissor({ 0, 0 }, Renderer::Data.SwapchainExtent);

	vk::PipelineViewportStateCreateInfo viewportInfo;
	viewportInfo
		.setViewports(viewport)
		.setScissors(scissor);

	// rasterizer
	vk::PipelineRasterizationStateCreateInfo rasterInfo;
	rasterInfo
		.setDepthClampEnable(false)
		.setRasterizerDiscardEnable(false)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setLineWidth(1.0f)
		.setCullMode(vk::CullModeFlagBits::eNone)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setDepthBiasEnable(false);

	// multisampling
	vk::PipelineMultisampleStateCreateInfo multisampleInfo;
	multisampleInfo
		.setSampleShadingEnable(false)
		.setRasterizationSamples(vk::SampleCountFlagBits::e1);

	// blending
	vk::PipelineColorBlendAttachmentState blendAttachment;
	blendAttachment
		.setColorWriteMask(
			vk::ColorComponentFlagBits::eR |
			vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB |
			vk::ColorComponentFlagBits::eA)
		.setBlendEnable(enableBlending)
		.setColorBlendOp(vk::BlendOp::eAdd)
		.setAlphaBlendOp(vk::BlendOp::eAdd)
		.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
		.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
		.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
		.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);

	std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(numBlendAttachments);

	//blendAttachments[0] = blendAttachment;
	for (int i = 0; i < numBlendAttachments; i++)
		blendAttachments[i] = blendAttachment;

	vk::PipelineColorBlendStateCreateInfo blendInfo;
	blendInfo
		.setLogicOpEnable(false)
		.setAttachments(blendAttachments);

	// create descriptor set layout
	{
		std::vector<vk::DescriptorSetLayoutBinding> bindings;
		for (auto& shader : shaders)
		{
			for (auto& b : shader.DescriptorSetLayoutBindings)
			{
				bindings.push_back(b);
			}
		}

		vk::DescriptorSetLayoutCreateInfo info{ {}, bindings };
		m_DescriptorSetLayout = Renderer::Data.Device.createDescriptorSetLayout(info);
	}

	// pipeline layout
	vk::PipelineLayoutCreateInfo layoutInfo;
	layoutInfo.setSetLayouts(m_DescriptorSetLayout);

	m_Layout = Renderer::Data.Device.createPipelineLayout(layoutInfo);
	if (!m_Layout)
	{
		SPDLOG_ERROR("Pipeline layout creation failed!");
		return false;
	}

	// depth state
	vk::PipelineDepthStencilStateCreateInfo depthStencilState;
	depthStencilState
		.setDepthTestEnable(depthTesting)
		.setDepthWriteEnable(depthTesting)
		.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false);

	// pipeline
	vk::GraphicsPipelineCreateInfo info;
	info.setLayout(m_Layout)
		.setStages(stages)
		.setRenderPass(Renderer::Data.RenderPass)
		.setSubpass(subpass)
		.setPVertexInputState(&vertexInputInfo)
		.setPInputAssemblyState(&assemblyInfo)
		.setPRasterizationState(&rasterInfo)
		.setPMultisampleState(&multisampleInfo)
		.setPColorBlendState(&blendInfo)
		.setPViewportState(&viewportInfo)
		.setPDepthStencilState(&depthStencilState);

	m_Pipeline = Renderer::Data.Device.createGraphicsPipeline({}, info).value;
	if (!m_Pipeline)
	{
		SPDLOG_ERROR("Pipeline creation failed!");
		return false;
	}

	return true;
}

void Pipeline::Destroy()
{
	if (m_Layout)
	{
		Renderer::Data.Device.destroyPipelineLayout(m_Layout);
		m_Layout = nullptr;
	}

	if (m_Pipeline)
	{
		Renderer::Data.Device.destroyPipeline(m_Pipeline);
		m_Pipeline = nullptr;
	}

	if (m_DescriptorSetLayout)
	{
		Renderer::Data.Device.destroyDescriptorSetLayout(m_DescriptorSetLayout);
		m_DescriptorSetLayout = nullptr;
	}
}
