#include <engine/hzpch.h>

#include "CoordinateSystemRenderPass.h"

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/Command.h>

// --------------------------------------------------------------------
// PUBLIC FUNCTIONS

CoordinateSystemRenderPass::CoordinateSystemRenderPass(AdvancedRenderer& renderer) :
	Renderer(renderer)
{
}

void CoordinateSystemRenderPass::Init()
{
	CreateShaders();

	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreatePipelineLayout();
	CreatePipeline();

	CreateUniformBuffer();
	CreateVertexBuffer();

	UpdateDescriptorSets();
}

void CoordinateSystemRenderPass::Exit()
{
	Vulkan.Device.destroyDescriptorSetLayout(DescriptorSetLayout);

	Vulkan.Device.destroyPipelineLayout(PipelineLayout);
	Vulkan.Device.destroyPipeline(Pipeline);

	VertexShader.Destroy();
	FragmentShader.Destroy();

	VertexBuffer.Destroy();

	UniformBuffer.Destroy();
}

void CoordinateSystemRenderPass::Begin()
{
	// begin rendering
	vk::RenderingAttachmentInfo screenAttachment;
	screenAttachment
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setImageView(Vulkan.SwapchainImageViews[Vulkan.CurrentImageIndex])
		.setLoadOp(vk::AttachmentLoadOp::eLoad)
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
	UpdateUniforms();
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

	// bind vertex buffer and draw
	uint32_t offset = 0;
	Vulkan.CommandBuffer.bindVertexBuffers(0,
										   VertexBuffer.BufferGPU.Buffer,
										   offset);

	Vulkan.CommandBuffer.draw(6, 1, 0, 0);
}

void CoordinateSystemRenderPass::End()
{
	Vulkan.CommandBuffer.endRendering();
}

// --------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void CoordinateSystemRenderPass::CreateShaders()
{
	VertexShader.Create(
		"shaders/coordinateSystem.vert",
		vk::ShaderStageFlagBits::eVertex);

	FragmentShader.Create(
		"shaders/coordinateSystem.frag",
		vk::ShaderStageFlagBits::eFragment);
}

void CoordinateSystemRenderPass::CreateVertexBuffer()
{
	VertexBuffer.Create(sizeof(AdvancedRenderer::Vertex) * 6);

	AdvancedRenderer::Vertex data[6] = {
		{ glm::vec3(0, 0, 0) },
		{ glm::vec3(1, 0, 0) },
		{ glm::vec3(0, 0, 0) },
		{ glm::vec3(0, 1, 0) },
		{ glm::vec3(0, 0, 0) },
		{ glm::vec3(0, 0, 1) },
	};

	VertexBuffer.Stage(data, sizeof(AdvancedRenderer::Vertex) * 6);

	{
		OneTimeCommand cmd;
		VertexBuffer.Copy(cmd);
	}
}

void CoordinateSystemRenderPass::CreatePipelineLayout()
{
	vk::PipelineLayoutCreateInfo info;
	info.setSetLayoutCount(1)
		.setSetLayouts(DescriptorSetLayout);

	PipelineLayout = Vulkan.Device.createPipelineLayout(info);
}

void CoordinateSystemRenderPass::CreatePipeline()
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
		.setTopology(vk::PrimitiveTopology::eLineList);

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
			vk::ColorComponentFlagBits::eB)
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
	renderingPipelineCreateInfo
		.setViewMask(0)
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

void CoordinateSystemRenderPass::CreateDescriptorSetLayout()
{
	vk::DescriptorSetLayoutBinding uniformBuffer;
	uniformBuffer
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setStageFlags(vk::ShaderStageFlagBits::eVertex)
		.setBinding(0);

	std::array<vk::DescriptorSetLayoutBinding, 1> bindings = {
		uniformBuffer,
	};

	vk::DescriptorSetLayoutCreateInfo info({}, bindings);
	DescriptorSetLayout = Vulkan.Device.createDescriptorSetLayout(info);
}

void CoordinateSystemRenderPass::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(Vulkan.DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(DescriptorSetLayout);

	auto r = Vulkan.Device.allocateDescriptorSets(info);
	HZ_ASSERT(r.size() == 1, "allocateDescriptorSets failed!");
	DescriptorSet = r.front();
}

void CoordinateSystemRenderPass::CreateUniformBuffer()
{
	UniformBuffer.Create(
		sizeof(UniformBuffer),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);
}

void CoordinateSystemRenderPass::UpdateUniforms()
{
	Uniforms.ProjectionView = Renderer.Camera.ProjectionView;
	Uniforms.Aspect = Renderer.Camera.Aspect;

	// copy
	UniformBuffer.Map(&Uniforms, sizeof(Uniforms));
}

void CoordinateSystemRenderPass::UpdateDescriptorSets()
{
	// uniforms fullscreen
	vk::DescriptorBufferInfo uniformBufferInfo;
	uniformBufferInfo
		.setBuffer(UniformBuffer)
		.setOffset(0)
		.setRange(VK_WHOLE_SIZE);

	vk::WriteDescriptorSet writeUniformBuffer;
	writeUniformBuffer
		.setBufferInfo(uniformBufferInfo)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setDstArrayElement(0)
		.setDstSet(DescriptorSet)
		.setDstBinding(0);

	// write
	std::array<vk::WriteDescriptorSet, 1> writes = {
		writeUniformBuffer,
	};

	Vulkan.Device.updateDescriptorSets(writes, {});
}

// --------------------------------------------------------------------
