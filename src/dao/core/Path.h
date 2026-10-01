// Lengths along a walked path: what dao/World.h's WalkPath measures with,
// over plain points so it can be tested without the game.
#pragma once

#include <cmath>
#include <cstddef>
#include <span>

namespace dao::core
{

struct Point3
{
    float x = 0;
    float y = 0;
    float z = 0;
};

inline float Distance(const Point3 &a, const Point3 &b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Along the points, first to last.
inline float PathLength(std::span<const Point3> points)
{
    float total = 0;
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        total += Distance(points[i - 1], points[i]);
    }
    return total;
}

// From `from` to the point at `next`, then along the rest; 0 past the end.
inline float PathRemaining(std::span<const Point3> points, std::size_t next, const Point3 &from)
{
    if (next >= points.size())
    {
        return 0;
    }
    return Distance(from, points[next]) + PathLength(points.subspan(next));
}

} // namespace dao::core
