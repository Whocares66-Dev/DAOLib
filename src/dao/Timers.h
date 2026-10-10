// Holds back the delayed script events that pace effects, and moves their time on when you
// choose.
//
// A damage-over-time effect ticks every 1.5 seconds, and a lasting area of effect, such as a
// trap's gas or a spell's cloud, pulses. Both are driven by script events sent with a delay
// (DelayEvent). Hold keeps those events waiting, and Advance delivers the ones that fall due, in
// order. dao/Clock.h holds and advances these timers together with effects and regeneration,
// and is usually what a plugin wants.
//
//   dao::timers::Hold(true);       // a turn-based fight begins
//   dao::timers::Advance(4.0f);    // each round
//   dao::timers::Hold(false);      // events still waiting go back to the game's queue
#pragma once

#include "dao/ObjectId.h"
#include "dao/generated/ScriptConstants.h"

#include <cstddef>

namespace dao::timers
{

// Holds delayed script events of the held kinds, or lets them run again.
// @param held true to hold them; false to put the waiting events back in the game's queue, each
//  with the time it had left
//
// Call it on the game thread. Only events of the held kinds (see HoldKind and HoldFrom) that
// were sent with a delay are held. The rest of the world's time, such as commands, the AI and
// animation, still runs. Held events are not written to a save, so release them before the game
// saves.
void Hold(bool held);

// Returns whether the timers are held.
bool Held();

// Moves time on for the held events of every object that is not on its own clock.
// @param seconds the time to add, in game seconds
//
// The events that fall due are delivered in order at the event queue's next update. When a
// script sends its event again, the new delay counts from the time the old one came due, so four
// seconds give a 1.5-second tick at 1.5 and at 3.0. Does nothing while the timers are not held.
void Advance(float seconds);

// Puts an object's held events on a clock of their own, or back on the shared one.
// @param own true for its own clock, which Advance leaves out; false for the shared one
//
// Does nothing while the timers are not held. Hold(false) returns every object to the shared
// clock.
void OwnClock(ObjectId object, bool own = true);

// Moves time on for one object's held events, whether or not it is on its own clock.
// @param seconds the time to add, in game seconds; fractions carry over
//
// Does nothing while the timers are not held.
void AdvanceObject(ObjectId object, float seconds);

// Sets whether script events of one kind are held, whatever their delay.
// @param held true to hold the kind; false never to hold it, either by kind or by HoldFrom's
//  delay
//
// By default EventType::DotTick and AoeHeartbeat are held, and DelayedGmChange, which the
// scripts use to end a fight, is never held. A plugin may add the kinds its own timers use. The
// change applies to events sent after the call, and to those already queued when a hold begins.
void HoldKind(script::EventType type, bool held = true);

// Also holds every script event sent with a delay of at least `seconds`, whatever its kind.
// @param seconds the shortest delay to hold; 0 or less to hold none by delay (the default)
//
// Plot timers wait for seconds at a time, while the scripts also chain steps with delays of 0 to
// 0.01 seconds that must not wait. A value of 1 holds the first and not the second. Kinds
// excluded with HoldKind(type, false) stay excluded.
void HoldFrom(float seconds);

// Starts or stops logging every script event the game queues with a delay, held or not.
// @param on true to start, false to stop
//
// Each event is logged at info level when it is queued, with its type, target, sender, delay and
// the world clock, and again when it is delivered. Use it to find out what a script times with
// DelayEvent before deciding whether to hold that kind.
void Trace(bool on);

// Returns how many events are held in all.
std::size_t Waiting();

// Returns how many events are held for one object.
std::size_t WaitingFor(ObjectId object);

} // namespace dao::timers
