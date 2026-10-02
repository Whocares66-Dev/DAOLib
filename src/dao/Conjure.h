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
// The hold itself not yet seen in play.
#pragma once

#include "dao/ObjectId.h"

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

} // namespace dao::commands
