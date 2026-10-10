// Reads the steps of a creature's current command and how long each still takes. The game runs a
// command as a list of smaller steps, called sub-actions: an animation, a wait, an aim. Use it to
// tell how much longer a command the creature did not choose, such as an effect landing on it or
// getting back up, will keep it busy. It works for any effect.
//
//   if (const auto held = dao::commands::HeldFor(creature))
//       spdlog::info("held {:.2f} s more{}", held->seconds, held->known ? "" : " at least");
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace dao::commands
{

// One step of a command. Times are in milliseconds of the creature's own clock.
struct SubAction
{
    std::uint32_t type = 0;      // the kind of step, such as 37 for an animation
    std::int32_t delayMs = 0;    // time still to wait before the step runs
    std::int32_t durationMs = 0; // 0 if the step is untimed, or timed but not yet started
    std::int32_t elapsedMs = 0;  // how much of the duration has passed
};

// Returns the sub-actions left in the creature's current command, the running one first.
// @return an empty list if the creature has no command or no sub-actions left, or the ID is not
// a creature
//
// Call it on the game thread.
std::vector<SubAction> SubActions(ObjectId creature);

// How much longer a command will keep a creature busy.
struct Held
{
    float seconds = 0; // the delays and remaining timed durations, added up
    // False if `seconds` is only a lower bound: a remaining sub-action is untimed, or is timed and
    // used up but still running, or none are left but the command is still current.
    bool known = true;
};

// Estimates from its sub-actions how much longer the creature's current command will keep it
// busy.
// @return nullopt if the creature has no current command, or the ID is not a creature
//
// Call it on the game thread. If `known` is false, the creature is busy for at least `seconds`.
// For example, the fall and the getting up after a knockback are not timed in advance.
std::optional<Held> HeldFor(ObjectId creature);

} // namespace dao::commands
