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
	constexpr static int MaxKernelSize = 32;

	struct
	{
		int GaussN = 8;
		float _unused[3];
		float Kernel[MaxKernelSize * MaxKernelSize];
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
