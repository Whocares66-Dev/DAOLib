// Example: an attack's approach and its steps (dao/Attack.h, dao/SubActions.h,
// dao/Animation.h's OfShot).
//
// For every attack and every ability used at another creature, logs as the
// command starts how dao::attack::ApproachOf says it will approach (by the
// target's ring or by a range, in reach or not, room on the ring or none)
// and where dao::attack::Walk says the approach will stop; then each step
// (sub-action) of the command as it ends, with its type, the time the
// engine gave it and how long it ran; and as an approach ends, where it did
// stop and how far it walked. For a shooter's attack it also logs the
// shot's three parts as dao::animation::OfShot times them, before the shot
// and after it. Set beside each other, the library's figures and the
// engine's steps say whether the one is the other.
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
    std::optional<dao::attack::Walked> said; // the walk, as Walk gave it at the start
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

// A creature's shot as OfShot times it now, with the scripts' two times as they are set now.
std::string ShotLine(dao::ObjectId who)
{
    const auto shot = dao::animation::OfShot(who);
    if (!shot)
    {
        return "none";
    }
    return fmt::format("aim {:.3f} s, aim loop {:.3f} s, fire {:.3f} s, {:.3f} s in all (attack time {}, aim time {})",
                       shot->aim, shot->aimLoop, shot->fire, shot->Total(),
                       Seconds(dao::animation::AttackDuration(who)), Seconds(dao::animation::AimDuration(who)));
}

bool IsApproach(std::uint32_t step)
{
    return step == kApproachMelee || step == kApproachRanged;
}

// How ApproachOf answered, for the log.
std::string ApproachLine(const std::optional<dao::attack::Approach> &approach)
{
    if (!approach)
    {
        return "none";
    }
    if (approach->ranged)
    {
        return fmt::format("by range: in reach {}, in sight {}{}, range {:.1f} m, stop within {:.1f} m",
                           approach->inReach, approach->lineOfSight, approach->needsSight ? "" : " (not needed)",
                           approach->range, approach->stopWithin);
    }
    return fmt::format("by the ring: in reach {}, finds room {} (room once filled {}, strikes at once {})",
                       approach->inReach, approach->findsRoom, approach->roomOnceFilled, approach->strikesAtOnce);
}

// How Walk answered, for the log.
std::string WalkLine(const std::optional<dao::attack::Walked> &walk)
{
    if (!walk)
    {
        return "none";
    }
    return walk->path.empty() ? std::string("no walk")
                              : fmt::format("{:.2f} m, to {}", walk->metres, Place(walk->path.back()));
}

// Logs the step a creature has just left, and for an approach where it ended against where Walk
// had put it.
void StepEnded(dao::ObjectId who, const Watch &watch)
{
    const double ran = std::chrono::duration<double>(Clock::now() - watch.since).count();
    spdlog::info("0x{:x} step {}: given {} ms, ran {:.3f} s", who, watch.step, watch.stepMs, ran);
    if (!IsApproach(watch.step))
    {
        return;
    }
    if (!watch.said || watch.said->path.empty())
    {
        spdlog::info("0x{:x} approach ended at {} after {:.2f} m; Walk had said {}", who, Place(watch.last),
                     watch.walked, WalkLine(watch.said));
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
    const auto ability = command.Ability();
    const auto approach =
        ability ? dao::attack::ApproachOf(who, *target, *ability) : dao::attack::ApproachOf(who, *target);
    watch.said = ability ? dao::attack::Walk(who, *target, *ability) : dao::attack::Walk(who, *target);
    watch.shooter = !ability && dao::attack::IsRanged(who);
    if (ability)
    {
        spdlog::info("0x{:x} uses ability {} (Range row {}) at 0x{:x} from {}", who, *ability,
                     dao::gda::GetInt(dao::script::Table::AbilitiesTalents, *ability, "Range").value_or(-1), *target,
                     Place(watch.last));
    }
    else
    {
        spdlog::info("0x{:x} {} 0x{:x} from {}", who, watch.shooter ? "shoots at" : "attacks", *target,
                     Place(watch.last));
    }
    spdlog::info("0x{:x} approach {}; Walk says {}", who, ApproachLine(approach), WalkLine(watch.said));
    if (watch.shooter)
    {
        spdlog::info("0x{:x} the shot before the scripts set its times: {}", who, ShotLine(who));
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
    if (it->second.shooter)
    {
        spdlog::info("0x{:x} the shot as the scripts set it: {}", who, ShotLine(who));
    }
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
