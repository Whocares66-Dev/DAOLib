// Movies.h: GFxMovieRoot::Advance counted by caller and thread.
#include "Movies.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
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
    g_step.Original()(record, edx, first, second, restricted);
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
}

} // namespace probe
