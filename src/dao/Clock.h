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
// clock is advanced past it or the effect is removed. The same with an
// ability that is a timed effect on its user and one on its target (a
// giant spider's Overwhelm, 2026-10-01): held, it never ends. The mod now
// advances the clock in quarter-second steps while the acting creature is
// at such a command after its own action, and takes that off the round's
// worth.
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

// How fast the game runs: 1 as it plays, 0.5 half speed, 2 twice; over
// `seconds` if given, eased there from the speed it had, else at once.
//
// The game's own time scale, read 2026-09-30, what its slow motion uses.
// The application keeps it at +0x2C8 (SetTimeScale, 0x005D81C0, sets it,
// or eases it there over a time), and its frame update (0x005D7560) hands
// it on each frame: to the world timer (EC::WorldTimer, the application's
// +0xD4, its scale at +0x54), which the game's logic runs on (each
// object's timer, so the AI, commands, effects and regeneration), and to
// the scene (0x008107F0, its slot 6), which animation plays by. Writing the
// world timer's scale directly, as this did before, lasted a frame: the
// update overwrites it from +0x2C8 each frame (in play, 2026-09-30, a
// creature's own time stayed at 1000 ms a wall second "at speed 2", and its
// pace with it). Seen in play this way, 2026-09-30 (examples/speed's
// meter): at 2 a creature's own time ran 2000 to 2046 ms a wall second and
// the hero ran 8 to 9 m/s against 4.5 to 4.7 at 1, a fight taking 32 s
// where one had taken 51; the frame rate did not change. The Flash UI
// keeps its own clock (Scaleform's GTimer) and is not changed.
//
// False, and nothing done, before the application is up or for a speed of
// 0 or less. Called on the game's thread. The speed is the game's one: two
// plugins setting it take turns, the last one set winning.
bool SetSpeed(float scale, float seconds = 0.0F);
// The speed set: the application's +0x2C8, 1 when none is.
float Speed();

} // namespace dao::clock
