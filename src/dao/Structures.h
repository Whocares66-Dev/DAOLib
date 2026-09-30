// The script engine's value types beyond int, float and object: the
// vector, and the engine structures script.ldf declares (ENGINE_STRUCTURE_0
// to 5: event, location, command, effect, itemproperty, player), as a
// plugin reads them. dao/Script.h moves them on and off the VM's stack.
//
// Read 2026-09-30 from the commands object's structure functions
// (CVirtualMachineCommands' slots 5, 6 and 8: create, destroy and copy,
// dao/Addresses.h) and the script commands that take each apart. NWN names
// two of the classes in DAO's RTTI (CGameEffect, CScriptEvent) and the
// location has NWN's CScriptLocation's shape.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/core/Msvc8.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace dao
{

// A script's vector: three floats, as the VM pushes and pops them
// (StackPushVector, 0x00A2D6D0: x, y, z, one float each).
struct Vector
{
    float x = 0;
    float y = 0;
    float z = 0;
};

// The script's `location`: plain data, 0x30 bytes. Its copy constructor
// (0x0064DD90) copies exactly the fields below, and its constructor
// (0x007ABD00) zeroes the six floats and sets the area to -1; the rest is
// the padding that keeps each vector on 16 bytes. GetPositionFromLocation
// reads the position, GetFacingFromLocation turns the orientation into the
// facing, and GetAreaFromLocation returns the area.
struct CScriptLocation
{
    Vector position;
    std::uint32_t padding0 = 0;
    Vector orientation;
    std::uint32_t padding1 = 0;
    ObjectId area = 0xFFFFFFFF;
    std::uint32_t padding2[3] = {};
};
static_assert(sizeof(CScriptLocation) == 0x30, "the engine allocates 0x30 bytes for a location");
static_assert(offsetof(CScriptLocation, orientation) == 0x10 && offsetof(CScriptLocation, area) == 0x20);

// The engine structures, by the number script.ldf gives each.
enum class EngineStructure : std::int32_t
{
    Event = 0,
    Location = 1,
    Command = 2,
    Effect = 3,
    ItemProperty = 4, // also a CGameEffect: the copy makes one of 0x7C bytes for both
    Player = 5,
};

// What CGameEffect and CScriptEvent both start with (the RTTI's base class
// descriptors: CDataArrays at +0 of each): a vtable, then pointers to the
// std::vectors of parameters a script reads by index with
// GetEffectInteger, GetEventInteger and the like, which call the same four
// functions for both. Ints at +0x4, floats at +0x8, objects at +0x10,
// strings at +0x14; +0xC is not read yet. Out of range, each gives what the
// engine gives: 0, 0.0, -1 and an empty string.
class CDataArrays
{
  public:
    CDataArrays() = delete;
    CDataArrays(const CDataArrays &) = delete;
    CDataArrays &operator=(const CDataArrays &) = delete;
    ~CDataArrays() = delete;

    std::int32_t Integer(std::int32_t index) const
    {
        using Fn = std::int32_t(__fastcall *)(const CDataArrays *, int, std::int32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetInteger))(this, 0, index);
    }
    float Float(std::int32_t index) const
    {
        using Fn = float(__fastcall *)(const CDataArrays *, int, std::int32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetFloat))(this, 0, index);
    }
    ObjectId Object(std::int32_t index) const
    {
        using Fn = ObjectId(__fastcall *)(const CDataArrays *, int, std::int32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetObject))(this, 0, index);
    }
    // The engine's string, valid while the structure is.
    std::wstring_view String(std::int32_t index) const
    {
        using Fn = const core::WStringRep *(__fastcall *)(const CDataArrays *, int, std::int32_t);
        const auto *rep = reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetString))(this, 0, index);
        return rep != nullptr ? rep->View() : std::wstring_view{};
    }

  protected:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

// An effect, and an item property. Polymorphic and 0x7C bytes. The fields
// are the ones GetEffectType and its six siblings read, one shared command
// body (0x0065C270) switching on the command's number. Checked in play
// 2026-09-30: type, duration type, creator and ability id matched the
// engine's getters on every call of a fight.
class CGameEffect : public CDataArrays
{
  public:
    CGameEffect() = delete;
    CGameEffect(const CGameEffect &) = delete;
    CGameEffect &operator=(const CGameEffect &) = delete;
    ~CGameEffect() = delete;

    std::uint32_t Id() const
    {
        return Read<std::uint32_t>(0x20);
    }
    // EFFECT_TYPE_* in script.ldf.
    std::int32_t Type() const
    {
        return Read<std::int32_t>(0x24);
    }
    // EFFECT_DURATION_TYPE_*.
    std::int32_t DurationType() const
    {
        return Read<std::int32_t>(0x28);
    }
    std::int32_t Animation() const
    {
        return Read<std::int32_t>(0x34);
    }
    std::uint32_t Flags() const
    {
        return Read<std::uint32_t>(0x3C);
    }
    ObjectId Creator() const
    {
        return Read<ObjectId>(0x40);
    }
    std::int32_t AbilityId() const
    {
        return Read<std::int32_t>(0x44);
    }
};

// An event: what an event script gets from GetCurrentEvent, and what
// SignalEvent sends. Polymorphic and 0x50 bytes. Its parameters are the
// CDataArrays' (GetEventInteger calls the same getter as GetEffectInteger);
// its own fields are the ones GetEventType (0x00658390), GetEventCreator
// (0x006582A0) and GetEventTarget (0x00659030) read.
class CScriptEvent : public CDataArrays
{
  public:
    CScriptEvent() = delete;
    CScriptEvent(const CScriptEvent &) = delete;
    CScriptEvent &operator=(const CScriptEvent &) = delete;
    ~CScriptEvent() = delete;

    // EVENT_TYPE_* in script.ldf.
    std::int32_t Type() const
    {
        return Read<std::int32_t>(0x20);
    }
    ObjectId Creator() const
    {
        return Read<ObjectId>(0x24);
    }
    ObjectId Target() const
    {
        return Read<ObjectId>(0x28);
    }
};

} // namespace dao
