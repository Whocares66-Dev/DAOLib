// A creature's death, revival and readiness, from the engine's own points (dao/Creatures.h).
//
// Notes on the death: read 2026-10-24 (DAOLib #255). Every setter of a creature's current health
// (the stats' SetCurrent, 0x0071B320: SetCreatureProperty, the regeneration's clamp, the death
// effect's ExecuteEffect) ends in its change notice (0x00719A00), which for the health calls
// CCCreature::OnHealthChanged (0x006BC4B0, dao/Addresses.h) with the old value. That handler tells
// a death (old above 0, new at or below) and a revive (the other way) from any other change, so
// detouring it sees each once, whatever the cause, on the frame it happens. A fatal blow's
// KillCreature applies the death effect (type 7, duration type 6, its creator the killer;
// 0x007A12E0), and the effect's ExecuteEffect lowers the health on its first tick; a script's
// SetCurrentHealth(0) lowers it at once, and the handler then applies the death effect with the
// creature as its creator. So after the original the creature has its death effect, and its
// creator is the killer. The scripts' EVENT_TYPE_DEATH follows once the death has played out,
// 1.5 to 5 s later (dao/Events.cpp's notes).
//
// Notes on readiness: a creature's template load sets each property to its minimum (health 1,
// mana 0; dev/PROPERTIES.md), and its spawn script (creature_core.nss, EVENT_TYPE_SPAWN:
// AS_InitCreature) sets its level, attributes, abilities and depletables. The engine queues the
// spawn event as the creature joins its area (0x006B6BC0, once, flag +0xC54) and runs the script
// at the queue's next turn, a frame or more after CreateObject returns, which is why a creature
// read just after its creation shows health 1 of 1 (the Eluvian server, its Stage.cpp). So
// readiness is the spawn event handled, through dao::events::OnScriptEventHandled.
//
// Seen in play, 2026-10-24, in Redcliffe village at night (examples/moments): of 17 deaths in a
// minute's fight, each came on its frame with its killer (Ser Perth, Dwyn, a walking corpse for a
// knight), 2.1 s before the scripts' EVENT_TYPE_DEATH, and two killed by the Eluvian server's
// stage_kill, which deals its damage from no one, with no killer. The siege's walking corpses rise
// again, and each rising came as a revival. A Ser Perth made by the Eluvian server's stage_spawn
// was ready with 499 of 499 health. The Eluvian server saw, the same day, three hunters a save
// restored dead come as deaths mid-load, Tamlen their killer (their saved death effect's
// creator), so a health change while the game is loading tells nothing; a death or revival then
// is the save's or the area's, not this session's.
#include "dao/Creatures.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Effects.h"
#include "dao/Events.h"
#include "dao/GameModes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"

#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::creatures
{

namespace
{

std::mutex g_mutex;
std::vector<std::function<void(ObjectId, ObjectId)>> g_deaths;
std::vector<std::function<void(ObjectId)>> g_revivals;

template <class T> std::vector<T> Copy(const std::vector<T> &callbacks)
{
    const std::lock_guard lock(g_mutex);
    return callbacks;
}

// The death effect's creator: the killer, or the creature itself for a death no one dealt.
ObjectId KillerOf(ObjectId creature)
{
    for (const effects::Standing &effect : effects::On(creature))
    {
        if (effect.type == script::EffectType::Death && effect.durationType == script::EffectDurationType::Death)
        {
            return effect.creator;
        }
    }
    return kInvalidObject;
}

// Whether the game is loading: a save restoring a creature dead sets its health to 0 then
// (0x006BEBA3, its saved death effect), as does an area building a creature placed dead, and
// neither is a death of this session.
bool Loading()
{
    const CClientExoApp *app = CClientExoApp::Get();
    const GameModeController *modes = app != nullptr ? app->GameModes() : nullptr;
    return modes != nullptr && modes->Mode() == GameMode::Loading;
}

using OnHealthChangedFn = void(__fastcall *)(CCCreature *creature, int edx, float oldHealth, std::uint32_t flag);
Detour<OnHealthChangedFn> g_onHealthChanged;

void __fastcall OnHealthChanged(CCCreature *creature, int edx, float oldHealth, std::uint32_t flag)
{
    g_onHealthChanged.Original()(creature, edx, oldHealth, flag);
    if (creature == nullptr || Loading())
    {
        return;
    }
    const auto now = creature->Health();
    if (!now)
    {
        return;
    }
    const ObjectId id = creature->Id();
    if (oldHealth > 0 && *now <= 0)
    {
        const ObjectId killer = KillerOf(id);
        for (const auto &observer : Copy(g_deaths))
        {
            detail::Guard("a death callback", [&] { observer(id, killer); });
        }
    }
    else if (oldHealth <= 0 && *now > 0)
    {
        for (const auto &observer : Copy(g_revivals))
        {
            detail::Guard("a revival callback", [&] { observer(id); });
        }
    }
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] { g_onHealthChanged.Attach(addr::kCCCreature_OnHealthChanged, &OnHealthChanged); });
}

} // namespace

