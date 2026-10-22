// dao/core/RayCast.h's capsules against triangles, boxes and capsules, and
// dao/core/Collision.h's overlap query over a level's shapes and a
// placeable's, as SetPosition's snap asks the engine's physics (DAOLib #233).
#include "dao/core/Collision.h"
#include "dao/core/RayCast.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

using dao::core::Capsule;
using dao::core::Point;
using dao::core::Triangle;
using dao::core::TriangleTree;
namespace collision = dao::core::collision;

// A capsule standing upright at (x, y), from z0 to z1.
Capsule Upright(float x, float y, float z0, float z1, float radius)
{
    return Capsule{{x, y, z0}, {x, y, z1}, radius};
}

// The floor's triangle under the origin: (0,0,0), (4,0,0), (0,4,0).
constexpr Triangle kFloor{{0, 0, 0}, {4, 0, 0}, {0, 4, 0}};

} // namespace

TEST_CASE("a capsule touches a triangle it passes through or comes within its radius of", "[overlap]")
{
    CHECK(dao::core::Overlaps(Upright(1, 1, -1, 1, 0.1F), kFloor));    // through the face
    CHECK(dao::core::Overlaps(Upright(1, 1, 0.25F, 2, 0.3F), kFloor)); // its lower end above the face
    CHECK_FALSE(dao::core::Overlaps(Upright(1, 1, 0.35F, 2, 0.3F), kFloor));
    // Beside the hypotenuse, from (4,0) to (0,4): the point (2.3, 2.3) is 0.3·√2 ≈ 0.42 from it.
    CHECK(dao::core::Overlaps(Upright(2.3F, 2.3F, -1, 1, 0.45F), kFloor));
    CHECK_FALSE(dao::core::Overlaps(Upright(2.3F, 2.3F, -1, 1, 0.4F), kFloor));
    // Lying along x past the corner at the origin: its end is 0.5 from it.
    CHECK(dao::core::Overlaps(Capsule{{-0.5F, 0, 0}, {-3, 0, 0}, 0.5F}, kFloor));
    CHECK_FALSE(dao::core::Overlaps(Capsule{{-0.6F, 0, 0}, {-3, 0, 0}, 0.5F}, kFloor));
    // In the triangle's plane, crossing an edge from outside: met with no radius.
    CHECK(dao::core::Overlaps(Capsule{{1, -1, 0}, {1, 1, 0}, 0}, kFloor));
}

TEST_CASE("a capsule overlaps a capsule or a sphere when their segments are within both radii", "[overlap]")
{
    const Capsule post = Upright(0, 0, 0, 2, 0.5F);
    CHECK(dao::core::Overlaps(post, Upright(0.9F, 0, 0, 2, 0.5F)));
    CHECK_FALSE(dao::core::Overlaps(post, Upright(1.1F, 0, 0, 2, 0.5F)));
    const Capsule ball{{0, 0, 3}, {0, 0, 3}, 0.6F}; // 1 above the post's top end
    CHECK(dao::core::Overlaps(post, ball));
    CHECK_FALSE(dao::core::Overlaps(post, Capsule{{0, 0, 3.2F}, {0, 0, 3.2F}, 0.6F}));
    // Crossed, not parallel: the nearest points are inside both segments.
    CHECK(dao::core::Overlaps(Capsule{{-1, 0, 1}, {1, 0, 1}, 0.1F}, Capsule{{0, -1, 1.15F}, {0, 1, 1.15F}, 0.1F}));
    CHECK_FALSE(
        dao::core::Overlaps(Capsule{{-1, 0, 1}, {1, 0, 1}, 0.1F}, Capsule{{0, -1, 1.25F}, {0, 1, 1.25F}, 0.1F}));
}

