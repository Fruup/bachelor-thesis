#pragma once

#undef near
#undef far

class Camera3D
{
public:
	Camera3D() = default;
	Camera3D(const Camera3D&) = default;
	Camera3D(float fov, float aspect, float near, float far);

	void ComputeMatrices();

	const auto& GetProjection() const { return Projection; }
	const auto& GetView() const { return View; }
	const auto& GetInvView() const { return InvView; }
	const auto& GetInvProjection() const { return InvProjection; }
	const auto& GetProjectionView() const { return ProjectionView; }
	const auto& GetInvProjectionView() const { return InvProjectionView; }

	float GetNear() const { return Near; }
	float GetFar() const { return Far; }
	float GetFOV() const { return FOV; }
	float GetAspect() const { return Aspect; }

	float Near, Far;
	float FOV, Aspect;

	glm::mat4 Projection, View;
	glm::mat4 ProjectionView;
	glm::mat4 InvView, InvProjection, InvProjectionView;
};
