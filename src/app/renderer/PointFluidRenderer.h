#pragma once

#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/Pipeline.h>
#include <engine/renderer/objects/VertexBuffer.h>
#include <engine/renderer/Renderer.h>

//#define VMA_IMPLEMENTATION
//#include <vma/vk_mem_alloc.h>

#include "FluidRenderer.h"

class PointFluidRenderer : public Renderer, public FluidRenderer
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec2 UV;

		static const std::array<vk::VertexInputAttributeDescription, 2> Attributes;
		static const vk::VertexInputBindingDescription Binding;
	};

	struct UniformBufferObject
	{
		glm::mat4 View;
		glm::mat4 Projection;

		float Radius; // world space
	};

public:
	bool VInit() override;
	void VExit() override;

	void Render() override;
	void Update(float time) override
	{
		Renderer::Update(time);
	}

private:
	bool CreateRenderPass() override;
	bool CreateFramebuffers() override;

private:
	bool CreateUniformBuffer();
	bool CreateVertexBuffer();
	bool CreatePipeline();
	bool CreateDescriptorSet();

	void UpdateUniforms();
	void UpdateDescriptorSets();
	void CollectRenderData();

	void RenderUI();

private:
	Shader VertexShader, FragmentShader;
	Pipeline Pipeline;

	vk::DescriptorSet DescriptorSet;

	UniformBufferObject Uniforms;
	Buffer UniformBuffer;

	VertexBuffer VertexBuffer;
	uint32_t NumVertices;

	bool Paused = false;
};
