#include "fix/LoadingPresent.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>

namespace fix
{

namespace
{

std::int64_t NowMs()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

// How long after the loading mode's last update a frame is still a loading screen's: a frame of
// the arrival's wait is 26 ms at the most in play, and the first frame of the game after it
// should be presented when it comes.
constexpr std::int64_t kLoadingWindowMs = 50;
std::int64_t g_intervalMs = 66; // the loading screen's own task's turn
// The update is the game's thread's; a present is whichever thread's draws the frame.
std::atomic<std::int64_t> g_lastUpdate{0};
std::atomic<std::int64_t> g_lastPresent{0};
std::atomic<std::uint32_t> g_skipped{0};
std::atomic<std::uint32_t> g_presented{0};
std::atomic<DWORD> g_taskThread{0}; // the thread inside the loading screen's task, or 0

// GameModeLoading::Update: __thiscall (float seconds), `ret 4`; what it leaves in eax is its
// base's, and is passed on unread.
using UpdateFn = std::uintptr_t(__fastcall *)(std::uintptr_t mode, int edx, float seconds);
dao::Detour<UpdateFn> g_update;

std::uintptr_t __fastcall Update(std::uintptr_t mode, int edx, float seconds)
{
    const std::int64_t now = NowMs();
    // A loading screen's first update: what the last one's presents came to, at debug.
    constexpr std::int64_t kBetweenScreensMs = 1000;
    if (now - g_lastUpdate.load(std::memory_order_relaxed) > kBetweenScreensMs)
    {
        const std::uint32_t presented = g_presented.exchange(0);
        const std::uint32_t skipped = g_skipped.exchange(0);
        spdlog::debug("loading present: over the last loading screen, {} frames presented and {} drawn and not "
                      "presented",
                      presented, skipped);
    }
    g_lastUpdate.store(now, std::memory_order_relaxed);
    return g_update.Original()(mode, edx, seconds);
}

// UpdateAndRenderLoadingGUITask::Execute: __thiscall (float seconds), `ret 4`; a turn of the
// loading screen's own task, on the render thread, which updates the loading screen, draws it
// and presents it, and answers in eax whether to be run again.
using TaskFn = std::uint32_t(__fastcall *)(std::uintptr_t task, int edx, float seconds);
dao::Detour<TaskFn> g_task;

std::uint32_t __fastcall Task(std::uintptr_t task, int edx, float seconds)
{
    g_taskThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
    const std::uint32_t result = g_task.Original()(task, edx, seconds);
    g_taskThread.store(0, std::memory_order_relaxed);
    return result;
}

// Whether this present is to be skipped: the loading mode is updating, and the last frame
// presented was presented less than the interval ago.
bool Skip()
{
    const std::int64_t now = NowMs();
    // The loading screen's own task presents it while an update of the loading mode is long at
    // its work, a save's read or an area's build, when no update is recent: its presents are a
    // loading screen's whenever they come.
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

} // namespace

void InstallLoadingPresentFix()
{
    std::array<char, 8> asked{};
    if (const DWORD length =
            GetEnvironmentVariableA("DAOLIB_LOADING_PRESENT", asked.data(), static_cast<DWORD>(asked.size()));
        length > 0 && length < asked.size())
    {
        g_intervalMs = std::strtol(asked.data(), nullptr, 10);
    }
    if (g_intervalMs <= 0)
    {
        spdlog::info("fixes: DAOLIB_LOADING_PRESENT=0, so every frame of a loading screen is presented as ever (issue "
                     "#73)");
        return;
    }
    // All four or none: a present skipped with no update seen would be skipped in play.
    if (!g_update.Attach(dao::addr::kGameModeLoading_Update, &Update) ||
        !g_task.Attach(dao::addr::kLoadingGUITask_Execute, &Task) ||
        !g_present.Attach(dao::addr::kD3DGraphicsDriver_Present, &Present) ||
        !g_presentChain.Attach(dao::addr::kD3DSwapChain_Present, &PresentChain))
    {
        g_presentChain.Detach();
        g_task.Detach();
        g_present.Detach();
        g_update.Detach();
        spdlog::warn("fixes: the present's detours could not be placed, so every frame of a loading screen is "
                     "presented as ever (issue #73)");
        return;
    }
    spdlog::info("fixes: a loading screen's frames are presented {} ms apart at the least, the rest drawn and not "
                 "presented, so its loading does not wait for the monitor (issue #73)",
                 g_intervalMs);
}

} // namespace fix
