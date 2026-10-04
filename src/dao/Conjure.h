// A spell held in its conjure: the engine's own cast, its caster drawing in
// the spell with the game's animation, moved on only by the time a plugin
// gives it. A turn-based mode hands a spell to the engine at once and
// feeds its conjure as the caster's turn pays for it, so that everything
// the game does to a conjuring caster it still does: the installed
// rules_core's damage rule interrupts it (shipped or fixed), a knockdown
// ends it, and the cost is taken only when the conjure completes.
//
//   dao::commands::HoldConjure(caster);           // as the spell is given
//   dao::commands::FeedConjure(caster, 0.016f);   // as its time is paid
//   dao::commands::HoldConjure(caster, false);    // the hold let go
//
// Read 2026-10-02 and seen in play the same day by the probe's conjure
// watch (dev/TURN_BASED.md, "A command's timed phases and the creature's
// clock"): a UseAbility's conjure is its sub-action of type 7, timed (its
// duration the ability's conjure, 3,866 ms for Chain Lightning), its
// elapsed time counting the caster's own milliseconds, done when it
// reaches the duration; the cost is taken at the type 8 after it. Each
// update, CSSubActionHandler::Execute runs it; for a held caster this sets
// the elapsed time first to what has been fed, so the conjure runs as
// ever, animation and IsConjuring with it, and completes only once fed its
// whole duration. The cast (37) and what follows then run in real time.
// The hold seen in play the same day, the turn-based mod's play test 33
// (DAOTurnBased dev/PLAYTEST-2026-10-02-33.md): a caster's Chain Lightning
// held conjuring through two spiders' turns, the lightning round her
// frame after frame and the game's own action label up; an ogre's ability
// cleared it before its time was paid; in the re-check, fed its whole
// time, it was let go and the bolt flew at the start of her next turn.
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>

namespace dao::commands
{

// Hold a caster's conjures (true): from now each conjure it runs moves only
// by what FeedConjure gives it, starting from where it stood. Let go
// (false), the one under way runs on in real time from where it stood.
// What else the caster does, moving into range, the cast, is not held.
void HoldConjure(ObjectId caster, bool hold = true);
bool ConjureHeld(ObjectId caster);

// Give a held caster's conjure `seconds` more, in steps as small as wanted,
// the fractions of a millisecond kept; never past its duration. Seconds
// given while no conjure is running wait for the next one.
void FeedConjure(ObjectId caster, float seconds);

// A caster's conjure under way, held or not: how far along and how long.
// Nothing if it is not conjuring, or before the first update after this
// part of the library is first called. From the frame's thread.
struct Conjuring
{
    float elapsed = 0;  // seconds
    float duration = 0; // seconds
};
std::optional<Conjuring> Conjure(ObjectId caster);

// The type of the sub-action a creature's current command is at, as the
// engine's IsConjuring answers it (dev/TURN_BASED.md, "A command's timed
// phases"): the type of the first sub-action left on its current command's
// list (IsConjuring reads it through 0x00697BF0), the one running or about
// to run, so never 0 between two of them; 0 only when the creature has no
// current command, or none of its sub-actions is left, or it is no
// creature. A UseAbility's run 15, 39, 50,
// 7 (the conjure), 8, 37 (seen in play). An attack's, seen in play
// 2026-10-04 (the turn-based mod, Zevran's ambush, 160 attacks) and read
// in their handlers: an approach, 4 for melee or 15 for a ranged weapon
// (dao/Attack.h), the only ones in which the attacker walks; then the
// attack's own. A melee blow is 1, which waits until the attacker faces
// its target within 60 degrees and sees it, then sends EVENT_TYPE_COMMAND_
// PENDING for the scripts to resolve it. A shot runs 39 (turn to face,
// 0x007A69E0), 37 (a timed animation, 0x007A4390), 6 (the aim, 0x007A2410:
// a timed wait that holds while the target is out of sight or more than
// 67.5 degrees off the facing) and 2 (the shot, 0x007A6490, which sends
// COMMAND_PENDING), with 37 again; not every shot runs every one. The
// command's 50, which puts the attack's own in, runs within the same
// update (several sub-actions run in one while its time lasts,
// 0x0073CB60), so it is not seen between frames. 0 between the last of
// them and the command's end, seen; whether it can be 0 for a frame
// before the command's step puts its sub-actions in is not read. On the
// game's thread.
std::uint32_t RunningSubAction(ObjectId creature);

} // namespace dao::commands
