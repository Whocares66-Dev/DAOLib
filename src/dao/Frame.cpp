#include "dao/Frame.h"

#include "dao/Addresses.h"
#include "dao/Guard.h"
#include "dao/Hook.h"

#include <Windows.h>

#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

namespace dao
{

namespace
{

std::mutex g_mutex;
std::vector<std::function<void()>> g_callbacks;
std::atomic<std::uint32_t> g_thread{0};
std::atomic<std::uint64_t> g_frames{0};

// The per-frame update: __thiscall, no arguments, true to go on running.
using UpdateFn = bool(__fastcall *)(void *game, int edx);
Detour<UpdateFn> g_update;

bool __fastcall Update(void *game, int edx)
{
    g_thread = GetCurrentThreadId();
    ++g_frames;
    std::vector<std::function<void()>> callbacks;
    {
        const std::lock_guard lock(g_mutex);
        callbacks = g_callbacks;
    }
    for (auto &callback : callbacks)
    {
        detail::Guard("a frame callback", callback);
    }
    return g_update.Original()(game, edx);
}

} // namespace

void OnFrame(std::function<void()> callback)
{
    const std::lock_guard lock(g_mutex);
    g_callbacks.push_back(std::move(callback));
    if (g_update.Original() == nullptr)
    {
        g_update.Attach(addr::kEclipseGame_Update, &Update);
    }
}

std::uint32_t FrameThreadId()
{
    return g_thread;
}

std::uint64_t FrameCount()
{
    return g_frames;
}

} // namespace dao
