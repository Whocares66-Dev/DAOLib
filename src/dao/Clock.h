// The game's clock for what wears off and what comes back: temporary
// effects (buffs, debuffs, damage and healing over time, timed summons,
// lasting areas), the regeneration of health, mana and stamina, and the
// script timers that pace effects (a damage over time's ticks, a lasting
// area's pulses: dao/Timers.h). A plugin that measures a fight in turns
// rather than seconds holds it, and spends a round's worth at a time.
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
// Timers, 2026-10-02 (dao/Timers.h): held, an effect did not wear off
// while the script events that make it hurt went on in real time, so a
// trap's poison (a damage over time each 1.5 s) hit a turn-based party for
// as long as the fight lasted, seen by the user in the Dalish origin's
// Elven Ruins. Its ticks, and a lasting area's pulses, now wait with the
// clock. Not reached: other timers a script keeps itself (dao/Timers.h
// says which kinds are held), and cooldowns, which have their own script
// commands (GetRemainingCooldown, SetCooldown). On the game's thread. Not
// yet seen in play.
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

#include "dao/ObjectId.h"

namespace dao::clock
{

// Stop effects wearing off, regeneration and the timers that pace effects
// (true), or let them run (false).
void Hold(bool held);
bool Held();

// Spend `seconds` of the clock on effects, regeneration and timers at
// once: each effect ages, and each creature regenerates, by that much more
// on its next update, and the timers that fall due in them come in order.
// Works while held. Objects on clocks of their own are passed by.
void Advance(float seconds);

// A creature's own time, for a fight measured per creature (each one's
// clock moving as its turn is spent): OwnClock(id) puts the effects on it,
// its regeneration and the timers sent to it (its damage over time's
// ticks) on a clock of its own, which the shared Advance passes by;
// Advance(id, seconds) moves that one object's three, in steps as small as
// wanted, the fractions of a millisecond kept. Hold(false) gives every
// object back to the shared clock.
//
//   dao::clock::Hold(true);
//   dao::clock::OwnClock(creature);          // each combatant
//   dao::clock::Advance(creature, 0.016f);   // as its turn is spent
//   dao::clock::Advance(4.0f);               // each round: areas, bystanders
//   dao::clock::Hold(false);
//
// Read 2026-10-02: an effect is aged by its owner's clock (the object it
// is on, dao/Effects.h), regeneration by its creature's (CCCreature::Update
// is UpdateRegeneration's one caller), a timer by its target's
// (dao/Timers.h). Not yet seen in play.
void OwnClock(ObjectId object, bool own = true);
void Advance(ObjectId object, float seconds);

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
