#pragma once

#include <yaml-cpp/yaml.h>
#include <glm/glm.hpp>

namespace YAML
{
	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& v)
		{
			Node node;
			for (glm::length_t i = 0; i < v.length(); i++)
				node.push_back(v[i]);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& v)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			for (glm::length_t i = 0; i < v.length(); i++)
				v[i] = node[i].as<float>();

			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& v)
		{
			Node node;
			for (glm::length_t i = 0; i < v.length(); i++)
				node.push_back(v[i]);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& v)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			for (glm::length_t i = 0; i < v.length(); i++)
				v[i] = node[i].as<float>();

			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& v)
		{
			Node node;
			for (glm::length_t i = 0; i < v.length(); i++)
				node.push_back(v[i]);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& v)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			for (glm::length_t i = 0; i < v.length(); i++)
				v[i] = node[i].as<float>();

			return true;
		}
	};
}
