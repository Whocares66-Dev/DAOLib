// The engine's game objects, as a plugin uses them: opaque classes, never
// made or copied by us, reached through the application's lookups by id
// (dao/ClientExoApp.h), with accessors for the fields read so far and
// wrappers for the engine functions whose signatures are known.
//
// The hierarchy is the RTTI's: CCCreature, CCPlaceable and CCWayPoint are
// CCObjects, and CCObject is a CGameObject, each at the start of the class
// derived from it, so these classes add no adjustment. The engine's casts
// between them are CGameObject's virtual As* slots (tools/object_casts.py,
// dev/object_casts.csv), which the lookups by id call.
//
// Every field here is one a script command returns (the generated
// descriptors in dao/generated/ScriptFields.h) and is checked against that
// command's own answer by the probe in play (dev/PROBE.md, "Script checks").
// In the first session, 2026-09-30, 14 of the 18 matched on every call:
// all but BodyBag, AILevel, the placeable's State and MapPinState, which no
// script called (dev/LIBRARY.md).
// The functions are called exactly as the engine calls them; how each was
// read is in dao/Addresses.h.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/Structures.h"
#include "dao/core/CommandWord.h"
#include "dao/core/Msvc8.h"
#include "dao/core/ObjectType.h"
#include "dao/generated/ObjectCasts.h"
#include "dao/generated/ScriptFields.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// C4624, "destructor was implicitly defined as deleted", is what Opaque is
// for: every class below is one the engine makes and frees, never us.
#pragma warning(push)
#pragma warning(disable : 4624)

namespace dao
{

// The base of every engine type here: never made, copied or destroyed by
// us, only pointed at. Its deleted members delete the derived classes'.
struct Opaque
{
    Opaque() = delete;
    Opaque(const Opaque &) = delete;
    Opaque &operator=(const Opaque &) = delete;
    ~Opaque() = delete;
};

// A command queued on an object (CCObject::QueueCommand): the scripts'
// `command`, made by CommandAttack, CommandWait and the rest. 0x14 bytes
// from the AI's pool (dao/Addresses.h, CommandPool), read 2026-09-30 from
// its constructor (0x00741F10), the 17 script commands that build one, and
// AddCommand, GetCommandType and GetCommandIsPlayerIssued:
//
//   +0x0   the word: sequence (bits 0-15), type (+2, the byte SetType
//          writes and GetCommandType reads); +3 not read
//   +0x4   the parameters, a CDataArrays of its own (dao/Structures.h)
//   +0x8   1 from the constructor; not read yet
//   +0xC   static: AddCommand stores its bStatic here before queueing
//   +0x10  player issued: what GetCommandIsPlayerIssued answers
//
// What each constructor puts in the parameters, by type (int, float, flag,
// object, string, vector; the index after each):
//
//   Attack             object 0 target, int 2 forced result (COMBAT_RESULT_*)
//   JumpToObject       object 0 target
//   JumpToLocation     vector 0 position, vector 1 orientation, object 0 area
//   Wait               int 0 milliseconds (the seconds times 1000, at least 0)
//   PlayAnimation      int 0 animation, int 2 loops, int 7 0 to blend in else 2,
//                      flag 4 play next
//   StartConversation  object 0 target, string 0 dialog file
//   MoveToLocation     vector 0 position, vector 1 orientation, flag 1 run,
//                      flag 10 deactivate at end
//   MoveToObject       object 0 target, float 0 max range (at least the min),
//                      float 1 min range, flag 1 run, flag 5 use original
//                      position; CommandMoveAwayFromObject makes the same
//                      type with float 4 the distance
//   UseAbility         int 0 ability, object 0 target (OBJECT_INVALID for a
//                      point), vector 0 target point, float 0 conjure time,
//                      string 0 ability source, object 1 OBJECT_INVALID
//   EquipItem          object 0 item, int 0 slot, int 1 weapon set
//   UnequipItem        object 0 item
//   UseObject          object 0 target, int 0 action
//   Deathblow          object 0 target, int 0 death type
//   SheatheWeapons     int 0 1 to draw, 0 to sheathe
//   SwitchWeaponSets   int 0 weapon set
//   Turn               vector 0 facing
//   Fly                vector 0 position, vector 1 orientation
//
// Not yet checked in play: the example plugin logs these for the commands
// AddCommand queues.
class Command : Opaque
{
  public:
    std::uint32_t Word() const
    {
        return *reinterpret_cast<const std::uint32_t *>(this);
    }
    core::CommandType Type() const
    {
        return core::TypeOf(Word());
    }
    // One higher each command in the game.
    std::uint16_t Sequence() const
    {
        return core::SequenceOf(Word());
    }

