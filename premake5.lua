-- globals
VULKAN_SDK = os.getenv('VULKAN_SDK')

VULKAN_LIBS = {
	"glslang",
	"shaderc",
	"shaderc_util",
	"shaderc_combined",
}

VULKAN_LIBS_RELEASE = {}
VULKAN_LIBS_DEBUG = {}

for k, v in pairs(VULKAN_LIBS) do
	VULKAN_LIBS_RELEASE[k] = "%{VULKAN_SDK}/Lib/" .. v
	VULKAN_LIBS_DEBUG[k] = "%{VULKAN_SDK}/Lib/" .. v .. "d"
end

-- workspace
workspace "fluids"
	configurations { "Debug", "Release", "Production" }
	platforms { "Win64" }
	architecture "x64"

	targetdir "build/%{cfg.buildcfg}/%{prj.name}"
	objdir "build/obj/%{prj.name}"

	filter "configurations:Debug"
		defines {
			"DEBUG",
		}

		symbols "On"

	filter "configurations:Release"
		defines {
			"NDEBUG",
			"RELEASE",
		}

		optimize "On"
		symbols "On"

	filter "configurations:Production"
		defines {
			"NDEBUG",
			"PRODUCTION",
		}

		optimize "On"

	filter "platforms:Win64"
		system "Windows"
		architecture "x64"

	defines {
		"SOLUTION_DIRECTORY=\"%{wks.location}\""
	}

-- projects

include "vendor/zlib"
include "vendor/partio"

project "fluids"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	openmp "On"

	includedirs {
		"src",
		"vendor",
		"vendor/glm",
		"vendor/yaml-cpp/include",
		"vendor/stb_image",
		"vendor/spdlog/include",
		"vendor/imgui",
		"vendor/partio/src",
		"%{VULKAN_SDK}/Include",
	}

	files {
		"src/**",
		"vendor/imgui/backends/imgui_impl_vulkan.*",
		"vendor/imgui/backends/imgui_impl_glfw.*",
	}

	links {
		"vendor/glfw/glfw3",
		"vendor/yaml-cpp/lib/%{cfg.buildcfg}/yaml-cpp",
		"vendor/imgui/bin/%{cfg.buildcfg}/ImGui",
		"%{VULKAN_SDK}/Lib/vulkan-1",
		"partio",
	}

	pchheader "engine/hzpch.h"
	pchsource "src/engine/hzpch.cpp"

	-- Filters should come last.
	-- See https://premake.github.io/docs/Filters/
	filter "configurations:Debug"
		links(VULKAN_LIBS_DEBUG)

	filter "configurations:Release"
		links(VULKAN_LIBS_RELEASE)

	filter "configurations:Production"
		links(VULKAN_LIBS_RELEASE)
