#include "engine/hzpch.h"

#include "PointFluidRenderer.h"

#include <engine/renderer/objects/Command.h>

// ------------------------------------------------------------------------

const float SPHERE_RADIUS = .1f;

const uint32_t MAX_PARTICLES = 10000;
const uint32_t MAX_VERTICES = MAX_PARTICLES * 6;

// ------------------------------------------------------------------------

decltype(PointFluidRenderer::Vertex::Attributes) PointFluidRenderer::Vertex::Attributes = {
	// uint32_t location
	// uint32_t binding
	// Format format = Format::eUndefined
	// uint32_t offset
	vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position) },
	vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, UV) },
};

decltype(PointFluidRenderer::Vertex::Binding) PointFluidRenderer::Vertex::Binding = vk::VertexInputBindingDescription{
	0, // uint32_t binding
	sizeof(PointFluidRenderer::Vertex), // uint32_t stride
	vk::VertexInputRate::eVertex, // VertexInputRate inputRate
};

// ------------------------------------------------------------------------

bool PointFluidRenderer::VInit()
{
	if (!CreatePipeline()) return false;
	if (!CreateDescriptorSet()) return false;
	if (!CreateVertexBuffer()) return false;
	if (!CreateUniformBuffer()) return false;

	ImGuiSubpass = 1;

	// init camera
	float aspect =
		float(Renderer::GetSwapchainExtent().width) /
		float(Renderer::GetSwapchainExtent().height);

	Camera = Camera3D(
		glm::radians(45.0f),
		aspect,
		0.1f,
		100.0f
	);

	CameraController.SetPosition({ 0, 3, -10 });
	CameraController.SetOrientation(glm::quatLookAt(glm::normalize(-CameraController.Position), { 0, 1, 0 }));

	CameraController.ComputeMatrices();

	return true;
}

void PointFluidRenderer::VExit()
{
	VertexShader.Destroy();
	FragmentShader.Destroy();

	Pipeline.Destroy();

	UniformBuffer.Destroy();
	VertexBuffer.Destroy();
}

void PointFluidRenderer::Render()
{
	if (!Paused)
		CurrentFrame = (CurrentFrame + 1) % Dataset->Snapshots.size();

	UpdateUniforms();
	UpdateDescriptorSets();
	
	if (!Paused)
		CollectRenderData();

	{
		CommandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			Pipeline.GetLayout(),
			0,
			DescriptorSet,
			{});
		
		CommandBuffer.bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			Pipeline);

		const uint32_t offset = 0;
		CommandBuffer.bindVertexBuffers(0, VertexBuffer.BufferGPU.Buffer, offset);

		CommandBuffer.draw(NumVertices, 1, 0, 0);
	}

	if (!Paused)
		CurrentFrame++;

	RenderUI();
}

