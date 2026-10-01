// The player's orders, given as the clicks give them: through the same
// engine functions explore mode's click handlers call, so a plugin's order
// is a click to everything that watches (the command player issued, the
// creature told its order arrived).
//
//   dao::orders::Attack(alistair, genlock);
//   dao::orders::UseAbility(morrigan, ABILITY_SPELL_FIREBALL, kInvalidObject, {}, point);
//
// Read 2026-09-30. Each makes the command's parameters (a CDataArrays from
// the engine's heap, which the command then owns), queues the command
// through CCObject::QueuePlayerCommand (QueueNewCommand with player issued
// set, not static, override -1; dao/Addresses.h), and signals
// EVENT_TYPE_ON_ORDER_RECEIVED (90) to the creature, which the scripts'
// follower AI listens for:
// - Attack (0x007F6410, __stdcall (creature, target)): Attack (1) at the
//   front of the queue, the target its object 0.
// - UseObject (0x007F6550, __stdcall (creature, object, action)): UseObject
//   (31) at the back, the object and the action (PLACEABLE_ACTION_*); a
//   DESTROY becomes an Attack. The object hears EVENT_TYPE_PLACEABLE_ONCLICK
//   (78) as well.
// - UseAbility (0x007F6690, __stdcall (ability, creature, item, target,
//   const Vector4 *point)): UseAbility (25) at the back with the ability,
//   the target, the item and the point; an ability whose row in the
//   ability table names an instant script signals that instead, and a
//   creature that cannot act (its slot 116 answering 1) or in a trigger
//   that forbids it gets nothing.
// - MoveTo: explore mode's own (GameModeExplore::MoveTo, dao/GameModes.h),
//   which records the point as the latest move's destination.
// On the game's thread, in explore or combat mode. Not yet seen in play.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"
#include "dao/generated/ScriptConstants.h"

#include <cstdint>

namespace dao::orders
{

// Each answers false, and gives no order, if the creature (or for MoveTo,
// explore mode) is not there.
bool Attack(ObjectId creature, ObjectId target);
bool UseObject(ObjectId creature, ObjectId object, script::PlaceableAction action = script::PlaceableAction::Use);
bool UseAbility(ObjectId creature, std::int32_t ability, ObjectId target, ObjectId item = kInvalidObject,
                const Vector &point = {});
bool MoveTo(ObjectId creature, const Vector &point);

} // namespace dao::orders
