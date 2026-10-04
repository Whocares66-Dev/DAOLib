// A creature's current command as the engine runs it: the sub-actions left
// on it, each with the time it still takes where the engine keeps that
// ahead, so a plugin can say how long a command the creature did not
// choose (ExecuteEffect as an effect lands, a getting up) still holds it,
// for any effect and with no list of kinds (DAOLib issue #18).
//
//   if (const auto held = dao::commands::HeldFor(creature))
//       spdlog::info("held {:.2f} s more{}", held->seconds, held->known ? "" : " at least");
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
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace dao::commands
{

struct SubAction
{
    std::uint32_t type = 0;
    std::int32_t delayMs = 0;    // still to wait before it runs
    std::int32_t durationMs = 0; // 0 for an untimed one, or a timed one not yet begun
    std::int32_t elapsedMs = 0;
};

// The current command's sub-actions left, the running one first; empty for
// no command, none left, or no creature. On the game's thread.
std::vector<SubAction> SubActions(ObjectId creature);

struct Held
{
    float seconds = 0; // the delays and timed time left, summed
    // False where a sub-action left is untimed, or timed and spent but still
    // there, or none is left of a command still current: at least `seconds`.
    bool known = true;
};
// How long the current command still holds the creature, from its
// sub-actions; nothing for no command. On the game's thread.
std::optional<Held> HeldFor(ObjectId creature);

} // namespace dao::commands
