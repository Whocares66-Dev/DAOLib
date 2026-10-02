// The probe's conjure watch: what a spell's command does, sub-action by
// sub-action, while its caster conjures (dev/TURN_BASED.md, "A command's
// timed phases and the creature's clock").
//
// A turn-based mode would hand a spell to the engine at once and hold its
// conjure by the caster's own clock, so that the installed rules_core
// interrupts it as it does in real time. Whether that can be done rests on
// what the conjure is: which of the command's sub-actions, whether its
// duration is the conjure time, what IsConjuring reads, and when the mana
// goes. The detour on CSSubActionHandler::Execute reads, for a creature
// whose current command is a UseAbility, the sub-action being run (its
// type and its delay, duration and elapsed time), the command's conjure
// time, IsConjuring and the creature's mana, and logs them to probe.log as
// they change, with what the run answered. It changes nothing.
#pragma once

namespace probe
{

// At plugin load.
void AttachConjureWatch();

} // namespace probe
