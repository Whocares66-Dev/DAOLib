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
//
// OnEmitterColor (DAOLib #224, read 2026-10-21; dao/Addresses.h at
// kEmitterSprite_Fill) detours each sprite emitter's fill for a frame, the
// emitter's slot +0xAC, and for the call alone puts the callbacks' colour in
// the emitter's live colour multiplier (+0x39C), which the fill multiplies
// every particle's age-map colour by; then puts the emitter's own back. The
// multiplier is animatable: a clip that keys it writes it on the game
// thread, and a write that came during the fill would be lost to the colour
// put back after it, for one frame. Seen in play 2026-10-21 (a crash in a
// trial callback): the fill is called from the emitters' update task
// (GraphicsStructures::EmitterUpdateTask) on the engine's task group's
// threads, so callbacks run on several threads at once.
//
// RunningEffects and EmitterColor's position (DAOLib #231, read
// 2026-10-22). The visual effect manager keeps its effects in a vector of
// pointers at +0x48 (begin) and +0x4C (end), which Find (0x0079D780) and
// Update (0x0079EE20) walk, skipping a null entry and one whose phase
// (+0x444) is 0. Each is a CCVisualEffect, a CCObject: its id at +0x10, its
// place at +0x120, its vfx_base row at +0x44C, the object it is on at
// +0x450 and its crust's target at +0x454. The manager adds to the vector
// only on the game's thread (Place posts a task from any other, 0x0079D360),
// and Update has no test of what is in view, so an effect off screen is
// there too. An effect on an object with a model is a child of the
// object's root node and its own place is not kept up (0x0079D580 copies
// the owner's only for an owner with none), so the owner's is taken. Its
// model: the effect's +0x154 (its +0x70 base's slot 24, 0x0069A000), whose
// +0x18's +4 is the GraphicsStructures::Object's base at +8 (the vftable
// whose slot 43 is SetScale, 0x00545330, and slot 69 SetTint, as
// kVfxObject_SetScale calls them, 0x007E9C40). A room's own effect models
// (its .rml's fxe_*) are scene models and not the manager's. An emitter's
// node keeps its world matrix at +0x60, its translation in the fourth
// column, as a light's position is read (ILight slot 2, 0x0053CA70).
//
// Seen in play 2026-10-22, GOG, Redcliffe by night in the siege: 57 to 71
// effects running, the poison clouds (row 92028, fxa_gas_aoe_p) placed at
// points about the hillside with their area's id at +0x450, the party's
// talents' and the reticles' (fxa_tal_demoral_c, sr_pty) on the party at
// their places, and a weapon's enchantment (row 1069, fxa_lit_swrd_c, with
// its light) on the item. The emitters of the effects on creatures were at
// their owners' x and y, about 1.4 m above. The model's emitter vector
// (+0x64) does not hold the nodes themselves: read as nodes, their matrices
// were garbage.
#include "dao/Vfx.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/GameModes.h"
#include "dao/GraphicsNodes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/Scene.h"
#include "dao/World.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <utility>
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

