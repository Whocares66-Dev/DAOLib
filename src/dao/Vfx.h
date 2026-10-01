// Drawing in the world with the game's own visual effects: a ring on the
// ground, as the engine draws an area being aimed, or any row of the
// visual effects table (vfx_base), placed at a point, scaled, moved and
// taken away.
//
//   const ObjectId ring = dao::vfx::PlaceRing(point, 4.0F);     // 4 m
//   dao::vfx::Move(ring, elsewhere);
//   dao::vfx::Remove(ring);
//
// Read 2026-09-30 from explore mode's aiming (0x00710F80). While an area
// ability is aimed, explore mode keeps one effect's id (0x00C0679C) and
// each frame either moves it to the aimed point or, when the look should
// change, takes it away and places another. It places through the visual
// effect manager (a singleton, 0x0079E820): Place (0x0079EDC0, __thiscall
// (const descriptor *, the id it had or -1, mode 2, const Vector4 *at,
// const Vector4 *facing or null, the object to attach to or 0), `ret
// 0x18`; the descriptor {the effect's row, the area's id, scale, 0}),
// Find (0x0079D780) and Remove (0x0079EAE0, (id, 0), `ret 8`); it moves an
// effect with CCObject::SetPosition (0x00695540) and turns it with its
// slot 112. The rings are vfx_base rows 30017 to 30019, ground decals made
// for a 10 m radius and scaled to the ability's (its radius over 10); the
// row is chosen by 0x0070AB20, 30019 when the point is out of reach. In
// play (2026-09-30, examples/preview) the two looked alike: a pale cream
// ring, near or far. Their Tint (vfx_base) is 0; a colour for "too far" is
// a different row (an area effect of another look, say), not this one.
//
// On the game's thread. An effect placed stays until removed, or until its
// area is unloaded; a plugin removes its own. Seen in play 2026-09-30: a
// ring placed and moved after the cursor, 22 placed and removed in a few
// minutes, none left behind.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstdint>
#include <optional>

namespace dao::vfx
{

// vfx_base's rows for explore mode's aiming.
inline constexpr std::int32_t kAreaRing = 30017;           // "AOE Selection", fxg_aoe_p
inline constexpr std::int32_t kConeArea = 30018;           // "Cone Selection", fxg_cone_p
inline constexpr std::int32_t kAreaRingOutOfReach = 30019; // "AOE No Selection", fxg_aoe_no_p; looks as 30017
// The rings' radius at scale 1.
inline constexpr float kRingRadius = 10.0F;

// Places a vfx_base row at a point in the current area, at `scale`, facing
// `facing` if given. Its id, or kInvalidObject if there is no area.
ObjectId Place(std::int32_t vfx, const Vector &at, float scale = 1.0F,
               const std::optional<Vector> &facing = std::nullopt);

// A ring of `radius` metres on the ground: the engine's aiming ring, or the
// one it shows for a point out of reach, which looked the same in play.
inline ObjectId PlaceRing(const Vector &at, float radius, bool inReach = true)
{
    return Place(inReach ? kAreaRing : kAreaRingOutOfReach, at, radius / kRingRadius);
}

// Moves a placed effect, and turns it if `facing` is given; false if it is
// gone.
bool Move(ObjectId effect, const Vector &at, const std::optional<Vector> &facing = std::nullopt);

// Takes a placed effect away; nothing for one already gone.
void Remove(ObjectId effect);

} // namespace dao::vfx
