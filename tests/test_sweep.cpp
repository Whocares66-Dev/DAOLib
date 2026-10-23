// dao/core/RayCast.h's capsule sweep against triangles, boxes and capsules, and dao/core/Collision.h's over a level's
// shapes and a placeable's, as the engine's SweepCapsule reports them (DAOLib #238, dev/COLLISION.md section 6).
#include "dao/core/Collision.h"
#include "dao/core/RayCast.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

namespace
{

using dao::core::Capsule;
using dao::core::Point;
using dao::core::SweepHit;
using dao::core::Triangle;
using dao::core::TriangleTree;
namespace collision = dao::core::collision;

constexpr float kTolerance = 1e-4F;

bool Near(float a, float b)
{
    return std::abs(a - b) < kTolerance;
}

bool Near(const Point &a, const Point &b)
{
    return Near(a[0], b[0]) && Near(a[1], b[1]) && Near(a[2], b[2]);
}

Capsule Ball(float x, float y, float z, float radius)
{
    return Capsule{{x, y, z}, {x, y, z}, radius};
}

Capsule Upright(float x, float y, float z0, float z1, float radius)
{
    return Capsule{{x, y, z0}, {x, y, z1}, radius};
}

constexpr Triangle kFloor{{0, 0, 0}, {4, 0, 0}, {0, 4, 0}};

} // namespace

TEST_CASE("a sphere swept at a triangle touches it at the analytic t, over its face, with the surface's normal",
          "[sweep]")
{
    // The centre falls 2 from z = 2 and touches when it is r = 0.5 above the face: three quarters of the way.
    const auto hit = dao::core::Sweep(Ball(1, 1, 2, 0.5F), {0, 0, -2}, kFloor);
    REQUIRE(hit);
    CHECK(Near(hit->t, 0.75F));
    CHECK(Near(hit->point, {1, 1, 0}));
    CHECK(Near(hit->normal, {0, 0, 1}));
    CHECK_FALSE(hit->startsInside);
    // From below, the other side of the face: the normal turns toward the capsule.
    const auto below = dao::core::Sweep(Ball(1, 1, -2, 0.5F), {0, 0, 2}, kFloor);
    REQUIRE(below);
    CHECK(Near(below->t, 0.75F));
    CHECK(Near(below->normal, {0, 0, -1}));
    // Not far enough, and not over the face.
    CHECK_FALSE(dao::core::Sweep(Ball(1, 1, 2, 0.5F), {0, 0, -1}, kFloor));
    CHECK_FALSE(dao::core::Sweep(Ball(10, 10, 2, 0.5F), {0, 0, -4}, kFloor));
    CHECK_FALSE(dao::core::Sweep(Ball(1, 1, 2, 0.5F), {0, 0, 1}, kFloor)); // away
    // Touching exactly at the move's end counts.
    const auto atEnd = dao::core::Sweep(Ball(1, 1, 1.5F, 0.5F), {0, 0, -1}, kFloor);
    REQUIRE(atEnd);
    CHECK(Near(atEnd->t, 1));
    // The same over a hundred metres, where a step short of the end is a visible distance from touching.
    const auto farEnd = dao::core::Sweep(Ball(1, 1, 100.5F, 0.5F), {0, 0, -100}, kFloor);
    REQUIRE(farEnd);
    CHECK(Near(farEnd->t, 1));
}

TEST_CASE("a pass at exactly the radius touches and one just beyond does not", "[sweep]")
{
    // Along x at 0.5 above the face, from x = -2: the sphere first touches at the corner (0, 1, 0) of the face's
    // edge, a third of the way to x = 4.
    const auto graze = dao::core::Sweep(Ball(-2, 1, 0.5F, 0.5F), {6, 0, 0}, kFloor);
    REQUIRE(graze);
    CHECK(Near(graze->t, 1.0F / 3.0F));
    CHECK_FALSE(graze->startsInside);
    CHECK_FALSE(dao::core::Sweep(Ball(-2, 1, 0.5F, 0.499F), {6, 0, 0}, kFloor));
}

