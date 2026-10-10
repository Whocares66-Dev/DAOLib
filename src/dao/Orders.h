// Gives the player's orders the way mouse clicks give them, by calling the same game functions
// that explore mode's click handlers call. The game and its scripts can't tell a plugin's order
// from a click: the command is marked as issued by the player, and the creature gets
// EVENT_TYPE_ON_ORDER_RECEIVED.
//
//   dao::orders::Attack(alistair, genlock);
//   using dao::kInvalidObject;
//   dao::orders::UseAbility(morrigan, 10003 /* Fireball */, kInvalidObject, kInvalidObject, point);
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"
#include "dao/generated/ScriptConstants.h"

#include <cstdint>

namespace dao::orders
{

// Orders the creature to attack a target, ahead of its other commands.
// @return false, and gives no order, if there is no such creature
//
// Call it, like every function here, on the game thread in explore or combat mode.
bool Attack(ObjectId creature, ObjectId target);

// Orders the creature to use an object, such as a door or a chest, after its other commands.
// PLACEABLE_ACTION_DESTROY becomes an attack.
// @return false, and gives no order, if there is no such creature
bool UseObject(ObjectId creature, ObjectId object, script::PlaceableAction action = script::PlaceableAction::Use);

// Orders the creature to use an ability, after its other commands.
// @param ability the ability's row in the ability table, such as 10003
// @param target the creature or object to aim at, or kInvalidObject for none
// @param item the item the ability comes from, or kInvalidObject for none
// @param point the place to aim at, for an ability aimed at the ground
// @return false, and gives no order, if there is no such creature
//
// An ability with an instant script runs that script instead. A creature that cannot act, or is
// standing in a trigger that forbids the ability, gets no order, but the function still returns
// true.
bool UseAbility(ObjectId creature, std::int32_t ability, ObjectId target, ObjectId item = kInvalidObject,
                const Vector &point = {});

// Orders the creature to move to a point, like a click on the ground in explore mode.
// @return false, and gives no order, if there is no such creature or the game is not in explore
// mode
bool MoveTo(ObjectId creature, const Vector &point);

} // namespace dao::orders
