#include <engine/hzpch.h>

#include "ShowImageRenderPass.h"

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Shader.h>

// --------------------------------------------------------------------

constexpr std::array<float, 4> ClearColor = {
	0.1f, 0.1f, 0.1f, 1.0f,
};

constexpr vk::ClearValue ClearValue = vk::ClearColorValue(ClearColor);

// --------------------------------------------------------------------
// PUBLIC FUNCTIONS

ShowImageRenderPass::ShowImageRenderPass(AdvancedRenderer& renderer) :
	Renderer(renderer)
{
}

void ShowImageRenderPass::Init()
{
	CreateShaders();

	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreatePipelineLayout();
	CreatePipeline();

	CreateUniformBuffer();

	UpdateDescriptorSets();
}

void ShowImageRenderPass::Exit()
{
	Vulkan.Device.destroyDescriptorSetLayout(DescriptorSetLayout);

	Vulkan.Device.destroyPipelineLayout(PipelineLayout);
	Vulkan.Device.destroyPipeline(Pipeline);

	VertexShader.Destroy();
	FragmentShader.Destroy();

	UniformBufferFullscreen.Destroy();
}

void ShowImageRenderPass::Begin()
{
	// begin rendering
	vk::RenderingAttachmentInfo screenAttachment;
	screenAttachment
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setImageView(Vulkan.SwapchainImageViews[Vulkan.CurrentImageIndex])
		.setClearValue(ClearValue)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore);

	std::array<vk::RenderingAttachmentInfo, 1> attachments = {
		screenAttachment,
	};

	vk::RenderingInfo renderingInfo;
	renderingInfo
		.setColorAttachments(attachments)
		.setLayerCount(1)
		.setRenderArea(vk::Rect2D({ 0, 0 }, Vulkan.SwapchainExtent ))
		.setViewMask(0);

	Vulkan.CommandBuffer.beginRendering(renderingInfo);

	// update descriptors
	UpdateUniformsFullscreen();
	UpdateDescriptorSets();

	// bind pipeline and descriptor set
	Vulkan.CommandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics,
		Pipeline
	);

	Vulkan.CommandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		PipelineLayout,
		0,
		DescriptorSet,
		{}
	);
}

void ShowImageRenderPass::End()
{
	Vulkan.CommandBuffer.endRendering();
}

// --------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void ShowImageRenderPass::CreateShaders()
{
	VertexShader.Create(
		"shaders/fullscreen.vert",
		vk::ShaderStageFlagBits::eVertex);

	FragmentShader.Create(
		"shaders/showImage.frag",
		vk::ShaderStageFlagBits::eFragment);
}

void ShowImageRenderPass::CreatePipelineLayout()
{
	vk::PipelineLayoutCreateInfo info;
	info.setSetLayoutCount(1)
		.setSetLayouts(DescriptorSetLayout);

	PipelineLayout = Vulkan.Device.createPipelineLayout(info);
}

void ShowImageRenderPass::CreatePipeline()
{
	// shader stages
	std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
		VertexShader.CreateInfo,
		FragmentShader.CreateInfo,
	};

	// vertex input
	vk::PipelineVertexInputStateCreateInfo vertexInputState;
	vertexInputState
		.setVertexAttributeDescriptions(AdvancedRenderer::Vertex::Attributes)
		.setVertexBindingDescriptions(AdvancedRenderer::Vertex::Binding);

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState;
	inputAssemblyState
		.setPrimitiveRestartEnable(false)
		.setTopology(vk::PrimitiveTopology::eTriangleList);

	// viewport and scissor
	vk::Viewport viewport(
		0.0f, 0.0f,
		float(Vulkan.SwapchainExtent.width),
		float(Vulkan.SwapchainExtent.height),
		0.0f, // min depth
		1.0f  // max depth
	);

	vk::Rect2D scissor({ 0, 0 }, Renderer::GetInstance().SwapchainExtent);

	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState
		.setViewports(viewport)
		.setScissors(scissor);

	// rasterizer
	vk::PipelineRasterizationStateCreateInfo rasterizationState;
	rasterizationState
		.setDepthClampEnable(false)
		.setRasterizerDiscardEnable(false)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setLineWidth(1.0f)
		.setCullMode(vk::CullModeFlagBits::eNone)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setDepthBiasEnable(false);

	// multisampling
	vk::PipelineMultisampleStateCreateInfo multiSampleState;
	multiSampleState
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
		.setBlendEnable(false);

	vk::PipelineColorBlendStateCreateInfo blendState;
	blendState
		.setLogicOpEnable(false)
		.setAttachments(blendAttachment);

	// depth state
	vk::PipelineDepthStencilStateCreateInfo depthStencilState;
	depthStencilState
		.setDepthTestEnable(false)
		.setDepthWriteEnable(false)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false);

	// rendering info
	vk::PipelineRenderingCreateInfo renderingPipelineCreateInfo;
	renderingPipelineCreateInfo.setViewMask(0)
		.setColorAttachmentFormats(Vulkan.SwapchainFormat);

	// pipeline
	vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
	pipelineCreateInfo
		.setPNext(&renderingPipelineCreateInfo)
		.setLayout(PipelineLayout)
		.setPColorBlendState(&blendState)
		.setPDepthStencilState(&depthStencilState)
		.setPInputAssemblyState(&inputAssemblyState)
		.setPMultisampleState(&multiSampleState)
		.setPRasterizationState(&rasterizationState)
		.setStages(stages)
		.setPVertexInputState(&vertexInputState)
		.setPViewportState(&viewportState)
		.setRenderPass(nullptr);

	auto r = Vulkan.Device.createGraphicsPipeline({}, pipelineCreateInfo);
	HZ_ASSERT(r.result == vk::Result::eSuccess, "Pipeline creation failed!");
	Pipeline = r.value;
}

