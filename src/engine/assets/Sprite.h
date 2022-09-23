#pragma once

#include <engine/renderer/objects/Image.h>

namespace Assets
{
	struct Sprite
	{
		~Sprite()
		{
			if (Image) Image.Destroy();
			if (NormalMap) NormalMap.Destroy();
			Frames.clear();
		}

		struct Frame
		{
			glm::vec2 Position;
			glm::vec2 Extent;

			glm::vec2 UV;
			glm::vec2 UVExtent;

			float Duration;
		};

		Image Image, NormalMap;
		std::vector<Frame> Frames;

		void AddFrame(float x, float y, float w, float h, float duration)
		{
			glm::vec2 invImgExtent = 1.0f / glm::vec2(Image.Dimensions.width, Image.Dimensions.height);

			Frames.push_back({
				glm::vec2(x, y),
				glm::vec2(w, h),
				glm::vec2(x, y) * invImgExtent,
				glm::vec2(w, h) * invImgExtent,
				duration,
				});
		}

		glm::vec2 GetDimensions() const
		{
			return glm::vec2(Image.Dimensions.width, Image.Dimensions.height);
		}
	};
}