TEST_CASE("a box is solid, and a capsule overlaps it within its radius of a face, an edge or a corner", "[overlap]")
{
    const Point half{1, 1, 1};
    CHECK(dao::core::Overlaps(Upright(0, 0, -0.5F, 0.5F, 0.1F), half)); // wholly inside
    CHECK(dao::core::Overlaps(Upright(0, 0, 1.2F, 3, 0.3F), half));     // above the top face
    CHECK_FALSE(dao::core::Overlaps(Upright(0, 0, 1.4F, 3, 0.3F), half));
    // Beside the edge along z at (1, 1): the axis at (1.3, 1.3) is 0.3·√2 ≈ 0.42 away.
    CHECK(dao::core::Overlaps(Upright(1.3F, 1.3F, -3, 3, 0.45F), half));
    CHECK_FALSE(dao::core::Overlaps(Upright(1.3F, 1.3F, -3, 3, 0.4F), half));
    // A sphere off the corner (1, 1, 1), 0.2·√3 ≈ 0.35 from it.
    CHECK(dao::core::Overlaps(Capsule{{1.2F, 1.2F, 1.2F}, {1.2F, 1.2F, 1.2F}, 0.36F}, half));
    CHECK_FALSE(dao::core::Overlaps(Capsule{{1.2F, 1.2F, 1.2F}, {1.2F, 1.2F, 1.2F}, 0.34F}, half));
    // Through the box with neither end in it.
    CHECK(dao::core::Overlaps(Capsule{{-3, 0.5F, 0}, {3, 0.5F, 0}, 0}, half));
}

TEST_CASE("a tree of triangles is touched by a capsule exactly when one of its triangles is", "[overlap]")
{
    // Scattered by a fixed rule, so each run tries the same: small triangles through a 40 m cube, and
    // capsules among them.
    auto at = [](int i, int stride) { return static_cast<float>(((i * stride) % 400) - 200) * 0.1F; };
    std::vector<Triangle> triangles;
    for (int i = 0; i < 400; ++i)
    {
        const Point a{at(i, 7), at(i, 13), at(i, 31)};
        const float s = at(i, 3) * 0.05F;
        triangles.push_back({a, {a[0] + s, a[1] - (s * 0.5F), a[2] + 0.3F}, {a[0] - 0.4F, a[1] + s, a[2] - s}});
    }
    const TriangleTree tree(triangles);
    int touched = 0;
    for (int i = 0; i < 300; ++i)
    {
        // Every third stands 0.3 to 0.9 beside a triangle's corner, the rest anywhere.
        const Point near = triangles[static_cast<std::size_t>(i)].a;
        const float off = 0.3F + (static_cast<float>(i % 7) * 0.1F);
        const Capsule capsule = i % 3 == 0
                                    ? Upright(near[0] + off, near[1], near[2] - 0.5F, near[2] + 0.5F, 0.5F)
                                    : Upright(at(i, 11), at(i, 17), at(i, 23), at(i, 23) + (at(i, 5) * 0.1F), 0.5F);
        bool any = false;
        for (const Triangle &t : triangles)
        {
            any = any || dao::core::Overlaps(capsule, t);
        }
        CHECK(tree.Overlaps(capsule) == any);
        touched += any ? 1 : 0;
    }
    // The sample touches some and misses some, so both answers are tried.
    CHECK(touched > 0);
    CHECK(touched < 300);
    CHECK_FALSE(TriangleTree(std::vector<Triangle>{}).Overlaps(Upright(0, 0, 0, 1, 1)));
}

TEST_CASE("a creature's capsule stands its height about its centre above the feet", "[overlap]")
{
    // A human's controller: radius 0.4, the caps' centres 0.8 apart, the centre 0.9 above the feet.
    const Capsule capsule = collision::CreatureCapsule({10, 20, 5}, 0.4F, 0.8F, 0.9F);
    CHECK(capsule.p0[0] == 10);
    CHECK(capsule.p0[1] == 20);
    CHECK(std::abs(capsule.p0[2] - 5.5F) < 1e-5F);
    CHECK(capsule.p1[0] == 10);
    CHECK(std::abs(capsule.p1[2] - 6.3F) < 1e-5F);
    CHECK(capsule.radius == 0.4F);
}

