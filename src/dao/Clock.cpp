// The clock for effects, regeneration and timers, and the game's speed
// (dao/Clock.h).
//
// Read 2026-09-30. Effects: dao/Effects.h, which this drives (and which
// also decides per effect). Regeneration: each creature's stats run
// UpdateRegeneration (0x0071B0E0, __thiscall (ms, bool in combat),
// `ret 8`) on its update, adding the milliseconds to an accumulator (the
// stats' +0x58); each 3000 ms out of combat, or 2000 in combat, every
// depletable property (health, mana or stamina) gains once what its
// regeneration property gives, the living only. Which property that is,
// is data: properties.gda's NonCombatEffect and CombatEffect columns
// (health 14 and 28, mana and stamina 29 and 30). This hooks
// UpdateRegeneration as dao/Effects.h hooks the effects' ageing, so a
// round's worth given at once regenerates as many times as it holds.
//
// Timers, 2026-10-02 (dao/Timers.h): held, an effect did not wear off
// while the script events that make it hurt went on in real time, so a
// trap's poison (a damage over time each 1.5 s) hit a turn-based party for
// as long as the fight lasted, seen by the user in the Dalish origin's
// Elven Ruins. Its ticks, and a lasting area's pulses, now wait with the
// clock. Not reached: other timers a script keeps itself (dao/Timers.h
// says which kinds are held), and cooldowns, which have their own script
// commands (GetRemainingCooldown, SetCooldown). On the game's thread. Not
// yet seen in play.
//
// A consequence, seen by the turn-based mod in play, 2026-09-30: a
// creature under an effect that holds it (a pin, a knockdown) is kept busy
// by a command that ends when the effect does (ExecuteEffect, type 32,
// queued at the front as the effect lands). Held, the effect does not end,
// so neither does the command, and the creature does nothing until the
// clock is advanced past it or the effect is removed. The same with an
// ability that is a timed effect on its user and one on its target (a
// giant spider's Overwhelm, 2026-10-01): held, it never ends. The mod now
// advances the clock in quarter-second steps while the acting creature is
// at such a command after its own action, and takes that off the round's
// worth.
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
// The world timer at the application's +0xD4, and the day's length in
// seconds at the timer's +0x50 (dao/Addresses.h).
constexpr std::uintptr_t kAppWorldTimer = 0xD4;
constexpr std::uintptr_t kTimerDayLength = 0x50;

} // namespace

// The game's own time scale, read 2026-09-30, what its slow motion uses.
// The application keeps it at +0x2C8 (SetTimeScale, 0x005D81C0, sets it,
// or eases it there over a time), and its frame update (0x005D7560) hands
// it on each frame: to the world timer (EC::WorldTimer, the application's
// +0xD4, its scale at +0x54), which the game's logic runs on (each
// object's timer, so the AI, commands, effects and regeneration), and to
// the scene (0x008107F0, its slot 6), which animation plays by. Writing the
// world timer's scale directly, as this did before, lasted a frame: the
// update overwrites it from +0x2C8 each frame (in play, 2026-09-30, a
// creature's own time stayed at 1000 ms a wall second "at speed 2", and its
// pace with it). Seen in play this way, 2026-09-30 (examples/speed's
// meter): at 2 a creature's own time ran 2000 to 2046 ms a wall second and
// the hero ran 8 to 9 m/s against 4.5 to 4.7 at 1, a fight taking 32 s
// where one had taken 51; the frame rate did not change. The Flash UI
// keeps its own clock (Scaleform's GTimer) and is not changed.
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

// The world's clock, read 2026-10-22 for Eluvian's snapshot (DAOLib #232).
// EC::WorldTimer is made once, in the application's start-up (0x00A35F30,
// called at 0x005DB83D, stored at the application's +0xD4). Its update
// (0x00A35A10) runs slot 6 (0x00A358F0), which adds the frame's seconds,
// at most 0.1 and times the scale (+0x54), to the double at +0x18, nothing
// while +0x58 is set; it then keeps that double as microseconds at +0x28.
// WorldTime (0x00A35AF0) gives the day and ms of +0x28 over the day's
// length (+0x4C ms) plus the offsets at +0x40 (day) and +0x44 (ms),
// carrying ms into days; while +0x58 is set, the day and ms it froze at
// (+0x38, +0x3C). A save writes WorldTime's two (slot 9, 0x00A35FD0). A
// load (slot 8, 0x00A36010) calls SetTime(day, ms, 0) (0x00A35A80), which
// sets the offsets so that WorldTime answers the saved values, then the
// update twice: it does not touch +0x18 or +0x28. No slot of the timer
// resets them, so +0x18, which GetTime (705) reads, counts the seconds of
// world time since the game was started, and is neither restored nor
// reset by a load. WorldTime is the clock a save keeps.
std::optional<WorldTime> Now()
{
    const CClientExoApp *app = CClientExoApp::Get();
    const auto timer =
        app != nullptr ? TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(app) + kAppWorldTimer) : std::nullopt;
    const auto length = timer && *timer != 0 ? TryRead<std::int32_t>(*timer + kTimerDayLength) : std::nullopt;
    if (!length)
    {
        return std::nullopt;
    }
    WorldTime now;
    now.dayLength = *length > 0 ? static_cast<std::uint32_t>(*length) : 0;
    // EC::WorldTimer::WorldTime: __thiscall (day *, ms *), `ret 8`.
    using WorldTimeFn = void(__fastcall *)(std::uintptr_t timer, int edx, std::uint32_t *day, std::uint32_t *ms);
    reinterpret_cast<WorldTimeFn>(Module::Address(addr::kWorldTimer_WorldTime))(*timer, 0, &now.day, &now.ms);
    now.seconds = (static_cast<double>(now.day) * now.dayLength) + (now.ms / 1000.0);
    return now;
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

// A creature's own time, for a fight measured per creature.
//
// Read 2026-10-02: an effect is aged by its owner's clock (the object it
// is on, dao/Effects.h), regeneration by its creature's (CCCreature::Update
// is UpdateRegeneration's one caller), a timer by its target's
// (dao/Timers.h). Seen in play the same day by the turn-based mod: a
// Walking Bomb's ticks on the ogre's own turns, and, once the effects'
// owner was found right (dao/TypeName.h, CompleteObject), a spider's
// Overwhelm run down by the spider's own advances alone; and regeneration
// (10ac778, two rounds of spiders, nobody in the party acting): Lyna, whose
// in-combat health regeneration is 1.0, gained 2.0 once a round, six
// rounds of six, each as her own clock ran out her turn's 4 s and at no
// other creature's turn; Tamlen, whose is 0, gained nothing.
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
