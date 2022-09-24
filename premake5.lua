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

	openmp "On"

	filter "configurations:Debug"
		links(VULKAN_LIBS_DEBUG)

		defines {
			"DEBUG",
		}

		symbols "On"

	filter "configurations:Release"
		links(VULKAN_LIBS)

		defines {
			"NDEBUG",
			"RELEASE",
		}

		optimize "On"

	filter "configurations:Production"
		links(VULKAN_LIBS)

		defines {
			"NDEBUG",
			"PRODUCTION",
		}

		optimize "On"

	filter "platforms:Win64"
		system "Windows"
		architecture "x64"

	includedirs {
		"src",
		"vendor",
		"vendor/glm",
		"vendor/yaml-cpp/include",
		"vendor/stb_image",
		"vendor/spdlog/include",
		"vendor/imgui",
		"vendor/partio/include",
		"%{VULKAN_SDK}/Include",
	}

	files {
		"vendor/imgui/backends/imgui_impl_vulkan.*",
		"vendor/imgui/backends/imgui_impl_glfw.*",
	}

	links {
		"vendor/glfw/glfw3",
		"vendor/yaml-cpp/lib/%{cfg.buildcfg}/yaml-cpp",
		"vendor/imgui/bin/%{cfg.buildcfg}/ImGui",
		"vendor/partio/lib/%{cfg.buildcfg}/partio",
		"vendor/zlib/lib/%{cfg.buildcfg}/zlib",
		"%{VULKAN_SDK}/Lib/vulkan-1",
	}

	defines {
		"SOLUTION_DIRECTORY=\"%{wks.location}\""
	}

-- projects

project "fluids"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	files "src/**"

	pchheader "engine/hzpch.h"
	pchsource "src/engine/hzpch.cpp"
