// Script commands, from a plugin: the engine's table of them, a handler of
// our own in a slot of it, and the script VM's typed stack a handler reads
// its arguments from and pushes its answer to.
//
// A script (.nss, compiled to .ncs) calls the engine by number. The script
// commands object (CClientExoApp+0x1E4) holds a table of 863 handlers, and
// the VM's ExecuteCommand calls `table[number](number, paramCount)` with that
// object in ecx; a handler pops its arguments, does its work, pushes its
// answer and returns 0, or a negative error (dao/Addresses.h has how each
// piece was read). So a plugin can:
//
//   - replace a command's handler, keeping the old one to call on (the
//     probe does this to check fields against the engine's answers), and
//   - put a new command in a free slot: about a hundred and fifty are
//     empty. A script reaches it once the toolset's script.ldf declares it
//     with that number (dev/SCRIPT_COMMANDS.md).
//
// The table exists once InitializeCommands has filled it, early in the
// application's start-up and after plugins load: Table() is null before the
// application exists. In the moment between the application storing the
// commands object and the VM initialising it, the table pointer is not yet
// written, so a plugin that wants its slots set before the first script
// runs detours addr::kCVirtualMachineCommands_InitializeCommands and sets
// them after the original returns, as the probe does.
//
//   int __fastcall MyCommand(void *commands, int edx, int id, int paramCount)
//   {
//       dao::ObjectId creature = 0;
//       if (!dao::script::Stack::PopObject(creature))
//           return dao::script::kStackUnderflow;
//       ...
//       dao::script::Stack::PushInt(answer);
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
class Command; // dao/Objects.h: a command queued on an object, made by CommandAttack and its kin
} // namespace dao

namespace dao::script
{

// The table's size: ExecuteCommand refuses a number of 0x35F or more.
inline constexpr int kSlots = 0x35F;

// What a handler returns: 0, or one of the VM's errors, which are NWN's
// numbers (NWN:EE's Constants/VirtualMachine.hpp; negative string refs).
// The exe's getters return -639 when their pop fails and -638 when their
// push does (GetTeamId, 0x0065BCD0); ExecuteCommand returns -642 for a
// number with no handler.
inline constexpr int kOk = 0;
inline constexpr int kStackOverflow = -638;
inline constexpr int kStackUnderflow = -639;
inline constexpr int kInvalidCommand = -642;

// A handler, as ExecuteCommand calls it: __thiscall on the commands object,
// the command's number and its parameter count, `ret 8`. Written
// __fastcall so ecx arrives as the engine sends it; edx is unused.
using Handler = int(__fastcall *)(void *commands, int edx, int id, int paramCount);

// The table, or null before the engine has made it.
Handler *Table();

// The handler in a slot, or null for an empty slot or a number out of range.
Handler Get(int id);

// Put a handler in a slot and return the one it replaces (null for a slot
// that was empty). Nothing happens, and null comes back, for a number out
// of range or before the table exists. The write is one aligned pointer, so
// the VM sees either handler, never half of one.
Handler Set(int id, Handler handler);

// Call a handler as ExecuteCommand would: for a replacement calling on the
// handler it replaced.
int Call(Handler handler, int id, int paramCount);

// Destroy an engine structure through the engine (the commands object's
// DestroyGameDefinedStructure), which frees it on the engine's heap.
void DestroyStructure(EngineStructure type, void *structure);

// A copy of an engine structure, made by the engine as the VM makes one on
// every push and pop (the commands object's CopyGameDefinedStructure), the
// caller's to destroy (DestroyStructure). Null if the engine made none.
void *CopyStructure(EngineStructure type, const void *structure);

// An engine structure popped off the stack: the VM's copy, which is the
// popper's to destroy; this does it. Move-only.
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

    T *get() const
    {
        return structure_;
    }
    T *operator->() const
    {
        return structure_;
    }
    explicit operator bool() const
    {
        return structure_ != nullptr;
    }

  private:
    T *structure_ = nullptr;
};
using OwnedEffect = Owned<CGameEffect, EngineStructure::Effect>;
using OwnedCommand = Owned<::dao::Command, EngineStructure::Command>;

using OwnedEvent = Owned<CScriptEvent, EngineStructure::Event>;

// An owned copy of one, to keep past the call that lent it: a command an
// AddCommand intercept holds back and queues later, say.
inline OwnedEffect Copy(const CGameEffect &effect)
{
    return OwnedEffect{static_cast<CGameEffect *>(CopyStructure(EngineStructure::Effect, &effect))};
}
inline OwnedEvent Copy(const CScriptEvent &event)
{
    return OwnedEvent{static_cast<CScriptEvent *>(CopyStructure(EngineStructure::Event, &event))};
}
inline OwnedCommand Copy(const ::dao::Command &command)
{
    return OwnedCommand{static_cast<::dao::Command *>(CopyStructure(EngineStructure::Command, &command))};
}

