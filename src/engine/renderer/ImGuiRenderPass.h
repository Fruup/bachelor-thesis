#pragma once

class ImGuiRenderPass
{
public:
	void Init();
	void Exit();

	void Begin();
	void End();

private:
	void CreateRenderPass();
	void CreateFramebuffers();

public:
	vk::RenderPass RenderPass;
	std::vector<vk::Framebuffer> Framebuffers;
};
