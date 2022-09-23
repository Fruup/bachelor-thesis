#pragma once

#include "engine/events/Layer.h"

class ImGuiLayer : public Layer
{
public:
	ImGuiLayer() = default;

	const char* GetName() override { return "ImGuiLayer"; }
	void HandleEvent(Event& e) override;
};
