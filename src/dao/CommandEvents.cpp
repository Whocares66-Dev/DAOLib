// Command starts and ends, from hooks on the commands system's two thunks.
// What a plugin otherwise polls for (busy two frames running means done), and
// what EVENT_TYPE_COMMAND_COMPLETE reaches a script with only sometimes
// (dev/LIBRARY.md, the fifth session).
//
// Read 2026-09-30. An object's command step (0x0073D320, each frame) ends
// the command at the head of its queue when it is done (ClearCurrentCommand,
// 0x0073C730, with 1) or failed (with the failure), and starts the next
// (0x0073C7D0); ClearAllCommands and the scripts' ClearCurrentCommand end
// it through ClearCurrentCommand too, with -2. Each tells the commands
// system (the application's +0x1AC): a start through one thunk (0x0064A500,
// __thiscall (object, command), to the system's slot 2), an end through
// another (0x0064A510, __thiscall (object, command, result, flag), to its
// slot 3), and each thunk has that one caller. The hooks are on the thunks:
// a start is seen before the command runs, an end before the command is let
// go, so the Command is whole in both.
//
// Seen in play, 2026-09-30 (the Eluvian server's session of 66 minutes and
// eight fights, 1,699 ends): 1,286 successes, 315 cleared, 29
// COMMAND_FAILED_INVALID_PATH, 5 COMMAND_FAILED_TIMEOUT, 3
// COMMAND_FAILED_PATH_ACTION_REQUIRED, and 61 with result 2, every one a
// PlayAnimation: COMMAND_LOOPING. A later session had 5 with -1,
// COMMAND_FAILED. A start has its end for every type but ExecuteEffect: the
// one a dying creature starts, the frame it dies, never ends (40 of 136
// starts), so a plugin pairing the two must not wait on those.
// EVENT_TYPE_COMMAND_COMPLETE came 2,094 times in the session, so the event
// and the end are not one to one.
//
// And from the turn-based mod's play, 2026-10-01: eight melee attackers on
// one human-sized target all had room, each attacking from 1.9 m; with
// fourteen, attacks ended COMMAND_FAILED_NO_SPACE_IN_MELEE_RING 15 to 90 ms
// after they were queued, and the game's AI then queued a move to the
// target and a second attack itself, the move often ending
// COMMAND_FAILED_INVALID_PATH at once. A use-object ended COMMAND_FAILED
// (-1) all five times in that session, the two that opened their door
// among them: for that command -1 is how it ends, not a failure, and what
// the object did is the test of it.
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
