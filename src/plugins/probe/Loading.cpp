#include "Loading.h"

#include "Recorder.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/World.h"
#include "dao/core/Msvc8.h"

#include <spdlog/spdlog.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace probe
{

namespace
{

// The loading mode and its data (dao/Addresses.h, "The loading mode's
// states").
constexpr std::size_t kState = 0x84;       // in the mode: a word
constexpr std::size_t kData = 0x130;       // in the mode: its GameModeLoadingData
constexpr std::size_t kAreaTag = 0xD4;     // in the mode: the area asked for, a std::wstring
constexpr std::size_t kWaypointTag = 0xF0; // in the mode: the waypoint asked for
constexpr std::size_t kAreaOwnTag = 0x180; // in an area: its tag
constexpr std::uint32_t kLongestTag = 256; // a tag longer than this is not a tag
constexpr std::size_t kLongestSteps = 5;   // steps named in a loading screen's last line

// 2 and 8 share a machine (0x006C76F0, not read: a game starting or a save
// loading, by the errors it gives).
constexpr std::uint16_t kStateStart = 2;
constexpr std::uint16_t kStateSave = 3;
constexpr std::uint16_t kStateListSwap = 4;
constexpr std::uint16_t kStateTransition = 5;
constexpr std::uint16_t kStateEnter = 6;
constexpr std::uint16_t kStateStartOther = 8;

// Where a state keeps its step, in the data; none for a state without steps.
std::optional<std::size_t> StepField(std::uint16_t state)
{
    switch (state)
    {
    case kStateStart:
    case kStateStartOther:
        return 0x64;
    case kStateSave:
        return 0x70;
    case kStateListSwap:
    case kStateTransition:
        return 0x6C;
    case kStateEnter:
        return 0x74;
    default:
        return std::nullopt;
    }
}

// The engine's own words for a state, where it has them.
const char *StateName(std::uint16_t state)
{
    switch (state)
    {
    case kStateSave:
        return " (save)";
    case kStateListSwap:
        return " (area list swap)";
    case kStateTransition:
        return " (area transition)";
    default:
        return "";
    }
}

std::int64_t Now()
{
    LARGE_INTEGER count{};
    QueryPerformanceCounter(&count);
    return count.QuadPart;
}

double Milliseconds(std::int64_t ticks)
{
    static const std::int64_t perSecond = [] {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart;
    }();
    return static_cast<double>(ticks) * 1000.0 / static_cast<double>(perSecond);
}

// The engine's string at an address, or nothing if it does not read as one.
std::string TextAt(std::uintptr_t at)
{
    const auto rep = dao::TryRead<dao::core::WStringRep>(at);
    if (!rep || rep->size > kLongestTag || rep->capacity < rep->size)
    {
        return {};
    }
    std::wstring text(rep->size, L'\0');
    const auto from = rep->capacity > dao::core::WStringRep::kInlineCapacity
                          ? reinterpret_cast<std::uintptr_t>(rep->data.pointer)
                          : at + offsetof(dao::core::WStringRep, data);
    if (!dao::TryCopy(text.data(), from, text.size() * sizeof(wchar_t)))
    {
        return {};
    }
    return dao::Utf8(text);
}

struct Step
{
    std::uint16_t state = 0;
    std::int32_t step = -1;
    std::uint32_t updates = 0;
    std::int64_t inside = 0; // ticks the engine spent in the step's updates
    std::int64_t began = 0;
    std::int64_t ended = 0;

    bool Is(std::uint16_t otherState, std::int32_t otherStep) const
    {
        return state == otherState && step == otherStep;
    }
};

// One loading screen: from the mode's first update to the game leaving it.
struct Screen
{
    bool open = false;
    std::int64_t began = 0;
    std::int64_t lastUpdate = 0; // when the last update returned
    Step current;
    std::vector<Step> steps;
    std::string area;
    std::string waypoint;
};

std::mutex g_mutex;
Screen g_screen;

void LogStep(const Step &step)
{
    spdlog::info("loading: state {}{} step {}: {} update(s), {:.1f} ms inside, {:.1f} ms in all", step.state,
                 StateName(step.state), step.step, step.updates, Milliseconds(step.inside),
                 Milliseconds(step.ended - step.began));
}

// The step under way ends at `at`; g_mutex held.
void EndStep(std::int64_t at)
{
    if (g_screen.current.updates == 0)
    {
        return;
    }
    g_screen.current.ended = at;
    LogStep(g_screen.current);
    g_screen.steps.push_back(g_screen.current);
    g_screen.current = Step{};
}

std::string RoomsNow()
{
    const std::vector<dao::world::Room> rooms = dao::world::Rooms();
    const auto loaded = std::ranges::count_if(rooms, &dao::world::Room::Loaded);
    const auto loading = std::ranges::count_if(rooms, [](const dao::world::Room &room) { return room.loading; });
    return std::format("{} rooms, {} loaded, {} loading", rooms.size(), loaded, loading);
}

// The loading screen is over: its steps summed, the longest named; g_mutex held.
void EndScreen()
{
    EndStep(g_screen.lastUpdate);
    std::int64_t inside = 0;
    std::uint32_t updates = 0;
    for (const Step &step : g_screen.steps)
    {
        inside += step.inside;
        updates += step.updates;
    }
    std::ranges::sort(g_screen.steps,
                      [](const Step &a, const Step &b) { return a.ended - a.began > b.ended - b.began; });
    std::string longest;
    for (std::size_t i = 0; i < std::min(g_screen.steps.size(), kLongestSteps); ++i)
    {
        const Step &step = g_screen.steps[i];
        longest += std::format("{}{}/{} {:.0f} ms ({:.0f} inside)", i == 0 ? "" : ", ", step.state, step.step,
                               Milliseconds(step.ended - step.began), Milliseconds(step.inside));
    }
    const dao::CCArea *area = dao::world::CurrentArea();
    spdlog::info("loading: done in {:.0f} ms, {:.0f} ms of it inside {} updates, over {} steps; longest (state/step): "
                 "{}; now in area \"{}\", {}",
                 Milliseconds(g_screen.lastUpdate - g_screen.began), Milliseconds(inside), updates,
                 g_screen.steps.size(), longest,
                 area != nullptr ? TextAt(reinterpret_cast<std::uintptr_t>(area) + kAreaOwnTag) : std::string(),
                 RoomsNow());
    g_screen = Screen{};
    SetLoadingPhase(0);
}

// GameModeLoading's update, its slot 8: __thiscall (float seconds), `ret 4`.
// What it leaves in eax is its base's, and is passed on unread.
using UpdateFn = std::uintptr_t(__fastcall *)(void *mode, int edx, float seconds);
dao::Detour<UpdateFn> g_update;

std::uintptr_t __fastcall Update(void *mode, int edx, float seconds)
{
    const auto at = reinterpret_cast<std::uintptr_t>(mode);
    const std::uint16_t state = dao::TryRead<std::uint16_t>(at + kState).value_or(0);
    const std::uintptr_t data = dao::TryRead<std::uintptr_t>(at + kData).value_or(0);
    const auto field = StepField(state);
    const std::int32_t step = field && data != 0 ? dao::TryRead<std::int32_t>(data + *field).value_or(-1) : -1;

    const std::int64_t before = Now();
    {
        const std::lock_guard lock(g_mutex);
        if (!g_screen.open)
        {
            g_screen.open = true;
            g_screen.began = before;
            g_screen.area = TextAt(at + kAreaTag);
            g_screen.waypoint = TextAt(at + kWaypointTag);
            spdlog::info(R"(loading: begins in state {}{}, for area "{}" at waypoint "{}"; {})", state,
                         StateName(state), g_screen.area, g_screen.waypoint, RoomsNow());
        }
        if (!g_screen.current.Is(state, step))
        {
            EndStep(before);
            g_screen.current.state = state;
            g_screen.current.step = step;
            g_screen.current.began = before;
        }
    }
    SetLoadingPhase(LoadingPhase(state, step, true));
    const std::uintptr_t result = g_update.Original()(mode, edx, seconds);
    SetLoadingPhase(LoadingPhase(state, step, false));
    const std::int64_t after = Now();
    {
        const std::lock_guard lock(g_mutex);
        ++g_screen.current.updates;
        g_screen.current.inside += after - before;
        g_screen.lastUpdate = after;
    }
    return result;
}

// Each frame: the game out of the loading mode ends the loading screen.
void Frame()
{
    const std::lock_guard lock(g_mutex);
    if (g_screen.open && dao::CClientExoApp::CurrentMode() != dao::GameMode::Loading)
    {
        EndScreen();
    }
}

} // namespace

void AttachLoadingWatch()
{
    if (!g_update.Attach(dao::addr::kGameModeLoading_Update, &Update))
    {
        spdlog::error("loading watch: could not detour GameModeLoading::Update at {:#010x}",
                      dao::Module::Preferred(dao::addr::kGameModeLoading_Update));
        return;
    }
    spdlog::info("loading watch: detoured GameModeLoading::Update at {:#010x}",
                 dao::Module::Preferred(dao::addr::kGameModeLoading_Update));
    dao::OnFrame(&Frame);
}

} // namespace probe
