#include "engine/hzpch.h"

#include "AdvancedFluidRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>
#include <engine/assets/AssetManager.h>

#include <engine/utils/PerformanceTimer.h>

const float SPHERE_RADIUS = .1f;

const uint32_t MAX_PARTICLES = 10000;
const uint32_t MAX_VERTICES = MAX_PARTICLES * 6;

// ------------------------------------------------------------------------

decltype(Vertex::Attributes) Vertex::Attributes = {
	// uint32_t location
	// uint32_t binding
	// Format format = Format::eUndefined
	// uint32_t offset
	vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position) },
	vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, UV) },
};

decltype(Vertex::Binding) Vertex::Binding = vk::VertexInputBindingDescription{
	0, // uint32_t binding
	sizeof(Vertex), // uint32_t stride
	vk::VertexInputRate::eVertex, // VertexInputRate inputRate
};

// ------------------------------------------------------------------------

bool AdvancedFluidRenderer::VInit()
{
	InitDepthPass();
	InitRayMarchPass();
	InitCompositionPass();

	// create command buffer
	CommandBuffer = Renderer::CreateSecondaryCommandBuffer();

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

	CameraController.SetPosition({ 0, -3, -10 });
	CameraController.SetOrientation(glm::quatLookAt(glm::normalize(-CameraController.Position), { 0, 1, 0 }));

	Camera.ComputeMatrices();
	
	// create particle buffer
	{
		ParticleBuffer.Size = sizeof(Particle) * MAX_PARTICLES + sizeof(int);

		ParticleBuffer.CPU.Create(
			ParticleBuffer.Size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
		);

		ParticleBuffer.GPU.Create(
			ParticleBuffer.Size,
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
	}

	// create vertex buffer
	if (!VertexBuffer.Create(sizeof(Vertex) * MAX_VERTICES))
		return false;

	return true;
}

void AdvancedFluidRenderer::VExit()
{
	DepthPass.FragmentShader.Destroy();
	DepthPass.VertexShader.Destroy();
	DepthPass.UniformBuffer.Destroy();
	DepthPass.Pipeline.Destroy();

	RayMarchPass.FragmentShader.Destroy();
	RayMarchPass.VertexShader.Destroy();
	RayMarchPass.UniformBuffer.Destroy();
	RayMarchPass.Pipeline.Destroy();

	CompositionPass.FragmentShader.Destroy();
	CompositionPass.VertexShader.Destroy();
	CompositionPass.UniformBuffer.Destroy();
	CompositionPass.Pipeline.Destroy();

	ParticleBuffer.CPU.Destroy();
	ParticleBuffer.GPU.Destroy();

	VertexBuffer.Destroy();
}

void AdvancedFluidRenderer::UpdateUniforms()
{
	Camera.ComputeMatrices();

	const auto& view = Camera.GetView();
	const auto& projection = Camera.GetProjection();

	// depth pass
	{
		DepthPass.Uniforms.View = view;
		DepthPass.Uniforms.Radius = SPHERE_RADIUS;
		DepthPass.Uniforms.Projection = projection;

		DepthPass.UniformBuffer.Map(&CompositionPass.Uniforms, sizeof(CompositionPass.Uniforms));
	}

	// ray march pass
	{
		RayMarchPass.Uniforms.ViewProjectionInv = glm::inverse(Camera.GetProjectionView());
		RayMarchPass.Uniforms.Resolution = glm::vec2{
			float(Renderer::GetSwapchainExtent().width),
			float(Renderer::GetSwapchainExtent().height),
		};

		RayMarchPass.UniformBuffer.Map(&CompositionPass.Uniforms, sizeof(CompositionPass.Uniforms));
	}

	// composition pass
	{
		CompositionPass.Uniforms.CameraPosition = CameraController.Position;
		CompositionPass.Uniforms.CameraDirection = CameraController.System[2];
		CompositionPass.Uniforms.LightDirection = glm::vec3(-1, -1, 1);

		CompositionPass.UniformBuffer.Map(&CompositionPass.Uniforms, sizeof(CompositionPass.Uniforms));
	}
}

void AdvancedFluidRenderer::UpdateDescriptorSet()
{
	std::vector<vk::WriteDescriptorSet> writes;

	// depth pass
	{
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo
			.setOffset(0)
			.setBuffer(DepthPass.UniformBuffer)
			.setRange(DepthPass.UniformBuffer.Size);

		vk::WriteDescriptorSet write;
		write
			.setDescriptorCount(1)
			.setDstBinding(0)
			.setDstSet(DepthPass.DescriptorSet)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(bufferInfo);

		writes.push_back(write);
	}

	// ray march pass
	{
		// uniform buffer
		{
			vk::DescriptorBufferInfo bufferInfo;
			bufferInfo
				.setOffset(0)
				.setBuffer(RayMarchPass.UniformBuffer)
				.setRange(RayMarchPass.UniformBuffer.Size);

			vk::WriteDescriptorSet write;
			write
				.setDescriptorCount(1)
				.setDstBinding(0)
				.setDstSet(RayMarchPass.DescriptorSet)
				.setDstArrayElement(0)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setBufferInfo(bufferInfo);

			writes.push_back(write);
		}

		// constant particle buffer
		{
			vk::DescriptorBufferInfo bufferInfo;
			bufferInfo
				.setBuffer(ParticleBuffer.GPU)
				.setRange(ParticleBuffer.Size);

			vk::WriteDescriptorSet write;
			write
				.setDescriptorCount(1)
				.setDstBinding(1)
				.setDstSet(RayMarchPass.DescriptorSet)
				.setDstArrayElement(0)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setBufferInfo(bufferInfo);

			writes.push_back(write);
		}

		// depth input attachment
		{
			vk::DescriptorImageInfo imageInfo;
			imageInfo
				.setImageView(DepthBuffer.GetView())
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

			vk::WriteDescriptorSet write;
			write
				.setDescriptorCount(1)
				.setDstBinding(2)
				.setDstSet(CompositionPass.DescriptorSet)
				.setDstArrayElement(0)
				.setDescriptorType(vk::DescriptorType::eInputAttachment)
				.setImageInfo(imageInfo);

			writes.push_back(write);
		}
	}

	// composition pass
	{
		// uniform buffer
		{
			vk::DescriptorBufferInfo bufferInfo;
			bufferInfo
				.setOffset(0)
				.setBuffer(CompositionPass.UniformBuffer)
				.setRange(CompositionPass.UniformBuffer.Size);

			vk::WriteDescriptorSet write;
			write
				.setDescriptorCount(1)
				.setDstBinding(0)
				.setDstSet(CompositionPass.DescriptorSet)
				.setDstArrayElement(0)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setBufferInfo(bufferInfo);

			writes.push_back(write);
		}

		// positions attachment
		{
			vk::DescriptorImageInfo imageInfo;
			imageInfo
				.setImageView(PositionsAttachment.ImageView)
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

			vk::WriteDescriptorSet write;
			write
				.setDescriptorCount(1)
				.setDstBinding(1)
				.setDstSet(CompositionPass.DescriptorSet)
				.setDstArrayElement(0)
				.setDescriptorType(vk::DescriptorType::eInputAttachment)
				.setImageInfo(imageInfo);

			writes.push_back(write);
		}

		// normals attachment
		{
			vk::DescriptorImageInfo imageInfo;
			imageInfo
				.setImageView(NormalsAttachment.ImageView)
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

			vk::WriteDescriptorSet write;
			write
				.setDescriptorCount(1)
				.setDstBinding(2)
				.setDstSet(CompositionPass.DescriptorSet)
				.setDstArrayElement(0)
				.setDescriptorType(vk::DescriptorType::eInputAttachment)
				.setImageInfo(imageInfo);

			writes.push_back(write);
		}
	}

	Device.updateDescriptorSets(writes, {});
}

void AdvancedFluidRenderer::UpdateDescriptorSetDepth()
{
	std::vector<vk::WriteDescriptorSet> writes;

	vk::DescriptorBufferInfo bufferInfo;
	bufferInfo
		.setOffset(0)
		.setBuffer(DepthPass.UniformBuffer)
		.setRange(DepthPass.UniformBuffer.Size);

	vk::WriteDescriptorSet write;
	write
		.setDescriptorCount(1)
		.setDstBinding(0)
		.setDstSet(DepthPass.DescriptorSet)
		.setDstArrayElement(0)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setBufferInfo(bufferInfo);

	writes.push_back(write);

	Device.updateDescriptorSets(writes, {});
}

void AdvancedFluidRenderer::UpdateDescriptorSetRayMarch()
{
	std::vector<vk::WriteDescriptorSet> writes;

	// uniform buffer
	{
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo
			.setOffset(0)
			.setBuffer(RayMarchPass.UniformBuffer)
			.setRange(RayMarchPass.UniformBuffer.Size);

		vk::WriteDescriptorSet write;
		write
			.setDescriptorCount(1)
			.setDstBinding(0)
			.setDstSet(RayMarchPass.DescriptorSet)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(bufferInfo);

		writes.push_back(write);
	}

	// constant particle buffer
	{
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo
			.setBuffer(ParticleBuffer.GPU)
			.setRange(ParticleBuffer.Size);

		vk::WriteDescriptorSet write;
		write
			.setDescriptorCount(1)
			.setDstBinding(1)
			.setDstSet(RayMarchPass.DescriptorSet)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(bufferInfo);

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
			.setDescriptorCount(1)
			.setDstBinding(2)
			.setDstSet(RayMarchPass.DescriptorSet)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eInputAttachment)
			.setImageInfo(imageInfo);

		writes.push_back(write);
	}

	Device.updateDescriptorSets(writes, {});
}

