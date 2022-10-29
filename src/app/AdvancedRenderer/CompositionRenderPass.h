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
	void UpdateUniformsFullscreen();

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
		glm::mat4 InvProjectionView;

		glm::vec3 CameraPosition;
		float _unused1;

		glm::vec3 CameraDirection;
		float _unused2;

		glm::vec3 LightDirection;
		float _unused3;
	} Uniforms;

	Buffer UniformBuffer;

	struct
	{
		float TexelWidth;
		float TexelHeight;
	} UniformsFullscreen;

	Buffer UniformBufferFullscreen;
};
