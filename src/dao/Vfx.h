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
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
// A tint multiplies the colour of every part of the effect whose material takes a user colour:
// the aiming rings (30017 to 30019), the auras' glyphs and most effects on VFX.mat. Parts drawn
// with the reticles' plain material, the selection reticles and the move marker (30000), keep
// their own colour.
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
// The tint shows on the parts Place says.
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

// One sprite emitter's colour, as the engine is about to draw its particles for a frame.
struct EmitterColor
{
    // What every particle's colour is multiplied by: the emitter's own until a callback sets it.
    Tint color;
    // The emitter's name in its model, or empty; lasts the call alone.
    std::wstring_view name;
    // The name of the model the emitter is part of (fxe_waterfall_redcliffe), or empty; lasts the call alone.
    std::wstring_view model;
    // Whether that model has a point light of its own: a fire or a spell's glow, not a waterfall or a mist.
    bool modelHasLight = false;
    // Where the emitter is in the world.
    Vector position;
    // The emitter's node, the same each frame for the same emitter; for telling emitters apart.
    std::uintptr_t node = 0;
};

// Adds a callback to run each time the engine draws a sprite emitter's particles, once a frame
// for each emitter in view: plain sprites and ribbons, not mesh particles.
// @param callback may change `color`; the rest is for reading
//
// Nothing is kept: the emitter's own colour is put back after the draw, so a callback that stops
// changing it leaves the emitter as vanilla has it. Callbacks run on the engine's worker threads,
// several at once, so keep them short and make what they share safe across threads.
void OnEmitterColor(std::function<void(EmitterColor &)> callback);

// One visual effect the area has running.
struct RunningEffect
{
    // The effect's id, as Place returns one.
    ObjectId id = kInvalidObject;
    // Its row of the visual effects table (vfx_base).
    std::int32_t vfx = 0;
    // The object it is on, or kInvalidObject for one placed at a point.
    ObjectId owner = kInvalidObject;
    // Where it is: its own place, or its owner's for one on an object; none for one on an item,
    // such as a weapon's enchantment, since a held item has no place of its own.
    std::optional<Vector> position;
    // The name of its model (fxe_fire_m_ns_p), or empty while it has none loaded.
    std::wstring model;
    // Whether that model has a point light of its own.
    bool modelHasLight = false;
};

// Returns every visual effect the area has running, off screen as well as in view: those a script,
// an ability or a plugin made, and the effects that are part of its rooms' own models (a fire or a
// torch's flame the level places), which have no id or row of their own: their id and owner are
// kInvalidObject, their vfx 0 and their position the model's place.
//
// Call it on the game thread.
std::vector<RunningEffect> RunningEffects();

// Runs the particles of the scene's fires, torches and other sprite emitters, ribbons among them, again from
// nothing: each one's particles are emptied and simulated for `seconds` from a random seed of its
// own place, so a paused frame shows them the same in every launch. Each emitter is run again in
// the engine's next frame that draws it.
// @param seconds how long to simulate, as the engine's own warm-up does: in steps of 1/15 s
// @return how many such emitters the scene has
//
// Call it on the game thread with the game paused. Emitters of physics or mesh particles are left
// as they are.
int ReplayEmitters(float seconds);

} // namespace dao::vfx
