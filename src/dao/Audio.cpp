// The sound events: a hook on fmod_event.dll's FMOD::Event::start.
//
// Read 2026-10-21 in the GOG build's fmod_event.dll (FMOD Ex EventSystem 4.24.24, the exports
// with capstone) and in the exe's imports (tools\daoexe.py --imports, dev/AUDIO.md). The exe
// imports the dll statically, so it is mapped before any plugin loads. The game imports
// `?start@Event@FMOD@@QAG?AW4FMOD_RESULT@@XZ` (RVA 0x11040): a __stdcall method, which MSVC
// takes with `this` on the stack and `ret 4`, not in ecx. The export checks the handle and
// jumps through the event's vtable. It is the one place a start passes, and Detours hooks its
// first bytes. The reads afterwards use the dll's C API, plain __stdcall:
// `_FMOD_Event_GetInfo@16` (index, name, info: with a null index and info it copies the
// template's name pointer, no allocation), `_FMOD_Event_Get3DAttributes@16` (a copy of two
// stored vectors), `_FMOD_Event_GetParentGroup@8`, `_FMOD_EventGroup_GetInfo@12`,
// `_FMOD_EventGroup_GetParentGroup@8`, `_FMOD_EventGroup_GetParentProject@8` and
// `_FMOD_EventProject_GetInfo@12`. The C++ EventGroup and EventProject methods are not exported
// (only their EventGroupI/EventProjectI bodies are), which is why the path is built through the
// C API. The C wrappers go through the vtable and not through the C++ export, so a call of
// FMOD_Event_Start would not be seen.
//
// Not seen in play.
#include "dao/Audio.h"

#include "dao/Guard.h"
#include "dao/Hook.h"

#include <Windows.h>

