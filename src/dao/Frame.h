// A plugin's own code on every frame of the game's update.
//
//   dao::OnFrame([] { overlay.Refresh(); });
//
// The game updates as a task on the engine's worker threads (Eclipse::
// UpdateTask, whose slot 3 calls the per-frame update, dao/Addresses.h).
// OnFrame detours that update and runs its callbacks just before it, on
// the thread that runs it, once per frame. That is the thread the scripts
// run on (FrameThreadId() equalled dao::script::ScriptThreadId() in play,
// 2026-09-30), so a callback may call script commands and send messages;
// and the update runs while the game is paused too.
//
// A callback runs inside the frame: keep it short, and do not block.
#pragma once

#include <cstdint>
#include <functional>

namespace dao
{

// Adds a callback; the first call detours the update. At plugin load or later.
void OnFrame(std::function<void()> callback);

// The thread the last frame ran on, 0 before the first.
std::uint32_t FrameThreadId();

// Frames since the first callback was added.
std::uint64_t FrameCount();

} // namespace dao
