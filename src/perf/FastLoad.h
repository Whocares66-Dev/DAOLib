// Fast load: the game's loading screens made short (DAOLib issue #75, of
// #64, #71, #72 and #73).
//
// It corrects no fault of the engine's, which is why it is here and not in
// src/fix: it is what the engine could have done and did not. Its parts,
// each a file under perf/fastload/ with its reading and what was seen in
// play:
//
// - AreaLists: the area lists the party leaves are kept, eight at the most,
//   and handed back when it returns.
// - AreaRooms: an area's rooms stay loaded when the party leaves it, and
//   the rooms of the indoor areas beside it are loaded ahead.
// - LoadingScreen: the loading screen takes a turn every 16 ms, not every
//   66, and is presented fifteen times a second still, so a loading waits
//   neither for its own screen nor for the monitor; and its picture comes
//   out of the black it begins in eight times as fast, so a short loading
//   shows it. (A door's screen has no picture: it is black by design.)
//
// They share one detour of the loading mode's update, from which each is
// told of every update before the engine's runs, and one switch: "Fast
// loading" in the options screen's Game tab, kept in daolib.ini beside the
// logs. Off, every part stands down from the next loading screen: nothing
// is kept, nothing is loaded ahead, and the loading screen is the
// engine's. In play, 2026-10-11, GOG, a door between Redcliffe village and
// its tavern: 2.2 to 3.0 s and 0.7 to 0.9 s as the game was that morning
// (with the engine's own allocator too, fix/EngineHeap.h), 0.46 to 0.58 s
// and 0.14 to 0.27 s with it.
#pragma once

namespace perf
{

// Once, from the loader's way into the game, before the plugins load: every
// game, whatever plugins it has. DAOLIB_FAST_LOAD=0 in the environment
// leaves all of it out, the switch too. Each part has a variable of its
// own, for finding a fault: DAOLIB_AREA_LISTS (how many are kept, 0 for
// none), DAOLIB_AREA_ROOMS=0, DAOLIB_LOADING_PACE (ms, 0 for the engine's),
// DAOLIB_LOADING_PRESENT (ms, 0 for every frame) and DAOLIB_LOADING_FADE
// (times as fast, 0 for the movie's own).
void InstallFastLoad();

} // namespace perf
