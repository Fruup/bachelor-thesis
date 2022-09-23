#include "engine/hzpch.h"

#include <engine/Application.h>
#include <engine/events/EventManager.h>

#include "app/Dataset.h"

#include "app/renderer/AdvancedFluidRenderer.h"
#include "app/renderer/PointFluidRenderer.h"

std::unique_ptr<FluidRenderer> g_FluidRenderer(new PointFluidRenderer);

class App : public Application
{
public:
	bool OnInit(YAML::Node& config)
	{
		std::string datasetPrefix;

		auto datasetConfig = config["datasetPrefix"];

		if (datasetConfig)
		{
			datasetPrefix = datasetConfig.as<std::string>();
		}
		else
		{
			datasetConfig = config["dataset"];
			datasetPrefix = "datasets/" + datasetConfig.as<std::string>() + "/ParticleData_Fluid_";
		}

		// load dataset
		Dataset dataset;
		if (!dataset.Init(datasetPrefix))
			return false;

		// init fluid renderer
		if (!g_FluidRenderer->Init(dataset))
			return false;

		// add application layer
		EventManager::LayerStack.AddLayer(new AppLayer);

		return true;
	}

	void OnExit()
	{
		g_FluidRenderer->Exit();

		g_FluidRenderer.reset();
	}

	void OnUpdate(float time)
	{
		g_FluidRenderer->Update(time);
	}

	void OnRender(float time)
	{
		g_FluidRenderer->RenderFrame();
	}

	class AppLayer : public Layer
	{
	public:
		const char* GetName() { return "App"; }

		void HandleEvent(Event& e)
		{
			g_FluidRenderer->HandleEvent(e);
		}
	};
};

int main()
{
	App app;
	app.Init();
	app.Run();
	app.Exit();
}
