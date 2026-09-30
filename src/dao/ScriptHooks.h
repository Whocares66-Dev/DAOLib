// Intercepting script commands: a plugin's function runs whenever a script
// calls a command, sees the call's arguments, and decides what happens.
//
//   dao::script::InterceptCommand(526 /* GetCombatState */, [](dao::script::CommandCall &call) {
//       const auto creature = call.Object(0);          // the first argument
//       const int result = call.Proceed();             // the engine answers
//       if (result == dao::script::kOk && ForcedCalm(creature))
//           call.SetIntAnswer(0);                       // and the script sees 0
//       return result;
//   });
//
// An intercept takes the command's slot in the engine's table (dao/
// Script.h) and keeps the handler it found there, which Proceed runs, so
// intercepts from several plugins chain: each Proceeds into the one
// installed before it, the last into the engine. One that answers without
// Proceed pops the arguments itself (Stack::Pop*) and pushes its answer.
//
// The table is filled after plugins load, so an intercept made at load is
// installed the moment the engine has made it (OnCommandsReady); one made
// later is installed at once. Intercepts run on the scripts' thread, in
// the middle of a script.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Script.h"
#include "dao/Structures.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace dao::script
{

// Runs `ready` once the engine has filled its command table, or at once if
// it has. The first call detours CVirtualMachineCommands::
// InitializeCommands (dao/Addresses.h) to find out.
void OnCommandsReady(std::function<void()> ready);

// True once the table is there.
bool CommandsReady();

// The thread that filled the command table, which is the one scripts run
// on (seen in play 2026-09-30: every script command on the thread that ran
// InitializeCommands); 0 until then. dao::script::Invoke belongs on it.
std::uint32_t ScriptThreadId();

// A call of a script command, as an intercept gets it.
class CommandCall
{
  public:
    CommandCall(int command, int paramCount, int link) : command_(command), paramCount_(paramCount), link_(link)
    {
    }

    int Command() const
    {
        return command_;
    }
    int ParamCount() const
    {
        return paramCount_;
    }

    // The n-th argument as the script passed it (0 the first), read in
    // place; nothing if it has another type or the call has fewer. A
    // vector takes three places, so arguments after one move down three.
    std::optional<std::int32_t> Int(int n) const;
    std::optional<float> Float(int n) const;
    std::optional<ObjectId> Object(int n) const;
    std::optional<std::wstring_view> String(int n) const; // valid until the call returns
    const CScriptLocation *Location(int n) const;
    const CGameEffect *Effect(int n) const;
    const CScriptEvent *Event(int n) const;

    // Runs the handler this intercept replaced: another plugin's intercept,
    // or the engine's command. Its result is the handler's.
    int Proceed();
    bool Proceeded() const
    {
        return proceeded_;
    }

    // After Proceed: the answer the command left on top of the stack.
    std::optional<std::int32_t> IntAnswer() const;
    std::optional<float> FloatAnswer() const;
    std::optional<ObjectId> ObjectAnswer() const;

    // Replace that answer (popped and pushed again). False if the answer
    // on top is not of that type.
    bool SetIntAnswer(std::int32_t value);
    bool SetFloatAnswer(float value);
    bool SetObjectAnswer(ObjectId value);

  private:
    int command_;
    int paramCount_;
    int link_; // this intercept's place in the command's chain within this module
    bool proceeded_ = false;
};

using Intercept = std::function<int(CommandCall &)>;

// Intercepts a command. Several from one plugin chain in the order made;
// each plugin's chain Proceeds into what was in the table before it. False
// for a number no command has.
bool InterceptCommand(int command, Intercept intercept);

} // namespace dao::script
