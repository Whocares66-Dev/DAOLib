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
#include <cstdint>
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

// The game's reaction withheld, toggled by a key the player binds (DAOLib #127; none by default, since a default can
// take a key the game uses, as F9 is its Quick load), and what the keys and mouse do read all the same: a line whenever
// one of W, A, S, D, Shift or the right mouse changes, or the mouse or the wheel moves, while it holds.
bool g_withheld = false;

void ToggleWithheld()
{
    g_withheld = !g_withheld;
    dao::input::Withhold(g_withheld ? dao::input::kMovement | dao::input::kCameraTurn | dao::input::kCameraZoom |
                                          dao::input::kCameraPan
                                    : 0U);
    spdlog::info("the game's reaction to the keys and mouse {}", g_withheld ? "withheld" : "given back");
}

void LogReadings()
{
    static std::array<bool, 6> was{};
    const std::array<bool, 6> now{
        dao::input::Down(dao::input::Key::W),         dao::input::Down(dao::input::Key::A),
        dao::input::Down(dao::input::Key::S),         dao::input::Down(dao::input::Key::D),
        dao::input::Down(dao::input::Key::LeftShift), dao::input::Down(dao::input::MouseButton::Right)};
    const dao::input::MouseMove moved = dao::input::MouseDelta();
    const std::int32_t notches = dao::input::WheelNotches();
    if (g_withheld && (now != was || moved.x != 0 || moved.y != 0 || notches != 0))
    {
        spdlog::info("W {} A {} S {} D {} Shift {} right mouse {}; mouse {} {}; wheel {}", now[0], now[1], now[2],
                     now[3], now[4], now[5], moved.x, moved.y, notches);
    }
    was = now;
}

// An action held rather than pressed (DAOLib #125): a line when its key, bound by the player, goes down and up.
constexpr std::wstring_view kHeld = L"DAOLibExampleHeld";

void LogHeld(std::wstring_view name)
{
    static std::vector<std::pair<std::wstring, bool>> was;
    auto found = std::ranges::find(was, name, &std::pair<std::wstring, bool>::first);
    if (found == was.end())
    {
        was.emplace_back(name, false);
        found = std::prev(was.end());
    }
    const bool now = dao::input::Held(name);
    if (now != found->second)
    {
        spdlog::info("{} {}", dao::Utf8(name), now ? "held" : "let go");
        found->second = now;
    }
}

// A key map of the example's own (DAOLib #130): its action may have a key the game's actions
// have, W say, and answers to it only while the map is on, when the game's action on that key does
// nothing; off, the game has the key back.
constexpr std::wstring_view kMap = L"DAOLibExampleMap";
constexpr std::wstring_view kMapAction = L"DAOLibExampleMapForward";
bool g_mapOn = false;

void ToggleMap()
{
    g_mapOn = !g_mapOn;
    dao::input::SetKeymap(kMap, g_mapOn);
    spdlog::info("the example's key map {}", g_mapOn ? "on" : "off");
}

void Frame()
{
    Walk();
    LogReadings();
    LogHeld(kHeld);
    LogHeld(kMapAction);
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
    dao::input::RegisterAction({.name = L"DAOLibExampleWithhold",
                                .label = L"DAOLib example: withhold the game's keys",
                                .section = L"DAOLib example"},
                               &ToggleWithheld);
    dao::input::RegisterAction(
        {.name = std::wstring(kHeld), .label = L"DAOLib example: log while held", .section = L"DAOLib example"}, [] {});
    dao::input::RegisterAction({.name = L"DAOLibExampleMapToggle",
                                .label = L"DAOLib example: the example's key map on or off",
                                .section = L"DAOLib example"},
                               &ToggleMap);
    dao::input::RegisterAction({.name = std::wstring(kMapAction),
                                .label = L"DAOLib example: in the key map, log while held",
                                .section = L"DAOLib example",
                                .keymap = std::wstring(kMap)},
                               [] { spdlog::info("{} pressed", dao::Utf8(kMapAction)); });
    dao::OnFrame(&Frame);
    return true;
}
