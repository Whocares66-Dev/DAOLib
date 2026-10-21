// A saved game (<area>.das, GFF4 type "SAVE") read into plain structs: the campaign with its journal
// and variables, the areas with the objects left in them, the hero and the party, and the game's state
// (DAOLib #202; the request is dev/PARAGON_SAVES.md, the file's layout dev/SAVES.md).
//
//   const auto file = dao::core::gff4::File::Parse(bytes);
//   const auto save = dao::core::savegame::Read(*file);   // or Read(bytes)
//   for (const auto &area : save->areas) ...
//
// Every part is read through Gff4's bounds-checked accessors, so a damaged or partial save gives what its
// parts held: a field the file lacks keeps the default of its member (0, empty, none), and a list that
// does not read is empty. A file that is not a SAVE with a top struct gives nothing. Nothing throws.
//
// What each field rests on is marked beside it in Savegame.cpp:
//   checked    the loader in the exe reads it so, at the address given (0x006882C0 the object load,
//              0x00683820 an area's creatures, 0x00683380 its placeables, 0x006F7290 the journal, 0x006BE980 the
//              creature's restore);
//   file       seen only in saves (tools/gff4.py --dump of the player's), the type and the meaning from the
//              field's name (xoreos's table, dev/gff4_fields.csv) and the values;
//   raw        kept as the file has it, no meaning known: the hero's morph, the events' data arrays, the
//              effects' engine data, one unnamed field of an item and two of the hero's stats.
// A default is that of this reader, not shown to be the engine's, unless the .cpp says so.
//
// What a caller does with the objects. The engine builds a creature, placeable, trigger, waypoint,
// store, sound or area-of-effect from its template (TemplateResRef, loaded first) and lays the saved
// fields over it; a field the save lacks keeps the template's value. So every such field here is an
// Opt, none where the save lacks it (the label is not in the struct's template), and a caller fills the
// rest from the template. A stat list or inventory list is none where its field is absent, and a
// creature's `stats` is none for a template whose Combatant flag is set (the writer saves combatants'
// stats only), which means the template's stats stay. A creature's commands and action queue are applied
// only when a saved game is loaded, not when an area is returned to. Stats are restored by their row of
// properties.gda. The hero's morph (16950) is read by the model builder from the PlayerChar struct, not
// by the creature's restore. A trigger's or a store's ObjectID is written and not read.
//
// Not read: the add-ins list (16006) and the story so far (16008), which the load path has no reader for.
//
// Write (DAOLib #221, below) takes the original file's bytes and a Save changed after Read, and gives the new
// bytes through Gff4's Editor; SavegameWrite.cpp marks what each part of it rests on. Not written: the two
// lists just named (they ride along untouched), a field a struct's template lacks, and the removal of a
// field or a child struct.
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace dao::core::savegame
{

inline constexpr std::uint32_t kNoObject = 0xFFFFFFFF;
inline constexpr std::uint32_t kNoStrref = 0xFFFFFFFF;
// A list's element made by a caller has no source. One Read made has the offset in the file of the struct it
// was read from (`source`), which is how Write pairs it with that struct whatever place it now holds in its list.
inline constexpr std::uint32_t kNoSource = 0xFFFFFFFF;

// A field the save may lack. See the note on the objects below.
template <class T> using Opt = std::optional<T>;

struct Vec3
{
    float x = 0;
    float y = 0;
    float z = 0;
    bool operator==(const Vec3 &) const = default;
};

// A rotation as the file has it: x, y, z, w.
struct Quat
{
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 1;
    bool operator==(const Quat &) const = default;
};

// -- Script variables

// An object variable holds an object's id, a resource variable a resource's name.
struct ObjectId
{
    std::uint32_t id = kNoObject;
    bool operator==(const ObjectId &) const = default;
};
struct ResRef
{
    std::wstring name;
    bool operator==(const ResRef &) const = default;
};
// SVI1 an int, SVF1 a float, SVS1 a string, SVO1 an object, SVR1 a resource: the struct's template says which.
using VariableValue = std::variant<std::int32_t, float, std::wstring, ObjectId, ResRef>;
struct Variable
{
    std::wstring name;
    VariableValue value;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// -- Effects, commands and the data arrays they carry

// A DTA1: the seven arrays an effect, a command or a script event carries its data in, by type. Kept as
// they are; what an effect's slots mean is the effect's type's.
struct DataArrays
{
    std::vector<std::int32_t> ints;
    std::vector<float> floats;
    std::vector<std::uint8_t> bools;
    std::vector<std::uint32_t> objects;
    std::vector<std::wstring> strings;
    std::vector<Vec3> vectors;
    std::vector<Quat> quaternions;
    bool Empty() const;
};

// A GME1.
struct Effect
{
    std::int32_t id = 0;
    std::int32_t type = 0;
    std::int32_t durationType = 0;
    std::int32_t duration =
        0; // milliseconds: AgeEffect adds the update's milliseconds to the elapsed time, capped at this (dev/COMBAT.md)
    std::int32_t timeIndex =
        0; // milliseconds, the elapsed time against that clock (a saved 599000 is just under ten minutes)
    std::int32_t animation = 0;
    Opt<std::int32_t> priority; // as saved; Priority() is what the engine ends with
    std::uint32_t creator = kNoObject;
    std::int32_t abilityId = 0;
    // An effect is made new and the save laid over it, with the data arrays only where they hold elements.
    DataArrays engineData;       // raw
    std::int32_t resource2 = -1; // the effect's constructor's
    std::int32_t flags = 0;
    DataArrays data;

    // The priority the engine ends with [exe 0x00A65380]: the saved one, or where the field is absent 999
    // for duration type 6, 0 for an effect with no animation and a type of 12 or more, else 0.
    std::int32_t Priority() const
    {
        if (priority)
        {
            return *priority;
        }
        if (durationType == 6)
        {
            return 999;
        }
        return 0;
    }
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A CMD1.
struct Command
{
    std::uint8_t commandId = 0;
    std::uint16_t id = 0; // SavegameCommandID: written, never read; a restored command gets a new id
    std::uint8_t isStatic = 0;
    DataArrays data;
    std::uint8_t playerIssued = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A SAC1.
struct Subaction
{
    std::uint32_t id = 0;
    std::uint8_t coreSubaction = 0;
    std::uint8_t coreInterruptable = 0;
    std::int32_t timeIndex = 0;
    std::int32_t lastTimeIndex = 0;
    std::int32_t length = 0;
    std::int32_t startTime = 0;
    DataArrays data;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A CAQ1: a creature's or placeable's command in progress and its sub-actions.
struct ActionQueue
{
    std::optional<Command> current;
    std::vector<Subaction> subactions;
};

// -- Items

// An ITL1. Where it sits is its Position: (slot, weapon set, 0) for an equipped item.
struct Item
{
    Opt<std::wstring> templateResRef;
    Opt<Vec3> position;
    Opt<std::uint32_t> id;
    Opt<std::int32_t> cost;
    Opt<std::int32_t> stackSize;
    Opt<std::uint32_t> abilityId;
    std::vector<std::int32_t> properties;
    std::vector<std::int32_t> propertyPowers;
    std::vector<std::int32_t> propertyEffectIds;
    std::vector<std::int32_t> propertyVfxIds;
    std::vector<std::wstring> subitemResRefs;
    Opt<std::int32_t> craftingRecipeId;
    Opt<std::int32_t> baseCost;
    Opt<std::wstring> tag;
    Opt<std::uint8_t> droppable;
    Opt<std::uint8_t> damaged;
    Opt<std::uint8_t> irremovable;
    Opt<std::uint8_t> indestructible;
    Opt<std::int32_t> materialType;
    Opt<std::uint8_t> stealable;
    Opt<std::uint8_t> infinite;
    Opt<std::int32_t> currentVfxPropertyId;
    Opt<std::uint32_t> currentVfxPropertyPower;
    Opt<std::uint8_t> field16237; // raw: unnamed in xoreos
    Opt<std::uint8_t> plot;
    std::vector<Effect> effects;
    std::vector<Variable> variables;

    // Where an equipped item is: its slot and weapon set, 0 where it has no Position.
    int Slot() const
    {
        return position ? static_cast<int>(position->x) : 0;
    }
    int WeaponSet() const
    {
        return position ? static_cast<int>(position->y) : 0;
    }
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// -- Creatures

// One entry of a creature's stat list, keyed by its row of properties.gda (`index`): the engine restores
// it into the property of that row, whose class (its row's) decides what is read, not the struct's label.
// An SMPL holds a base, an ATTR a base and a modifier, a DPLT (health, mana, stamina) a base, modifier,
// current value and the two regenerations; `kind` is the label, the fields the struct lacks are none.
struct Stat
{
    enum class Kind : std::uint8_t
    {
        Simple,
        Attribute,
        Depletable,
    };
    Kind kind = Kind::Simple;
    std::uint16_t index = 0;
    Opt<float> base;
    Opt<float> modifier;
    Opt<float> current;
    Opt<std::int32_t> combatRegen;
    Opt<std::int32_t> regen;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A CQS1.
struct Quickslot
{
    std::uint32_t ability = 0;
    std::wstring itemTag;
    std::wstring templateResRef;
    std::int8_t number = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A creature's CRS1 (an ordinary creature's) or CPS1 (a party member's, with the quick bar): its spells,
// talents and skills as ids of their rows, its stats, and the quick bar's slots (five lists in a CPS1).
struct Stats
{
    std::vector<std::uint32_t> spells;
    std::vector<std::uint32_t> talents;
    std::vector<std::uint32_t> skills;
    std::vector<Stat> stats;
    std::vector<Stat> heroicStats;
    std::vector<Stat> heroicPartyStats;
    std::vector<std::int32_t> modalAbilities;
    std::array<std::vector<Quickslot>, 5> quickslots; // the CPS1's Quickslots, Quickslots1 to Quickslots4
    Opt<std::uint32_t> currentQbar;
    Opt<std::int8_t> lockQbar;
    Opt<std::int8_t> autoLevelUp;
    Opt<std::wstring> field16339; // raw: unnamed in xoreos
    Opt<std::wstring> field16340; // raw: unnamed
    bool hasQuickbar = false;     // it was a CPS1
    // The stat of that row of properties.gda in the ordinary list, or null.
    const Stat *Find(std::uint16_t row) const;
};

// A creature's APP1 (the hero's), APL1 (a pool member's) or APC1 (an area's).
struct Appearance
{
    std::string label;
    Opt<std::uint16_t> type;
    Opt<std::uint8_t> gender;
    Opt<float> gore;
    Opt<std::uint8_t> decapitated;
    Opt<std::vector<std::uint8_t>> heraldryVariation;
    Opt<std::uint16_t> originalType;
    Opt<std::wstring> morphName; // only the hero's
};

struct ClassRank
{
    std::uint32_t classId = 0;
    std::uint32_t rank = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A PTE1.
struct TacticEntry
{
    std::uint32_t targetObject = kNoObject;
    std::uint32_t conditionObject = kNoObject;
    std::uint8_t enabled = 0;
    std::uint8_t target = 0;
    std::uint16_t condition = 0;
    std::int32_t command = 0;
    std::int32_t commandParam = 0;
    std::wstring commandItemTag;
    std::wstring commandItemResRef;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A PTT1.
struct Tactics
{
    std::uint8_t enabled = 0;
    std::vector<TacticEntry> entries;
    std::uint8_t dirty = 0;
    std::uint8_t presetType = 0;
    std::uint8_t presetIndex = 0;
    std::vector<std::uint8_t> presetList;
    std::vector<std::vector<TacticEntry>> custom; // each a PTL1's entries
};

// The hero's portrait camera (CRP1).
struct Portrait
{
    Opt<float> pitch;
    Opt<float> yaw;
    Opt<float> tint;
    Opt<float> expression;
    Opt<float> distance;
    Opt<float> positionH;
    Opt<float> positionV;
};

// A CRP1 (the hero), a CRL1 (a pool member of the party) or a CRE1 (an area's).
struct Creature
{
    enum class Kind : std::uint8_t
    {
        Area,
        Pool,
        Hero,
    };
    Kind kind = Kind::Area;
    Opt<std::wstring> templateResRef;
    Opt<std::wstring> tag;
    Opt<Vec3> position;
    Opt<Quat> orientation;
    Opt<std::uint32_t> id;
    Opt<std::uint8_t> active;
    Opt<std::int8_t> partyMembers; // CRP1, CRL1: whether it is in the party
    Opt<std::int8_t> nonPartyMembers;
    Opt<std::int32_t> partySubstate;
    Opt<std::uint8_t> partyLocked;
    Opt<std::uint8_t> partyFollow;
    // None where the save lacks it, which is the template's Combatant flag (+0xBD4) set, not a creature never
    // made active: the writer saves the stats of combatants only.
    std::optional<Stats> stats;
    Opt<std::vector<Item>> backpack;
    Opt<std::vector<Item>> plotItems;
    Opt<std::int32_t> money;
    Opt<std::uint32_t> activeSet; // the equipment's active weapon set
    Opt<std::vector<Item>> equipment;
    Opt<std::uint8_t> immortal;
    Opt<std::wstring> eventScript;
    Opt<std::int32_t> maxItems;
    Opt<std::vector<std::int32_t>> craftingRecipes;
    Opt<std::uint8_t> plot;
    Opt<std::int32_t> treasureGroup;
    Opt<std::uint32_t> nameStrref; // CRP1, CRL1
    Opt<std::wstring> nameText;    // the name's own text, the hero's typed name; null where the field is absent
    Opt<std::uint32_t> loopingAnimation;
    Opt<float> interactionRadius;
    Opt<std::int32_t> importance;
    std::optional<Appearance> appearance;
    std::optional<Portrait> portrait; // the hero's
    Opt<std::uint32_t> groupId;
    Opt<std::int32_t> teamId;
    Opt<std::uint8_t> stealth;
    Opt<std::uint8_t> plotGiver; // SavegameIsPlotGiver, 16455
    Opt<std::int8_t> canLevelUp;
    Opt<std::uint8_t> trackable;
    Opt<std::uint8_t> controllable;
    Opt<std::uint8_t> interactive;
    Opt<std::uint8_t> race;
    Opt<std::uint8_t> canChangeEquipment;
    Opt<std::vector<ClassRank>> classRanks;
    Opt<std::uint8_t> isGhost;
    Opt<std::int8_t> showAsAllyOnMap;
    Opt<std::uint8_t> isStatue;
    std::vector<std::int16_t> minimizedSkillHeaders;
    std::vector<std::int16_t> minimizedTalentHeaders;
    Opt<std::uint8_t> itemsScaled;
    Opt<float> heartbeatInterval;
    Opt<float> roamRadius;
    Opt<Vec3> roamCenter;
    Opt<std::wstring> poolName;
    Opt<std::uint8_t> poolAvailable;
    Opt<std::uint8_t> noPermDeath;
    Opt<std::int32_t> timeSinceDeath;
    Opt<std::uint32_t> bodybagId;
    Opt<std::uint32_t> rank;
    std::vector<Effect> effects;
    std::vector<Command> commands;
    std::optional<ActionQueue> actionQueue;
    Opt<std::uint8_t> tacticsHasTable;
    std::optional<Tactics> tactics;
    std::vector<std::uint8_t> playerMorph; // the hero's: an embedded morph file ("GFF V4.0 MORP"), raw
    Opt<std::wstring> playerSoundSet;
    Opt<std::wstring> defaultSoundSet;
    std::vector<Variable> variables;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// -- The objects left in an area

// A PLC1.
struct Placeable
{
    Opt<std::wstring> templateResRef;
    Opt<std::wstring> tag;
    Opt<Vec3> position;
    Opt<Quat> orientation;
    Opt<std::uint32_t> id;
    Opt<std::uint16_t> state;
    Opt<std::uint8_t> useable;
    Opt<std::uint8_t> active;
    Opt<std::uint8_t> immortal;
    Opt<std::wstring> eventScript;
    std::vector<Item> items;
    Opt<std::int32_t> maxItems;
    Opt<std::uint8_t> plot;
    Opt<std::int32_t> health;
    Opt<std::int32_t> maxHealth;
    Opt<std::int32_t> rank;
    Opt<std::int32_t> treasureGroup;
    Opt<std::uint32_t> nameStrref;
    Opt<std::wstring> nameText; // the name's own text, the hero's typed name; null where the field is absent
    Opt<std::uint16_t> lootableCreatureAppearanceType;
    Opt<std::int32_t> pickLock;
    Opt<std::uint8_t> trapDetected;
    Opt<std::int32_t> detectCheck;
    Opt<std::int32_t> disarmCheck;
    Opt<float> interactionRadius;
    Opt<std::int32_t> teamId;
    Opt<std::uint8_t> plotGiver; // 16455
    Opt<std::uint8_t> isBodybag;
    Opt<std::uint32_t> lootableObjectId;
    std::vector<Effect> effects;
    std::vector<Command> commands;
    std::optional<ActionQueue> actionQueue;
    std::vector<Variable> variables;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A SND1.
struct Sound
{
    Vec3 position;
    std::uint32_t id = kNoObject;
    std::wstring tag;
    std::wstring soundTag;
    std::int32_t active = 0;
    std::wstring name;
    Vec3 soundPosition;
    Quat soundOrientation;
    float volume = 0;
    float pitch = 0;
    std::int32_t fadeIn = 0;
    std::int32_t fadeOut = 0;
    float maxDistanceMult = 0;
    float coneInside = 0;
    float coneOutside = 0;
    float coneVolume = 0;
    std::int32_t priority = 0;
    std::int32_t occludable = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A trigger of any of its six templates: TRGR a plain trigger, TRTP a trap, TRAT a transition to another
// area, TRAR a reverb volume, TRAV a reverb volume with sounds, TRAM a music volume. The engine picks
// the class by `type` (SavegameAreaTriggerType, 16112), not by the struct's label, which is kept in
// `label` as a cross-check; the fields of the other kinds are none. The engine reads no ObjectID for a
// trigger (it gets a new one at each load), nor a trap's fields (detectable to lastDisarmed) nor a
// transition's loadScreen: they are written and ignored.
struct Trigger
{
    enum class Kind : std::uint8_t
    {
        Plain,       // 0 TRGR (also any value above 5)
        Transition,  // 1 TRAT
        Trap,        // 2 TRTP
        AudioReverb, // 3 TRAR
        AudioVolume, // 4 TRAV
        MusicVolume, // 5 TRAM
    };
    std::string label;
    Kind KindOf() const
    {
        const auto v = type.value_or(0);
        return v >= 0 && v <= 5 ? static_cast<Kind>(v) : Kind::Plain;
    }
    Opt<std::wstring> templateResRef;
    Opt<std::wstring> tag;
    Opt<Vec3> position;
    Opt<Quat> orientation;
    Opt<std::uint32_t> id;
    std::vector<Vec3> geometry; // each point as saved; the engine adds the saved position to it
    Opt<std::int32_t> type;
    Opt<std::uint8_t> active;
    Opt<std::wstring> eventScript;
    std::vector<Command> commands;
    std::optional<ActionQueue> actionQueue;
    std::vector<Variable> variables;
    // TRTP
    Opt<std::uint8_t> detectable;
    Opt<std::uint8_t> disarmable;
    Opt<std::int32_t> detectCheck;
    Opt<std::int32_t> disarmCheck;
    Opt<std::int32_t> lastDisarmed;
    // TRAT
    Opt<std::int32_t> loadScreen;
    // TRAR, TRAV
    Opt<std::wstring> reverbPreset;
    // TRAV
    std::vector<Sound> sounds;
    // TRAM
    Opt<std::int32_t> musicEnterState;
    Opt<std::int32_t> musicExitState;
    Opt<float> musicEnterDelay;
    Opt<float> musicExitDelay;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// An AOE1.
struct AreaOfEffect
{
    Opt<Vec3> position;
    Opt<Quat> orientation;
    Opt<std::uint32_t> id;
    Opt<std::wstring> eventScript;
    Opt<std::wstring> tag;
    Opt<std::int32_t> aoeId;
    Opt<std::uint8_t> shape;
    Opt<float> radius;
    Opt<float> width;
    Opt<float> length;
    Opt<std::int32_t> creator;
    Opt<std::uint32_t> duration;
    Opt<std::uint8_t> durationType;
    Opt<std::uint8_t> linked;
    Opt<std::int32_t> abilityId;
    Opt<std::int32_t> flags;
    Opt<std::uint8_t> stationary;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A WPT1, with its map note (16710 to 16714) when it has one.
struct Waypoint
{
    Opt<Vec3> position;
    Opt<Quat> orientation;
    Opt<std::uint32_t> id;
    Opt<std::uint8_t> active;
    Opt<std::wstring> tag;
    Opt<std::uint8_t> mapNote; // 0 and the rest of the note is not read by the engine
    Opt<std::uint8_t> mapNoteEnabled;
    Opt<std::wstring> mapNoteText;
    Opt<std::uint32_t> mapNoteType;
    Opt<std::uint32_t> mapNoteTextStrref;
    Opt<std::wstring> mapNoteTextOwn;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// An STO1.
struct Store
{
    Opt<std::wstring> templateResRef;
    Opt<Vec3> position;
    Opt<std::uint32_t> id;
    Opt<std::int32_t> markDown;
    Opt<std::int32_t> markUp;
    Opt<std::int32_t> gold;
    Opt<std::int32_t> maxBuyPrice;
    std::vector<std::uint32_t> willNotBuy;
    std::vector<std::uint32_t> willOnlyBuy;
    std::vector<Item> items;
    Opt<std::wstring> eventScript;
    Opt<std::wstring> tag;
    Opt<std::int32_t> maxItems;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// The explored cells of an area's map (16016): one bit a cell, bit y * width + x of the bytes, least
// significant bit first, in W * H / 8 + 1 bytes. The save does not hold the grid's width and height (they
// come from the area's map rectangle in 6 m cells), so SetSize takes them and checks the bytes agree.
struct AreaMap
{
    std::vector<std::uint8_t> bits;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // Sets the grid's size; false, leaving it unset, if the bytes are not W * H / 8 + 1 of them.
    bool SetSize(std::uint32_t w, std::uint32_t h);
    // Whether a cell is explored; false outside the grid or before SetSize.
    bool Explored(std::uint32_t x, std::uint32_t y) const;
    // The number of cells explored.
    std::size_t ExploredCount() const;
};

// An ARE1: an area kept in the save, by its tag.
struct Area
{
    std::wstring tag;
    std::uint32_t id = kNoObject; // SavegameAreaList's ObjectID; the engine ignores a struct whose id is -1 or absent
    // Whether the engine takes this area's struct (an ObjectID that is not -1) rather than the .are's alone.
    bool Used() const
    {
        return id != kNoObject;
    }
    std::vector<Placeable> placeables;
    std::vector<Creature> creatures;
    std::vector<Trigger> triggers;
    std::vector<AreaOfEffect> aoes;
    std::vector<Waypoint> waypoints;
    AreaMap map;
    std::vector<Store> stores;
    std::vector<std::int32_t> roomsViewed;
    std::vector<Sound> sounds;
    std::int32_t minCreatureImportance = 0;
    std::vector<Effect> effects;
    std::vector<Variable> variables;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// -- The campaign

struct Group
{
    std::uint32_t id = 0;
    std::vector<std::uint32_t> hostiles;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A QEST.
struct Quest
{
    std::uint32_t titleStrref = kNoStrref;
    std::wstring titleText;
    std::uint32_t textStrref = kNoStrref;
    std::wstring text;
    std::wstring parentPlot;
    std::wstring resRef;
    std::wstring areaTag;
    std::uint8_t completed = 0;
    std::wstring group;
    std::uint8_t updated = 0;
    std::wstring offerId;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

struct PlotDestination
{
    std::wstring tag;
    std::vector<std::wstring> guids;  // 16513 to 16515
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

struct ConversationLine
{
    std::uint32_t speaker = 0;
    std::uint32_t text = 0;
    std::uint8_t reply = 0;
};

struct JournalGroup
{
    std::wstring resRef;
    std::uint8_t openInCurrent = 0;
    std::uint8_t openInCompleted = 0;
    std::uint16_t priority = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A JRNL.
struct Journal
{
    std::vector<Quest> active;
    std::vector<Quest> completed;
    std::vector<Quest> orphans;
    std::wstring activeResRef; // SavegameJournalResRef, 16510: the quest marked active, empty for none
    std::uint32_t storyTextStrref = kNoStrref;
    std::wstring storyText;
    std::vector<PlotDestination> plotDestinations;
    std::vector<std::vector<ConversationLine>> conversations;
    std::vector<JournalGroup> groups;
};

struct WorldMapPin
{
    std::int32_t id = 0;
    std::wstring tag;
    std::int8_t state = 0;
    std::int8_t recentlyActivated = 0;
    std::int8_t activatedPreviously = 0;
    std::int8_t lastState = 0;
    std::uint32_t nameStrref = kNoStrref;
    std::wstring nameText;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

struct WorldMapMap
{
    std::int32_t id = 0;
    std::wstring tag;
    std::wstring playerLocation;
    std::vector<WorldMapPin> pins;
    std::vector<std::pair<float, float>> travelPathBefore;
    std::vector<std::pair<float, float>> travelPathAfter;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

struct WorldMap
{
    std::wstring primaryMap;
    std::wstring secondaryMap;
    std::vector<WorldMapMap> maps;
    std::uint32_t guiStatus = 0;
    std::uint32_t lastPinClicked = 0;
};

struct PlotAction
{
    std::uint16_t id = 0;
    std::uint16_t state = 0;
    Opt<std::int32_t> count;
    std::int8_t updated = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

struct PlotActions
{
    std::uint16_t enabled = 0;
    std::uint16_t currentSet = 0;
    std::vector<PlotAction> list;
};

struct AmbientDialog
{
    std::uint32_t owner = 0;
    std::uint32_t speaker = 0;
    std::wstring resRef;
    std::uint32_t line = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A MODU.
struct Campaign
{
    std::uint32_t id = kNoObject;
    std::wstring resource; // the campaign's .cif
    std::vector<Group> groups;
    std::int32_t creatureTimeBeforeDecay = 60000; // the engine's default, ms a corpse stays
    Journal journal;
    std::vector<AmbientDialog> ambientDialogs;
    WorldMap worldMap;
    PlotActions plotActions;
    std::vector<Variable> variables;
    // Where the party is at the save (not only where the campaign began, dev/PARAGON_SAVES.md).
    std::wstring entryAreaList;
    std::wstring entryArea;
    Vec3 entryPosition;
    Vec3 entryOrientation{1, 0, 0}; // a direction (three floats), not a quaternion; the engine's default
};

// -- The party

// A PLEN: a plot's GUID (32 hex digits) and its four words of flags.
struct PlotEntry
{
    std::wstring guid;
    std::array<std::uint32_t, 4> flags{};
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

struct Approval
{
    std::uint32_t id = 0;
    std::int32_t level = 0;
    std::uint32_t descStrref = kNoStrref;
    std::wstring descText;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A PTIT.
struct StoredItem
{
    std::uint32_t item = 0;
    std::uint32_t owner = 0;
    std::uint32_t slot = 0;
    std::uint32_t weaponSet = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A PRTY.
struct Party
{
    // The party is fresh when a save loads, so a field the save lacks is the party's default. The lists
    // and flags the loader reads only when present are none here when absent. A pool creature whose id the
    // world has already is skipped by the engine.
    std::uint32_t id = 0;            // read to make the party; 0 where absent
    std::vector<Creature> pool;      // CRL1, the creatures the party has had, in the order they joined
    Opt<std::vector<Item>> backpack; // place: x and y of the item's Position; none of slot or set
    Opt<std::vector<Item>> plotItems;
    Opt<std::int32_t> money;
    Opt<std::int32_t> maxItems;
    Opt<std::vector<std::int32_t>> craftingRecipes;
    std::uint32_t selectedCharacter = kNoObject; // applied after the party is placed, unless none
    std::uint32_t pickerGuiStatus = 2;           // the engine's default
    std::vector<Approval> approvals;
    Opt<std::uint32_t> leader; // applied only if that creature exists
    std::vector<StoredItem> itemStorage;
    std::vector<std::uint32_t> newItems;
    Opt<std::uint8_t> autoLevelDefault; // the engine takes 1 as true
    Opt<std::uint8_t> quickbarLocked;
    Opt<std::uint8_t> holdPositions;
    Opt<std::uint8_t> runInDriveMode;
    Opt<std::uint8_t> backpackSort;
    Opt<float> qbarExpansion;
    std::vector<PlotEntry> plots; // the plot manager's: every plot the game knows
    std::vector<std::wstring> unreadCodex;
    std::vector<std::uint32_t> seenLines;
    const PlotEntry *FindPlot(std::wstring_view guid) const;
};

// -- The game's state

// An SCE1: a script event's payload.
struct ScriptEvent
{
    std::int32_t type = 0;
    std::uint32_t creator = kNoObject;
    std::uint32_t target = kNoObject;
    DataArrays data; // raw
    std::wstring scriptName;
    std::vector<std::wstring> resources;
};

// An entry of the event queue (16630): an EBAS, or an ESCR with the script event it raises.
struct QueuedEvent
{
    // What the engine does with an entry by its SavegameEventID (the handler id): 10 and 24 are script
    // events (with an SCE1), 6, 7, 11, 12, 13, 16 and 27 are plain events, any other id is dropped.
    enum class Kind : std::uint8_t
    {
        Dropped,
        Plain,
        Script,
    };
    Kind KindOf() const
    {
        switch (id)
        {
        case 10:
        case 24:
            return Kind::Script;
        case 6:
        case 7:
        case 11:
        case 12:
        case 13:
        case 16:
        case 27:
            return Kind::Plain;
        default:
            return Kind::Dropped;
        }
    }
    std::string label; // EBAS or ESCR; the engine goes by the id
    std::uint32_t day = 0;
    std::uint32_t time = 0;
    std::uint32_t caller = kNoObject;
    std::uint32_t target = kNoObject;
    std::uint32_t id = 0;
    std::optional<ScriptEvent> script;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

// A GMST.
struct GameState
{
    std::int32_t effectStartingId = 1; // the effect counter; the engine's is 1 where the field is absent
    std::vector<QueuedEvent> eventQueue;
    std::int32_t worldDay = 0;
    std::int32_t worldTimeMs = 0;
};

// A VERS. The engine's load path reads none of it (the build number is written, never checked), so a
// save of another build loads.
struct Version
{
    std::uint8_t cheatUsed = 0;
    std::int32_t buildNumber = 0;
    std::int32_t saveVersion = 0;
};

// One of the three pools the object ids are drawn from (a WDBE): group 0 ids from 1, group 1 from
// 0x40000000, group 2 from 0x80000000; the last id given out. Taken only if within the pool's range.
struct IdPool
{
    std::uint32_t group = 0xFFFFFFFF;
    std::uint32_t lastId = 0;
    std::uint32_t source = kNoSource; // where Read found it, for Write
};

struct Save
{
    Campaign campaign;
    std::vector<Area> areas;
    std::optional<Creature> hero; // PLYR's CRP1
    float mapZoom = 0;
    std::uint8_t mapLegend = 0;
    std::int32_t timePlayedSeconds = 0;
    Party party;
    Version version;
    GameState state;
    std::vector<IdPool> idPools; // SavegameWorldDatabase, 16500

    // The area of that tag, or null.
    // The area of that tag, as the engine's area list finds it: the case of the tag does not matter.
    const Area *FindArea(std::wstring_view tag) const;
    // The creature of that id: the hero, a pool member, or one an area keeps; null if none.
    const Creature *FindCreature(std::uint32_t id) const;
    // The object ids that more than one object of the save holds, each once, in order: the hero, the pool and
    // the areas' creatures, placeables, waypoints, sounds and AOEs, the areas themselves, and every item in a
    // backpack, an equipment list, a container or the party's. The engine keeps one object to an id, so a
    // creature copied with its items, or an item given the id of another, is two objects of one id, and the
    // second does not stand in the world (seen 2026-10-21: a copied genlock with its items' ids unchanged, and
    // the area list's teardown freed an item twice and crashed). A trigger's and a store's id is not read by
    // the engine and not counted; -1 (kNoObject) is no id.
    std::vector<std::uint32_t> DuplicateIds() const;
};

// The save in a file already parsed, or nothing if it is not a GFF4 "SAVE" with a top struct.
std::optional<Save> Read(const gff4::File &das);
// The save in a .das's bytes.
std::optional<Save> Read(std::span<const std::byte> bytes);

// A member Write could not put in the file, and why, with how many times it came to that.
struct Unwritten
{
    std::string structLabel; // the template of the struct it was for, such as "CRE1"
    std::uint32_t field = 0; // the field's label (dev/gff4_fields.csv)
    std::string reason;
    std::size_t count = 0;
};

// What Write made: the new file's bytes, and what it could not write.
struct Written
{
    std::vector<std::byte> bytes;
    std::vector<Unwritten> unwritten;
    // The object ids the Save holds more than once (Save::DuplicateIds); written all the same.
    std::vector<std::uint32_t> duplicateIds;
};

// The .das's bytes with a Save's changes written into them. `original` is the file `save` was read from
// (Read, then edited): the result is a copy of it in which what differs is set, so everything Read does not
// model (the add-ins list, the story so far, the junk words of a vector list) goes through untouched, and a
// Save read and written back unchanged gives the same bytes. Nothing in the file moves: a list that grows,
// shrinks or is reordered is written afresh at the end and its field pointed at it, and the old one stays
// as dead bytes.
//
// Lists. An element of a list that Read made is the struct it was read from, whatever its place in the list
// now (its `source`): so one taken out of the middle, moved, or edited keeps what the file holds of it that
// the Save does not. An element a caller made, or one copied from another (the same source twice, or from
// another list), is a new struct built from its members alone. A list shorter or longer than the file's
// has its elements removed or added so. A struct that has to be new takes the template of the list's field
// or, for a generic list, of the kind of the element (a variable's type, a stat's kind, a trigger's type
// or label, an event's label). Its object id is -1 until a member sets it (the engine ignores such an
// object, so a caller gives a new one its id).
//
// Members. An Opt member that is none is left as the file has it; it never removes a field. A member
// the struct's template has no field for, or whose field is another type, cannot be written (a save's
// templates are fixed; adding a field would move every struct of the template), and a plain member
// is left as the file has it without a word, while an Opt that is set or a list that is not empty is told in
// `unwritten`. A child struct the file lacks (a null reference) is made when its member is set, or, for the
// campaign's, the party's and the game state's own structs, when the Save has something in it. A child
// struct is never taken out.
//
// A campaign variable the save adds is loaded by the game only if its name and type are declared (by the
// campaign's CIF, the module's IFO or the template: the loader creates no names, dev/SAVES.md), so one added
// under a new name is written and then ignored; change a declared one's value. An object's id is one to an object
// (DuplicateIds), and the caller's.
//
// Nothing is checked against the game's rules: the values are the caller's, and the id pools, a creature's
// id in the party, an area's id and the like are not made to agree. Nothing for a file that is not a SAVE.
std::optional<Written> Write(std::span<const std::byte> original, const Save &save);

} // namespace dao::core::savegame
