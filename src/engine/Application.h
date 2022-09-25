#pragma once

#include <yaml-cpp/yaml.h>

struct ApplicationData
{
	bool Running = false;

	struct
	{
		std::string Title;
		int Width, Height;
		int PixelSize;
		bool ShowDemoWindow;
	} Config;
};

class Application
{
public:
	void Init();
	void Exit();
	void Run();

	virtual bool OnInit(YAML::Node& config) = 0;
	virtual void OnExit() = 0;

	virtual void OnUpdate(float time) = 0;
	virtual void OnRender(float time) = 0;

	ApplicationData Data;

private:
	void RenderImGui();
};
