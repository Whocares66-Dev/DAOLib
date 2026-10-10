// The scripts' value types other than int, float and object: vector, resource, and the engine
// structures that script.ldf declares (event, location, command, effect, itemproperty and
// player). Use them to read and change the values scripts pass around. dao/Script.h pushes them
// on and pops them off the VM's stack.

// Notes on this file: read 2026-09-30 from the commands object's structure functions
// (CVirtualMachineCommands' slots 5, 6 and 8: create, destroy and copy, dao/Addresses.h) and the
// script commands that take each structure apart. script.ldf declares them as ENGINE_STRUCTURE_0
// to 5. NWN names two of the classes in DAO's RTTI (CGameEffect, CScriptEvent), and the location
// has the shape of NWN's CScriptLocation.
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

// Notes on Vector: the VM pushes and pops it as x, y, z, one float each (StackPushVector,
// 0x00A2D6D0).

// A script's `vector`: three floats.
struct Vector
{
    float x = 0;
    float y = 0;
    float z = 0;
};

// Notes on CScriptLocation: plain data, 0x30 bytes. Its copy constructor (0x0064DD90) copies
// exactly the fields below, and its constructor (0x007ABD00) zeroes the six floats and sets the
// area to -1; the rest is the padding that keeps each vector on 16 bytes.
// GetPositionFromLocation reads the position, GetFacingFromLocation turns the orientation into
// the facing, and GetAreaFromLocation returns the area.

// A script's `location`: a position and an orientation in an area.
struct CScriptLocation
{
    Vector position;
    std::uint32_t padding0 = 0;
    Vector orientation; // GetFacingFromLocation computes the facing from this
    std::uint32_t padding1 = 0;
    ObjectId area = 0xFFFFFFFF; // the area, or kInvalidObject for none
    std::uint32_t padding2[3] = {};
};
static_assert(sizeof(CScriptLocation) == 0x30, "the engine allocates 0x30 bytes for a location");
static_assert(offsetof(CScriptLocation, orientation) == 0x10 && offsetof(CScriptLocation, area) == 0x20);

// Notes on Resource: on the VM's stack a resource is a string under a tag of its own (0x60),
// so it has a type of its own here, to keep it apart from a string argument.

// A script's `resource`: the name of a file the game loads, which a script writes as
// R"gen00fl_dog.utc".
struct Resource
{
    std::wstring name; // the file's name, with its extension
};

// The engine structures, with the same numbers script.ldf gives them.
enum class EngineStructure : std::int32_t
{
    Event = 0,
    Location = 1,
    Command = 2,
    Effect = 3,
    ItemProperty = 4, // also a CGameEffect: copying either makes a 0x7C-byte object
    Player = 5,
};

// Notes on CDataArrays: the RTTI's base class descriptors put CDataArrays at +0 of both
// CGameEffect and CScriptEvent, and a command's parameters are one too (dao/Objects.h,
// Command). It is 0x20 bytes: a vtable, then pointers to the lists, ints at +0x4, floats at
// +0x8, flags at +0xC, objects at +0x10, strings at +0x14, vectors at +0x18. GetEffectInteger,
// GetEventInteger, GetCommandInt and the like call the same functions for all three. Out of
// range, each getter gives what the engine gives: 0, 0.0, 0, -1, an empty string and, for a
// vector, nothing.

// The parameter lists that an effect, an event or a command carries, read and written by index.
// Scripts read them with GetEffectInteger, GetEventInteger, GetCommandInt and similar commands.
class CDataArrays
{
  public:
    CDataArrays() = delete;
    CDataArrays(const CDataArrays &) = delete;
    CDataArrays &operator=(const CDataArrays &) = delete;
    ~CDataArrays() = delete;

