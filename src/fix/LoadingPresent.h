// A loading screen's frames are not all presented (DAOLib issue #73).
//
// On arrival the loading mode waits for the rooms around the party to load,
// one check a frame (dao/Addresses.h, "The loading mode's states", state 6
// step 6), and every such frame presents the loading screen. With vertical
// sync on, the present waits for the monitor, and what the streaming thread
// needs of the Direct3D device to build the rooms waits with it: through
// the wait that thread spent over half its time waiting for a lock under
// CreateIndexBuffer and CreateTexture. In play, into Redcliffe village by a
// door, the wait was 320 to 672 ms with the sync on and 190 to 227 with it
// off.
//
// So while the loading mode is updating, or the loading screen's own task
// is the one presenting, a frame is presented only when the
// last was presented a fifteenth of a second ago or more: the pace the
// loading screen's own task draws it at before the arrival (dao/Addresses.h,
// "The loading screen's task"). The frames between are drawn and not
// presented, so nothing waits for the monitor; a present skipped answers
// that the device is not lost, which is all the engine asks of it. Vertical
// sync stays as the player set it, and outside a loading screen every frame
// is presented as ever. In play, 2026-10-11, windowed with the sync on, as
// a plugin of the same code: the wait 200 to 302 ms, ten door screens 5.33 s
// where they had been 6.75 s, and nothing odd at a loading screen's end at
// a quarter second's grain.
#pragma once

namespace fix
{

// Once, from the loader's way into the game, before the plugins load: every
// game, whatever plugins it has. DAOLIB_LOADING_PRESENT=<ms> in the
// environment sets the least time between two presents of a loading screen
// in place of 66; 0 presents every frame, the engine as it was.
void InstallLoadingPresentFix();

} // namespace fix
