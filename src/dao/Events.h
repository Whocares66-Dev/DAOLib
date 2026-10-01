// The game's script events, as the engine delivers them: a plugin's
// callback sees each one, and whom it is for, just before the target's
// scripts do.
//
//   dao::events::OnScriptEvent([](dao::ObjectId target, const dao::CScriptEvent &event) {
//       if (event.Type() == dao::script::EventType::Damaged)
//           spdlog::info("{:#x} was hurt by {:#x}", target, event.Creator());
//   });
//
// Events reach an object two ways. Most travel through the engine's event
// queue (dao/Addresses.h, "The engine's event queue"): SignalEvent and the
// engine queue a CScriptEvent, AIMaster::ProcessEvent hands it to the
// target's handler when it is due, and the handler runs the object's
// script for it (CCObject::RunEventScript, 0x00698960). Some never queue:
// engine code calls RunEventScript at once, 18 places in all (a creature's
// death, an order received, a selection, effects, the command queue). So
// OnScriptEvent detours RunEventScript, which sees both, and the queue
// too, for the events it hands an area or the module, which have handlers
// of their own. Read 2026-09-30, after a play test in which four deaths
// and most EVENT_TYPE_COMMAND_COMPLETEs never reached a ProcessEvent hook
// (the Eluvian server's report). Seen in play with the new hook, 2026-09-30
// (20:07 to 20:14, two fights): every death came, each 1.5 to 2.5 s after
// the creature was dead by IsDead, once its death had played out, the
// creator the killer for a death from a blow and null for one by an
// applied effect. Over a longer session the same day, 39 deaths gave 39
// events, a median 2.15 s after IsDead and at most 5.14 s (once 20 ms
// before a frame's IsDead poll). For the moment of death itself, a plugin watches IsDead
// or EVENT_TYPE_DAMAGED.
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
