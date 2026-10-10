// A spell held in its conjure (dao/Conjure.h): the engine's own cast, its caster drawing in
// the spell with the game's animation, moved on only by the time a plugin
// gives it. A turn-based mode hands a spell to the engine at once and
// feeds its conjure as the caster's turn pays for it, so that everything
// the game does to a conjuring caster it still does: the installed
// rules_core's damage rule interrupts it (shipped or fixed), a knockdown
// ends it, and the cost is taken only when the conjure completes.
//
// Read 2026-10-02 and seen in play the same day by the probe's conjure
// watch (dev/TURN_BASED.md, "A command's timed phases and the creature's
// clock"): a UseAbility's conjure is its sub-action of type 7, timed (its
// duration the ability's conjure, 3,866 ms for Chain Lightning), its
// elapsed time counting the caster's own milliseconds, done when it
// reaches the duration; the cost is taken at the type 8 after it. Each
// update, CSSubActionHandler::Execute runs it; for a held caster this sets
// the elapsed time first to what has been fed, so the conjure runs as
// ever, animation and IsConjuring with it, and completes only once fed its
// whole duration. The cast (37) and what follows then run in real time.
// The hold seen in play the same day, the turn-based mod's play test 33
// (DAOTurnBased dev/PLAYTEST-2026-10-02-33.md): a caster's Chain Lightning
// held conjuring through two spiders' turns, the lightning round her
// frame after frame and the game's own action label up; an ogre's ability
// cleared it before its time was paid; in the re-check, fed its whole
// time, it was let go and the bolt flew at the start of her next turn.
#include "dao/Conjure.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/core/ObjectType.h"

#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace dao::commands
{

namespace
{

// A sub-action's type at +0, its elapsed time at +0xC and its duration at
// +0x14, in milliseconds (dao/Addresses.h, RunCurrentCommand); the conjure
// is type 7 (dev/TURN_BASED.md).
constexpr std::uint32_t kConjure = 7;
constexpr std::uintptr_t kElapsed = 0xC;
constexpr std::uintptr_t kDuration = 0x14;

struct Held
{
    double fed = 0;               // milliseconds given and not yet spent
    double elapsed = 0;           // the held conjure's, in milliseconds
    std::uintptr_t subAction = 0; // the conjure it is holding
};

struct Seen
{
    std::int32_t elapsed = 0;
    std::int32_t duration = 0;
};

std::mutex g_lock;
std::unordered_map<ObjectId, Held> g_held;
std::unordered_map<ObjectId, Seen> g_seen;

// CSSubActionHandler::Execute: __thiscall (object, sub-action), `ret 8`.
using ExecuteFn = std::int32_t(__fastcall *)(void *handler, int edx, CCObject *object, std::uintptr_t subAction);
Detour<ExecuteFn> g_execute;

std::int32_t __fastcall Execute(void *handler, int edx, CCObject *object, std::uintptr_t subAction)
{
    if (object != nullptr && subAction != 0 && object->Type() == core::ObjectType::Creature &&
        TryRead<std::uint32_t>(subAction) == kConjure)
    {
        const auto elapsed = TryRead<std::int32_t>(subAction + kElapsed);
        const auto duration = TryRead<std::int32_t>(subAction + kDuration);
        if (elapsed && duration && *duration > 0)
        {
            const ObjectId id = object->Id();
            const std::lock_guard lock(g_lock);
            std::int32_t now = *elapsed;
            if (const auto held = g_held.find(id); held != g_held.end())
            {
                Held &hold = held->second;
                if (hold.subAction != subAction)
                {
                    // A conjure first seen held: from where it stands, which
                    // is at most this update's real time on.
                    hold.subAction = subAction;
                    hold.elapsed = *elapsed;
                }
                hold.elapsed = std::fmin(hold.elapsed + hold.fed, *duration);
                hold.fed = 0;
                now = static_cast<std::int32_t>(hold.elapsed);
                *reinterpret_cast<std::int32_t *>(subAction + kElapsed) = now;
            }
            g_seen[id] = Seen{now, *duration};
        }
    }
    return g_execute.Original()(handler, edx, object, subAction);
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] { g_execute.Attach(addr::kCSSubActionHandler_Execute, &Execute); });
}

} // namespace

