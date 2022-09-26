#include <engine/hzpch.h>

#include "CompositionRenderPass.h"

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Shader.h>

// --------------------------------------------------------------------

//auto& Vulkan = Renderer::GetInstance();

constexpr std::array<float, 4> ClearColor = {
	0.1f, 0.1f, 0.1f, 1.0f,
};

constexpr vk::ClearValue ClearValue = vk::ClearColorValue(ClearColor);

// --------------------------------------------------------------------
// PUBLIC FUNCTIONS

CompositionRenderPass::CompositionRenderPass(AdvancedRenderer& renderer) :
	Renderer(renderer)
{
}

void CompositionRenderPass::Init()
{
	CreateShaders();

	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreatePipelineLayout();
	CreatePipeline();

	CreateUniformBuffer();

	UpdateDescriptorSets();
}

void CompositionRenderPass::Exit()
{
	Vulkan.Device.destroyDescriptorSetLayout(DescriptorSetLayout);

	Vulkan.Device.destroyPipelineLayout(PipelineLayout);
	Vulkan.Device.destroyPipeline(Pipeline);

	VertexShader.Destroy();
	FragmentShader.Destroy();

	UniformBuffer.Destroy();
}

void CompositionRenderPass::Begin()
{
	// update uniforms
	UpdateUniforms();

	// begin rendering
	vk::RenderingAttachmentInfo screenAttachment;
	screenAttachment
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setImageView(Vulkan.SwapchainImageViews[Vulkan.CurrentImageIndex])
		.setClearValue(ClearValue)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eDontCare);

	vk::RenderingAttachmentInfo positionsAttachment;
	positionsAttachment
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setImageView(Renderer.PositionsBuffer.GPU.ImageView)
		.setLoadOp(vk::AttachmentLoadOp::eLoad)
		.setStoreOp(vk::AttachmentStoreOp::eDontCare);

	vk::RenderingAttachmentInfo normalsAttachment;
	normalsAttachment
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setImageView(Renderer.NormalsBuffer.GPU.ImageView)
		.setLoadOp(vk::AttachmentLoadOp::eLoad)
		.setStoreOp(vk::AttachmentStoreOp::eDontCare);

	std::array<vk::RenderingAttachmentInfo, 3> attachments = {
		screenAttachment,
		positionsAttachment,
		normalsAttachment,
	};

	vk::RenderingInfo renderingInfo;
	renderingInfo
		.setColorAttachments(attachments)
		.setLayerCount(1)
		.setRenderArea(vk::Rect2D({ 0, 0 }, Vulkan.SwapchainExtent ))
		.setViewMask(0);

	Vulkan.CommandBuffer.beginRendering(renderingInfo);

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

void CompositionRenderPass::End()
{
	Vulkan.CommandBuffer.endRendering();
}

// --------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void CompositionRenderPass::CreateShaders()
{
	VertexShader.Create(
		"shaders/fullscreen.vert",
		vk::ShaderStageFlagBits::eVertex);

	FragmentShader.Create(
		"shaders/advanced/composition.frag",
		vk::ShaderStageFlagBits::eFragment);
}

void CompositionRenderPass::CreatePipelineLayout()
{
	vk::PipelineLayoutCreateInfo info;
	info.setSetLayoutCount(1)
		.setSetLayouts(DescriptorSetLayout);

	PipelineLayout = Vulkan.Device.createPipelineLayout(info);
}

void CompositionRenderPass::CreatePipeline()
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
	blendAttachment.setBlendEnable(false);

	vk::PipelineColorBlendStateCreateInfo blendState;
	blendState
		.setLogicOpEnable(false)
		.setAttachments(blendAttachment);

	// depth state
	vk::PipelineDepthStencilStateCreateInfo depthStencilState;
	depthStencilState
		.setDepthTestEnable(true)
		.setDepthWriteEnable(true)
		.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false);

	// rendering info
	vk::PipelineRenderingCreateInfo renderingPipelineCreateInfo;
	renderingPipelineCreateInfo
		.setDepthAttachmentFormat(Renderer.DepthBuffer.Format);

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

void CompositionRenderPass::CreateDescriptorSetLayout()
{
	vk::DescriptorSetLayoutBinding uniformBuffer;
	uniformBuffer
		.setBinding(0)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setStageFlags(vk::ShaderStageFlagBits::eFragment);

	std::array<vk::DescriptorSetLayoutBinding, 1> bindings = {
		uniformBuffer,
	};

	vk::DescriptorSetLayoutCreateInfo info({}, bindings);
	DescriptorSetLayout = Vulkan.Device.createDescriptorSetLayout(info);
}

void CompositionRenderPass::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(Vulkan.DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(DescriptorSetLayout);

	auto r = Vulkan.Device.allocateDescriptorSets(info);
	HZ_ASSERT(r.size() == 1, "allocateDescriptorSets failed!");
	DescriptorSet = r.front();
}

void CompositionRenderPass::CreateUniformBuffer()
{
	UniformBuffer.Create(
		sizeof(Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);
}

void CompositionRenderPass::UpdateUniforms()
{
	Uniforms.InvProjection = Renderer.Camera.GetInvProjection();

	Uniforms.CameraPosition = Renderer.CameraController.Position;
	Uniforms.CameraDirection = Renderer.CameraController.System[2];

	Uniforms.LightDirection = glm::vec3(-1, -1, 1);

	// copy
	UniformBuffer.Map(&Uniforms, sizeof(Uniforms));
}

void CompositionRenderPass::UpdateDescriptorSets()
{
	vk::DescriptorBufferInfo bufferInfo;
	bufferInfo
		.setBuffer(UniformBuffer)
		.setOffset(0)
		.setRange(VK_WHOLE_SIZE);

	vk::WriteDescriptorSet writeUniform;
	writeUniform
		.setBufferInfo(bufferInfo)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setDstArrayElement(0)
		.setDstBinding(0)
		.setDstSet(DescriptorSet);

	std::array<vk::WriteDescriptorSet, 1> writes = {
		writeUniform
	};

	Vulkan.Device.updateDescriptorSets(writes, {});
}

// --------------------------------------------------------------------
