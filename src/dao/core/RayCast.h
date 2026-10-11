// Rays against triangles, quickly: a tree of boxes over a set of triangles,
// and the nearest triangle a ray meets. What bakes a level's light probes
// from its geometry in a second or two (DAOLib #65): a room and the rooms
// seen from it are half a million triangles, and a ray tried against each
// took a second a probe.
//
// The tree is made once over triangles that then stay as they are: each
// node a box and either two children or a few triangles, half of its
// triangles to each child by their centres along the box's longer side. A triangle is met
// from either side. Nothing here allocates while a ray is cast.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace dao::core
{

using Point = std::array<float, 3>;

struct Triangle
{
    Point a{};
    Point b{};
    Point c{};
};

// Where a ray met a triangle.
struct RayHit
{
    std::uint32_t triangle = 0; // its index in what the tree was made over
    float distance = 0;         // along the ray, in lengths of its direction
    float u = 0;                // the weight of the triangle's b there
    float v = 0;                // and of its c; a's is 1 - u - v
};

class TriangleTree
{
  public:
    // Makes the tree over `triangles`, which are copied.
    explicit TriangleTree(std::span<const Triangle> triangles);

    // Returns the nearest triangle a ray meets.
    // @param direction of any length but none; the hit's distance is in its lengths
    // @param least how far along the ray a hit counts from, so a ray that
    // starts on a surface does not meet it
    // @param most how far along it a hit still counts
    // @return nullopt if it meets none
    std::optional<RayHit> Cast(const Point &origin, const Point &direction, float least = 1e-3F,
                               float most = std::numeric_limits<float>::infinity()) const;

    std::size_t Size() const
    {
        return triangles_.size();
    }
    const Triangle &At(std::size_t index) const
    {
        return triangles_[index];
    }

  private:
    struct Node
    {
        Point low{};
        Point high{};
        std::uint32_t first = 0; // of its triangles in order_, or its left child
        std::uint32_t right = 0; // its right child
        std::uint32_t count = 0; // of its triangles; 0 for a node with children
    };

    std::uint32_t Build(std::uint32_t first, std::uint32_t count, std::vector<Point> &centres);

    std::vector<Triangle> triangles_;
    std::vector<std::uint32_t> order_;
    std::vector<Node> nodes_;
};

} // namespace dao::core
