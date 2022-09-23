#pragma once

class Camera2D
{
public:
	Camera2D() = default;
	Camera2D(const Camera2D&) = default;
	Camera2D(float width, float height);

	glm::vec2 WorldToPixel(const glm::vec2& x);
	glm::vec2 PixelToWorld(const glm::vec2& x);
	glm::vec2 WorldToPixelDistance(const glm::vec2& x);
	float WorldToPixelDistance(float x);
	glm::vec2 PixelToWorldDistance(const glm::vec2& x);
	float PixelToWorldDistance(float x);

	void ComputeMatrices();
	float ComputeScaleExponentForScale(float desiredScale) { return logf(desiredScale) / logf(ScaleBasis); }

	glm::vec2 Position{};
	float ScaleExponent = 0.0f;
	float ScaleBasis = 1.1f;

	auto GetPixelsPerUnit() const { return PixelsPerUnit; }
	auto GetWidth() const { return Width; }
	auto GetHeight() const { return Height; }
	auto GetAspect() const { return Aspect; }

	auto GetProjection() const { return Projection; }
	auto GetView() const { return View; }
	auto GetInvView() const { return InvView; }
	auto GetProjectionView() const { return ProjectionView; }

	void SetPixelsPerUnit(float f) { PixelsPerUnit = f; }

private:
	float PixelsPerUnit;
	float Width, Height;
	float Aspect;

	glm::mat4 Projection, View, ViewScale, ViewTranslate, InvView, ProjectionView;
};
