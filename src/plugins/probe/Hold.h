// The probe's grab hold: one creature's animation held by the engine's own
// pause for it, apart from the game's time scale, and let go again (DAOLib
// #159, for DAOTurnBased's issue 119, a grab held between its grabber's
// turns).
//
// Off unless DAOLIB_PROBE_HOLD=<seconds> is set. Then a creature whose
// EFFECT_TYPE_GRABBING is seen (as it ages, so on the update after it is
// applied) is paused DAOLIB_PROBE_HOLD_AFTER_MS later (3000 by default: an
// ogre's first punch comes some 2.7 s after its grabbing effect) for that
// many seconds of real time (fractions too: 4.5), and resumed; and with it,
// in the same frames, DAOLIB_PROBE_HOLD_VICTIM (an object id) if that is
// set. From the effect to five seconds after the resume, probe.log has every
// animation event handled, by type and creature; from the hold on, once a
// second, the grabber's and the victim's models' animation rates, effects'
// time left and sub-actions. Then it waits for the next grabbing effect, so
// one session holds several grabs.
#pragma once

#include <cstdint>

namespace probe
{

// At plugin load.
void AttachHoldWatch();

// From the animation event detour: a creature handled one of its animation's events.
void OnAnimationEvent(const void *creature, std::uint32_t type);

} // namespace probe
