#include "dao/Gui.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"

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
