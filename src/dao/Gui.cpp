#include "dao/Gui.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/core/Msvc8.h"

#include <cstddef>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::gui
{

namespace
{

using AnyFSCommand = std::function<void(void *owner, std::string_view name, std::string_view args)>;

std::mutex g_mutex;
std::map<std::string, FSCommandHandler, std::less<>> g_fsCommands;
std::map<std::string, ExternalInterfaceHandler, std::less<>> g_externalCalls;
std::vector<AnyFSCommand> g_anyFSCommand;
std::vector<ExternalInterfaceHandler> g_anyExternal;

// The registry's lookups (dao/Addresses.h): __thiscall on the registry,
// the owner, the call's name, and its arguments, `ret 0xC` and `ret 0x10`.
using FSCommandFn = void(__fastcall *)(void *registry, int edx, void *owner, const char *command, const char *args);
using ExternalFn = void(__fastcall *)(void *registry, int edx, void *owner, const char *method, const Value *args,
                                      std::uint32_t count);
Detour<FSCommandFn> g_dispatchFSCommand;
Detour<ExternalFn> g_dispatchExternal;

void __fastcall DispatchFSCommand(void *registry, int edx, void *owner, const char *command, const char *args)
{
    const std::string_view name = command != nullptr ? command : "";
    const std::string_view text = args != nullptr ? args : "";
    std::vector<AnyFSCommand> observers;
    FSCommandHandler handler;
    {
        const std::lock_guard lock(g_mutex);
        observers = g_anyFSCommand;
        if (const auto found = g_fsCommands.find(name); found != g_fsCommands.end())
        {
            handler = found->second;
        }
    }
    for (const auto &observer : observers)
    {
        observer(owner, name, text);
    }
    if (handler)
    {
        handler(owner, text);
        return;
    }
    g_dispatchFSCommand.Original()(registry, edx, owner, command, args);
}

void __fastcall DispatchExternal(void *registry, int edx, void *owner, const char *method, const Value *args,
                                 std::uint32_t count)
{
    const Call call{owner, method != nullptr ? method : "",
                    args != nullptr ? std::span<const Value>(args, count) : std::span<const Value>{}};
    std::vector<ExternalInterfaceHandler> observers;
    ExternalInterfaceHandler handler;
    {
        const std::lock_guard lock(g_mutex);
        observers = g_anyExternal;
        if (const auto found = g_externalCalls.find(call.name); found != g_externalCalls.end())
        {
            handler = found->second;
        }
    }
    for (const auto &observer : observers)
    {
        observer(call);
    }
    if (handler)
    {
        handler(call);
        return;
    }
    g_dispatchExternal.Original()(registry, edx, owner, method, args, count);
}

// Under g_mutex.
void AttachLocked()
{
    if (g_dispatchFSCommand.Original() == nullptr)
    {
        g_dispatchFSCommand.Attach(addr::kGuiCommands_DispatchFSCommand, &DispatchFSCommand);
    }
    if (g_dispatchExternal.Original() == nullptr)
    {
        g_dispatchExternal.Attach(addr::kGuiCommands_DispatchExternalInterface, &DispatchExternal);
    }
}

} // namespace

namespace
{

// The engine's iterator over its set of movies (dao/Addresses.h).
struct SetIterator
{
    void *set = nullptr;
    std::byte *node = nullptr;
    bool valid = false;
};

void *Manager()
{
    return *At<void *>(addr::kGuiManager);
}

} // namespace

std::vector<void *> Owners()
{
    std::vector<void *> owners;
    void *manager = Manager();
    if (manager == nullptr)
    {
        return owners;
    }
    void *set = static_cast<std::byte *>(manager) + 0x70;
    using StepFn = void *(__fastcall *)(void *, int, SetIterator *);
    const auto begin = reinterpret_cast<StepFn>(Module::Address(addr::kGuiMovieSet_Begin));
    const auto next = reinterpret_cast<StepFn>(Module::Address(addr::kGuiMovieSet_Next));
    SetIterator it;
    begin(set, 0, &it);
    // Past this many, something is wrong: the game has a few dozen.
    for (int guard = 0; it.valid && it.node != nullptr && guard < 1024; ++guard)
    {
        owners.push_back(*reinterpret_cast<void **>(it.node + 0xC));
        next(set, 0, &it);
    }
    return owners;
}

std::wstring_view NameOf(const void *owner)
{
    return owner != nullptr ? static_cast<const core::WStringRep *>(owner)->View() : std::wstring_view{};
}

void *FindOwner(std::wstring_view name)
{
    for (void *owner : Owners())
    {
        if (NameOf(owner) == name)
        {
            return owner;
        }
    }
    return nullptr;
}

void *MovieOf(const void *owner)
{
    if (owner == nullptr)
    {
        return nullptr;
    }
    const auto movie = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(owner) + 0x1C);
    return movie ? reinterpret_cast<void *>(*movie) : nullptr;
}

