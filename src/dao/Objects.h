// The engine's game objects, as a plugin uses them: opaque classes, never
// made or copied by us, reached through the application's lookups by id
// (dao/ClientExoApp.h), with accessors for the fields read so far and
// wrappers for the engine functions whose signatures are known.
//
// The hierarchy is the RTTI's: CCCreature, CCPlaceable and CCWayPoint are
// CCObjects (the cast every game object answers, dev/SCRIPT_COMMANDS.md),
// and CCObject is a CGameObject. The lookups return the same pointer for
// every level, so these classes add no adjustment.
//
// Every field here is one a script command returns (the generated
// descriptors in dao/generated/ScriptFields.h) and is checked against that
// command's own answer by the probe in play (dev/PROBE.md, "Script checks").
// The functions are called exactly as the engine calls them; how each was
// read is in dao/Addresses.h.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/core/CommandWord.h"
#include "dao/generated/ScriptFields.h"

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

// What the object array holds; every game object is one.
class CGameObject : Opaque
{
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

} // namespace dao