TEST_CASE("a capsule that touches at the start is reported at t = 0, with a normal from the shape", "[sweep]")
{
    const auto near = dao::core::Sweep(Ball(1, 1, 0.3F, 0.5F), {0, 0, -1}, kFloor);
    REQUIRE(near);
    CHECK(near->t == 0);
    CHECK(near->startsInside);
    CHECK(Near(near->point, {1, 1, 0}));
    CHECK(Near(near->normal, {0, 0, 1}));
    // Through the face: no distance to give a side, so against the move, or up with none.
    const auto through = dao::core::Sweep(Upright(1, 1, -1, 1, 0.1F), {0, 0, 1}, kFloor);
    REQUIRE(through);
    CHECK(through->t == 0);
    CHECK(through->startsInside);
    CHECK(Near(through->normal, {0, 0, -1}));
    const auto still = dao::core::Sweep(Upright(1, 1, -1, 1, 0.1F), {0, 0, 0}, kFloor);
    REQUIRE(still);
    CHECK(Near(still->normal, {0, 0, 1}));
}

TEST_CASE("a capsule swept at a box touches its face, edge and corner at the analytic t", "[sweep]")
{
    const Point half{1, 1, 1};
    // The face x = 1: the centre stops at x = 1.5, 1.5 of 4 along.
    const auto face = dao::core::Sweep(Ball(3, 0, 0, 0.5F), {-4, 0, 0}, half);
    REQUIRE(face);
    CHECK(Near(face->t, 0.375F));
    CHECK(Near(face->point, {1, 0, 0}));
    CHECK(Near(face->normal, {1, 0, 0}));
    // The edge along z at (1, 1): the centre stops 0.5 off it along the diagonal.
    const float diagonal = 0.5F / std::numbers::sqrt2_v<float>;
    const auto edge = dao::core::Sweep(Ball(3, 3, 0, 0.5F), {-4, -4, 0}, half);
    REQUIRE(edge);
    CHECK(Near(edge->t, (2 - diagonal) / 4));
    CHECK(Near(edge->point, {1, 1, 0}));
    CHECK(Near(edge->normal, {(std::numbers::sqrt2_v<float> * 0.5F), (std::numbers::sqrt2_v<float> * 0.5F), 0}));
    // The corner (1, 1, 1).
    const float third = 0.5F * std::numbers::inv_sqrt3_v<float>;
    const auto corner = dao::core::Sweep(Ball(3, 3, 3, 0.5F), {-4, -4, -4}, half);
    REQUIRE(corner);
    CHECK(Near(corner->t, (2 - third) / 4));
    CHECK(Near(corner->point, {1, 1, 1}));
    CHECK(Near(corner->normal,
               {std::numbers::inv_sqrt3_v<float>, std::numbers::inv_sqrt3_v<float>, std::numbers::inv_sqrt3_v<float>}));
    // A standing capsule, its segment along the face.
    const auto standing = dao::core::Sweep(Upright(3, 0, -0.5F, 0.5F, 0.25F), {-4, 0, 0}, half);
    REQUIRE(standing);
    CHECK(Near(standing->t, 0.4375F));
    CHECK(Near(standing->normal, {1, 0, 0}));
    // Past the box, and starting inside it.
    CHECK_FALSE(dao::core::Sweep(Ball(3, 2, 0, 0.5F), {-4, 0, 0}, half));
    const auto inside = dao::core::Sweep(Ball(0, 0, 0, 0.1F), {5, 0, 0}, half);
    REQUIRE(inside);
    CHECK(inside->t == 0);
    CHECK(inside->startsInside);
    CHECK(Near(inside->normal, {-1, 0, 0}));
}

