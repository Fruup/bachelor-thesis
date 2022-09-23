#include "engine/hzpch.h"
#include "engine/camera/Camera2D.h"

#include <glm/ext/matrix_transform.hpp>

constexpr auto Id = glm::identity<glm::mat4x4>();

Camera2D::Camera2D(float width, float height) :
	Width(width), Height(height),
	Aspect(width / height),
	ScaleBasis(1.1f),
	PixelsPerUnit(16.0f)
{
	Projection =
		glm::translate(Id, { -1.0f, -1.0f, 0.0f }) *
		glm::scale(Id, { 2.0f / Width, 2.0f / Height, 1.0f });

	ComputeMatrices();
}

glm::vec2 Camera2D::WorldToPixel(const glm::vec2& x)
{
	const auto r = View * glm::vec4{ x.x, x.y, 0.0f, 1.0f };
	return { r.x, r.y };
}

glm::vec2 Camera2D::PixelToWorld(const glm::vec2& x)
{
	const auto r = InvView * glm::vec4{ x.x, x.y, 0.0f, 1.0f };
	return { r.x, r.y };
}

glm::vec2 Camera2D::WorldToPixelDistance(const glm::vec2& x)
{
	const auto r = ViewScale * glm::vec4{ x.x, x.y, 0.0f, 1.0f };
	return { r.x, r.y };
}

float Camera2D::WorldToPixelDistance(float x)
{
	return WorldToPixelDistance(glm::vec2(x)).x;
}

glm::vec2 Camera2D::PixelToWorldDistance(const glm::vec2& x)
{
	const auto r = glm::inverse(ViewScale) * glm::vec4{ x.x, x.y, 0.0f, 1.0f };
	return { r.x, r.y };
}

float Camera2D::PixelToWorldDistance(float x)
{
	return PixelToWorldDistance(glm::vec2(x)).x;
}

void Camera2D::ComputeMatrices()
{
	float scale = powf(ScaleBasis, ScaleExponent);

	ViewScale =
		glm::scale(Id, { PixelsPerUnit, PixelsPerUnit, 1.0f }) *
		glm::scale(Id, { scale, scale, 1.0f });

	ViewTranslate =
		glm::translate(Id, { -Position.x, -Position.y, 0.0f });

	View = glm::translate(Id, { Width/2, Height/2, 0.0f }) * ViewScale * ViewTranslate;

	ProjectionView = Projection * View;
	InvView = glm::inverse(View);
}
