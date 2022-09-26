#pragma once

#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/Buffer.h>

class AdvancedRenderer;

class CompositionRenderPass
{
public:
	CompositionRenderPass(AdvancedRenderer& renderer);

	void Init();
	void Exit();

	void Begin();
	void End();

private:
	void CreateShaders();

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

	AdvancedRenderer& Renderer;

	struct
	{
		glm::mat4 InvProjection;

		glm::vec3 CameraPosition;
		glm::vec3 CameraDirection;

		glm::vec3 LightDirection;
	} Uniforms;

	Buffer UniformBuffer;
};
