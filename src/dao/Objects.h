// The engine's game objects: creatures, placeables, waypoints, areas, the module, and the
// commands queued on them. Look an object up by id with CClientExoApp (dao/ClientExoApp.h), then
// use these classes to read its fields and call the engine's own functions on it. The classes
// are opaque: the engine creates and frees every object, and a plugin only holds pointers.

// Notes on this file: the hierarchy is the RTTI's: CCCreature, CCPlaceable and CCWayPoint are
// CCObjects, and CCObject is a CGameObject, each at the start of the class derived from it, so
// these classes add no adjustment. The engine's casts between them are CGameObject's virtual As*
// slots (tools/object_casts.py, dev/object_casts.csv), which the lookups by id call.
//
// Every field here is one a script command returns (the generated descriptors in
// dao/generated/ScriptFields.h) and is checked against that command's own answer by the probe in
// play (dev/PROBE.md, "Script checks"). In the first session, 2026-09-30, 14 of the 18 matched on
// every call: all but BodyBag, AILevel, the placeable's State and MapPinState, which no script
// called (dev/LIBRARY.md). The functions are called exactly as the engine calls them; how each
// was read is in dao/Addresses.h.
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

// The base of the engine types in this file. A plugin can only hold pointers to them; it never
// creates, copies or destroys one.
struct Opaque
{
    Opaque() = delete;
    Opaque(const Opaque &) = delete;
    Opaque &operator=(const Opaque &) = delete;
    ~Opaque() = delete;
};

// Notes on Command: 0x14 bytes from the AI's pool (dao/Addresses.h, CommandPool), read
// 2026-09-30 from its constructor (0x00741F10), the 17 script commands that build one, and
// AddCommand, GetCommandType and GetCommandIsPlayerIssued:
// - +0x0 the word: sequence (bits 0-15), type (+2, the byte SetType writes and GetCommandType
//   reads); +3 not read
// - +0x4 the parameters, a CDataArrays of its own (dao/Structures.h)
// - +0x8 1 from the constructor; not read yet
// - +0xC static: AddCommand stores its bStatic here before queueing
// - +0x10 player issued: what GetCommandIsPlayerIssued answers
// The parameters by type are not yet checked in play: the example plugin logs them for the
// commands AddCommand queues.

// A command queued on an object (CCObject::QueueCommand). This is the scripts' `command` type,
// which CommandAttack, CommandWait and the other Command* functions create.
//
// The parameters each type of command carries, as kind and index (int, float, flag, object,
// string or vector) followed by what the value means:
//
//   Attack             object 0 target, int 2 forced result (COMBAT_RESULT_*)
//   JumpToObject       object 0 target
//   JumpToLocation     vector 0 position, vector 1 orientation, object 0 area
//   Wait               int 0 milliseconds (the seconds times 1000, at least 0)
//   PlayAnimation      int 0 animation, int 2 loops, int 7 blend (0 to blend in,
//                      otherwise 2), flag 4 play next
//   StartConversation  object 0 target, string 0 dialog file
//   MoveToLocation     vector 0 position, vector 1 orientation, flag 1 run,
//                      flag 10 deactivate at end
//   MoveToObject       object 0 target, float 0 max range (at least the min),
//                      float 1 min range, flag 1 run, flag 5 use original
//                      position. CommandMoveAwayFromObject makes this type
//                      too, with float 4 the distance
//   UseAbility         int 0 ability, object 0 target (OBJECT_INVALID for a
//                      point), vector 0 target point, float 0 conjure time,
//                      string 0 ability source, object 1 OBJECT_INVALID
//   EquipItem          object 0 item, int 0 slot, int 1 weapon set
//   UnequipItem        object 0 item
//   UseObject          object 0 target, int 0 action
//   Deathblow          object 0 target, int 0 death type
//   SheatheWeapons     int 0 draw (1) or sheathe (0)
//   SwitchWeaponSets   int 0 weapon set
//   Turn               vector 0 facing
//   Fly                vector 0 position, vector 1 orientation
class Command : Opaque
{
  public:
    // Returns the command's first word, which holds its sequence number and type
    // (dao/core/CommandWord.h).
    std::uint32_t Word() const
    {
        return *reinterpret_cast<const std::uint32_t *>(this);
    }
    // Returns the command's type.
    core::CommandType Type() const
    {
        return core::TypeOf(Word());
    }
    // Returns the command's sequence number. Each new command in the game gets the next number.
    std::uint16_t Sequence() const
    {
        return core::SequenceOf(Word());
    }

