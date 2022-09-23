#pragma once

#include "FluidRenderer.h"

#include <engine/renderer/objects/Pipeline.h>
#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/VertexBuffer.h>
#include <engine/renderer/objects/Buffer.h>

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

class PointFluidRenderer : public FluidRenderer
{
public:
	bool VInit() override;
	void VExit() override;

	void RenderFrame() override;

private:
	void CollectRenderData();

	void UpdateParticleBuffer();
	void UpdateUniforms();
	void UpdateDescriptorSet();

	void UpdateDescriptorSetDepth();
	void UpdateDescriptorSetRayMarch();
	void UpdateDescriptorSetComposition();

	void InitDepthPass();
	void InitRayMarchPass();
	void InitCompositionPass();

	void RenderUI();

	struct
	{
		struct
		{
			glm::mat4 View;
			glm::mat4 Projection;

			float Radius; // world space
		} Uniforms;

		Buffer UniformBuffer;

		vk::DescriptorSet DescriptorSet;
		Pipeline Pipeline;
		Shader FragmentShader, VertexShader;
	} DepthPass;

	struct
	{
		struct
		{
			glm::mat4 ViewProjectionInv;
			glm::vec2 Resolution;
		} Uniforms;

		Buffer UniformBuffer;

		vk::DescriptorSet DescriptorSet;
		Pipeline Pipeline;
		Shader FragmentShader, VertexShader;
	} RayMarchPass;

	struct
	{
		struct
		{
			glm::vec3 CameraPosition;
			glm::vec3 CameraDirection;

			glm::vec3 LightDirection;
		} Uniforms;

		Buffer UniformBuffer;

		vk::DescriptorSet DescriptorSet;
		Pipeline Pipeline;
		Shader FragmentShader, VertexShader;
	} CompositionPass;

	std::vector<Vertex> Vertices;
	uint32_t CurrentVertex = 0;

	VertexBuffer VertexBuffer;

	vk::CommandBuffer CommandBuffer;

	struct
	{
		Buffer CPU;
		Buffer GPU;
		size_t Size;
	} ParticleBuffer;

	bool Paused = false;
};
