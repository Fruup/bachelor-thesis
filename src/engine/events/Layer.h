#pragma once

#include "engine/events/Event.h"

class Layer
{
public:
	virtual const char* GetName() = 0;
	virtual void HandleEvent(Event& e) {}
};

class LayerStack
{
public:
	Ref<Layer> AddLayer(Layer* layer);
	void PostEvent(Event& e);

	std::vector<Ref<Layer>> Layers;
};
