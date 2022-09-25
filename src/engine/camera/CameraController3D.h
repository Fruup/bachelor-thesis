#pragma once

class Camera3D;
class Event;

class CameraController3D
{
public:
	CameraController3D(Camera3D& camera);
	CameraController3D(CameraController3D&) = delete;

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
