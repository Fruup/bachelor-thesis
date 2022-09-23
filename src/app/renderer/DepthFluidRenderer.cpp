#include "engine/hzpch.h"

#include "DepthFluidRenderer.h"

#include <engine/renderer/objects/Command.h>

// ------------------------------------------------------------------------

const float SPHERE_RADIUS = .01f;

const uint32_t MAX_PARTICLES = 10000;
const uint32_t MAX_VERTICES = MAX_PARTICLES * 6;

// ------------------------------------------------------------------------

decltype(DepthFluidRenderer::Vertex::Attributes) DepthFluidRenderer::Vertex::Attributes = {
	// uint32_t location
	// uint32_t binding
	// Format format = Format::eUndefined
	// uint32_t offset
	vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position) },
	vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, UV) },
};

decltype(DepthFluidRenderer::Vertex::Binding) DepthFluidRenderer::Vertex::Binding = vk::VertexInputBindingDescription{
	0, // uint32_t binding
	sizeof(DepthFluidRenderer::Vertex), // uint32_t stride
	vk::VertexInputRate::eVertex, // VertexInputRate inputRate
};

// ------------------------------------------------------------------------

bool DepthFluidRenderer::VInit()
{
	if (!CreatePipeline()) return false;
	if (!CreateDescriptorSet()) return false;
	if (!CreateVertexBuffer()) return false;
	if (!CreateUniformBuffer()) return false;

	ImGuiSubpass = 2;

	// init camera
	float aspect =
		float(Renderer::GetSwapchainExtent().width) /
		float(Renderer::GetSwapchainExtent().height);

	Camera = Camera3D(
		glm::radians(45.0f),
		aspect,
		0.1f,
		1000.0f
	);

	CameraController.SetPosition({ 0, 3, -10 });
	CameraController.SetOrientation(glm::quatLookAt(glm::normalize(-CameraController.Position), { 0, 1, 0 }));
	
	Camera.ComputeMatrices();

	return true;
}

void DepthFluidRenderer::VExit()
{
	DepthPass.VertexShader.Destroy();
	DepthPass.FragmentShader.Destroy();
	DepthPass.Pipeline.Destroy();
	DepthPass.UniformBuffer.Destroy();
	DepthPass.VertexBuffer.Destroy();

	CompositionPass.VertexShader.Destroy();
	CompositionPass.FragmentShader.Destroy();
	CompositionPass.Pipeline.Destroy();
	CompositionPass.UniformBuffer.Destroy();
}

void DepthFluidRenderer::Render()
{
	if (!Paused)
		CurrentFrame = (CurrentFrame + 1) % Dataset.Snapshots.size();

	UpdateUniforms();

	if (!Paused)
		CollectRenderData();

	// 0. pass: depth
	{
		UpdateDescriptorSetDepth();

		CommandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			DepthPass.Pipeline.GetLayout(),
			0,
			DepthPass.DescriptorSet,
			{});

		CommandBuffer.bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			DepthPass.Pipeline);

		const uint32_t offset = 0;
		CommandBuffer.bindVertexBuffers(
			0,
			DepthPass.VertexBuffer.BufferGPU.Buffer,
			offset);

		CommandBuffer.draw(DepthPass.NumVertices, 1, 0, 0);
	}

	// 1. pass: compose
	{
		CommandBuffer.nextSubpass(vk::SubpassContents::eInline);

		UpdateDescriptorSetComposition();

		CommandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			CompositionPass.Pipeline.GetLayout(),
			0,
			CompositionPass.DescriptorSet,
			{});

		CommandBuffer.bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			CompositionPass.Pipeline);

		CommandBuffer.draw(3, 1, 0, 0);
	}

	if (!Paused)
		CurrentFrame++;

	RenderUI();
}

