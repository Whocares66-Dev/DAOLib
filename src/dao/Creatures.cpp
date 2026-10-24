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
// was ready with 499 of 499 health.
#include "dao/Creatures.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Effects.h"
#include "dao/Events.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Objects.h"

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

using OnHealthChangedFn = void(__fastcall *)(CCCreature *creature, int edx, float oldHealth, std::uint32_t flag);
Detour<OnHealthChangedFn> g_onHealthChanged;

void __fastcall OnHealthChanged(CCCreature *creature, int edx, float oldHealth, std::uint32_t flag)
{
    g_onHealthChanged.Original()(creature, edx, oldHealth, flag);
    if (creature == nullptr)
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

} // namespace dao::creatures
