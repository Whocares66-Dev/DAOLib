// A creature's moments as the engine has them: its death and revival on the frame its health
// crosses 0, and the frame its spawn script has set it up. For plugins that would otherwise poll
// each frame or wait out the scripts' late EVENT_TYPE_DEATH.
//
//   dao::creatures::OnDeath([](dao::ObjectId creature, dao::ObjectId killer) {
//       spdlog::info("{:#x} killed by {:#x}", creature, killer);
//   });
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace dao::creatures
{

// Calls `observer` on the frame a creature dies: its health falls from above 0 to 0 or below,
// by a blow, a script's SetCurrentHealth, a spawn set dead or anything else. It comes before the
// scripts' EVENT_TYPE_DEATH, which waits for the death to play out.
// @param observer receives the creature and its killer: the death effect's creator, which is
// the creature itself when no one killed it (a script's SetCurrentHealth(0)), or kInvalidObject
// if it has no death effect
//
// Not while the game is loading: a creature a save restores dead, or an area places dead, does
// not die again. Callbacks run on the game thread, inside the engine's change of the health:
// they can read the creature, but should not kill, revive or destroy anything.
void OnDeath(std::function<void(ObjectId creature, ObjectId killer)> observer);

// Calls `observer` on the frame a dead creature comes back: its health rises from 0 or below to
// above 0. Otherwise like OnDeath.
void OnRevive(std::function<void(ObjectId creature)> observer);

// Calls `observer` once for each creature made, on the frame its spawn script has set it up:
// its level, attributes, abilities, health and mana are then the game's.
//
// A creature made by the scripts' CreateObject, by dao::objects (dao/ObjectIds.h) or placed in an
// area as it first loads gets it. One restored from a save does not, as it is not spawned again
// (its stats come with it), and neither does one whose template has no script. Callbacks run on
// the game thread.
void OnReady(std::function<void(ObjectId creature)> observer);

// A creature's movement as its movement controller has it now.
struct Movement
{
    float speed = 0;      // metres a second it moves at now, eased toward `target`
    float target = 0;     // metres a second it eases toward, by its state and the effects on it
    float multiplier = 1; // the product of its speed effects (Haste, slows): what the table's speeds are scaled by
    float offset = 0;     // added to the table's speed before the multiplier
    // 0 idle, 1 walking, 2 running, 3 stopping, 4 turning on the spot
    std::int32_t state = 0;
    bool combat = false; // in combat the table's combat speed is taken, walking or running
};

// Returns a creature's movement now: its speed, the speed it eases toward and its effects' multiplier.
// @return nullopt if there is no such creature. Call it on the game thread.
std::optional<Movement> MovementOf(ObjectId creature);
} // namespace dao::creatures