void AdvancedFluidRenderer::UpdateDescriptorSetComposition()
{
	std::vector<vk::WriteDescriptorSet> writes;

	// uniform buffer
	{
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo
			.setOffset(0)
			.setBuffer(CompositionPass.UniformBuffer)
			.setRange(CompositionPass.UniformBuffer.Size);

		vk::WriteDescriptorSet write;
		write
			.setDescriptorCount(1)
			.setDstBinding(0)
			.setDstSet(CompositionPass.DescriptorSet)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(bufferInfo);

		writes.push_back(write);
	}

	// positions attachment
	{
		vk::DescriptorImageInfo imageInfo;
		imageInfo
			.setImageView(PositionsAttachment.ImageView)
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

		vk::WriteDescriptorSet write;
		write
			.setDescriptorCount(1)
			.setDstBinding(1)
			.setDstSet(CompositionPass.DescriptorSet)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eInputAttachment)
			.setImageInfo(imageInfo);

		writes.push_back(write);
	}

	// normals attachment
	{
		vk::DescriptorImageInfo imageInfo;
		imageInfo
			.setImageView(NormalsAttachment.ImageView)
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

		vk::WriteDescriptorSet write;
		write
			.setDescriptorCount(1)
			.setDstBinding(2)
			.setDstSet(CompositionPass.DescriptorSet)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eInputAttachment)
			.setImageInfo(imageInfo);

		writes.push_back(write);
	}

	Device.updateDescriptorSets(writes, {});
}

