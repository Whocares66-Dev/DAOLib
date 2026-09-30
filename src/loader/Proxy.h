#pragma once

namespace loader::proxy
{

// Load the system's version.dll and point every exported thunk at it. Called
// from DllMain before anything else: the thunks jump through null until it
// has run. False if the system DLL could not be loaded, which is a process
// that cannot go on either way.
bool Load();

} // namespace loader::proxy
