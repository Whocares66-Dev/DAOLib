// Example: the game's speed (dao/Clock.h).
//
// Fights run at another speed: when the game enters combat mode the game
// runs at kFightSpeed (2, twice as fast; 0.5 would be slow motion), and at
// its own speed again when the fight ends.
//
// It also measures what the speed reaches, since play has not agreed on it
// (dao/Clock.h): once a second of wall time it logs the milliseconds the
// controlled creature's own update was given in that second (its object
// timer's, which effects, regeneration and commands run on) and how far it
// moved. At speed 2 the first should be about 2000; the second says whether
// walking keeps pace.
//
// The game's mode is read each frame (a frame callback is cheap, and the
// mode changes on the frame the engine switches it); the speed is set only
// when the mode changes. What it does goes to example_speed.log.
#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Clock.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Objects.h"
#include "dao/Plugin.h"
#include "dao/SafeRead.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>

namespace
{

constexpr float kFightSpeed = 2.0F; // of the game's own speed

// The measure: the controlled creature's update time, summed.
constexpr std::size_t kObjectTimer = 0x2B8; // in a creature: its timer record
constexpr std::size_t kTimerMs = 0x8;       // in the record: ms since the last update
std::atomic<dao::ObjectId> g_watched{dao::kInvalidObject};
std::atomic<std::int64_t> g_updateMs{0};

using UpdateFn = void(__fastcall *)(dao::CCCreature *, int);
dao::Detour<UpdateFn> g_update;

void __fastcall Update(dao::CCCreature *creature, int edx)
{
    g_update.Original()(creature, edx);
    if (creature != nullptr && creature->Id() == g_watched.load(std::memory_order_relaxed))
    {
        const auto record = dao::TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(creature) + kObjectTimer);
        if (const auto ms = record && *record != 0 ? dao::TryRead<std::int32_t>(*record + kTimerMs) : std::nullopt)
        {
            g_updateMs += *ms;
        }
    }
}

void Measure()
{
    using Clock = std::chrono::steady_clock;
    static Clock::time_point last = Clock::now();
    static std::optional<dao::Vector> from;
    const dao::ObjectId self = dao::script::api::GetMainControlled();
    g_watched = self;
    const auto now = Clock::now();
    const auto wall = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
    if (wall < 1000)
    {
        return;
    }
    last = now;
    const dao::CCObject *creature = dao::CClientExoApp::Get()->GetObjectById(self);
    const std::optional<dao::Vector> here = creature != nullptr ? std::optional(creature->Position()) : std::nullopt;
    float moved = 0;
    if (from && here)
    {
        moved = std::hypot(here->x - from->x, here->y - from->y);
    }
    from = here;
    spdlog::info("speed {:.2f}: {} ms of wall time, {} ms of the creature's, {:.2f} m moved", dao::clock::Speed(), wall,
                 g_updateMs.exchange(0), moved);
}

void Frame()
{
    if (dao::CClientExoApp::Get() == nullptr)
    {
        return;
    }
    Measure();
    static std::optional<dao::GameMode> last;
    const auto mode = dao::CClientExoApp::CurrentMode();
    if (!mode || mode == last)
    {
        return;
    }
    const bool fighting = *mode == dao::GameMode::Combat;
    const bool wasFighting = last == dao::GameMode::Combat;
    last = mode;
    if (fighting != wasFighting)
    {
        const bool took = dao::clock::SetSpeed(fighting ? kFightSpeed : 1.0F);
        spdlog::info("{}: the world at {:.2f} speed{}", fighting ? "a fight" : "the fight is over", dao::clock::Speed(),
                     took ? "" : " (not taken: no world timer, or the frame's hook not placed)");
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_speed");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    g_update.Attach(dao::addr::kCCCreature_Update, &Update);
    dao::OnFrame(&Frame);
    return true;
}
