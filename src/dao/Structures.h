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
#include "dao/generated/ScriptConstants.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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

// A script's `resource`: a file the game loads by name, as a script writes
// R"gen00fl_dog.utc". On the VM's stack it is a string under a tag of its
// own (0x60), so it has a type of its own here, to keep it apart from a
// string argument.
struct Resource
{
    std::wstring name;
};

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
// descriptors: CDataArrays at +0 of each), and what a command's parameters
// are (dao/Objects.h, Command): a vtable, then pointers to the lists of
// parameters a script reads by index with GetEffectInteger,
// GetEventInteger, GetCommandInt and the like, which call the same
// functions for all three. 0x20 bytes: ints at +0x4, floats at +0x8, flags
// at +0xC, objects at +0x10, strings at +0x14, vectors at +0x18. Out of
// range, each gives what the engine gives: 0, 0.0, 0, -1, an empty string
// and, for a vector, nothing.
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
    // What the command constructors store their scripts' booleans in
    // (bRunToLocation, bPlayNext and the like), 4 bytes each.
    std::int32_t Flag(std::int32_t index) const
    {
        using Fn = std::int32_t(__fastcall *)(const CDataArrays *, int, std::int32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetFlag))(this, 0, index);
    }
    // Read from the list directly, as the engine's accessor does: no
    // getter was found, and the list is plain data.
    std::optional<::dao::Vector> Vector(std::int32_t index) const
    {
        const auto *list = Read<const VectorList *>(0x18);
        if (list == nullptr || list->data == nullptr || index < 0 || std::cmp_greater_equal(index, list->size))
        {
            return std::nullopt;
        }
        return *reinterpret_cast<const ::dao::Vector *>(list->data + (static_cast<std::size_t>(index) * 16));
    }

    // The engine's own setters: each makes its list if there is none and
    // grows it to the index. What a changed parameter does depends on who
    // reads it: a command not yet started takes it as if its script had
    // passed it.
    void SetInteger(std::int32_t index, std::int32_t value)
    {
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetInteger))(this, 0, index, value);
    }
    void SetFloat(std::int32_t index, float value)
    {
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, float);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetFloat))(this, 0, index, value);
    }
    void SetFlag(std::int32_t index, std::int32_t value)
    {
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetFlag))(this, 0, index, value);
    }
    void SetObject(std::int32_t index, ObjectId value)
    {
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, ObjectId);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetObject))(this, 0, index, value);
    }
    void SetVector(std::int32_t index, const ::dao::Vector &value)
    {
        // The setter copies 16 bytes with movaps, so the source is aligned.
        alignas(16) const float padded[4] = {value.x, value.y, value.z, 0.0F};
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, const float *);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetVector))(this, 0, index, padded);
    }

  protected:
    struct VectorList
    {
        const std::byte *data;
        std::uint32_t size;
        std::uint32_t capacity;
    };

    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
    template <class T> void Write(std::size_t offset, const T &value)
    {
        *reinterpret_cast<T *>(reinterpret_cast<std::byte *>(this) + offset) = value;
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
    // EFFECT_TYPE_*, the scripts' own (effect_constants_h.nss).
    script::EffectType Type() const
    {
        return Read<script::EffectType>(0x24);
    }
    // EFFECT_DURATION_TYPE_* in script.ldf, which names 0 to 3 and 6. 4 is
    // what an equipped item's effects carry (seen in play by the Eluvian
    // MCP server, 2026-09-30); 5 is unnamed too. RemoveEffectsByParameters
    // leaves 4, 5 and 6 alone unless asked (0x007A0C90).
    script::EffectDurationType DurationType() const
    {
        return Read<script::EffectDurationType>(0x28);
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

    // A temporary effect's time (EFFECT_DURATION_TYPE_TEMPORARY), read
    // 2026-09-30: how long it lasts and how long it has lasted, in
    // milliseconds. Engine_ApplyEffectOnObject stores the duration it is
    // given, times 1000, at +0x2C (0x0065C760); AddElapsed (0x0079F2A0)
    // adds each update's time to +0x30, never past the duration; the
    // effect ends when nothing is left (dao/Effects.h). Zero for effects
    // of other duration types. Not yet seen in play.
    std::int32_t DurationMs() const
    {
        return Read<std::int32_t>(kDuration);
    }
    std::int32_t ElapsedMs() const
    {
        return Read<std::int32_t>(kElapsed);
    }
    float Duration() const
    {
        return static_cast<float>(DurationMs()) / 1000;
    }
    float Remaining() const
    {
        return static_cast<float>(DurationMs() - ElapsedMs()) / 1000;
    }
    // Sets the time it has left, within its duration; zero or less ends it
    // on its owner's next update.
    void SetRemaining(float seconds)
    {
        const auto left = static_cast<std::int32_t>(seconds * 1000);
        const std::int32_t duration = DurationMs();
        std::int32_t elapsed = duration - left;
        if (left <= 0)
        {
            elapsed = duration;
        }
        else if (left >= duration)
        {
            elapsed = 0;
        }
        Write(kElapsed, elapsed);
    }

    static constexpr std::size_t kDuration = 0x2C;
    static constexpr std::size_t kElapsed = 0x30;
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

    // EVENT_TYPE_* in script.ldf; scripts also make their own numbers.
    script::EventType Type() const
    {
        return Read<script::EventType>(0x20);
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
