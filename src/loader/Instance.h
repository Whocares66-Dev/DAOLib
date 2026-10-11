// A second copy of the game beside the first, asked for by a name in
// DAOLIB_INSTANCE (DAOLib #67). Without the variable nothing here does
// anything, and the game is the one it ever was.
//
// The game runs one at a time by a named mutex, and every copy shares one
// profile, Documents\BioWare\Dragon Age (dev/BUILDS.md). With a name, this
// process's mutex is the name's own (dao/core/Instance.h) and the Documents
// folder it is told of is the instance's, under the user's local application
// data (%LOCALAPPDATA%\DAOLib\instances\<name>), so its settings, saves,
// screenshots and DAOLib's logs are its own. A new instance's profile is
// given a copy of the default one's settings, add-in lists and saves, once,
// so it has the same content and the same games to load; what it saves
// after that is its own. Starting it is the launcher's.
#pragma once

namespace loader::instance
{

// Reads DAOLIB_INSTANCE and, if it names an instance, detours the calls the mutex and the
// Documents folder come from. Call it before anything asks for Documents: the log's folder
// is under it. It logs nothing, there being no log yet. It leaves the variable as what the
// process is: the instance's name in lower case, or unset if the name was not taken.
void Install();

// Says in the log what Install did: nothing without the variable, the instance's mutex and
// folder with it, or why a name was not taken.
void Report();

// Gives a new instance's profile the default profile's saves, which can take a while:
// call it where the loader lock is not held, before the game's code runs. Does nothing
// for an instance whose profile has them, or for the game as ever.
void SeedSaves();

} // namespace loader::instance
