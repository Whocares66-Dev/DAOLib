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
// It also logs each animation the controlled creature is told to play
// (OnPlayed), with its speed and blend.
//
// A key the player binds ("DAOLib example: hold everyone's pose") pauses the game and sets every
// one of the 64 creatures nearest to the start of its animations (SetPhase), so the same frame can be taken
// again; pressed again, it unpauses. DAOLib #131.
//
// What it sees goes to example_animation.log.
#include "dao/Animation.h"
#include "dao/Abilities.h"
#include "dao/ClientExoApp.h"
#include "dao/CommandEvents.h"
#include "dao/Input.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

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

bool g_posed = false;

void TogglePose()
{
    auto *app = dao::CClientExoApp::Get();
    if (app == nullptr)
    {
        return;
    }
    g_posed = !g_posed;
    app->SetPaused(g_posed);
    if (!g_posed)
    {
        spdlog::info("poses given back, unpaused");
        return;
    }
    const dao::ObjectId controlled = dao::script::api::GetMainControlled();
    const dao::script::Array near = dao::script::api::GetNearestObject(
        controlled, static_cast<std::int32_t>(dao::script::constants::OBJECT_TYPE_CREATURE), 64, 0, 0, 1);
    int held = 0;
    for (std::size_t i = 0; i < near.Size(); ++i)
    {
        held += dao::animation::SetPhase(near.Object(i), 0.0F) ? 1 : 0;
    }
    spdlog::info("paused; {} of {} creatures near held at their animations' start", held, near.Size());
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
    dao::animation::OnPlayed([](dao::ObjectId who, std::int32_t animation, float speed, float blend, bool flag) {
        if (who == dao::script::api::GetMainControlled())
        {
            spdlog::info("0x{:x} plays animation {} at speed {:.2f}, blend {:.2f}, flag {}", who, animation, speed,
                         blend, flag);
        }
    });
    dao::input::RegisterAction({.name = L"ExampleAnimationHoldPoses",
                                .label = L"DAOLib example: hold everyone's pose",
                                .section = L"DAOLib example"},
                               &TogglePose);
    return true;
}
