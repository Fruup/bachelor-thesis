#include <engine/hzpch.h>

#include "Renderer.h"
#include "ImGuiRenderPass.h"

// ----------------------------------------------------------------------

//auto& Vulkan = Renderer::GetInstance();

// ----------------------------------------------------------------------

void ImGuiRenderPass::Init()
{
	CreateRenderPass();
	CreateFramebuffers();
}

void ImGuiRenderPass::Exit()
{
	Vulkan.Device.destroyRenderPass(RenderPass);

	for (int i = 0; i < Framebuffers.size(); ++i)
		Vulkan.Device.destroyFramebuffer(Framebuffers[i]);
}

void ImGuiRenderPass::Begin()
{
	const std::array<float, 4> ClearColor = {
		1.0f, 0.0f, 1.0f, 1.0f,
	};

	std::array<vk::ClearValue, 1> ClearValue = {
		vk::ClearColorValue(ClearColor),
	};

	vk::RenderPassBeginInfo info;
	info.setFramebuffer(Framebuffers[Vulkan.CurrentImageIndex])
		.setRenderArea(vk::Rect2D({ 0, 0 }, Vulkan.SwapchainExtent))
		.setRenderPass(RenderPass)
		.setClearValues(ClearValue);

	Vulkan.CommandBuffer.beginRenderPass(info, vk::SubpassContents::eInline);
}

void ImGuiRenderPass::End()
{
	Vulkan.CommandBuffer.endRenderPass();
}

// ----------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void ImGuiRenderPass::CreateRenderPass()
{
	// attachment
	vk::AttachmentDescription colorAttachment;
	colorAttachment
		.setLoadOp(vk::AttachmentLoadOp::eLoad)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setFormat(Vulkan.SwapchainFormat)
		.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

	vk::AttachmentReference colorAttachmentRef(
		0,
		vk::ImageLayout::eColorAttachmentOptimal
	);

	// subpass
	vk::SubpassDescription subpass;
	subpass
		.setColorAttachments(colorAttachmentRef)
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

	// dependency
	vk::SubpassDependency dependency1;
	dependency1
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setDstSubpass(0)
		.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

	vk::SubpassDependency dependency2;
	dependency2
		.setSrcSubpass(0)
		.setDstSubpass(VK_SUBPASS_EXTERNAL)
		.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

	std::array<vk::SubpassDependency, 2> dependencies = {
		dependency1,
		dependency2,
	};

	// render pass
	vk::RenderPassCreateInfo renderPassCreateInfo;
	renderPassCreateInfo
		.setAttachments(colorAttachment)
		.setDependencies(dependencies)
		.setSubpasses(subpass);

	RenderPass = Vulkan.Device.createRenderPass(renderPassCreateInfo);
}

void ImGuiRenderPass::CreateFramebuffers()
{
	Framebuffers.resize(Vulkan.SwapchainImages.size());

	for (size_t i = 0; i < Framebuffers.size(); i++)
	{
		vk::FramebufferCreateInfo info;
		info.setAttachments(Vulkan.SwapchainImageViews[i])
			.setRenderPass(RenderPass)
			.setWidth(Vulkan.SwapchainExtent.width)
			.setHeight(Vulkan.SwapchainExtent.height)
			.setLayers(1);

		Framebuffers[i] = Vulkan.Device.createFramebuffer(info);
		HZ_ASSERT(Framebuffers[i], "Swapchain framebuffer creation failed!");
	}
}

// ----------------------------------------------------------------------