TEST_CASE("a capsule swept at another capsule or a sphere touches at the sum of their radii", "[sweep]")
{
    const Capsule post = Upright(0, 0, 0, 2, 0.5F);
    // Side by side, 1 apart when they touch.
    const auto side = dao::core::Sweep(Upright(3, 0, 0, 2, 0.5F), {-3, 0, 0}, post);
    REQUIRE(side);
    CHECK(Near(side->t, 2.0F / 3.0F));
    CHECK(Near(side->normal, {1, 0, 0}));
    CHECK(Near(side->point[0], 0.5F)); // on the post's surface, not its axis
    CHECK(Near(side->point[1], 0));
    // A ball falling on the post's top end.
    const auto top = dao::core::Sweep(Ball(0, 0, 5, 0.5F), {0, 0, -5}, post);
    REQUIRE(top);
    CHECK(Near(top->t, 0.4F));
    CHECK(Near(top->point, {0, 0, 2.5F}));
    CHECK(Near(top->normal, {0, 0, 1}));
    CHECK_FALSE(dao::core::Sweep(Upright(3, 0, 0, 2, 0.5F), {-1, 0, 0}, post));
    // Already within the radii.
    const auto start = dao::core::Sweep(Upright(0.9F, 0, 0, 2, 0.5F), {1, 0, 0}, post);
    REQUIRE(start);
    CHECK(start->t == 0);
    CHECK(start->startsInside);
    CHECK(Near(start->normal, {1, 0, 0}));
}

TEST_CASE("a tree of triangles sweeps to the earliest touch of any of them", "[sweep]")
{
    // The triangles of the overlap test's rule: small ones through a 40 m cube.
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
    int missed = 0;
    for (int i = 0; i < 200; ++i)
    {
        // Every third starts 1 to 2 beside a triangle's corner and moves toward and through it, the rest elsewhere.
        const Point near = triangles[static_cast<std::size_t>(i)].a;
        const float off = 1.0F + (static_cast<float>(i % 5) * 0.25F);
        const Capsule capsule = i % 3 == 0 ? Upright(near[0] + off, near[1], near[2] - 0.5F, near[2] + 0.5F, 0.2F)
                                           : Upright(at(i, 11), at(i, 17), at(i, 23), at(i, 23) + 0.5F, 0.2F);
        const Point motion = i % 3 == 0
                                 ? Point{-2 * off, 0.1F, 0}
                                 : Point{static_cast<float>((i % 5) - 2) * 1.5F, static_cast<float>((i % 3) - 1) * 1.5F,
                                         static_cast<float>(i % 4) - 1.5F};
        std::optional<SweepHit> earliest;
        for (const Triangle &t : triangles)
        {
            if (const auto hit = dao::core::Sweep(capsule, motion, t); hit && (!earliest || hit->t < earliest->t))
            {
                earliest = hit;
            }
        }
        const auto fromTree = tree.Sweep(capsule, motion);
        REQUIRE(fromTree.has_value() == earliest.has_value());
        if (earliest)
        {
            ++touched;
            CHECK(fromTree->t == earliest->t);
            CHECK(fromTree->startsInside == earliest->startsInside);
            // The capsule at the touch is within a hair of the triangles: it overlaps them at a radius a hair more.
            const Point step{motion[0] * fromTree->t, motion[1] * fromTree->t, motion[2] * fromTree->t};
            const Capsule there{{capsule.p0[0] + step[0], capsule.p0[1] + step[1], capsule.p0[2] + step[2]},
                                {capsule.p1[0] + step[0], capsule.p1[1] + step[1], capsule.p1[2] + step[2]},
                                capsule.radius + 1e-3F};
            CHECK(tree.Overlaps(there));
        }
        else
        {
            ++missed;
        }
    }
    // Some sweeps touch and some do not, so both answers are tried.
    CHECK(touched > 0);
    CHECK(missed > 0);
    CHECK_FALSE(TriangleTree(std::vector<Triangle>{}).Sweep(Ball(0, 0, 0, 1), {1, 1, 1}));
}

