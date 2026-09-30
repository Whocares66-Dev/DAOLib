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
#include "dao/core/CommandWord.h"
#include "dao/core/ObjectType.h"
#include "dao/generated/ObjectCasts.h"
#include "dao/generated/ScriptFields.h"

#include <cstddef>
#include <cstdint>

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
// `command`, made by CommandAttack, CommandWait and the rest.
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

  private:
    template <class T> T *As(generated::CastSlot slot)
    {
        return static_cast<T *>(Cast(slot));
    }

    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

class CCObject : public CGameObject
{
  public:
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
    // for the default. Every queued action of every creature, the player's
    // included, went through this in play (dev/TURN_BASED.md).
    int QueueCommand(Command *command, bool addToFront, int overrideBehaviour = -1)
    {
        using Fn = int(__fastcall *)(CCObject *, int, Command *, int, int);
        return reinterpret_cast<Fn>(Module::Address(addr::kObject_QueueCommand))(this, 0, command, addToFront ? 1 : 0,
                                                                                 overrideBehaviour);
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
};
class CCModule : public CGameObject
{
};
class SEffectsHolder : Opaque
{
};
class ItemPossessor : Opaque
{
};

} // namespace dao
