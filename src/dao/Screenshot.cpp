// dao/Screenshot.h. Read 2026-10-15 for DAOLib #133 in GOG's exe: GameModeController's
// TakeScreenshotMessage handler (0x00642050) names the file and asks the graphics for the
// picture (Eclipse::IGraphicsInternal's slot 35, 0x00527F90). With the message's flag 0, as the
// key sends it, that queues a request on the scene (its +0x58, 0x0045A320), taken when a frame is
// drawn, and the file is written by D3DX's D3DXSaveSurfaceToFileW through a D3DSaveSurfaceTask
// (its vftable 0x00B2EC38). So the frame the picture is of may already be drawn when the message
// comes, a plugin's screen in it. Here the message is taken, the plugins hide what they will, two
// frames go by, the message is sent again as the key made it, and the plugins are told once
// D3DX has written a file.
#include "dao/Screenshot.h"

#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Messages.h"

#include <spdlog/spdlog.h>

#include <windows.h>

#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::screenshot
{

namespace
{

struct Listener
{
    std::function<void()> before;
    std::function<void()> after;
};

std::mutex g_mutex;
std::vector<Listener> g_listeners; // under g_mutex

// The screenshot held: frames until the message goes on, then frames left to wait for the file.
constexpr int kFramesHidden = 2;
constexpr int kFramesForTheFile = 300;
enum class Stage
{
    Idle,
    Hiding,  // the plugins told; the message waits for frames drawn without what they hid
    Writing, // the message sent on; waiting for D3DX to write the file
};
Stage g_stage = Stage::Idle; // the game thread's
int g_frames = 0;
bool g_resending = false; // the game thread's: the message sent on is let through
std::atomic<bool> g_written{false};

using SaveSurfaceFn = HRESULT(WINAPI *)(LPCWSTR file, DWORD format, void *surface, const void *palette,
                                        const RECT *rect);
Detour<SaveSurfaceFn> g_saveSurface;

HRESULT WINAPI SaveSurface(LPCWSTR file, DWORD format, void *surface, const void *palette, const RECT *rect)
{
    const HRESULT result = g_saveSurface.Original()(file, format, surface, palette, rect);
    g_written.store(true, std::memory_order_relaxed);
    return result;
}

std::vector<Listener> Listeners()
{
    const std::lock_guard lock(g_mutex);
    return g_listeners;
}

void Tell(bool before)
{
    for (const Listener &listener : Listeners())
    {
        detail::Guard(before ? "a screenshot's before" : "a screenshot's after",
                      before ? listener.before : listener.after);
    }
}

void SendOn()
{
    auto *app = CClientExoApp::Get();
    auto *modes = app != nullptr ? app->GameModes() : nullptr;
    if (modes == nullptr)
    {
        return;
    }
    g_resending = true;
    messages::Send(modes, messages::Make<messages::TakeScreenshotMessage>());
    g_resending = false;
}

void Step()
{
    switch (g_stage)
    {
    case Stage::Idle:
        return;
    case Stage::Hiding:
        if (++g_frames >= kFramesHidden)
        {
            g_written.store(false, std::memory_order_relaxed);
            g_stage = Stage::Writing;
            g_frames = 0;
            SendOn();
        }
        return;
    case Stage::Writing:
        if (g_written.load(std::memory_order_relaxed) || ++g_frames >= kFramesForTheFile)
        {
            if (!g_written.load(std::memory_order_relaxed))
            {
                spdlog::warn("screenshot: no file written in {} frames; the plugins are told all the same",
                             kFramesForTheFile);
            }
            g_stage = Stage::Idle;
            Tell(false);
        }
        return;
    }
}

bool Taken(void * /*receiver*/, const messages::TakeScreenshotMessage &message)
{
    if (g_resending)
    {
        return false;
    }
    const bool asTheKeySendsIt =
        message.flag == 0 &&
        std::ranges::all_of(message.strings, [](const core::WStringRep &s) { return s.size == 0; });
    if (!asTheKeySendsIt)
    {
        return false;
    }
    if (g_stage == Stage::Idle)
    {
        g_stage = Stage::Hiding;
        g_frames = 0;
        Tell(true);
    }
    return true; // a second while one is held is the same picture
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        if (HMODULE d3dx = GetModuleHandleW(L"d3dx9_36.dll"); d3dx != nullptr)
        {
            g_saveSurface.AttachTo(reinterpret_cast<SaveSurfaceFn>(GetProcAddress(d3dx, "D3DXSaveSurfaceToFileW")),
                                   &SaveSurface);
        }
        messages::InterceptMessage<messages::TakeScreenshotMessage>(&Taken);
        OnFrame(&Step);
    });
}

} // namespace

void OnScreenshot(std::function<void()> before, std::function<void()> after)
{
    {
        const std::lock_guard lock(g_mutex);
        g_listeners.push_back({std::move(before), std::move(after)});
    }
    Attach();
}

} // namespace dao::screenshot
