// Draws in the world with the game's own visual effects. Place any row of the visual effects table
// (vfx_base) at a point, then move it, scale it and remove it. Use it for markers on the ground,
// such as the ring the game draws while you aim an area ability.
//
//   const ObjectId ring = dao::vfx::PlaceRing(point, 4.0F);     // 4 m radius
//   dao::vfx::Move(ring, elsewhere);
//   dao::vfx::Remove(ring);
//
// Call every function on the game thread. An effect stays until it is removed or its area is
// unloaded, so remove the effects you place.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace dao::vfx
{

// The vfx_base rows that explore mode uses for aiming.
inline constexpr std::int32_t kAreaRing = 30017;           // "AOE Selection", fxg_aoe_p
inline constexpr std::int32_t kConeArea = 30018;           // "Cone Selection", fxg_cone_p
inline constexpr std::int32_t kAreaRingOutOfReach = 30019; // "AOE No Selection", fxg_aoe_no_p; looks like 30017
// The radius of the aiming rings at scale 1, in metres.
inline constexpr float kRingRadius = 10.0F;

// A colour to tint an effect's model with: red, green, blue and alpha, each from 0 to 1.
struct Tint
{
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

// Places a vfx_base row at a point in the current area.
// @param vfx the vfx_base row
// @param at where to place it, in world coordinates
// @param scale its size relative to the row's own, the same on all three axes
// @param facing the direction it faces; the row's default if not given
// @param tint a colour to use in place of the row's own Tint column, if given
// @return the effect's id, or kInvalidObject if no area is loaded
//
// So far a tint has not visibly changed any effect's colour in the game, the aiming rings
// included.
ObjectId Place(std::int32_t vfx, const Vector &at, float scale = 1.0F,
               const std::optional<Vector> &facing = std::nullopt, const std::optional<Tint> &tint = std::nullopt);

// Places the game's aiming ring on the ground.
// @param radius the ring's radius in metres
// @param inReach false for the out-of-reach ring, which looks the same in the game
// @return the effect's id, or kInvalidObject if no area is loaded
inline ObjectId PlaceRing(const Vector &at, float radius, bool inReach = true)
{
    return Place(inReach ? kAreaRing : kAreaRingOutOfReach, at, radius / kRingRadius);
}

// Moves a placed effect, and turns it to `facing` if that is given.
// @return false if the effect no longer exists
bool Move(ObjectId effect, const Vector &at, const std::optional<Vector> &facing = std::nullopt);

// Removes a placed effect and clears its SetPickable setting. Does nothing if the effect is
// already gone.
void Remove(ObjectId effect);

// Changes a placed effect's scale without removing and placing it again.
// @return false if the effect no longer exists
bool SetScale(ObjectId effect, float scale);

// Changes a placed effect's tint without removing and placing it again.
// @return false if the effect no longer exists
//
// As with Place, a tint has so far not visibly changed an effect's colour in the game.
bool SetTint(ObjectId effect, const Tint &tint);

// Excludes an effect from the cursor's pick, which decides what is under the cursor, or includes
// it again.
// @param pickable false to exclude it, true to include it again
//
// Some rows, such as the move marker (30000), have a shape the cursor's ray hits, so a marker
// moved under the cursor would be picked instead of the ground. An excluded effect is ignored by
// the explore-mode cursor, by clicks, and by dao::world::PickAt, but dao::world::Raycast still
// hits it.
void SetPickable(ObjectId effect, bool pickable);

// Returns false if SetPickable excluded the effect from the cursor's pick, otherwise true.
bool IsPickable(ObjectId effect);

} // namespace dao::vfx
