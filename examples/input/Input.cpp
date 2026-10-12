// Example: a key of the plugin's own (dao/Input.h).
//
// Two actions are registered at load, as the game registers its own: one
// with a default key (F11) that works while exploring and in a fight, and
// one with no key, for the player to bind in the controls screen. Each
// press is logged with the game's mode and how often the key has been
// pressed; once a second for the first minute the log says what became of
// the registration, since the game takes the actions only when its mode
// controller starts.
//
// What to look for in play: F11 logging one line per press and none while a
// menu or a conversation is up, and both actions' lines in KeyBindings.ini
// after the first run. Seen 2026-10-01 (dao/Input.h), all but the
// conversation. The controls screen lists both under a header "Mods" at
// the end of its list: bind a key to the second there and it is logged,
// and still bound after a restart (seen the same day).
// What it does goes to example_input.log.
#include "dao/Input.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Party.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <string_view>

namespace
{

constexpr std::wstring_view kPing = L"DAOLibExamplePing";
constexpr std::wstring_view kUnbound = L"DAOLibExampleUnbound";

const char *Name(dao::input::ActionState state)
{
    switch (state)
    {
    case dao::input::ActionState::Unknown:
        return "unknown";
    case dao::input::ActionState::Waiting:
        return "waiting for the game";
    case dao::input::ActionState::Registered:
        return "registered";
    case dao::input::ActionState::NameTaken:
        return "name taken";
    }
    return "?";
}

void Pressed(std::wstring_view action)
{
    const auto mode = dao::CClientExoApp::CurrentMode();
    spdlog::info("{} pressed, {} time(s), in mode {}", dao::Utf8(action), dao::input::Presses(action),
                 mode ? static_cast<int>(*mode) : -1);
}

// A walk with no key: each of the four directions held for a second and a half in turn, and
// where the controlled creature stood before and after each. Only where a file
// example_input.walk beside the plugin asks for it, and once, in the world.
void Walk()
{
    using Clock = std::chrono::steady_clock;
    static const bool asked = std::filesystem::exists(dao::Module::OwnFolder() / L"example_input.walk");
    static int step = -1; // the direction held, 4 once all are done
    static Clock::time_point since;
    static const std::array<const char *, 4> kNames{"forward", "backward", "left", "right"};
    // It starts in explore mode and then runs to its end whatever the game goes into: a walk may
    // set off a conversation, and what a held direction does there is part of what is looked for.
    const auto mode = dao::CClientExoApp::CurrentMode();
    if (!asked || step > 4 || (step == -1 && mode != dao::GameMode::Explore))
    {
        return;
    }
    const dao::ObjectId creature = dao::party::Controlled();
    if (creature == dao::kInvalidObject)
    {
        return;
    }
    const auto log = [&](const char *what) {
        const dao::Vector at = dao::script::api::GetPosition(creature);
        spdlog::info("walk: {}, the creature at {:.2f} {:.2f} {:.2f}, mode {}", what, at.x, at.y, at.z,
                     mode ? static_cast<int>(*mode) : -1);
    };
    if (step == -1)
    {
        // A few seconds in the world first, the load settled.
        static const Clock::time_point entered = Clock::now();
        if (Clock::now() - entered < std::chrono::seconds(4))
        {
            return;
        }
        log("before");
        step = 0;
        since = Clock::now();
        dao::input::Hold(dao::input::Move::Forward, true);
        return;
    }
    if (Clock::now() - since < std::chrono::milliseconds(1500))
    {
        return;
    }
    since = Clock::now();
    if (step < 4)
    {
        dao::input::Hold(static_cast<dao::input::Move>(step), false);
        log(kNames.at(static_cast<std::size_t>(step)));
    }
    else
    {
        log("a second and a half after the last was let go");
    }
    ++step;
    if (step < 4)
    {
        dao::input::Hold(static_cast<dao::input::Move>(step), true);
    }
}

void Frame()
{
    Walk();
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point started = Clock::now();
    static Clock::time_point last = started;
    static dao::input::ActionState was = dao::input::ActionState::Unknown;
    if (Clock::now() - started > std::chrono::minutes(1) || Clock::now() - last < std::chrono::seconds(1))
    {
        return;
    }
    last = Clock::now();
    const dao::input::ActionState now = dao::input::StateOf(kPing);
    if (now != was)
    {
        spdlog::info("{}: {}; {}: {}", dao::Utf8(kPing), Name(now), dao::Utf8(kUnbound),
                     Name(dao::input::StateOf(kUnbound)));
        was = now;
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_input");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    const bool ping = dao::input::RegisterAction(
        {.name = std::wstring(kPing), .label = L"DAOLib example: log a line", .key = dao::input::Key::F11},
        [] { Pressed(kPing); });
    const bool unbound = dao::input::RegisterAction(
        {.name = std::wstring(kUnbound), .label = L"DAOLib example: no key until bound", .section = L"DAOLib example"},
        [] { Pressed(kUnbound); });
    // A name with a space is refused, and so is a name given twice.
    const bool spaced = dao::input::RegisterAction({.name = L"Not A Name", .label = L"refused"}, [] {});
    const bool twice = dao::input::RegisterAction({.name = std::wstring(kPing), .label = L"refused"}, [] {});
    spdlog::info("registered: {} {}; refused as they should be: {} {}", ping, unbound, !spaced, !twice);
    dao::OnFrame(&Frame);
    return true;
}