    // Returns the command's parameters (see the table above).
    // @return the parameters, or null only for a command the command pool failed to fill
    const CDataArrays *Parameters() const
    {
        return Read<const CDataArrays *>(0x4);
    }
    // Returns the command's parameters, so you can change them before the command starts.
    // @return the parameters, or null only for a command the command pool failed to fill
    CDataArrays *Parameters()
    {
        return Read<CDataArrays *>(0x4);
    }
    // Returns whether the command is static, the bStatic argument of AddCommand.
    bool Static() const
    {
        return Read<std::int32_t>(0xC) != 0;
    }
    // Returns whether the player issued the command, the same as GetCommandIsPlayerIssued.
    bool PlayerIssued() const
    {
        return Read<std::int32_t>(0x10) != 0;
    }

    // Returns the object the command acts on. This is object 0 of an attack, a jump to an
    // object, a conversation, a move to an object, an ability, a use or a deathblow.
    // @return the target, or nullopt for other types and for a command with no parameters
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
    // Returns the point the command moves to or aims at: the position of a move, a jump or a
    // flight, or the target point of an ability.
    // @return the point, or nullopt for other types and for a command with no parameters
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
    // Returns the ability of a UseAbility command: an ABILITY_* id, which is its row in the
    // ABI_base table.
    // @return the ability, or nullopt for other types and for a command with no parameters
    std::optional<std::int32_t> Ability() const
    {
        return Type() == core::CommandType::UseAbility ? Integer(0) : std::nullopt;
    }
    // Returns how long a Wait command waits, in milliseconds.
    // @return the time, or nullopt for other types and for a command with no parameters
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

// Notes on CGameObject: it is what the object array holds. The RTTI's base class descriptors put
// CGameObject at +0 of every class here (tools/daoexe.py --bases CCCreature), so a pointer to one
// is a pointer to the other, as the lookups by id assume.

// Any game object; the base of every object class in this file. The As* functions cast it to a
// more specific class.
class CGameObject : Opaque
{
  public:
    // Notes on Id: CGameObject's constructor (0x00A58240, 2026-09-30) makes a
    // SmartData::Attribute<unsigned int> at +4 and sets it to the id through the attribute's slot
    // 21; an attribute keeps its value at +0xC, as the application's paused flag does
    // (dao/ClientExoApp.h). Checked against the id asked for by the probe's script checks.

    // Returns the object's id, the number CClientExoApp's lookups take.
    ObjectId Id() const
    {
        return Read<ObjectId>(kId);
    }

    // Returns the object's type (dao/core/ObjectType.h). The engine's typed lookups check it
    // before they cast.
    core::ObjectType Type() const
    {
        return static_cast<core::ObjectType>(Read<std::uint8_t>(kType));
    }

    // Notes on the casts: each is a virtual call, as the engine's lookups make them
    // (dao/generated/ObjectCasts.h).

    // Returns the object as a CCObject, or null if it is not one.
    CCObject *AsObject()
    {
        return As<CCObject>(generated::CastSlot::CCObject);
    }
    // Returns the object as a creature, or null if it is not one.
    CCCreature *AsCreature()
    {
        return As<CCCreature>(generated::CastSlot::CCCreature);
    }
    // Returns the object as a placeable, or null if it is not one.
    CCPlaceable *AsPlaceable()
    {
        return As<CCPlaceable>(generated::CastSlot::CCPlaceable);
    }
    // Returns the object as a waypoint, or null if it is not one.
    CCWayPoint *AsWayPoint()
    {
        return As<CCWayPoint>(generated::CastSlot::CCWayPoint);
    }
    // Returns the object as an item, or null if it is not one.
    CCItem *AsItem()
    {
        return As<CCItem>(generated::CastSlot::CCItem);
    }
    // Returns the object as an area, or null if it is not one.
    CCArea *AsArea()
    {
        return As<CCArea>(generated::CastSlot::CCArea);
    }
    // Returns the object as the module, or null if it is not one.
    CCModule *AsModule()
    {
        return As<CCModule>(generated::CastSlot::CCModule);
    }
    // Notes on AsEffectsHolder: the holder is a base inside the object, at +0x70 of every
    // CCObject and +0xCC of an area.

