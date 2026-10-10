// Script commands from C++. Read the engine's table of command handlers, put your own handler in
// a slot to replace a command or add a new one, and use the script VM's typed stack, which a
// handler pops its arguments from and pushes its result to. Invoke calls any command from C++.
//
//   int __fastcall MyCommand(void *commands, int edx, int id, int paramCount)
//   {
//       dao::ObjectId creature = 0;
//       if (!dao::script::Stack::PopObject(creature))
//           return dao::script::kStackUnderflow;
//       ...
//       dao::script::Stack::PushInt(result);
//       return dao::script::kOk;
//   }
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"
#include "dao/core/ScriptArray.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dao
{
class Command; // a command queued on an object (dao/Objects.h)
} // namespace dao

namespace dao::script
{

// Notes on the error numbers: they are NWN's (NWN:EE's Constants/VirtualMachine.hpp; negative
// string refs). The exe's getters return -639 when their pop fails and -638 when their push does
// (GetTeamId, 0x0065BCD0); ExecuteCommand returns -642 for a number with no handler.

// The number of slots in the command table. ExecuteCommand rejects a command number of 0x35F or
// more.
inline constexpr int kSlots = 0x35F;

inline constexpr int kOk = 0;                // what a handler returns when it succeeds
inline constexpr int kStackOverflow = -638;  // what a handler returns when a push fails
inline constexpr int kStackUnderflow = -639; // what a handler returns when a pop fails
inline constexpr int kInvalidCommand = -642; // what ExecuteCommand returns for an empty slot

// A command handler. It pops its arguments off Stack, pushes its result, and returns kOk or one
// of the error codes above. The engine calls it as a __thiscall member of the commands object, so
// it is declared __fastcall: ecx arrives as `commands`, and edx is unused.
// @param commands the script commands object (CClientExoApp::Commands)
// @param id the command's number
// @param paramCount the number of arguments the script pushed
using Handler = int(__fastcall *)(void *commands, int edx, int id, int paramCount);

// Returns the command table: kSlots handlers, indexed by command number.
// @return the table, or null before the engine has created it
//
// The engine fills the table early in start-up, after plugins load. To set your slots before the
// first script runs, detour addr::kCVirtualMachineCommands_InitializeCommands and call Set after
// the original returns.
Handler *Table();

// Returns the handler in a slot.
// @return the handler, or null if the slot is empty, the number is out of range, or the table
//  does not exist yet
Handler Get(int id);

// Puts a handler in a slot, either to replace a command or to add a new one. About 150 slots are
// empty, and a script can call a new command once script.ldf declares it with that number.
// @return the handler it replaced, or null if the slot was empty. Also returns null, and changes
//  nothing, if the number is out of range or the table does not exist yet
//
// The write is atomic, so the VM sees either the old handler or the new one, never half of one.
Handler Set(int id, Handler handler);

// Calls a handler the way ExecuteCommand does. A replacement handler uses it to call the one it
// replaced.
// @return what the handler returns
int CallHandler(Handler handler, int id, int paramCount);

// Destroys an engine structure by calling the engine, which frees it on the engine's heap. Does
// nothing if the structure is null or the application does not exist yet.
void DestroyStructure(EngineStructure type, void *structure);

// Copies an engine structure by calling the engine, the same copy the VM makes on every push and
// pop.
// @return the copy, which you must free with DestroyStructure; null if the engine made no copy or
//  the application does not exist yet
void *CopyStructure(EngineStructure type, const void *structure);

// An engine structure that your plugin owns, such as one popped off the stack. When the Owned is
// destroyed, it has the engine destroy the structure. Move-only.
template <class T, EngineStructure Type> class Owned
{
  public:
    Owned() = default;
    explicit Owned(T *structure) : structure_(structure)
    {
    }
    ~Owned()
    {
        if (structure_ != nullptr)
        {
            DestroyStructure(Type, structure_);
        }
    }
    Owned(Owned &&other) noexcept : structure_(std::exchange(other.structure_, nullptr))
    {
    }
    Owned &operator=(Owned &&other) noexcept
    {
        std::swap(structure_, other.structure_);
        return *this;
    }
    Owned(const Owned &) = delete;
    Owned &operator=(const Owned &) = delete;

    // Returns the structure, or null if there is none.
    T *get() const
    {
        return structure_;
    }
    T *operator->() const
    {
        return structure_;
    }
    // Returns whether there is a structure.
    explicit operator bool() const
    {
        return structure_ != nullptr;
    }

  private:
    T *structure_ = nullptr;
};
using OwnedEffect = Owned<CGameEffect, EngineStructure::Effect>;      // an effect your plugin owns
using OwnedCommand = Owned<::dao::Command, EngineStructure::Command>; // a command your plugin owns

using OwnedEvent = Owned<CScriptEvent, EngineStructure::Event>; // an event your plugin owns

// Makes an owned copy of an effect, so you can keep it after the call that passed it to you.
// @return the copy, or an empty Owned if the engine made no copy
inline OwnedEffect Copy(const CGameEffect &effect)
{
    return OwnedEffect{static_cast<CGameEffect *>(CopyStructure(EngineStructure::Effect, &effect))};
}
// Makes an owned copy of an event, so you can keep it after the call that passed it to you.
// @return the copy, or an empty Owned if the engine made no copy
inline OwnedEvent Copy(const CScriptEvent &event)
{
    return OwnedEvent{static_cast<CScriptEvent *>(CopyStructure(EngineStructure::Event, &event))};
}
// Makes an owned copy of a command, so you can keep it after the call that passed it to you. For
// example, an AddCommand intercept can hold a command back and queue it later.
// @return the copy, or an empty Owned if the engine made no copy
inline OwnedCommand Copy(const ::dao::Command &command)
{
    return OwnedCommand{static_cast<::dao::Command *>(CopyStructure(EngineStructure::Command, &command))};
}

// A script array that your plugin owns (dao/core/ScriptArray.h), such as one popped off the
// stack. When the Array is destroyed, it has the engine free the array. Move-only. A popped
// array shares its elements with the VM's copy. The element getters return an empty value (0,
// kInvalidObject, an empty string or null) if the index is out of range or the array holds
// another type.
//
// A default-constructed Array holds no array, and the stack refuses to push it. To pass an empty
// array, create one with Empty:
//
//   dao::script::api::LoadCutscene(L"bed110cs_tainted_mirror.cut", player, 1,
//       dao::script::Array::Empty(dao::core::ArrayTag::String),
//       dao::script::Array::Empty(dao::core::ArrayTag::Object), 0);
class Array
{
  public:
    Array() = default;
    // Takes ownership of an engine array.
    explicit Array(core::ArrayRep *array) : array_(array)
    {
    }
    // Creates an empty array of the given type, the same way the engine does.
    // @return the array, or no array if the engine could not allocate it
    static Array Empty(core::ArrayTag kind);
    ~Array();
    Array(Array &&other) noexcept : array_(std::exchange(other.array_, nullptr))
    {
    }
    Array &operator=(Array &&other) noexcept
    {
        std::swap(array_, other.array_);
        return *this;
    }
    Array(const Array &) = delete;
    Array &operator=(const Array &) = delete;