void AdvancedFluidRenderer::CollectRenderData()
{
	CurrentVertex = 0;

	Vertex* target = 
		reinterpret_cast<Vertex*>(Device.mapMemory(VertexBuffer.BufferCPU.Memory, 0, VertexBuffer.BufferCPU.Size));

	glm::vec3 up(0, 1, 0);

	/*struct {
		glm::vec3 Position{ 0, 0, 0 };
	} particle;*/

	Vertex quad[6];

	for (auto& particle : Dataset->Snapshots[CurrentFrame])
	{
		glm::vec3 z = CameraController.System[2];
		glm::vec3 x = glm::normalize(glm::cross(up, z));
		glm::vec3 y = glm::normalize(glm::cross(x, z));

		glm::vec3 p(particle[0], particle[1], particle[2]);

		glm::vec3 topLeft = p + SPHERE_RADIUS * (-x + y);
		glm::vec3 topRight = p + SPHERE_RADIUS * (+x + y);
		glm::vec3 bottomLeft = p + SPHERE_RADIUS * (-x - y);
		glm::vec3 bottomRight = p + SPHERE_RADIUS * (+x - y);

		quad[0] = { topLeft, { 0, 0 } };
		quad[1] = { bottomLeft, { 0, 1 } };
		quad[2] = { topRight, { 1, 0 } };

		quad[3] = { topRight, { 1, 0 } };
		quad[4] = { bottomLeft, { 0, 1 } };
		quad[5] = { bottomRight, { 1, 1 } };

		memcpy(target + CurrentVertex, quad, sizeof(quad));
		CurrentVertex += 6;
	}

	Device.unmapMemory(VertexBuffer.BufferCPU.Memory);
}

