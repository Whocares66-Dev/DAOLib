// The probe's movie watch: who advances the Flash movies, on which thread and by how much time,
// with the game paused and not (DAOLib #134). A plugin's screen stops advancing while the game is
// paused though the game's own menus go on; the detour on GFxMovieRoot::Advance counts each
// caller and thread's calls, their smallest and largest time and how many were of no time, and
// logs them to probe.log every two seconds with whether the game is paused. It changes nothing.
#pragma once

namespace probe
{

// At plugin load.
void AttachMovieWatch();

} // namespace probe