    // Returns whether there is an array.
    explicit operator bool() const
    {
        return array_ != nullptr;
    }
    // Returns the engine's array, or null if there is none.
    core::ArrayRep *get() const
    {
        return array_;
    }
    // Returns the array's tag, which says what type it holds (core::ArrayTag), or 0 if there is
    // no array.
    std::int32_t Tag() const
    {
        return array_ != nullptr ? array_->tag : 0;
    }
    // Returns the number of elements, or 0 if there is no array.
    std::size_t Size() const
    {
        return array_ != nullptr ? array_->Elements().size() : 0;
    }

    std::int32_t Int(std::size_t index) const;         // an int element, or 0
    float Float(std::size_t index) const;              // a float element, or 0.0
    ObjectId Object(std::size_t index) const;          // an object element, or kInvalidObject
    std::wstring_view String(std::size_t index) const; // a string element, or empty
    // Returns an element of an array of engine structures, or null if there is none.
    const void *Structure(std::size_t index) const;
    const CGameEffect *Effect(std::size_t index) const; // an effect element, or null
    const CScriptEvent *Event(std::size_t index) const; // an event element, or null

  private:
    std::uint32_t Element(core::ArrayTag kind, std::size_t index) const;

    core::ArrayRep *array_ = nullptr;
};

// Notes on Stack: the VM's pops and pushes were found by voting across the exe's commands
// against their script.ldf signatures (dao/Addresses.h). Strings are the engine's std::wstring
// on the stack; these copy to and from a plugin's own, through dao/String.h.

// The script VM's typed stack. In a handler, each pop takes the next argument, in the order the
// script passed them, and a push hands the result back to the script. Each returns false, or an
// empty Owned or Array, if the VM refused: the stack was empty, or the value on top was of
// another type.
//
// The VM copies any structure or array you push, so you keep your own. A popped one is your own
// copy, and the Owned or Array it comes in frees it.
struct Stack
{
    static bool PopInt(std::int32_t &out);
    static bool PopFloat(float &out);
    static bool PopObject(ObjectId &out);
    static bool PopString(std::wstring &out);
    static bool PopResource(Resource &out);
    static bool PopVector(Vector &out);
    static bool PopLocation(CScriptLocation &out);
    static OwnedEffect PopEffect();
    static OwnedEvent PopEvent();
    static OwnedCommand PopCommand();
    static Array PopArray();

