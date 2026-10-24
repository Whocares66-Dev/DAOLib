// A creature's current command's sub-actions, and the time they still hold
// it, for any effect and with no list of kinds (DAOLib issue #18).
//
// Read 2026-10-04 (dao/Addresses.h, RunCurrentCommand; dev/TURN_BASED.md,
// "A command's timed phases"): the current command's sub-actions are a
// std::list of pointers at the creature's +0x2DC record's +0xC, the head
// the one running (what IsConjuring answers, dao/Conjure.h's
// RunningSubAction). Each keeps its type at +0, a delay at +0x18, a
// duration at +0x14 and the time elapsed at +0xC, in the creature's own
// milliseconds. The update spends the delay first, then for a timed one
// (a duration above 0) the elapsed time up to the duration; an untimed one
// runs until its handler says it is done, so its time is not known ahead,
// and some set their duration only as they begin (an animation's, 37, its
// length; an aim's, 6). So the time a command still holds a creature is
// known exactly only where every sub-action left is timed, begun and not
// yet spent.
//
// Seen in play 2026-10-04 (the turn-based mod against the high dragon, 50
// holds of the party): a knockback (effect type 1057) is an ExecuteEffect
// with one sub-action, 51, timed to the effect's 1200 ms and found spent
// (1200 of 1200) as the turn began; then none left while the command
// still held the creature, 0.6 to 3.2 s in all, the fall and the rise.
// After the dragon's roar the count fell about 0.4 s short of the hold.
#include "dao/SubActions.h"

#include "dao/ClientExoApp.h"
#include "dao/CommandEvents.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/core/ObjectType.h"

#include <algorithm>
#include <mutex>
#include <utility>

namespace dao::commands
{

namespace
{

// The creature's command record (+0x2DC), its sub-actions' std::list at
// the record's +0xC: the head node at the list's +4, the size at +8; a
// node's next at +0 and its value, the sub-action, at +8.
constexpr std::uintptr_t kRecord = 0x2DC;
constexpr std::uintptr_t kList = 0xC;
constexpr std::uintptr_t kHead = 4;
constexpr std::uintptr_t kSize = 8;
constexpr std::uintptr_t kValue = 8;
// A sub-action's fields (dao/Addresses.h, RunCurrentCommand).
constexpr std::uintptr_t kElapsed = 0xC;
constexpr std::uintptr_t kDuration = 0x14;
constexpr std::uintptr_t kDelay = 0x18;
// As many nodes as are read before the list is taken to be broken.
constexpr std::uint32_t kMostNodes = 64;

} // namespace

std::vector<SubAction> SubActions(ObjectId creature)
{
    std::vector<SubAction> found;
    CClientExoApp *app = CClientExoApp::Get();
    const CCObject *object = app != nullptr ? app->GetObjectById(creature) : nullptr;
    if (object == nullptr || object->Type() != core::ObjectType::Creature)
    {
        return found;
    }
    const auto record = TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object) + kRecord);
    if (!record || *record == 0)
    {
        return found;
    }
    const std::uintptr_t list = *record + kList;
    const auto head = TryRead<std::uint32_t>(list + kHead);
    const auto size = TryRead<std::uint32_t>(list + kSize);
    if (!head || *head == 0 || !size || *size == 0)
    {
        return found;
    }
    auto node = TryRead<std::uint32_t>(*head);
    for (std::uint32_t i = 0; i < std::min(*size, kMostNodes) && node && *node != 0 && *node != *head; ++i)
    {
        if (const auto at = TryRead<std::uint32_t>(*node + kValue); at && *at != 0)
        {
            SubAction sub;
            sub.type = TryRead<std::uint32_t>(*at).value_or(0);
            sub.delayMs = TryRead<std::int32_t>(*at + kDelay).value_or(0);
            sub.durationMs = TryRead<std::int32_t>(*at + kDuration).value_or(0);
            sub.elapsedMs = TryRead<std::int32_t>(*at + kElapsed).value_or(0);
            found.push_back(sub);
        }
        node = TryRead<std::uint32_t>(*node);
    }
    return found;
}

