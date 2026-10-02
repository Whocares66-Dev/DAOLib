// Example: the clock effects, regeneration and the scripts' timers run on
// (dao/Clock.h, dao/Timers.h, dao/Effects.h).
//
// In a fight, the creature the player controls when it starts lives at
// half speed: its effects wear off, its health, mana and stamina come back
// and its damage over time ticks at half the pace, while everyone else's
// time runs as it always does. When the fight ends the clock is let go and
// what waits carries on. A turn-based mode does the same with rounds:
// hold the clock, give each combatant a clock of its own, and move each one
// on as its turn is spent.
//
// The plugin holds the clock and moves it on itself, each frame, by the
// frame's wall time at the game's speed, nothing while the game is paused:
// the shared clock by all of it, the creature's own by half. Once a second
// it logs the timers waiting, the creature's among them, and the time left
// on each of the creature's temporary effects. To see it: stand the
// creature in a poison or a burning area in a fight, or let a spider poison
// it; the log's ticks for it come half as often as for anyone else, and
// its effects' time left runs down at half the rate. What it does goes to
// example_clock.log.
#include "dao/Clock.h"
#include "dao/ClientExoApp.h"
#include "dao/Effects.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/Timers.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <optional>
#include <string>

namespace
{

constexpr float kOwnPace = 0.5F; // of everyone else's

using WallClock = std::chrono::steady_clock;

dao::ObjectId g_creature = dao::kInvalidObject;
std::optional<WallClock::time_point> g_lastFrame;
WallClock::time_point g_lastLog;

void Begin()
{
    g_creature = dao::script::api::GetMainControlled();
    dao::clock::Hold(true);
    dao::clock::OwnClock(g_creature);
    g_lastFrame = WallClock::now();
    g_lastLog = *g_lastFrame;
    spdlog::info("a fight: {:#010x} on a clock of its own at {:.1f} the pace, everyone else as before", g_creature,
                 kOwnPace);
}

void End()
{
    spdlog::info("the fight is over: the clock let go, {} timers given back", dao::timers::Waiting());
    dao::clock::Hold(false);
    g_creature = dao::kInvalidObject;
    g_lastFrame.reset();
}

void Log()
{
    std::string effects;
    for (const auto &effect : dao::effects::On(g_creature))
    {
        if (effect.duration > 0)
        {
            effects += fmt::format(" {} ({:.1f} of {:.1f} s left)", fmt::underlying(effect.type), effect.remaining,
                                   effect.duration);
        }
    }
    spdlog::info("timers waiting: {}, {} of them the creature's; its effects:{}", dao::timers::Waiting(),
                 dao::timers::Waiting(g_creature), effects.empty() ? std::string(" none timed") : effects);
}

void Frame()
{
    dao::CClientExoApp *app = dao::CClientExoApp::Get();
    if (app == nullptr)
    {
        return;
    }
    const auto mode = dao::CClientExoApp::CurrentMode();
    const bool fighting = mode == dao::GameMode::Combat;
    if (fighting && !g_lastFrame)
    {
        Begin();
    }
    else if (!fighting && g_lastFrame)
    {
        End();
    }
    if (!g_lastFrame)
    {
        return;
    }
    const auto now = WallClock::now();
    const float wall = std::chrono::duration<float>(now - *g_lastFrame).count();
    g_lastFrame = now;
    if (!app->IsPaused())
    {
        const float seconds = wall * dao::clock::Speed();
        dao::clock::Advance(seconds);
        dao::clock::Advance(g_creature, seconds * kOwnPace);
    }
    if (now - g_lastLog >= std::chrono::seconds(1))
    {
        g_lastLog = now;
        Log();
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_clock");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::OnFrame(&Frame);
    return true;
}
