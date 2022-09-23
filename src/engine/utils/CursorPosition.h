#pragma once

struct CursorPosition
{
	operator glm::vec2() const { return { float(x), float(y) }; }
	CursorPosition operator - (const CursorPosition& rhs) const { return { x - rhs.x, y - rhs.y }; }
	double Distance(const CursorPosition& b) const { return (b.x - x) * (b.x - x) + (b.y - y) * (b.y - y); }

	double x, y;
};