bool PointFluidRenderer::CreateRenderPass()
{
	// attachments
	const uint32_t screenAttachmentIndex = 0;
	const uint32_t depthAttachmentIndex = 1;

	vk::AttachmentDescription screenAttachment;
	screenAttachment
		.setFormat(SwapchainFormat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	vk::AttachmentDescription depthAttachment;
	depthAttachment
		.setFormat(vk::Format::eD32Sfloat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
		/*.setFinalLayout(vk::ImageLayout::eTransferSrcOptimal)*/;

	// subpasses
	vk::SubpassDescription renderSubpass;
	{
		vk::AttachmentReference depth(depthAttachmentIndex, vk::ImageLayout::eDepthStencilAttachmentOptimal);

		vk::AttachmentReference color[] = {
			vk::AttachmentReference(screenAttachmentIndex, vk::ImageLayout::eColorAttachmentOptimal),
		};

		renderSubpass
			.setColorAttachments(color)
			.setPDepthStencilAttachment(&depth)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	}

	vk::SubpassDescription imguiSubpass;
	{
		vk::AttachmentReference color[] = {
			vk::AttachmentReference(screenAttachmentIndex, vk::ImageLayout::eColorAttachmentOptimal),
		};

		imguiSubpass
			.setColorAttachments(color)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	}

	const uint32_t renderSubpassIndex = 0;
	const uint32_t imguiSubpassIndex = 1;

	std::array<vk::SubpassDescription, 2> subpasses = {
		renderSubpass,
		imguiSubpass,
	};

	// dependencies
	std::vector<vk::SubpassDependency> dependencies;

	{
		vk::SubpassDependency dependency;
		dependency
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(renderSubpassIndex)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits::eNone)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

		dependencies.push_back(dependency);
	}

	{
		vk::SubpassDependency dependency;
		dependency
			.setSrcSubpass(renderSubpassIndex)
			.setDstSubpass(imguiSubpassIndex)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

		dependencies.push_back(dependency);
	}

	// create render pass
	{
		std::vector<vk::AttachmentDescription> attachments = {
			screenAttachment,
			depthAttachment,
		};

		vk::RenderPassCreateInfo info;
		info.setAttachments(attachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		RenderPass = Device.createRenderPass(info);
		if (!RenderPass)
		{
			SPDLOG_ERROR("Failed to create render pass!");
			return false;
		}
	}

	return true;
}

bool PointFluidRenderer::CreateFramebuffers()
{
	SwapchainFramebuffers.resize(SwapchainImages.size());

	for (size_t i = 0; i < SwapchainFramebuffers.size(); i++)
	{
		std::vector<vk::ImageView> attachments = {
			SwapchainImageViews[i],
			DepthBuffer.GetView(),
		};

		vk::FramebufferCreateInfo info;
		info.setAttachments(attachments)
			.setRenderPass(RenderPass)
			.setWidth(SwapchainExtent.width)
			.setHeight(SwapchainExtent.height)
			.setLayers(1);

		SwapchainFramebuffers[i] = Device.createFramebuffer(info);
		if (!SwapchainFramebuffers[i])
		{
			SPDLOG_ERROR("Swapchain framebuffer creation failed!");
			return false;
		}
	}

	return true;
}

bool PointFluidRenderer::CreateUniformBuffer()
{
	return UniformBuffer.Create(
		sizeof(Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent);
}

bool PointFluidRenderer::CreateVertexBuffer()
{
	return VertexBuffer.Create(sizeof(Vertex) * MAX_VERTICES);
}

bool PointFluidRenderer::CreatePipeline()
{
	bool r;

	// shaders
	r = VertexShader.Create(
		"shaders/point.vert",
		vk::ShaderStageFlagBits::eVertex,
		{
			DescriptorSetLayoutBinding{
				0, 1,
				vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
			},
		}
	);

	r = FragmentShader.Create(
		"shaders/point.frag",
		vk::ShaderStageFlagBits::eFragment,
		{}
	);

	if (!r) return false;

	// pipeline
	r = Pipeline.Create(
		{ VertexShader, FragmentShader },
		0, // subpass
		Vertex::Attributes,
		Vertex::Binding,
		vk::PrimitiveTopology::eTriangleList,
		true, // depth
		true // blend
	);
	
	return true;
}

bool PointFluidRenderer::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(Pipeline.GetDescriptorSetLayout());

	DescriptorSet = Device.allocateDescriptorSets(info).front();

	return true;
}

void PointFluidRenderer::UpdateUniforms()
{
	Uniforms.Projection = Camera.GetProjection();
	Uniforms.View = Camera.GetView();
	Uniforms.Radius = SPHERE_RADIUS;

	// copy
	UniformBuffer.Map(&Uniforms, sizeof(Uniforms));
}

void PointFluidRenderer::UpdateDescriptorSets()
{
	std::vector<vk::WriteDescriptorSet> writes;

	{
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo
			.setBuffer(UniformBuffer)
			.setOffset(0)
			.setRange(UniformBuffer.Size);

		vk::WriteDescriptorSet write;
		write
			.setBufferInfo(bufferInfo)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDstBinding(0)
			.setDstSet(DescriptorSet);

		writes.push_back(write);
	}

	Device.updateDescriptorSets(writes, {});
}

void PointFluidRenderer::CollectRenderData()
{
	NumVertices = 0;

	Vertex* target = reinterpret_cast<Vertex*>(
		Device.mapMemory(VertexBuffer.BufferCPU.Memory, 0, VertexBuffer.BufferCPU.Size));

	const glm::vec3 up(0, 1, 0);

	for (auto& particle : Dataset->Snapshots[CurrentFrame])
	{
		glm::vec3 z = CameraController.System[2];
		glm::vec3 x = glm::normalize(glm::cross(up, z));
		glm::vec3 y = glm::normalize(glm::cross(x, z));

		glm::vec3 topLeft = particle.Position + SPHERE_RADIUS * (-x + y);
		glm::vec3 topRight = particle.Position + SPHERE_RADIUS * (+x + y);
		glm::vec3 bottomLeft = particle.Position + SPHERE_RADIUS * (-x - y);
		glm::vec3 bottomRight = particle.Position + SPHERE_RADIUS * (+x - y);

		target[NumVertices++] = { topLeft, { 0, 0 } };
		target[NumVertices++] = { bottomLeft, { 0, 1 } };
		target[NumVertices++] = { topRight, { 1, 0 } };
		target[NumVertices++] = { topRight, { 1, 0 } };
		target[NumVertices++] = { bottomLeft, { 0, 1 } };
		target[NumVertices++] = { bottomRight, { 1, 1 } };
	}

	Device.unmapMemory(VertexBuffer.BufferCPU.Memory);

	// copy
	if (NumVertices > 0)
	{
		auto cmd = Command::BeginOneTimeCommand();
		VertexBuffer.Copy(cmd);
		Command::EndOneTimeCommand(cmd);
	}
}

void PointFluidRenderer::RenderUI()
{
	ImGui::Begin("PointFluidRenderer");

	ImGui::Checkbox("Paused", &Paused);

	ImGui::End();
}
