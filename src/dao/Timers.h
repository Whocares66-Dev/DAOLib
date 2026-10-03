// The timers the game's scripts keep themselves, on a plugin's clock: the
// script events they send an object after a delay (DelayEvent), above all
// the ones that pace effects. A damage-over-time effect hurts its creature
// once each 1.5 s by an event the creature's script re-arms each time
// (EVENT_TYPE_DOT_TICK, effect_dot2_h), and a lasting area of effect (a
// trap's gas, a spell's cloud) pulses by an EVENT_TYPE_AOE_HEARTBEAT its
// script re-arms each pulse, at an interval of its own. Held, those wait;
// advanced, those that fall due come in order, each re-armed from the time
// it came due, so a round's four seconds give a 1.5 s tick at 1.5 and 3.0
// and the next waits for 4.5. dao/Clock.h holds and advances this with
// effects and regeneration; a plugin that measures a fight in rounds wants
// all three.
//
//   dao::timers::Hold(true);       // a turn-based fight begins
//   dao::timers::Advance(4.0f);    // each round
//   dao::timers::Hold(false);      // what is still waiting is queued again
//
// Read 2026-10-02 (dao/Addresses.h, "The queue as it runs"). Every delayed
// event enters the engine's queue through one function with its delay,
// and leaves it when the world clock passes its time. While held, a script
// event of a held kind sent with a delay is kept here instead, due that
// delay from now on this clock; one already in the queue when the hold
// began is taken out as it comes due, due as long after the hold began as
// it was then. Each update of the queue first delivers what has come due
// here, earliest first, the clock moved to each one's time as it goes, as
// the engine itself would deliver it (DeliverNow).
//
// Each object the events are sent to has its clock here: a damage over
// time's ticks are sent to its victim, an area's pulses to the area. The
// shared Advance moves them all; OwnClock takes an object off it, to move
// by Advance(object, seconds) alone (a creature's own time in a fight
// measured per creature). A clock's time counts from its first event: an
// object advanced with nothing held for it is not owed that time. Released, what is left
// goes back into the queue with the time it has left. The queue's own
// clearing (a game loaded) drops what is held here too.
//
// Events of other kinds, and any sent with no delay, are left alone: the
// scripts sequence much by DelayEvent (plots, cutscenes, the first tick of
// a damage over time, sent "next frame" with 0), and holding those would
// stop the game. An area's first pulse is not a delayed event: its update
// runs the script with it once, at once. The world's own time still runs:
// creatures' commands, the AI, animation. What is held here is not in the
// queue a save writes; the game refuses to save in combat, where a
// turn-based fight is, and a plugin that holds outside one should let go
// before a save. Seen in play 2026-10-02 by the turn-based mod: a Walking
// Bomb's ticks came on the ogre's own turns, and the Elven Ruins' toxic gas
// poisoned its victim through 8 rounds of the shared clock and no more.
#pragma once

#include "dao/ObjectId.h"
#include "dao/generated/ScriptConstants.h"

#include <cstddef>

namespace dao::timers
{

// The damage-over-time tick, a script's own event type (events_h.nss:
// EVENT_TYPE_DOT_TICK, 1010), not one of the engine's.
inline constexpr script::EventType kDotTick = static_cast<script::EventType>(1010);

// Hold the timers of the kinds held (true), or let them run again, giving
// back what waits with the time it has left (false). On the game's thread.
void Hold(bool held);
bool Held();

// Spend `seconds` on what is held for every object not on a clock of its
// own: what falls due in them is delivered on the queue's next update, in
// order. Nothing while not held.
void Advance(float seconds);

// Put an object on a clock of its own (true), which the shared Advance
// passes by, or back on the shared one (false). While held; Hold(false)
// gives every object back.
void OwnClock(ObjectId object, bool own = true);

// Spend `seconds` on what is held for one object, whether or not it is on
// its own clock: in small steps if wanted, fractions kept. Nothing while
// not held.
void Advance(ObjectId object, float seconds);

// Which kinds of script event are held: kDotTick and AOE_HEARTBEAT unless
// a plugin says otherwise; a plugin's own timers may be added. Takes
// effect for events sent after it.
void HoldKind(script::EventType type, bool held = true);

// How many events wait here now, in all and for one object.
std::size_t Waiting();
std::size_t Waiting(ObjectId object);

} // namespace dao::timers
