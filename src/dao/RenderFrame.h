// A rendered frame ends, on the rendering thread, before the engine presents it.
#pragma once

#include <functional>

// COM objects are released through Release(), not a C++ destructor.
struct IDirect3DDevice9; // NOLINT(cppcoreguidelines-virtual-class-destructor)

namespace dao::render
{

// device is borrowed for the callback; bySwapChain says which engine Present was called.
// Runs before overlays draw, also when fast loading subsequently skips displaying this frame.
// This marks a rendered frame's boundary, not a successful display or an update-thread tick.
using FrameEndEvent = std::function<void(IDirect3DDevice9 *device, bool bySwapChain)>;

// Register at plugin load or as the device is created, before its first frame ends.
// Callbacks may draw, but must restore the device's state and must
// not present recursively. They retain their state across calls and run in registration order
// in this copy of the static library. Returns false for an empty callback or if either engine
// presentation function could not be hooked; nothing is added.
// Hooks the engine's callers, leaving the Direct3D Present vtables and code untouched.
bool OnFrameEnd(FrameEndEvent callback);

} // namespace dao::render
