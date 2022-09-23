#include "engine/hzpch.h"
#include "engine/input/Input.h"

#include "engine/renderer/Renderer.h"

#include "engine/events/EventManager.h"
#include "engine/events/Mouse.h"
#include "engine/events/Keyboard.h"

InputData Input::Data;

void PostEvent(Event& e)
{
	EventManager::PostEvent(e);
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		Events::MouseDown e{ button, Input::GetCursorPosition() };
		PostEvent(e);

		// click candidate
		if (button == GLFW_MOUSE_BUTTON_1)
		{
			Input::Data.LeftClick = true;
			Input::Data.LeftClickPosition = Input::GetCursorPosition();
		}
		else if (button == GLFW_MOUSE_BUTTON_2)
		{
			Input::Data.RightClick = true;
			Input::Data.RightClickPosition = Input::GetCursorPosition();
		}
	}
	else if (action == GLFW_RELEASE)
	{
		// first click
		if (Input::Data.LeftClick)
		{
			Events::MouseClick ce{ button, Input::GetCursorPosition() };
			PostEvent(ce);

			Input::Data.LeftClick = false;
		}

		if (Input::Data.RightClick)
		{
			Events::MouseClick ce{ button, Input::GetCursorPosition() };
			PostEvent(ce);

			Input::Data.RightClick = false;
		}

		// ...then up
		Events::MouseUp e{ button, Input::GetCursorPosition() };
		PostEvent(e);
	}
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		Events::KeyDown e{ key };
		PostEvent(e);
	}
	else if (action == GLFW_RELEASE)
	{
		Events::KeyUp e{ key };
		PostEvent(e);
	}
}

void CharCallback(GLFWwindow* window, unsigned int c)
{
	Events::KeyCharacter e{ c };
	PostEvent(e);
}

void MouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	Events::MouseWheel e{ float(yoffset), Input::GetCursorPosition() };
	PostEvent(e);
}

void MouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto prev = Input::Data.CursorPosition;
	Input::Data.CursorPosition = { xpos, ypos };

	Events::MouseMove e{ Input::GetCursorPosition(), prev };
	PostEvent(e);

	constexpr double DISTANCE_THRESHOLD = 3.0;
	if (Input::Data.LeftClick &&
		Input::Data.CursorPosition.Distance(Input::Data.LeftClickPosition) > DISTANCE_THRESHOLD)
		Input::Data.LeftClick = false;
	if (Input::Data.RightClick &&
		Input::Data.CursorPosition.Distance(Input::Data.RightClickPosition) > DISTANCE_THRESHOLD)
		Input::Data.RightClick = false;
}

void Input::Init()
{
	auto window = Renderer::GetInstance().GetWindow();

	// get initial cursor position
	glfwGetCursorPos(Renderer::GetInstance().GetWindow(), &Data.CursorPosition.x, &Data.CursorPosition.y);

	// set glfw callbacks
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);

	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, MouseWheelCallback);
	glfwSetCursorPosCallback(window, MouseMoveCallback);
	glfwSetCharCallback(window, CharCallback);
}

void Input::Update(float time)
{
}