    // Returns the integer parameter at an index, or 0 if there is none.
    std::int32_t Integer(std::int32_t index) const
    {
        using Fn = std::int32_t(__fastcall *)(const CDataArrays *, int, std::int32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetInteger))(this, 0, index);
    }
    // Returns the float parameter at an index, or 0.0 if there is none.
    float Float(std::int32_t index) const
    {
        using Fn = float(__fastcall *)(const CDataArrays *, int, std::int32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetFloat))(this, 0, index);
    }
    // Returns the object parameter at an index, or kInvalidObject if there is none.
    ObjectId Object(std::int32_t index) const
    {
        using Fn = ObjectId(__fastcall *)(const CDataArrays *, int, std::int32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetObject))(this, 0, index);
    }
    // Returns the string parameter at an index, or an empty string if there is none.
    // The view points into the engine's own string, and is valid only as long as the structure.
    std::wstring_view String(std::int32_t index) const
    {
        using Fn = const core::WStringRep *(__fastcall *)(const CDataArrays *, int, std::int32_t);
        const auto *rep = reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetString))(this, 0, index);
        return rep != nullptr ? rep->View() : std::wstring_view{};
    }
    // Returns the flag parameter at an index, or 0 if there is none. The command constructors
    // store a script's booleans, such as bRunToLocation and bPlayNext, as flags.
    std::int32_t Flag(std::int32_t index) const
    {
        using Fn = std::int32_t(__fastcall *)(const CDataArrays *, int, std::int32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_GetFlag))(this, 0, index);
    }

    // Notes on Vector: read from the list directly, as the engine's accessor does: no getter was
    // found, and the list is plain data, 16 bytes an entry.

    // Returns the vector parameter at an index.
    // @return the vector, or nullopt if there is none
    std::optional<::dao::Vector> Vector(std::int32_t index) const
    {
        const auto *list = Read<const VectorList *>(0x18);
        if (list == nullptr || list->data == nullptr || index < 0 || std::cmp_greater_equal(index, list->size))
        {
            return std::nullopt;
        }
        return *reinterpret_cast<const ::dao::Vector *>(list->data + (static_cast<std::size_t>(index) * 16));
    }

    // Notes on the setters: they are the engine's own, and each makes its list if there is none
    // and grows it to the index.

    // Sets the integer parameter at an index. A command that has not started yet uses the new
    // value as if its script had passed it. Elsewhere, the effect of a change depends on what
    // reads the value. The same applies to the other setters.
    void SetInteger(std::int32_t index, std::int32_t value)
    {
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetInteger))(this, 0, index, value);
    }
    // Sets the float parameter at an index.
    void SetFloat(std::int32_t index, float value)
    {
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, float);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetFloat))(this, 0, index, value);
    }
    // Sets the flag parameter at an index.
    void SetFlag(std::int32_t index, std::int32_t value)
    {
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetFlag))(this, 0, index, value);
    }
    // Sets the object parameter at an index.
    void SetObject(std::int32_t index, ObjectId value)
    {
        using Fn = void(__fastcall *)(CDataArrays *, int, std::int32_t, ObjectId);
        reinterpret_cast<Fn>(Module::Address(addr::kCDataArrays_SetObject))(this, 0, index, value);
    }
    // Sets the vector parameter at an index.
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

// Notes on CGameEffect: polymorphic and 0x7C bytes. The fields are the ones GetEffectType and
// its six siblings read, one shared command body (0x0065C270) switching on the command's number.
// Checked in play 2026-09-30: type, duration type, creator and ability id matched the engine's
// getters on every call of a fight.

// A script's `effect` or `itemproperty`, with its parameters (CDataArrays).
class CGameEffect : public CDataArrays
{
  public:
    CGameEffect() = delete;
    CGameEffect(const CGameEffect &) = delete;
    CGameEffect &operator=(const CGameEffect &) = delete;
    ~CGameEffect() = delete;

    // Returns the effect's id.
    std::uint32_t Id() const
    {
        return Read<std::uint32_t>(0x20);
    }
    // Returns the effect's type, one of the scripts' EFFECT_TYPE_* constants
    // (effect_constants_h.nss).
    script::EffectType Type() const
    {
        return Read<script::EffectType>(0x24);
    }