TEST_CASE("a quaternion's pose turns as the quaternion does", "[overlap]")
{
    // 90 degrees about z: x goes to y.
    const float s = 0.70710678F;
    const collision::Pose turned = collision::PoseOf({0, 0, s, s}, {1, 2, 3});
    const Point moved = collision::Apply(turned, {1, 0, 0});
    CHECK(std::abs(moved[0] - 1) < 1e-5F);
    CHECK(std::abs(moved[1] - 3) < 1e-5F);
    CHECK(std::abs(moved[2] - 3) < 1e-5F);
    // Not of length 1: the same turn.
    const Point again = collision::Apply(collision::PoseOf({0, 0, 2, 2}, {1, 2, 3}), {1, 0, 0});
    CHECK(std::abs(again[1] - 3) < 1e-5F);
    // None at all: no turn.
    CHECK(collision::PoseOf({0, 0, 0, 0}, {1, 2, 3}).rotation == std::array<float, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1});
}

TEST_CASE("a shape placed by a pose is overlapped in its own turn: a turned box, a lying capsule", "[overlap]")
{
    // A box 4 long along its x and 0.2 thick, turned 90 degrees about z, at (5, 5): it runs along y.
    const float s = 0.70710678F;
    const collision::Pose turned = collision::PoseOf({0, 0, s, s}, {5, 5, 0});
    const Point half{2, 0.1F, 1};
    CHECK(collision::Overlaps(Upright(5, 6.5F, -0.5F, 0.5F, 0.2F), collision::Kind::Box, half, 0, 0, turned));
    CHECK_FALSE(collision::Overlaps(Upright(6.5F, 5, -0.5F, 0.5F, 0.2F), collision::Kind::Box, half, 0, 0, turned));
    // A capsule along its y, laid along x by the turn: from (4, 5) to (6, 5), radius 0.25.
    const collision::Pose laid = collision::PoseOf({0, 0, -s, s}, {5, 5, 0});
    CHECK(collision::Overlaps(Upright(6, 5.4F, -1, 1, 0.2F), collision::Kind::Capsule, {}, 0.25F, 2, laid));
    CHECK_FALSE(collision::Overlaps(Upright(5, 6, -1, 1, 0.2F), collision::Kind::Capsule, {}, 0.25F, 2, laid));
    CHECK(collision::Overlaps(Upright(5, 5.5F, 0, 1, 0.1F), collision::Kind::Sphere, {}, 0.5F, 0, laid));
    CHECK_FALSE(collision::Overlaps(Upright(5, 5, 0, 1, 0.1F), collision::Kind::Mesh, {}, 0, 0, laid));
}

TEST_CASE("a box turned every way is met where its pose puts its own points, and not off its thin sides", "[overlap]")
{
    // A rod 4 long along its own x, 0.2 thick, under a turn about no one axis.
    const collision::Pose pose = collision::PoseOf({0.3F, 0.5F, 0.2F, 0.79F}, {2, -3, 1});
    const Point half{2, 0.1F, 0.1F};
    const Point alongRod = collision::Apply(pose, {1.5F, 0, 0});
    const Point offRod = collision::Apply(pose, {0, 1.5F, 0});
    const Point aboveRod = collision::Apply(pose, {0, 0, 1.5F});
    CHECK(collision::Overlaps(Capsule{alongRod, alongRod, 0.05F}, collision::Kind::Box, half, 0, 0, pose));
    CHECK_FALSE(collision::Overlaps(Capsule{offRod, offRod, 0.05F}, collision::Kind::Box, half, 0, 0, pose));
    CHECK_FALSE(collision::Overlaps(Capsule{aboveRod, aboveRod, 0.05F}, collision::Kind::Box, half, 0, 0, pose));
}

