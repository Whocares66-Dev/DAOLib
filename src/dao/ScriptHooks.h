// Intercepts script commands. Your function runs whenever a script calls the command: it can read
// and change the arguments, let the game run the command, and change the value the script gets
// back.
//
//   dao::script::InterceptCommand(526 /* GetCombatState */, [](dao::script::CommandCall &call) {
//       const auto creature = call.Object(0);          // the first argument
//       const int result = call.Proceed();             // the game runs the command
//       if (result == dao::script::kOk && ForcedCalm(creature))
//           call.SetIntAnswer(0);                       // and the script gets 0 instead
//       return result;
//   });
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

// Runs `ready` once the game has filled in its table of script commands, or right away if it
// already has. The first call places a hook to find out when that happens.
void OnCommandsReady(std::function<void()> ready);

// Returns whether the game has filled in its table of script commands.
bool CommandsReady();

// Returns the ID of the game thread: the thread that filled in the command table, which scripts
// and frame callbacks run on. Call dao::script::Invoke on this thread.
// @return 0 until the table is filled in
std::uint32_t ScriptThreadId();

// One call of a script command, as an intercept receives it. Arguments are numbered from 0. A
// vector argument takes three places, so the arguments after it are numbered three higher.
class CommandCall
{
  public:
    // @param command the command's number
    // @param paramCount how many arguments the script passed
    // @param link this intercept's position in the command's chain of intercepts
    CommandCall(int command, int paramCount, int link) : command_(command), paramCount_(paramCount), link_(link)
    {
    }

    // Returns the command's number.
    int Command() const
    {
        return command_;
    }
    // Returns how many arguments the script passed.
    int ParamCount() const
    {
        return paramCount_;
    }

    // Reads argument `n` where it sits on the script stack, without removing it.
    // @return nullopt if the argument has a different type, or there are fewer than n + 1
    // arguments
    std::optional<std::int32_t> Int(int n) const;
    // Reads argument `n` as a float, or returns nullopt as Int does.
    std::optional<float> Float(int n) const;
    // Reads argument `n` as an object, or returns nullopt as Int does.
    std::optional<ObjectId> Object(int n) const;
    // Reads argument `n` as a string, or returns nullopt as Int does. The view is valid until the
    // call returns.
    std::optional<std::wstring_view> String(int n) const;
    // Reads argument `n` as a location, or returns null in the same cases as Int.
    const CScriptLocation *Location(int n) const;
    // Reads argument `n` as an effect, or returns null in the same cases as Int.
    const CGameEffect *Effect(int n) const;
    // Reads argument `n` as an event, or returns null in the same cases as Int.
    const CScriptEvent *Event(int n) const;
    // Reads argument `n` as a command, such as the second argument of AddCommand, or returns null
    // in the same cases as Int.
    const ::dao::Command *Command(int n) const;

    // Changes argument `n` before the command runs, as if the script had passed the new value.
    // Later intercepts and the game see the new value.
    // @return false if the argument has a different type, there are too few arguments, or
    // Proceed has already run
    bool SetInt(int n, std::int32_t value) const;
    // Changes float argument `n`, or returns false as SetInt does.
    bool SetFloat(int n, float value) const;
    // Changes object argument `n`, or returns false as SetInt does.
    bool SetObject(int n, ObjectId value) const;

    // Runs the next handler in the chain: another plugin's intercept, or the game's own command.
    // @return the handler's result, such as kOk
    //
    // The handler removes the arguments from the stack. An intercept that answers without calling
    // Proceed must pop the arguments itself (Stack::Pop*) and push its own answer.
    int Proceed();
    // Returns whether Proceed has run.
    bool Proceeded() const
    {
        return proceeded_;
    }
    // Returns what Proceed returned, once it has run.
    int ProceededResult() const
    {
        return result_;
    }

    // Reads the value the command left on top of the stack for the script. Call it after Proceed.
    // @return nullopt if that value is not an int
    std::optional<std::int32_t> IntAnswer() const;
    // Reads the value as a float, or returns nullopt if it is not one.
    std::optional<float> FloatAnswer() const;
    // Reads the value as an object, or returns nullopt if it is not one.
    std::optional<ObjectId> ObjectAnswer() const;

    // Replaces the value on top of the stack, so the script gets `value`. Call it after Proceed.
    // @return false if the value on top is not an int
    bool SetIntAnswer(std::int32_t value);
    // Replaces a float value, or returns false if the value on top is not a float.
    bool SetFloatAnswer(float value);
    // Replaces an object value, or returns false if the value on top is not an object.
    bool SetObjectAnswer(ObjectId value);

  private:
    int command_;
    int paramCount_;
    int link_; // this intercept's position in the command's chain within this module
    bool proceeded_ = false;
    int result_ = 0;
};

// A plugin's handler for a command. It returns the command's result, usually what Proceed
// returned.
using Intercept = std::function<int(CommandCall &)>;

// Runs `intercept` each time a script calls the command.
// @param command the command's number, listed in dev/SCRIPT_COMMANDS.md
// @return false if no command has that number, or `intercept` is empty
//
// Intercepts form a chain: each one's Proceed runs the intercept installed before it, from this
// plugin or another, and the first one installed runs the game's command. An intercept added
// before the game fills in its command table is installed when it does. Intercepts run on the
// game thread, in the middle of a script. If one throws, the rest of the chain answers instead.
bool InterceptCommand(int command, Intercept intercept);

} // namespace dao::script
