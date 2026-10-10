// Runs a plugin's code once every frame, on the game thread.
//
// The callbacks run on the same thread as the game's scripts, so they may call script commands
// and send messages. The rest of the library calls this thread the game thread.
//
//   dao::OnFrame([] { overlay.Refresh(); });
#pragma once

#include <cstdint>
#include <functional>

namespace dao
{

// Adds a callback to run once per frame, just before the game's update.
// @param callback runs inside the frame, so keep it short and never block
//
// Call it when the plugin loads or later. Callbacks also run while the game is paused.
void OnFrame(std::function<void()> callback);

// Returns the ID of the game thread, the one the last frame ran on; 0 before the first frame.
std::uint32_t FrameThreadId();

// Returns how many frames have run since the first callback was added.
std::uint64_t FrameCount();

} // namespace dao
