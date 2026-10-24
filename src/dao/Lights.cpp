// The ambient light each draw is given (dao/Lights.h).
//
// Read 2026-10-16 in the GOG exe, DAOLib #147; dao/Addresses.h has the
// addresses at kLightState_Fill and after. An object's light state is
// filled once a frame, when a material first asks for its lights:
// Renderer::LightState::Fill sets the state's +0x80, g_vLightParams[4], to
// its argument plus the colour of each ambient light in the state's +0x30.
// OnAmbient detours Fill and hands the callbacks what it made, then writes
// back what they set; every draw of the object that frame takes it from
// there. The shaders that read it, from DAOElgaraShaders' disassembly
// (2026-10-16): terrain1/2, static1 and prop1 all declare g_vLightParams;
// SpeedTree's spt_* declare g_vLightDiffuse and g_vColorMultiplier instead.
//
// Which chooser chose a state's lights is not in the state, so the
// choosers are detoured too and each state they choose for is recorded:
// ChooseForLevel for the level's pieces, ChooseForMoving for what the
// manager marks as moving. A state chosen before OnAmbient was first called
// is Unknown until it is chosen again (on a light's or the object's change).
//
// Seen in play 2026-10-16 by DAOElgaraShaders, from a detour of its own on
// Fill: the argument was zero in every sample, and most of a level's states
// had no ambient light at all, so the ambient was (0, 0, 0, 1). Redcliffe
// village: 79 of 82 states with none, 3 with one of 0.160 0.200 0.160. The
// Arl of Denerim's dungeon: 121 of 130 with none, 9 with one of 0.082 0.084
// 0.052.
//
// OnAmbient itself seen 2026-10-16 by DAOElgaraShaders, its callback
// setting the level's states to a sky light of about 0.015 0.021 0.028 in
// Redcliffe village by day: 30.7% of the frame changed, all of it the
// level, the figures and the sky not. Of every 20,000 calls about 13,460
// were ForLevel and 6,540 ForMoving, none Unknown (the callback added at
// load).
// OnProbe (DAOLib #225, read 2026-10-21): Fill takes the state's first
// chosen probe (its list at +0x24, an entry's +0x18 the node, as for the
// ambient lights) and copies the probe's cube map to +0x114 and its three
// irradiance matrices (0x0053C8F0, 0x0053C900, 0x0053C910) to +0x130, +0x170
// and +0x1B0, 48 floats in a row, only when the probe is not the one it kept
// at +0x110 (unless the byte at 0x00C1F370 is 0, when it copies every time);
// it then keeps the probe there. So a change made to the matrices would stay
// and be made again on the next. OnProbe clears +0x110 before Fill for a state
// that has a probe chosen, and the engine copies afresh; for one with none,
// Fill's own reset to the default matrices (0x004D33C0) is left as it is.
// Seen in play 2026-10-21, vanilla Redcliffe village by night: the probes
// handed out were the level's (lak100n_ll) and the portraits' rig's
// (outside_day); a callback zeroing the level's, against vanilla from the
// same paused view, left the hills as they were (within 0.4 of 255) and the
// figures darker (56, 48, 63 to 44, 36, 47 on average over them).
//
// OnPointLight (DAOLib #223, read 2026-10-21; dao/Addresses.h at
// kObjectNodePointLight_vftable) detours the light colour Fill takes each
// point and ambient light's colour from, before the flicker is multiplied in,
// and hands the callbacks a point light's: so the scaled colour is the one
// drawn that frame, and nothing is left in the light.
//
// Seen in play 2026-10-21, vanilla Redcliffe village by night: every light
// the engine took was in the level pieces' lists, the party portraits' rig as
// well, so the node says nothing of whose a light is; its owner's model's name
// does (dao/GraphicsNodes.h, the model's blueprint's; the owner's own +0x14,
// found first by a scan of the owners for the engine's strings, has the same
// names for these and none for a placed effect): lak100n_3_4_1_ll's fire_RT_0
// and fire_bounce_RT_0, lak100n_4_2_1_lc's char_11, portrait_lightrig's KEY_C,
// RIM_C, FILL_C, KEY_L, RIM_L and FILL_L (one rig to each portrait), and
// oth999d_0_ll's and oth999d_0_lc's. With every point light's colour set to 0 by a
// callback, against vanilla from the same paused view, the party's portraits
// went black and the figures darker: the colour returned is the one drawn.
//
// OnSunShare (DAOLib #230, read 2026-10-22; dao/Addresses.h at
// kLightState_EaseSun) detours the easing of a state's share of the sun,
// whose one caller hands it the target from the walk grid's sun subset, and
// hands the callbacks that target: so the engine's easing, and the sun's
// colour it scales, stay its own. Which chooser is choosing is known from
// the choosers' detours, which the easing runs inside.
//
// Seen in play 2026-10-22, GOG: in the Denerim market by day, 22,098 level
// pieces' shares set in ten seconds against as many ambient lights worked
// out, so the choosers run about once a frame for each object; none in
// vanilla's shade there. In Redcliffe village by night, paused in a fight,
// 39 to 855 in ten seconds were in vanilla's shade; the example's callback
// setting every share to 1, switched off and on again by a file it read
// each frame, made one figure under the hill darker (red, green, blue
// 109.8 92.0 117.5 to 103.6 86.0 110.1 over it) and as it was again within
// four seconds, nothing else in the frame changing.
#include "dao/Lights.h"

