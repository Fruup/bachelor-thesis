#include "engine/hzpch.h"
#include "engine/events/Layer.h"

Ref<Layer> LayerStack::AddLayer(Layer* layer)
{
	Ref<Layer> ref(layer);
	Layers.push_back(ref);
	return ref;
}

void LayerStack::PostEvent(Event& e)
{
	for (auto& layer : Layers)
	{
		if (e.Handled)
			break;

		layer->HandleEvent(e);
	}
}
