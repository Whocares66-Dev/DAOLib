// A log of the game window's close requests and activations (DAOLib #199).
// Twice a copy of the game exited as a window close does, with nobody having
// closed its window (Eluvian #149); the lines here tell the sources apart:
// an SC_CLOSE (the title bar's close, Alt+F4), a WM_CLOSE and whether it was
// sent by another thread or posted, a session's end, another application
// taking or giving the front (with its process), and the keys that close or
// leave a game in the 2 s after one. dev/CRASHES.md says how to read them.
//
// Why two hooks and not a subclass. A subclass (GWLP_WNDPROC) sees one
// window's messages, sent and posted alike, but only until the game makes the
// window anew, and it replaces a procedure the game or a plugin may replace
// again or restore over ours, which would unlink ours or break a chain. Hooks
// are per thread, not per window, so a window made, re-made or destroyed on
// the thread is covered all the same, and nothing of the game's is changed:
// WH_CALLWNDPROC sees every message sent to a window of the thread, before
// its procedure, and WH_GETMESSAGE every message posted to it as it is taken
// off the queue. Between them nothing is seen twice (a posted message
// dispatched is not "sent" to the hook), and both run on the window's thread,
// so the state below needs no lock. Hooks are put on the thread from another,
// which the system allows within a process, so none is placed before the
// window exists, and a window made on another thread is hooked as soon
// as the poll finds it.
//
// Cost: the hook runs for every message, so it switches on the message first
// and returns; nothing is allocated, and no call is made, for any that is not
// one of those listed. Only the events themselves reach the logger.
#include "loader/WindowLog.h"

#include "dao/Log.h"
#include "dao/String.h"
#include "dao/core/WindowWatch.h"
#include "loader/CrashLog.h"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <string>

namespace loader::windowlog
{
namespace
{

// Touched only on the hooked thread, by both hooks.
dao::core::WindowWatch g_watch;

HHOOK g_sent = nullptr;
HHOOK g_posted = nullptr;
DWORD g_thread = 0;

// Which hook a message came by.
enum class Way
{
    Sent,
    Posted,
};

// The process and the image of a thread, quietly: "?" for what could not be
// read (another user's process, a thread already gone).
std::string ThreadOwner(DWORD threadId, DWORD &process)
{
    process = 0;
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadId);
    if (thread == nullptr)
    {
        return "?";
    }
    process = GetProcessIdOfThread(thread);
    CloseHandle(thread);
    if (process == 0)
    {
        return "?";
    }
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process);
    if (handle == nullptr)
    {
        return "?";
    }
    std::array<wchar_t, MAX_PATH * 2> image{};
    auto size = static_cast<DWORD>(image.size());
    const bool ok = QueryFullProcessImageNameW(handle, 0, image.data(), &size) != FALSE;
    CloseHandle(handle);
    return ok ? dao::Utf8(std::wstring_view(image.data(), size)) : "?";
}

// "sent by another thread", "sent by this thread" or "posted".
const char *Origin(Way way)
{
    if (way == Way::Posted)
    {
        return "posted";
    }
    const DWORD flags = InSendMessageEx(nullptr);
    return (flags & (ISMEX_SEND | ISMEX_NOTIFY | ISMEX_CALLBACK)) != 0 ? "sent by another thread"
                                                                       : "sent by this thread";
}

void LogActivation(HWND window, WPARAM wParam, LPARAM lParam)
{
    const auto other = static_cast<DWORD>(lParam);
    DWORD process = 0;
    const std::string image = other != 0 ? ThreadOwner(other, process) : std::string("?");
    spdlog::info("window {:#x}: WM_ACTIVATEAPP, {}; other thread {} of process {} ({})",
                 reinterpret_cast<std::uintptr_t>(window), wParam != FALSE ? "activated" : "deactivated", other,
                 process, image);
}

void LogKey(HWND window, WPARAM key)
{
    const char *name = "Alt";
    if (key == VK_ESCAPE)
    {
        name = "Esc";
    }
    else if (key == VK_F4)
    {
        name = "F4";
    }
    spdlog::info("window {:#x}: {} keydown within 2 s of an activation", reinterpret_cast<std::uintptr_t>(window),
                 name);
}

