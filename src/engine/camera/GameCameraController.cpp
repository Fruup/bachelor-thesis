#include "engine/hzpch.h"
#include "engine/camera/GameCameraController.h"

#include "engine/camera/Camera2D.h"
#include "engine/events/Event.h"
#include "engine/events/Mouse.h"
#include "engine/events/Keyboard.h"
#include "engine/input/Input.h"
#include "engine/utils/Utils.h"

static float RandomFloat()
{
	return float(Utils::RandomInt(0, 1000)) * .001f;
}

GameCameraController::GameCameraController(Camera2D& camera) :
	Camera(camera)
{
}

void GameCameraController::HandleEvent(Event& e)
{
	EventDispatcher d(e);

	d.Dispatch<Events::MouseDown>([this](Events::MouseDown& e)
		{
			if (e.Button == GLFW_MOUSE_BUTTON_2)
			{
				Dragging = true;
				FocusEnable = false;
			}
		});

	d.Dispatch<Events::MouseUp>([this](Events::MouseUp& e)
		{
			if (e.Button == GLFW_MOUSE_BUTTON_2)
				Dragging = false;
		});

	d.Dispatch<Events::MouseMove>([this](Events::MouseMove& e)
		{
			if (Dragging)
				m_Position += Camera.PixelToWorldDistance(e.Delta);
		});

	d.Dispatch<Events::MouseWheel>([this](Events::MouseWheel& e)
		{
			Camera.ScaleExponent += e.Wheel;
			FocusEnable = false;
		});
}

void GameCameraController::Update(float time)
{
	m_ShakeIntensity *= 1.0f - 10.0f * time;

	if (!FocusEnable)
	{
		FocusPosition = m_Position;
		FocusScaleExponent = Camera.ScaleExponent;
	}

	if (!Dragging)
	{
		float deltaScale = FocusScaleExponent - Camera.ScaleExponent;
		if (std::abs(deltaScale) < .001f) Camera.ScaleExponent = FocusScaleExponent;
		else Camera.ScaleExponent += deltaScale * time * 15.0f;

		auto deltaPosition = FocusPosition - m_Position;
		if (glm::dot(deltaPosition, deltaPosition) < .001f)
			m_Position = FocusPosition;
		else m_Position += deltaPosition * time * 15.0f;
	}

	// shake offset
	float alpha = glm::two_pi<float>() * RandomFloat();
	glm::vec2 randomVector(cosf(alpha), sinf(alpha));
	m_ShakeOffset = randomVector * m_ShakeIntensity;

	// set camera position
	Camera.Position = m_Position + m_ShakeOffset;

	Camera.ComputeMatrices();
}

void GameCameraController::FocusPoint(const glm::vec2& worldPoint)
{
	FocusEnable = true;
	FocusPosition = worldPoint;
}

void GameCameraController::FocusRect(const ViewRect& rect)
{
	FocusEnable = true;
	FocusPosition = rect.Center();

	// scale
	float q;
	if (rect.Width / rect.Height > Camera.GetAspect())
		q = Camera.GetWidth() / Camera.GetPixelsPerUnit() / rect.Width;
	else
		q = Camera.GetHeight() / Camera.GetPixelsPerUnit() / rect.Height;

	FocusScaleExponent = Camera.ComputeScaleExponentForScale(q);
}

void GameCameraController::Unfocus()
{
	FocusEnable = false;
}

void GameCameraController::Shake(float intensity)
{
	m_ShakeIntensity = intensity;
}
