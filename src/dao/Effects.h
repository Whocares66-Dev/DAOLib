// Controls the clock that temporary effects run on: buffs, debuffs, damage and healing over
// time, timed summons and lasting areas of effect.
//
// A plugin can stop that clock, move it on in jumps for every object or for one, or decide for
// each effect how fast it ages. dao/Clock.h drives it together with regeneration and timers.
// `On` lists an object's effects with the time each has left.
//
//   dao::effects::Hold(true);       // the player's turn: nothing wears off
//   ...
//   dao::effects::Advance(6.0f);    // the round is over: six seconds more
//   dao::effects::Hold(false);
#pragma once

#include "dao/Structures.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace dao::effects
{

// Stops the time of every temporary effect, or lets it run again.
// @param held true to stop it; false to let it run and return every object to the shared clock
//
// While held, effects neither wear off nor end. Permanent and instant effects are not affected.
void Hold(bool held);

// Returns whether the effects' clock is stopped.
bool Held();

// Ages every temporary effect by an extra amount of time, once, on its owner's next update.
// @param seconds the time to add, in game seconds
//
// This works while held. An effect that runs out ends on that update. Effects applied after the
// call, and effects on objects with their own clock (OwnClock), are not aged by it.
void Advance(float seconds);

// Puts an object's effects on a clock of their own, or back on the shared one.
// @param owner the object the effects are on, not the one that applied them
// @param own true for its own clock, which Advance leaves out; false for the shared one
void OwnClock(ObjectId owner, bool own = true);

// Ages the effects on one object by an extra amount of time, whether or not the object is on its
// own clock.
// @param owner the object the effects are on, not the one that applied them
// @param seconds the time to add, in game seconds; fractions of a millisecond carry over
void AdvanceObject(ObjectId owner, float seconds);

// Adds a callback that decides how much each temporary effect ages.
// @param ageing called with the effect and the milliseconds it would age by once Hold and Advance
//  are taken into account; returns the milliseconds to age it by instead
//
// It runs on the game thread for every temporary effect on every update, so keep it quick.
void OnAgeing(std::function<std::int32_t(const CGameEffect &effect, std::int32_t ms)> ageing);

// One effect on an object, as listed by On.
struct Standing
{
    std::uint32_t id = 0;
    script::EffectType type = script::EffectType::Invalid;
    script::EffectDurationType durationType = script::EffectDurationType::Invalid;
    std::int32_t ability = 0;          // the ability that applied it, 0 for none
    ObjectId creator = kInvalidObject; // the object that applied it
    float duration = 0;                // seconds; 0 for an effect that is not temporary
    float remaining = 0;               // seconds left; 0 for an effect that is not temporary
};

// Returns the effects on an object, with each one's duration and time left.
// @return a copy, so changing it changes nothing in the game; empty if the object has no effects
//  or does not exist
//
// Call it on the game thread (dao/Frame.h), since it calls the GetEffects script command.
std::vector<Standing> On(ObjectId object);

} // namespace dao::effects
