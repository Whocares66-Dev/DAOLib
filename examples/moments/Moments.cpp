// Example: the moments a plugin would otherwise poll for (dao/Creatures.h, dao/GameModes.h).
//
// Each death is logged on its frame with the killer, each revival, each creature made once its
// spawn script has set it up (with its health then), each change of the game's mode, each load
// once the party stands in the area, and each time the game takes the player's input again; each
// hold an effect takes, with its effect and length; each attack approach the engine gives up; and
// every five seconds the controlled creature's movement. The
// scripts' own EVENT_TYPE_DEATH is logged too, to show how much later it comes. What it sees goes
// to example_moments.log.
#include "dao/Attack.h"
#include "dao/ClientExoApp.h"
#include "dao/Creatures.h"
#include "dao/Events.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Objects.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/SubActions.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <string>

namespace
{

namespace api = dao::script::api;

std::string Named(dao::ObjectId object)
{
    return object != dao::kInvalidObject ? fmt::format("{} 0x{:x}", dao::Utf8(api::GetName(object)), object)
                                         : std::string("nobody");
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_moments");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::creatures::OnDeath([](dao::ObjectId creature, dao::ObjectId killer) {
        spdlog::info("death: {} killed by {}", Named(creature), Named(killer));
    });
    dao::creatures::OnRevive([](dao::ObjectId creature) { spdlog::info("revived: {}", Named(creature)); });
    dao::creatures::OnReady([](dao::ObjectId creature) {
        const auto *made = dao::CClientExoApp::Get()->GetCreatureById(creature);
        spdlog::info("ready: {} with health {:.0f} of {:.0f}", Named(creature),
                     made != nullptr ? made->Health().value_or(-1) : -1,
                     made != nullptr ? made->MaxHealth().value_or(-1) : -1);
    });
    dao::events::OnScriptEvent(dao::script::EventType::Death, [](dao::ObjectId target, const dao::CScriptEvent &) {
        spdlog::info("the scripts' death event: {}", Named(target));
    });
    dao::modes::OnModeChanged([](dao::GameMode before, dao::GameMode now) {
        spdlog::info("mode: {} to {}", static_cast<int>(before), static_cast<int>(now));
    });
    dao::modes::OnAreaLoaded([] {
        const dao::ObjectId hero = api::GetHero();
        spdlog::info("area loaded: {} in {}", Named(hero), dao::Utf8(api::GetTag(api::GetArea(hero))));
    });
    dao::modes::OnInputBack([] { spdlog::info("input back"); });
    dao::commands::OnHold([](dao::ObjectId creature, const dao::commands::Hold &hold) {
        spdlog::info("held: {} by effect {} (type {}, ability {}, from {}), {:.2f} s{}", Named(creature),
                     hold.effect ? hold.effect->id : 0, hold.effect ? static_cast<int>(hold.effect->type) : 0,
                     hold.effect ? hold.effect->ability : 0,
                     Named(hold.effect ? hold.effect->creator : dao::kInvalidObject), hold.timing.seconds,
                     hold.timing.known ? "" : " at least");
    });
    dao::attack::OnApproachGivesUp(
        [](dao::ObjectId attacker, dao::attack::GiveUp why, std::int32_t status, bool ranged) {
            spdlog::info("approach gave up: {} ({}), why {}, status {}", Named(attacker), ranged ? "ranged" : "melee",
                         static_cast<int>(why), status);
        });
    // Every five seconds, the controlled creature's movement.
    dao::OnFrame([] {
        static auto last = std::chrono::steady_clock::now();
        if (std::chrono::steady_clock::now() - last < std::chrono::seconds(5))
        {
            return;
        }
        last = std::chrono::steady_clock::now();
        const dao::ObjectId who = api::GetMainControlled();
        if (const auto movement = dao::creatures::MovementOf(who))
        {
            spdlog::info("movement: {} at {:.2f} m/s toward {:.2f}, multiplier {:.2f}, state {}{}", Named(who),
                         movement->speed, movement->target, movement->multiplier, movement->state,
                         movement->combat ? ", in combat" : "");
        }
    });
    return true;
}