void HoldConjure(ObjectId caster, bool hold)
{
    Attach();
    const std::lock_guard lock(g_lock);
    if (hold)
    {
        g_held.try_emplace(caster);
    }
    else
    {
        g_held.erase(caster);
    }
}

bool ConjureHeld(ObjectId caster)
{
    const std::lock_guard lock(g_lock);
    return g_held.contains(caster);
}

void FeedConjure(ObjectId caster, float seconds)
{
    if (seconds <= 0)
    {
        return;
    }
    const std::lock_guard lock(g_lock);
    if (const auto held = g_held.find(caster); held != g_held.end())
    {
        held->second.fed += static_cast<double>(seconds) * 1000;
    }
}

std::optional<Conjuring> Conjure(ObjectId caster)
{
    Attach();
    CClientExoApp *app = CClientExoApp::Get();
    const CCObject *object = app != nullptr ? app->GetObjectById(caster) : nullptr;
    if (object == nullptr || object->Type() != core::ObjectType::Creature)
    {
        return std::nullopt;
    }
    // CCCreature::ForScript_IsConjuring: __thiscall, no arguments; the
    // running sub-action's type (dev/TURN_BASED.md).
    using IsConjuringFn = std::uint32_t(__fastcall *)(const CCObject *, int);
    if (reinterpret_cast<IsConjuringFn>(Module::Address(addr::kCCCreature_IsConjuring))(object, 0) != kConjure)
    {
        return std::nullopt;
    }
    const std::lock_guard lock(g_lock);
    const auto seen = g_seen.find(caster);
    if (seen == g_seen.end())
    {
        return std::nullopt;
    }
    return Conjuring{static_cast<float>(seen->second.elapsed) / 1000, static_cast<float>(seen->second.duration) / 1000};
}

// The type of the sub-action a creature's current command is at, as the
// engine's IsConjuring answers it (dev/TURN_BASED.md, "A command's timed
// phases"): the type of the first sub-action left on its current command's
// list (IsConjuring reads it through 0x00697BF0), the one running or about
// to run, so never 0 between two of them; 0 only when the creature has no
// current command, or none of its sub-actions is left, or it is no
// creature. A UseAbility's run 15, 39, 50,
// 7 (the conjure), 8, 37 (seen in play). An attack's, seen in play
// 2026-10-04 (the turn-based mod, Zevran's ambush, 160 attacks) and read
// in their handlers: an approach, 4 for melee or 15 for a ranged weapon
// (dao/Attack.h), the only ones in which the attacker walks; then the
// attack's own. A melee blow is 1 (after a turn to face, 39, to within 1
// degree), which waits while the target is more than 60 degrees above or
// below the attacker or out of sight, then sends EVENT_TYPE_COMMAND_PENDING
// for the scripts to resolve it. A shot runs 39 (turn to face,
// 0x007A69E0), 37 (a timed animation, 0x007A4390), 6 (the aim, 0x007A2410:
// a timed wait that holds while the target is out of sight or more than
// 67.5 degrees off the facing) and 2 (the shot, 0x007A6490, which sends
// COMMAND_PENDING), with 37 again; not every shot runs every one. The
// command's 50, which puts the attack's own in, runs within the same
// update (several sub-actions run in one while its time lasts,
// 0x0073CB60), so it is not seen between frames. 0 between the last of
// them and the command's end, seen; whether it can be 0 for a frame
// before the command's step puts its sub-actions in is not read. On the
// game's thread.
std::uint32_t RunningSubAction(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    const CCObject *object = app != nullptr ? app->GetObjectById(creature) : nullptr;
    if (object == nullptr || object->Type() != core::ObjectType::Creature)
    {
        return 0;
    }
    using IsConjuringFn = std::uint32_t(__fastcall *)(const CCObject *, int);
    return reinterpret_cast<IsConjuringFn>(Module::Address(addr::kCCCreature_IsConjuring))(object, 0);
}

} // namespace dao::commands
