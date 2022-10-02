#pragma once

#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/Buffer.h>
#include <engine/renderer/objects/VertexBuffer.h>

class AdvancedRenderer;

class CoordinateSystemRenderPass
{
public:
	CoordinateSystemRenderPass(AdvancedRenderer& renderer);

	void Init();
	void Exit();

	void Begin();
	void End();

private:
	void CreateShaders();

	void CreateVertexBuffer();

	void CreatePipelineLayout();
	void CreatePipeline();

	void CreateDescriptorSetLayout();
	void CreateDescriptorSet();

	void CreateUniformBuffer();

	void UpdateUniforms();

	void UpdateDescriptorSets();

public:
	vk::Pipeline Pipeline;
	vk::PipelineLayout PipelineLayout;

	vk::DescriptorSetLayout DescriptorSetLayout;
	vk::DescriptorSet DescriptorSet;

	Shader VertexShader, FragmentShader;

	VertexBuffer VertexBuffer;

	AdvancedRenderer& Renderer;

	struct
	{
		glm::mat4 ProjectionView;
		float Aspect;
	} Uniforms;

	Buffer UniformBuffer;
};
