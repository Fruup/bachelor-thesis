#include "engine/hzpch.h"

#include "engine/events/EventManager.h"
#include "engine/events/Mouse.h"
#include "engine/input/Input.h"
#include "engine/camera/EditorCameraController3D.h"

#include <glm/gtx/quaternion.hpp>

Camera3D::Camera3D(float fov, float aspect, float near, float far) :
	FOV(fov),
	Near(near),
	Far(far),
	Aspect(aspect)
{
	Projection = glm::perspectiveLH(-fov, aspect, near, far);
}

void Camera3D::ComputeMatrices()
{
	InvView = glm::inverse(View);
	ProjectionView = Projection * View;
}

EditorCameraController3D::EditorCameraController3D(Camera3D& camera) :
	Camera(camera),
	Position(glm::vec3()),
	Orientation(glm::quatLookAt(glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)))
{
	ComputeMatrices();
}

void EditorCameraController3D::Update(float time)
{
}

void EditorCameraController3D::HandleEvent(Event& e)
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
		Position -= 0.25f * e.Wheel * System[2];
	});

	d.Dispatch<Events::MouseMove>([&](const Events::MouseMove& e) {
		if (Panning)
		{
			// ... wild quaternion stuff
			// pan around the point Camera.Position + <distance> * System[2];
		}
		else if (Dragging)
		{
			// todo:
			// compute this velocity from the projection of the cursor position
			// onto the camera plane
			float v = 0.01f;

			Position += v * e.Delta.x * System[0] + v * e.Delta.y * System[1];
		}
	});

	ComputeMatrices();
}

void EditorCameraController3D::SetPosition(const glm::vec3& position)
{
	Position = position;

	ComputeMatrices();
}

void EditorCameraController3D::SetOrientation(const glm::quat& orientation)
{
	Orientation = orientation;

	ComputeMatrices();
}

void EditorCameraController3D::ComputeMatrices()
{
	auto orientation = glm::toMat4(Orientation);

	System = orientation;
	Camera.View = glm::translate(orientation, Position);

	Camera.ComputeMatrices();
}
