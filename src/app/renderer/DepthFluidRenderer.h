#pragma once

#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/Pipeline.h>
#include <engine/renderer/objects/VertexBuffer.h>
#include <engine/renderer/Renderer.h>

//#define VMA_IMPLEMENTATION
//#include <vma/vk_mem_alloc.h>

#include "FluidRenderer.h"

class DepthFluidRenderer : public Renderer, public FluidRenderer
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec2 UV;

		static const std::array<vk::VertexInputAttributeDescription, 2> Attributes;
		static const vk::VertexInputBindingDescription Binding;
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
	void UpdateDescriptorSetDepth();
	void UpdateDescriptorSetComposition();
	void CollectRenderData();

	void RenderUI();

private:
	struct
	{
		uint32_t Index;

		Shader VertexShader, FragmentShader;
		Pipeline Pipeline;

		struct
		{
			glm::mat4 View;
			glm::mat4 Projection;

			float Radius; // world space
		} Uniforms;
		Buffer UniformBuffer;

		vk::DescriptorSet DescriptorSet;

		VertexBuffer VertexBuffer;
		uint32_t NumVertices;
	} DepthPass;

	struct
	{
		uint32_t Index;

		Shader VertexShader, FragmentShader;
		Pipeline Pipeline;

		struct
		{
			glm::mat4 InvViewProjection;

			glm::vec3 CameraPosition;
			glm::vec3 CameraDirection;

			glm::vec3 LightDirection;
		} Uniforms;
		Buffer UniformBuffer;

		vk::DescriptorSet DescriptorSet;
	} CompositionPass;

	bool Paused = false;
};