    // Returns the object's effects holder, which keeps the effects on it, or null if it has none.
    // The holder's layout is not yet known.
    SEffectsHolder *AsEffectsHolder()
    {
        return As<SEffectsHolder>(generated::CastSlot::SEffectsHolder);
    }
    // Notes on AsItemPossessor: the inventory is at +0x440 of a creature, placeable or store,
    // +0x70 of the party.

    // Returns the object's inventory, or null if it has none. Its layout is not yet known.
    ItemPossessor *AsItemPossessor()
    {
        return As<ItemPossessor>(generated::CastSlot::ItemPossessor);
    }

    // Casts the object to any class the engine can cast to. Use it for classes that have no As*
    // function here.
    // @return the object as that class, or null if it is not one
    void *Cast(generated::CastSlot slot)
    {
        using Fn = void *(__fastcall *)(CGameObject *, int);
        const Fn *vtable = *reinterpret_cast<const Fn *const *>(this);
        return vtable[static_cast<std::size_t>(slot)](this, 0);
    }

    static constexpr std::size_t kId = 0x10;   // offset of the id
    static constexpr std::size_t kType = 0x14; // offset of the type byte

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

// An object in the world: a creature, placeable, waypoint or item. It has a position, an area
// and a queue of commands.
class CCObject : public CGameObject
{
  public:
    // Notes on CurrentEvent: GetCurrentEvent is 0x00658210.

    // Returns the event the object's script is handling, the same one GetCurrentEvent returns to
    // the script. The engine owns the event, and it is valid only while the script runs.
    // @return the event, or null if there is none
    const CScriptEvent *CurrentEvent() const
    {
        return Read<const CScriptEvent *>(0x2A8);
    }

    // Notes on Position, AreaId and Area: GetPosition's handler (0x00655400) reads the three
    // floats at +0x120, which a fourth pads to 16 bytes. GetArea (0x00655000) reads the id at
    // +0x254, which line of sight compares too (0x00668DB0). Area calls the object's slot 40 and
    // casts the result to an area (0x00695180), as the scripts' line of sight finds it.

    // Returns the object's position, the same as GetPosition.
    Vector Position() const
    {
        return Read<Vector>(0x120);
    }
    // Returns the id of the area the object is in, the same as GetArea.
    ObjectId AreaId() const
    {
        return Read<ObjectId>(0x254);
    }
    // Returns the area the object is in.
    // @return the area, or null if the object is not in an area
    CCArea *Area()
    {
        using Fn = CCArea *(__fastcall *)(CCObject *);
        return reinterpret_cast<Fn>(Module::Address(addr::kCCObject_Area))(this);
    }

    // Returns the object's AI level, the same as GetAILevel.
    std::int32_t AILevel() const
    {
        return generated::CCObject_AILevel.Get(this);
    }
    // Returns the object's group, the same as GetGroupId.
    std::int32_t GroupId() const
    {
        return generated::CCObject_GroupId.Get(this);
    }
    // Returns the object's team, the same as GetTeamId.
    std::int32_t TeamId() const
    {
        return generated::CCObject_TeamId.Get(this);
    }

    // Notes on QueueCommand: every queued action of every creature, the player's included, went
    // through it in play (dev/TURN_BASED.md). The -1 default for overrideBehaviour is the
    // commands table's (dao/Addresses.h).