void AdvancedFluidRenderer::Render()
{
	PerformanceTimer _timer("AdvancedFluidRenderer::RenderFrame");

	if (CurrentFrame > Dataset->Snapshots.size() - 1)
		CurrentFrame = 0;

	if (!Paused)
	{
		PerformanceTimer _timer("CollectRenderData");

		CollectRenderData();
	}

	UpdateParticleBuffer();
	UpdateUniforms();

	//UpdateDescriptorSet();

	// begin command buffer
	auto& cmd = CommandBuffer;

	// issue buffer copies
	if (CurrentVertex > 0)
	{
		auto copyCmd = Command::BeginOneTimeCommand();
		VertexBuffer.Copy(copyCmd);
		Command::EndOneTimeCommand(copyCmd);
	}

	// 0. pass: depth
	{
		UpdateDescriptorSetDepth();

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			DepthPass.Pipeline.GetLayout(),
			0,
			DepthPass.DescriptorSet,
			{}
		);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, DepthPass.Pipeline);

		const vk::DeviceSize offset = 0;
		cmd.bindVertexBuffers(0, VertexBuffer.BufferGPU.Buffer, offset);

		cmd.draw(CurrentVertex, 1, 0, 0);
	}

	// 1. pass: ray march
	{
		UpdateDescriptorSetRayMarch();

		cmd.nextSubpass(vk::SubpassContents::eInline);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			RayMarchPass.Pipeline.GetLayout(),
			0,
			RayMarchPass.DescriptorSet,
			{}
		);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, RayMarchPass.Pipeline);

		cmd.draw(3, 1, 0, 0);
	}

	// 2. pass: composition
	{
		UpdateDescriptorSetComposition();

		cmd.nextSubpass(vk::SubpassContents::eInline);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			CompositionPass.Pipeline.GetLayout(),
			0,
			CompositionPass.DescriptorSet,
			{}
		);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, CompositionPass.Pipeline);

		cmd.draw(3, 1, 0, 0);
	}

	//cmd.end();

	if (!Paused)
		CurrentFrame++;

	RenderUI();
}

void AdvancedFluidRenderer::RenderUI()
{
	ImGui::Begin("AdvancedFluidRenderer");

	ImGui::Checkbox("Paused", &Paused);

	ImGui::End();
}

void AdvancedFluidRenderer::UpdateParticleBuffer()
{
	auto& snapshot = Dataset->Snapshots[CurrentFrame];

	// collect particle data
	int NumParticles = snapshot.size();

	char8_t* data = reinterpret_cast<char8_t*>(
		Device.mapMemory(ParticleBuffer.CPU.Memory, 0, ParticleBuffer.Size)
	);
	
	//memcpy(data, &NumParticles, sizeof(int));
	//memcpy(data + sizeof(int), snapshot.data(), ParticleBuffer.Size - sizeof(NumParticles));

	Device.unmapMemory(ParticleBuffer.CPU.Memory);

	// issue copy
	vk::BufferCopy region{ 0, 0, ParticleBuffer.Size };
	auto cmd = Command::BeginOneTimeCommand();

	cmd.copyBuffer(ParticleBuffer.CPU.Buffer, ParticleBuffer.GPU.Buffer, region);
	
	Command::EndOneTimeCommand(cmd);
}

