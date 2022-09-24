#pragma once

#include "engine/camera/ViewRect.h"

#undef near
#undef far

class Event;

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

	float GetNear() const { return Near; }
	float GetFar() const { return Far; }
	float GetFOV() const { return FOV; }
	float GetAspect() const { return Aspect; }

	float Near, Far;
	float FOV, Aspect;

	glm::mat4 Projection, View, InvView, InvProjection, ProjectionView;
};

class EditorCameraController3D
{
public:
	EditorCameraController3D(Camera3D& camera);
	EditorCameraController3D(EditorCameraController3D&) = delete;

	void Update(float time);
	void HandleEvent(Event& e);

	void SetPosition(const glm::vec3& position);
	void SetOrientation(const glm::quat& orientation);

	void ComputeMatrices();

	Camera3D& Camera;

	bool Panning = false;
	bool Dragging = false;

	glm::vec3 Position;
	glm::quat Orientation;

	glm::mat3 System;
};
