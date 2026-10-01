// Calling a script command by its name, with values whose types are known
// only at run time: for a console, a test harness or a remote tool that
// takes a command and its arguments as text. Code that knows its command
// calls the typed wrapper instead (dao/generated/ScriptApi.h).
//
//   using dao::script::Value;
//   const auto team = dao::script::Call("GetTeamId", {Value::Object(creature)});
//   if (team) spdlog::info("team {}", team.value.integer);
//
// The command comes from dao/generated/ScriptTable.h, which says each one's
// parameter types and defaults; the arguments are checked against it
// first (dao/core/ScriptValue.h, CompleteArguments), and those left off
// take their defaults, OBJECT_SELF being read at the call. Then the call is
// made as dao::script::Invoke makes one. int, float, string, object, vector
// and resource move by name; an array answer of ints, floats, strings or
// objects comes back as its elements. A command with a location, effect,
// event or command anywhere in its signature is refused.
//
// On the scripts' thread only, as Invoke (a frame callback is).
#pragma once

#include "dao/core/ScriptValue.h"
#include "dao/generated/ScriptTable.h"

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace dao::script
{

using Value = core::ScriptValue;
using CommandInfo = core::ScriptCommandInfo;

struct CallResult
{
    bool ok = false;
    Value value; // the answer, of the command's answer type; Void for none
    std::string error;

    explicit operator bool() const
    {
        return ok;
    }
};

CallResult Call(const CommandInfo &command, std::span<const Value> args);
CallResult Call(std::string_view name, std::span<const Value> args);
inline CallResult Call(std::string_view name, std::initializer_list<Value> args)
{
    return Call(name, std::span<const Value>(args.begin(), args.size()));
}

} // namespace dao::script
