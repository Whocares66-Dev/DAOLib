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

// A colour an effect's model is tinted with: red, green, blue and alpha,
// each 0 to 1.
struct Tint
{
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

// Places a vfx_base row at a point in the current area, at `scale`, facing
// `facing` if given, tinted with `tint` if given. Its id, or kInvalidObject
// if there is no area.
//
// The tint, read 2026-10-01 from the effect's set-up (0x007EA550): the
// descriptor's fourth word is a pointer to a colour of four floats, which
// the set-up hands to the model (its slot 69) in place of the row's own
// Tint column (a packed colour, made into the same four floats). So one
// row can be placed in any colour, if its material takes one.
//
// Which do, read 2026-10-01 after play showed one that does not. The
// model's slot 69 (0x00546700) sets the material parameter
// mml_vUserColorMultiplier on the model's meshes. The reticles' material,
// GeneralGui.mat, has that parameter only in its "ReticalTint" semantic
// (the pixel shader ReticleTint.psh, the parameter 1 1 1 1 by default);
// its "Default" semantic has the texture alone. A model's material object
// (.mao, a few lines of text) names the semantic. Four name ReticalTint:
// fx_aoe_radius, fx_aoe_radius_no, fx_cone_radius and fx_aura_circle, the
// aiming ring, its out-of-reach twin and the cone (rows 30017, 30019 and
// 30018). So those take a tint. The selection reticles and the move marker
// (row 30000, move_indicator.mao) name Default and ignore it: in play, the
// turn-based mod's marker placed with a red tint was its usual pale gold.
// A material object of the same name in the override folder that names
// ReticalTint would make one of those take a tint, the game's own use of
// it unchanged while no tint is given. Tried in play that evening by the
// turn-based mod, with both of the marker's material objects (move_indicator
// and move_icon) so changed: the marker was still gold, which may be the
// folder not being read for them or the colour not reaching that shader.
//
// The colour is one the game uses itself, on other materials: the effect's
// set-up (0x007EA550) takes the caller's tint or, with none, the row's own
// from the table's Tint column (0xAARRGGBB; 137 of the 993 rows have one,
// the crusts and auras on VFX.mat, whose shaders read the same parameter),
// and hands either to the same call. A tinted ring is not yet seen in
// play; examples/preview places one.
//
// The scale is one number, and not only here: the effect's graphics object
// (GraphicsStructures::Object, its SetScale 0x00545330) keeps a single
// float at its +0x54, sizes its bounds and its pick shape by it, and hands
// three copies of it to the function that composes its transform
// (0x00545E30). So an effect cannot be stretched along one axis without
// detours where that number is spread, and there is little to stretch:
// of the reticles only the party ring (SR_PTY) is a plain quad, with a
// ring for a texture; SR_HLT (30001) is four small arrows of 18 vertices
// each. A line on the ground is a row of small effects, or a model made
// for it.
ObjectId Place(std::int32_t vfx, const Vector &at, float scale = 1.0F,
               const std::optional<Vector> &facing = std::nullopt, const std::optional<Tint> &tint = std::nullopt);

// A ring of `radius` metres on the ground: the engine's aiming ring, or the
// one it shows for a point out of reach, which looked the same in play. In
// a colour if `tint` is given: the rings' material is one that takes it.
inline ObjectId PlaceRing(const Vector &at, float radius, bool inReach = true,
                          const std::optional<Tint> &tint = std::nullopt)
{
    return Place(inReach ? kAreaRing : kAreaRingOutOfReach, at, radius / kRingRadius, std::nullopt, tint);
}

// Moves a placed effect, and turns it if `facing` is given; false if it is
// gone.
bool Move(ObjectId effect, const Vector &at, const std::optional<Vector> &facing = std::nullopt);

// Takes a placed effect away; nothing for one already gone.
void Remove(ObjectId effect);

// Leaves an effect out of the cursor's pick, or puts it back in.
//
// Some rows have a shape the cursor's ray meets. Seen in play, 2026-10-01
// (the turn-based mod): row 30000, "Movement Indicator", the game's own
// move marker, is one of the selection reticles' models, and moved after
// the cursor each frame it took the pick from the ground under it: the
// mode's CursorGround read a point and nothing by turns, and a click there
// would not have been a move. The aiming rings (30017 to 30019) have no
// such shape.
//
// An effect left out is taken from what the cursor's ray met before the
// mode chooses among it (a detour of GameModeExplore::CastCursorRay), so
// the mode's CursorObject, CursorGround and CursorHits, and a click, are
// as they would be without it; dao::world::PickAt leaves it out too. The
// effect still has its shape: dao::world::Raycast meets it. Remove forgets
// it. Seen in play, 2026-10-01 (the turn-based mod, the real mouse): with
// the marker left out and moved under the cursor each frame, the mode's
// ground was there in 80 reads of 80, nine clicks of nine there were
// moves, and over a spider the cursor's object was the spider.
void SetPickable(ObjectId effect, bool pickable);
bool IsPickable(ObjectId effect);

} // namespace dao::vfx
