// Lets a plugin watch the game's script events: each event, and the object it is for, just before
// that object's scripts handle it.
//
//   dao::events::OnScriptEvent([](dao::ObjectId target, const dao::CScriptEvent &event) {
//       if (event.Type() == dao::script::EventType::Damaged)
//           spdlog::info("{:#x} was hurt by {:#x}", target, event.Creator());
//   });
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <functional>

namespace dao::events
{

// A callback for a script event. It receives the object, area or module the event is for, and
// the event itself, which is valid only during the call.
using ScriptEventCallback = std::function<void(ObjectId target, const CScriptEvent &event)>;

// Calls `callback` for every script event, whether it was queued or sent straight to its target.
//
// Call it when the plugin loads or later. Callbacks run on the game thread and can only watch:
// they cannot change or stop the event. EVENT_TYPE_DEATH arrives about 1.5 to 5 seconds after
// IsDead starts returning true for the creature. To catch the moment of death, watch IsDead or
// EVENT_TYPE_DAMAGED.
void OnScriptEvent(ScriptEventCallback callback);

// Calls `callback` for script events of one type only. Otherwise it works like the overload
// above.
//
// For EVENT_TYPE_DAMAGED, Float(0) is the damage, Int(0) its DAMAGE_TYPE_* value, Int(1) the
// ability, and the creator is the attacker. The event comes only for a blow that does not kill;
// a fatal blow sends the death event instead.
void OnScriptEvent(script::EventType type, ScriptEventCallback callback);

} // namespace dao::events
