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

// Capsules (DAOLib #233): the engine's overlap query is PhysX's
// overlapCapsuleShapes (dev/COLLISION.md, section 5), which asks whether a
// capsule shares a point with each shape: a sphere, box or capsule as a
// solid, a triangle mesh by its triangles. Each test here is the distance
// between the capsule's segment and the other thing against the radius, the
// closest points as Ericson's Real-Time Collision Detection gives them (5.1.5
// a point and a triangle, 5.1.9 two segments).
namespace
{

Point Plus(const Point &a, const Point &b)
{
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

Point Times(const Point &a, float s)
{
    return {a[0] * s, a[1] * s, a[2] * s};
}

float Clamp01(float v)
{
    return std::clamp(v, 0.0F, 1.0F);
}

float SquaredDistance(const Point &a, const Point &b)
{
    const Point d = Minus(a, b);
    return Dot(d, d);
}

// The square of the least distance between the segments p1-q1 and p2-q2.
float SegmentsSquared(const Point &p1, const Point &q1, const Point &p2, const Point &q2)
{
    constexpr float kNone = 1e-12F;
    const Point d1 = Minus(q1, p1);
    const Point d2 = Minus(q2, p2);
    const Point r = Minus(p1, p2);
    const float a = Dot(d1, d1);
    const float e = Dot(d2, d2);
    const float f = Dot(d2, r);
    float s = 0;
    float t = 0;
    if (a <= kNone && e <= kNone)
    {
        return Dot(r, r);
    }
    if (a <= kNone)
    {
        t = Clamp01(f / e);
    }
    else
    {
        const float c = Dot(d1, r);
        if (e <= kNone)
        {
            s = Clamp01(-c / a);
        }
        else
        {
            const float b = Dot(d1, d2);
            const float denominator = (a * e) - (b * b);
            // Parallel segments: any s will do, and 0 is as good as another.
            s = denominator > 0 ? Clamp01(((b * f) - (c * e)) / denominator) : 0;
            t = ((b * s) + f) / e;
            if (t < 0)
            {
                t = 0;
                s = Clamp01(-c / a);
            }
            else if (t > 1)
            {
                t = 1;
                s = Clamp01((b - c) / a);
            }
        }
    }
    return SquaredDistance(Plus(p1, Times(d1, s)), Plus(p2, Times(d2, t)));
}

// The point of a triangle nearest a point.
Point NearestOnTriangle(const Point &p, const Triangle &t)
{
    const Point ab = Minus(t.b, t.a);
    const Point ac = Minus(t.c, t.a);
    const Point ap = Minus(p, t.a);
    const float d1 = Dot(ab, ap);
    const float d2 = Dot(ac, ap);
    if (d1 <= 0 && d2 <= 0)
    {
        return t.a;
    }
    const Point bp = Minus(p, t.b);
    const float d3 = Dot(ab, bp);
    const float d4 = Dot(ac, bp);
    if (d3 >= 0 && d4 <= d3)
    {
        return t.b;
    }
    const float vc = (d1 * d4) - (d3 * d2);
    if (vc <= 0 && d1 >= 0 && d3 <= 0)
    {
        return Plus(t.a, Times(ab, d1 / (d1 - d3)));
    }
    const Point cp = Minus(p, t.c);
    const float d5 = Dot(ab, cp);
    const float d6 = Dot(ac, cp);
    if (d6 >= 0 && d5 <= d6)
    {
        return t.c;
    }
    const float vb = (d5 * d2) - (d1 * d6);
    if (vb <= 0 && d2 >= 0 && d6 <= 0)
    {
        return Plus(t.a, Times(ac, d2 / (d2 - d6)));
    }
    const float va = (d3 * d6) - (d5 * d4);
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
    {
        return Plus(t.b, Times(Minus(t.c, t.b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    }
    const float denominator = 1.0F / (va + vb + vc);
    return Plus(t.a, Plus(Times(ab, vb * denominator), Times(ac, vc * denominator)));
}

// Whether the segment p0-p1 passes through a box [low, high], ends included.
bool SegmentMeetsBox(const Point &p0, const Point &p1, const Point &low, const Point &high)
{
    const Point d = Minus(p1, p0);
    float enter = 0;
    float leave = 1;
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        if (std::abs(d[axis]) < 1e-12F)
        {
            if (p0[axis] < low[axis] || p0[axis] > high[axis])
            {
                return false;
            }
            continue;
        }
        float near = (low[axis] - p0[axis]) / d[axis];
        float far = (high[axis] - p0[axis]) / d[axis];
        if (near > far)
        {
            std::swap(near, far);
        }
        enter = std::max(enter, near);
        leave = std::min(leave, far);
        if (enter > leave)
        {
            return false;
        }
    }
    return true;
}

// The square of a point's distance from a box [low, high], 0 inside it.
float BoxSquared(const Point &p, const Point &low, const Point &high)
{
    float sum = 0;
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        const float out = std::max({low[axis] - p[axis], p[axis] - high[axis], 0.0F});
        sum += out * out;
    }
    return sum;
}

// Whether a capsule's bounding box meets a box [low, high].
bool BoundsMeet(const Capsule &capsule, const Point &low, const Point &high)
{
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        if (std::min(capsule.p0[axis], capsule.p1[axis]) - capsule.radius > high[axis] ||
            std::max(capsule.p0[axis], capsule.p1[axis]) + capsule.radius < low[axis])
        {
            return false;
        }
    }
    return true;
}

} // namespace

bool Overlaps(const Capsule &capsule, const Triangle &triangle)
{
    const float most = capsule.radius * capsule.radius;
    const Point direction = Minus(capsule.p1, capsule.p0);
    // Through the triangle: the distance is none. Otherwise the nearest points
    // are an end of the segment and the triangle, or the segment and an edge.
    if (Meets(triangle, capsule.p0, direction, 0, 1))
    {
        return true;
    }
    return SquaredDistance(capsule.p0, NearestOnTriangle(capsule.p0, triangle)) <= most ||
           SquaredDistance(capsule.p1, NearestOnTriangle(capsule.p1, triangle)) <= most ||
           SegmentsSquared(capsule.p0, capsule.p1, triangle.a, triangle.b) <= most ||
           SegmentsSquared(capsule.p0, capsule.p1, triangle.b, triangle.c) <= most ||
           SegmentsSquared(capsule.p0, capsule.p1, triangle.c, triangle.a) <= most;
}

bool Overlaps(const Capsule &capsule, const Capsule &other)
{
    const float reach = capsule.radius + other.radius;
    return SegmentsSquared(capsule.p0, capsule.p1, other.p0, other.p1) <= reach * reach;
}

bool Overlaps(const Capsule &capsule, const Point &halfExtents)
{
    const Point high = halfExtents;
    const Point low{-high[0], -high[1], -high[2]};
    if (SegmentMeetsBox(capsule.p0, capsule.p1, low, high))
    {
        return true;
    }
    // Apart, the nearest points are an end of the segment and the box, or the
    // segment and one of the box's twelve edges: a face's nearest point to an
    // inner point of the segment means the segment runs along the face, and
    // then an end or an edge is as near.
    const float most = capsule.radius * capsule.radius;
    if (BoxSquared(capsule.p0, low, high) <= most || BoxSquared(capsule.p1, low, high) <= most)
    {
        return true;
    }
    auto corner = [&](int i) {
        return Point{(i & 1) != 0 ? high[0] : low[0], (i & 2) != 0 ? high[1] : low[1], (i & 4) != 0 ? high[2] : low[2]};
    };
    for (int i = 0; i < 8; ++i)
    {
        for (int bit = 1; bit < 8; bit <<= 1)
        {
            if ((i & bit) == 0 && SegmentsSquared(capsule.p0, capsule.p1, corner(i), corner(i | bit)) <= most)
            {
                return true;
            }
        }
    }
    return false;
}

bool TriangleTree::Overlaps(const Capsule &capsule) const
{
    if (nodes_.empty())
    {
        return false;
    }
    std::array<std::uint32_t, kStackMost> stack{};
    std::size_t depth = 0;
    stack[depth++] = 0;
    while (depth > 0)
    {
        --depth;
        if (depth >= stack.size())
        {
            break;
        }
        const Node &node = nodes_[stack[depth]];
        if (!BoundsMeet(capsule, node.low, node.high))
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
            if (core::Overlaps(capsule, triangles_[order_[i]]))
            {
                return true;
            }
        }
    }
    return false;
}

} // namespace dao::core
