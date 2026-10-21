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
#include "dao/Lights.h"

#include "dao/Addresses.h"
#include "dao/GraphicsNodes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"

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

using Callbacks = std::vector<std::function<void(Ambient &)>>;
using PointCallbacks = std::vector<std::function<void(PointLight &)>>;
using ProbeCallbacks = std::vector<std::function<void(ProbeLight &)>>;

std::mutex g_mutex;
std::shared_ptr<const Callbacks> g_callbacks;           // replaced whole, never changed in place
std::shared_ptr<const PointCallbacks> g_pointCallbacks; // the same
std::shared_ptr<const ProbeCallbacks> g_probeCallbacks; // the same
std::unordered_map<std::uintptr_t, Chosen> g_chosen;

using FillFn = void(__fastcall *)(void *state, int edx, const float *base);
using ChooseForLevelFn = void(__fastcall *)(void *manager, int edx, void **node, const void *sunDirection,
                                            const void *sunColor, char ambientLights, char other);
using ChooseForMovingFn = void(__fastcall *)(void *manager, int edx, void **node, const void *sunDirection,
                                             const void *other, char ambientLights);
using LightColorFn = void(__fastcall *)(const void *node, int edx, float *color);

Detour<FillFn> g_fill;
Detour<LightColorFn> g_color;
Detour<ChooseForLevelFn> g_chooseForLevel;
Detour<ChooseForMovingFn> g_chooseForMoving;

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
    g_chooseForLevel.Original()(manager, edx, node, sunDirection, sunColor, ambientLights, other);
    Record(node, Chosen::ForLevel);
}

void __fastcall ChooseForMoving(void *manager, int edx, void **node, const void *sunDirection, const void *other,
                                char ambientLights)
{
    g_chooseForMoving.Original()(manager, edx, node, sunDirection, other, ambientLights);
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
    for (const auto &callback : *callbacks)
    {
        detail::Guard("a point light callback", [&] { callback(light); });
    }
    std::memcpy(color, light.color.data(), sizeof(Color));
}

// Detours Fill and the choosers, once, for OnAmbient and OnProbe; under g_mutex.
void AttachFill()
{
    if (!g_fill.IsAttached() && Module::IsKnownExecutable())
    {
        g_chooseForLevel.Attach(addr::kLightManager_ChooseForLevel, &ChooseForLevel);
        g_chooseForMoving.Attach(addr::kLightManager_ChooseForMoving, &ChooseForMoving);
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

} // namespace dao::lights