    // Null only for a command the pool could not fill.
    const CDataArrays *Parameters() const
    {
        return Read<const CDataArrays *>(0x4);
    }
    CDataArrays *Parameters()
    {
        return Read<CDataArrays *>(0x4);
    }
    bool Static() const
    {
        return Read<std::int32_t>(0xC) != 0;
    }
    bool PlayerIssued() const
    {
        return Read<std::int32_t>(0x10) != 0;
    }

    // The object a command acts on, for the types whose object 0 is one
    // (above); nothing for the rest, and for a command with no parameters.
    std::optional<ObjectId> Target() const
    {
        using enum core::CommandType;
        switch (Type())
        {
        case Attack:
        case JumpToObject:
        case StartConversation:
        case MoveToObject:
        case UseAbility:
        case UseObject:
        case Deathblow:
            return Object(0);
        default:
            return std::nullopt;
        }
    }
    // The point a command goes to or aims at: a move's, a jump's or a
    // flight's position, an ability's target point.
    std::optional<Vector> Position() const
    {
        using enum core::CommandType;
        switch (Type())
        {
        case JumpToLocation:
        case MoveToLocation:
        case UseAbility:
        case Fly:
            return Parameters() != nullptr ? Parameters()->Vector(0) : std::nullopt;
        default:
            return std::nullopt;
        }
    }
    // A UseAbility's ability (ABILITY_*, the ABI_base table's row).
    std::optional<std::int32_t> Ability() const
    {
        return Type() == core::CommandType::UseAbility ? Integer(0) : std::nullopt;
    }
    std::optional<std::int32_t> WaitMilliseconds() const
    {
        return Type() == core::CommandType::Wait ? Integer(0) : std::nullopt;
    }

  private:
    template <class T> T Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
    std::optional<ObjectId> Object(std::int32_t index) const
    {
        return Parameters() != nullptr ? std::optional<ObjectId>(Parameters()->Object(index)) : std::nullopt;
    }
    std::optional<std::int32_t> Integer(std::int32_t index) const
    {
        return Parameters() != nullptr ? std::optional<std::int32_t>(Parameters()->Integer(index)) : std::nullopt;
    }
};

class CCObject;
class CCCreature;
class CCPlaceable;
class CCWayPoint;
class CCItem;
class CCArea;
class CCModule;
class SEffectsHolder;
class ItemPossessor;

// What the object array holds. Every game object is one, at its start: the
// RTTI's base class descriptors put CGameObject at +0 of every class here
// (tools/daoexe.py --bases CCCreature), so a pointer to one is a pointer to
// the other, as the lookups by id assume.
class CGameObject : Opaque
{
  public:
    // The object's id, what the lookups by id take. CGameObject's
    // constructor (0x00A58240, 2026-09-30) makes a
    // SmartData::Attribute<unsigned int> at +4 and sets it to the id through
    // the attribute's slot 21; an attribute keeps its value at +0xC, as the
    // application's paused flag does (dao/ClientExoApp.h). Checked against
    // the id asked for by the probe's script checks.
    ObjectId Id() const
    {
        return Read<ObjectId>(kId);
    }

    // The byte the constructor stores from its first argument, which the
    // typed lookups compare before they cast (dao/core/ObjectType.h).
    core::ObjectType Type() const
    {
        return static_cast<core::ObjectType>(Read<std::uint8_t>(kType));
    }

