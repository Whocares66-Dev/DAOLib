// Example: one creature's animation paused, slowed and resumed, apart from
// the game's speed (dao/Animation.h).
//
// Every creature that starts an attack takes turns: its first attack's
// animation is slowed to a quarter until the attack ends, its second is
// paused for two seconds and then resumed, and so on. A new command while a
// creature is paused resumes it first, so a stun or a death plays at once;
// a plugin holding a creature for longer resumes it when its command ends,
// too (dao::animation::Pause). Each change
// is logged with the rate the library reads back.
//
// What it does goes to example_hold.log.
#include "dao/Animation.h"
#include "dao/CommandEvents.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace
{

using Clock = std::chrono::steady_clock;
constexpr auto kPauseFor = std::chrono::seconds(2);
constexpr float kSlow = 0.25F;

// The game thread's alone: commands start and end on it, and frames run on it.
std::unordered_map<dao::ObjectId, int> g_attacks;              // attacks started, by creature
std::unordered_map<dao::ObjectId, Clock::time_point> g_paused; // until when, by creature
std::unordered_map<dao::ObjectId, bool> g_slowed;

void Log(const char *what, dao::ObjectId who)
{
    spdlog::info("0x{:x} {}: rate now {}", who, what, dao::animation::Rate(who).value_or(-1.0F));
}

void Resume(dao::ObjectId who, const char *why)
{
    if (g_paused.erase(who) != 0 && dao::animation::Pause(who, false))
    {
        Log(why, who);
    }
}

void Started(dao::ObjectId who, const dao::Command &command)
{
    Resume(who, "resumed for its new command");
    if (command.Type() != dao::core::CommandType::Attack)
    {
        return;
    }
    if (++g_attacks[who] % 2 == 1)
    {
        if (dao::animation::SetRate(who, kSlow))
        {
            g_slowed[who] = true;
            Log("slowed for its attack", who);
        }
        return;
    }
    if (dao::animation::Pause(who, true))
    {
        g_paused[who] = Clock::now() + kPauseFor;
        Log("paused for two seconds", who);
    }
}

void Ended(dao::ObjectId who, const dao::Command &command, std::int32_t /*result*/)
{
    if (command.Type() == dao::core::CommandType::Attack && g_slowed.erase(who) != 0 &&
        dao::animation::SetRate(who, 1.0F))
    {
        Log("back to normal speed", who);
    }
}

void Frame()
{
    const auto now = Clock::now();
    for (auto it = g_paused.begin(); it != g_paused.end();)
    {
        if (now < it->second)
        {
            ++it;
            continue;
        }
        const dao::ObjectId who = it->first;
        it = g_paused.erase(it);
        if (dao::animation::Pause(who, false))
        {
            Log("resumed after two seconds", who);
        }
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_hold");
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