namespace
{

template <class Fn> Fn Slot(void *view, std::size_t slot)
{
    return (*static_cast<Fn *const *>(view))[slot];
}

// SetVariableArray, slot 13: __thiscall (type, path, index, data, count,
// setType), `ret 0x18`.
bool SetArrayOf(void *view, std::int32_t kind, const char *path, std::uint32_t index, const void *data,
                std::uint32_t count, Movie::SetType type)
{
    if (view == nullptr)
    {
        return false;
    }
    using Fn = bool(__fastcall *)(void *, int, std::int32_t, const char *, std::uint32_t, const void *, std::uint32_t,
                                  Movie::SetType);
    return Slot<Fn>(view, 13)(view, 0, kind, path, index, data, count, type);
}

} // namespace

bool Movie::IsAvailable(const char *path) const
{
    using Fn = bool(__fastcall *)(void *, int, const char *);
    return view_ != nullptr && Slot<Fn>(view_, 10)(view_, 0, path);
}

bool Movie::SetVariable(const char *path, const Value &value, SetType type)
{
    using Fn = bool(__fastcall *)(void *, int, const char *, const Value *, SetType);
    return view_ != nullptr && Slot<Fn>(view_, 11)(view_, 0, path, &value, type);
}

bool Movie::SetArray(const char *path, std::span<const std::int32_t> values, std::uint32_t index, SetType type)
{
    return SetArrayOf(view_, 0, path, index, values.data(), static_cast<std::uint32_t>(values.size()), type);
}

bool Movie::SetArray(const char *path, std::span<const double> values, std::uint32_t index, SetType type)
{
    return SetArrayOf(view_, 1, path, index, values.data(), static_cast<std::uint32_t>(values.size()), type);
}

bool Movie::SetArray(const char *path, std::span<const wchar_t *const> values, std::uint32_t index, SetType type)
{
    // GFx takes the array of string pointers as its data.
    return SetArrayOf(view_, 4, path, index, static_cast<const void *>(values.data()),
                      static_cast<std::uint32_t>(values.size()), type);
}

bool Movie::SetArray(const char *path, std::span<const Value> values, std::uint32_t index, SetType type)
{
    return SetArrayOf(view_, 5, path, index, values.data(), static_cast<std::uint32_t>(values.size()), type);
}

bool Movie::SetArraySize(const char *path, std::uint32_t count, SetType type)
{
    using Fn = bool(__fastcall *)(void *, int, const char *, std::uint32_t, SetType);
    return view_ != nullptr && Slot<Fn>(view_, 14)(view_, 0, path, count, type);
}

std::uint32_t Movie::ArraySize(const char *path) const
{
    using Fn = std::uint32_t(__fastcall *)(void *, int, const char *);
    return view_ != nullptr ? Slot<Fn>(view_, 15)(view_, 0, path) : 0;
}

bool Movie::Invoke(const char *method, std::span<const Value> args)
{
    using Fn = bool(__fastcall *)(void *, int, const char *, Value *, const Value *, std::uint32_t);
    return view_ != nullptr &&
           Slot<Fn>(view_, 18)(view_, 0, method, nullptr, args.data(), static_cast<std::uint32_t>(args.size()));
}

namespace
{

// The application's GUI system, GUI::DragonAgeGUISystem, or null before
// it is made.
void *GuiSystem()
{
    const auto *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return nullptr;
    }
    const auto system = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + 0x158);
    if (!system || *system == 0)
    {
        return nullptr;
    }
    void *gui = reinterpret_cast<void *>(*system);
    return TypeName(gui).rfind("GUI::", 0) == 0 ? gui : nullptr;
}