    // The engine's casts (dao/generated/ObjectCasts.h): the object as that
    // class, or null if it is not one. A virtual call each, as the engine's
    // lookups make them.
    CCObject *AsObject()
    {
        return As<CCObject>(generated::CastSlot::CCObject);
    }
    CCCreature *AsCreature()
    {
        return As<CCCreature>(generated::CastSlot::CCCreature);
    }
    CCPlaceable *AsPlaceable()
    {
        return As<CCPlaceable>(generated::CastSlot::CCPlaceable);
    }
    CCWayPoint *AsWayPoint()
    {
        return As<CCWayPoint>(generated::CastSlot::CCWayPoint);
    }
    CCItem *AsItem()
    {
        return As<CCItem>(generated::CastSlot::CCItem);
    }
    CCArea *AsArea()
    {
        return As<CCArea>(generated::CastSlot::CCArea);
    }
    CCModule *AsModule()
    {
        return As<CCModule>(generated::CastSlot::CCModule);
    }
    // The effects on the object: a base inside it, at +0x70 of every
    // CCObject and +0xCC of an area. Its layout is not read yet.
    SEffectsHolder *AsEffectsHolder()
    {
        return As<SEffectsHolder>(generated::CastSlot::SEffectsHolder);
    }
    // Its inventory: at +0x440 of a creature, placeable or store, +0x70 of
    // the party. Not read yet either.
    ItemPossessor *AsItemPossessor()
    {
        return As<ItemPossessor>(generated::CastSlot::ItemPossessor);
    }

    // Any cast, for the classes with no wrapper here.
    void *Cast(generated::CastSlot slot)
    {
        using Fn = void *(__fastcall *)(CGameObject *, int);
        const Fn *vtable = *reinterpret_cast<const Fn *const *>(this);
        return vtable[static_cast<std::size_t>(slot)](this, 0);
    }

    static constexpr std::size_t kId = 0x10;
    static constexpr std::size_t kType = 0x14;

  protected:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }

  private:
    template <class T> T *As(generated::CastSlot slot)
    {
        return static_cast<T *>(Cast(slot));
    }
};

class CCObject : public CGameObject
{
  public:
    // The event the object's script is handling, what GetCurrentEvent
    // (0x00658210) gives a script run on it; null when there is none. The
    // engine's own, valid while the script runs.
    const CScriptEvent *CurrentEvent() const
    {
        return Read<const CScriptEvent *>(0x2A8);
    }

    std::int32_t AILevel() const
    {
        return generated::CCObject_AILevel.Get(this);
    }
    std::int32_t GroupId() const
    {
        return generated::CCObject_GroupId.Get(this);
    }
    std::int32_t TeamId() const
    {
        return generated::CCObject_TeamId.Get(this);
    }

    // Queue a command, as the scripts' AddCommand does: at the back, or at
    // the front; overrideBehaviour is AddCommand's nOverrideAddBehavior, -1
    // for the commands table's (dao/Addresses.h). Every queued action of
    // every creature, the player's included, went through this in play
    // (dev/TURN_BASED.md). The queue takes the command, from any of its
    // callers: a detour on this that refuses one frees it
    // (dao::script::DestroyStructure(EngineStructure::Command, command)),
    // since nobody else will.
    int QueueCommand(Command *command, bool addToFront, int overrideBehaviour = -1)
    {
        using Fn = int(__fastcall *)(CCObject *, int, Command *, int, int);
        return reinterpret_cast<Fn>(Module::Address(addr::kCCObject_QueueCommand))(this, 0, command, addToFront ? 1 : 0,
                                                                                   overrideBehaviour);
    }

    // The queue: the command at its head, the one being carried out; one by
    // position, 0 the head; and how many there are. Commands the queue
    // owns, valid until the object moves on from them.
    const Command *CurrentCommand() const
    {
        using Fn = const Command *(__fastcall *)(const CCObject *, int);
        return reinterpret_cast<Fn>(Module::Address(addr::kCCObject_GetCurrentCommand))(this, 0);
    }
    const Command *CommandAt(std::uint16_t index) const
    {
        using Fn = const Command *(__fastcall *)(const CCObject *, int, std::uint32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCCObject_GetCommandByIndex))(this, 0, index);
    }
    std::int32_t CommandQueueSize() const
    {
        return Read<std::int32_t>(0x2D4);
    }
};

