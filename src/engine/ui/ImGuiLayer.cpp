#include "engine/hzpch.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/events/Keyboard.h"
#include "engine/events/Mouse.h"

void UpdateKeyModifiers(ImGuiIO& io)
{
	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
#ifdef _WIN32
	io.KeySuper = false;
#else
	io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
#endif
}

void ImGuiLayer::HandleEvent(Event& e)
{
	ImGuiIO& io = ImGui::GetIO();
	EventDispatcher d(e);

	if (io.WantCaptureKeyboard)
	{
		d.Dispatch<Events::KeyDown>(
			[&](Events::KeyDown& e)
			{
				if (e.Key >= 0 && e.Key < IM_ARRAYSIZE(io.KeysDown))
					io.KeysDown[e.Key] = true;

				UpdateKeyModifiers(io);

				e.Handled = true;
			}
		);

		d.Dispatch<Events::KeyUp>(
			[&](Events::KeyUp& e)
			{
				if (e.Key >= 0 && e.Key < IM_ARRAYSIZE(io.KeysDown))
					io.KeysDown[e.Key] = false;

				UpdateKeyModifiers(io);

				e.Handled = true;
			}
		);

		d.Dispatch<Events::KeyCharacter>(
			[&](Events::KeyCharacter& e)
			{
				io.AddInputCharacter(e.Char);

				e.Handled = true;
			}
		);
	}

	if (io.WantCaptureMouse)
	{
		d.Dispatch<Events::MouseClick>(
			[&](Events::MouseClick& e)
			{
				e.Handled = true;
			}
		);

		d.Dispatch<Events::MouseDown>(
			[&](Events::MouseDown& e)
			{
				e.Handled = true;
			}
		);

		d.Dispatch<Events::MouseUp>(
			[&](Events::MouseUp& e)
			{
				e.Handled = true;
			}
		);

		d.Dispatch<Events::MouseMove>(
			[&](Events::MouseMove& e)
			{
				e.Handled = true;
			}
		);

		d.Dispatch<Events::MouseWheel>(
			[&](Events::MouseWheel& e)
			{
				io.MouseWheel += e.Wheel;

				e.Handled = true;
			}
		);
	}
}