std::optional<Held> HeldFor(ObjectId creature)
{
    const std::vector<SubAction> left = SubActions(creature);
    if (left.empty())
    {
        // A command still current with no sub-action left holds the
        // creature by something else, its time not kept ahead: a
        // knockback's fall and rise after its sub-action 51 (seen in play
        // 2026-10-04).
        CClientExoApp *app = CClientExoApp::Get();
        const CCObject *object = app != nullptr ? app->GetObjectById(creature) : nullptr;
        if (object != nullptr && object->Type() == core::ObjectType::Creature && object->CurrentCommand() != nullptr)
        {
            return Held{0, false};
        }
        return std::nullopt;
    }
    Held held;
    std::int64_t ms = 0;
    for (const SubAction &sub : left)
    {
        ms += std::max(sub.delayMs, 0);
        if (sub.durationMs > 0 && sub.elapsedMs < sub.durationMs)
        {
            ms += sub.durationMs - sub.elapsedMs;
        }
        else
        {
            // Untimed, or timed and its time spent yet still on the list:
            // its handler waits on something the engine does not time
            // ahead (a knockback's 51 at 1200 of 1200 ms, seen in play).
            held.known = false;
        }
    }
    held.seconds = static_cast<float>(ms) / 1000.0F;
    return held;
}

// Notes on a hold: read 2026-10-24 (DAOLib #257). An effect that holds its target queues the
// ExecuteEffect command on it (CCObject::QueueExecuteEffect, 0x00695F20; type 32, at the front),
// which keeps no effect: its start (0x00742B70) adds sub-action 51, whose handler (0x007AAC10)
// takes each tick the effect at the head of the creature's effects holder (+0x158, whose first
// word is its effect of the highest priority) and shows it, a stronger one arriving in its place.
// So the hold's effect is that head, matched by its id (CGameEffect +0x20) to dao::effects::On.
// Seen in play, 2026-10-24, in Redcliffe's siege (examples/moments): a knockdown staged on a
// walking corpse came as effect type 1014 from Lyna, held 1.37 s; each death came as a hold too,
// by the death effect (7) from its killer, 2.1 s; a knight's knockback (1027) as at least 0 s,
// untimed; and some holds came with no effect left, the command outliving it.

namespace
{

constexpr std::size_t kEffectsHead = 0x158; // in a creature: its effects holder, whose first word is the top effect
constexpr std::size_t kEffectId = 0x20;     // in an effect: its id, CGameEffect::Id

std::mutex g_holdMutex;
std::vector<std::function<void(ObjectId, const Hold &)>> g_holdObservers;
std::vector<ObjectId> g_starting; // the creatures whose effect command started this frame

void FireHolds()
{
    std::vector<ObjectId> starting;
    std::vector<std::function<void(ObjectId, const Hold &)>> observers;
    {
        const std::lock_guard lock(g_holdMutex);
        starting.swap(g_starting);
        observers = g_holdObservers;
    }
    for (const ObjectId creature : starting)
    {
        if (const auto hold = HoldOf(creature))
        {
            for (const auto &observer : observers)
            {
                detail::Guard("a hold callback", [&] { observer(creature, *hold); });
            }
        }
    }
}

} // namespace

std::optional<Hold> HoldOf(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    const CCObject *object = app != nullptr ? app->GetObjectById(creature) : nullptr;
    const Command *command =
        object != nullptr && object->Type() == core::ObjectType::Creature ? object->CurrentCommand() : nullptr;
    if (command == nullptr || command->Type() != core::CommandType::ExecuteEffect)
    {
        return std::nullopt;
    }
    Hold hold;
    hold.timing = HeldFor(creature).value_or(Held{0, false});
    const auto at = reinterpret_cast<std::uintptr_t>(object);
    const auto holder = TryRead<std::uintptr_t>(at + kEffectsHead).value_or(0);
    const auto top = holder != 0 ? TryRead<std::uintptr_t>(holder).value_or(0) : 0;
    const auto id = top != 0 ? TryRead<std::uint32_t>(top + kEffectId) : std::nullopt;
    if (id)
    {
        for (const effects::Standing &effect : effects::On(creature))
        {
            if (effect.id == *id)
            {
                hold.effect = effect;
                break;
            }
        }
    }
    return hold;
}

void OnHold(std::function<void(ObjectId creature, const Hold &hold)> observer)
{
    {
        const std::lock_guard lock(g_holdMutex);
        g_holdObservers.push_back(std::move(observer));
    }
    static std::once_flag once;
    std::call_once(once, [] {
        commands::OnStart([](ObjectId object, const Command &command) {
            if (command.Type() == core::CommandType::ExecuteEffect)
            {
                const std::lock_guard lock(g_holdMutex);
                g_starting.push_back(object);
            }
        });
        OnFrame(&FireHolds);
    });
}

} // namespace dao::commands