void AdvancedFluidRenderer::InitDepthPass()
{
	// shaders
	DepthPass.VertexShader.Create(
		AssetPathRel("shaders/depthPass.vert"),
		vk::ShaderStageFlagBits::eVertex,
		{
			DescriptorSetLayoutBinding{
				0, 1,
				vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			}
		}
	);

	DepthPass.FragmentShader.Create(
		AssetPathRel("shaders/depthPass.frag"),
		vk::ShaderStageFlagBits::eFragment,
		{}
	);

	// pipeline
	DepthPass.Pipeline.Create(
		{ DepthPass.VertexShader, DepthPass.FragmentShader },
		0, // subpass
		Vertex::Attributes,
		Vertex::Binding,
		vk::PrimitiveTopology::eTriangleList,
		true, // depth
		false // blend
	);

	// uniform buffer
	DepthPass.UniformBuffer.Create(
		sizeof(DepthPass.Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// descriptor set
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(DepthPass.Pipeline.GetDescriptorSetLayout());

	DepthPass.DescriptorSet = Device.allocateDescriptorSets(info).front();
}

void AdvancedFluidRenderer::InitRayMarchPass()
{
	// shaders
	RayMarchPass.VertexShader.Create(
		AssetPathRel("shaders/fullscreen.vert"),
		vk::ShaderStageFlagBits::eVertex,
		{}
	);

	RayMarchPass.FragmentShader.Create(
		AssetPathRel("shaders/rayMarch.frag"),
		vk::ShaderStageFlagBits::eFragment,
		{
			DescriptorSetLayoutBinding{
				0, 1,
				vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eFragment,
			},
			DescriptorSetLayoutBinding{
				1, 1,
				vk::DescriptorType::eStorageBuffer,
				vk::ShaderStageFlagBits::eFragment,
			},
			DescriptorSetLayoutBinding{
				2, 1,
				vk::DescriptorType::eInputAttachment,
				vk::ShaderStageFlagBits::eFragment,
			},
		}
	);

	// pipeline
	RayMarchPass.Pipeline.Create(
		{ RayMarchPass.VertexShader, RayMarchPass.FragmentShader },
		1, // subpass
		Vertex::Attributes,
		Vertex::Binding,
		vk::PrimitiveTopology::eTriangleList,
		false,
		false,
		2
	);

	// uniform buffer
	RayMarchPass.UniformBuffer.Create(
		sizeof(RayMarchPass.Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// descriptor set
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(RayMarchPass.Pipeline.GetDescriptorSetLayout());

	RayMarchPass.DescriptorSet = Device.allocateDescriptorSets(info).front();
}

void AdvancedFluidRenderer::InitCompositionPass()
{
	// shaders
	CompositionPass.VertexShader.Create(
		AssetPathRel("shaders/fullscreen.vert"),
		vk::ShaderStageFlagBits::eVertex,
		{}
	);

	CompositionPass.FragmentShader.Create(
		AssetPathRel("shaders/composition.frag"),
		vk::ShaderStageFlagBits::eFragment,
		{
			DescriptorSetLayoutBinding{
				0, 1,
				vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eFragment,
			},
			DescriptorSetLayoutBinding{
				1, 1,
				vk::DescriptorType::eInputAttachment,
				vk::ShaderStageFlagBits::eFragment,
			},
			DescriptorSetLayoutBinding{
				2, 1,
				vk::DescriptorType::eInputAttachment,
				vk::ShaderStageFlagBits::eFragment,
			},
		}
	);

	// pipeline
	CompositionPass.Pipeline.Create(
		{ CompositionPass.VertexShader, CompositionPass.FragmentShader },
		2, // subpass
		Vertex::Attributes,
		Vertex::Binding,
		vk::PrimitiveTopology::eTriangleList,
		false,
		true
	);

	// uniform buffer
	CompositionPass.UniformBuffer.Create(
		sizeof(CompositionPass.Uniforms),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// descriptor set
	vk::DescriptorSetAllocateInfo info;
	info.setDescriptorPool(DescriptorPool)
		.setDescriptorSetCount(1)
		.setSetLayouts(CompositionPass.Pipeline.GetDescriptorSetLayout());

	CompositionPass.DescriptorSet = Device.allocateDescriptorSets(info).front();
}