class CCCreature : public CCObject
{
  public:
    std::int32_t Package() const
    {
        return generated::CCCreature_Package.Get(this);
    }
    std::int32_t PackageAI() const
    {
        return generated::CCCreature_PackageAI.Get(this);
    }
    std::int32_t TreasureCategory() const
    {
        return generated::CCCreature_CreatureTreasureCategory.Get(this);
    }
    std::int32_t CombatantType() const
    {
        return generated::CCCreature_CombatantType.Get(this);
    }
    ObjectId BodyBag() const
    {
        return generated::CCCreature_CreatureBodyBag.Get(this);
    }
    // The first int of the creature's combat block, which the pointer at
    // +0xC60 points to; its +0x60 is the attack timing SetAttackDuration
    // writes, not wrapped yet.
    std::int32_t CombatState() const
    {
        return generated::CCCreature_CombatState.Get(this);
    }

    // What the scripts' SetCombatState does, through the engine's own
    // setter (__thiscall, one int, `ret 4`). Not yet exercised in play.
    void SetCombatState(std::int32_t state)
    {
        using Fn = void(__fastcall *)(CCCreature *, int, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kCCCreature_SetCombatState))(this, 0, state);
    }

    // The creature's portrait, rendered as the party's are, any creature
    // (dao/Addresses.h, UpdatePortrait): the first call makes its portrait
    // renderer, and every call renders it again. The answer is the URL a
    // movie loads it by, "img://portrait_<tag><counter>" (loadMovie, or an image
    // control), empty if the engine made no portrait. From the scripts'
    // thread (a frame callback); rendering is not free, so once per change
    // rather than per frame. Read 2026-09-30, not yet seen in play.
    std::wstring RenderPortrait()
    {
        using Fn = int(__fastcall *)(CCCreature *, int);
        reinterpret_cast<Fn>(Module::Address(addr::kCCCreature_UpdatePortrait))(this, 0);
        return PortraitImage();
    }
    // Releases the portrait renderer, as the engine's own screens do when
    // they close (dao/Addresses.h, ReleasePortrait): the renderer and its
    // clone of the creature go, and the next RenderPortrait makes a new
    // one. A movie showing the image should
    // let it go first. Nothing if there is none. Not yet seen in play.
    void ReleasePortrait()
    {
        using Fn = bool(__fastcall *)(CCCreature *, int);
        reinterpret_cast<Fn>(Module::Address(addr::kCCCreature_ReleasePortrait))(this, 0);
    }

    // The URL of the portrait last rendered, without rendering; empty if
    // there is none.
    std::wstring PortraitImage() const
    {
        const auto *renderer = Read<const std::byte *>(kPortraitRenderer);
        if (renderer == nullptr)
        {
            return {};
        }
        const auto &name = *reinterpret_cast<const core::WStringRep *>(renderer + kPortraitName);
        return name.View().empty() ? std::wstring{} : L"img://" + std::wstring(name.View());
    }

    static constexpr std::size_t kPortraitRenderer = 0x78C;
    static constexpr std::size_t kPortraitName = 0xD4C; // in the renderer

    // A creature property by the scripts' PROPERTY_* id, as
    // GetCreatureProperty reads it (dao/Addresses.h, CCCreatureStats):
    // attributes, health and mana or stamina, level, experience, speed.
    // Straight from the creature's stats, off the scripts' thread too.
    // Nothing for a creature with no stats. The first try passed the
    // getters the stats rather than their property list and crashed the
    // game in play, 2026-09-30 (dao/Addresses.h); this form not yet seen.
    std::optional<float> Property(std::int32_t property,
                                  script::PropertyValue value = script::PropertyValue::Total) const
    {
        const auto *stats = static_cast<const std::byte *>(Stats());
        if (stats == nullptr)
        {
            return std::nullopt;
        }
        using Fn = float(__fastcall *)(const void *, int, std::int32_t);
        std::uintptr_t getter = addr::kCreatureProperties_GetTotal;
        switch (value)
        {
        case script::PropertyValue::Base:
            getter = addr::kCreatureProperties_GetBase;
            break;
        case script::PropertyValue::Current:
            getter = addr::kCreatureProperties_GetCurrent;
            break;
        case script::PropertyValue::Modifier:
            getter = addr::kCreatureProperties_GetModifier;
            break;
        default:
            break;
        }
        return reinterpret_cast<Fn>(Module::Address(getter))(stats + kProperties, 0, property);
    }
    std::optional<float> Property(script::PropertyAttribute property,
                                  script::PropertyValue value = script::PropertyValue::Total) const
    {
        return Property(static_cast<std::int32_t>(property), value);
    }
    std::optional<float> Property(script::PropertySimple property,
                                  script::PropertyValue value = script::PropertyValue::Total) const
    {
        return Property(static_cast<std::int32_t>(property), value);
    }

    // As SetCreatureProperty does: Total and Base both set the base.
    // False for a creature with no stats. Not yet exercised in play.
    bool SetProperty(std::int32_t property, float amount, script::PropertyValue value)
    {
        void *stats = Stats();
        if (stats == nullptr)
        {
            return false;
        }
        std::uintptr_t setter = addr::kCreatureStats_SetBase;
        if (value == script::PropertyValue::Current)
        {
            setter = addr::kCreatureStats_SetCurrent;
        }
        else if (value == script::PropertyValue::Modifier)
        {
            setter = addr::kCreatureStats_SetModifier;
        }
        using Fn = void(__fastcall *)(void *, int, std::int32_t, float, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(setter))(stats, 0, property, amount, 1);
        return true;
    }

    // Health and mana or stamina now, and their most (the scripts'
    // PROPERTY_DEPLETABLE_HEALTH and _MANA_STAMINA, current and total).
    std::optional<float> Health() const
    {
        return Property(script::constants::PROPERTY_DEPLETABLE_HEALTH, script::PropertyValue::Current);
    }
    std::optional<float> MaxHealth() const
    {
        return Property(script::constants::PROPERTY_DEPLETABLE_HEALTH);
    }
    std::optional<float> ManaStamina() const
    {
        return Property(script::constants::PROPERTY_DEPLETABLE_MANA_STAMINA, script::PropertyValue::Current);
    }
    std::optional<float> MaxManaStamina() const
    {
        return Property(script::constants::PROPERTY_DEPLETABLE_MANA_STAMINA);
    }
    std::optional<float> Level() const
    {
        return Property(script::PropertySimple::Level);
    }

  private:
    // The creature's CCCreatureStats: the value of its attribute at +0x7C4.
    // The setters take it; the getters take its property list, +0x190.
    void *Stats() const
    {
        return Read<void *>(kStats);
    }
    static constexpr std::size_t kStats = 0x7D0;
    static constexpr std::size_t kProperties = 0x190;
};

