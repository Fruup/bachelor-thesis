#include "engine/hzpch.h"

#include <engine/Application.h>
#include <engine/events/EventManager.h>
#include <engine/renderer/Renderer.h>

#include "app/Dataset.h"

#include "app/renderer/AdvancedFluidRenderer.h"
#include "app/renderer/PointFluidRenderer.h"

static std::unique_ptr<PointFluidRenderer> g_FluidRenderer(new PointFluidRenderer);

Renderer& Renderer::GetInstance()
{
	return *g_FluidRenderer;
}

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
		g_FluidRenderer->SetDataset(dataset);

		// add application layer
		EventManager::LayerStack.AddLayer(new AppLayer);

		return true;
	}

	void OnExit()
	{
	}

	void OnUpdate(float time)
	{
	}

	void OnRender(float time)
	{
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

	g_FluidRenderer.reset();
}
