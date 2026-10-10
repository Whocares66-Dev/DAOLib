// Makes a spell's conjure, the wind-up before the cast, progress only as fast as a plugin feeds
// it time.
//
// Otherwise the game casts the spell as usual: the caster plays the conjure animation, damage
// can interrupt it, a knockdown ends it, and the spell's cost is taken only when the conjure
// completes. Only the conjure's progress is held, and FeedConjure moves it on. RunningSubAction
// tells which step of its current command a creature is at.
//
//   dao::commands::HoldConjure(caster);           // when the spell is ordered
//   dao::commands::FeedConjure(caster, 0.016f);   // as the caster's turn spends time
//   dao::commands::HoldConjure(caster, false);    // release the hold
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>

namespace dao::commands
{

// Makes a caster's conjures progress only through FeedConjure, or releases them.
// @param hold true to hold; false to let the current conjure run on in real time from where it is
//
// A conjure already under way is held where it is. Moving into range, the cast, and everything
// after the conjure still run in real time.
void HoldConjure(ObjectId caster, bool hold = true);

// Returns whether a caster's conjures are held.
bool ConjureHeld(ObjectId caster);

// Gives a held caster's conjure more time.
// @param seconds the time to give, in game seconds; fractions of a millisecond carry over, and 0
//  or less does nothing
//
// The conjure never runs past its duration. Time given while no conjure is running waits for
// the next one. Does nothing for a caster that is not held.
void FeedConjure(ObjectId caster, float seconds);

// How far along a conjure is.
struct Conjuring
{
    float elapsed = 0;  // seconds so far
    float duration = 0; // seconds in all
};

// Returns how far along a caster's conjure is, held or not.
// @return nullopt if the caster is not conjuring or is not a creature, or if no game update has
//  run since this part of the library was first used
//
// Call it on the game thread (dao/Frame.h).
std::optional<Conjuring> Conjure(ObjectId caster);

// Returns the type of the sub-action that a creature's current command is running or is about to
// run. The game carries out a command, such as an attack, as a series of sub-actions: approach,
// aim, blow, and so on. This is the same value the IsConjuring script command returns.
// @return the type; 0 if the creature has no current command, has no sub-action left in it, or
//  is not a creature
//
// The known types:
// - 1: a melee blow
// - 2: a shot
// - 4: the approach for a melee attack (dao/Attack.h)
// - 6: a ranged attack's aim
// - 7: a spell's conjure
// - 8: an ability's cost being taken, after the conjure
// - 15: the approach for a ranged attack or an ability
// - 37: a timed animation, such as a cast
// - 39: turning to face the target
//
// The creature walks only in 4 and 15. Call it on the game thread.
std::uint32_t RunningSubAction(ObjectId creature);

} // namespace dao::commands
