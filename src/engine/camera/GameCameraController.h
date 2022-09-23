#pragma once

#include "engine/camera/ViewRect.h"

class Camera2D;
class Event;

class GameCameraController
{
public:
	GameCameraController(Camera2D& camera);
	GameCameraController(GameCameraController&) = delete;

	void Update(float time);

	void FocusPoint(const glm::vec2& worldPoint);
	void FocusRect(const ViewRect& rect);
	void Unfocus();

	void Shake(float intensity);

	void HandleEvent(Event& e);

	Camera2D& Camera;

	glm::vec2 m_Position;

	bool FocusEnable = false;
	glm::vec2 FocusPosition{};
	float FocusScaleExponent = 0.0f;
	bool Dragging = false;

	float m_ShakeIntensity = 0.0f;
	glm::vec2 m_ShakeOffset;
};