bool IsWatchedKey(WPARAM key)
{
    return key == VK_ESCAPE || key == VK_F4 || key == VK_MENU;
}

// One message, for either hook. Everything not named here returns at the
// switch.
void Observe(Way way, HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE)
        {
            g_watch.SysClose(GetTickCount64());
            spdlog::info("window {:#x}: WM_SYSCOMMAND SC_CLOSE, lParam {:#x}, {}",
                         reinterpret_cast<std::uintptr_t>(window), static_cast<std::uint64_t>(lParam), Origin(way));
        }
        break;
    case WM_CLOSE:
        spdlog::info("window {:#x}: WM_CLOSE, {}, {} in the second before", reinterpret_cast<std::uintptr_t>(window),
                     Origin(way),
                     g_watch.CloseFollowsSysClose(GetTickCount64()) ? "after an SC_CLOSE" : "with no SC_CLOSE");
        break;
    case WM_QUERYENDSESSION:
    case WM_ENDSESSION:
        spdlog::info("window {:#x}: {}, wParam {:#x}, lParam {:#x}, {}", reinterpret_cast<std::uintptr_t>(window),
                     message == WM_ENDSESSION ? "WM_ENDSESSION" : "WM_QUERYENDSESSION",
                     static_cast<std::uint64_t>(wParam), static_cast<std::uint64_t>(lParam), Origin(way));
        break;
    case WM_ACTIVATEAPP:
        if (wParam != FALSE)
        {
            g_watch.Activated(GetTickCount64());
        }
        LogActivation(window, wParam, lParam);
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        // Not a key held down: the repeats of Alt would fill the log.
        if (IsWatchedKey(wParam) && (lParam & (1 << 30)) == 0 && g_watch.KeySoonAfterActivation(GetTickCount64()))
        {
            LogKey(window, wParam);
        }
        break;
    default:
        break;
    }
}

LRESULT CALLBACK SentHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        const auto *msg = reinterpret_cast<const CWPSTRUCT *>(lParam);
        Observe(Way::Sent, msg->hwnd, msg->message, msg->wParam, msg->lParam);
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK PostedHook(int code, WPARAM wParam, LPARAM lParam)
{
    // Only a message taken off the queue, not one peeked at and left.
    if (code == HC_ACTION && wParam == PM_REMOVE)
    {
        const auto *msg = reinterpret_cast<const MSG *>(lParam);
        Observe(Way::Posted, msg->hwnd, msg->message, msg->wParam, msg->lParam);
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void Unhook()
{
    if (g_sent != nullptr)
    {
        UnhookWindowsHookEx(g_sent);
    }
    if (g_posted != nullptr)
    {
        UnhookWindowsHookEx(g_posted);
    }
    g_sent = nullptr;
    g_posted = nullptr;
    g_thread = 0;
}

void Hook(DWORD thread)
{
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&SentHook), &self);
    g_sent = SetWindowsHookExW(WH_CALLWNDPROC, &SentHook, self, thread);
    g_posted = SetWindowsHookExW(WH_GETMESSAGE, &PostedHook, self, thread);
    if (g_sent == nullptr || g_posted == nullptr)
    {
        spdlog::warn("window log: hooks on thread {} failed, error {}", thread, GetLastError());
        Unhook();
        return;
    }
    g_thread = thread;
    spdlog::info("window log: close requests and activations of the game's window logged (thread {})", thread);
}

// Once a second: the window's thread, hooked if it is not already, and again
// if the window is ever made on another.
DWORD WINAPI Poll(LPVOID /*unused*/)
{
    for (;;)
    {
        if (HWND window = loader::crashlog::GameWindow(); window != nullptr)
        {
            const DWORD thread = GetWindowThreadProcessId(window, nullptr);
            if (thread != 0 && thread != g_thread)
            {
                Unhook();
                Hook(thread);
            }
        }
        Sleep(1000);
    }
}

} // namespace

void Start()
{
    HANDLE thread = CreateThread(nullptr, 0, &Poll, nullptr, 0, nullptr);
    if (thread == nullptr)
    {
        spdlog::warn("window log: its thread could not start, error {}", GetLastError());
        return;
    }
    CloseHandle(thread);
}

} // namespace loader::windowlog