void OnDeath(std::function<void(ObjectId creature, ObjectId killer)> observer)
{
    {
        const std::lock_guard lock(g_mutex);
        g_deaths.push_back(std::move(observer));
    }
    Attach();
}

void OnRevive(std::function<void(ObjectId creature)> observer)
{
    {
        const std::lock_guard lock(g_mutex);
        g_revivals.push_back(std::move(observer));
    }
    Attach();
}

void OnReady(std::function<void(ObjectId creature)> observer)
{
    events::OnScriptEventHandled(script::EventType::Spawn, [observer = std::move(observer)](ObjectId object) {
        CClientExoApp *app = CClientExoApp::Get();
        if (app != nullptr && app->GetCreatureById(object) != nullptr)
        {
            observer(object);
        }
    });
}

// Notes on movement: read 2026-10-24 (DAOLib #257). A creature's movement controller is in it at
// +0xA30 (its RTTI's MovementController, vtable 0x00B596C0; dev/ANIMATION.md, dev/PATHFINDING.md).
// Its target speed (0x00A63120) is apr_base's Game_WalkSpeed or Game_RunSpeed by its state (+0xF0:
// 1 walk, 2 run; 0, 3 and 4 none), or Game_CbtSpeed in combat (+0xF9), plus +0xDC, times +0xD4; its
// update (0x00A635D0) keeps that at +0xE4 and eases the speed at +0xE0 toward it by 8 m/s each
// second. +0xD4 is the product of the speed effects (types 9 and 11, each's engine float 1): their
// apply and remove handlers (0x007A0610, 0x007A0690) store it through the creature's +0x70 slot
// 0x58 (0x006A3704), so it is what the turn-based mod reckoned from the effects itself. Seen in
// play, 2026-10-24: Lyna in combat at 4.66 m/s (apr_base's human Game_CbtSpeed), multiplier 1; a
// movement rate effect staged with no potency made it 0 and stopped her; removed, 1 and 4.66 again.

namespace
{

constexpr std::size_t kMovement = 0xA30; // in a creature: its movement controller
constexpr std::size_t kMultiplier = 0xD4;
constexpr std::size_t kOffset = 0xDC;
constexpr std::size_t kSpeed = 0xE0;
constexpr std::size_t kTargetSpeed = 0xE4;
constexpr std::size_t kState = 0xF0;
constexpr std::size_t kCombat = 0xF9;

} // namespace

std::optional<Movement> MovementOf(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    CCCreature *found = app != nullptr ? app->GetCreatureById(creature) : nullptr;
    if (found == nullptr)
    {
        return std::nullopt;
    }
    const auto at = reinterpret_cast<std::uintptr_t>(found) + kMovement;
    Movement movement;
    movement.speed = TryRead<float>(at + kSpeed).value_or(0);
    movement.target = TryRead<float>(at + kTargetSpeed).value_or(0);
    movement.multiplier = TryRead<float>(at + kMultiplier).value_or(1);
    movement.offset = TryRead<float>(at + kOffset).value_or(0);
    movement.state = TryRead<std::int32_t>(at + kState).value_or(0);
    movement.combat = TryRead<std::uint8_t>(at + kCombat).value_or(0) != 0;
    return movement;
}

} // namespace dao::creatures
