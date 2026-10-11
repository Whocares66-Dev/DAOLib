#include "perf/fastload/LoadingScreen.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace perf::fastload::screen
{

namespace
{

constexpr DWORD kEngineSleepMs = 66;            // the task's own turn
constexpr std::uint32_t kStepRead = 0x3D872B02; // 0.066F, the GUI's step as the engine has it
constexpr float kEngineStep = 0.066F;
constexpr std::int64_t kLeastPaceMs = 4;

// How long after the loading mode's last update a frame is still a loading screen's: a frame of
// the arrival's wait is 26 ms at the most in play, and the first frame of the game after it
// should be presented when it comes.
constexpr std::int64_t kLoadingWindowMs = 50;

std::int64_t g_paceMs = 16;     // the task's turn; 0 for the engine's own
std::int64_t g_intervalMs = 66; // between two presents of a loading screen; 0 for every frame
std::uintptr_t g_step = 0;      // where the GUI's step is, once it is found as read
// The update is the game's thread's; the task and its sleep are the render thread's; a present
// is whichever thread's draws the frame.
std::atomic<std::int64_t> g_lastUpdate{0};
std::atomic<std::int64_t> g_lastPresent{0};
std::atomic<std::uint32_t> g_skipped{0};
std::atomic<std::uint32_t> g_presented{0};
std::atomic<DWORD> g_taskThread{0}; // the thread inside the loading screen's task, or 0

// UpdateAndRenderLoadingGUITask::Execute: __thiscall (float seconds), `ret 4`; a turn of the
// loading screen's own task, which sleeps, updates the loading screen, draws it and presents it,
// and answers in eax whether to be run again.
using TaskFn = std::uint32_t(__fastcall *)(std::uintptr_t task, int edx, float seconds);
dao::Detour<TaskFn> g_task;

std::uint32_t __fastcall Task(std::uintptr_t task, int edx, float seconds)
{
    g_taskThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
    const std::uint32_t result = g_task.Original()(task, edx, seconds);
    g_taskThread.store(0, std::memory_order_relaxed);
    return result;
}

// The engine's sleep: __cdecl (ms), over the system's. The task's own sleep of 66 ms, the one
// it begins each turn with, is answered with the pace.
using SleepFn = void(__cdecl *)(DWORD ms);
dao::Detour<SleepFn> g_sleep;

void __cdecl EngineSleep(DWORD ms)
{
    const bool tasks = ms == kEngineSleepMs && g_taskThread.load(std::memory_order_relaxed) == GetCurrentThreadId();
    g_sleep.Original()(tasks && On() ? static_cast<DWORD>(g_paceMs) : ms);
}

// Whether this present is to be skipped: it is a loading screen's, and the last frame presented
// was presented less than the interval ago. The loading screen's own task presents it while an
// update of the loading mode is long at its work, a save's read or an area's build, when no
// update is recent: its presents are a loading screen's whenever they come.
bool Skip()
{
    if (!On())
    {
        return false;
    }
    const std::int64_t now = NowMs();
    const bool loading = now - g_lastUpdate.load(std::memory_order_relaxed) <= kLoadingWindowMs ||
                         g_taskThread.load(std::memory_order_relaxed) == GetCurrentThreadId();
    if (loading && now - g_lastPresent.load(std::memory_order_relaxed) < g_intervalMs)
    {
        g_skipped.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    if (loading)
    {
        g_presented.fetch_add(1, std::memory_order_relaxed);
    }
    g_lastPresent.store(now, std::memory_order_relaxed);
    return false;
}

// D3DGraphicsDriver::Present and D3DSwapChain::Present: __thiscall, no arguments; the device's
// Present with four nulls and the swap chain's with five, each answering in al, and al alone,
// whether the device is not lost (dao/Addresses.h, "The frame's present"). Which a frame goes
// through is the engine's choice: in play, windowed, the loading screen's own task used the
// driver's and the frames of the arrival's wait the swap chain's.
using PresentFn = bool(__fastcall *)(std::uintptr_t self, int edx);
dao::Detour<PresentFn> g_present;
dao::Detour<PresentFn> g_presentChain;

bool __fastcall Present(std::uintptr_t driver, int edx)
{
    return Skip() || g_present.Original()(driver, edx);
}

bool __fastcall PresentChain(std::uintptr_t chain, int edx)
{
    return Skip() || g_presentChain.Original()(chain, edx);
}

// A number of ms from the environment, or what it is when the variable is not set.
std::int64_t Asked(const char *variable, std::int64_t otherwise)
{
    std::array<char, 8> value{};
    const DWORD length = GetEnvironmentVariableA(variable, value.data(), static_cast<DWORD>(value.size()));
    return length > 0 && length < value.size() ? std::strtol(value.data(), nullptr, 10) : otherwise;
}

// The pace: the task's sleep answered, and the GUI's step found as read.
void AttachPace()
{
    const std::int64_t asked = Asked("DAOLIB_LOADING_PACE", g_paceMs);
    g_paceMs = asked <= 0 ? 0 : std::clamp<std::int64_t>(asked, kLeastPaceMs, kEngineSleepMs);
    if (g_paceMs == 0)
    {
        spdlog::info("fast load: DAOLIB_LOADING_PACE=0, so the loading screen's task sleeps its 66 ms a turn as ever");
        return;
    }
    const std::uintptr_t step = dao::Module::Address(dao::addr::kLoadingGUITask_Step);
    const auto stepNow = step != 0 ? dao::TryRead<std::uint32_t>(step) : std::nullopt;
    if (!stepNow || *stepNow != kStepRead || !g_sleep.Attach(dao::addr::kMultithreading_Sleep, &EngineSleep))
    {
        g_paceMs = 0;
        spdlog::warn("fast load: the loading screen's task is not as read (its step {:#x}) or its sleep could not be "
                     "detoured, so it sleeps as ever",
                     stepNow.value_or(0));
        return;
    }
    g_step = step;
    spdlog::info("fast load: the loading screen's task takes a turn every {} ms, not every 66, so a loading's steps "
                 "do not wait on it",
                 g_paceMs);
}

// The presents: both of the engine's.
void AttachPresents()
{
    g_intervalMs = Asked("DAOLIB_LOADING_PRESENT", g_intervalMs);
    if (g_intervalMs <= 0)
    {
        g_intervalMs = 0;
        spdlog::info("fast load: DAOLIB_LOADING_PRESENT=0, so every frame of a loading screen is presented as ever");
        return;
    }
    if (!g_present.Attach(dao::addr::kD3DGraphicsDriver_Present, &Present) ||
        !g_presentChain.Attach(dao::addr::kD3DSwapChain_Present, &PresentChain))
    {
        g_presentChain.Detach();
        g_present.Detach();
        g_intervalMs = 0;
        spdlog::warn("fast load: the present's detours could not be placed, so every frame of a loading screen is "
                     "presented as ever");
        return;
    }
    spdlog::info("fast load: a loading screen's frames are presented {} ms apart at the least, the rest drawn and "
                 "not presented, so its loading does not wait for the monitor",
                 g_intervalMs);
}

} // namespace

void Attach()
{
    // Which thread is in the task is what both the pace and the presents ask.
    if (!g_task.Attach(dao::addr::kLoadingGUITask_Execute, &Task))
    {
        spdlog::warn("fast load: the loading screen's task could not be detoured, so its pace and its presents are "
                     "the engine's");
        return;
    }
    AttachPace();
    AttachPresents();
    Apply();
}

void Apply()
{
    if (g_step == 0)
    {
        return;
    }
    // The step is a constant among the exe's read-only data, which nothing else reads.
    const float seconds = On() ? static_cast<float>(g_paceMs) / 1000.0F : kEngineStep;
    DWORD was = 0;
    if (VirtualProtect(reinterpret_cast<void *>(g_step), sizeof seconds, PAGE_READWRITE, &was) != FALSE)
    {
        std::memcpy(reinterpret_cast<void *>(g_step), &seconds, sizeof seconds);
        VirtualProtect(reinterpret_cast<void *>(g_step), sizeof seconds, was, &was);
    }
}

void BeforeUpdate(const Update &update)
{
    if (update.first)
    {
        // What the last loading screen's presents came to, at debug.
        const std::uint32_t presented = g_presented.exchange(0);
        const std::uint32_t skipped = g_skipped.exchange(0);
        spdlog::debug("fast load: over the last loading screen, {} frames presented and {} drawn and not presented",
                      presented, skipped);
    }
    g_lastUpdate.store(update.now, std::memory_order_relaxed);
}

} // namespace perf::fastload::screen
