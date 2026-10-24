// Reads the steps of a creature's current command and how long each still takes. The game runs a
// command as a list of smaller steps, called sub-actions: an animation, a wait, an aim. Use it to
// tell how much longer a command the creature did not choose, such as an effect landing on it or
// getting back up, will keep it busy. It works for any effect.
//
//   if (const auto held = dao::commands::HeldFor(creature))
//       spdlog::info("held {:.2f} s more{}", held->seconds, held->known ? "" : " at least");
#pragma once

#include "dao/Effects.h"
#include "dao/ObjectId.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace dao::commands
{

// The kinds of sub-action a plugin meets most, as SubAction::type has them.
namespace subaction
{
inline constexpr std::uint32_t kApproachMelee = 4;   // an attack's walk to its place on the target's ring
inline constexpr std::uint32_t kApproachRanged = 15; // an attack's walk into its range
inline constexpr std::uint32_t kAnimation = 37;      // a timed animation step
inline constexpr std::uint32_t kTurnToFace = 39;     // the turn to face the target
inline constexpr std::uint32_t kExecuteEffect = 51;  // an effect's hold on the creature it is on
} // namespace subaction

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

// An effect's hold on a creature: the effect its current command shows, and how much longer it
// keeps the creature busy.
struct Hold
{
    // The effect, as dao::effects::On has it; none once the effect is gone but the command is still
    // current (a dying creature's, or the end of a knockback).
    std::optional<effects::Standing> effect;
    Held timing; // as HeldFor, the fall and the getting up included where they are timed
};

// Returns the hold a creature is in: whether its current command is an effect's (the engine's
// ExecuteEffect, which a knockdown, stun, paralysis or sleep queues on it), and which effect.
// @return nullopt if the creature's current command is not an effect's, or it is not a creature
//
// The effect is the one the command's step shows now: the creature's effect of the highest
// priority, which a stronger effect arriving replaces. Call it on the game thread.
std::optional<Hold> HoldOf(ObjectId creature);

// Calls `observer` when an effect's command takes hold of a creature, on the frame after it starts,
// with the hold as HoldOf has it then. Callbacks run on the game thread.
void OnHold(std::function<void(ObjectId creature, const Hold &hold)> observer);

} // namespace dao::commands
