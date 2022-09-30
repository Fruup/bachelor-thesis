#pragma once

#include <engine/renderer/objects/Shader.h>
#include <engine/renderer/objects/Buffer.h>

class AdvancedRenderer;

class GaussRenderPass
{
public:
	GaussRenderPass(AdvancedRenderer& renderer);

	void Init();
	void Exit();

	void Begin();
	void End();

	void RenderUI();

private:
	void CreateUniformBuffer();
	void CreateShaders();

	void CreatePipelineLayout();
	void CreatePipeline();

	void CreateDescriptorSetLayout();
	void CreateDescriptorSet();

	void UpdateDescriptorSet();
	void UpdateUniforms();

private:
	struct
	{
		int GaussN = 1;
		float Kernel[64 * 64];
	} Uniforms;

	Buffer UniformBuffer;

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
};
