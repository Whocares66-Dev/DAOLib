#include "dao/core/RayCast.h"

#include <algorithm>
#include <cmath>

namespace dao::core
{

namespace
{

constexpr std::uint32_t kLeafMost = 4;
constexpr std::size_t kStackMost = 64; // the tree's depth is the log of its triangles and a little

Point Minus(const Point &a, const Point &b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Point Cross(const Point &a, const Point &b)
{
    return {(a[1] * b[2]) - (a[2] * b[1]), (a[2] * b[0]) - (a[0] * b[2]), (a[0] * b[1]) - (a[1] * b[0])};
}

float Dot(const Point &a, const Point &b)
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

// How far along a ray it is inside a box, if it is before `most`: the slab
// test, with one over the direction so a ray along an axis divides by none.
bool MeetsBox(const Point &low, const Point &high, const Point &origin, const Point &inverse, float most)
{
    float enter = 0;
    float leave = most;
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        float near = (low[axis] - origin[axis]) * inverse[axis];
        float far = (high[axis] - origin[axis]) * inverse[axis];
        if (near > far)
        {
            std::swap(near, far);
        }
        // A ray along a face's plane gives a NaN here, which these let through.
        enter = near > enter ? near : enter;
        leave = far < leave ? far : leave;
    }
    return enter <= leave;
}

} // namespace

TriangleTree::TriangleTree(std::span<const Triangle> triangles) : triangles_(triangles.begin(), triangles.end())
{
    order_.resize(triangles_.size());
    std::vector<Point> centres(triangles_.size());
    for (std::uint32_t i = 0; i < triangles_.size(); ++i)
    {
        order_[i] = i;
        const Triangle &t = triangles_[i];
        centres[i] = {(t.a[0] + t.b[0] + t.c[0]) / 3, (t.a[1] + t.b[1] + t.c[1]) / 3, (t.a[2] + t.b[2] + t.c[2]) / 3};
    }
    nodes_.reserve(triangles_.size());
    if (!triangles_.empty())
    {
        Build(0, static_cast<std::uint32_t>(triangles_.size()), centres);
    }
}

std::uint32_t TriangleTree::Build(std::uint32_t first, std::uint32_t count, std::vector<Point> &centres)
{
    const auto index = static_cast<std::uint32_t>(nodes_.size());
    nodes_.emplace_back();
    Point low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Point high{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
               std::numeric_limits<float>::lowest()};
    Point centreLow = low;
    Point centreHigh = high;
    for (std::uint32_t i = first; i < first + count; ++i)
    {
        const Triangle &t = triangles_[order_[i]];
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            low[axis] = std::min({low[axis], t.a[axis], t.b[axis], t.c[axis]});
            high[axis] = std::max({high[axis], t.a[axis], t.b[axis], t.c[axis]});
            centreLow[axis] = std::min(centreLow[axis], centres[order_[i]][axis]);
            centreHigh[axis] = std::max(centreHigh[axis], centres[order_[i]][axis]);
        }
    }
    nodes_[index].low = low;
    nodes_[index].high = high;
    if (count <= kLeafMost)
    {
        nodes_[index].first = first;
        nodes_[index].count = count;
        return index;
    }
    std::size_t axis = 0;
    for (std::size_t other = 1; other < 3; ++other)
    {
        if (centreHigh[other] - centreLow[other] > centreHigh[axis] - centreLow[axis])
        {
            axis = other;
        }
    }
    // Half to each side by the centres along the longer side: the tree is
    // then as deep as the log of its triangles and no deeper, whatever they
    // are, which the stack a ray walks it with counts on.
    const std::uint32_t left = count / 2;
    const auto begin = order_.begin() + static_cast<std::ptrdiff_t>(first);
    std::nth_element(begin, begin + static_cast<std::ptrdiff_t>(left), begin + static_cast<std::ptrdiff_t>(count),
                     [&](std::uint32_t one, std::uint32_t other) { return centres[one][axis] < centres[other][axis]; });
    const std::uint32_t leftChild = Build(first, left, centres);
    const std::uint32_t rightChild = Build(first + left, count - left, centres);
    nodes_[index].first = leftChild;
    nodes_[index].right = rightChild;
    nodes_[index].count = 0;
    return index;
}

namespace
{

// Moeller and Trumbore's test, from either side: where the ray meets the triangle, as a
// distance along it and the place in the triangle, if it does between `least` and `most`.
struct Meeting
{
    float distance = 0;
    float u = 0;
    float v = 0;
};

std::optional<Meeting> Meets(const Triangle &t, const Point &origin, const Point &direction, float least, float most)
{
    const Point e1 = Minus(t.b, t.a);
    const Point e2 = Minus(t.c, t.a);
    const Point p = Cross(direction, e2);
    const float det = Dot(e1, p);
    if (std::abs(det) < 1e-12F)
    {
        return std::nullopt;
    }
    const float inv = 1.0F / det;
    const Point from = Minus(origin, t.a);
    const float u = Dot(from, p) * inv;
    if (u < 0 || u > 1)
    {
        return std::nullopt;
    }
    const Point q = Cross(from, e1);
    const float v = Dot(direction, q) * inv;
    if (v < 0 || u + v > 1)
    {
        return std::nullopt;
    }
    const float distance = Dot(e2, q) * inv;
    if (distance <= least || distance >= most)
    {
        return std::nullopt;
    }
    return Meeting{distance, u, v};
}

} // namespace

std::optional<RayHit> TriangleTree::Cast(const Point &origin, const Point &direction, float least, float most) const
{
    if (nodes_.empty())
    {
        return std::nullopt;
    }
    const Point inverse{1.0F / direction[0], 1.0F / direction[1], 1.0F / direction[2]};
    std::optional<RayHit> nearest;
    std::array<std::uint32_t, kStackMost> stack{};
    std::size_t depth = 0;
    stack[depth++] = 0;
    while (depth > 0)
    {
        // The stack never holds more than it has room for (the pushes below ask first); said
        // here in a way the analyser can follow.
        --depth;
        if (depth >= stack.size())
        {
            break;
        }
        const Node &node = nodes_[stack[depth]];
        if (!MeetsBox(node.low, node.high, origin, inverse, most))
        {
            continue;
        }
        if (node.count == 0)
        {
            if (depth + 2 <= kStackMost)
            {
                stack[depth++] = node.first;
                stack[depth++] = node.right;
            }
            continue;
        }
        for (std::uint32_t i = node.first; i < node.first + node.count; ++i)
        {
            if (const auto met = Meets(triangles_[order_[i]], origin, direction, least, most))
            {
                most = met->distance;
                nearest = RayHit{order_[i], met->distance, met->u, met->v};
            }
        }
    }
    return nearest;
}

} // namespace dao::core
