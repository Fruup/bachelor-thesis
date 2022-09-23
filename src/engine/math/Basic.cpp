#include "engine/hzpch.h"
#include "engine/math/Basic.h"

namespace Math
{
	LineLineIntersection ComputeLineLineIntersection(const Line& a, const Line& b)
	{
		// are the lines the same
		if (a == b)
		{
			return LineLineIntersection{
				.Parallel = true,
				.Point = .5f * (a.a + a.b),
				.t1 = .5f,
				.t2 = .5f,
			};
		}

		// compute intersection
		LineLineIntersection intersection;

		glm::vec2 ar = a.b - a.a;
		glm::vec2 br = b.b - b.a;

		float num = Cross(a.a - b.a, br);
		float denom = -Cross(ar, br);

		if (std::abs(denom) < Epsilon)
			intersection.Parallel = true;
		else
		{
			intersection.t1 = num / denom;
			intersection.t2 = Cross(a.a - b.a, ar) / denom;
			intersection.Point = (1.0f - intersection.t1) * a.a + intersection.t1 * a.b;
		}

		return intersection;
	}

	float PointLineDistance(const glm::vec2& p, const Line& l)
	{
		glm::vec2 r = l.b - l.a;
		float k = glm::dot(r, p - l.a) / glm::length(r);

		return glm::distance(l.a + std::clamp(k, 0.0f, 1.0f) * r, p);

		/*if (glm::dot(l.a - l.b, p - l.b) >= 0.0f && glm::dot(l.b - l.a, p - l.a) >= 0.0f)
			return Cross(l.b - l.a, p - l.a) / glm::distance(l.a, l.b);
		else
			return std::min(glm::distance(l.a, p), glm::distance(l.b, p));*/
	}

	glm::vec2 ComputeBasePoint(const glm::vec2& p, const Line& l)
	{
		glm::vec2 r = l.b - l.a;
		float k = glm::dot(r, p - l.a) / glm::length(r);
		return l.a + k * r;
	}
}
