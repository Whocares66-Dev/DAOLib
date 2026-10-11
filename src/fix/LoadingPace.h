// The loading screen's task takes its turns at a shorter pace (DAOLib
// issue #71).
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
// The sleep and the step are set to the same shorter time, 16 ms, each
// written only where the bytes are the ones read, so the loading screen's
// own animation keeps its speed. How often the loading screen is presented
// is the present's fix's to say (fix/LoadingPresent.h), which keeps it at
// fifteen a second: a task that presented at every turn would hold the
// Direct3D device from the loading it is the screen of, and at a turn of
// 1 ms did, every loading screen twice as long.
#pragma once

namespace fix
{

// Once, from the loader's way into the game, before the plugins load and
// before the present's fix, which detours the function this writes into
// (after it, the bytes are not the ones read and nothing is written): every
// game, whatever plugins it has. DAOLIB_LOADING_PACE=<ms> in the
// environment sets the turn in place of 16 (4 to 66); 0 leaves the engine's
// 66 as it is.
void InstallLoadingPaceFix();

} // namespace fix
