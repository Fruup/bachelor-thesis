#pragma once

#include "engine/camera/ViewRect.h"

class Camera2D;
class Event;

class EditorCameraController
{
public:
	EditorCameraController(Camera2D& camera);
	EditorCameraController(EditorCameraController&) = delete;

	void Update(float time);

	void FocusPoint(const glm::vec2& worldPoint);
	void FocusScale(float scaleExponent);
	void FocusRect(const ViewRect& rect);
	void Unfocus();

	void HandleEvent(Event& e);

	Camera2D& Camera;

	bool FocusEnable = false;
	glm::vec2 FocusPosition{};
	float FocusScaleExponent = 0.0f;
	bool Dragging = false;
};