namespace
{

constexpr std::size_t kColorMultiplier = 0x39C; // in an emitter: four floats, rgba
constexpr std::size_t kNodeWorld = 0x60;        // in a node: its world matrix, 16 floats, row by row

// Where the node is in the world: its matrix's fourth column.
Vector NodePosition(std::uintptr_t node)
{
    const auto *m = reinterpret_cast<const float *>(node + kNodeWorld);
    const float w = m[15] != 0.0F ? m[15] : 1.0F;
    return {m[3] / w, m[7] / w, m[11] / w};
}
static_assert(sizeof(Tint) == 4 * sizeof(float), "a Tint is the multiplier's four floats");

using EmitterCallbacks = std::vector<std::function<void(EmitterColor &)>>;
std::mutex g_emitterMutex;
std::shared_ptr<const EmitterCallbacks> g_emitterCallbacks; // replaced whole, never changed in place

using FillFn = void(__fastcall *)(void *emitter, int edx);
Detour<FillFn> g_spriteFill;
Detour<FillFn> g_ribbonFill;
Detour<FillFn> g_ribbonPhysicsFill;

// Runs the callbacks over the emitter's colour, the fill with what they set, and puts its own back.
void FillWith(const Detour<FillFn> &fill, void *emitter, int edx)
{
    std::shared_ptr<const EmitterCallbacks> callbacks;
    {
        const std::lock_guard lock(g_emitterMutex);
        callbacks = g_emitterCallbacks;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(emitter);
    auto *const multiplier = reinterpret_cast<float *>(address + kColorMultiplier);
    EmitterColor color;
    std::memcpy(&color.color, multiplier, sizeof(Tint));
    const Tint own = color.color;
    color.name = detail::NodeName(address);
    color.model = detail::ModelName(address);
    color.modelHasLight = detail::ModelHasPointLight(address);
    color.position = NodePosition(address);
    color.node = address;
    for (const auto &callback : *callbacks)
    {
        detail::Guard("an emitter colour callback", [&] { callback(color); });
    }
    std::memcpy(multiplier, &color.color, sizeof(Tint));
    fill.Original()(emitter, edx);
    std::memcpy(multiplier, &own, sizeof(Tint));
}

void __fastcall SpriteFill(void *emitter, int edx)
{
    FillWith(g_spriteFill, emitter, edx);
}

void __fastcall RibbonFill(void *emitter, int edx)
{
    FillWith(g_ribbonFill, emitter, edx);
}

void __fastcall RibbonPhysicsFill(void *emitter, int edx)
{
    FillWith(g_ribbonPhysicsFill, emitter, edx);
}

} // namespace

void OnEmitterColor(std::function<void(EmitterColor &)> callback)
{
    const std::lock_guard lock(g_emitterMutex);
    auto callbacks = g_emitterCallbacks ? std::make_shared<EmitterCallbacks>(*g_emitterCallbacks)
                                        : std::make_shared<EmitterCallbacks>();
    callbacks->push_back(std::move(callback));
    g_emitterCallbacks = std::move(callbacks);
    if (!g_spriteFill.IsAttached() && Module::IsKnownExecutable())
    {
        g_spriteFill.Attach(addr::kEmitterSprite_Fill, &SpriteFill);
        g_ribbonFill.Attach(addr::kEmitterRibbon_Fill, &RibbonFill);
        g_ribbonPhysicsFill.Attach(addr::kEmitterRibbonPhysics_Fill, &RibbonPhysicsFill);
    }
}

namespace
{

constexpr std::size_t kEffects = 0x48; // in the manager: begin and end of its effects
constexpr std::size_t kPhase = 0x444;  // in an effect: 0 when it is gone
constexpr std::size_t kRow = 0x44C;    // in an effect: its vfx_base row
constexpr std::size_t kOwner = 0x450;  // in an effect: the object it is on

RunningEffect Running(std::uintptr_t address)
{
    const auto *effect = reinterpret_cast<const CCObject *>(address);
    RunningEffect running;
    running.id = effect->Id();
    running.vfx = *reinterpret_cast<const std::int32_t *>(address + kRow);
    running.position = effect->Position();
    // A located effect's +0x450 is its area's id, which names no object here.
    const ObjectId on = *reinterpret_cast<const ObjectId *>(address + kOwner);
    if (CCObject *owner = on != kInvalidObject ? CClientExoApp::Get()->GetObjectById(on) : nullptr)
    {
        running.owner = on;
        running.position = owner->AsItem() == nullptr ? std::optional(owner->Position()) : std::nullopt;
    }
    const std::uintptr_t object = detail::GraphicsObjectOf(address);
    running.model = detail::ObjectModelName(object);
    running.modelHasLight = detail::ObjectHasPointLight(object);
    return running;
}

} // namespace

std::vector<RunningEffect> RunningEffects()
{
    std::vector<RunningEffect> effects;
    if (!Module::IsKnownExecutable() || CClientExoApp::Get() == nullptr)
    {
        return effects;
    }
    const auto manager = reinterpret_cast<std::uintptr_t>(Manager());
    if (manager == 0)
    {
        return effects;
    }
    const auto *begin = reinterpret_cast<const std::uintptr_t *>(detail::Field(manager + kEffects));
    const auto *end = reinterpret_cast<const std::uintptr_t *>(detail::Field(manager + kEffects + 4));
    for (const auto *entry = begin; begin != nullptr && entry < end; ++entry)
    {
        if (*entry != 0 && detail::Field(*entry + kPhase) != 0)
        {
            effects.push_back(Running(*entry));
        }
    }
    // The rooms' own models with an emitter (DAOLib #242), one an object, from the scene.
    std::uintptr_t last = 0;
    scene::ForEach(scene::Kind::Emitter, [&](const scene::Object &object, const scene::Node &) {
        if (!object.room || object.address == last)
        {
            return;
        }
        last = object.address;
        RunningEffect effect;
        effect.position = object.place;
        effect.model = std::wstring(object.model);
        effect.modelHasLight = detail::ObjectHasPointLight(object.address);
        effects.push_back(std::move(effect));
    });
    return effects;
}

// Read 2026-10-23 and 2026-10-30 for DAOLib #131 (dev/EFFECTS.md, "How an emitter spawns and
// moves its particles", "The emitter presimulation" and "Ribbons").
//
// The update runs inside the engine's frame, not on the game thread: the renderer's collect of a
// drawn object (0x0046DE10) walks its emitters, gives each the frame's context at +0x4E4, which
// the fill takes its vertex buffer from, and queues the emitter's own task (+0x4E8, an
// EmitterUpdateTask, whose run 0x0053E340 is Update(false, -1)) for the engine's task group's
// threads. The update, slot 34, __thiscall (bool presimulate, float seconds), `ret 8`, the same
// for a plain sprite and a ribbon, runs a presimulation in steps of 0.0666 s when its first
// argument is set and builds no vertices then; a normal update ends in the vertex fill (slot 43)
// whether it stepped the particles or not. Called from the game thread the fill crashed, writing
// 0x3EB0 (2026-10-23): no frame, no context. While paused the tasks go on with no time passed:
// in play, Denerim market 2026-10-30, 6,100 sprite fills a second, paused or not. So the replay
// is done in an emitter's own task, the next time it is updated: the emitter emptied, the
// presimulation, then the frame's own update, which fills the vertices.
//
// What a replay must hold still, each found by a pair of launches that still differed:
// - The random draws. They come from four linear congruential generators of the engine's
//   (dao/Addresses.h, kParticleRandomSeed), shared by every emitter: a particle's birth, the
//   sprite's spawn, the movement spread, a ribbon's own. The tasks run on several threads at
//   once, so each replay holds a lock and seeds all four from the emitter's own place first:
//   what it draws does not hang on the order the threads take them in.
// - The spawn count's carry, the part of a particle not yet spawned (+0x398; the update clears
//   it itself when it skips an emitter).
// - The frame it is done in: an emitter updated only while seen is skipped by the update until
//   its object has been seen (Held), and a presimulation then makes nothing.
// - A ribbon's ring, UV counter and start frame: the torch's flame in the market is one
//   (fxe_fire_small_p's Flame, an ObjectNodeEmitterSpriteLinked), and its ring keeps a dead
//   particle's angle for the next born in its slot.
// Physics and mesh particles hold engine objects, so those emitters are left as they are.
namespace
{

constexpr std::size_t kParticles = 0x500; // in a sprite emitter: its particles, 0x80 bytes each
constexpr std::size_t kParticleSize = 0x80;
constexpr std::size_t kRibbonSlots = 0x101; // a ribbon's ring of particles, made once with the emitter
constexpr std::size_t kRibbonUv = 0x528;    // a ribbon's UV counter, -1 as made
constexpr std::size_t kRibbonFrame = 0x52C; // and its flipbook's start frame
constexpr std::size_t kSpawnCarry = 0x398;  // the part of a particle the spawn count carries to the next update
constexpr std::size_t kBlueprint = 0x310;   // in an emitter: its blueprint
constexpr std::size_t kClearSlot = 41;      // an emitter's own clear of its particles

using SpriteUpdateFn = void(__fastcall *)(std::uintptr_t emitter, int edx, std::int32_t presimulate, float seconds);
Detour<SpriteUpdateFn> g_spriteUpdate;

std::mutex g_replayMutex;
std::uint32_t g_replayAsked = 0;                              // the last replay asked for; 0 none
float g_replaySeconds = 0;                                    // its time
std::unordered_map<std::uintptr_t, std::uint32_t> g_replayed; // each emitter's last replay done

enum class Kind : std::uint8_t
{
    Sprite,
    Ribbon,
    Other
};

// A plain sprite or a ribbon of SpriteParticle, by its vftable.
Kind KindOf(std::uintptr_t emitter)
{
    const auto vftable = *reinterpret_cast<const std::uintptr_t *>(emitter);
    if (vftable == Module::Address(addr::kEmitterSprite_Vftable))
    {
        return Kind::Sprite;
    }
    return vftable == Module::Address(addr::kEmitterSpriteLinked_Vftable) ? Kind::Ribbon : Kind::Other;
}

// The generators' state for an emitter: from its place, so the same emitter draws the same in every launch.
std::uint32_t SeedOf(std::uintptr_t emitter)
{
    const Vector place = NodePosition(emitter);
    std::uint32_t seed = 2166136261U; // FNV-1a over the place's bits
    for (const float value : {place.x, place.y, place.z})
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof bits);
        for (int i = 0; i < 4; ++i)
        {
            seed = (seed ^ ((bits >> (8 * i)) & 0xFFU)) * 16777619U;
        }
    }
    return seed;
}

