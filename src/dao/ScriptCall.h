// Calls a script command by name, with arguments whose types are known only at run time. This is
// for a console, a test harness or a remote tool that receives a command and its arguments as
// text. If your code knows which command it calls, use the typed functions in
// dao/generated/ScriptApi.h instead.
//
//   using dao::script::Value;
//   const auto team = dao::script::Call("GetTeamId", {Value::Object(creature)});
//   if (team) spdlog::info("team {}", team.value.integer);
#pragma once

#include "dao/core/ScriptValue.h"
#include "dao/generated/ScriptTable.h"

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace dao::script
{

// A script value of any type: int, float, string, object, vector or resource.
using Value = core::ScriptValue;
// A script command's number, name, signature and default arguments.
using CommandInfo = core::ScriptCommandInfo;

// The outcome of a call by name.
struct CallResult
{
    bool ok = false;   // true if the command ran and its return value was read
    Value value;       // the return value, of the command's return type; Void if it returns nothing
    std::string error; // what went wrong, for a log; empty on success

    // Returns whether the call succeeded.
    explicit operator bool() const
    {
        return ok;
    }
};

// Calls a script command with the given arguments. Arguments you leave off take the command's
// defaults; a default of OBJECT_SELF is worked out at the time of the call. A returned array of
// ints, floats, strings or objects comes back as its elements.
// @param command the command, from dao/generated/ScriptTable.h
// @param args the arguments in the script's order. They are checked against the command's
// signature before the call.
// @return a result with `ok` false and `error` set if the arguments do not match, the command
// takes or returns a location, effect, event or command, or the call fails
//
// Call it only on the game thread, like dao::script::Invoke. Frame callbacks run there.
CallResult Call(const CommandInfo &command, std::span<const Value> args);
// Calls the script command with this name, as above. Returns a result with `ok` false if there is
// no such command.
CallResult Call(std::string_view name, std::span<const Value> args);
// Calls the script command with this name, taking the arguments as a braced list.
inline CallResult Call(std::string_view name, std::initializer_list<Value> args)
{
    return Call(name, std::span<const Value>(args.begin(), args.size()));
}

} // namespace dao::script
