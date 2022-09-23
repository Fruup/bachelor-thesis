#include "engine/hzpch.h"
#include "engine/camera/EditorCameraController.h"

#include "engine/camera/Camera2D.h"
#include "engine/events/Event.h"
#include "engine/events/Mouse.h"
#include "engine/events/Keyboard.h"
#include "engine/input/Input.h"

EditorCameraController::EditorCameraController(Camera2D& camera) :
	Camera(camera)
{
}

void EditorCameraController::HandleEvent(Event& e)
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
				Camera.Position += Camera.PixelToWorldDistance(e.Delta);
		});

	d.Dispatch<Events::MouseWheel>([this](Events::MouseWheel& e)
		{
			Camera.ScaleExponent += e.Wheel;
			FocusEnable = false;
		});
}

void EditorCameraController::Update(float time)
{
	if (!FocusEnable)
	{
		FocusPosition = Camera.Position;
		FocusScaleExponent = Camera.ScaleExponent;
	}

	if (!Dragging)
	{
		float deltaScale = FocusScaleExponent - Camera.ScaleExponent;
		if (std::abs(deltaScale) < .001f) Camera.ScaleExponent = FocusScaleExponent;
		else Camera.ScaleExponent += deltaScale * time * 15.0f;

		auto deltaPosition = FocusPosition - Camera.Position;
		if (glm::dot(deltaPosition, deltaPosition) < .001f)
			Camera.Position = FocusPosition;
		else Camera.Position += deltaPosition * time * 15.0f;
	}

	Camera.ComputeMatrices();
}

void EditorCameraController::FocusPoint(const glm::vec2& worldPoint)
{
	FocusEnable = true;
	FocusPosition = worldPoint;
}

void EditorCameraController::FocusScale(float scaleExponent)
{
	FocusEnable = true;
	FocusScaleExponent = scaleExponent;
}

void EditorCameraController::FocusRect(const ViewRect& rect)
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

void EditorCameraController::Unfocus()
{
	FocusEnable = false;
}