// Whether the update would leave the emitter as it is this frame, as its own first test has it
// (0x00552D00 to 0x00552D57): an emitter updated only while seen, while the graphics cull and its
// owner has not been seen (its flag 0x20), or one with no opacity.
bool Held(std::uintptr_t emitter)
{
    constexpr std::size_t kBlueprintFlags = 0x131; // in the blueprint: 2 UpdateOnlyWhenVisible
    constexpr std::size_t kEmitterOwner = 0x154;   // in an emitter: its GraphicsStructures::Object
    constexpr std::size_t kOwnerBase = 8;          // in the object: the base whose slot 41 answers its flags
    constexpr std::size_t kOpacity = 0x4E0;
    constexpr std::size_t kCullsSlot = 0xF8 / 4;
    constexpr std::size_t kFlagsSlot = 0xA4 / 4;
    using BoolFn = bool(__fastcall *)(void *self, int edx);
    using FlagsFn = std::uint32_t(__fastcall *)(void *self, int edx);
    if (*reinterpret_cast<const float *>(emitter + kOpacity) <= 0.0F)
    {
        return true;
    }
    auto *graphics = *reinterpret_cast<void **>(Module::Address(addr::kGraphics));
    const auto blueprint = *reinterpret_cast<const std::uintptr_t *>(emitter + kBlueprint);
    const bool culls = reinterpret_cast<BoolFn>((*static_cast<void ***>(graphics))[kCullsSlot])(graphics, 0);
    if (!culls || (*reinterpret_cast<const std::uint8_t *>(blueprint + kBlueprintFlags) & 2U) == 0)
    {
        return false;
    }
    auto *owner =
        reinterpret_cast<void *>(*reinterpret_cast<const std::uintptr_t *>(emitter + kEmitterOwner) + kOwnerBase);
    return (reinterpret_cast<FlagsFn>((*static_cast<void ***>(owner))[kFlagsSlot])(owner, 0) & 0x20U) == 0;
}

