#pragma once

#include <iostream>
#include <cstdint>
#include <cmath>
#include <memory>
#include <optional>
#include <filesystem>

#include <list>
#include <queue>
#include <string>
#include <vector>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <spdlog/spdlog.h>
#include <glfw/glfw3.h>
#include <yaml-cpp/yaml.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <imgui/imgui.h>

#include "engine/Engine.h"
