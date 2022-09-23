#pragma once

struct ViewRect
{
	ViewRect() = default;
	ViewRect(const glm::vec2& offset, const glm::vec2& extent) :
		Offset(offset), Extent(extent)
	{
	}

	union
	{
		glm::vec2 Offset;
		struct { float x, y; };
	};

	union
	{
		glm::vec2 Extent;
		struct { float Width, Height; };
	};

	glm::vec2 Center() const { return { x + .5f * Width, y + .5f * Height }; }
};
