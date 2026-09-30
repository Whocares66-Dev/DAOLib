// Questions about the world's geometry, as the engine asks them: what a ray
// meets, and whether one point can be seen from another.
//
//   for (const dao::world::Hit &hit : dao::world::Raycast(eye, forward))
//       ...
//   if (dao::world::LineOfSight(archer, target))
//       ...
//
// Read 2026-09-30. An area keeps its physics (Physics::PhysicsScene, over
// PhysX's NxScene) at the +0xCC of the object at its +0x62C (0x00537D60
// reads it). The engine asks that scene two questions:
// - every shape along a ray, to any distance: World::Raycast (0x006497A0,
//   the scene's slot 8, NxScene::raycastAllShapes), which the cursor pick
//   asks 15 times a second (dao/GameModes.h);
// - the nearest shape between two points: the area's LineOfSight
//   (0x0067C3F0, the scene's slot 4, raycastClosestShape over the segment),
//   which the scripts' CheckLineOfSightObject and CheckLineOfSightVector
//   ask, and which lets doors alone among placeables block the view.
// Each takes a mask of collision groups, and a shape is met when its own
// groups (PhysX's NxGroupsMask, first word) share a bit with the mask.
//
// On the game's thread only, where frame callbacks, events and the hooks
// on script commands run: the raycast gathers its hits in a static vector
// (0x00C347B0) that the cursor pick shares. Called on another thread once
// a frame has run (dao::FrameThreadId), each answers nothing: no hits, no
// sight.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace dao
{
class CCArea;
}

namespace dao::world
{

// Collision groups: a shape's, and a query's mask. Read 2026-09-30 from
// the code that tests them; which bit is which kind of object or ground is
// not yet told apart beyond what is named.
namespace groups
{
// A hit with any of these and an object id is an object; with none of
// them and any of kGround, the ground (0x00648B70).
inline constexpr std::uint32_t kObjects = 0x1 | 0x2 | 0x4 | 0x20000;
inline constexpr std::uint32_t kGround = 0x8 | 0x20;
// Placeables: LineOfSight looks a hit with this up as one.
inline constexpr std::uint32_t kPlaceables = 0x4;
// The generous volume an object is picked by, beside its body; Raycast's
// `selectionVolumes` adds it to the mask.
inline constexpr std::uint32_t kSelection = 0x20000;
// Set on ground that cannot be walked: explore mode refuses a move there.
inline constexpr std::uint32_t kNotWalkable = 0x01000000;

// The engine's own masks.
inline constexpr std::uint32_t kCursor = 0x2607; // the cursor pick, 0x0070B590
inline constexpr std::uint32_t kSight = 0x24;    // line of sight: placeables and 0x20
} // namespace groups

// One shape a ray met, as the engine keeps it: 0x30 bytes, read 2026-09-30
// from its constructor (0x00648B10) and World::Raycast, which fills each
// from PhysX's hit (the physics' own, 0x00829900). Explore mode keeps the
// cursor's 32 of these (dao/GameModes.h).
struct Hit
{
    enum class Kind : std::int32_t
    {
        Nothing = 0, // met a shape with neither kind of group
        Ground = 1,  // the area's ground: walkmesh, terrain
        Object = 2,  // a creature, placeable or other object; `object` is its id
    };

    Vector position;
    std::uint32_t padding0 = 0;
    Kind kind = Kind::Nothing;
    ObjectId object = kInvalidObject;
    // groups::kNotWalkable clear: explore mode's walkable test requires it
    // of a ground hit.
    bool walkable = false;
    std::uint32_t unknown1c = 0;
    float unknown20 = 0; // from the shape's owner (its slot 18); not read yet
    std::uint32_t groups = 0;
    std::uint32_t unknown28 = 0;
    std::uint32_t padding2c = 0;

    bool IsGround() const
    {
        return kind == Kind::Ground;
    }
    // The object met, if it was one.
    std::optional<ObjectId> Object() const
    {
        return kind == Kind::Object ? std::optional<ObjectId>(object) : std::nullopt;
    }
};
static_assert(sizeof(Hit) == 0x30);

// How far above a creature's position the scripts' line of sight looks
// from and to (CheckLineOfSightObject, 0x00668DB0).
inline constexpr float kEyeHeight = 1.8F;

// The area the player is in: the module's current area (CCModule's
// 0x005FE470), or the controlled creature's. Null before a game is loaded.
CCArea *CurrentArea();

// Every shape in the current area along a ray from `origin` in
// `direction`, to any distance, nearest first; at most 32, as the engine
// gathers them. `mask` chooses the groups met. With `selectionVolumes`,
// objects' pick volumes are met too, as the cursor meets them, and an
// object may then appear twice (its body and its volume); without, once.
std::vector<Hit> Raycast(const Vector &origin, const Vector &direction, std::uint32_t mask = groups::kCursor,
                         bool selectionVolumes = false);

// The nearest shape between two points, if any: Raycast, cut at `to`.
std::optional<Hit> FirstHit(const Vector &from, const Vector &to, std::uint32_t mask = groups::kSight);

// Whether `to` can be seen from `from`, as the engine judges it: the
// nearest shape between them among groups::kSight blocks, except a
// placeable that is not a door or of base type 8 (0x0067A670 reads
// GetPlaceableBaseType), and `ignore`. Only the nearest is judged, so a
// chest in front of a wall leaves the view clear, as the scripts see it.
// Converts to bool, true when the view is clear.
struct Sight
{
    bool clear = false;
    std::optional<Vector> blockedAt; // where the view was cut, when it was

    explicit operator bool() const
    {
        return clear;
    }
};
Sight LineOfSight(const Vector &from, const Vector &to, ObjectId ignore = kInvalidObject);

// Whether `target` can be seen from `viewer`, as CheckLineOfSightObject
// asks it: from kEyeHeight above one's position to kEyeHeight above the
// other's, in the viewer's area; not if they are in different areas.
Sight LineOfSight(ObjectId viewer, ObjectId target);

} // namespace dao::world
