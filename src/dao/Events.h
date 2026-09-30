// The game's script events, as the engine delivers them: a plugin's
// callback sees each one, and whom it is for, just before the target's
// scripts do.
//
//   dao::events::OnScriptEvent([](dao::ObjectId target, const dao::CScriptEvent &event) {
//       if (event.Type() == dao::script::EventType::Damaged)
//           spdlog::info("{:#x} was hurt by {:#x}", target, event.Creator());
//   });
//
// Events travel through the engine's event queue (dao/Addresses.h, "The
// engine's event queue"): SignalEvent and the engine itself queue a
// CScriptEvent for a target, and AIMaster::ProcessEvent hands it to the
// target when it is due. OnScriptEvent detours ProcessEvent. Seen in play
// 2026-09-30 (dev/LIBRARY.md): damage, perception, attack impacts, ability
// casts, heartbeats, game mode changes, module and area loads, command
// completions and the scripts' own event numbers all come this way. Not
// every event does: most EVENT_TYPE_COMMAND_COMPLETEs are delivered some
// other way, so a callback sees only a few of those.
//
// Callbacks run on the scripts' thread, and see the event the target is
// about to handle; they observe, they do not change or stop it.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <functional>

namespace dao::events
{

using ScriptEventCallback = std::function<void(ObjectId target, const CScriptEvent &event)>;

// Adds a callback for every script event; the first call detours the
// engine's event processing. At plugin load or later.
void OnScriptEvent(ScriptEventCallback callback);

// Adds a callback for script events of one type.
void OnScriptEvent(script::EventType type, ScriptEventCallback callback);

} // namespace dao::events