    static bool PushInt(std::int32_t value);
    static bool PushFloat(float value);
    static bool PushObject(ObjectId value);
    static bool PushString(std::wstring_view value);
    static bool PushResource(const Resource &value);
    static bool PushVector(const Vector &value);
    static bool PushLocation(const CScriptLocation &value);
    static bool PushEffect(const CGameEffect &value);
    static bool PushEvent(const CScriptEvent &value);
    static bool PushCommand(const ::dao::Command &value);
    // Pushes an array. The VM makes its own copy, which shares the elements. Returns false if
    // there is no array.
    static bool PushArray(const Array &value);

    // Pops an engine structure of any type.
    // @return the popped copy, which you must free with DestroyStructure; null if the VM refused
    static void *PopStructure(EngineStructure type);
    // Pushes an engine structure of any type. The VM copies it.
    static bool PushStructure(EngineStructure type, const void *value);
};

// -- calling a script command from C++ ------------------------------------
//
// Notes on calling: it is done the way a script calls a command: its arguments pushed on the
// VM's stack last first, the command's handler run from the table, its answer popped.
// Everything a script can call, a plugin can then call too (NWNX does the same for Neverwinter
// Nights). The thread scripts run on is the game's logic thread, where hooks on the engine's
// update and on script commands are called.

// Returns the object a command sees as OBJECT_SELF, the object the running script belongs to.
// @return the object, or kInvalidObject if the application does not exist yet
ObjectId Self();

// Sets OBJECT_SELF for as long as it exists, and restores the previous one when destroyed.
class ScopedSelf
{
  public:
    // Sets OBJECT_SELF to `self`.
    explicit ScopedSelf(ObjectId self);
    ~ScopedSelf();
    ScopedSelf(const ScopedSelf &) = delete;
    ScopedSelf &operator=(const ScopedSelf &) = delete;

  private:
    std::int32_t hadSelf_ = 0;
    ObjectId previous_ = kInvalidObject;
};

// Returns the number of values on the VM's stack, or 0 if the VM does not exist yet.
std::int32_t StackDepth();
// Cuts the VM's stack back to a depth, destroying the values above it as a pop would. Does
// nothing if the depth is at or above the current one.
void TruncateStack(std::int32_t depth);

namespace detail
{
bool Push(std::int32_t value);
bool Push(float value);
bool Push(ObjectId value);
bool Push(std::wstring_view value);
bool Push(const Vector &value);
bool Push(const CScriptLocation &value);
bool Push(const CGameEffect &value);
bool Push(const CScriptEvent &value);
bool Push(const Array &value);
bool Push(const ::dao::Command &value);
bool Push(const Resource &value);
inline bool Push(const wchar_t *value)
{
    return Push(std::wstring_view{value});
}
inline bool Push(const std::wstring &value)
{
    return Push(std::wstring_view{value});
}

inline bool PushReversed()
{
    return true;
}
template <class First, class... Rest> bool PushReversed(const First &first, const Rest &...rest)
{
    return PushReversed(rest...) && Push(first);
}

template <class R> std::optional<R> Pop();

// Runs a command's handler the way ExecuteCommand does.
int Run(int command, int paramCount);
} // namespace detail

// Calls a script command by number, the same way a script does. dao/generated/ScriptApi.h wraps
// every command in a function with its real signature, so you rarely need to call this directly.
//
//   auto team = dao::script::Invoke<std::int32_t>(807, creature); // GetTeamId(creature)
//
// @param command the command's number
// @param args the arguments, in the order a script would pass them
// @return for a command that returns an R, an optional holding the result, empty if the call
//  failed; for R = void, true if the call succeeded
//
// Call it on the game's logic thread, where scripts run, because the VM is not thread-safe. It is
// fine to call while a script is running. If the call fails, the stack is cut back to the depth
// it had before the call.
template <class R = void, class... Args> auto Invoke(int command, const Args &...args)
{
    const std::int32_t depth = StackDepth();
    const bool pushed = detail::PushReversed(args...);
    const int result = pushed ? detail::Run(command, static_cast<int>(sizeof...(Args))) : kStackOverflow;
    if constexpr (std::is_void_v<R>)
    {
        if (result != kOk)
        {
            TruncateStack(depth);
        }
        return result == kOk;
    }
    else
    {
        std::optional<R> answer;
        if (result == kOk)
        {
            answer = detail::Pop<R>();
        }
        if (!answer)
        {
            TruncateStack(depth);
        }
        return answer;
    }
}

} // namespace dao::script
