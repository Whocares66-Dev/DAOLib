// Example: how long attacks and abilities take (dao/Animation.h).
//
// For everyone in a fight, each attack and each ability used is logged
// twice: when its command starts, with the times dao/Animation.h says it
// will take (the swing's animation, the attack time the scripts last set,
// an ability's conjure, loop, cast and cast end), and when it ends, with
// how long it really took on the clock. Set beside each other, the two say
// whether the library's figures are the engine's. A time-based turn bar
// costs actions with them (the turn-based mod's).
//
// What it sees goes to example_animation.log.
#include "dao/Animation.h"
#include "dao/Abilities.h"
#include "dao/CommandEvents.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace
{

using Clock = std::chrono::steady_clock;

std::mutex g_lock;
std::unordered_map<dao::ObjectId, Clock::time_point> g_started; // by creature

std::string Seconds(const std::optional<float> &seconds)
{
    return seconds ? std::to_string(*seconds).substr(0, 5) + " s" : std::string("none");
}

void Started(dao::ObjectId who, const dao::Command &command)
{
    const auto type = command.Type();
    if (type != dao::core::CommandType::Attack && type != dao::core::CommandType::UseAbility)
    {
        return;
    }
    {
        const std::lock_guard lock(g_lock);
        g_started[who] = Clock::now();
    }
    if (const auto ability = command.Ability())
    {
        const auto timing = dao::animation::OfAbility(who, *ability);
        spdlog::info("0x{:x} uses {} ({}): conjure {}, loop {}, cast {}, cast end {}", who,
                     dao::Utf8(dao::abilities::Name(*ability).value_or(L"?")), *ability, Seconds(timing.conjure),
                     Seconds(timing.conjureLoop), Seconds(timing.cast), Seconds(timing.castEnd));
        return;
    }
    spdlog::info("0x{:x} attacks: swing animation {}, the scripts' last attack time {}, aim {}", who,
                 Seconds(dao::animation::Length(who, dao::animation::kAttack)),
                 Seconds(dao::animation::AttackDuration(who)), Seconds(dao::animation::AimDuration(who)));
}

void Ended(dao::ObjectId who, const dao::Command &command, std::int32_t result)
{
    const auto type = command.Type();
    if (type != dao::core::CommandType::Attack && type != dao::core::CommandType::UseAbility)
    {
        return;
    }
    std::optional<double> took;
    {
        const std::lock_guard lock(g_lock);
        if (const auto it = g_started.find(who); it != g_started.end())
        {
            took = std::chrono::duration<double>(Clock::now() - it->second).count();
            g_started.erase(it);
        }
    }
    spdlog::info("0x{:x} {} ended ({}) after {}; the scripts' attack time now {}", who,
                 type == dao::core::CommandType::Attack ? "attack" : "ability", dao::commands::ResultName(result),
                 took ? std::to_string(*took).substr(0, 5) + " s" : std::string("?"),
                 Seconds(dao::animation::AttackDuration(who)));
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_animation");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::commands::OnStart(&Started);
    dao::commands::OnEnd(&Ended);
    return true;
}
