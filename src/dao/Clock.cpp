#include "dao/Clock.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Effects.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/Timers.h"
#include "dao/TypeName.h"
#include "dao/core/OwedTime.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace dao::clock
{

namespace
{

std::atomic<bool> g_held{false};

// What each creature's regeneration is owed of the plugin's advances, by
// its stats' address, on the shared clock or the creature's own
// (dao/core/OwedTime.h), as dao/Effects.cpp does for effects.
std::mutex g_lock;
core::OwedTime<std::uintptr_t> g_owed;
constexpr std::size_t kForgetAfter = 4000;

// The creature updating on this thread: CCCreature::Update is the one
// caller of UpdateRegeneration (0x006BE829), and hands it the stats of the
// creature it updates.
thread_local ObjectId t_creature = kInvalidObject;

// CCCreature::Update, slot 87: __thiscall, no arguments, `ret`.
using CreatureUpdateFn = std::int32_t(__fastcall *)(void *creature, int edx);
Detour<CreatureUpdateFn> g_creatureUpdate;

std::int32_t __fastcall CreatureUpdate(void *creature, int edx)
{
    const auto id = creature != nullptr
                        ? TryRead<ObjectId>(reinterpret_cast<std::uintptr_t>(creature) + CGameObject::kId)
                        : std::nullopt;
    const ObjectId outer = std::exchange(t_creature, id.value_or(kInvalidObject));
    const std::int32_t result = g_creatureUpdate.Original()(creature, edx);
    t_creature = outer;
    return result;
}

// CCCreatureStats::UpdateRegeneration: __thiscall (ms, bool in combat),
// `ret 8`.
using UpdateRegenerationFn = void(__fastcall *)(void *stats, int edx, std::int32_t ms, bool inCombat);
Detour<UpdateRegenerationFn> g_updateRegeneration;

void __fastcall UpdateRegeneration(void *stats, int edx, std::int32_t ms, bool inCombat)
{
    if (stats != nullptr)
    {
        const std::lock_guard lock(g_lock);
        if (g_owed.Items() > kForgetAfter)
        {
            g_owed.ForgetItems();
        }
        ms = (g_held.load(std::memory_order_relaxed) ? 0 : ms) +
             g_owed.Owed(reinterpret_cast<std::uintptr_t>(stats), t_creature);
    }
    g_updateRegeneration.Original()(stats, edx, ms, inCombat);
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_creatureUpdate.Attach(addr::kCCCreature_Update, &CreatureUpdate);
        g_updateRegeneration.Attach(addr::kCCCreatureStats_UpdateRegeneration, &UpdateRegeneration);
    });
}

// The game's time scale: the application's +0x2C8, which its frame update
// hands on each frame (dao/Clock.h).
constexpr std::size_t kTimeScale = 0x2C8;

} // namespace

bool SetSpeed(float scale, float seconds)
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr || scale <= 0 || seconds < 0)
    {
        return false;
    }
    // CClientExoApp::SetTimeScale: __thiscall (scale, seconds to ease over,
    // an owner's id or -1 for none), `ret 0xC`.
    using Fn = void(__fastcall *)(CClientExoApp *, int, float, float, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kCClientExoApp_SetTimeScale))(app, 0, scale, seconds, -1);
    return true;
}

float Speed()
{
    const CClientExoApp *app = CClientExoApp::Get();
    const auto scale =
        app != nullptr ? TryRead<float>(reinterpret_cast<std::uintptr_t>(app) + kTimeScale) : std::nullopt;
    // Below 0 is the engine's "none set", which its update leaves alone.
    return scale && *scale >= 0 ? *scale : 1.0F;
}

void Hold(bool held)
{
    Attach();
    g_held = held;
    if (!held)
    {
        const std::lock_guard lock(g_lock);
        g_owed.ShareAll();
    }
    effects::Hold(held);
    timers::Hold(held);
}

bool Held()
{
    return g_held;
}

void Advance(float seconds)
{
    Attach();
    {
        const std::lock_guard lock(g_lock);
        g_owed.Advance(static_cast<double>(seconds) * 1000);
    }
    effects::Advance(seconds);
    timers::Advance(seconds);
}

void OwnClock(ObjectId object, bool own)
{
    Attach();
    {
        const std::lock_guard lock(g_lock);
        g_owed.SetOwn(object, own);
    }
    effects::OwnClock(object, own);
    timers::OwnClock(object, own);
}

void AdvanceObject(ObjectId object, float seconds)
{
    Attach();
    {
        const std::lock_guard lock(g_lock);
        g_owed.Advance(object, static_cast<double>(seconds) * 1000);
    }
    effects::AdvanceObject(object, seconds);
    timers::AdvanceObject(object, seconds);
}

} // namespace dao::clock
