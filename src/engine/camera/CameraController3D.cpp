#include "engine/hzpch.h"

#include "engine/camera/CameraController3D.h"

#include "engine/events/EventManager.h"
#include "engine/events/Mouse.h"
#include "engine/input/Input.h"
#include "Camera3D.h"

#include <glm/gtx/quaternion.hpp>

CameraController3D::CameraController3D(Camera3D& camera) :
	Camera(camera),
	Position(glm::vec3(0, 0, -5)),
	Orientation(glm::quatLookAtLH(glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)))
{
	ComputeMatrices();
}

void CameraController3D::Update(float time)
{
}

void CameraController3D::HandleEvent(Event& e)
{
	EventDispatcher d(e);

	d.Dispatch<Events::MouseDown>([&](const Events::MouseDown& e) {
		if (e.Button == GLFW_MOUSE_BUTTON_LEFT)
			Panning = true;
		else if (e.Button == GLFW_MOUSE_BUTTON_RIGHT)
			Dragging = true;
	});

	d.Dispatch<Events::MouseUp>([&](const Events::MouseUp& e) {
		if (e.Button == GLFW_MOUSE_BUTTON_LEFT)
			Panning = false;
		else if (e.Button == GLFW_MOUSE_BUTTON_RIGHT)
			Dragging = false;
	});

	d.Dispatch<Events::MouseWheel>([&](const Events::MouseWheel& e) {
		R -= 0.2f * e.Wheel;
	});

	d.Dispatch<Events::MouseMove>([&](const Events::MouseMove& e) {
		if (Panning)
		{
			// ... wild quaternion stuff
			// pan around the point Camera.Position + <distance> * System[2];

			const float v = 0.001;
			RotationX += v * e.Delta.x;
			RotationY += v * e.Delta.y;
		}
		else if (Dragging)
		{
			// todo:
			// compute this velocity from the projection of the cursor position
			// onto the camera plane
			float v = 0.01f;

			//Position -= v * e.Delta.x * System[0] + v * e.Delta.y * System[1];
		}
	});

	ComputeMatrices();
}

void CameraController3D::ComputeMatrices()
{
	constexpr float pi = glm::pi<float>();
	RotationY = std::clamp(RotationY, -0.49f * pi, +0.49f * pi);

	Position = glm::rotate(glm::quat({ RotationY, RotationX, 0 }), { 0, 0, -R });
	Orientation = glm::quatLookAt(-glm::normalize(Position), { 0, 1, 0 });
	glm::mat4 orientation = glm::toMat4(glm::inverse(Orientation));

	Camera.View = glm::translate(orientation, -Position);
	System = glm::transpose(Camera.View);
	//System = Camera.View;

	Camera.ComputeMatrices();
}
