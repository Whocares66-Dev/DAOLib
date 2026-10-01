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
