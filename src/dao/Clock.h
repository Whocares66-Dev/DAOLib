// Controls how fast time passes for effects and regeneration, and how fast the whole game runs.
//
// Hold stops temporary effects from wearing off (buffs, debuffs, damage over time, timed
// summons, lasting areas of effect), stops health, mana and stamina from regenerating, and holds
// the script timers that pace effects (dao/Timers.h). Advance then moves all of them on by an
// amount of time you choose, so a plugin can measure a fight in turns instead of seconds.
// SetSpeed changes how fast the whole game runs.
//
//   dao::clock::Hold(true);                 // a turn-based fight begins
//   dao::clock::Advance(roundSeconds);      // each round, however long it took
//   dao::clock::Hold(false);                // the fight is over
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>

namespace dao::clock
{

// Stops effects from wearing off and creatures from regenerating, and holds the timers that pace
// effects; or lets all of them run again.
// @param held true to stop them, false to let them run
//
// While the clock is held, only Advance moves them on. Cooldowns still run, and so do the timers
// that dao/Timers.h does not hold. A creature held by an effect, such as a pin or a knockdown,
// stays held until the clock is advanced past the effect or the effect is removed. Hold(false)
// returns every object given to OwnClock to the shared clock.
void Hold(bool held);

// Returns whether the clock is held.
bool Held();

// Moves effects, regeneration and timers on by an amount of time, in one step.
// @param seconds the time to add, in game seconds
//
// On its next update, each effect ages and each creature regenerates by that much extra time,
// and the timers that fall due fire in order. This works while the clock is held. Objects on
// their own clocks (OwnClock) are left out.
void Advance(float seconds);

// Puts a creature's effects, regeneration and timers on a clock of their own, or back on the
// shared clock.
// @param object the creature, or any other object
// @param own true for its own clock, false for the shared one
//
// Advance then leaves the object out, and AdvanceObject moves its clock on. Hold(false) returns
// every object to the shared clock.
//
//   dao::clock::Hold(true);
//   dao::clock::OwnClock(creature);                // for each combatant
//   dao::clock::AdvanceObject(creature, 0.016f);   // as its turn is spent
//   dao::clock::Advance(4.0f);                     // each round, for areas and bystanders
//   dao::clock::Hold(false);
void OwnClock(ObjectId object, bool own = true);

// Moves one object's own clock (see OwnClock) on.
// @param seconds the time to add, in game seconds; fractions of a millisecond carry over
void AdvanceObject(ObjectId object, float seconds);

// Sets how fast the game runs, using the same time scale as the game's own slow motion.
// @param scale the speed: 1 for normal, 0.5 for half speed, 2 for double speed
// @param seconds how long to ease from the current speed to the new one; 0 to change at once
// @return false, with nothing changed, if the game is not up yet, if `scale` is 0 or less, or if
//  `seconds` is negative
//
// Call it on the game thread. It changes the speed of the game's logic and animation, but not of
// the Flash interface. All plugins share one speed, and the last one set wins.
bool SetSpeed(float scale, float seconds = 0.0F);

// Returns the speed last set, or 1 if none has been set.
float Speed();

// The world's clock at a moment: the game's calendar day and the time into it.
struct WorldTime
{
    std::uint32_t day = 0;       // the calendar's day number, as a save keeps it
    std::uint32_t ms = 0;        // milliseconds into the day, below a day's length
    double seconds = 0;          // day and ms together, in seconds
    std::uint32_t dayLength = 0; // a day's length in seconds, 7200 in vanilla
};

// Returns the world's clock now, the one a save writes and a load restores, or nullopt before the
// game is up.
//
// It runs at the game's speed (SetSpeed), and a load sets it to the save's time. Call it on the
// game thread.
std::optional<WorldTime> Now();

} // namespace dao::clock