// The ids a plugin's screens take, past the game's 0-0x3F.
std::uint32_t g_nextScreenId = 0x100;

// The GUI system's by-name slots (17 open, 18 close, 19 hide), each
// __thiscall (const std::wstring &name), `ret 4`.
bool ByName(std::size_t slot, std::wstring_view name)
{
    void *gui = GuiSystem();
    if (gui == nullptr)
    {
        return false;
    }
    WString text(name);
    using Fn = void(__fastcall *)(void *, int, const core::WStringRep *);
    Slot<Fn>(gui, slot)(gui, 0, text.Engine());
    return true;
}

// The id the GUI system knows a scene by, or -1 (its slot 12).
std::int32_t SceneId(void *gui, std::wstring_view name)
{
    WString text(name);
    using Fn = std::int32_t(__fastcall *)(void *, int, const core::WStringRep *);
    return Slot<Fn>(gui, 12)(gui, 0, text.Engine());
}

} // namespace

bool AddScreen(std::wstring_view name, std::wstring_view movie, std::int32_t layer, std::uint32_t flags)
{
    void *gui = GuiSystem();
    if (gui == nullptr || SceneId(gui, name) != -1)
    {
        return false;
    }
    // As the GUI system makes its own: 0xC0 bytes from the engine's
    // allocator, GUIScene(name, filename, layer, b, flags), AddScene. The
    // GUI system owns it from there.
    using NewFn = void *(__cdecl *)(std::size_t);
    void *scene = reinterpret_cast<NewFn>(Module::Address(addr::kOperatorNew))(0xC0);
    if (scene == nullptr)
    {
        return false;
    }
    WString sceneName(name);
    WString filename(movie);
    using SceneFn = void *(__fastcall *)(void *, int, const core::WStringRep *, const core::WStringRep *, std::int32_t,
                                         std::int32_t, std::uint32_t);
    reinterpret_cast<SceneFn>(Module::Address(addr::kGUIScene_Constructor))(scene, 0, sceneName.Engine(),
                                                                            filename.Engine(), layer, 0, flags);
    const std::uint32_t id = g_nextScreenId++;
    using AddFn = void(__fastcall *)(void *, int, std::uint32_t, void *);
    reinterpret_cast<AddFn>(Module::Address(addr::kGUISystem_AddScene))(gui, 0, id, scene);
    return SceneId(gui, name) == static_cast<std::int32_t>(id);
}

bool ShowScreen(std::wstring_view name)
{
    return ByName(17, name);
}

bool HideScreen(std::wstring_view name)
{
    return ByName(19, name);
}

bool CloseScreen(std::wstring_view name)
{
    return ByName(18, name);
}

bool ReturnFSCommand(void *owner, std::span<const std::int32_t> values)
{
    return Movie{MovieOf(owner)}.SetArray("_root.fsc_retval", values);
}

void Call::Return(const Value &answer) const
{
    void *movie = MovieOf(owner);
    if (movie == nullptr)
    {
        return;
    }
    using Fn = void(__fastcall *)(void *, int, const Value *);
    const auto *vtable = *static_cast<Fn *const *>(movie);
    vtable[42](movie, 0, &answer);
}

void OnFSCommand(std::string name, FSCommandHandler handler)
{
    const std::lock_guard lock(g_mutex);
    g_fsCommands.insert_or_assign(std::move(name), std::move(handler));
    AttachLocked();
}

void OnExternalInterface(std::string name, ExternalInterfaceHandler handler)
{
    const std::lock_guard lock(g_mutex);
    g_externalCalls.insert_or_assign(std::move(name), std::move(handler));
    AttachLocked();
}

void OnAnyFSCommand(std::function<void(void *owner, std::string_view name, std::string_view args)> observer)
{
    const std::lock_guard lock(g_mutex);
    g_anyFSCommand.push_back(std::move(observer));
    AttachLocked();
}

void OnAnyExternalInterface(ExternalInterfaceHandler observer)
{
    const std::lock_guard lock(g_mutex);
    g_anyExternal.push_back(std::move(observer));
    AttachLocked();
}

} // namespace dao::gui