TEST_CASE("a box turned by a pose is swept in its own space and the hit comes back in the level's", "[sweep]")
{
    // A rod 4 long along its own x, 0.2 thick, turned 90 degrees about z at (5, 5): it runs along y.
    const float s = 0.70710678F;
    const collision::Pose turned = collision::PoseOf({0, 0, s, s}, {5, 5, 0});
    const Point half{2, 0.1F, 1};
    const auto hit = collision::Sweep(Ball(8, 5, 0, 0.5F), {-6, 0, 0}, collision::Kind::Box, half, 0, 0, turned);
    REQUIRE(hit);
    CHECK(Near(hit->t, 0.4F));
    CHECK(Near(hit->point, {5.1F, 5, 0}));
    CHECK(Near(hit->normal, {1, 0, 0}));
    // Along the rod's length it is not in the way; along its width it is.
    CHECK_FALSE(collision::Sweep(Ball(8, 8, 0, 0.5F), {-6, 0, 0}, collision::Kind::Box, half, 0, 0, turned));
    const auto along = collision::Sweep(Ball(5, 9, 0, 0.5F), {0, -6, 0}, collision::Kind::Box, half, 0, 0, turned);
    REQUIRE(along);
    CHECK(Near(along->t, 1.5F / 6));
    CHECK(Near(along->normal, {0, 1, 0}));
    // A sphere shape and a capsule shape laid along x, and a mesh kind that has no sweep of its own.
    const collision::Pose laid = collision::PoseOf({0, 0, -s, s}, {5, 5, 0});
    const auto sphere = collision::Sweep(Ball(5, 9, 0, 0.5F), {0, -6, 0}, collision::Kind::Sphere, {}, 1, 0, laid);
    REQUIRE(sphere);
    CHECK(Near(sphere->t, 2.5F / 6));
    const auto capsule =
        collision::Sweep(Ball(4, 9, 0, 0.5F), {0, -6, 0}, collision::Kind::Capsule, {}, 0.25F, 2, laid);
    REQUIRE(capsule);
    CHECK(Near(capsule->t, 3.25F / 6));
    CHECK_FALSE(collision::Sweep(Ball(5, 9, 0, 0.5F), {0, -6, 0}, collision::Kind::Mesh, {}, 0, 0, laid));
}

TEST_CASE("the engine's swept shape is a capsule 0.1 m tall from the start point", "[sweep]")
{
    const Capsule shape = collision::SweptCapsule({1, 2, 3}, 0.25F);
    CHECK(shape.p0 == Point{1, 2, 3});
    CHECK(Near(shape.p1, {1, 2, 3.1F}));
    CHECK(shape.radius == 0.25F);
}

