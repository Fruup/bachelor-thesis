#include "engine/hzpch.h"
#include "engine/math/Polygon.h"
#include "engine/math/Basic.h"

namespace Math
{
	inline float det(const glm::vec2& a, const glm::vec2& b)
	{
		return a.x * b.y - a.y * b.x;
	}

	inline float det(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
	{
		return det(b - a, c - a);
	}

	inline bool IsConcave(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
	{
		return det(a, b, c) < 0.0f;
	}

	bool Polygon::CheckPoint(const glm::vec2& p) const
	{
		if (p.x < m_AABB.Position.x || p.y < m_AABB.Position.y ||
			p.x > m_AABB.Position.x + m_AABB.Extent.x || p.y > m_AABB.Position.y + m_AABB.Extent.y)
			return false;

		Line l{ m_AABB.Position - glm::vec2(5), p };

		int c = 0;
		for (size_t i = 0; i < size(); i++)
		{
			auto intersection = ComputeLineLineIntersection({ m_Points[i], m_Points[(i + 1) % size()] }, l);
			if (intersection.ExistsHalfInclusive())
				++c;
		}

		return c % 2;
	}

	bool Polygon::CheckLine(const Math::Line& l) const
	{
		for (size_t i = 0; i < size(); ++i)
		{
			Math::Line pl = { m_Points[i], m_Points[(i + 1) % size()] };

			if (pl != l && Math::ComputeLineLineIntersection(pl, l).ExistsInclusive())
				return true;
		}

		return false;
	}

	void Polygon::ComputeAABB()
	{
		float top = FLT_MAX, left = FLT_MAX;
		float bottom = FLT_MIN, right = FLT_MIN;

		for (auto& p : m_Points)
		{
			if (p.x < left) left = p.x;
			if (p.x > right) right = p.x;
			if (p.y < top) top = p.y;
			if (p.y > bottom) bottom = p.y;
		}

		m_AABB.Position = { left, top };
		m_AABB.Extent = { right - left, bottom - top };
	}

	void Polygon::ReorderPoints()
	{
		ComputeConcaveIndices();

		if (m_ConcaveIndices.size() > size() / 2)
		{
			for (size_t i = 1; i <= size() / 2; i++)
				std::swap(m_Points[i], m_Points[size() - i]);
		}

		ComputeConcaveIndices();
	}

	void Polygon::ComputeInnerPoints()
	{
		m_InnerPoints.resize(0);
		m_InnerPoints.reserve(size());

		size_t N = size();
		const float Displacement = std::max(m_AABB.Extent.x, m_AABB.Extent.y) * 3e-3f;

		for (size_t i = 0; i < N; i++)
		{
			const glm::vec2& a = m_Points[i];
			const glm::vec2& b = m_Points[(i + N - 1) % N];
			const glm::vec2& c = m_Points[(i + 1) % N];

			glm::vec2 rb = glm::normalize(a - b);
			glm::vec2 rc = glm::normalize(a - c);

			float k = -1.0f;
			if (IsConcave(a, b, c))
				k = +1.0f;

			m_InnerPoints.push_back(a + k * Displacement * glm::normalize(rb + rc));

#if 0
			if (IsConcave(a, b, c))
			{
				// add two inner vertices
				m_InnerPoints.push_back(a + Displacement * rc);
				m_InnerPoints.push_back(a + Displacement * rb);
			}
			else
			{
				// add only one inner vertex
				m_InnerPoints.push_back(a - Displacement * glm::normalize(rb + rc));
			}
#endif
		}
	}

	void Polygon::ComputeConcaveIndices()
	{
		m_ConcaveIndices.resize(0);
		size_t N = size();

		for (size_t i = 0; i < N; i++)
			if (IsConcave(m_Points[i], m_Points[(i + N - 1) % N], m_Points[(i + 1) % N]))
				m_ConcaveIndices.push_back(i);
	}

	std::vector<glm::vec2> Polygon::GetConcavePoints(bool inner) const
	{
		std::vector<glm::vec2> r;

#if 0
		if (inner)
		{
			r.reserve(2 * m_ConcaveIndices.size());

			for (auto& i : m_ConcaveIndices)
			{
				r.push_back(m_InnerPoints[i]);
				r.push_back(m_InnerPoints[i + 1]);
			}
		}
		else
		{
			r.reserve(m_ConcaveIndices.size());

			for (auto& i : m_ConcaveIndices)
				r.push_back(m_Points[i]);
		}
#endif

		r.reserve(m_ConcaveIndices.size());

		if (inner)
			for (auto& i : m_ConcaveIndices)
				r.push_back(m_InnerPoints[i]);
		else
			for (auto& i : m_ConcaveIndices)
				r.push_back(m_Points[i]);

		return r;
	}
}
