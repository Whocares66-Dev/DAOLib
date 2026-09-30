// The game's clock for what wears off and what comes back: temporary
// effects (buffs, debuffs, damage and healing over time, timed summons,
// lasting areas) and the regeneration of health, mana and stamina. A
// plugin that measures a fight in turns rather than seconds holds it, and
// spends a round's worth at a time.
//
//   dao::clock::Hold(true);                 // a turn-based fight begins
//   dao::clock::Advance(roundSeconds);      // each round, however long it took
//   dao::clock::Hold(false);                // the fight is over
//
// Read 2026-09-30. Effects: dao/Effects.h, which this drives (and which
// also decides per effect). Regeneration: each creature's stats run
// UpdateRegeneration (0x0071B0E0, __thiscall (ms, bool in combat),
// `ret 8`) on its update, adding the milliseconds to an accumulator (the
// stats' +0x58); each 3000 ms out of combat, or 2000 in combat, every
// depletable property (health, mana or stamina) gains once what its
// regeneration property gives, the living only. Which property that is,
// is data: properties.gda's NonCombatEffect and CombatEffect columns
// (health 14 and 28, mana and stamina 29 and 30). This hooks
// UpdateRegeneration as dao/Effects.h hooks the effects' ageing, so a
// round's worth given at once regenerates as many times as it holds.
//
// Not reached: timers a script keeps itself (DelayEvent), and cooldowns,
// which have their own script commands (GetRemainingCooldown,
// SetCooldown). On the game's thread. Not yet seen in play.
//
// A consequence, seen by the turn-based mod in play, 2026-09-30: a
// creature under an effect that holds it (a pin, a knockdown) is kept busy
// by a command that ends when the effect does (ExecuteEffect, type 32,
// queued at the front as the effect lands). Held, the effect does not end,
// so neither does the command, and the creature does nothing until the
// clock is advanced past it or the effect is removed.
#pragma once

namespace dao::clock
{

// Stop effects wearing off and regeneration (true), or let both run
// (false).
void Hold(bool held);
bool Held();

// Spend `seconds` of the clock on effects and regeneration at once: each
// effect ages, and each creature regenerates, by that much more on its
// next update. Works while held.
void Advance(float seconds);

// How fast the game runs: 1 as it plays, 0.5 half speed, 2 twice. Read
// 2026-09-30; the game keeps two clocks, and this scales both.
// - The world timer (EC::WorldTimer, the application's +0xD4), which the
//   game's logic runs on: an object's timer (0x006FA1D0) asks it for the
//   world time, and CCCreature's update hands the difference to
//   regeneration and the effects. Its update (0x00A358F0) multiplies each
//   frame's seconds by a float at its +0x54 that the engine sets to 1 and
//   never changes. In play, 2026-09-30, setting it alone changed nothing
//   the player could see.
// - The frame's timer (an EC::ITimer at the Eclipse game object's +0xB4),
//   which the frame runs on: EclipseGame's update ticks it (0x004025C0),
//   keeps its seconds as the frame's (the game's +0xBC), and gives its
//   total to the scene (0x008107F0), what animation plays by. This hooks
//   the timer's update (EC::ITimer::Update, 0x004CB160) and scales that
//   timer's seconds, and the total with them.
// Not yet seen in play with both. The Flash UI keeps its own clock
// (Scaleform's GTimer) and is not slowed. Called on any thread; takes
// effect from the next frame.
void SetSpeed(float scale);
float Speed();

} // namespace dao::clock