    // Queues a command on the object, the same as the scripts' AddCommand.
    // @param command the command. The queue takes ownership of it
    // @param addToFront true to put it at the front of the queue, false to put it at the back
    // @param overrideBehaviour AddCommand's nOverrideAddBehavior. 0 just adds; 1 clears the
    //  queue first; 2 clears it hard; 3 clears it soft or hard, depending on whether the
    //  command's target is the creature's current target; -1 uses the default for the
    //  command's type, from the commands table
    // @return 1, or 0 if the command is null
    //
    // If you detour this function and refuse a command, free it with
    // dao::script::DestroyStructure(EngineStructure::Command, command), because nothing else will.
    int QueueCommand(Command *command, bool addToFront, int overrideBehaviour = -1)
    {
        using Fn = int(__fastcall *)(CCObject *, int, Command *, int, int);
        return reinterpret_cast<Fn>(Module::Address(addr::kCCObject_QueueCommand))(this, 0, command, addToFront ? 1 : 0,
                                                                                   overrideBehaviour);
    }

    // Returns the command at the head of the queue, which is the one being carried out. The
    // queue owns the command, and it stays valid until the object moves on from it.
    // @return the command, or null if the queue is empty
    const Command *CurrentCommand() const
    {
        using Fn = const Command *(__fastcall *)(const CCObject *, int);
        return reinterpret_cast<Fn>(Module::Address(addr::kCCObject_GetCurrentCommand))(this, 0);
    }
    // Returns the command at a position in the queue, where 0 is the head. The queue owns the
    // command, and it stays valid until the object moves on from it.
    // @return the command, or null if the position is past the end of the queue
    const Command *CommandAt(std::uint16_t index) const
    {
        using Fn = const Command *(__fastcall *)(const CCObject *, int, std::uint32_t);
        return reinterpret_cast<Fn>(Module::Address(addr::kCCObject_GetCommandByIndex))(this, 0, index);
    }
    // Returns the number of commands in the queue.
    std::int32_t CommandQueueSize() const
    {
        return Read<std::int32_t>(0x2D4);
    }
};

// A creature: a party member, or anyone else who fights, talks or walks.
class CCCreature : public CCObject
{
  public:
    // Returns the creature's package, the same as GetPackage.
    std::int32_t Package() const
    {
        return generated::CCCreature_Package.Get(this);
    }
    // Returns the creature's AI package, the same as GetPackageAI.
    std::int32_t PackageAI() const
    {
        return generated::CCCreature_PackageAI.Get(this);
    }
    // Returns the creature's treasure category, the same as GetCreatureTreasureCategory.
    std::int32_t TreasureCategory() const
    {
        return generated::CCCreature_CreatureTreasureCategory.Get(this);
    }
    // Returns the creature's combatant type, the same as GetCombatantType.
    std::int32_t CombatantType() const
    {
        return generated::CCCreature_CombatantType.Get(this);
    }
    // Returns the creature's body bag, the placeable it leaves behind when it dies. The same as
    // GetCreatureBodyBag.
    ObjectId BodyBag() const
    {
        return generated::CCCreature_CreatureBodyBag.Get(this);
    }

    // Notes on CombatState: it is the first int of the creature's combat block, which the pointer
    // at +0xC60 points to; the block's +0x60 is the attack timing SetAttackDuration writes, not
    // wrapped yet.

    // Returns the creature's combat state, the same as GetCombatState.
    std::int32_t CombatState() const
    {
        return generated::CCCreature_CombatState.Get(this);
    }

    // Notes on SetCombatState: the engine's own setter, __thiscall, one int, `ret 4`. Not yet
    // exercised in play.

    // Sets the creature's combat state, the same as the scripts' SetCombatState. Not yet tested
    // in the game.
    void SetCombatState(std::int32_t state)
    {
        using Fn = void(__fastcall *)(CCCreature *, int, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kCCCreature_SetCombatState))(this, 0, state);
    }

    // Notes on RenderPortrait: the engine's UpdatePortrait (dao/Addresses.h), read 2026-09-30,
    // not yet seen in play.

    // Renders the creature's portrait the same way the party's portraits are rendered. It works
    // for any creature. The first call creates a portrait renderer for the creature, and every
    // call renders the portrait again.
    // @return the URL a Flash movie loads the image from, "img://portrait_<tag><counter>", with
    //  loadMovie or in an image control; empty if the engine made no portrait
    //
    // Call it on the game thread, for example from a frame callback. Rendering is not cheap, so
    // render once per change, not every frame. ReleasePortrait frees the renderer.
    std::wstring RenderPortrait()
    {
        using Fn = int(__fastcall *)(CCCreature *, int);
        reinterpret_cast<Fn>(Module::Address(addr::kCCCreature_UpdatePortrait))(this, 0);
        return PortraitImage();
    }
    // Notes on ReleasePortrait: the engine's own ReleasePortrait (dao/Addresses.h). Not yet seen
    // in play.

