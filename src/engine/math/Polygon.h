#pragma once

#include <engine/math/Basic.h>

namespace Math
{
	struct Rect
	{
		glm::vec2 Position;
		glm::vec2 Extent;
	};

	class Polygon
	{
	public:
		Polygon() = default;

		Polygon(const std::vector<glm::vec2>& points)
		{
			m_Points = points;
			ReorderPoints();
			ComputeAABB();
			ComputeInnerPoints();
		}

		Polygon(std::vector<glm::vec2>&& points)
		{
			m_Points = points;
			ReorderPoints();
			ComputeAABB();
			ComputeInnerPoints();
		}

		bool CheckPoint(const glm::vec2& p) const;
		bool CheckLine(const Math::Line& l) const;

		const std::vector<glm::vec2>& GetPoints(bool inner = false) const { return inner ? m_InnerPoints : m_Points; }
		std::vector<glm::vec2> GetConcavePoints(bool inner = false) const;
		const std::vector<size_t>& GetConcaveIndices() const { return m_ConcaveIndices; }
		size_t size() const { return m_Points.size(); }
		const Rect& GetAABB() const { return m_AABB; }

		const glm::vec2& operator [] (size_t i) const { return m_Points[i]; }

	private:
		void ComputeAABB();
		void ReorderPoints();
		void ComputeInnerPoints();
		void ComputeConcaveIndices();

	private:
		std::vector<glm::vec2> m_Points;
		std::vector<glm::vec2> m_InnerPoints;
		std::vector<size_t> m_ConcaveIndices;
		Rect m_AABB;
	};
}