TEST_CASE("a level's shapes are swept by their masks, one hit a shape, a mesh at its earliest triangle", "[sweep]")
{
    collision::StaticPhysics physics;
    collision::TriangleMesh floor;
    floor.points = {{-10, -10, 0}, {10, -10, 0}, {10, 10, 0}, {-10, 10, 0}};
    floor.triangles = {{0, 1, 2}, {0, 2, 3}};
    physics.meshes.push_back(floor);
    collision::Actor ground;
    ground.room = 7;
    collision::StaticShape mesh;
    mesh.kind = collision::Kind::Mesh;
    mesh.mesh = 0;
    mesh.groupsMask = {0x82002020, 0, 0, 0};
    ground.shapes.push_back(mesh);
    ground.globalPose.translation = {0, 0, 1}; // the floor at z 1
    physics.actors.push_back(ground);
    collision::Actor wall;
    wall.room = 9;
    collision::StaticShape box;
    box.kind = collision::Kind::Box;
    box.dimensions = {0.5F, 5, 2};
    box.groupsMask = {0x80010000, 0, 0, 0}; // a terrain wall alone
    box.localPose.translation = {3, 0, 2};
    wall.shapes.push_back(box);
    physics.actors.push_back(wall);

    // Down onto the floor: the cap's lowest centre z = 3 meets z = 1 + 0.25 at 1.75 of 3.
    const auto down =
        collision::SweepAll(physics, collision::SweptCapsule({0, 0, 3}, 0.25F), {0, 0, -3}, collision::kPlacementMask);
    REQUIRE(down.size() == 1);
    CHECK(Near(down[0].hit.t, 1.75F / 3));
    CHECK(Near(down[0].hit.normal, {0, 0, 1}));
    CHECK(down[0].bits0 == 0x82002020);
    CHECK(down[0].actor == 0);
    CHECK(down[0].room == 7);
    CHECK(down[0].shape == 0);
    // Across to the wall's face at x = 2.5.
    const auto across =
        collision::SweepAll(physics, collision::SweptCapsule({0, 0, 2}, 0.25F), {6, 0, 0}, collision::kPlacementMask);
    REQUIRE(across.size() == 1);
    CHECK(Near(across[0].hit.t, 0.375F));
    CHECK(Near(across[0].hit.normal, {-1, 0, 0}));
    CHECK(across[0].bits0 == 0x80010000);
    CHECK(across[0].actor == 1);
    CHECK(across[0].room == 9);
    // Both, in the actors' order and not sorted by t; and each alone by the mask.
    const Capsule swept = collision::SweptCapsule({0, 0, 2}, 0.25F);
    const auto both = collision::SweepAll(physics, swept, {6, 0, -1.5F}, collision::kPlacementMask);
    REQUIRE(both.size() == 2);
    CHECK(both[0].actor == 0);
    CHECK(Near(both[0].hit.t, 0.5F));
    CHECK(both[1].actor == 1);
    CHECK(Near(both[1].hit.t, 0.375F));
    CHECK(collision::SweepAll(physics, swept, {6, 0, -1.5F}, collision::kRoomMask).size() == 1);
    CHECK(collision::SweepAll(physics, swept, {6, 0, -1.5F}, 0x10000).size() == 1);
    CHECK(collision::SweepAll(physics, swept, {6, 0, -1.5F}, collision::kItems).empty());
    // A mesh shape whose mesh is not there meets nothing.
    physics.meshes.clear();
    CHECK(collision::SweepAll(physics, swept, {6, 0, -1.5F}, collision::kPlacementMask).size() == 1);
}

TEST_CASE("a turned mesh shape is swept in its own space and the hit comes back in the level's", "[sweep]")
{
    // A square in the mesh's xy plane, turned 90 degrees about x so that it stands in the level's xz plane at y = 0,
    // raised 1.
    collision::StaticPhysics physics;
    collision::TriangleMesh square;
    square.points = {{-10, -10, 0}, {10, -10, 0}, {10, 10, 0}, {-10, 10, 0}};
    square.triangles = {{0, 1, 2}, {0, 2, 3}};
    physics.meshes.push_back(square);
    collision::Actor actor;
    actor.globalPose = collision::PoseOf(
        {(std::numbers::sqrt2_v<float> * 0.5F), 0, 0, (std::numbers::sqrt2_v<float> * 0.5F)}, {0, 0, 1});
    collision::StaticShape mesh;
    mesh.kind = collision::Kind::Mesh;
    mesh.mesh = 0;
    mesh.groupsMask = {0x20, 0, 0, 0};
    actor.shapes.push_back(mesh);
    physics.actors.push_back(actor);
    const auto hits = collision::SweepAll(physics, Ball(0, 5, 1, 0.5F), {0, -5, 0}, collision::kStaticGeometry);
    REQUIRE(hits.size() == 1);
    CHECK(Near(hits[0].hit.t, 0.9F));
    CHECK(Near(hits[0].hit.point, {0, 0, 1}));
    CHECK(Near(hits[0].hit.normal, {0, 1, 0}));
}