// A script array popped off the stack (dao/core/ScriptArray.h): the
// popper's copy, sharing the elements with the VM's, freed through the
// engine. Move-only. The element getters read what the tag says the array
// holds; an index out of range, or the wrong kind, gives the empty value.
//
// A default Array is no array: the stack refuses to push one, so a command
// that takes an array is not called with it. For a command that wants an
// array and may have an empty one (LoadCutscene's two, of the tags to
// replace and the objects to put in their place), make one:
//
//   dao::script::api::LoadCutscene(L"bed110cs_tainted_mirror.cut", player, 1,
//       dao::script::Array::Empty(dao::core::ArrayTag::String),
//       dao::script::Array::Empty(dao::core::ArrayTag::Object), 0);
class Array
{
  public:
    Array() = default;
    explicit Array(core::ArrayRep *array) : array_(array)
    {
    }
    // An array of that kind with nothing in it, made as the engine makes
    // one (its constructor by tag, 0x00A2E920). Seen in play, 2026-10-01:
    // LoadCutscene took an empty array of strings and one of objects.
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

    explicit operator bool() const
    {
        return array_ != nullptr;
    }
    core::ArrayRep *get() const
    {
        return array_;
    }
    std::int32_t Tag() const
    {
        return array_ != nullptr ? array_->tag : 0;
    }
    std::size_t Size() const
    {
        return array_ != nullptr ? array_->Elements().size() : 0;
    }

    std::int32_t Int(std::size_t index) const;
    float Float(std::size_t index) const;
    ObjectId Object(std::size_t index) const; // kInvalidObject out of range
    std::wstring_view String(std::size_t index) const;
    // An element of an array of engine structures, or null.
    const void *Structure(std::size_t index) const;
    const CGameEffect *Effect(std::size_t index) const;
    const CScriptEvent *Event(std::size_t index) const;

  private:
    std::uint32_t Element(core::ArrayTag kind, std::size_t index) const;

    core::ArrayRep *array_ = nullptr;
};

// The script VM's typed stack (dao/Addresses.h, voted across the exe's
// commands against their script.ldf signatures). Each pop takes the next
// argument off, in the order the script passed them; each push is the
// answer. False (or null) if the VM refused: an empty stack, or a value of
// another type on top.
//
// Strings are the engine's std::wstring on the stack; these copy to and
// from a plugin's own, through dao/String.h. A structure or an array
// pushed is copied by the VM, so the caller keeps its own; one popped is
// the popper's copy, freed by the Owned or Array it comes in.
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
    // The VM makes a copy of its own, sharing the elements.
    static bool PushArray(const Array &value);

    // Any engine structure: popped, the VM's copy, null if refused, the
    // caller's to destroy (DestroyStructure); pushed, copied.
    static void *PopStructure(EngineStructure type);
    static bool PushStructure(EngineStructure type, const void *value);
};

// -- calling a script command from C++ ------------------------------------
//
// The way a script calls one: its arguments pushed on the VM's stack last
// first, the command's handler run from the table, its answer popped.
// Everything a script can call, a plugin can then call too (NWNX does the
// same for Neverwinter Nights); dao/generated/ScriptApi.h wraps each
// command with its script.ldf signature, so this is rarely used directly.
//
// Run it on the thread scripts run on (the game's logic thread, where
// hooks on the engine's update and on script commands are called): the VM
// is not made for two threads. Calling while a script is running is fine,
// the stack is left as it was found. A failed call cuts the stack back to
// its depth before the call.

// The object a command sees as OBJECT_SELF: the commands object keeps the
// object the running script belongs to at +8, with +4 saying it is set
// (NWN's CVirtualMachineCmdImplementer has the same two; GetCurrentEvent
// and SignalEvent read +8).
ObjectId Self();

// Sets OBJECT_SELF for as long as it lives, then puts it back.
class ScopedSelf
{
  public:
    explicit ScopedSelf(ObjectId self);
    ~ScopedSelf();
    ScopedSelf(const ScopedSelf &) = delete;
    ScopedSelf &operator=(const ScopedSelf &) = delete;

  private:
    std::int32_t hadSelf_ = 0;
    ObjectId previous_ = kInvalidObject;
};

// The number of values on the VM's stack, and cutting it back to a depth
// (the values above it destroyed as a pop destroys them).
std::int32_t StackDepth();
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

// The command's handler, run as ExecuteCommand runs it.
int Run(int command, int paramCount);
} // namespace detail

// Calls a script command: `Invoke<std::int32_t>(807, creature)` is
// GetTeamId(creature). R is the answer's type (void for none); the answer
// comes back in an optional, empty if the command failed, or a bool for a
// command with none.
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
