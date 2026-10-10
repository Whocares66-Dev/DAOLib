// The game's visual effects placed, moved and removed (dao/Vfx.h).
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
#include "dao/Vfx.h"

#include "dao/Addresses.h"
#include "dao/GameModes.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/World.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace dao::vfx
{

namespace
{

struct alignas(16) Vector4
{
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 0;
};

Vector4 Padded(const Vector &v)
{
    return {v.x, v.y, v.z, 0.0F};
}

// What Place is given (0x00711320 builds one on the stack).
struct Descriptor
{
    std::int32_t vfx;
    ObjectId area;
    float scale;
    const Tint *tint; // null for the row's own
};
static_assert(sizeof(Descriptor) == 16);
static_assert(sizeof(Tint) == 16, "the engine's colour is four floats");

void *Manager()
{
    using Fn = void *(*)();
    return reinterpret_cast<Fn>(Module::Address(addr::kVfxManager_Get))();
}

// The placed effect's object, or null when it is gone.
void *Find(ObjectId effect)
{
    using Fn = void *(__fastcall *)(void *, int, ObjectId);
    return reinterpret_cast<Fn>(Module::Address(addr::kVfxManager_Find))(Manager(), 0, effect);
}

constexpr std::size_t kSetOrientation = 112; // the effect object's vtable slot

void Turn(void *object, const Vector4 &facing)
{
    using Fn = void(__fastcall *)(void *, int, const Vector4 *);
    const Fn *vtable = *static_cast<const Fn *const *>(object);
    vtable[kSetOrientation](object, 0, &facing);
}

} // namespace

// The tint, read 2026-10-01 from the effect's set-up (0x007EA550): the
// descriptor's fourth word is a pointer to a colour of four floats, which
// the set-up hands to the model (its slot 69) in place of the row's own
// Tint column (a packed colour, made into the same four floats). So one
// row might be placed in any colour. In play it has not been: see below
// before relying on it.
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
// it unchanged while no tint is given.
//
// In play that evening neither showed. The turn-based mod's marker, with
// both of its material objects (move_indicator and move_icon) so changed,
// was still gold. And the aiming ring itself, placed with a red tint by
// examples/preview, was its own pale gold: the play tester's bright
// pixels had red, green and blue within a few points of each other.
//
// Read after that. The shader is not the trouble: ReticleTint.psh is the
// texture times aur_vExternalColor times mml_vUserColorMultiplier. Nor is
// the kind of node. Slot 69 keeps the colour in the model (+0x2CC) and
// goes through the model's drawable nodes (the list at +0x5C); to each
// that is a mesh, a sprite emitter, an emitter of meshes or a weapon
// trail (kinds 1, 6, 7 and 0x13 to 0x15) and has a material instance
// (+0x150), it gives a value for the parameter that points at that colour
// (0x004F3F40).
//
// Nor is it the moment. A second play test set the tint after placing,
// every other frame (SetTint): the ring's model had its one part from the
// frame it was placed, the call answered each time, and the ring was pale
// gold still. So a caller's tint has not been seen to colour anything,
// and a reticle is not to be had in another colour this way.
//
// What the tint may be for. The game's auras (fxa_aura_p: rows 93052 and
// 93054 are one model, blue and orange by their Tint) are built as the
// aiming ring is, a plane on the same material and semantic, with an
// emitter on VFX.mat beside it; an emitter sets the parameter itself from
// a colour its model keeps (ObjectNodeEmitter's slot 13, at 0x00541473).
// So the Tint column may show only in emitters, and a tint from a caller
// may too. Not tried in play. Not read: what gives a mesh's shader the
// parameter when it is drawn, and what the plane's animated MaterialColor
// (animation target 0x24) writes.
//
// The colour is one the game uses itself: the effect's set-up (0x007EA550)
// takes the caller's tint or, with none, the row's own from the table's
// Tint column (0xAARRGGBB; 137 of the 993 rows have one, crusts and auras
// on VFX.mat, whose shaders read the same parameter), and hands either to
// the same call.
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
ObjectId Place(std::int32_t vfx, const Vector &at, float scale, const std::optional<Vector> &facing,
               const std::optional<Tint> &tint)
{
    const CCArea *area = world::CurrentArea();
    if (area == nullptr)
    {
        return kInvalidObject;
    }
    // The set-up reads the colour during the call (mode 2 below loads the
    // effect now, not by a task that would outlive this), and copies it.
    const Descriptor descriptor{vfx, area->Id(), scale, tint ? &*tint : nullptr};
    const Vector4 where = Padded(at);
    const Vector4 direction = Padded(facing.value_or(Vector{}));
    // Mode 2, as explore mode passes: the effect is loaded now, not by a task.
    constexpr std::int32_t kMode = 2;
    using Fn = ObjectId(__fastcall *)(void *, int, const Descriptor *, ObjectId, std::int32_t, const Vector4 *,
                                      const Vector4 *, std::uint32_t);
    return reinterpret_cast<Fn>(Module::Address(addr::kVfxManager_Place))(
        Manager(), 0, &descriptor, kInvalidObject, kMode, &where, facing ? &direction : nullptr, 0);
}

bool Move(ObjectId effect, const Vector &at, const std::optional<Vector> &facing)
{
    void *object = Find(effect);
    if (object == nullptr)
    {
        return false;
    }
    const Vector4 where = Padded(at);
    using SetPositionFn = void(__fastcall *)(void *, int, const Vector4 *, std::int32_t);
    reinterpret_cast<SetPositionFn>(Module::Address(addr::kCCObject_SetPosition))(object, 0, &where, 0);
    if (facing)
    {
        Turn(object, Padded(*facing));
    }
    return true;
}

// SetScale and SetTint change a placed effect's size or colour where it stands, as its set-up set
// them when it was placed: a mark turned from gold to red without taking it away and placing
// another. SetScale is not yet seen in play. SetTint ran in play without harm and without a colour
// to show for it on the aiming ring (see Place).
bool SetScale(ObjectId effect, float scale)
{
    void *object = Find(effect);
    if (object == nullptr)
    {
        return false;
    }
    using Fn = void(__fastcall *)(void *, int, float);
    reinterpret_cast<Fn>(Module::Address(addr::kVfxObject_SetScale))(object, 0, scale);
    return true;
}

bool SetTint(ObjectId effect, const Tint &tint)
{
    void *object = Find(effect);
    if (object == nullptr)
    {
        return false;
    }
    using Fn = void(__fastcall *)(void *, int, const Tint *);
    reinterpret_cast<Fn>(Module::Address(addr::kVfxObject_SetTint))(object, 0, &tint);
    return true;
}

namespace
{

std::mutex g_mutex;
std::vector<ObjectId> g_unpicked; // under g_mutex

// GameModeExplore::CastCursorRay: __thiscall, no arguments. It fills the
// mode's hits, nearest first, and their count; the mode chooses among them
// after. The effects left out are taken from the hits here.
using CastFn = void(__fastcall *)(void *mode, int edx);
Detour<CastFn> g_cast;

void __fastcall CastCursorRay(void *mode, int edx)
{
    g_cast.Original()(mode, edx);
    std::vector<ObjectId> unpicked;
    {
        const std::lock_guard lock(g_mutex);
        unpicked = g_unpicked;
    }
    if (mode == nullptr || unpicked.empty())
    {
        return;
    }
    auto *bytes = static_cast<std::byte *>(mode);
    auto *hits = reinterpret_cast<world::Hit *>(bytes + GameModeExplore::kHits);
    auto *count = reinterpret_cast<std::int32_t *>(bytes + GameModeExplore::kHitCount);
    const std::int32_t had = std::clamp(*count, 0, GameModeExplore::kMaxHits);
    std::int32_t kept = 0;
    for (std::int32_t i = 0; i < had; ++i)
    {
        if (std::ranges::find(unpicked, hits[i].object) != unpicked.end())
        {
            continue;
        }
        if (kept != i)
        {
            hits[kept] = hits[i];
        }
        ++kept;
    }
    *count = kept;
}

} // namespace

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
void SetPickable(ObjectId effect, bool pickable)
{
    if (effect == kInvalidObject)
    {
        return;
    }
    const std::lock_guard lock(g_mutex);
    const auto found = std::ranges::find(g_unpicked, effect);
    if (pickable)
    {
        if (found != g_unpicked.end())
        {
            g_unpicked.erase(found);
        }
        return;
    }
    if (found == g_unpicked.end())
    {
        g_unpicked.push_back(effect);
    }
    if (g_cast.Original() == nullptr)
    {
        g_cast.Attach(addr::kGameModeExplore_CastCursorRay, &CastCursorRay);
    }
}

bool IsPickable(ObjectId effect)
{
    const std::lock_guard lock(g_mutex);
    return std::ranges::find(g_unpicked, effect) == g_unpicked.end();
}

void Remove(ObjectId effect)
{
    if (effect == kInvalidObject)
    {
        return;
    }
    SetPickable(effect, true);
    using Fn = void(__fastcall *)(void *, int, ObjectId, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kVfxManager_Remove))(Manager(), 0, effect, 0);
}

} // namespace dao::vfx
