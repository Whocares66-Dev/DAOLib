// Movies.h: GFxMovieRoot::Advance counted by caller and thread.
#include "Movies.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/GameModes.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/core/Msvc8.h"

#include <spdlog/spdlog.h>

#include <windows.h>

#include <intrin.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

namespace
{

struct Seen
{
    std::uint32_t calls = 0;
    std::uint32_t still = 0; // of no time
    float least = 1e9F;
    float most = 0.0F;
    std::set<std::uintptr_t> movies;
};

std::mutex g_mutex;
std::map<std::tuple<std::uintptr_t, DWORD>, Seen> g_seen; // by caller and thread
auto g_since = std::chrono::steady_clock::now();

// GFxMovieRoot::Advance: __thiscall (float seconds, unsigned catch-up), `ret 8`, a float in st(0)
// (dao/Addresses.h).
using AdvanceFn = float(__fastcall *)(void *movie, int edx, float seconds, unsigned catchUp);
dao::Detour<AdvanceFn> g_advance;

// Each movie record's step (dao/Addresses.h, kMovieRecord_Step): the record's name (+0), flags
// (+0x24), shown byte (+0x28) and speed (+0x30), and the two times and restriction it is given.
struct Stepped
{
    std::string name;
    std::uint32_t flags = 0;
    bool shown = false;
    float speed = 0.0F;
    std::uint32_t calls = 0;
    std::uint32_t restricted = 0;
    float firstLeast = 1e9F;
    float firstMost = -1e9F;
    float secondLeast = 1e9F;
    float secondMost = -1e9F;
};
std::map<std::uintptr_t, Stepped> g_stepped; // by record, under g_mutex

using StepFn = void(__fastcall *)(void *record, int edx, float first, float second, std::int32_t restricted);
dao::Detour<StepFn> g_step;

// The arrival watch (DAOLib #83): what the movies cost from a loading screen until 1.5 s after the
// game has left the loading mode, the frames a door's black is made of. Each record's steps and
// each OpenMovie are timed by name, and logged once the window has passed.
struct Timed
{
    std::uint32_t calls = 0;
    double ms = 0;
    double most = 0;
};
std::map<std::string, Timed> g_arrivalSteps; // by record's name, under g_mutex
std::map<std::string, Timed> g_arrivalOpens; // by movie's name, under g_mutex
std::chrono::steady_clock::time_point g_lastLoading{};
bool g_arriving = false;

// OpenMovie, __thiscall (screen, const std::wstring &name, layer, flags), `ret 0x10` (dao/Addresses.h).
using OpenMovieFn = std::uintptr_t(__fastcall *)(void *manager, int edx, std::uintptr_t screen,
                                                 const dao::core::WStringRep *name, std::uint32_t layer,
                                                 std::uint32_t flags);
dao::Detour<OpenMovieFn> g_openMovie;

void Add(std::map<std::string, Timed> &into, const std::string &name, double ms)
{
    Timed &timed = into[name];
    ++timed.calls;
    timed.ms += ms;
    timed.most = std::max(timed.most, ms);
}

void ReportArrival()
{
    double steps = 0;
    for (const auto &[name, timed] : g_arrivalSteps)
    {
        steps += timed.ms;
    }
    spdlog::info("movies: around a loading screen, until 1.5 s after it: {:.1f} ms in the movies' steps", steps);
    for (const auto &[name, timed] : g_arrivalSteps)
    {
        if (timed.ms >= 1.0)
        {
            spdlog::info("movies:   step {}: {} calls, {:.1f} ms, the longest {:.1f} ms", name, timed.calls, timed.ms,
                         timed.most);
        }
    }
    for (const auto &[name, timed] : g_arrivalOpens)
    {
        spdlog::info("movies:   opened {}: {} times, {:.1f} ms, the longest {:.1f} ms", name, timed.calls, timed.ms,
                     timed.most);
    }
    g_arrivalSteps.clear();
    g_arrivalOpens.clear();
}

// Whether the watch is on now: the loading mode, or under 1.5 s since it was. Reports a window
// that has passed. Under g_mutex.
bool Arriving()
{
    const auto now = std::chrono::steady_clock::now();
    const auto *app = dao::CClientExoApp::Get();
    const auto *modes = app != nullptr ? app->GameModes() : nullptr;
    if (modes != nullptr && modes->Mode() == dao::GameMode::Loading)
    {
        g_lastLoading = now;
        g_arriving = true;
        return true;
    }
    if (g_arriving && now - g_lastLoading > std::chrono::milliseconds(1500))
    {
        g_arriving = false;
        ReportArrival();
    }
    return g_arriving;
}

double MsSince(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

std::uintptr_t __fastcall OpenMovie(void *manager, int edx, std::uintptr_t screen, const dao::core::WStringRep *name,
                                    std::uint32_t layer, std::uint32_t flags)
{
    const auto start = std::chrono::steady_clock::now();
    const std::uintptr_t record = g_openMovie.Original()(manager, edx, screen, name, layer, flags);
    const double ms = MsSince(start);
    const std::lock_guard lock(g_mutex);
    if (Arriving())
    {
        Add(g_arrivalOpens, dao::Utf8(name->View()), ms);
    }
    return record;
}

void __fastcall Step(void *record, int edx, float first, float second, std::int32_t restricted)
{
    {
        const auto at = reinterpret_cast<std::uintptr_t>(record);
        const std::lock_guard lock(g_mutex);
        Stepped &seen = g_stepped[at];
        if (seen.name.empty())
        {
            seen.name = dao::Utf8(reinterpret_cast<const dao::core::WStringRep *>(at)->View());
        }
        seen.flags = dao::TryRead<std::uint32_t>(at + 0x24).value_or(0);
        seen.shown = dao::TryRead<std::uint8_t>(at + 0x28).value_or(0) != 0;
        seen.speed = dao::TryRead<float>(at + 0x30).value_or(0);
        ++seen.calls;
        seen.restricted += (restricted & 0xFF) != 0 ? 1U : 0U;
        seen.firstLeast = std::min(seen.firstLeast, first);
        seen.firstMost = std::max(seen.firstMost, first);
        seen.secondLeast = std::min(seen.secondLeast, second);
        seen.secondMost = std::max(seen.secondMost, second);
    }
    const auto start = std::chrono::steady_clock::now();
    g_step.Original()(record, edx, first, second, restricted);
    const double ms = MsSince(start);
    const std::lock_guard lock(g_mutex);
    if (Arriving())
    {
        Add(g_arrivalSteps, dao::Utf8(reinterpret_cast<const dao::core::WStringRep *>(record)->View()), ms);
    }
}

void ReportSteps(bool paused)
{
    for (const auto &[record, seen] : g_stepped)
    {
        spdlog::info("movies: {} paused: record {} flags {:#x} {} speed {:.2f}: {} steps, {} restricted, first "
                     "{:.4f} to {:.4f}, second {:.4f} to {:.4f}",
                     paused ? "game" : "game not", seen.name, seen.flags, seen.shown ? "shown" : "hidden", seen.speed,
                     seen.calls, seen.restricted, seen.firstLeast, seen.firstMost, seen.secondLeast, seen.secondMost);
    }
    g_stepped.clear();
}

void Report()
{
    auto *app = dao::CClientExoApp::Get();
    const bool paused = app != nullptr && app->IsPaused();
    for (const auto &[key, seen] : g_seen)
    {
        spdlog::info("movies: {} paused: caller {:#010x} thread {}: {} calls, {} of no time, {:.4f} to {:.4f} s, "
                     "{} movies",
                     paused ? "game" : "game not", std::get<0>(key), std::get<1>(key), seen.calls, seen.still,
                     seen.least, seen.most, seen.movies.size());
    }
    g_seen.clear();
    ReportSteps(paused);
}

float __fastcall Advance(void *movie, int edx, float seconds, unsigned catchUp)
{
    const auto caller = dao::Module::Preferred(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    {
        const std::lock_guard lock(g_mutex);
        Seen &seen = g_seen[{caller, GetCurrentThreadId()}];
        ++seen.calls;
        seen.still += seconds == 0.0F ? 1U : 0U;
        seen.least = std::min(seen.least, seconds);
        seen.most = std::max(seen.most, seconds);
        seen.movies.insert(reinterpret_cast<std::uintptr_t>(movie));
        const auto now = std::chrono::steady_clock::now();
        if (now - g_since >= std::chrono::seconds(2))
        {
            g_since = now;
            Report();
        }
    }
    return g_advance.Original()(movie, edx, seconds, catchUp);
}

} // namespace

namespace probe
{

void AttachMovieWatch()
{
    if (!g_advance.Attach(dao::addr::kGFxMovieRoot_Advance, &Advance))
    {
        spdlog::warn("movies: could not detour GFxMovieRoot::Advance");
    }
    if (!g_step.Attach(dao::addr::kMovieRecord_Step, &Step))
    {
        spdlog::warn("movies: could not detour a movie record's step");
    }
    if (!g_openMovie.Attach(dao::addr::kScaleformManager_OpenMovie, &OpenMovie))
    {
        spdlog::warn("movies: could not detour OpenMovie");
    }
}

} // namespace probe
