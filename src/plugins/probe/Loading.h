// The probe's loading watch: where a loading screen's time goes, step by
// step (DAOLib #64).
//
// The loading game mode runs one state a frame, each a machine in numbered
// steps (dao/Addresses.h, "The loading mode's states"): an area transition
// within the area list in memory, a swap to another list, a save, a load.
// Whether areas could be loaded ahead of a door rests on which steps the
// time is in: the rooms and models of the area arrived in, which a preload
// could have ready, or the saving and rebuilding of an area list and the
// waits around them, which it could not.
//
// The detour on the mode's update times every step: how many updates it
// took, how long the engine was inside them (work that holds the frame),
// and how long it lasted in all (the rest is waiting on other threads). A
// line per step goes to probe.log as the step ends, and a line per loading
// screen when the game has left the mode.
#pragma once

namespace probe
{

// At plugin load.
void AttachLoadingWatch();

} // namespace probe