bool DepthFluidRenderer::CreateRenderPass()
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

	vk::SubpassDescription depthSubpass;
	{
		vk::AttachmentReference depth(depthAttachmentIndex, vk::ImageLayout::eDepthStencilAttachmentOptimal);

		depthSubpass
			.setPDepthStencilAttachment(&depth)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	}

	vk::SubpassDescription compositionSubpass;
	{
		vk::AttachmentReference input(
			depthAttachmentIndex,
			vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal);

		vk::AttachmentReference color[] = {
			vk::AttachmentReference(screenAttachmentIndex, vk::ImageLayout::eColorAttachmentOptimal),
		};

		compositionSubpass
			.setInputAttachments(input)
			.setColorAttachments(color)
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

	DepthPass.Index = 0;
	CompositionPass.Index = 1;
	ImGuiSubpass = 2;

	std::array<vk::SubpassDescription, 3> subpasses = {
		depthSubpass,
		compositionSubpass,
		imguiSubpass,
	};

	// dependencies
	std::vector<vk::SubpassDependency> dependencies;

	{
		vk::SubpassDependency dependency;
		dependency
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(DepthPass.Index)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)
			.setSrcAccessMask(vk::AccessFlagBits::eNone)
			.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

		dependencies.push_back(dependency);
	}

	{
		vk::SubpassDependency dependency;
		dependency
			.setSrcSubpass(DepthPass.Index)
			.setDstSubpass(CompositionPass.Index)
			.setSrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits::eNone)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

		dependencies.push_back(dependency);
	}

	{
		vk::SubpassDependency dependency;
		dependency
			.setSrcSubpass(CompositionPass.Index)
			.setDstSubpass(ImGuiSubpass)
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

bool DepthFluidRenderer::CreateFramebuffers()
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

bool DepthFluidRenderer::CreateUniformBuffer()
{
	DepthPass.UniformBuffer.Create(
		sizeof(DepthPass.Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent);

	CompositionPass.UniformBuffer.Create(
		sizeof(CompositionPass.Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent);

	return true;
}

bool DepthFluidRenderer::CreateVertexBuffer()
{
	return DepthPass.VertexBuffer.Create(sizeof(Vertex) * MAX_VERTICES);
}

bool DepthFluidRenderer::CreatePipeline()
{
	bool r;

	// 0. pass: depth
	{
		// shaders
		r = DepthPass.VertexShader.Create(
			"shaders/depthRenderer/depthPass.vert",
			vk::ShaderStageFlagBits::eVertex,
			{
				DescriptorSetLayoutBinding{
					0, 1,
					vk::DescriptorType::eUniformBuffer,
					vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
				},
			}
		);

		r = DepthPass.FragmentShader.Create(
			"shaders/depthRenderer/depthPass.frag",
			vk::ShaderStageFlagBits::eFragment,
			{}
		);

		if (!r) return false;

		// pipeline
		r = DepthPass.Pipeline.Create(
			{ DepthPass.VertexShader, DepthPass.FragmentShader },
			0, // subpass
			Vertex::Attributes,
			Vertex::Binding,
			vk::PrimitiveTopology::eTriangleList,
			true, // depth
			false // blend
		);

		if (!r) return false;
	}

	// 1. pass: compose
	{
		// shaders
		r = CompositionPass.VertexShader.Create(
			"shaders/depthRenderer/fullscreen.vert",
			vk::ShaderStageFlagBits::eVertex,
			{}
		);

		r = CompositionPass.FragmentShader.Create(
			"shaders/depthRenderer/composition.frag",
			vk::ShaderStageFlagBits::eFragment,
			{
				DescriptorSetLayoutBinding{
					0, 1,
					vk::DescriptorType::eUniformBuffer,
					vk::ShaderStageFlagBits::eFragment
				},
				DescriptorSetLayoutBinding{
					1, 1,
					vk::DescriptorType::eInputAttachment,
					vk::ShaderStageFlagBits::eFragment
				},
			}
		);

		if (!r) return false;

		// pipeline
		r = CompositionPass.Pipeline.Create(
			{ CompositionPass.VertexShader, CompositionPass.FragmentShader },
			1, // subpass
			Vertex::Attributes,
			Vertex::Binding,
			vk::PrimitiveTopology::eTriangleList,
			false, // depth
			false // blend
		);

		if (!r) return false;
	}

	return true;
}

bool DepthFluidRenderer::CreateDescriptorSet()
{
	// 0. pass
	{
		vk::DescriptorSetAllocateInfo info;
		info.setDescriptorPool(DescriptorPool)
			.setDescriptorSetCount(1)
			.setSetLayouts(DepthPass.Pipeline.GetDescriptorSetLayout());

		DepthPass.DescriptorSet = Device.allocateDescriptorSets(info).front();
	}

	// 1. pass
	{
		vk::DescriptorSetAllocateInfo info;
		info.setDescriptorPool(DescriptorPool)
			.setDescriptorSetCount(1)
			.setSetLayouts(CompositionPass.Pipeline.GetDescriptorSetLayout());

		CompositionPass.DescriptorSet = Device.allocateDescriptorSets(info).front();
	}

	return true;
}

void DepthFluidRenderer::UpdateUniforms()
{
	// 0. pass
	{
		DepthPass.Uniforms.Projection = Camera.GetProjection();
		DepthPass.Uniforms.View = Camera.GetView();
		DepthPass.Uniforms.Radius = SPHERE_RADIUS;

		// copy
		DepthPass.UniformBuffer.Map(&DepthPass.Uniforms, sizeof(DepthPass.Uniforms));
	}

	// 1. pass
	{
		auto testH = glm::inverse(Camera.GetProjection() * Camera.GetView()) * glm::vec4(0, 0, .5, 1);
		auto test = glm::vec3(testH) / testH.w;

		CompositionPass.Uniforms.InvViewProjection = glm::inverse(Camera.GetProjection());
		CompositionPass.Uniforms.CameraPosition = CameraController.Position;
		CompositionPass.Uniforms.CameraDirection = CameraController.System[2];
		CompositionPass.Uniforms.LightDirection = glm::vec3(1, -1, 1);
		CompositionPass.Uniforms.LightDirection = glm::vec3(0, 0, 1);

		// copy
		CompositionPass.UniformBuffer.Map(&CompositionPass.Uniforms, sizeof(CompositionPass.Uniforms));
	}
}

void DepthFluidRenderer::UpdateDescriptorSetDepth()
{
	std::vector<vk::WriteDescriptorSet> writes;

	{
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo
			.setBuffer(DepthPass.UniformBuffer)
			.setOffset(0)
			.setRange(DepthPass.UniformBuffer.Size);

		vk::WriteDescriptorSet write;
		write
			.setBufferInfo(bufferInfo)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDstBinding(0)
			.setDstSet(DepthPass.DescriptorSet);

		writes.push_back(write);
	}

	Device.updateDescriptorSets(writes, {});
}

void DepthFluidRenderer::UpdateDescriptorSetComposition()
{
	std::vector<vk::WriteDescriptorSet> writes;

	{
		// uniform buffer
		{
			vk::DescriptorBufferInfo bufferInfo;
			bufferInfo
				.setBuffer(CompositionPass.UniformBuffer)
				.setOffset(0)
				.setRange(CompositionPass.UniformBuffer.Size);

			vk::WriteDescriptorSet write;
			write
				.setBufferInfo(bufferInfo)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDstBinding(0)
				.setDstSet(CompositionPass.DescriptorSet);

			writes.push_back(write);
		}

		// depth input attachment
		{
			vk::DescriptorImageInfo imageInfo;
			imageInfo
				.setImageView(DepthBuffer.GetView())
				.setImageLayout(vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal);

			vk::WriteDescriptorSet write;
			write
				.setImageInfo(imageInfo)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eInputAttachment)
				.setDstBinding(1)
				.setDstSet(CompositionPass.DescriptorSet);

			writes.push_back(write);
		}
	}

	Device.updateDescriptorSets(writes, {});
}

void DepthFluidRenderer::CollectRenderData()
{
	DepthPass.NumVertices = 0;

	Vertex* target = reinterpret_cast<Vertex*>(
		Device.mapMemory(DepthPass.VertexBuffer.BufferCPU.Memory, 0, DepthPass.VertexBuffer.BufferCPU.Size));

	const glm::vec3 up(0, 1, 0);

	for (auto& particle : Dataset.Snapshots[CurrentFrame])
	{
		glm::vec3 z = CameraController.System[2];
		glm::vec3 x = glm::normalize(glm::cross(up, z));
		glm::vec3 y = glm::normalize(glm::cross(x, z));

		glm::vec3 topLeft = particle.Position + SPHERE_RADIUS * (-x + y);
		glm::vec3 topRight = particle.Position + SPHERE_RADIUS * (+x + y);
		glm::vec3 bottomLeft = particle.Position + SPHERE_RADIUS * (-x - y);
		glm::vec3 bottomRight = particle.Position + SPHERE_RADIUS * (+x - y);

		target[DepthPass.NumVertices++] = { topLeft, { 0, 0 } };
		target[DepthPass.NumVertices++] = { bottomLeft, { 0, 1 } };
		target[DepthPass.NumVertices++] = { topRight, { 1, 0 } };
		target[DepthPass.NumVertices++] = { topRight, { 1, 0 } };
		target[DepthPass.NumVertices++] = { bottomLeft, { 0, 1 } };
		target[DepthPass.NumVertices++] = { bottomRight, { 1, 1 } };
	}

	Device.unmapMemory(DepthPass.VertexBuffer.BufferCPU.Memory);

	// copy
	if (DepthPass.NumVertices > 0)
	{
		auto cmd = Command::BeginOneTimeCommand();
		DepthPass.VertexBuffer.Copy(cmd);
		Command::EndOneTimeCommand(cmd);
	}
}

void DepthFluidRenderer::RenderUI()
{
	ImGui::Begin("DepthFluidRenderer");

	ImGui::Checkbox("Paused", &Paused);

	ImGui::End();
}
