#include "dao/CommandEvents.h"

#include "dao/Addresses.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/generated/ScriptConstants.h"

#include <mutex>
#include <utility>
#include <vector>

namespace dao::commands
{

namespace
{

std::mutex g_lock;
std::vector<std::function<void(ObjectId, const Command &)>> g_starts;
std::vector<std::function<void(ObjectId, const Command &, std::int32_t)>> g_ends;

template <class T> std::vector<T> Copy(const std::vector<T> &callbacks)
{
    const std::lock_guard lock(g_lock);
    return callbacks;
}

ObjectId IdOf(const CCObject *object)
{
    return object != nullptr ? object->Id() : kInvalidObject;
}

// The commands system's start: __thiscall (object, command).
using StartFn = void(__fastcall *)(void *, int, CCObject *, Command *);
Detour<StartFn> g_start;

void __fastcall Start(void *system, int edx, CCObject *object, Command *command)
{
    if (command != nullptr)
    {
        for (const auto &observer : Copy(g_starts))
        {
            detail::Guard("a command start callback", [&] { observer(IdOf(object), *command); });
        }
    }
    g_start.Original()(system, edx, object, command);
}

// The commands system's end: __thiscall (object, command, result, flag).
using EndFn = void(__fastcall *)(void *, int, CCObject *, Command *, std::int32_t, std::int32_t);
Detour<EndFn> g_end;

void __fastcall End(void *system, int edx, CCObject *object, Command *command, std::int32_t result, std::int32_t flag)
{
    if (command != nullptr)
    {
        for (const auto &observer : Copy(g_ends))
        {
            detail::Guard("a command end callback", [&] { observer(IdOf(object), *command, result); });
        }
    }
    g_end.Original()(system, edx, object, command, result, flag);
}

} // namespace

std::string_view ResultName(std::int32_t result)
{
    if (result == kSuccess)
    {
        return "success";
    }
    if (result == kLooping)
    {
        return "looping";
    }
    if (result == kFailed)
    {
        return "COMMAND_FAILED";
    }
    return script::Name(static_cast<script::CommandFailed>(result));
}

void OnStart(std::function<void(ObjectId object, const Command &command)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] { g_start.Attach(addr::kCommands_Start, &Start); });
    const std::lock_guard lock(g_lock);
    g_starts.push_back(std::move(observer));
}

void OnEnd(std::function<void(ObjectId object, const Command &command, std::int32_t result)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] { g_end.Attach(addr::kCommands_End, &End); });
    const std::lock_guard lock(g_lock);
    g_ends.push_back(std::move(observer));
}

} // namespace dao::commands
