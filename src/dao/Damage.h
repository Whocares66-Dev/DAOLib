// A creature's weapon damage per hand, for any creature: the game's own estimate, the one its
// character record shows for the party. The game's script works it out, so a plugin keeps no copy
// of the formula, and a mod that changes the script changes the figure.
//
//   if (const auto damage = dao::damage::PerHand(creature))
//       spdlog::info("{:.1f} main hand, {:.1f} off hand", damage->mainHand, damage->offHand.value_or(0));
//
// Call it on the game thread.
#pragma once

#include "dao/ObjectId.h"

#include <optional>

namespace dao::damage
{

// A creature's weapon damage per hand.
struct HandDamage
{
    float mainHand = 0; // the main hand's weapon or, unarmed, the creature's fists
    // The off hand's weapon. nullopt when it holds no weapon: empty, a shield, or not used for
    // dual wielding.
    std::optional<float> offHand;
};

// Works out a creature's damage per hand with the game's own script and returns it.
// @return the damage, or nullopt if `creature` is not a creature, the script commands are not
// ready, or the call is not on the game thread
//
// The figure is a steady estimate: an average hit scaled by attack speed, with the creature's
// rank, attributes and damage bonus counted, and no dice, armour or resistance. It also rewrites
// the creature's properties 50 and 49, which the character record reads and no script reads.
std::optional<HandDamage> PerHand(ObjectId creature);

} // namespace dao::damage
