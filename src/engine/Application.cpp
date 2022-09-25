#include "engine/hzpch.h"
#include "engine/Application.h"

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/renderer/Renderer.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/events/EventManager.h"
#include "engine/utils/Utils.h"
#include "engine/utils/Statistics.h"
#include "engine/utils/PerformanceTimer.h"

void Application::Init()
{
	// init asset manager
	AssetManager::Init();

	// load configuration
	YAML::Node config;

	try
	{
		config = YAML::LoadFile(AssetManager::GetAssetPath("config.yml"));
		const auto gameConfig = config["game"];
		const auto displayConfig = config["display"];

		Data.Config.Title = displayConfig["title"].as<std::string>("Application");
		Data.Config.Width = displayConfig["width"].as<int>(500);
		Data.Config.Height = displayConfig["height"].as<int>(500);
		Data.Config.PixelSize = displayConfig["pixelSize"].as<int>(1);
		Data.Config.ShowDemoWindow = displayConfig["showDemoWindow"].as<bool>(true);
	}
	catch (std::exception e)
	{
	}

	// init renderer
	if (!Renderer::GetInstance().Init(
		Data.Config.Width,
		Data.Config.Height,
		Data.Config.PixelSize,
		Data.Config.Title.c_str()))
	{
		SPDLOG_ERROR("Failed to init renderer!");
		return;
	}

	AssetManager::LoadStartupAssets();
	Input::Init();

	// add layers
	EventManager::LayerStack.AddLayer(new ImGuiLayer);

	// user init
	if (!OnInit(config))
	{
		SPDLOG_ERROR("Failed to initialize user side of application!");
		return;
	}

	// done
	Data.Running = true;
}

void Application::Exit()
{
	OnExit();
	AssetManager::Exit();
	Renderer::GetInstance().Exit();
	EventManager::Exit();
}

void Application::RenderImGui()
{
	static bool s_MenuOpened = false;
	bool showMenu = s_MenuOpened || ImGui::GetMousePos().y < 30.0f;

	ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

	ImGui::Begin("Dockspace", nullptr, windowFlags);

	ImGui::PopStyleVar(3);

	// main menu bar
	/*s_MenuOpened = false;
	if (showMenu)
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				s_MenuOpened = true;

				ImGui::MenuItem("test");
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}*/

	ImGui::DockSpace(ImGui::GetID("MyDockSpace"), {}, dockspaceFlags);

	if (Data.Config.ShowDemoWindow)
		ImGui::ShowDemoWindow(&Data.Config.ShowDemoWindow);

	ImGui::End();
}

void Application::Run()
{
	float delta = 1.0f / 60.0f;

	while (Data.Running)
	{
		GlobalStatistics.Begin();

		// update
		glfwPollEvents();
		Input::Update(delta);
		OnUpdate(delta);

		Renderer::GetInstance().Update(delta);

		Renderer::GetInstance().Begin();

		Renderer::GetInstance().Render();

		RenderImGui();
		Renderer::GetInstance().RenderUI();
		GlobalStatistics.ImGuiRender();

		OnRender(delta);

		Renderer::GetInstance().End();

		// close window requested?
		if (glfwWindowShouldClose(Renderer::GetInstance().GetWindow()))
			Data.Running = false;
	}
}
