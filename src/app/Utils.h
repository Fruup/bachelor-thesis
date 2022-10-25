#pragma once

void TransitionImageLayout(vk::CommandBuffer& cmd,
						   vk::Image image,
						   vk::ImageLayout oldLayout,
						   vk::ImageLayout newLayout,
						   vk::AccessFlags srcAccessMask,
						   vk::AccessFlags dstAccessMask,
						   vk::PipelineStageFlags srcStageMask,
						   vk::PipelineStageFlags dstStageMask);

float RandomFloat(float min = 0.0f, float max = 1.0f);
