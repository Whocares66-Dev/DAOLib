// The clock temporary effects run on: buffs, debuffs, damage and healing
// over time, timed summons, lasting areas. A plugin can hold it still, move
// it on by a jump, or decide per effect how fast it runs.
//
//   dao::effects::Hold(true);       // the player's turn: nothing wears off
//   ...
//   dao::effects::Advance(6.0f);    // the round is over: six seconds more
//   dao::effects::Hold(false);
//
// Read 2026-09-30 (dao/Addresses.h, "How a temporary effect runs out").
// Every temporary effect on every object ages through one engine function,
// AddElapsed, each time its owner updates, by the milliseconds since the
// owner's last update; the effect ends once its whole duration has
// elapsed. This hooks AddElapsed, so the engine's own check still ends an
// effect, on its owner's next update after its time runs out. An effect's
// own figures are CGameEffect::Duration, Remaining and SetRemaining
// (dao/Structures.h).
//
// What this does not reach: regeneration of health, mana and stamina,
// which dao/Clock.h holds and advances with the effects, and anything a
// script times itself with DelayEvent. Only temporary effects age; permanent and
// instant ones are not affected. Not yet seen in play.
#pragma once

#include "dao/Structures.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace dao::effects
{

// Stop every temporary effect's time (true), or let it run again (false).
// Held effects neither wear off nor end.
void Hold(bool held);
bool Held();

// Age every temporary effect by `seconds` more, each on its owner's next
// update, once; an effect that runs out ends then. Works while held: the
// way to spend a round's worth of time on effects at once. Effects applied
// after the call are not aged by it.
void Advance(float seconds);

// Decide per effect how much it ages: called with each temporary effect
// and the milliseconds the engine would age it by, and returns the
// milliseconds to age it by instead. Runs on the game's thread, very
// often: keep it quick. After Hold and Advance, which it sees the result
// of.
void OnAgeing(std::function<std::int32_t(const CGameEffect &effect, std::int32_t ms)> ageing);

// An object's effects as they stand, with the time each has left: what the
// scripts' GetEffects lists (type, ability, creator), and the duration and
// time left that no script command gives, read from each effect
// (CGameEffect::Duration, Remaining). A snapshot, taken from the copies the
// command hands back: changing one changes nothing. From the frame's thread,
// since it calls a script command. Not yet seen in play.
struct Standing
{
    std::uint32_t id = 0;
    script::EffectType type = script::EffectType::Invalid;
    script::EffectDurationType durationType = script::EffectDurationType::Invalid;
    std::int32_t ability = 0;          // the ability that applied it, 0 for none
    ObjectId creator = kInvalidObject; // who applied it
    float duration = 0;                // seconds; 0 for an effect that is not temporary
    float remaining = 0;               // seconds; the same
};
std::vector<Standing> On(ObjectId object);

} // namespace dao::effects
