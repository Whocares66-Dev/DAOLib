// A creature's moments as the engine has them: its death and revival on the frame its health
// crosses 0, and the frame its spawn script has set it up. For plugins that would otherwise poll
// each frame or wait out the scripts' late EVENT_TYPE_DEATH.
//
//   dao::creatures::OnDeath([](dao::ObjectId creature, dao::ObjectId killer) {
//       spdlog::info("{:#x} killed by {:#x}", creature, killer);
//   });
#pragma once

#include "dao/ObjectId.h"

#include <functional>

namespace dao::creatures
{

// Calls `observer` on the frame a creature dies: its health falls from above 0 to 0 or below,
// by a blow, a script's SetCurrentHealth, a spawn set dead or anything else. It comes before the
// scripts' EVENT_TYPE_DEATH, which waits for the death to play out.
// @param observer receives the creature and its killer: the death effect's creator, which is
// the creature itself when no one killed it (a script's SetCurrentHealth(0)), or kInvalidObject
// if it has no death effect
//
// Callbacks run on the game thread, inside the engine's change of the health: they can read the
// creature, but should not kill, revive or destroy anything.
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

} // namespace dao::creatures
