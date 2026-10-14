#include "perf/FastLoad.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Options.h"
#include "perf/fastload/AreaLists.h"
#include "perf/fastload/AreaRooms.h"
#include "perf/fastload/LoadingScreen.h"
#include "perf/fastload/Shared.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>

namespace perf
{

namespace
{

constexpr std::size_t kState = 0x84;             // in the loading mode: its state, a word
constexpr std::int64_t kBetweenScreensMs = 1000; // an update this long after the last begins a loading screen
constexpr const wchar_t *kSection = L"FastLoad";
constexpr const wchar_t *kKey = L"On";

// The switch is the options screen's, on the game thread; the parts ask from the update thread
// and, the loading screen's, from the render thread. Off until the player turns it on: it is a
// change to the game's speed, not a fix, so it is the player's to choose (the user, 2026-10-13).
std::atomic<bool> g_on{false};
std::int64_t g_lastUpdate = 0;

// The loader's own settings: daolib.ini beside the logs, so an instance of the game has its own.
std::filesystem::path Settings()
{
    return dao::LogDirectory() / "daolib.ini";
}

void Set(bool on)
{
    g_on.store(on, std::memory_order_relaxed);
    fastload::screen::Apply();
    if (WritePrivateProfileStringW(kSection, kKey, on ? L"1" : L"0", Settings().c_str()) == FALSE)
    {
        spdlog::warn("fast load: the switch could not be kept in {}; it is {} until the game is closed",
                     Settings().string(), on ? "on" : "off");
    }
    spdlog::info("fast load: turned {} in the options screen, from the next loading screen", on ? "on" : "off");
}

// GameModeLoading::Update: __thiscall (float seconds), `ret 4`. What it leaves in eax is its
// base's, and is passed on unread. The one detour of it: each part is told before the engine's
// update runs, and those that follow the update through are told after.
using UpdateFn = std::uintptr_t(__fastcall *)(std::uintptr_t mode, int edx, float seconds);
dao::Detour<UpdateFn> g_update;

std::uintptr_t __fastcall Update(std::uintptr_t mode, int edx, float seconds)
{
    fastload::Update update;
    update.mode = mode;
    update.state = fastload::At<std::uint16_t>(mode, kState);
    update.now = fastload::NowMs();
    update.first = update.now - g_lastUpdate > kBetweenScreensMs;
    g_lastUpdate = update.now;
    fastload::lists::BeforeUpdate(update);
    fastload::rooms::BeforeUpdate(update);
    fastload::screen::BeforeUpdate(update);
    const std::uintptr_t result = g_update.Original()(mode, edx, seconds);
    fastload::lists::AfterUpdate();
    return result;
}

bool LeftOut()
{
    std::array<char, 4> value{};
    const DWORD length = GetEnvironmentVariableA("DAOLIB_FAST_LOAD", value.data(), static_cast<DWORD>(value.size()));
    return length == 1 && value[0] == '0';
}

} // namespace

bool fastload::On()
{
    return g_on.load(std::memory_order_relaxed);
}

void InstallFastLoad()
{
    if (LeftOut())
    {
        spdlog::info("fast load: DAOLIB_FAST_LOAD=0, so none of it is in: the game loads as it always did");
        return;
    }
    if (!g_update.Attach(dao::addr::kGameModeLoading_Update, &Update))
    {
        spdlog::warn("fast load: the loading mode's update could not be detoured, so none of it is in");
        return;
    }
    g_on.store(GetPrivateProfileIntW(kSection, kKey, 0, Settings().c_str()) != 0, std::memory_order_relaxed);
    fastload::lists::Attach();
    fastload::rooms::Attach();
    fastload::screen::Attach();
    const bool shown = dao::options::AddSection(
        L"Performance",
        {{
            .name = L"DAOLibFastLoad",
            .label = L"Fast loading",
            .tooltip = L"Keeps the places the party has been in memory and loads the rooms next door ahead, so "
                       L"loading screens are short. Changes from the next loading screen.",
            .get = [] { return fastload::On(); },
            .set =
                [](bool wanted) {
                    Set(wanted);
                    return wanted;
                },
        }},
        {}, dao::options::Column::Right); // the left is where a plugin's section goes first
    spdlog::info("fast load: {} ({}); its switch is {}", g_on.load() ? "on" : "off", Settings().string(),
                 shown ? "\"Fast loading\" on the options screen's Game tab" : "not on the options screen");
}

} // namespace perf