// Empties the emitter as it was made, once the generators are seeded: its own clear, the spawn
// count's carry, and a ribbon's ring, UV counter and start frame as its constructor sets them
// (0x00586090: the start frame rand01 of the ribbon's generator with RandomStartFrame, else 0).
void Clear(std::uintptr_t emitter, Kind kind)
{
    constexpr std::size_t kRandomStartFrame = 0x203; // in the blueprint
    using ClearFn = void(__fastcall *)(std::uintptr_t self, int edx);
    reinterpret_cast<ClearFn>((*reinterpret_cast<const std::uintptr_t *const *>(emitter))[kClearSlot])(emitter, 0);
    *reinterpret_cast<float *>(emitter + kSpawnCarry) = 0.0F;
    if (kind != Kind::Ribbon)
    {
        return;
    }
    if (auto *ring = *reinterpret_cast<std::byte **>(emitter + kParticles))
    {
        std::memset(ring, 0, kRibbonSlots * kParticleSize);
    }
    *reinterpret_cast<std::int32_t *>(emitter + kRibbonUv) = -1;
    const auto blueprint = *reinterpret_cast<const std::uintptr_t *>(emitter + kBlueprint);
    float frame = 0.0F;
    if (*reinterpret_cast<const std::uint8_t *>(blueprint + kRandomStartFrame) != 0)
    {
        auto &state = *reinterpret_cast<std::uint32_t *>(Module::Address(addr::kParticleRandomSeedRibbon));
        state = (state * 0x343FDU) + 0x269EC3U;
        frame = static_cast<float>(static_cast<double>((state >> 16) & 0x7FFFU) / 32767.0);
    }
    *reinterpret_cast<float *>(emitter + kRibbonFrame) = frame;
}