#include "dao/Addresses.h"
#include "dao/GraphicsNodes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/SceneDetail.h"
#include "dao/World.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dao::lights
{

namespace
{

constexpr std::size_t kAmbient = 0x80;         // in the state: g_vLightParams[4], four floats
constexpr std::size_t kAmbientLights = 0x34;   // in the state: begin and end of the chosen ambient lights
constexpr std::size_t kNodeOfReference = 0x18; // in what an entry points to: the light's node
constexpr std::size_t kStateOfNode = 0x68 / 4; // in the node's vtable: its light state
constexpr std::size_t kMostAmbientLights = 64; // more than any state was seen with (one)

constexpr std::size_t kOwnColor = 0x1A0;   // in a light's node: four floats
constexpr std::size_t kProbes = 0x24;      // in the state: begin and end of the chosen probes
constexpr std::size_t kKeptProbe = 0x110;  // in the state: the probe whose irradiance it holds
constexpr std::size_t kIrradiance = 0x130; // in the state: three 4x4 matrices of floats

// The scene's light manager and its lists (DAOLib #256, read 2026-10-24): the scene makes its
// managers with 0x0046C620 (0x00539FE0) and keeps the default one, Renderer::LightManagerDefault,
// at +0xDC, unless it defers them; each of its six lists is the chooser at +0, the most per object
// at +4 and a vector of 8-byte elements at +0xC and +0x10, each the record whose +0x18 is the
// light's node (Insert, 0x00464A50; AddLights, 0x00464E30, puts a point light in the figures' or
// the levels' pair by its node's +0x18C, and flagged or not by its blueprint's flag). A light's
// interface, whose slot 2 answers its place, is the node's +0x180; a probe's irradiance is its
// blueprint's (node +0x174) +0x160. Seen in play, 2026-10-24, at night in Redcliffe's village
// (examples/lights): 8 point lights, 1 the level's unflagged (Lyna's sword's glow, fxa_lit_swrd_c, at
// her place with her as owner), 4 the level's flagged (the layout's fires, lak100n_*_ll, 16 m of reach
// and their warm colour, no owner) and 3 the figures' flagged; 1 probe. With SetObjectsTakeUnflagged
// off, the sword light's colour was taken 220 times in 10 s against 650 with it on, and 650 again when
// it was put back; the third left is another of the light's users, about once a frame, not traced.
constexpr std::size_t kSceneLights = 0xDC;        // in the area's scene: its light manager
constexpr std::size_t kListBegin = 0xC;           // in a list: the elements' begin, then end
constexpr std::size_t kElement = 8;               // an element's size
constexpr std::size_t kRecordNode = 0x18;         // in an element's record: the light's node
constexpr std::size_t kProbeList = 0x3C;          // in the manager
constexpr std::size_t kLightOfNode = 0x180;       // in a light's node: its light interface
constexpr std::size_t kPlaceSlot = 2;             // in the light interface's vtable: its place
constexpr std::size_t kStrength = 0x1B4;          // in a point light's node
constexpr std::size_t kFlicker = 0x1BC;           // in a point light's node
constexpr std::size_t kOwnerObject = 0x154;       // in a light's node: the graphics object it is of
constexpr std::size_t kBlueprint = 0x174;         // in a light's node
constexpr std::size_t kProbeIrradiance = 0x160;   // in a probe's blueprint: 48 floats
constexpr std::size_t kChosenProbes = 0x20;       // in a light state: the chosen probes' group, begin at +4
constexpr std::size_t kUnflagged[] = {0xC, 0x84}; // the levels' and the figures' unflagged lists

struct ListOf
{
    std::size_t at;
    LightList list;
};
constexpr ListOf kPointLists[] = {{0xC, LightList::LevelsUnflagged},
                                  {0x24, LightList::LevelsFlagged},
                                  {0x6C, LightList::FiguresFlagged},
                                  {0x84, LightList::FiguresUnflagged}};

using Callbacks = std::vector<std::function<void(Ambient &)>>;
using PointCallbacks = std::vector<std::function<void(PointLight &)>>;
using ProbeCallbacks = std::vector<std::function<void(ProbeLight &)>>;
using SunCallbacks = std::vector<std::function<void(SunShare &)>>;

std::mutex g_mutex;
std::shared_ptr<const Callbacks> g_callbacks;           // replaced whole, never changed in place
std::shared_ptr<const PointCallbacks> g_pointCallbacks; // the same
std::shared_ptr<const ProbeCallbacks> g_probeCallbacks; // the same
std::shared_ptr<const SunCallbacks> g_sunCallbacks;     // the same
std::unordered_map<std::uintptr_t, Chosen> g_chosen;

using FillFn = void(__fastcall *)(void *state, int edx, const float *base);
using ChooseForLevelFn = void(__fastcall *)(void *manager, int edx, void **node, const void *sunDirection,
                                            const void *sunColor, char ambientLights, char other);
using ChooseForMovingFn = void(__fastcall *)(void *manager, int edx, void **node, const void *sunDirection,
                                             const void *other, char ambientLights);
using LightColorFn = void(__fastcall *)(const void *node, int edx, float *color);
using EaseSunFn = void(__fastcall *)(void *state, int edx, float target, char immediate);

Detour<FillFn> g_fill;
Detour<LightColorFn> g_color;
Detour<ChooseForLevelFn> g_chooseForLevel;
Detour<ChooseForMovingFn> g_chooseForMoving;
Detour<EaseSunFn> g_easeSun;

thread_local Chosen t_choosing = Chosen::Unknown; // the chooser running on this thread

// The light state of a node, as the choosers find it.
std::uintptr_t StateOf(void **node)
{
    using StateFn = std::uintptr_t(__fastcall *)(void *self, int edx);
    void *self = *node;
    const auto *vtable = *static_cast<StateFn *const *>(self);
    return vtable[kStateOfNode](self, 0);
}

void Record(void **node, Chosen chosen)
{
    const std::uintptr_t state = StateOf(node);
    if (state != 0)
    {
        const std::lock_guard lock(g_mutex);
        g_chosen[state] = chosen;
    }
}

void __fastcall ChooseForLevel(void *manager, int edx, void **node, const void *sunDirection, const void *sunColor,
                               char ambientLights, char other)
{
    t_choosing = Chosen::ForLevel;
    g_chooseForLevel.Original()(manager, edx, node, sunDirection, sunColor, ambientLights, other);
    t_choosing = Chosen::Unknown;
    Record(node, Chosen::ForLevel);
}

void __fastcall ChooseForMoving(void *manager, int edx, void **node, const void *sunDirection, const void *other,
                                char ambientLights)
{
    t_choosing = Chosen::ForMoving;
    g_chooseForMoving.Original()(manager, edx, node, sunDirection, other, ambientLights);
    t_choosing = Chosen::Unknown;
    Record(node, Chosen::ForMoving);
}

// The colours of the ambient lights Fill has just added, as it took them.
std::size_t ReadLights(std::uintptr_t state, std::array<Color, kMostAmbientLights> &out)
{
    auto *const begin = *reinterpret_cast<void ***>(state + kAmbientLights);
    auto *const end = *reinterpret_cast<void ***>(state + kAmbientLights + 4);
    // Through OnPointLight's detour, if it is attached: it leaves an ambient light's colour as it is.
    const auto color = reinterpret_cast<LightColorFn>(Module::Address(addr::kObjectNodeLight_Color));
    std::size_t count = 0;
    for (void **entry = begin; begin != nullptr && entry < end && count < out.size(); ++entry)
    {
        const auto reference = *static_cast<const std::uintptr_t *>(*entry);
        if (reference != 0)
        {
            color(*reinterpret_cast<const void *const *>(reference + kNodeOfReference), 0, out[count++].data());
        }
    }
    return count;
}

// Hands the ambient light callbacks what Fill made of the state's ambient light, and sets what they set.
void RunAmbient(std::uintptr_t state, const float *base, Chosen chosen, const Callbacks &callbacks)
{
    std::array<Color, kMostAmbientLights> lights{};
    Ambient ambient;
    std::memcpy(ambient.color.data(), reinterpret_cast<const float *>(state + kAmbient), sizeof(Color));
    std::memcpy(ambient.base.data(), base, sizeof(Color));
    ambient.lights = std::span<const Color>(lights.data(), ReadLights(state, lights));
    ambient.chosen = chosen;
    ambient.state = state;
    for (const auto &callback : callbacks)
    {
        detail::Guard("an ambient light callback", [&] { callback(ambient); });
    }
    std::memcpy(reinterpret_cast<float *>(state + kAmbient), ambient.color.data(), sizeof(Color));
}

// Whether the chooser chose a probe for the state.
bool HasProbe(std::uintptr_t state)
{
    const std::uintptr_t begin = detail::Field(state + kProbes);
    return begin != 0 && begin != detail::Field(state + kProbes + 4);
}

// Hands the probe callbacks the irradiance Fill has just copied, in place.
void RunProbe(std::uintptr_t state, Chosen chosen, const ProbeCallbacks &callbacks)
{
    const std::uintptr_t node = detail::Field(state + kKeptProbe);
    if (node == 0)
    {
        return;
    }
    ProbeLight probe{.irradiance = std::span<float, 48>(reinterpret_cast<float *>(state + kIrradiance), 48),
                     .model = detail::ModelName(node),
                     .chosen = chosen,
                     .probe = node,
                     .state = state};
    for (const auto &callback : callbacks)
    {
        detail::Guard("a light probe callback", [&] { callback(probe); });
    }
}

void __fastcall Fill(void *state, int edx, const float *base)
{
    const auto address = reinterpret_cast<std::uintptr_t>(state);
    std::shared_ptr<const Callbacks> ambientCallbacks;
    std::shared_ptr<const ProbeCallbacks> probeCallbacks;
    Chosen chosen = Chosen::Unknown;
    {
        const std::lock_guard lock(g_mutex);
        ambientCallbacks = g_callbacks;
        probeCallbacks = g_probeCallbacks;
        if (const auto found = g_chosen.find(address); found != g_chosen.end())
        {
            chosen = found->second;
        }
    }
    if (probeCallbacks && HasProbe(address))
    {
        // The engine copies the probe's irradiance afresh, so a callback's change is never made again on it.
        *reinterpret_cast<std::uintptr_t *>(address + kKeptProbe) = 0;
    }
    g_fill.Original()(state, edx, base);
    if (ambientCallbacks)
    {
        RunAmbient(address, base, chosen, *ambientCallbacks);
    }
    if (probeCallbacks)
    {
        RunProbe(address, chosen, *probeCallbacks);
    }
}

// A light's place: its light interface's own answer (slot 2), the translation of the node's matrix
// over its w, as Fill takes it (0x0046B44F; 0x0053CC90 -> 0x0053CA70).
Vector PlaceOf(std::uintptr_t node)
{
    using PlaceFn = void(__fastcall *)(void *light, int edx, float *out);
    auto *const light = reinterpret_cast<void *>(node + kLightOfNode);
    alignas(16) std::array<float, 4> out{};
    (*reinterpret_cast<PlaceFn *const *>(light))[kPlaceSlot](light, 0, out.data());
    return Vector{out[0], out[1], out[2]};
}

float ReachOf(std::uintptr_t node)
{
    using ReachFn = float(__fastcall *)(const void *node, int edx);
    return reinterpret_cast<ReachFn>(Module::Address(addr::kObjectNodePointLight_Reach))(
        reinterpret_cast<const void *>(node), 0);
}

void __fastcall LightColor(const void *node, int edx, float *color)
{
    g_color.Original()(node, edx, color);
    const auto address = reinterpret_cast<std::uintptr_t>(node);
    if (*reinterpret_cast<const std::uintptr_t *>(address) != Module::Address(addr::kObjectNodePointLight_vftable))
    {
        return;
    }
    std::shared_ptr<const PointCallbacks> callbacks;
    {
        const std::lock_guard lock(g_mutex);
        callbacks = g_pointCallbacks;
    }
    PointLight light;
    std::memcpy(light.color.data(), color, sizeof(Color));
    std::memcpy(light.own.data(), reinterpret_cast<const float *>(address + kOwnColor), sizeof(Color));
    light.name = detail::NodeName(address);
    light.model = detail::ModelName(address);
    light.node = address;
    light.place = PlaceOf(address);
    light.reach = ReachOf(address);
    for (const auto &callback : *callbacks)
    {
        detail::Guard("a point light callback", [&] { callback(light); });
    }
    std::memcpy(color, light.color.data(), sizeof(Color));
}

void __fastcall EaseSun(void *state, int edx, float target, char immediate)
{
    std::shared_ptr<const SunCallbacks> callbacks;
    {
        const std::lock_guard lock(g_mutex);
        callbacks = g_sunCallbacks;
    }
    SunShare sun{.share = target, .chosen = t_choosing, .state = reinterpret_cast<std::uintptr_t>(state)};
    for (const auto &callback : *callbacks)
    {
        detail::Guard("a sun share callback", [&] { callback(sun); });
    }
    g_easeSun.Original()(state, edx, sun.share, immediate);
}

// Detours the choosers, once; under g_mutex.
void AttachChoosers()
{
    if (!g_chooseForLevel.IsAttached() && Module::IsKnownExecutable())
    {
        g_chooseForLevel.Attach(addr::kLightManager_ChooseForLevel, &ChooseForLevel);
        g_chooseForMoving.Attach(addr::kLightManager_ChooseForMoving, &ChooseForMoving);
    }
}

// Detours Fill and the choosers, once, for OnAmbient and OnProbe; under g_mutex.
void AttachFill()
{
    AttachChoosers();
    if (!g_fill.IsAttached() && Module::IsKnownExecutable())
    {
        g_fill.Attach(addr::kLightState_Fill, &Fill);
    }
}

} // namespace

void OnPointLight(std::function<void(PointLight &)> callback)
{
    const std::lock_guard lock(g_mutex);
    auto callbacks =
        g_pointCallbacks ? std::make_shared<PointCallbacks>(*g_pointCallbacks) : std::make_shared<PointCallbacks>();
    callbacks->push_back(std::move(callback));
    g_pointCallbacks = std::move(callbacks);
    if (!g_color.IsAttached() && Module::IsKnownExecutable())
    {
        g_color.Attach(addr::kObjectNodeLight_Color, &LightColor);
    }
}

void OnAmbient(std::function<void(Ambient &)> callback)
{
    const std::lock_guard lock(g_mutex);
    auto callbacks = g_callbacks ? std::make_shared<Callbacks>(*g_callbacks) : std::make_shared<Callbacks>();
    callbacks->push_back(std::move(callback));
    g_callbacks = std::move(callbacks);
    AttachFill();
}

void OnProbe(std::function<void(ProbeLight &)> callback)
{
    const std::lock_guard lock(g_mutex);
    auto callbacks =
        g_probeCallbacks ? std::make_shared<ProbeCallbacks>(*g_probeCallbacks) : std::make_shared<ProbeCallbacks>();
    callbacks->push_back(std::move(callback));
    g_probeCallbacks = std::move(callbacks);
    AttachFill();
}

void OnSunShare(std::function<void(SunShare &)> callback)
{
    const std::lock_guard lock(g_mutex);
    auto callbacks =
        g_sunCallbacks ? std::make_shared<SunCallbacks>(*g_sunCallbacks) : std::make_shared<SunCallbacks>();
    callbacks->push_back(std::move(callback));
    g_sunCallbacks = std::move(callbacks);
    AttachChoosers();
    if (!g_easeSun.IsAttached() && Module::IsKnownExecutable())
    {
        g_easeSun.Attach(addr::kLightState_EaseSun, &EaseSun);
    }
}

namespace
{

// The current area's light manager, checked by its vtable; 0 with none.
std::uintptr_t AreaManager()
{
    CCArea *area = world::CurrentArea();
    const auto scene = area != nullptr ? reinterpret_cast<std::uintptr_t>(area->Scene()) : 0;
    const auto manager = scene != 0 ? TryRead<std::uintptr_t>(scene + kSceneLights).value_or(0) : 0;
    if (manager == 0 || TryRead<std::uintptr_t>(manager) != Module::Address(addr::kLightManagerDefault_vftable))
    {
        return 0;
    }
    return manager;
}

// The nodes of a vector of elements, each a record whose +0x18 is the node.
std::vector<std::uintptr_t> NodesOf(std::uintptr_t begin, std::uintptr_t end, std::size_t stride)
{
    std::vector<std::uintptr_t> nodes;
    for (std::uintptr_t at = begin; begin != 0 && at < end && nodes.size() < 4096; at += stride)
    {
        const auto record = TryRead<std::uintptr_t>(at).value_or(0);
        const auto node = record != 0 ? TryRead<std::uintptr_t>(record + kRecordNode).value_or(0) : 0;
        if (node != 0)
        {
            nodes.push_back(node);
        }
    }
    return nodes;
}

std::vector<std::uintptr_t> ListNodes(std::uintptr_t list)
{
    return NodesOf(TryRead<std::uintptr_t>(list + kListBegin).value_or(0),
                   TryRead<std::uintptr_t>(list + kListBegin + 4).value_or(0), kElement);
}

SceneProbe ProbeOf(std::uintptr_t node)
{
    SceneProbe probe;
    probe.node = node;
    probe.place = PlaceOf(node);
    const auto blueprint = TryRead<std::uintptr_t>(node + kBlueprint).value_or(0);
    if (blueprint != 0)
    {
        TryCopy(probe.irradiance.data(), blueprint + kProbeIrradiance, sizeof(probe.irradiance));
    }
    probe.model = std::wstring(detail::ModelName(node));
    return probe;
}

SceneLight LightOf(std::uintptr_t node, LightList list, const std::unordered_map<std::uintptr_t, ObjectId> &owners)
{
    SceneLight light;
    light.node = node;
    light.list = list;
    light.place = PlaceOf(node);
    light.reach = ReachOf(node);
    // The colour as the engine works it out, not through OnPointLight's detour and its callbacks.
    const auto color = g_color.IsAttached()
                           ? g_color.Original()
                           : reinterpret_cast<LightColorFn>(Module::Address(addr::kObjectNodeLight_Color));
    color(reinterpret_cast<const void *>(node), 0, light.color.data());
    TryCopy(light.own.data(), node + kOwnColor, sizeof(Color));
    light.strength = TryRead<float>(node + kStrength).value_or(0);
    light.flicker = TryRead<float>(node + kFlicker).value_or(0);
    for (std::size_t i = 0; i < 3; ++i)
    {
        light.color[i] *= light.strength * light.flicker;
    }
    const auto object = TryRead<std::uintptr_t>(node + kOwnerObject).value_or(0);
    if (const auto owner = owners.find(object); owner != owners.end())
    {
        light.owner = owner->second;
    }
    light.model = std::wstring(detail::ModelName(node));
    light.name = std::wstring(detail::NodeName(node));
    return light;
}

// The engine's quality pass, and what SetObjectsTakeUnflagged asked: after it, the unflagged lists
// take none for objects while the switch is off.
using ApplyQualityFn = void(__fastcall *)(void *manager, int edx);
Detour<ApplyQualityFn> g_applyQuality;
std::atomic<bool> g_takeUnflagged{true};

void TakeNoneOfUnflagged(std::uintptr_t manager)
{
    using SetLimitsFn = void(__fastcall *)(void *list, int edx, std::uint32_t most, void *chooser);
    const auto setLimits = reinterpret_cast<SetLimitsFn>(Module::Address(addr::kLightList_SetLimits));
    for (const std::size_t at : kUnflagged)
    {
        auto *const list = reinterpret_cast<void *>(manager + at);
        setLimits(list, 0, 0, *static_cast<void **>(list));
    }
}

void __fastcall ApplyQuality(void *manager, int edx)
{
    g_applyQuality.Original()(manager, edx);
    if (!g_takeUnflagged.load())
    {
        TakeNoneOfUnflagged(reinterpret_cast<std::uintptr_t>(manager));
    }
}

} // namespace

std::vector<SceneLight> PointLights()
{
    std::vector<SceneLight> lights;
    const std::uintptr_t manager = AreaManager();
    if (manager == 0)
    {
        return lights;
    }
    const auto owners = detail::GraphicsOwners();
    const auto pointLight = Module::Address(addr::kObjectNodePointLight_vftable);
    for (const ListOf &list : kPointLists)
    {
        for (const std::uintptr_t node : ListNodes(manager + list.at))
        {
            if (TryRead<std::uintptr_t>(node) == pointLight)
            {
                lights.push_back(LightOf(node, list.list, owners));
            }
        }
    }
    return lights;
}

std::vector<SceneProbe> Probes(std::uintptr_t state)
{
    std::vector<SceneProbe> probes;
    if (state == 0)
    {
        return probes;
    }
    // The group's entries are pointers to the manager's elements, whose first word is the record.
    for (const std::uintptr_t node : NodesOf(TryRead<std::uintptr_t>(state + kChosenProbes + 4).value_or(0),
                                             TryRead<std::uintptr_t>(state + kChosenProbes + 8).value_or(0), 4))
    {
        probes.push_back(ProbeOf(node));
    }
    return probes;
}

std::vector<SceneProbe> AreaProbes()
{
    std::vector<SceneProbe> probes;
    if (const std::uintptr_t manager = AreaManager(); manager != 0)
    {
        for (const std::uintptr_t node : ListNodes(manager + kProbeList))
        {
            probes.push_back(ProbeOf(node));
        }
    }
    return probes;
}

void SetObjectsTakeUnflagged(bool take)
{
    if (!Module::IsKnownExecutable())
    {
        return;
    }
    static std::once_flag once;
    std::call_once(once, [] { g_applyQuality.Attach(addr::kLightManagerDefault_ApplyQuality, &ApplyQuality); });
    if (g_takeUnflagged.exchange(take) == take)
    {
        return;
    }
    const std::uintptr_t manager = AreaManager();
    if (manager == 0)
    {
        return;
    }
    // Vanilla's limits are the quality pass's own; it is run again to put them back.
    if (take)
    {
        g_applyQuality.Original()(reinterpret_cast<void *>(manager), 0);
    }
    else
    {
        TakeNoneOfUnflagged(manager);
    }
    using MarkChangedFn = void(__fastcall *)(void *manager, int edx);
    reinterpret_cast<MarkChangedFn>(Module::Address(addr::kLightManager_MarkChanged))(reinterpret_cast<void *>(manager),
                                                                                      0);
}

} // namespace dao::lights
