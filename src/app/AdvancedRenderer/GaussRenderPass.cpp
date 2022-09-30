#include <engine/hzpch.h>

#include "GaussRenderPass.h"

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/utils/PerformanceTimer.h>

// --------------------------------------------------------------

#define PI 3.1415926535897932384626433832795
#define E  2.7182818284590452353602874713527

float gauss(float x)
{
	return powf(E, -0.5f*x*x);
}

float radius(int i, int j)
{
	return sqrtf(float(i) * float(i) + float(j) * float(j));
}

void ComputeGaussKernel(int N, float* out)
{
	PROFILE_FUNCTION();

	float sum = 0.0f;

	for (int i = 0; i <= N; i++)
	{
		for (int j = 0; j <= N; j++)
		{
			float y = gauss(radius(i, j));
			out[j + i * (N + 1)] = y;

			if (i == 0 && j == 0)
				sum += y;
			else if (i != 0 && j != 0)
				sum += 4.0f * y;
			else if (i != 0 || j != 0)
				sum += 2.0f * y;
		}
	}

	for (int i = 0; i <= N; i++)
		for (int j = 0; j <= N; j++)
			out[j + i * (N + 1)] /= sum;

#if 0
	for (int i = 0; i <= N; i++)
	{
		for (int j = 0; j <= N; j++)
		{
			std::cout << out[j + i * (N + 1)] << ", ";
		}

		std::cout << std::endl;
	}

	std::cout << sum << std::endl;

	SPDLOG_INFO("");
#endif
}

GaussRenderPass::GaussRenderPass(AdvancedRenderer& renderer) :
	Renderer(renderer)
{
	/*std::vector<float> out;
	ComputeGaussKernel(1, out);*/
}

// --------------------------------------------------------------

void GaussRenderPass::Init()
{
	CreateShaders();

	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreatePipelineLayout();
	CreatePipeline();

	CreateUniformBuffer();

	UpdateDescriptorSet();

	ComputeGaussKernel(Uniforms.GaussN, Uniforms.Kernel);
}

void GaussRenderPass::Exit()
{
	UniformBuffer.Destroy();

	Vulkan.Device.destroyPipeline(Pipeline);
	Vulkan.Device.destroyPipelineLayout(PipelineLayout);
	
	Vulkan.Device.destroyDescriptorSetLayout(DescriptorSetLayout);

	VertexShader.Destroy();
	FragmentShader.Destroy();

	UniformBufferFullscreen.Destroy();
}

void GaussRenderPass::Begin()
{
	vk::ClearDepthStencilValue clearValue(0.5f);

	vk::RenderingAttachmentInfo smoothedDepthAttachment;
	smoothedDepthAttachment
		.setClearValue(clearValue)
		.setLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setImageView(Renderer.SmoothedDepthBuffer.GPU.ImageView)
		.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);

	vk::RenderingInfo renderingInfo;
	renderingInfo
		.setPDepthAttachment(&smoothedDepthAttachment)
		.setLayerCount(1)
		.setRenderArea(vk::Rect2D({ 0, 0 }, Vulkan.SwapchainExtent ))
		.setViewMask(0);

	Vulkan.CommandBuffer.beginRendering(renderingInfo);

	// update descriptor set
	UpdateDescriptorSet();

	UpdateUniforms();

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

void GaussRenderPass::End()
{
	Vulkan.CommandBuffer.endRendering();
}

void GaussRenderPass::RenderUI()
{
	ImGui::Begin("GaussRenderPass");

	if (ImGui::SliderInt("Gauss N", &Uniforms.GaussN, 1, 63))
	{
		ComputeGaussKernel(Uniforms.GaussN, Uniforms.Kernel);
	}

	ImGui::End();
}