class CCPlaceable : public CCObject
{
  public:
    std::int32_t PickLockLevel() const
    {
        return generated::CCPlaceable_PlaceablePickLockLevel.Get(this);
    }
    std::int32_t KeyRequired() const
    {
        return generated::CCPlaceable_PlaceableKeyRequired.Get(this);
    }
    std::int32_t AutoRemoveKey() const
    {
        return generated::CCPlaceable_PlaceableAutoRemoveKey.Get(this);
    }
    std::uint8_t BaseType() const
    {
        return generated::CCPlaceable_PlaceableBaseType.Get(this);
    }
    std::uint16_t State() const
    {
        return generated::CCPlaceable_PlaceableState.Get(this);
    }
    std::uint16_t Action() const
    {
        return generated::CCPlaceable_PlaceableAction.Get(this);
    }
    std::int32_t TreasureRank() const
    {
        return generated::CCPlaceable_PlaceableTreasureRank.Get(this);
    }
    std::int32_t TreasureCategory() const
    {
        return generated::CCPlaceable_PlaceableTreasureCategory.Get(this);
    }
};

class CCWayPoint : public CCObject
{
  public:
    std::int32_t MapPinState() const
    {
        return generated::CCWayPoint_MapPinState.Get(this);
    }
};

// Classes the casts reach that have no fields read yet.
class CCItem : public CCObject
{
};
class CCArea : public CGameObject
{
  public:
    // As CCObject::CurrentEvent, for an area's script (GetCurrentEvent).
    const CScriptEvent *CurrentEvent() const
    {
        return Read<const CScriptEvent *>(0x3C4);
    }
};
class CCModule : public CGameObject
{
  public:
    // As CCObject::CurrentEvent, for the module's script (GetCurrentEvent).
    const CScriptEvent *CurrentEvent() const
    {
        return Read<const CScriptEvent *>(0x390);
    }
};
class SEffectsHolder : Opaque
{
};
class ItemPossessor : Opaque
{
};

} // namespace dao

#pragma warning(pop)
