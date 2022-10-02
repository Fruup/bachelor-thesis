#include "engine/hzpch.h"

#include "DiskRenderPass.h"
#include "DiskRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Shader.h>

// --------------------------------------------------------------------
// PUBLIC FUNCTIONS

DiskRenderPass::DiskRenderPass(DiskRenderer& renderer) :
	Renderer(renderer)
{
}

void DiskRenderPass::Init()
{
	CreateShaders();

	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreatePipelineLayout();
	CreatePipeline();

	CreateUniformBuffer();

	UpdateDescriptorSets();
}

void DiskRenderPass::Exit()
{
	Vulkan.Device.destroyDescriptorSetLayout(DescriptorSetLayout);

	Vulkan.Device.destroyPipelineLayout(PipelineLayout);
	Vulkan.Device.destroyPipeline(Pipeline);

	VertexShader.Destroy();
	FragmentShader.Destroy();

	UniformBuffer.Destroy();
}

void DiskRenderPass::Begin()
{
	std::array<float, 4> clearValue = { 0.1f, 0.1f, 0.1f, 1.0f };

	// update uniforms
	UpdateUniforms();
	UpdateDescriptorSets();

	// begin rendering
	vk::RenderingAttachmentInfo screenAttachment;
	screenAttachment
		.setClearValue(vk::ClearColorValue(clearValue))
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setImageView(Vulkan.SwapchainImageViews[Vulkan.CurrentImageIndex])
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore);

	vk::RenderingAttachmentInfo depthAttachment;
	depthAttachment
		.setClearValue(vk::ClearDepthStencilValue(1.0f))
		.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
		.setImageView(Renderer.DepthBuffer.GPU.ImageView)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eDontCare);

	vk::RenderingInfo renderingInfo;
	renderingInfo
		.setColorAttachments(screenAttachment)
		.setPDepthAttachment(&depthAttachment)
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

void DiskRenderPass::End()
{
	Vulkan.CommandBuffer.endRendering();
}

// --------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void DiskRenderPass::CreateShaders()
{
	VertexShader.Create(
		"shaders/disk/disk.vert",
		vk::ShaderStageFlagBits::eVertex);

	FragmentShader.Create(
		"shaders/disk/disk.frag",
		vk::ShaderStageFlagBits::eFragment);
}

void DiskRenderPass::CreatePipelineLayout()
{
	vk::PipelineLayoutCreateInfo info;
	info.setSetLayoutCount(1)
		.setSetLayouts(DescriptorSetLayout);

	PipelineLayout = Vulkan.Device.createPipelineLayout(info);
}

void DiskRenderPass::CreatePipeline()
{
	// shader stages
	std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
		VertexShader.CreateInfo,
		FragmentShader.CreateInfo,
	};

	// vertex input
	vk::PipelineVertexInputStateCreateInfo vertexInputState;
	vertexInputState
		.setVertexAttributeDescriptions(DiskRenderer::Vertex::Attributes)
		.setVertexBindingDescriptions(DiskRenderer::Vertex::Binding);

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
		.setBlendEnable(false)
		.setColorWriteMask(vk::ColorComponentFlagBits::eR |
						   vk::ColorComponentFlagBits::eG |
						   vk::ColorComponentFlagBits::eB);

	vk::PipelineColorBlendStateCreateInfo blendState;
	blendState
		.setLogicOpEnable(false)
		.setAttachments(blendAttachment);

	// depth state
	vk::PipelineDepthStencilStateCreateInfo depthStencilState;
	depthStencilState
		.setDepthTestEnable(true)
		.setDepthWriteEnable(true)
		.setDepthCompareOp(vk::CompareOp::eLess)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false);

	// rendering info
	vk::PipelineRenderingCreateInfo renderingPipelineCreateInfo;
	renderingPipelineCreateInfo
		.setDepthAttachmentFormat(Renderer.DepthBuffer.Format)
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

void DiskRenderPass::CreateDescriptorSetLayout()
{
	vk::DescriptorSetLayoutBinding binding;
	binding
		.setBinding(0)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

	vk::DescriptorSetLayoutCreateInfo info({}, binding);
	DescriptorSetLayout = Vulkan.Device.createDescriptorSetLayout(info);
}

void DiskRenderPass::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(Vulkan.DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(DescriptorSetLayout);

	auto r = Vulkan.Device.allocateDescriptorSets(info);
	HZ_ASSERT(r.size() == 1, "allocateDescriptorSets failed!");
	DescriptorSet = r.front();
}

void DiskRenderPass::CreateUniformBuffer()
{
	UniformBuffer.Create(
		sizeof(Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);
}

void DiskRenderPass::UpdateUniforms()
{
	Uniforms.Projection = Renderer.Camera.GetProjection();
	Uniforms.View = Renderer.Camera.GetView();
	Uniforms.Radius = Renderer.Dataset->ParticleRadius;

	// copy
	UniformBuffer.Map(&Uniforms, sizeof(Uniforms));
}

void DiskRenderPass::UpdateDescriptorSets()
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
