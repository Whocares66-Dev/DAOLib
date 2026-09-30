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

} // namespace dao::clock
