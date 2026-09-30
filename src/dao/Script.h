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
#include <string>
#include <string_view>
#include <utility>

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
using OwnedEvent = Owned<CScriptEvent, EngineStructure::Event>;

// A script array popped off the stack (dao/core/ScriptArray.h): the
// popper's copy, sharing the elements with the VM's, freed through the
// engine. Move-only. The element getters read what the tag says the array
// holds; an index out of range, or the wrong kind, gives the empty value.
class Array
{
  public:
    Array() = default;
    explicit Array(core::ArrayRep *array) : array_(array)
    {
    }
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
    static bool PopVector(Vector &out);
    static bool PopLocation(CScriptLocation &out);
    static OwnedEffect PopEffect();
    static OwnedEvent PopEvent();
    static Array PopArray();

    static bool PushInt(std::int32_t value);
    static bool PushFloat(float value);
    static bool PushObject(ObjectId value);
    static bool PushString(std::wstring_view value);
    static bool PushVector(const Vector &value);
    static bool PushLocation(const CScriptLocation &value);
    static bool PushEffect(const CGameEffect &value);
    static bool PushEvent(const CScriptEvent &value);
    // The VM makes a copy of its own, sharing the elements.
    static bool PushArray(const Array &value);

    // Any engine structure: popped, the VM's copy, null if refused, the
    // caller's to destroy (DestroyStructure); pushed, copied.
    static void *PopStructure(EngineStructure type);
    static bool PushStructure(EngineStructure type, const void *value);
};

} // namespace dao::script
