// Example: changing combat without replacing a script (dao/ScriptHooks.h).
//
// The party takes half the damage it would. Dragon Age works out an
// attack's damage in script (combat_h.nss, effect_damage_h.nss), and a mod
// that changes those scripts has to recompile everything that includes
// them, and cannot share the game with another that does. This changes
// the result where the scripts hand it to the engine instead, so it needs
// no script of its own and other plugins can do the same beside it.
//
// effect_damage_h.nss says where: "THIS IS THE ONLY SINGLE POINT IN GAME
// THAT MODIFIES HEALTH IN THE GAME AFTER CHARACTER GENERATION!", its
// SetCurrentHealth, which for a creature is the engine's
// SetCreatureProperty(creature, PROPERTY_DEPLETABLE_HEALTH, new health,
// PROPERTY_VALUE_CURRENT). The plugin intercepts that command and, for a
// party member losing health, gives the engine a new health with half the
// loss. Two limits, both the scripts': the floating number over the
// creature still shows the whole hit (it is shown before), and a blow the
// scripts judge fatal kills (they apply a death effect instead of
// lowering the health). What it does goes to example_damage.log.
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/ScriptHooks.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

namespace
{

constexpr float kShare = 0.5F; // of each hit the party takes

namespace api = dao::script::api;
namespace constants = dao::script::constants;

int SetCreatureProperty(dao::script::CommandCall &call)
{
    // SetCreatureProperty(object creature, int property, float value, int value type)
    const auto creature = call.Object(0);
    const auto newHealth = call.Float(2);
    if (creature && newHealth && call.Int(1) == constants::PROPERTY_DEPLETABLE_HEALTH &&
        call.Int(3) == constants::PROPERTY_VALUE_CURRENT && (api::IsHero(*creature) || api::IsFollower(*creature)))
    {
        const float health = api::GetCreatureProperty(*creature, constants::PROPERTY_DEPLETABLE_HEALTH,
                                                      constants::PROPERTY_VALUE_CURRENT);
        if (*newHealth < health)
        {
            const float taken = (health - *newHealth) * kShare;
            call.SetFloat(2, health - taken);
            spdlog::info("{:#010x} hit for {:.1f}, takes {:.1f}", *creature, health - *newHealth, taken);
        }
    }
    return call.Proceed();
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_damage");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::script::InterceptCommand(dao::script::id::SetCreatureProperty, &SetCreatureProperty);
    return true;
}
