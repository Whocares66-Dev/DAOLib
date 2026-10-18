// A log of the game window's close requests and activations, so an exit that
// no one asked for can be traced to its source (DAOLib #199). It logs only:
// every message passes on unchanged.
#pragma once

namespace loader::windowlog
{

// Starts the thread that finds the game's window and puts the hooks on its
// thread (and on the window's new thread if it is ever made on another). From
// the loader's way into the game, before the plugins load.
void Start();

} // namespace loader::windowlog
