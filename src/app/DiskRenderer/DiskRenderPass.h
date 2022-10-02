#pragma once

#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/Buffer.h>

class DiskRenderer;

class DiskRenderPass
{
public:
	DiskRenderPass(DiskRenderer& renderer);

	void Init();
	void Exit();

	void Begin();
	void End();

	void Render();

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

	DiskRenderer& Renderer;

	struct
	{
		glm::mat4 View;
		glm::mat4 Projection;

		float Radius; // world space
	} Uniforms;

	Buffer UniformBuffer;
};
