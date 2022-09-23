#pragma once

#include "engine/events/Event.h"

namespace Events
{
	struct KeyDown : public Event
	{
		EVENT_TYPE(0xa416515c);

		KeyDown(int key) :
			Event(StaticType),
			Key(key)
		{
		}

		int Key;
	};

	struct KeyUp : public Event
	{
		EVENT_TYPE(0x2a8b672a);

		KeyUp(int key) :
			Event(StaticType),
			Key(key)
		{
		}

		int Key;
	};

	struct KeyCharacter : public Event
	{
		EVENT_TYPE(0xa269b5);

		KeyCharacter(unsigned int c) :
			Event(StaticType),
			Char(c)
		{
		}

		unsigned int Char;
	};
}