    // Frees the creature's portrait renderer and the copy of the creature it draws, as the
    // game's own screens do when they close. The next RenderPortrait creates a new renderer.
    // Does nothing if there is no renderer. Unload the image from any movie showing it first.
    // Not yet tested in the game.
    void ReleasePortrait()
    {
        using Fn = bool(__fastcall *)(CCCreature *, int);
        reinterpret_cast<Fn>(Module::Address(addr::kCCCreature_ReleasePortrait))(this, 0);
    }

    // Returns the URL of the most recently rendered portrait without rendering again, or an
    // empty string if there is none.
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

    static constexpr std::size_t kPortraitRenderer = 0x78C; // offset of the portrait renderer
    static constexpr std::size_t kPortraitName = 0xD4C;     // the image's name, in the renderer

    // Notes on Property: it reads as GetCreatureProperty does (dao/Addresses.h, CCCreatureStats).
    // The first try passed the getters the stats rather than their property list and crashed the
    // game in play, 2026-09-30 (dao/Addresses.h); this form is not yet seen in play.

    // Returns a creature property by its PROPERTY_* id, such as an attribute, health, mana or
    // stamina, level, experience or speed. It reads the creature's stats directly, so you can
    // call it off the game thread.
    // @param property the PROPERTY_* id
    // @param value which value to read: the total, the base, the current value or the modifier
    // @return the value, or nullopt if the creature has no stats
    std::optional<float> Property(std::int32_t property,
                                  script::PropertyValue value = script::PropertyValue::Total) const
    {
        const auto *stats = static_cast<const std::byte *>(Stats());
        if (stats == nullptr)
        {
            return std::nullopt;
        }
        using Fn = float(__fastcall *)(const void *, int, std::int32_t);
        AddressId getter = addr::kCreatureProperties_GetTotal;
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
    // Returns an attribute. The same as Property with the attribute's id.
    std::optional<float> Property(script::PropertyAttribute property,
                                  script::PropertyValue value = script::PropertyValue::Total) const
    {
        return Property(static_cast<std::int32_t>(property), value);
    }
    // Returns a simple property. The same as Property with the property's id.
    std::optional<float> Property(script::PropertySimple property,
                                  script::PropertyValue value = script::PropertyValue::Total) const
    {
        return Property(static_cast<std::int32_t>(property), value);
    }

    // Notes on SetProperty: not yet exercised in play.

    // Sets a creature property, the same as the scripts' SetCreatureProperty. Not yet tested in
    // the game.
    // @param property the PROPERTY_* id
    // @param amount the new value
    // @param value which value to set. Total and Base both set the base value
    // @return false if the creature has no stats
    bool SetProperty(std::int32_t property, float amount, script::PropertyValue value) const
    {
        void *stats = Stats();
        if (stats == nullptr)
        {
            return false;
        }
        AddressId setter = addr::kCreatureStats_SetBase;
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

    // Returns the creature's current health, or nullopt if it has no stats.
    std::optional<float> Health() const
    {
        return Property(script::constants::PROPERTY_DEPLETABLE_HEALTH, script::PropertyValue::Current);
    }
    // Returns the creature's maximum health, or nullopt if it has no stats.
    std::optional<float> MaxHealth() const
    {
        return Property(script::constants::PROPERTY_DEPLETABLE_HEALTH);
    }
    // Returns the creature's current mana or stamina, or nullopt if it has no stats.
    std::optional<float> ManaStamina() const
    {
        return Property(script::constants::PROPERTY_DEPLETABLE_MANA_STAMINA, script::PropertyValue::Current);
    }
    // Returns the creature's maximum mana or stamina, or nullopt if it has no stats.
    std::optional<float> MaxManaStamina() const
    {
        return Property(script::constants::PROPERTY_DEPLETABLE_MANA_STAMINA);
    }
    // Returns the creature's level, or nullopt if it has no stats.
    std::optional<float> Level() const
    {
        return Property(script::PropertySimple::Level);
    }

    // Notes on Stats: the stats are the value of the creature's attribute at +0x7C4. The
    // property setters take the stats, and the getters their property list at +0x190.

    // Returns the creature's stats, the engine's CCCreatureStats object, or null if it has none.
    // dao/Abilities.h reads ability cooldowns from it.
    void *Stats() const
    {
        return Read<void *>(kStats);
    }
    static constexpr std::size_t kStats = 0x7D0; // offset of the stats pointer

  private:
    static constexpr std::size_t kProperties = 0x190;
};

// A placeable: a door, chest, lever or other usable thing in the world. Each getter returns the
// same value as the script command of the same name (GetPlaceable...).
class CCPlaceable : public CCObject
{
  public:
    // Returns the lock's pick-lock level.
    std::int32_t PickLockLevel() const
    {
        return generated::CCPlaceable_PlaceablePickLockLevel.Get(this);
    }
    // Returns nonzero if a key is required to open the placeable.
    std::int32_t KeyRequired() const
    {
        return generated::CCPlaceable_PlaceableKeyRequired.Get(this);
    }
    // Returns nonzero if the key is removed from the inventory when used.
    std::int32_t AutoRemoveKey() const
    {
        return generated::CCPlaceable_PlaceableAutoRemoveKey.Get(this);
    }
    // Returns the placeable's base type.
    std::uint8_t BaseType() const
    {
        return generated::CCPlaceable_PlaceableBaseType.Get(this);
    }
    // Returns the placeable's state.
    std::uint16_t State() const
    {
        return generated::CCPlaceable_PlaceableState.Get(this);
    }
    // Returns the placeable's action.
    std::uint16_t Action() const
    {
        return generated::CCPlaceable_PlaceableAction.Get(this);
    }
    // Returns the placeable's treasure rank.
    std::int32_t TreasureRank() const
    {
        return generated::CCPlaceable_PlaceableTreasureRank.Get(this);
    }
    // Returns the placeable's treasure category.
    std::int32_t TreasureCategory() const
    {
        return generated::CCPlaceable_PlaceableTreasureCategory.Get(this);
    }
};

// A waypoint: a marked point in an area, which can also be a map pin.
class CCWayPoint : public CCObject
{
  public:
    // Returns the state of the waypoint's map pin, the same as GetMapPinState.
    std::int32_t MapPinState() const
    {
        return generated::CCWayPoint_MapPinState.Get(this);
    }
};

// An item. None of its fields are wrapped yet.
class CCItem : public CCObject
{
};
// An area of the world.
class CCArea : public CGameObject
{
  public:
    // Returns the event the area's script is handling, like CCObject::CurrentEvent, or null if
    // there is none.
    const CScriptEvent *CurrentEvent() const
    {
        return Read<const CScriptEvent *>(0x3C4);
    }
};
// The module: the game being played, which holds the areas. Get it with
// CClientExoApp::GetModule.
class CCModule : public CGameObject
{
  public:
    // Notes on CurrentArea: the engine's function (0x005FE470) returns the module's own area at
    // +0x388 once that one is loaded (its byte at +0x45C), else the area of the application's
    // controlled creature.

    // Returns the area the player is in.
    // @return the area, or null before an area has loaded
    CCArea *CurrentArea()
    {
        using Fn = CCArea *(__fastcall *)(CCModule *);
        return reinterpret_cast<Fn>(Module::Address(addr::kCCModule_CurrentArea))(this);
    }

    // Returns the event the module's script is handling, like CCObject::CurrentEvent, or null if
    // there is none.
    const CScriptEvent *CurrentEvent() const
    {
        return Read<const CScriptEvent *>(0x390);
    }
};
// The effects on an object (CGameObject::AsEffectsHolder). Its layout is not yet known.
class SEffectsHolder : Opaque
{
};
// An object's inventory (CGameObject::AsItemPossessor). Its layout is not yet known.
class ItemPossessor : Opaque
{
};

} // namespace dao

#pragma warning(pop)
