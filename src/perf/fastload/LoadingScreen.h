// The loading screen takes its turns at a shorter pace, and is presented no
// oftener than it was (DAOLib issues #71 and #73). A part of fast load
// (perf/FastLoad.h).
//
// While a loading screen is up it is drawn by a task of its own, on the
// render thread (dao/Addresses.h, "The loading screen's task"), which
// begins every turn with a sleep of 66 ms and then updates the GUI by a
// fixed step of 0.066 s: about fifteen frames a second, by design. The
// game's own frame waits on the render thread while loading, the loading
// mode goes one step a frame, and the end of a loading spins until that
// task has woken and stopped. So a loading screen's steps go at that pace:
// in play a door between Redcliffe village and its tavern was eleven
// updates, several of them single frames of 67 to 86 ms with nothing to do.
//
// The pace: the task's sleep is answered with 16 ms where it asks for its
// 66, and the GUI's step is written as the same time, so the loading
// screen's own animation keeps its speed. Nothing is written into code.
//
// The presents: on arrival the loading mode waits for the rooms around the
// party, one check a frame, and every such frame presents the loading
// screen; with vertical sync on the present waits for the monitor, and
// what the streaming thread needs of the Direct3D device to build the rooms
// waits with it (in play that wait was 320 to 672 ms with the sync on and
// 190 to 227 with it off). And a task that presented at every turn of 16 ms
// would hold the device from the loading it is the screen of: at 16 ms a
// save's load was 5.7 s where it had been 3.9, and at 1 ms every loading
// screen was twice as long. So while the loading mode is updating, or the
// loading screen's own task is the one presenting, a frame is presented
// only when the last was presented 66 ms ago or more; the frames between
// are drawn and not presented, and a present skipped answers that the
// device is not lost, which is all the engine asks of it. Vertical sync
// stays as the player set it, and outside a loading screen every frame is
// presented as ever.
//
// In play, 2026-10-11, GOG, windowed, the sync on: with the rooms kept
// (perf/fastload/AreaRooms.h), ten doors and swaps 4.1 s where they were
// 6.0 s at the engine's pace; a save's load 3.5 s at the start and 2.2 to
// 2.6 s from in the game; the loading screen's fade the same by its
// brightness every quarter second.
#pragma once

#include "perf/fastload/Shared.h"

namespace perf::fastload::screen
{

// Places the detours. DAOLIB_LOADING_PACE=<ms> sets the turn in place of 16
// (4 to 66; 0 leaves the engine's 66), DAOLIB_LOADING_PRESENT=<ms> the
// least time between two presents of a loading screen in place of 66 (0
// presents every frame).
void Attach();

// The GUI's step as the switch now stands: after the player has changed it.
void Apply();

// Before an update of the loading mode.
void BeforeUpdate(const Update &update);

} // namespace perf::fastload::screen
