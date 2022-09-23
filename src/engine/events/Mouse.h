#pragma once

#include "engine/events/Event.h"
#include "engine/utils/CursorPosition.h"

namespace Events
{
	struct MouseUp : public Event
	{
		EVENT_TYPE(0x1b2ed7b7);

		MouseUp(int32_t button, const CursorPosition& cursorPosition) :
			Event(StaticType),
			Button(button),
			CursorPosition(cursorPosition)
		{
		}

		int32_t Button;
		CursorPosition CursorPosition;
	};

	struct MouseDown : public Event
	{
		EVENT_TYPE(0xba97c512);

		MouseDown(int32_t button, const CursorPosition& cursorPosition) :
			Event(StaticType),
			Button(button),
			CursorPosition(cursorPosition)
		{
		}

		int32_t Button;
		CursorPosition CursorPosition;
	};

	struct MouseMove : public Event
	{
		EVENT_TYPE(0x92a63a48);

		MouseMove(const CursorPosition& cursorPosition, const CursorPosition& prevCursorPosition) :
			Event(StaticType),
			Delta(prevCursorPosition - cursorPosition),
			CursorPosition(cursorPosition),
			PrevCursorPosition(prevCursorPosition)
		{
		}

		::CursorPosition CursorPosition;
		::CursorPosition PrevCursorPosition;
		glm::vec2 Delta;
	};

	struct MouseWheel : public Event
	{
		EVENT_TYPE(0xaf1cb0b1);

		MouseWheel(float wheel, const CursorPosition& cursorPosition) :
			Event(StaticType),
			Wheel(wheel),
			CursorPosition(cursorPosition)
		{
		}

		float Wheel;
		CursorPosition CursorPosition;
	};

	struct MouseClick : public Event
	{
		EVENT_TYPE(0x5efbca40);

		MouseClick(int32_t button, const CursorPosition& cursorPosition) :
			Event(StaticType),
			Button(button),
			CursorPosition(cursorPosition)
		{
		}

		int32_t Button;
		CursorPosition CursorPosition;
	};
}
