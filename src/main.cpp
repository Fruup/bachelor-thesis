#include "engine/hzpch.h"

#include <engine/Application.h>
#include <engine/events/EventManager.h>
#include <engine/renderer/Renderer.h>

#include <engine/utils/PerformanceTimer.h>
#include <engine/utils/Statistics.h>
#include <engine/input/Input.h>

#include "app/Dataset.h"
#include "app/AdvancedRenderer/AdvancedRenderer.h"
#include "app/DiskRenderer/DiskRenderer.h"

//auto g_Renderer = std::make_unique<DiskRenderer>();
auto g_Renderer = std::make_unique<AdvancedRenderer>();

Renderer& Renderer::GetInstance()
{
	static Renderer _r;
	return _r;
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
		//Dataset* dataset = new Dataset(datasetPrefix);
		Dataset* dataset = new Dataset(1.0f, 50000);
		if (!dataset->Loaded)
			return false;

		// init renderer
		g_Renderer->Init(dataset);

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
			g_Renderer->HandleEvent(e);
		}
	};
};

void ApplicationImGui()
{
	ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	// dockspace
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

	ImGui::Begin("Dockspace", nullptr, windowFlags);

	ImGui::PopStyleVar(1);

	ImGui::DockSpace(ImGui::GetID("MyDockSpace"), {}, dockspaceFlags);
	ImGui::End();
}

int main()
{
	App app;
	app.Init();
	//app.Run();

	float delta = 1.0f / 60.0f;

	while (app.Data.Running)
	{
		PROFILE_SCOPE("Frame");

		GlobalStatistics.Begin();

		// input
		glfwPollEvents();
		Input::Update(delta);

		// update
		g_Renderer->Update(delta);

		// render
		Vulkan.Begin();

		ApplicationImGui();
		g_Renderer->RenderUI();
		GlobalStatistics.ImGuiRender();

		g_Renderer->Render();

		Vulkan.End();

		// close window requested?
		if (glfwWindowShouldClose(Vulkan.Window))
			app.Data.Running = false;
	}

	g_Renderer->Exit();
	g_Renderer.reset();

	app.Exit();
}
