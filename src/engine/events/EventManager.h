#pragma once

#include "engine/events/Layer.h"

class EventManager
{
public:
	static void PostEvent(Event& e)
	{
		LayerStack.PostEvent(e);
	}

	static void Exit()
	{
		LayerStack.Layers.clear();
	}

	static LayerStack LayerStack;
};
