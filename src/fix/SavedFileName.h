// A file's name in a render command is copied from a buffer that holds what
// the copy takes (DAOLib issue #78).
//
// Renderer::ThreadedGraphicsDriver records, for the render thread, an object
// to be saved to a file (a screenshot is one): its slots 84 and 85, commands
// 0x27 and 0x28 (dao/Addresses.h, "A saved file's name"). Each copies the
// file's name into the command as a fixed 0x400 bytes from the std::wstring's
// buffer, whatever the string's length, so the copy reads up to a kilobyte
// past the end of a short name. In the engine's own pools that was other
// memory of the pool. With the engine's memory on the system's heap
// (perf/EngineHeap.h) a short string can be the last thing before a page
// that is not there, and the read faults: in play, 2026-10-11, the game
// crashed as a screenshot was asked of it, the name's memory ending 0x2A0
// bytes before such a page.
//
// Both are detoured, and the name is handed on as a string of the loader's
// own whose buffer is the 0x400 bytes the copy takes: the name's characters
// at its start and nothing after. The command gets what it always meant to
// hold.
#pragma once

namespace fix
{

// Once, from the loader's way into the game, before the plugins load: every
// game, whatever plugins it has.
void InstallSavedFileNameFix();

} // namespace fix
