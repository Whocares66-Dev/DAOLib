// Example: an attack's approach and its steps (dao/Attack.h, dao/SubActions.h).
//
// For every attack and every ability used at another creature, logs each
// step (sub-action) of the command as it ends: its type, the time the
// engine gave it and how long it ran, so a shot's parts and an ability's
// kind of approach can be read off. For a shooter's attack it also logs, as
// the attack starts, where dao::attack::Walk says the approach will stop,
// and as the approach ends, where it did stop and how far it walked. Set
// beside each other, the two say whether the library's walk is the
// engine's.
//
// What it sees goes to example_attack.log.
#include "dao/Attack.h"
#include "dao/Animation.h"
#include "dao/CommandEvents.h"
#include "dao/Frame.h"
#include "dao/Gda.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/SubActions.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace
{

using Clock = std::chrono::steady_clock;

// The approaches: a creature walks only in these (dao/Conjure.h).
constexpr std::uint32_t kApproachMelee = 4;
constexpr std::uint32_t kApproachRanged = 15;

// A command under way, by the creature whose it is. Everything here runs on
// the game thread: the command events and the frame callback.
struct Watch
{
    bool shooter = false;                    // an attack with a ranged weapon
    std::optional<dao::attack::Walked> said; // a shooter's walk, as Walk gave it at the start
    dao::Vector last;                        // where the creature stood last frame
    float walked = 0;                        // metres moved in an approach
    std::uint32_t step = 0;                  // the running sub-action's type; 0 before the first
    std::int32_t stepMs = 0;                 // the longest duration the step showed
    std::int32_t elapsedMs = 0;              // the step's elapsed time last frame
    Clock::time_point since;                 // when the step was first seen
};
std::unordered_map<dao::ObjectId, Watch> g_watched;

std::string Place(const dao::Vector &at)
{
    return fmt::format("({:.2f}, {:.2f}, {:.2f})", at.x, at.y, at.z);
}

float Distance(const dao::Vector &a, const dao::Vector &b)
{
    return std::sqrt(((a.x - b.x) * (a.x - b.x)) + ((a.y - b.y) * (a.y - b.y)) + ((a.z - b.z) * (a.z - b.z)));
}

std::string Seconds(const std::optional<float> &seconds)
{
    return seconds ? fmt::format("{:.3f} s", *seconds) : std::string("none");
}

bool IsApproach(std::uint32_t step)
{
    return step == kApproachMelee || step == kApproachRanged;
}

// Logs the step a creature has just left, and for a shooter's approach where it ended against
// where Walk had put it.
void StepEnded(dao::ObjectId who, const Watch &watch)
{
    const double ran = std::chrono::duration<double>(Clock::now() - watch.since).count();
    spdlog::info("0x{:x} step {}: given {} ms, ran {:.3f} s", who, watch.step, watch.stepMs, ran);
    if (!watch.shooter || !IsApproach(watch.step))
    {
        return;
    }
    if (!watch.said)
    {
        spdlog::info("0x{:x} approach ended at {} after {:.2f} m; Walk had said none", who, Place(watch.last),
                     watch.walked);
    }
    else if (watch.said->path.empty())
    {
        spdlog::info("0x{:x} approach ended at {} after {:.2f} m; Walk had said no walk", who, Place(watch.last),
                     watch.walked);
    }
    else
    {
        spdlog::info("0x{:x} approach ended at {} after {:.2f} m; Walk had said {:.2f} m, ending {:.2f} m from there",
                     who, Place(watch.last), watch.walked, watch.said->metres,
                     Distance(watch.last, watch.said->path.back()));
    }
}

void Started(dao::ObjectId who, const dao::Command &command)
{
    const auto type = command.Type();
    const auto target = command.Target();
    if ((type != dao::core::CommandType::Attack && type != dao::core::CommandType::UseAbility) || !target ||
        *target == who || *target == dao::kInvalidObject)
    {
        return;
    }
    if (const auto it = g_watched.find(who); it != g_watched.end() && it->second.step != 0)
    {
        StepEnded(who, it->second);
    }
    Watch watch;
    watch.last = dao::script::api::GetPosition(who);
    if (const auto ability = command.Ability())
    {
        spdlog::info("0x{:x} uses ability {} at 0x{:x}: its Range row {}", who, *ability, *target,
                     dao::gda::GetInt(dao::script::Table::AbilitiesTalents, *ability, "Range").value_or(-1));
    }
    else if (dao::attack::IsRanged(who))
    {
        watch.shooter = true;
        watch.said = dao::attack::Walk(who, *target);
        const auto approach = dao::attack::ApproachOf(who, *target);
        spdlog::info("0x{:x} shoots at 0x{:x} from {}: in reach {}, in sight {}, range {:.1f} m, stop within {:.1f} m",
                     who, *target, Place(watch.last), approach && approach->inReach, approach && approach->lineOfSight,
                     approach ? approach->range : 0.0F, approach ? approach->stopWithin : 0.0F);
        if (watch.said && !watch.said->path.empty())
        {
            spdlog::info("0x{:x} Walk says {:.2f} m, to {}", who, watch.said->metres, Place(watch.said->path.back()));
        }
        else
        {
            spdlog::info("0x{:x} Walk says {}", who, watch.said ? "no walk" : "none");
        }
        spdlog::info("0x{:x} the scripts' last attack time {}, aim loop {}; the Aim animation {}, Fire {}", who,
                     Seconds(dao::animation::AttackDuration(who)), Seconds(dao::animation::AimDuration(who)),
                     Seconds(dao::animation::Length(who, 249)), Seconds(dao::animation::Length(who, 243)));
    }
    else
    {
        spdlog::info("0x{:x} attacks 0x{:x} in melee", who, *target);
    }
    g_watched[who] = watch;
}

void Ended(dao::ObjectId who, const dao::Command &command, std::int32_t result)
{
    const auto it = g_watched.find(who);
    const auto type = command.Type();
    if (it == g_watched.end() || (type != dao::core::CommandType::Attack && type != dao::core::CommandType::UseAbility))
    {
        return;
    }
    if (it->second.step != 0)
    {
        StepEnded(who, it->second);
    }
    spdlog::info("0x{:x} {} ended: {} ({})", who, type == dao::core::CommandType::Attack ? "attack" : "ability",
                 dao::commands::ResultName(result), result);
    g_watched.erase(it);
}

// Follows each watched creature's running step, and how far it moves while that is an approach.
void Frame()
{
    for (auto &[who, watch] : g_watched)
    {
        const auto steps = dao::commands::SubActions(who);
        const std::uint32_t running = steps.empty() ? 0 : steps.front().type;
        const std::int32_t elapsed = steps.empty() ? 0 : steps.front().elapsedMs;
        if (running != watch.step || elapsed < watch.elapsedMs)
        {
            if (watch.step != 0)
            {
                StepEnded(who, watch);
            }
            watch.step = running;
            watch.stepMs = 0;
            watch.since = Clock::now();
        }
        const dao::Vector at = dao::script::api::GetPosition(who);
        if (IsApproach(running))
        {
            watch.walked += Distance(at, watch.last);
        }
        watch.last = at;
        watch.elapsedMs = elapsed;
        watch.stepMs = steps.empty() ? watch.stepMs : std::max(watch.stepMs, steps.front().durationMs);
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_attack");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::commands::OnStart(&Started);
    dao::commands::OnEnd(&Ended);
    dao::OnFrame(&Frame);
    return true;
}
