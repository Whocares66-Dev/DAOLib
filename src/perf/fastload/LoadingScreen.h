// The loading screen takes its turns at a shorter pace, is presented no
// oftener than it was, and comes out of its black sooner (DAOLib issues #71,
// #73 and #81). A part of fast load (perf/FastLoad.h).
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
// The fade: the loading screen with a picture is the movie load_town (the
// scene Loading), thirty frames a second, and its picture is there from the
// first frame under a black rectangle, at depth 47 of the main timeline,
// whose alpha falls from whole to nothing over 66 frames: 2.2 s, of which a
// loading that is over in a second shows little but black. The clock cannot hurry it: the
// engine advances a movie two frames a turn at the most (the loop at
// 0x0051ADA0 calls the movie's Advance with a catch-up of 1), which at the
// pace here is four times its speed and still half a second. So the movie
// is changed as the game opens its file (dao/Gui.h, OnMovieLoad): the cover
// fades eight times as fast, nothing at all after nine frames, and every
// other thing in the movie comes at its own time. It is taken as the switch
// stands when the game first opens the movie, so a change of the switch
// reaches the fade at the game's next start. A door is not this screen: it
// is load_light (the scene LightLoading), black by design with the word
// "Loading" after 0.85 s of the movie, and is not changed; load_fade is
// black too. In play, 2026-10-11, GOG, windowed, the window pictured thirty
// times a second: a save loaded from in the game came out of its black in
// 0.6 s where it took 2.5 (#73), the picture and the tip as ever after; and
// the doors between Redcliffe village and its tavern, load_light by the
// screens shown, were black from end to end, 1.1 to 1.5 s.
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
// presents every frame), DAOLIB_LOADING_FADE=<times> how many times as fast
// the cover fades in place of 8 (0 or 1 leaves the movie's own).
void Attach();

// The GUI's step as the switch now stands: after the player has changed it.
void Apply();

// Before an update of the loading mode.
void BeforeUpdate(const Update &update);

} // namespace perf::fastload::screen
