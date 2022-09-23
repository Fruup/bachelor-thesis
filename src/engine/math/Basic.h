#pragma once

#include <glm/glm.hpp>

namespace Math
{
	constexpr float Epsilon = 1e-6f;

	inline float Cross(const glm::vec2& a, const glm::vec2& b)
	{
		return a.x * b.y - a.y * b.x;
	}

	inline glm::vec2 Skew(const glm::vec2& v)
	{
		return glm::vec2(-v.y, v.x);
	}

	// lines

	struct Line
	{
		glm::vec2 a, b;

		bool operator == (const Line& other) const
		{
			return
				(a == other.a && b == other.b) ||
				(a == other.b && b == other.a);
		}
	};

	struct LineLineIntersection
	{
		bool Parallel = false;
		glm::vec2 Point{};
		float t1{ -1.0f }, t2{ -1.0f };

		bool ExistsExclusive() const
		{
			return t1 > 0.0f && t1 < 1.0f && t2 > 0.0f && t2 < 1.0f;
		}

		bool ExistsInclusive() const
		{
			return t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f;
		}

		bool ExistsHalfInclusive() const
		{
			return t1 >= 0.0f && t1 < 1.0f && t2 >= 0.0f && t2 < 1.0f;
		}

		operator bool() const { return ExistsInclusive(); }
	};

	LineLineIntersection ComputeLineLineIntersection(const Line& a, const Line& b);

	float PointLineDistance(const glm::vec2& p, const Line& l);

	glm::vec2 ComputeBasePoint(const glm::vec2& p, const Line& l);
}