TEST_CASE("a placeable's shapes are swept where it stands, turned, by their masks", "[sweep]")
{
    collision::Shape crate;
    crate.kind = collision::Kind::Box;
    crate.dimensions = {0.5F, 0.5F, 0.5F};
    crate.position = {1, 0, 0.5F};
    crate.mask = 0x81000004;
    collision::Shape trigger = crate;
    trigger.mask = 0x8;
    trigger.position = {-1, 0, 0.5F};
    const std::vector<collision::Shape> shapes{crate, trigger};
    // At (10, 10) turned 90 degrees about z: the crate is at (10, 11), the trigger at (10, 9).
    const float s = 0.70710678F;
    const collision::Pose placed = collision::PoseOf({0, 0, s, s}, {10, 10, 0});
    const Capsule swept = collision::SweptCapsule({10, 14, 0.5F}, 0.25F);
    const auto crateHit = collision::SweepAll(shapes, placed, swept, {0, -6, 0}, collision::kPlacementMask);
    REQUIRE(crateHit.size() == 1);
    CHECK(Near(crateHit[0].hit.t, 0.375F)); // the centre stops at y = 11.75
    CHECK(Near(crateHit[0].hit.normal, {0, 1, 0}));
    CHECK(crateHit[0].shape == 0);
    CHECK(crateHit[0].bits0 == 0x81000004);
    CHECK(crateHit[0].actor == -1);
    const auto triggerHit = collision::SweepAll(shapes, placed, swept, {0, -6, 0}, collision::kTriggers);
    REQUIRE(triggerHit.size() == 1);
    CHECK(triggerHit[0].shape == 1);
    CHECK(Near(triggerHit[0].hit.t, 4.25F / 6));
}

TEST_CASE("the camera keeps the nearest or the farthest hit from the start and stands a sphere's radius off it",
          "[sweep]")
{
    auto hitAt = [](Point point, Point normal) {
        collision::ShapeHit h;
        h.hit.point = point;
        h.hit.normal = normal;
        return h;
    };
    const std::vector<collision::ShapeHit> hits{hitAt({0, 0, 5}, {0, 0, 1}), hitAt({0, 0, 2}, {1, 0, 0}),
                                                hitAt({0, 0, 8}, {0, 1, 0})};
    const Point start{0, 0, 0};
    const auto nearest = collision::PickedCentre(hits, start, 0.25F, collision::Pick::Nearest);
    REQUIRE(nearest);
    CHECK(Near(*nearest, {0.25F, 0, 2}));
    const auto farthest = collision::PickedCentre(hits, start, 0.25F, collision::Pick::Farthest);
    REQUIRE(farthest);
    CHECK(Near(*farthest, {0, 0.25F, 8}));
    // A tie keeps the earlier of them.
    const std::vector<collision::ShapeHit> tie{hitAt({0, 0, 3}, {1, 0, 0}), hitAt({0, 3, 0}, {0, 1, 0})};
    CHECK(Near(*collision::PickedCentre(tie, start, 1, collision::Pick::Nearest), {1, 0, 3}));
    CHECK(Near(*collision::PickedCentre(tie, start, 1, collision::Pick::Farthest), {1, 0, 3}));
    // None, or no number.
    CHECK_FALSE(collision::PickedCentre({}, start, 0.25F, collision::Pick::Nearest));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<collision::ShapeHit> bad{hitAt({nan, 0, 1}, {0, 0, 1})};
    CHECK_FALSE(collision::PickedCentre(bad, start, 0.25F, collision::Pick::Nearest));
    const std::vector<collision::ShapeHit> badNormal{hitAt({0, 0, 1}, {0, nan, 1})};
    CHECK_FALSE(collision::PickedCentre(badNormal, start, 0.25F, collision::Pick::Nearest));
}