    // Notes on DurationType: script.ldf names 0 to 3 and 6. 4 is what an equipped item's effects
    // carry (seen in play by the Eluvian MCP server, 2026-09-30); 5 is unnamed too.
    // RemoveEffectsByParameters leaves 4, 5 and 6 alone unless asked (0x007A0C90).

    // Returns the effect's duration type, one of EFFECT_DURATION_TYPE_* in script.ldf. Effects
    // from an equipped item have type 4, which script.ldf does not name.
    script::EffectDurationType DurationType() const
    {
        return Read<script::EffectDurationType>(0x28);
    }
    // Returns the effect's animation, the same value GetEffectAnimation returns.
    std::int32_t Animation() const
    {
        return Read<std::int32_t>(0x34);
    }
    // Returns the effect's flags, the same value GetEffectFlags returns.
    std::uint32_t Flags() const
    {
        return Read<std::uint32_t>(0x3C);
    }
    // Returns the object that created the effect.
    ObjectId Creator() const
    {
        return Read<ObjectId>(0x40);
    }
    // Returns the id of the ability that made the effect, the same value GetEffectAbilityID
    // returns.
    std::int32_t AbilityId() const
    {
        return Read<std::int32_t>(0x44);
    }

    // Notes on a temporary effect's time, read 2026-09-30: Engine_ApplyEffectOnObject stores the
    // duration it is given, times 1000, at +0x2C (0x0065C760); AddElapsed (0x0079F2A0) adds each
    // update's time to +0x30, never past the duration; the effect ends when nothing is left
    // (dao/Effects.h). Not yet seen in play.

    // Returns how long a temporary effect lasts, in milliseconds, or 0 for other duration types.
    std::int32_t DurationMs() const
    {
        return Read<std::int32_t>(kDuration);
    }
    // Returns how long a temporary effect has lasted so far, in milliseconds, or 0 for other
    // duration types.
    std::int32_t ElapsedMs() const
    {
        return Read<std::int32_t>(kElapsed);
    }
    // Returns how long a temporary effect lasts, in seconds. Not yet tested in the game.
    float Duration() const
    {
        return static_cast<float>(DurationMs()) / 1000;
    }
    // Returns how long a temporary effect has left, in seconds. Not yet tested in the game.
    float Remaining() const
    {
        return static_cast<float>(DurationMs() - ElapsedMs()) / 1000;
    }
    // Sets how long a temporary effect has left, clamped to its duration. Not yet tested in the
    // game.
    // @param seconds the time left. Zero or less ends the effect at its owner's next update
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

    static constexpr std::size_t kDuration = 0x2C; // offset of the duration, in milliseconds
    static constexpr std::size_t kElapsed = 0x30;  // offset of the elapsed time, in milliseconds
};

// Notes on CScriptEvent: polymorphic and 0x50 bytes. Its parameters are the CDataArrays'
// (GetEventInteger calls the same getter as GetEffectInteger); its own fields are the ones
// GetEventType (0x00658390), GetEventCreator (0x006582A0) and GetEventTarget (0x00659030) read.

// A script's `event`, with its parameters (CDataArrays). An event script gets one from
// GetCurrentEvent, and SignalEvent sends one.
class CScriptEvent : public CDataArrays
{
  public:
    CScriptEvent() = delete;
    CScriptEvent(const CScriptEvent &) = delete;
    CScriptEvent &operator=(const CScriptEvent &) = delete;
    ~CScriptEvent() = delete;

    // Returns the event's type: one of EVENT_TYPE_* in script.ldf, or a custom number a script
    // chose.
    script::EventType Type() const
    {
        return Read<script::EventType>(0x20);
    }
    // Returns the object that sent the event.
    ObjectId Creator() const
    {
        return Read<ObjectId>(0x24);
    }
    // Returns the object the event is sent to.
    ObjectId Target() const
    {
        return Read<ObjectId>(0x28);
    }
};

} // namespace dao
