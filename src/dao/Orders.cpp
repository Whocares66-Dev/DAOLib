// The player's orders, through the engine functions explore mode's click
// handlers call, so a plugin's order is a click to everything that watches
// (the command player issued, the creature told its order arrived).
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
#include "dao/Orders.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/GameModes.h"
#include "dao/Module.h"

namespace dao::orders
{

namespace
{

bool Exists(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    return app != nullptr && app->GetCreatureById(creature) != nullptr;
}

} // namespace

bool Attack(ObjectId creature, ObjectId target)
{
    if (!Exists(creature))
    {
        return false;
    }
    using Fn = void(__stdcall *)(ObjectId, ObjectId);
    reinterpret_cast<Fn>(Module::Address(addr::kOrders_Attack))(creature, target);
    return true;
}

bool UseObject(ObjectId creature, ObjectId object, script::PlaceableAction action)
{
    if (!Exists(creature))
    {
        return false;
    }
    // The action is read as its low 16 bits.
    using Fn = void(__stdcall *)(ObjectId, ObjectId, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kOrders_UseObject))(creature, object,
                                                                   static_cast<std::int32_t>(action) & 0xFFFF);
    return true;
}

bool UseAbility(ObjectId creature, std::int32_t ability, ObjectId target, ObjectId item, const Vector &point)
{
    if (!Exists(creature))
    {
        return false;
    }
    alignas(16) const float at[4] = {point.x, point.y, point.z, 0.0F};
    using Fn = void(__stdcall *)(std::int32_t, ObjectId, ObjectId, ObjectId, const float *);
    reinterpret_cast<Fn>(Module::Address(addr::kOrders_UseAbility))(ability, creature, item, target, at);
    return true;
}

bool MoveTo(ObjectId creature, const Vector &point)
{
    GameModeExplore *explore = CClientExoApp::CurrentExplore();
    if (explore == nullptr || !Exists(creature))
    {
        return false;
    }
    explore->MoveTo(creature, point);
    return true;
}

} // namespace dao::orders
