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

	void ComputeMatrices();

	Camera3D& Camera;

	bool Panning = false;
	bool Dragging = false;

	glm::vec3 Position;
	glm::quat Orientation;
	float RotationX = 0.0f, RotationY = 0.0f;
	float R = 10.0f;

	glm::mat3 System;
};
