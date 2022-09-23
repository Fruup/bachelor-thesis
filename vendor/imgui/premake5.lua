workspace "ImGui"
	configurations { "Debug", "Release", "Production" }
	platforms { "Win64" }
	architecture "x64"

project "ImGui"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"

	targetdir "bin/%{cfg.buildcfg}"
	objdir "bin-int/%{cfg.buildcfg}"

	files { "*.h", "*.cpp" }
	
	filter "system:windows"
		systemversion "latest"
		staticruntime "On"

	filter "system:linux"
		pic "On"
		systemversion "latest"
		staticruntime "On"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Production"
		runtime "Release"
		optimize "on"
