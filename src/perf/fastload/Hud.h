// The HUD kept, hidden, through an area transition, and shown again on arrival, where the
// engine closes it and makes it anew (DAOLib #83). A part of fast load (perf/FastLoad.h).
//
// The GUI's game mode change (DragonAgeGUISystem's slot 77) shows the HUD's seven scenes for
// explore and combat. For the GUI mode it hides them and keeps their movies loaded; for a save
// (the loading mode in state 3) it keeps them up; for every other mode, the loading mode in an
// area transition among them, it closes them, which drops their movies. So every door made
// the HUD again on arrival: a new view of each movie and its scripts' set-up, some 200 ms of
// the 0.3 s between the loading mode's last update and the first frame shown, in play in
// Redcliffe village and its tavern (Portraits 54 to 63 ms, NavBar and Quickbar about 20 each,
// the Quickbar's first step 71). While fast load is on, a close asked for while the loading
// mode is in an area transition's first state (7) hides the HUD the way the GUI mode does;
// the arrival's show then finds the movies and shows them. Everything else closes as ever.
#pragma once

namespace perf::fastload::hud
{

// At the loader's start.
void Attach();

} // namespace perf::fastload::hud