TEST_CASE("a level's shapes overlap a capsule by their bits0 and the query's mask, a mesh by its triangles",
          "[overlap]")
{
    collision::StaticPhysics physics;
    collision::TriangleMesh floor;
    floor.points = {{-10, -10, 0}, {10, -10, 0}, {10, 10, 0}, {-10, 10, 0}};
    floor.triangles = {{0, 1, 2}, {0, 2, 3}};
    physics.meshes.push_back(floor);
    collision::Actor ground;
    collision::StaticShape mesh;
    mesh.kind = collision::Kind::Mesh;
    mesh.mesh = 0;
    mesh.groupsMask = {0x82002020, 0, 0, 0}; // walkable static geometry
    ground.shapes.push_back(mesh);
    ground.globalPose.translation = {0, 0, 1}; // the floor at z 1
    physics.actors.push_back(ground);
    collision::Actor wall;
    collision::StaticShape box;
    box.kind = collision::Kind::Box;
    box.dimensions = {0.5F, 5, 2};
    box.groupsMask = {0x80010000, 0, 0, 0}; // a terrain wall alone
    box.localPose.translation = {3, 0, 2};
    wall.shapes.push_back(box);
    physics.actors.push_back(wall);

    // Stood on the floor as the engine stands a creature, the capsule's foot 0.1 above it.
    const Capsule standing = collision::CreatureCapsule({0, 0, 1}, 0.4F, 0.8F, 0.9F);
    CHECK_FALSE(collision::Overlaps(physics, standing, collision::kPlacementMask));
    // Sunk into it: the mesh is met.
    CHECK(collision::Overlaps(physics, collision::CreatureCapsule({0, 0, 0.8F}, 0.4F, 0.8F, 0.9F),
                              collision::kPlacementMask));
    // Inside the wall: met by the placement mask, not by the sight ray's, which no terrain wall meets.
    const Capsule inWall = collision::CreatureCapsule({3, 0, 1}, 0.4F, 0.8F, 0.9F);
    CHECK(collision::Overlaps(physics, inWall, collision::kPlacementMask));
    CHECK_FALSE(collision::Overlaps(physics, inWall, collision::kSightMask));
    // A mesh shape whose mesh is not there meets nothing.
    physics.meshes.clear();
    CHECK_FALSE(collision::Overlaps(physics, collision::CreatureCapsule({0, 0, 0.8F}, 0.4F, 0.8F, 0.9F),
                                    collision::kPlacementMask));
}

TEST_CASE("a placeable's shapes overlap a capsule where it stands, turned, by their masks", "[overlap]")
{
    collision::Shape crate;
    crate.kind = collision::Kind::Box;
    crate.dimensions = {0.5F, 0.5F, 0.5F};
    crate.position = {1, 0, 0.5F}; // off the model's origin along its x
    crate.mask = 0x81000004;       // a placeable
    collision::Shape trigger = crate;
    trigger.mask = 0x8; // what the query's mask leaves out
    trigger.position = {-1, 0, 0.5F};
    const std::vector<collision::Shape> shapes{crate, trigger};
    // The placeable at (10, 10), turned 90 degrees about z: the crate is at (10, 11).
    const float s = 0.70710678F;
    const collision::Pose placed = collision::PoseOf({0, 0, s, s}, {10, 10, 0});
    CHECK(collision::Overlaps(shapes, placed, Upright(10, 11, 0.2F, 0.8F, 0.1F), collision::kPlacementMask));
    CHECK_FALSE(collision::Overlaps(shapes, placed, Upright(11, 10, 0.2F, 0.8F, 0.1F), collision::kPlacementMask));
    // The trigger at (10, 9) is not in the mask.
    CHECK_FALSE(collision::Overlaps(shapes, placed, Upright(10, 9, 0.2F, 0.8F, 0.1F), collision::kPlacementMask));
    CHECK(collision::Overlaps(shapes, placed, Upright(10, 9, 0.2F, 0.8F, 0.1F), collision::kTriggers));
}