void ShowImageRenderPass::CreateDescriptorSetLayout()
{
	vk::DescriptorSetLayoutBinding uniformBufferFullscreen;
	uniformBufferFullscreen
		.setBinding(2)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setStageFlags(vk::ShaderStageFlagBits::eVertex);

	vk::DescriptorSetLayoutBinding depthAttachment;
	depthAttachment
		.setBinding(0)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setStageFlags(vk::ShaderStageFlagBits::eFragment);

	std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
		uniformBufferFullscreen,
		depthAttachment,
	};

	vk::DescriptorSetLayoutCreateInfo info({}, bindings);
	DescriptorSetLayout = Vulkan.Device.createDescriptorSetLayout(info);
}

void ShowImageRenderPass::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(Vulkan.DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(DescriptorSetLayout);

	auto r = Vulkan.Device.allocateDescriptorSets(info);
	HZ_ASSERT(r.size() == 1, "allocateDescriptorSets failed!");
	DescriptorSet = r.front();
}

void ShowImageRenderPass::CreateUniformBuffer()
{
	UniformBufferFullscreen.Create(
		sizeof(UniformsFullscreen),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);
}

void ShowImageRenderPass::UpdateUniformsFullscreen()
{
	UniformsFullscreen.TexelWidth = float(1.0f) / float(Vulkan.SwapchainExtent.width);
	UniformsFullscreen.TexelHeight = float(1.0f) / float(Vulkan.SwapchainExtent.height);

	// copy
	UniformBufferFullscreen.Map(&UniformsFullscreen, sizeof(UniformsFullscreen));
}

void ShowImageRenderPass::UpdateDescriptorSets()
{
	// uniforms fullscreen
	vk::DescriptorBufferInfo bufferFullscreenInfo;
	bufferFullscreenInfo
		.setBuffer(UniformBufferFullscreen)
		.setOffset(0)
		.setRange(VK_WHOLE_SIZE);

	vk::WriteDescriptorSet writeUniformFullscreen;
	writeUniformFullscreen
		.setBufferInfo(bufferFullscreenInfo)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setDstArrayElement(0)
		.setDstBinding(2)
		.setDstSet(DescriptorSet);

	// smoothed depth
	vk::DescriptorImageInfo depthImageInfo;
	depthImageInfo
		.setImageView(Renderer.SmoothedDepthBuffer.GPU.ImageView)
		.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		.setSampler(Renderer.SmoothedDepthBuffer.GPU.Sampler);

	vk::WriteDescriptorSet writeDepthImage;
	writeDepthImage
		.setImageInfo(depthImageInfo)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDstArrayElement(0)
		.setDstBinding(0)
		.setDstSet(DescriptorSet);

	// write
	std::array<vk::WriteDescriptorSet, 2> writes = {
		writeUniformFullscreen,
		writeDepthImage,
	};

	Vulkan.Device.updateDescriptorSets(writes, {});
}

// --------------------------------------------------------------------
