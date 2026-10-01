// Example: a key of the plugin's own (dao/Input.h).
//
// Two actions are registered at load, as the game registers its own: one
// with a default key (F11) that works while exploring and in a fight, and
// one with no key, for the player to bind (in KeyBindings.ini). Each
// press is logged with the game's mode and how often the key has been
// pressed; once a second for the first minute the log says what became of
// the registration, since the game takes the actions only when its mode
// controller starts.
//
// What to look for in play: F11 logging one line per press and none while a
// menu or a conversation is up, and both actions' lines in KeyBindings.ini
// after the first run. Seen 2026-10-01 (dao/Input.h), all but the
// conversation; the controls screen did not list the actions.
// What it does goes to example_input.log.
#include "dao/Input.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <chrono>
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

void Frame()
{
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
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    const bool ping = dao::input::RegisterAction(
        {.name = std::wstring(kPing), .label = L"DAOLib example: log a line", .key = dao::input::Key::F11},
        [] { Pressed(kPing); });
    const bool unbound = dao::input::RegisterAction(
        {.name = std::wstring(kUnbound), .label = L"DAOLib example: no key until bound"}, [] { Pressed(kUnbound); });
    // A name with a space is refused, and so is a name given twice.
    const bool spaced = dao::input::RegisterAction({.name = L"Not A Name", .label = L"refused"}, [] {});
    const bool twice = dao::input::RegisterAction({.name = std::wstring(kPing), .label = L"refused"}, [] {});
    spdlog::info("registered: {} {}; refused as they should be: {} {}", ping, unbound, !spaced, !twice);
    dao::OnFrame(&Frame);
    return true;
}
