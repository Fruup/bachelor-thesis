#include <engine/hzpch.h>

#include "Utils.h"

#include <random>
#include <engine/renderer/Renderer.h>

void TransitionImageLayout(vk::Image image,
						   vk::ImageLayout oldLayout,
						   vk::ImageLayout newLayout,
						   vk::AccessFlags srcAccessMask,
						   vk::AccessFlags dstAccessMask,
						   vk::PipelineStageFlags srcStageMask,
						   vk::PipelineStageFlags dstStageMask)
{
	vk::ImageMemoryBarrier barrier;
	barrier
		.setOldLayout(oldLayout)
		.setNewLayout(newLayout)
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,
													   0, /* baseMipLevel */
													   1, /* levelCount */
													   0, /* baseArrayLayer */
													   1  /* layerCount */ ));

	Vulkan.CommandBuffer.pipelineBarrier(srcStageMask,
										 dstStageMask,
										 {},
										 {},
										 {},
										 barrier);
}

float RandomFloat(float min, float max)
{
	static std::random_device rd;
	static std::mt19937 engine(rd());
	
	std::uniform_real_distribution<float> distribution(min, max);
	return distribution(engine);
}