void __fastcall SpriteUpdate(std::uintptr_t emitter, int edx, std::int32_t presimulate, float seconds)
{
    const Kind kind = KindOf(emitter);
    if (presimulate == 0 && kind != Kind::Other)
    {
        const std::lock_guard lock(g_replayMutex);
        if (g_replayAsked != 0 && g_replayed[emitter] != g_replayAsked && !Held(emitter))
        {
            g_replayed[emitter] = g_replayAsked;
            const std::uint32_t seed = SeedOf(emitter);
            for (const AddressId generator : {addr::kParticleRandomSeed, addr::kParticleRandomSeedSpawn,
                                              addr::kParticleRandomSeedSpread, addr::kParticleRandomSeedRibbon})
            {
                *reinterpret_cast<std::uint32_t *>(Module::Address(generator)) = seed;
            }
            Clear(emitter, kind);
            g_spriteUpdate.Original()(emitter, edx, 1, g_replaySeconds);
            g_spriteUpdate.Original()(emitter, edx, presimulate, seconds);
            return;
        }
    }
    g_spriteUpdate.Original()(emitter, edx, presimulate, seconds);
}

} // namespace

int ReplayEmitters(float seconds)
{
    // The classes as dao::TypeName gives a template's, decorated (seen in play, Denerim market: 83
    // plain sprites, 18 ribbons and one of physics particles).
    constexpr std::string_view kSprite =
        ".?AV?$ObjectNodeEmitterSprite@VSpriteParticle@GraphicsStructures@@@GraphicsStructures@@";
    constexpr std::string_view kRibbon =
        ".?AV?$ObjectNodeEmitterSpriteLinked@VSpriteParticle@GraphicsStructures@@@GraphicsStructures@@";
    if (!Module::IsKnownExecutable() || CClientExoApp::Get() == nullptr)
    {
        return 0;
    }
    {
        const std::lock_guard lock(g_replayMutex);
        if (!g_spriteUpdate.IsAttached() && !g_spriteUpdate.Attach(addr::kEmitterSprite_Update, &SpriteUpdate))
        {
            return 0;
        }
        g_replayed.clear();
        g_replaySeconds = std::max(seconds, 0.0F);
        g_replayAsked = g_replayAsked + 1 != 0 ? g_replayAsked + 1 : 1;
    }
    int emitters = 0;
    scene::ForEach(scene::Kind::Emitter, [&](const scene::Object &, const scene::Node &node) {
        emitters += node.address != 0 && (node.type == kSprite || node.type == kRibbon) ? 1 : 0;
    });
    return emitters;
}

} // namespace dao::vfx

namespace dao::detail
{

std::vector<ObjectId> RunningEffectIds()
{
    std::vector<ObjectId> ids;
    const auto manager = Module::IsKnownExecutable() ? reinterpret_cast<std::uintptr_t>(vfx::Manager()) : 0;
    if (manager == 0)
    {
        return ids;
    }
    const auto *begin = reinterpret_cast<const std::uintptr_t *>(Field(manager + vfx::kEffects));
    const auto *end = reinterpret_cast<const std::uintptr_t *>(Field(manager + vfx::kEffects + 4));
    for (const auto *entry = begin; begin != nullptr && entry < end; ++entry)
    {
        if (*entry != 0 && Field(*entry + vfx::kPhase) != 0)
        {
            ids.push_back(reinterpret_cast<const CCObject *>(*entry)->Id());
        }
    }
    return ids;
}

} // namespace dao::detail