#include <spdlog/spdlog.h>

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace dao::audio
{

namespace
{

constexpr int kFmodOk = 0;
// FMOD_EVENTPROPERTY_MODE, the event's FMOD_MODE, and its FMOD_3D bit. The engine reads the same
// property by the same index and tests the same bit (dev/AUDIO.md, the sound object's properties).
constexpr int kPropertyMode = 14;
constexpr unsigned kMode3D = 0x10;
// A group nests this deep at most in the game's projects; a longer chain is cut short.
constexpr int kMaxDepth = 16;

struct FmodVector
{
    float x, y, z;
};

using StartFn = int(__stdcall *)(void *);
using GetInfoFn = int(__stdcall *)(void *, int *, char **, void *);
using Get3DFn = int(__stdcall *)(void *, FmodVector *, FmodVector *, FmodVector *);
using GroupFn = int(__stdcall *)(void *, void **);
using NamedInfoFn = int(__stdcall *)(void *, int *, char **);
using PropertyFn = int(__stdcall *)(void *, int, void *, bool);

struct Fmod
{
    GetInfoFn eventInfo = nullptr;
    Get3DFn event3D = nullptr;
    GroupFn eventGroup = nullptr;
    NamedInfoFn groupInfo = nullptr;
    GroupFn groupParent = nullptr;
    GroupFn groupProject = nullptr;
    NamedInfoFn projectInfo = nullptr;
    PropertyFn eventProperty = nullptr;
};

Fmod g_fmod;
Detour<StartFn> g_start;
std::mutex g_lock;
std::vector<std::function<void(const EventStart &)>> g_observers;

// An export of fmod_event.dll by name.
template <class T> bool Resolve(HMODULE dll, const char *name, T &out)
{
    out = reinterpret_cast<T>(GetProcAddress(dll, name));
    return out != nullptr;
}

bool ResolveAll(HMODULE dll, StartFn &start)
{
    return Resolve(dll, "?start@Event@FMOD@@QAG?AW4FMOD_RESULT@@XZ", start) &&
           Resolve(dll, "_FMOD_Event_GetInfo@16", g_fmod.eventInfo) &&
           Resolve(dll, "_FMOD_Event_Get3DAttributes@16", g_fmod.event3D) &&
           Resolve(dll, "_FMOD_Event_GetParentGroup@8", g_fmod.eventGroup) &&
           Resolve(dll, "_FMOD_EventGroup_GetInfo@12", g_fmod.groupInfo) &&
           Resolve(dll, "_FMOD_EventGroup_GetParentGroup@8", g_fmod.groupParent) &&
           Resolve(dll, "_FMOD_EventGroup_GetParentProject@8", g_fmod.groupProject) &&
           Resolve(dll, "_FMOD_EventProject_GetInfo@12", g_fmod.projectInfo) &&
           Resolve(dll, "_FMOD_Event_GetPropertyByIndex@16", g_fmod.eventProperty);
}

// Builds "project/group/.../event" into `path`, for an event whose own name is `name`.
void BuildPath(void *event, const char *name, std::string &path)
{
    const char *parts[kMaxDepth];
    int count = 0;
    void *group = nullptr;
    void *project = nullptr;
    if (g_fmod.eventGroup(event, &group) == kFmodOk)
    {
        while (group != nullptr && count < kMaxDepth)
        {
            char *groupName = nullptr;
            void *parent = nullptr;
            if (g_fmod.groupInfo(group, nullptr, &groupName) != kFmodOk || groupName == nullptr)
            {
                break;
            }
            parts[count++] = groupName;
            if (g_fmod.groupParent(group, &parent) != kFmodOk || parent == nullptr)
            {
                // The top group: its project is the path's first part.
                if (g_fmod.groupProject(group, &project) != kFmodOk)
                {
                    project = nullptr;
                }
                break;
            }
            group = parent;
        }
    }
    path.clear();
    char *projectName = nullptr;
    if (project != nullptr && g_fmod.projectInfo(project, nullptr, &projectName) == kFmodOk && projectName != nullptr)
    {
        path += projectName;
        path += '/';
    }
    for (int i = count - 1; i >= 0; --i)
    {
        path += parts[i];
        path += '/';
    }
    path += name;
}

void Report(void *event)
{
    thread_local std::string path;
    EventStart sound;
    sound.when = std::chrono::steady_clock::now();
    sound.event = event;
    char *name = nullptr;
    if (g_fmod.eventInfo(event, nullptr, &name, nullptr) != kFmodOk)
    {
        name = nullptr;
    }
    unsigned mode = 0;
    FmodVector at{};
    if (g_fmod.eventProperty(event, kPropertyMode, &mode, true) == kFmodOk && (mode & kMode3D) != 0 &&
        g_fmod.event3D(event, &at, nullptr, nullptr) == kFmodOk)
    {
        sound.position[0] = at.x;
        sound.position[1] = at.y;
        sound.position[2] = at.z;
        sound.positional = true;
    }
    if (name != nullptr)
    {
        sound.name = name;
        BuildPath(event, name, path);
        sound.path = path;
    }
    std::vector<std::function<void(const EventStart &)>> observers;
    {
        const std::lock_guard lock(g_lock);
        observers = g_observers;
    }
    for (const auto &observer : observers)
    {
        detail::Guard("a sound event callback", [&] { observer(sound); });
    }
}

int __stdcall Start(void *event)
{
    const int result = g_start.Original()(event);
    if (result == kFmodOk && event != nullptr)
    {
        Report(event);
    }
    return result;
}

} // namespace

void OnEventStart(std::function<void(const EventStart &sound)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] {
        // Mapped by the exe's own imports before any plugin runs.
        const HMODULE dll = GetModuleHandleW(L"fmod_event.dll");
        StartFn start = nullptr;
        if (dll == nullptr || !ResolveAll(dll, start) || !g_start.AttachTo(start, &Start))
        {
            spdlog::error("sound events: fmod_event.dll's Event::start could not be hooked");
        }
    });
    const std::lock_guard lock(g_lock);
    g_observers.push_back(std::move(observer));
}

} // namespace dao::audio
