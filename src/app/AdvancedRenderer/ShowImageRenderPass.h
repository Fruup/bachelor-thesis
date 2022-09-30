#pragma once

#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/Buffer.h>

class AdvancedRenderer;

class ShowImageRenderPass
{
public:
	ShowImageRenderPass(AdvancedRenderer& renderer);

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
		float TexelWidth;
		float TexelHeight;
	} UniformsFullscreen;

	Buffer UniformBufferFullscreen;

	vk::ImageView ImageView;
};