// --------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void GaussRenderPass::CreateUniformBuffer()
{
	UniformBuffer.Create(
		sizeof(Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);

	UniformBufferFullscreen.Create(
		sizeof(UniformsFullscreen),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);
}

void GaussRenderPass::CreateShaders()
{
	VertexShader.Create(
		"shaders/fullscreen.vert",
		vk::ShaderStageFlagBits::eVertex);

	FragmentShader.Create(
		"shaders/advanced/gauss.frag",
		vk::ShaderStageFlagBits::eFragment);
}

void GaussRenderPass::CreatePipelineLayout()
{
	vk::PipelineLayoutCreateInfo info;
	info.setSetLayoutCount(1)
		.setSetLayouts(DescriptorSetLayout);

	PipelineLayout = Vulkan.Device.createPipelineLayout(info);
}

void GaussRenderPass::CreatePipeline()
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

	vk::Rect2D scissor({ 0, 0 }, Vulkan.SwapchainExtent);

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
		.setDepthTestEnable(false)
		.setDepthWriteEnable(true)
		.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false);

	// rendering info
	vk::PipelineRenderingCreateInfo renderingPipelineCreateInfo;
	renderingPipelineCreateInfo
		.setViewMask(0)
		.setDepthAttachmentFormat(Renderer.SmoothedDepthBuffer.Format);

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

void GaussRenderPass::CreateDescriptorSetLayout()
{
	vk::DescriptorSetLayoutBinding uniformBinding;
	uniformBinding
		.setBinding(0)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setStageFlags(vk::ShaderStageFlagBits::eFragment);

	vk::DescriptorSetLayoutBinding resolutionUniformBinding;
	resolutionUniformBinding
		.setBinding(2)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setStageFlags(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex);

	vk::DescriptorSetLayoutBinding depthSamplerBinding;
	depthSamplerBinding
		.setBinding(1)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setStageFlags(vk::ShaderStageFlagBits::eFragment);

	std::array<vk::DescriptorSetLayoutBinding, 3> bindings = {
		uniformBinding,
		resolutionUniformBinding,
		depthSamplerBinding,
	};

	vk::DescriptorSetLayoutCreateInfo info({}, bindings);
	DescriptorSetLayout = Vulkan.Device.createDescriptorSetLayout(info);
}

void GaussRenderPass::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(Vulkan.DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(DescriptorSetLayout);

	auto r = Vulkan.Device.allocateDescriptorSets(info);
	HZ_ASSERT(r.size() == 1, "allocateDescriptorSets failed!");
	DescriptorSet = r.front();
}

void GaussRenderPass::UpdateUniforms()
{
	// copy
	UniformBuffer.Map(&Uniforms, sizeof(Uniforms));

	// fullscreen uniforms
	UniformsFullscreen.TexelWidth = float(1.0f) / float(Vulkan.SwapchainExtent.width);
	UniformsFullscreen.TexelHeight = float(1.0f) / float(Vulkan.SwapchainExtent.height);

	// copy
	UniformBufferFullscreen.Map(&UniformsFullscreen, sizeof(UniformsFullscreen));
}

void GaussRenderPass::UpdateDescriptorSet()
{
	// uniforms 1 (all up to kernel array)
	vk::DescriptorBufferInfo bufferInfo;
	bufferInfo
		.setBuffer(UniformBuffer)
		.setOffset(0)
		.setRange(sizeof(int) + sizeof(float) * (Uniforms.GaussN + 1) * (Uniforms.GaussN + 1));

	vk::WriteDescriptorSet writeUniform;
	writeUniform
		.setBufferInfo(bufferInfo)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setDstArrayElement(0)
		.setDstBinding(0)
		.setDstSet(DescriptorSet);

	// depth
	vk::DescriptorImageInfo depthImageInfo;
	depthImageInfo
		.setImageView(Renderer.DepthBuffer.GPU.ImageView)
		.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		.setSampler(Renderer.DepthBuffer.GPU.Sampler);

	vk::WriteDescriptorSet writeDepthSampler;
	writeDepthSampler
		.setImageInfo(depthImageInfo)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDstArrayElement(0)
		.setDstBinding(1)
		.setDstSet(DescriptorSet);

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


	std::array<vk::WriteDescriptorSet, 3> writes = {
		writeUniform,
		writeDepthSampler,
		writeUniformFullscreen,
	};

	Vulkan.Device.updateDescriptorSets(writes, {});
}
