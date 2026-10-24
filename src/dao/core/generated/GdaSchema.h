// Written by tools/gen_gda_schema.py from dev/gda_schema.csv (tools/gda_schema, the GOG
// install's worksheets scanned) and dev/gda_references.csv; do not edit.
//
// The game's merged data tables, as the scripts' TABLE_* and m2da_base name them: each one's
// columns and what each holds, and a typed row for the tables callers read by column. A mod may
// add tables and columns: read those through dao/core/M2da.h as ever.
//
//   const auto *abilities = dao::core::tables::Find(1);
//   if (const auto row = dao::core::tables::AbilitiesTalents::Of(*merged, 10000))
//       const std::optional<std::int32_t> name = row->NameStrRef();
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/TableRow.h"
#include "dao/generated/ScriptConstants.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace dao::core::tables
{

// What a column's values are.
enum class Kind : std::uint8_t
{
    Plain,
    StrRef,    // a talk table's string id
    Resource,  // a file's name, without its extension
    Worksheet, // a worksheet's label
    Row,       // the ID of a row of another worksheet, Column::target
};

struct Column
{
    std::string_view name; // as the game's files spell it: its hash's name
    std::uint32_t hash = 0;
    gff4::Type type = gff4::Type::S32;
    Kind kind = Kind::Plain;
    std::string_view extension; // a Resource's, without its dot
    std::int64_t target = -1;   // a Row's worksheet
};

struct Worksheet
{
    std::int64_t id = 0;
    std::string_view label;  // m2da_base's
    std::string_view prefix; // its files' names start with it
    std::string_view name;   // the scripts' TABLE_* constant's, or Sheet's
    std::span<const Column> columns;
};

// Returns every worksheet the game merges, by ID.
std::span<const Worksheet> Worksheets();
// Returns a worksheet by its ID; null for one the scan did not see (a mod's).
const Worksheet *Find(std::int64_t id);
// Returns a worksheet's column by its name, in any case; null for none.
const Column *ColumnOf(const Worksheet &worksheet, std::string_view name);

// The worksheets with no TABLE_* constant of the scripts', by m2da_base's label: with
// dao::script::Table, one name for each ID.
enum class Sheet : std::int64_t
{
    Projectiles4 = 4,                       // Projectiles
    Portraits = 7,                          // Portraits
    VisualEffects = 8,                      // Visual Effects
    MaceVariation = 11,                     // mace_variation
    GreatswordVariation = 12,               // greatsword_variation
    GlovesHeavyVariation = 13,              // gloves_heavy_variation
    GlovesMediumVariation = 14,             // gloves_medium_variation
    GlovesLightVariation = 15,              // gloves_light_variation
    GlovesMassiveVariation = 16,            // gloves_massive_variation
    BootsMassiveVariation = 17,             // boots_massive_variation
    BootsHeavyVariation = 18,               // boots_heavy_variation
    BootsLightVariation = 19,               // boots_light_variation
    BootsMediumVariation = 20,              // boots_medium_variation
    HelmetMageVariation = 21,               // helmet_mage_variation
    HelmetMassiveVariation = 22,            // helmet_massive_variation
    HelmetHeavyVariation = 23,              // helmet_heavy_variation
    HelmetMediumVariation = 24,             // helmet_medium_variation
    HelmetLightVariation = 25,              // helmet_light_variation
    ArmorMassiveVariation = 26,             // armor_massive_variation
    ArmorMediumVariation = 27,              // armor_medium_variation
    ArmorLightVariation = 28,               // armor_light_variation
    ArmorHeavyVariation = 29,               // armor_heavy_variation
    LongswordVariation = 30,                // longsword_variation
    StaffVariation = 31,                    // staff_variation
    WandVariation = 32,                     // wand_variation
    DaggerVariation = 33,                   // dagger_variation
    WaraxeVariation = 34,                   // waraxe_variation
    BattleaxeVariation = 35,                // battleaxe_variation
    MaulVariation = 36,                     // maul_variation
    LroundShieldVariation = 37,             // lround_shield_variation
    SroundShieldVariation = 38,             // sround_shield_variation
    KiteShieldVariation = 39,               // kite_shield_variation
    TowerShieldVariation = 40,              // tower_shield_variation
    CrossbowVariation = 41,                 // crossbow_variation
    ShortbowVariation = 42,                 // shortbow_variation
    LongbowVariation = 43,                  // longbow_variation
    BoltVariation = 44,                     // bolt_variation
    ArrowVariation = 45,                    // arrow_variation
    ClothingVariation = 46,                 // clothing_variation
    ShaleBodyVariation = 47,                // shale_body_variation
    ShaleChestVariation = 48,               // shale_chest_variation
    ShaleRightArmVariation = 49,            // shale_right_arm_variation
    ShaleLeftArmVariation = 50,             // shale_left_arm_variation
    DogCollarVariation = 51,                // dog_collar_variation
    DogWarpaintVariation = 52,              // dog_warpaint_variation
    GolemHeads = 53,                        // golem_heads
    PdemonHeads = 54,                       // pdemon_heads
    SkeletonHeads = 55,                     // skeleton_heads
    CorpseHeads = 56,                       // corpse_heads
    ArcaneHorrorHeads = 57,                 // arcane_horror_heads
    RevenantHeads = 58,                     // revenant_heads
    AbominationHeads = 59,                  // abomination_heads
    HighDragonHeads = 60,                   // high_dragon_heads
    LadyHeads = 61,                         // lady_heads
    SuccubusHeads = 62,                     // Succubus_heads
    ShriekHeads = 63,                       // shriek_heads
    WerewolfHeads = 64,                     // werewolf_heads
    GenlockHeads = 65,                      // genlock_heads
    HurlockHeads = 66,                      // hurlock_heads
    OgreHeads = 67,                         // ogre_heads
    SylvanHeads = 68,                       // sylvan_heads
    HumanBeard = 69,                        // human_beard
    Eyes = 70,                              // eyes
    DwarfHair = 71,                         // dwarf_hair
    QunariHair = 72,                        // qunari_hair
    ElfHair = 73,                           // elf_hair
    HumanHair = 74,                         // human_hair
    ElfHeads = 75,                          // elf_heads
    ChildHeads = 76,                        // child_heads
    QunariHeads = 77,                       // qunari_heads
    DwarfHeads = 78,                        // dwarf_heads
    HumanHeads = 79,                        // human_heads
    NakedVariations = 80,                   // Naked Variations
    Tattoos81 = 81,                         // Tattoos
    BodyTintMasks = 82,                     // Body Tint Masks
    EyeTints = 83,                          // Eye Tints
    HairTints = 84,                         // Hair Tints
    SkinTints = 85,                         // Skin Tints
    Tattoos86 = 86,                         // Tattoos
    TintsOverrides = 87,                    // Tints Overrides
    BodyTints = 88,                         // Body Tints
    BodyParts = 90,                         // Body Parts
    WeaponParts = 91,                       // Weapon Parts
    Genders = 92,                           // Genders
    AnimationEvents = 95,                   // Animation Events
    Properties = 98,                        // Properties
    Heraldry = 99,                          // Heraldry
    PlaceableActions = 100,                 // Placeable Actions
    SyncAbilityAnimations = 101,            // Sync Ability Animations
    SyncDeathAnimations = 102,              // Sync Death Animations
    UnsyncDeathAnimations = 103,            // Unsync Death Animations
    NoSyncCombatAnimations = 104,           // No Sync Combat Animations
    Globals = 105,                          // globals
    WaypointTypes = 106,                    // Waypoint Types
    ItemStatDefines = 109,                  // Item Stat Defines
    GUITypes = 111,                         // GUI Types
    SurvivalSkil = 112,                     // Survival Skil
    PartyPickerOrdering = 113,              // Party picker ordering
    JobBoards = 115,                        // Job boards
    JobBoardPlots = 116,                    // Job board plots
    PassiveAbilities = 117,                 // Passive Abilities
    SyncLargeCreatures1 = 118,              // Sync Large Creatures 1
    SyncLargeCreatures2 = 119,              // Sync Large Creatures 2
    SyncLargeCreatures3 = 120,              // Sync Large Creatures 3
    SyncLargeCreatures4 = 121,              // Sync Large Creatures 4
    SyncLargeCreatures5 = 122,              // Sync Large Creatures 5
    SyncPlaceableAmbientAnimations = 123,   // Sync Placeable Ambient Animations
    SyncCreatureAmbientAnimations = 124,    // Sync Creature Ambient Animations
    MeleeRing1 = 125,                       // Melee Ring 1
    MeleeRing2 = 126,                       // Melee Ring 2
    MeleeRing3 = 127,                       // Melee Ring 3
    MeleeRing4 = 128,                       // Melee Ring 4
    MeleeRing5 = 129,                       // Melee Ring 5
    MeleeRing6 = 130,                       // Melee Ring 6
    MeleeRing7 = 131,                       // Melee Ring 7
    MeleeRing8 = 132,                       // Melee Ring 8
    MeleeRing9 = 133,                       // Melee Ring 9
    DialogTags = 143,                       // Dialog Tags
    CraftingRecipesTypes = 145,             // Crafting Recipes Types
    SoundWeaponActions = 146,               // Sound Weapon Actions
    SoundBodyfallFoley = 147,               // Sound Bodyfall Foley
    SoundBodyfallCreatureSize = 148,        // Sound Bodyfall Creature Size
    SoundScuffing = 149,                    // Sound Scuffing
    SoundFootsteps = 150,                   // Sound Footsteps
    SoundFoley = 151,                       // Sound Foley
    GUISounds = 152,                        // GUI sounds
    SoundItemsMaterials = 153,              // Sound Items Materials
    SoundWeaponHits = 154,                  // Sound Weapon Hits
    SoundMovementVegetation = 155,          // Sound Movement Vegetation
    SoundStaticGeometryMaterialTypes = 156, // Sound Static Geometry Material Types
    SoundProjectsList = 157,                // Sound projects list
    PartyPickerInformation = 158,           // Party Picker Information
    MaterialRules = 159,                    // Material Rules
    EquipmentLayout = 161,                  // Equipment Layout
    DateTimeLocalizationInformation = 162,  // Date/Time Localization Information
    DialogIcons = 202,                      // Dialog "Icons"
    ConversationsDepthOfField203 = 203,     // Conversations Depth Of Field
    ConversationsDepthOfField204 = 204,     // Conversations Depth Of Field
    DeathHints = 205,                       // Death Hints
    LoadHints = 206,                        // Load Hints
    Popups = 207,                           // Popups
    BackgroundDescriptions = 208,           // Background Descriptions
    CharacterBackgroundIcons = 209,         // character background icons
    CharacterBackgroundNames = 210,         // character background names
    PlayerSoundSets = 212,                  // player sound sets
    RaceIcons = 213,                        // Race Icons
    MapPinImages = 215,                     // Map Pin Images
    CLIArmyDSFortRoof = 223,                // CLI_Army_DS_FortRoof
    PlotActionSets = 224,                   // PlotActionSets
    AreaData = 225,                         // Area Data
    AreaScaleData = 227,                    // Area Scale Data
    CLIArmyDSRedcliffe = 228,               // CLI_Army_DS_Redcliffe
    LookAtSettings = 229,                   // Look At Settings
    ALAlistair = 256,                       // ALAlistair
    ALDog = 257,                            // ALDog
    ALLeliana = 258,                        // ALLeliana
    ALLoghain = 259,                        // ALLoghain
    ALMorrigan = 260,                       // ALMorrigan
    ALOghren = 261,                         // ALOghren
    ALSten = 262,                           // ALSten
    ALWynne = 263,                          // ALWynne
    ALZevran = 264,                         // ALZevran
    ALIrving = 265,                         // ALIrving
    ALArcherDefault = 266,                  // ALArcher_Default
    InputMappings = 267,                    // InputMappings
    HeroicStats = 269,                      // HeroicStats
    HeroicPartyStats = 270,                 // HeroicPartyStats
    DeathHintsRandom = 272,                 // DeathHintsRandom
    SoundSettings = 273,                    // sound_settings
    InternalSoundsettings = 274,            // internal_soundsettings
    HighLeveLoadHints = 275,                // HighLeveLoadHints
    MidLeveLoadHints = 276,                 // MidLeveLoadHints
    LowLevelLoadHints = 277,                // LowLevelLoadHints
    ARLCoprsesArmy = 278,                   // ARL_CoprsesArmy
    SoundsetProbablityModifier = 281,       // Soundset probablity modifier
    OriginAutoscreenshotLabels = 282,       // Origin autoscreenshot labels
    MainCampaignAutoscreenshotLabels = 283, // Main campaign autoscreenshot labels
    VLowLevelLoadHints = 284,               // VLowLevelLoadHints
    ALMouse = 285,                          // ALMouse
    ALJowan = 286,                          // ALJowan
    TrainingTutorials = 287,                // TrainingTutorials
    PostGameSave = 288,                     // PostGameSave
    FollowerAniHackTags = 289,              // follower_ani_hack_tags
    ToolsetGroups = 290,                    // toolset_groups
    ALShale = 291,                          // ALShale
    Keymap = 292,                           // keymap
    ChargenPreload = 293,                   // chargen_preload
    AlwarriorDefault = 1002,                // alwarrior_default
    AlwizardDefault = 1003,                 // alwizard_default
    AlrogueDefault = 1004,                  // alrogue_default
    PlotaudioBase = 1005,                   // plotaudio_base
    EpilogueTextPositions = 1007,           // epilogue_text_positions
    Poses = 1009,                           // poses
    PoseTransitions = 1010,                 // pose_transitions
    ToolsetHenchmen = 1011,                 // toolset_henchmen
    TargetWpsWow = 1012,                    // target_wps_wow
    TargetWpsDenerim = 1013,                // target_wps_denerim
    TargetWpsUnderground = 1014,            // target_wps_underground
    TargetWpsClimaxDenerim = 1015,          // target_wps_climax_denerim
    TargetWpsFade = 1016,                   // target_wps_fade
    HealerTacticPreset = 2004,              // HealerTacticPreset
    NukerTacticPreset = 2005,               // NukerTacticPreset
    BufferTacticPreset = 2006,              // BufferTacticPreset
    CrowdCTacticPreset = 2007,              // CrowdCTacticPreset
    DebufferTacticPreset = 2008,            // DebufferTacticPreset
    WardogTacticsPreset = 2010,             // WardogTacticsPreset
    AIPDefault = 10011,                     // AIP_Default
    AIPBearDefault = 10012,                 // AIP_Bear_Default
    AIPBearGreat = 10013,                   // AIP_Bear_Great
    AIPBearBereskan = 10014,                // AIP_Bear_Bereskan
    AIPBronto = 10015,                      // AIP_Bronto
    AIPGolemStone = 10016,                  // AIP_Golem_Stone
    AIPGolemSteel = 10017,                  // AIP_Golem_Steel
    AIPGolemCairdin = 10018,                // AIP_Golem_Cairdin
    AIPCorpseDevouring = 10019,             // AIP_Corpse_Devouring
    AIPCorpseShambling = 10020,             // AIP_Corpse_Shambling
    AIPCorpseEnraged = 10021,               // AIP_Corpse_Enraged
    AIPWildSylvan = 10022,                  // AIP_Wild_Sylvan
    AIPGrandOak = 10023,                    // AIP_Grand_Oak
    AIPOgreDefault = 10024,                 // AIP_Ogre_Default
    AIPRevenant = 10025,                    // AIP_Revenant
    AIPWerewolf = 10026,                    // AIP_Werewolf
    AIPWerewolfLeader = 10027,              // AIP_Werewolf_Leader
    AIPShriekDefault = 10028,               // AIP_Shriek_Default
    AIPShriekAlpha = 10029,                 // AIP_Shriek_Alpha
    AIPDSTwoHanded = 10030,                 // AIP_DS_Two_Handed
    AIPWolfDefault = 10031,                 // AIP_Wolf_Default
    AIPWolfPackLeader = 10032,              // AIP_Wolf_Pack_Leader
    AIPDeepstalkerDefault = 10033,          // AIP_Deepstalker_Default
    AIPDeepstalkerLeader = 10034,           // AIP_Deepstalker_Leader
    AIPSpiderGiant = 10035,                 // AIP_Spider_Giant
    AIPSpiderPoisonous = 10036,             // AIP_Spider_Poisonous
    AIPSpiderCorrupted = 10037,             // AIP_Spider_Corrupted
    AIPDragonling = 10038,                  // AIP_Dragonling
    AIPDragon = 10039,                      // AIP_Dragon
    AIPDrake = 10040,                       // AIP_Drake
    AIPHighDragon1 = 10041,                 // AIP_High_Dragon1
    AIPWitherfang = 10042,                  // AIP_Witherfang
    AIPDogDefault = 10043,                  // AIP_Dog_Default
    AIPDogAlpha = 10045,                    // AIP_Dog_Alpha
    AIPSkeleton = 10046,                    // AIP_Skeleton
    AIPSkeletonFanged = 10047,              // AIP_Skeleton_Fanged
    AIPSkeletonEnraged = 10048,             // AIP_Skeleton_Enraged
    AIPSuccubus = 10049,                    // AIP_Succubus
    AIPShade = 10050,                       // AIP_Shade
    AIPAshWraith = 10051,                   // AIP_Ash_Wraith
    AIPRageDemon = 10052,                   // AIP_Rage_Demon
    AIPAbominationSloth = 10053,            // AIP_Abomination_Sloth
    AIPAbominationRage = 10054,             // AIP_Abomination_Rage
    AIPAbominationHunger = 10055,           // AIP_Abomination_Hunger
    AIPAbominationDesire = 10056,           // AIP_Abomination_Desire
    AIPArcaneHorrorBalanced = 10057,        // AIP_Arcane_Horror_Balanced
    AIPArcaneHorrorNuker = 10058,           // AIP_Arcane_Horror_Nuker
    AIPArcaneHorrorSupport = 10059,         // AIP_Arcane_Horror_Support
    AIPPrideDemon = 10060,                  // AIP_Pride_Demon
    AIPBroodmother = 10061,                 // AIP_Broodmother
    AIPBroodmotherTentacle = 10062,         // AIP_Broodmother_Tentacle
    AIPArchdemon = 10064,                   // AIP_Archdemon
    AIPDSArcher = 10065,                    // AIP_DS_Archer
    AIPDSWPNSHLDHurlock = 10066,            // AIP_DS_WPN_SHLD_Hurlock
    AIPDSWPNSHLDGenlock = 10067,            // AIP_DS_WPN_SHLD_Genlock
    AIPDSDualWield = 10068,                 // AIP_DS_Dual_Wield
    AIPDSAlphaArcher = 10069,               // AIP_DS_Alpha_Archer
    AIPDSAlphaTwoHanded = 10070,            // AIP_DS_Alpha_Two_Handed
    AIPDSAlphaDualWield = 10071,            // AIP_DS_Alpha_Dual_Wield
    AIPDSAlphaWPNSHLD = 10072,              // AIP_DS_Alpha_WPN_SHLD
    AIPDSEmissaryOffensive = 10073,         // AIP_DS_Emissary_Offensive
    AIPDSEmissaryDefensive = 10074,         // AIP_DS_Emissary_Defensive
    AIPDSEmissaryBalanced = 10075,          // AIP_DS_Emissary_Balanced
    AIPArcher = 10076,                      // AIP_Archer
    AIPArcherHuman = 10077,                 // AIP_Archer_Human
    AIPArcherElf = 10078,                   // AIP_Archer_Elf
    AIPArcherDwarf = 10079,                 // AIP_Archer_Dwarf
    AIPWeaponShield = 10080,                // AIP_Weapon_Shield
    AIPWeaponShieldHuman = 10081,           // AIP_Weapon_Shield_Human
    AIPWeaponShieldDwarf = 10082,           // AIP_Weapon_Shield_Dwarf
    AIPTwoHanded = 10083,                   // AIP_Two_Handed
    AIPTwoHandedHuman = 10084,              // AIP_Two_Handed_Human
    AIPDualWeapons = 10085,                 // AIP_Dual_Weapons
    AIPDualWeaponsHuman = 10086,            // AIP_Dual_Weapons_Human
    AIPDualWeaponsElf = 10087,              // AIP_Dual_Weapons_Elf
    AIPRogue = 10088,                       // AIP_Rogue
    AIPCasterNuker1 = 10089,                // AIP_Caster_Nuker_1
    AIPCasterNuker2 = 10090,                // AIP_Caster_Nuker_2
    AIPCasterNukerBlood = 10091,            // AIP_Caster_Nuker_Blood
    AIPCasterBalanced1 = 10092,             // AIP_Caster_Balanced_1
    AIPCasterBalanced2 = 10093,             // AIP_Caster_Balanced_2
    AIPCasterBalancedBlood = 10094,         // AIP_Caster_Balanced_Blood
    AIPCasterHealer = 10095,                // AIP_Caster_Healer
    AIPCasterBuffer = 10096,                // AIP_Caster_Buffer
    AIPAdvancedArcaneWarrior = 10097,       // AIP_Advanced_Arcane_Warrior
    AIPAdvancedAssassin = 10098,            // AIP_Advanced_Assassin
    AIPAdvancedBerserker = 10099,           // AIP_Advanced_Berserker
    AIPAdvancedChampion = 10100,            // AIP_Advanced_Champion
    AIPAdvancedDuelist = 10101,             // AIP_Advanced_Duelist
    AIPAdvancedReaver = 10102,              // AIP_Advanced_Reaver
    AIPAdvancedShapeshifter = 10103,        // AIP_Advanced_Shapeshifter
    AIPAdvancedTemplar = 10105,             // AIP_Advanced_Templar
    AIPFollowerAlistair = 10106,            // AIP_Follower_Alistair
    AIPFollowerDog = 10107,                 // AIP_Follower_Dog
    AIPFollowerMorrigan = 10108,            // AIP_Follower_Morrigan
    AIPFollowerWynne = 10109,               // AIP_Follower_Wynne
    AIPFollowerLeliana = 10110,             // AIP_Follower_Leliana
    AIPFollowerSten = 10111,                // AIP_Follower_Sten
    AIPFollowerOghren = 10113,              // AIP_Follower_Oghren
    AIPFollowerZevran = 10114,              // AIP_Follower_Zevran
    AIPFollowerLoghain = 10115,             // AIP_Follower_Loghain
    AIPFollowerGenericMelee = 10116,        // AIP_Follower_Generic_Melee
    AIPFollowerGenericArcher = 10117,       // AIP_Follower_Generic_Archer
    AIPFollowerGenericMage = 10118,         // AIP_Follower_Generic_Mage
    AIPBranka = 10119,                      // AIP_Branka
    AIPCaridin = 10120,                     // AIP_Caridin
    AIPDeepstalkerSpitter = 10121,          // AIP_Deepstalker_Spitter
    AIPDeepstalkerMatriarch = 10122,        // AIP_Deepstalker_Matriarch
    AIPSkeletonArcher10123 = 10123,         // AIP_Skeleton_Archer
    AIPDSEmissaryOffensive2 = 10124,        // AIP_DS_Emissary_Offensive2
    AIPDSEmissaryOffensive3 = 10125,        // AIP_DS_Emissary_Offensive3
    AIPWisp = 10126,                        // AIP_Wisp
    AIPFollowerGenericRogue = 10127,        // AIP_Follower_Generic_Rogue
    AIPFollowerMouse = 10128,               // AIP_Follower_Mouse
    AIPSkeletonArcher10129 = 10129,         // AIP_Skeleton_Archer
    AIPCoward = 10130,                      // AIP_Coward
    AIPBard = 10131,                        // AIP_Bard
    AIPCasterNukerClimax = 10132,           // AIP_Caster_Nuker_Climax
    AIPCasterNukerPyro = 10133,             // AIP_Caster_Nuker_Pyro
    AIPDSEmissaryElite = 10134,             // AIP_DS_Emissary_Elite
    AIPSpiderGiantLessCC = 10135,           // AIP_Spider_Giant_Less_CC
    AIPDSConjurer = 10136,                  // AIP_DS_Conjurer
    AIPDSAlpha2HArcher = 10137,             // AIP_DS_Alpha_2H_Archer
    AIPWerewolfShadow = 10138,              // AIP_Werewolf_Shadow
    AIPSuccubusFade = 10139,                // AIP_Succubus_Fade
    AIPGaxkang = 10140,                     // AIP_Gaxkang
    AIPClimaxDwarf = 10141,                 // AIP_ClimaxDwarf
    AIPClimaxElf = 10142,                   // AIP_ClimaxElf
};

// A row of Abilities (abi_*, worksheet 1).
class AbilitiesTalents : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(dao::script::Table::AbilitiesTalents);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<AbilitiesTalents> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<AbilitiesTalents>(AbilitiesTalents(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::int32_t> NameStrRef() const
    {
        return Whole<std::int32_t>(0x3273F70DU);
    }
    std::optional<std::int32_t> DescStrRef() const
    {
        return Whole<std::int32_t>(0x50A903B6U);
    }
    std::optional<std::int32_t> ToolTipStrRef() const
    {
        return Whole<std::int32_t>(0xCA603B31U);
    }
    std::optional<std::int32_t> StrID_Effect() const
    {
        return Whole<std::int32_t>(0x2D678F38U);
    }
    std::optional<std::wstring_view> Icon() const
    {
        return Text(0xBCDDB871U);
    }
    std::optional<std::int32_t> AbilityType() const
    {
        return Whole<std::int32_t>(0xE1A8F4EAU);
    }
    std::optional<std::int32_t> AbilityMode() const
    {
        return Whole<std::int32_t>(0x88A8FFA4U);
    }
    std::optional<std::int32_t> Conditions() const
    {
        return Whole<std::int32_t>(0xB1CAEE1DU);
    }
    std::optional<std::int32_t> Condition_Mode() const
    {
        return Whole<std::int32_t>(0x07342EA2U);
    }
    std::optional<std::int32_t> Condition_Group() const
    {
        return Whole<std::int32_t>(0xE443ACF6U);
    }
    std::optional<float> Cost() const
    {
        return Real<float>(0xF03D763EU);
    }
    std::optional<float> CostUpkeep() const
    {
        return Real<float>(0x277E5E68U);
    }
    std::optional<std::int32_t> PrereqAbility() const
    {
        return Whole<std::int32_t>(0x04D6C80AU);
    }
    std::optional<std::int32_t> PrereqLevel() const
    {
        return Whole<std::int32_t>(0xE17EF364U);
    }
    std::optional<std::int32_t> PrereqAttribute() const
    {
        return Whole<std::int32_t>(0xB15C2B51U);
    }
    std::optional<std::int32_t> PrereqAttValue() const
    {
        return Whole<std::int32_t>(0x7CFE85D1U);
    }
    std::optional<std::int32_t> GUIType() const
    {
        return Whole<std::int32_t>(0xDE32796DU);
    }
    std::optional<std::int32_t> TargetType() const
    {
        return Whole<std::int32_t>(0x0F642429U);
    }
    std::optional<std::int32_t> AutoTarget() const
    {
        return Whole<std::int32_t>(0x1C7CA631U);
    }
    std::optional<std::int32_t> Range() const
    {
        return Whole<std::int32_t>(0x81F2A384U);
    }
    std::optional<std::int32_t> Speed() const
    {
        return Whole<std::int32_t>(0x07F4F006U);
    }
    std::optional<std::int32_t> UseType() const
    {
        return Whole<std::int32_t>(0xACEFC4C2U);
    }
    std::optional<std::wstring_view> SpellScript() const
    {
        return Text(0x06296D47U);
    }
    std::optional<float> Cooldown() const
    {
        return Real<float>(0x17F393BAU);
    }
    std::optional<std::int32_t> ConjureVFX() const
    {
        return Whole<std::int32_t>(0x2B6BDC9FU);
    }
    std::optional<std::wstring_view> BlendTree() const
    {
        return Text(0x70F463FAU);
    }
    std::optional<std::int32_t> ConjureAnim() const
    {
        return Whole<std::int32_t>(0x9ABEAEF1U);
    }
    std::optional<std::int32_t> CastAnim() const
    {
        return Whole<std::int32_t>(0xF95409F5U);
    }
    std::optional<std::int32_t> Projectile() const
    {
        return Whole<std::int32_t>(0x015032BBU);
    }
    std::optional<std::int32_t> AutoCancel() const
    {
        return Whole<std::int32_t>(0x56625C69U);
    }
    std::optional<std::int32_t> AutoDraw() const
    {
        return Whole<std::int32_t>(0xE35112C1U);
    }
    std::optional<float> Threat_Use() const
    {
        return Real<float>(0xD464D283U);
    }
    std::optional<float> Threat_Impact() const
    {
        return Real<float>(0x415100DDU);
    }
    std::optional<std::int32_t> AOE_Type() const
    {
        return Whole<std::int32_t>(0x2EB7565CU);
    }
    std::optional<std::int32_t> AOE_Idx() const
    {
        return Whole<std::int32_t>(0x5D3E0787U);
    }
    std::optional<float> AOE_Param1() const
    {
        return Real<float>(0x5E269FD0U);
    }
    std::optional<float> AOE_Param2() const
    {
        return Real<float>(0x750BCC13U);
    }
    std::optional<std::int32_t> VFX_Impact0() const
    {
        return Whole<std::int32_t>(0xC956AC09U);
    }
    std::optional<std::int32_t> VFX_Impact1() const
    {
        return Whole<std::int32_t>(0xD04D9D48U);
    }
    std::optional<std::int32_t> Flags() const
    {
        return Whole<std::int32_t>(0xD97C90C5U);
    }
    std::optional<std::int32_t> ShowIntactics() const
    {
        return Whole<std::int32_t>(0x50A30845U);
    }
    std::optional<std::int32_t> ShowInCharGen() const
    {
        return Whole<std::int32_t>(0xECE22F72U);
    }
    std::optional<std::int32_t> Passive_ID() const
    {
        return Whole<std::int32_t>(0xC4EE8EE0U);
    }
    std::optional<std::int32_t> Crust_ID() const
    {
        return Whole<std::int32_t>(0xC4D9FED5U);
    }
    std::optional<std::int32_t> Resistance() const
    {
        return Whole<std::int32_t>(0x6E1D17EAU);
    }
    std::optional<std::int32_t> DamageInfo() const
    {
        return Whole<std::int32_t>(0x2D0A1F47U);
    }
    std::optional<std::int32_t> Condition_AppGroup() const
    {
        return Whole<std::int32_t>(0xC7E5BB24U);
    }
    std::optional<std::int32_t> ProcessWithEvent() const
    {
        return Whole<std::int32_t>(0x57119DC9U);
    }
    std::optional<float> Fatigue() const
    {
        return Real<float>(0x77CFF3E3U);
    }
};

// A row of Appearances (apr_*, worksheet 2).
class Appearance : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(dao::script::Table::Appearance);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<Appearance> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<Appearance>(Appearance(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::int32_t> String_Ref() const
    {
        return Whole<std::int32_t>(0xD0D80E8DU);
    }
    std::optional<std::wstring_view> ModelType() const
    {
        return Text(0x5D092182U);
    }
    std::optional<std::wstring_view> ModelRace() const
    {
        return Text(0x364F5082U);
    }
    std::optional<std::wstring_view> ModelName() const
    {
        return Text(0xFD8403D5U);
    }
    std::optional<std::uint8_t> PlayHumanoidAnimations() const
    {
        return Whole<std::uint8_t>(0x469E64C4U);
    }
    std::optional<std::wstring_view> ModelGenderOverride() const
    {
        return Text(0x7B02EB1AU);
    }
    std::optional<std::int32_t> MaxScaleLevel() const
    {
        return Whole<std::int32_t>(0xD0C45555U);
    }
    std::optional<std::int32_t> AvoidAOEChance() const
    {
        return Whole<std::int32_t>(0x87DC5EFDU);
    }
    std::optional<std::int32_t> RulesRace() const
    {
        return Whole<std::int32_t>(0x19FBE813U);
    }
    std::optional<std::int32_t> SynchronizedCombat() const
    {
        return Whole<std::int32_t>(0xCDD753B2U);
    }
    std::optional<std::int32_t> AI_Armor_Type() const
    {
        return Whole<std::int32_t>(0xAC8E1D85U);
    }
    std::optional<std::int32_t> Crust_Effect() const
    {
        return Whole<std::int32_t>(0x1BB8AB1FU);
    }
    std::optional<std::wstring_view> ModelTree() const
    {
        return Text(0x500976ECU);
    }
    std::optional<std::wstring_view> CombatModelTree() const
    {
        return Text(0xDE80B418U);
    }
    std::optional<std::wstring_view> MovementTree() const
    {
        return Text(0xEB48D02AU);
    }
    std::optional<std::wstring_view> CombatMoveTree() const
    {
        return Text(0x75F492ECU);
    }
    std::optional<std::wstring_view> OverlayTree() const
    {
        return Text(0x4375DAC6U);
    }
    std::optional<std::wstring_view> OverrideTree() const
    {
        return Text(0xD56E6E09U);
    }
    std::optional<std::wstring_view> AmbientOverride() const
    {
        return Text(0x367842F0U);
    }
    std::optional<std::wstring_view> BlendCurves() const
    {
        return Text(0xC351732AU);
    }
    std::optional<float> WeaponScale() const
    {
        return Real<float>(0x07458FE3U);
    }
    std::optional<float> WeaponOffset() const
    {
        return Real<float>(0x15A045B8U);
    }
    std::optional<std::wstring_view> BloodColr() const
    {
        return Text(0x88DB1765U);
    }
    std::optional<float> Anim_WalkSpeed() const
    {
        return Real<float>(0xCD9CC343U);
    }
    std::optional<float> Anim_RunSpeed() const
    {
        return Real<float>(0xBD6D4446U);
    }
    std::optional<float> Anim_CbtStrFSpeed() const
    {
        return Real<float>(0x7FB8C4E9U);
    }
    std::optional<float> Anim_CbtSpeed() const
    {
        return Real<float>(0x10727878U);
    }
    std::optional<float> Game_WalkSpeed() const
    {
        return Real<float>(0x958F23C8U);
    }
    std::optional<float> Game_RunSpeed() const
    {
        return Real<float>(0xA0DCD78CU);
    }
    std::optional<float> Game_CbtStrFSpeed() const
    {
        return Real<float>(0x03186054U);
    }
    std::optional<float> Game_CbtSpeed() const
    {
        return Real<float>(0x0DC3EBB2U);
    }
    std::optional<float> Height() const
    {
        return Real<float>(0x0DD1F4C1U);
    }
    std::optional<float> Height_F() const
    {
        return Real<float>(0x1B23D645U);
    }
    std::optional<float> ModelScaleX() const
    {
        return Real<float>(0xA3977767U);
    }
    std::optional<float> ModelScaleY() const
    {
        return Real<float>(0xBA8C4626U);
    }
    std::optional<float> ModelScaleZ() const
    {
        return Real<float>(0x91A115E5U);
    }
    std::optional<float> TerrainFollowPitch() const
    {
        return Real<float>(0xD11FCE15U);
    }
    std::optional<float> TerrainFollowRoll() const
    {
        return Real<float>(0x9B8A2CBEU);
    }
    std::optional<std::int32_t> TerrainFollowPrecision() const
    {
        return Whole<std::int32_t>(0xB6D228CDU);
    }
    std::optional<std::int32_t> Head_Worksheet() const
    {
        return Whole<std::int32_t>(0x72A9CF04U);
    }
    std::optional<std::int32_t> Hair_Worksheet() const
    {
        return Whole<std::int32_t>(0x513675A5U);
    }
    std::optional<std::int32_t> Eyes_Worksheet() const
    {
        return Whole<std::int32_t>(0x08A324C1U);
    }
    std::optional<std::int32_t> Beard_Worksheet() const
    {
        return Whole<std::int32_t>(0x74296F4FU);
    }
    std::optional<std::wstring_view> Neck_Model() const
    {
        return Text(0xDC9C928DU);
    }
    std::optional<std::uint8_t> Targetable() const
    {
        return Whole<std::uint8_t>(0x48E10119U);
    }
    std::optional<std::uint8_t> Disabled_Armor_Sounds() const
    {
        return Whole<std::uint8_t>(0x429CA38FU);
    }
    std::optional<std::uint8_t> Disabled_Weapon_Sounds() const
    {
        return Whole<std::uint8_t>(0x05B8D212U);
    }
    std::optional<std::wstring_view> Default_Sound_Set() const
    {
        return Text(0x1F6B90F1U);
    }
    std::optional<std::wstring_view> Sound_Footstep_Type() const
    {
        return Text(0xDA48E219U);
    }
    std::optional<std::int32_t> Sound_Unarmed() const
    {
        return Whole<std::int32_t>(0x7D86F95AU);
    }
    std::optional<std::int32_t> Sound_Default_Armor() const
    {
        return Whole<std::int32_t>(0x8EA8DC24U);
    }
    std::optional<std::wstring_view> Sound_Monster_Size() const
    {
        return Text(0xC7CB6561U);
    }
    std::optional<std::uint8_t> HasTurnAttacks() const
    {
        return Whole<std::uint8_t>(0xB0EAC96BU);
    }
    std::optional<std::int32_t> ValidDeathBlows() const
    {
        return Whole<std::int32_t>(0x8A442AFEU);
    }
    std::optional<std::int32_t> Creature_Type() const
    {
        return Whole<std::int32_t>(0xC847DE53U);
    }
    std::optional<std::int32_t> Creature_Group() const
    {
        return Whole<std::int32_t>(0xAD719366U);
    }
    std::optional<std::int32_t> OneShotKills() const
    {
        return Whole<std::int32_t>(0x6DFD1BAEU);
    }
    std::optional<std::int32_t> CanDoDeathBlows() const
    {
        return Whole<std::int32_t>(0x98147E3DU);
    }
    std::optional<std::int32_t> DefaultEffectFlags() const
    {
        return Whole<std::int32_t>(0x88D2A3E7U);
    }
    std::optional<float> PerSpace() const
    {
        return Real<float>(0xB04DB298U);
    }
    std::optional<std::uint8_t> MeleeRingUpdateAlways() const
    {
        return Whole<std::uint8_t>(0x7ACD13EEU);
    }
    std::optional<float> MeleeRingDisabledStart() const
    {
        return Real<float>(0x6B668B36U);
    }
    std::optional<float> MeleeRingDisabledEnd() const
    {
        return Real<float>(0x126D9F8AU);
    }
    std::optional<float> MeleeRingOffset() const
    {
        return Real<float>(0xED58B0C9U);
    }
    std::optional<std::int32_t> BHasAppearAnim() const
    {
        return Whole<std::int32_t>(0xE016BF10U);
    }
    std::optional<std::int32_t> BHasDisappearAnim() const
    {
        return Whole<std::int32_t>(0x6B47C703U);
    }
    std::optional<std::int32_t> AppearVFX() const
    {
        return Whole<std::int32_t>(0x1EFAE6E6U);
    }
    std::optional<std::int32_t> BCanBleed() const
    {
        return Whole<std::int32_t>(0x423AD260U);
    }
    std::optional<std::int32_t> BloodPool() const
    {
        return Whole<std::int32_t>(0xBC1BB19CU);
    }
    std::optional<std::wstring_view> DefaultWeapon() const
    {
        return Text(0x2C6B1C7FU);
    }
    std::optional<std::wstring_view> DefaultArmor() const
    {
        return Text(0x8606D68BU);
    }
    std::optional<std::wstring_view> CorpseModel() const
    {
        return Text(0xCE39BFCDU);
    }
    std::optional<std::int32_t> DecayBehaviour() const
    {
        return Whole<std::int32_t>(0x98BFB469U);
    }
    std::optional<float> AttackFL() const
    {
        return Real<float>(0x7A6A6A6DU);
    }
    std::optional<float> AttackFlWidth() const
    {
        return Real<float>(0x06C061C3U);
    }
    std::optional<std::int32_t> AttackFLRing() const
    {
        return Whole<std::int32_t>(0x2BCC18D5U);
    }
    std::optional<float> AttackFR() const
    {
        return Real<float>(0xAE2B55B2U);
    }
    std::optional<float> AttackFRWidth() const
    {
        return Real<float>(0xE3E95F21U);
    }
    std::optional<std::int32_t> AttackFRRing() const
    {
        return Whole<std::int32_t>(0x12ABA98EU);
    }
    std::optional<float> AttackBR() const
    {
        return Real<float>(0x2149C2E5U);
    }
    std::optional<float> AttackBRWidth() const
    {
        return Real<float>(0xFA45D03AU);
    }
    std::optional<std::int32_t> AttackBRRing() const
    {
        return Whole<std::int32_t>(0x11AD92F1U);
    }
    std::optional<float> AttackBL() const
    {
        return Real<float>(0xF508FD3AU);
    }
    std::optional<float> AttackBLWidth() const
    {
        return Real<float>(0x1F6CEED8U);
    }
    std::optional<std::int32_t> AttackBLRing() const
    {
        return Whole<std::int32_t>(0x28CA23AAU);
    }
    std::optional<float> AttackReach() const
    {
        return Real<float>(0xADC2772EU);
    }
    std::optional<std::wstring_view> CodexPlot() const
    {
        return Text(0xE693EB92U);
    }
    std::optional<std::int32_t> CodexFlag() const
    {
        return Whole<std::int32_t>(0xDD77E59FU);
    }
    std::optional<std::int32_t> AprRulesFlags() const
    {
        return Whole<std::int32_t>(0xB6484AA7U);
    }
    std::optional<std::int32_t> BDisableGore() const
    {
        return Whole<std::int32_t>(0x52EA2ED0U);
    }
    std::optional<std::int32_t> BumpLevel() const
    {
        return Whole<std::int32_t>(0x744DFB70U);
    }
    std::optional<std::int32_t> AppearanceRestrictionGroup() const
    {
        return Whole<std::int32_t>(0x9274062DU);
    }
    std::optional<float> AppearanceRating() const
    {
        return Real<float>(0xD83000C6U);
    }
    std::optional<std::int32_t> GroupType() const
    {
        return Whole<std::int32_t>(0xDE532FBDU);
    }
};

// A row of Classes (cla_*, worksheet 3).
class RulesClasses : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(dao::script::Table::RulesClasses);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<RulesClasses> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<RulesClasses>(RulesClasses(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::int32_t> NameStrRef() const
    {
        return Whole<std::int32_t>(0x3273F70DU);
    }
    std::optional<std::int32_t> PluralStrRef() const
    {
        return Whole<std::int32_t>(0x81D044F9U);
    }
    std::optional<std::int32_t> LowerStrRef() const
    {
        return Whole<std::int32_t>(0x07680B5CU);
    }
    std::optional<std::int32_t> DescStrRef() const
    {
        return Whole<std::int32_t>(0x50A903B6U);
    }
    std::optional<std::int32_t> ToolTipStrRef() const
    {
        return Whole<std::int32_t>(0xCA603B31U);
    }
    std::optional<std::wstring_view> Icon() const
    {
        return Text(0xBCDDB871U);
    }
    std::optional<std::wstring_view> Constant() const
    {
        return Text(0x817EB725U);
    }
    std::optional<std::int32_t> Package() const
    {
        return Whole<std::int32_t>(0x42A165DCU);
    }
    std::optional<float> BaseHealth() const
    {
        return Real<float>(0x2FC8E9A0U);
    }
    std::optional<std::int32_t> BUsesMana() const
    {
        return Whole<std::int32_t>(0x34E302CFU);
    }
    std::optional<float> BaseManaStamina() const
    {
        return Real<float>(0x2750EE95U);
    }
    std::optional<float> DepletableProgression() const
    {
        return Real<float>(0x4A74A90CU);
    }
    std::optional<float> StrAdjust() const
    {
        return Real<float>(0x074519FDU);
    }
    std::optional<float> ConAdjust() const
    {
        return Real<float>(0xD95DFD87U);
    }
    std::optional<float> WillAdjust() const
    {
        return Real<float>(0x4536E13CU);
    }
    std::optional<float> MagAdjust() const
    {
        return Real<float>(0x4EA4E6E5U);
    }
    std::optional<float> IntAdjust() const
    {
        return Real<float>(0xD1354F6EU);
    }
    std::optional<float> DexAdjust() const
    {
        return Real<float>(0x52B03A15U);
    }
    std::optional<std::int32_t> MinLevel() const
    {
        return Whole<std::int32_t>(0x7443CC93U);
    }
    std::optional<std::int32_t> BaseClass() const
    {
        return Whole<std::int32_t>(0x37760E31U);
    }
    std::optional<float> BaseAttack() const
    {
        return Real<float>(0x142B2993U);
    }
    std::optional<float> BaseDefense() const
    {
        return Real<float>(0xDCB124FCU);
    }
    std::optional<float> DefensePerLevel() const
    {
        return Real<float>(0xAE8F9C55U);
    }
    std::optional<float> HealthPerLevel() const
    {
        return Real<float>(0x8B42E5BAU);
    }
    std::optional<std::int32_t> LevelsPerAbility() const
    {
        return Whole<std::int32_t>(0x5C2019DBU);
    }
    std::optional<std::int32_t> LevelsPerSkill() const
    {
        return Whole<std::int32_t>(0x8F437E36U);
    }
    std::optional<float> DamagePerLevel() const
    {
        return Real<float>(0x5FAB35DDU);
    }
    std::optional<std::int32_t> StartingAbility1() const
    {
        return Whole<std::int32_t>(0x4D8C23ABU);
    }
    std::optional<std::int32_t> StartingAbility2() const
    {
        return Whole<std::int32_t>(0x66A17068U);
    }
    std::optional<std::int32_t> PlayerClass() const
    {
        return Whole<std::int32_t>(0x76124935U);
    }
    std::optional<std::wstring_view> CharGenLabel() const
    {
        return Text(0x042B26E3U);
    }
    std::optional<std::wstring_view> Achievement() const
    {
        return Text(0x1E6721D3U);
    }
};

// A row of Items (bitm_*, worksheet 6).
class Items : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(dao::script::Table::Items);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<Items> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<Items>(Items(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::int32_t> NameStrRef() const
    {
        return Whole<std::int32_t>(0x3273F70DU);
    }
    std::optional<std::int32_t> DescStrRef() const
    {
        return Whole<std::int32_t>(0x50A903B6U);
    }
    std::optional<std::int32_t> InvSlotWidth() const
    {
        return Whole<std::int32_t>(0xABC4B477U);
    }
    std::optional<std::int32_t> InvSlotHeight() const
    {
        return Whole<std::int32_t>(0xDC0F6A8AU);
    }
    std::optional<std::int32_t> EquippableSlots() const
    {
        return Whole<std::int32_t>(0xD5B0AE22U);
    }
    std::optional<std::int32_t> WearR() const
    {
        return Whole<std::int32_t>(0x7083F7D4U);
    }
    std::optional<std::int32_t> WearL() const
    {
        return Whole<std::int32_t>(0xA4C2C80BU);
    }
    std::optional<std::int32_t> WieldR() const
    {
        return Whole<std::int32_t>(0x48859B70U);
    }
    std::optional<std::int32_t> WieldL() const
    {
        return Whole<std::int32_t>(0x9CC4A4AFU);
    }
    std::optional<std::uint8_t> Heraldry() const
    {
        return Whole<std::uint8_t>(0x73299AECU);
    }
    std::optional<float> WeaponOffset() const
    {
        return Real<float>(0x15A045B8U);
    }
    std::optional<std::uint8_t> CanRotateIcon() const
    {
        return Whole<std::uint8_t>(0x01B8EADEU);
    }
    std::optional<std::int32_t> TargetType() const
    {
        return Whole<std::int32_t>(0x0F642429U);
    }
    std::optional<std::int32_t> Variation_Worksheet() const
    {
        return Whole<std::int32_t>(0x43B54707U);
    }
    std::optional<std::int32_t> DefaultMaterial() const
    {
        return Whole<std::int32_t>(0x5D98902BU);
    }
    std::optional<std::wstring_view> DefaultModel() const
    {
        return Text(0xE7779C8CU);
    }
    std::optional<std::wstring_view> DefaultIcon() const
    {
        return Text(0xA980657EU);
    }
    std::optional<std::wstring_view> DefaultResource() const
    {
        return Text(0x575CB751U);
    }
    std::optional<std::int32_t> WeaponWield() const
    {
        return Whole<std::int32_t>(0x1DE44006U);
    }
    std::optional<std::wstring_view> WieldModel() const
    {
        return Text(0x8BF8FC78U);
    }
    std::optional<std::wstring_view> WieldModelOverlay() const
    {
        return Text(0x543CC507U);
    }
    std::optional<float> CombatRating() const
    {
        return Real<float>(0xA2CE7725U);
    }
    std::optional<float> StrengthModifier() const
    {
        return Real<float>(0x1772174CU);
    }
    std::optional<std::int32_t> BaseDamage() const
    {
        return Whole<std::int32_t>(0x3F1A84F6U);
    }
    std::optional<std::int32_t> StackSize() const
    {
        return Whole<std::int32_t>(0x39974122U);
    }
    std::optional<std::int32_t> MinItemProps() const
    {
        return Whole<std::int32_t>(0xBF0CC645U);
    }
    std::optional<std::int32_t> MaxItemProps() const
    {
        return Whole<std::int32_t>(0x62AAD6B5U);
    }
    std::optional<std::wstring_view> PropColumn() const
    {
        return Text(0xC0D6C7F2U);
    }
    std::optional<std::int32_t> StartingCharges() const
    {
        return Whole<std::int32_t>(0x10982F98U);
    }
    std::optional<std::int32_t> QBBehaviour() const
    {
        return Whole<std::int32_t>(0x902052D4U);
    }
    std::optional<std::int32_t> StorePanelSort() const
    {
        return Whole<std::int32_t>(0xC02A1003U);
    }
    std::optional<std::int32_t> Type() const
    {
        return Whole<std::int32_t>(0x12A02374U);
    }
    std::optional<std::int32_t> Speed() const
    {
        return Whole<std::int32_t>(0x07F4F006U);
    }
    std::optional<float> Range() const
    {
        return Real<float>(0x81F2A384U);
    }
    std::optional<std::int32_t> Projectile() const
    {
        return Whole<std::int32_t>(0x015032BBU);
    }
    std::optional<float> ProjectileOffset() const
    {
        return Real<float>(0xFA66B18DU);
    }
    std::optional<std::int32_t> Ammo() const
    {
        return Whole<std::int32_t>(0xE6203B7EU);
    }
    std::optional<std::int32_t> StorePanel() const
    {
        return Whole<std::int32_t>(0xBC632071U);
    }
    std::optional<std::int32_t> ModalabilityBlendTree() const
    {
        return Whole<std::int32_t>(0x074DC804U);
    }
    std::optional<std::int32_t> InvSoundType() const
    {
        return Whole<std::int32_t>(0x7E0505A1U);
    }
    std::optional<std::int32_t> ArmorType() const
    {
        return Whole<std::int32_t>(0xD39B8235U);
    }
    std::optional<std::int32_t> DoNotDrop() const
    {
        return Whole<std::int32_t>(0x80C0AABCU);
    }
    std::optional<std::int32_t> ReqProp1() const
    {
        return Whole<std::int32_t>(0x4F10F824U);
    }
    std::optional<float> ReqProp1Val() const
    {
        return Real<float>(0xE9CD3A6CU);
    }
    std::optional<std::int32_t> ReqProp2() const
    {
        return Whole<std::int32_t>(0x643DABE7U);
    }
    std::optional<float> ReqProp2Val() const
    {
        return Real<float>(0x67423D8FU);
    }
    std::optional<std::int32_t> PiercingType() const
    {
        return Whole<std::int32_t>(0x97F088C1U);
    }
    std::optional<std::int32_t> RuneSlots() const
    {
        return Whole<std::int32_t>(0x7AF8B426U);
    }
    std::optional<std::int32_t> RuneCount() const
    {
        return Whole<std::int32_t>(0x017132ECU);
    }
    std::optional<std::int32_t> SoundVariation() const
    {
        return Whole<std::int32_t>(0xEC2488DCU);
    }
    std::optional<std::int32_t> IsHeavy() const
    {
        return Whole<std::int32_t>(0x89DE51F1U);
    }
    std::optional<std::int32_t> ShowMaterial() const
    {
        return Whole<std::int32_t>(0x66F63962U);
    }
    std::optional<std::uint8_t> SuppressPropertyVFX() const
    {
        return Whole<std::uint8_t>(0x6E6F65B3U);
    }
};

// A row of Portraits (portraits*, worksheet 7).
class Portraits : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(Sheet::Portraits);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<Portraits> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<Portraits>(Portraits(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::wstring_view> Tag() const
    {
        return Text(0x88578372U);
    }
    std::optional<float> CameraPitch() const
    {
        return Real<float>(0x5B84AEC7U);
    }
    std::optional<float> CameraRotation() const
    {
        return Real<float>(0x97A92B07U);
    }
    std::optional<float> CameraDistance() const
    {
        return Real<float>(0xEFBEFD6CU);
    }
    std::optional<float> CameraZAdjust() const
    {
        return Real<float>(0xDB253899U);
    }
    std::optional<float> CameraXAdjust() const
    {
        return Real<float>(0x3A4BFC34U);
    }
    std::optional<std::int32_t> PortraitTint() const
    {
        return Whole<std::int32_t>(0xC9210008U);
    }
    std::optional<float> Expression() const
    {
        return Real<float>(0x39950924U);
    }
    std::optional<std::int32_t> Appearance() const
    {
        return Whole<std::int32_t>(0x052BEA01U);
    }
    std::optional<std::wstring_view> Override() const
    {
        return Text(0x66B0206FU);
    }
    std::optional<float> LightXAdjust() const
    {
        return Real<float>(0x8327EA19U);
    }
    std::optional<float> LightYAdjust() const
    {
        return Real<float>(0x1E280B6FU);
    }
    std::optional<float> LightZAdjust() const
    {
        return Real<float>(0x62492EB4U);
    }
};

// A row of Visual Effects (vfx_*, worksheet 8).
class VisualEffects : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(Sheet::VisualEffects);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<VisualEffects> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<VisualEffects>(VisualEffects(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::wstring_view> Type() const
    {
        return Text(0x12A02374U);
    }
    std::optional<std::uint8_t> FixedOrientation() const
    {
        return Whole<std::uint8_t>(0x9E459C07U);
    }
    std::optional<std::uint8_t> AlignToGround() const
    {
        return Whole<std::uint8_t>(0x4BC7021FU);
    }
    std::optional<std::wstring_view> ModelName() const
    {
        return Text(0xFD8403D5U);
    }
    std::optional<std::wstring_view> ModelName1() const
    {
        return Text(0xE2C28829U);
    }
    std::optional<float> Size1() const
    {
        return Real<float>(0x514CC359U);
    }
    std::optional<std::wstring_view> ModelName2() const
    {
        return Text(0xC9EFDBEAU);
    }
    std::optional<float> Size2() const
    {
        return Real<float>(0x7A61909AU);
    }
    std::optional<std::wstring_view> ModelName3() const
    {
        return Text(0xD0F4EAABU);
    }
    std::optional<float> Size3() const
    {
        return Real<float>(0x637AA1DBU);
    }
    std::optional<std::wstring_view> BlendTreeName() const
    {
        return Text(0x1109E419U);
    }
    std::optional<std::uint8_t> CrustEffect() const
    {
        return Whole<std::uint8_t>(0xBE06F2EFU);
    }
    std::optional<std::wstring_view> SoundImpact() const
    {
        return Text(0x14169941U);
    }
    std::optional<float> SoundImpactVolume() const
    {
        return Real<float>(0x4F23F2A0U);
    }
    std::optional<float> SoundImpactPitch() const
    {
        return Real<float>(0xF068B861U);
    }
    std::optional<std::wstring_view> SoundDuration() const
    {
        return Text(0x17FB7406U);
    }
    std::optional<float> SoundDurationVolume() const
    {
        return Real<float>(0x1CE8F87BU);
    }
    std::optional<float> SoundDurationPitch() const
    {
        return Real<float>(0x6E4AE829U);
    }
    std::optional<std::wstring_view> SoundCessation() const
    {
        return Text(0x896C1D4FU);
    }
    std::optional<float> SoundCessationVolume() const
    {
        return Real<float>(0xBB9BD79BU);
    }
    std::optional<float> SoundCessationPitch() const
    {
        return Real<float>(0x66974335U);
    }
    std::optional<std::wstring_view> Tint() const
    {
        return Text(0xF2692253U);
    }
    std::optional<std::int32_t> LoadGlobal() const
    {
        return Whole<std::int32_t>(0x713FEF95U);
    }
    std::optional<std::uint8_t> PauseAnimate() const
    {
        return Whole<std::uint8_t>(0x5C35C9C5U);
    }
    std::optional<std::int32_t> Priority() const
    {
        return Whole<std::int32_t>(0x76E5BBCBU);
    }
    std::optional<std::uint8_t> ShowInPaperDoll() const
    {
        return Whole<std::uint8_t>(0xF23D2AA1U);
    }
};

// A row of Placeables (placeable_types*, worksheet 93).
class PlaceableTypes : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(dao::script::Table::PlaceableTypes);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<PlaceableTypes> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<PlaceableTypes>(PlaceableTypes(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::int32_t> StrRef() const
    {
        return Whole<std::int32_t>(0x406341BEU);
    }
    std::optional<std::wstring_view> ModelName() const
    {
        return Text(0xFD8403D5U);
    }
    std::optional<std::wstring_view> BlendTree() const
    {
        return Text(0x70F463FAU);
    }
    std::optional<std::int32_t> DestroyVFX() const
    {
        return Whole<std::int32_t>(0x17326D1BU);
    }
    std::optional<std::int32_t> CrustVFX() const
    {
        return Whole<std::int32_t>(0x5A6EE720U);
    }
    std::optional<float> OcclusionFactor() const
    {
        return Real<float>(0x913E3094U);
    }
    std::optional<std::wstring_view> StateController() const
    {
        return Text(0x9A320A51U);
    }
    std::optional<std::int32_t> RemoveWhenDead() const
    {
        return Whole<std::int32_t>(0x1DEAA1EAU);
    }
    std::optional<std::int32_t> BaseType() const
    {
        return Whole<std::int32_t>(0x5010EDC3U);
    }
    std::optional<float> TurnRate() const
    {
        return Real<float>(0x5F75A070U);
    }
    std::optional<std::int32_t> EnableCollisionEvent() const
    {
        return Whole<std::int32_t>(0xE19F98B1U);
    }
    std::optional<std::int32_t> GeneratePathInfo() const
    {
        return Whole<std::int32_t>(0x8CE0771DU);
    }
    std::optional<float> Height() const
    {
        return Real<float>(0x0DD1F4C1U);
    }
    std::optional<std::int32_t> Health() const
    {
        return Whole<std::int32_t>(0x484BAACDU);
    }
    std::optional<std::wstring_view> PickLockSuccess() const
    {
        return Text(0xC4E72213U);
    }
    std::optional<std::wstring_view> PickLockFailure() const
    {
        return Text(0x4018F4DDU);
    }
    std::optional<std::wstring_view> OpenContainerSound() const
    {
        return Text(0x98094859U);
    }
    std::optional<std::wstring_view> ClosedToOpened() const
    {
        return Text(0x367D454EU);
    }
    std::optional<std::wstring_view> OpenedToClosed() const
    {
        return Text(0xAEF1F392U);
    }
    std::optional<std::wstring_view> Destroyed() const
    {
        return Text(0xA311BEB6U);
    }
    std::optional<std::wstring_view> Used() const
    {
        return Text(0x014D9660U);
    }
    std::optional<std::wstring_view> Locked() const
    {
        return Text(0x475F4948U);
    }
    std::optional<std::wstring_view> Hit() const
    {
        return Text(0x581828EFU);
    }
    std::optional<std::wstring_view> Unknown() const
    {
        return Text(0x66463574U);
    }
    std::optional<std::wstring_view> Sword() const
    {
        return Text(0x6164B55FU);
    }
    std::optional<std::wstring_view> Axe() const
    {
        return Text(0xC46B6ACDU);
    }
    std::optional<std::wstring_view> Blunt() const
    {
        return Text(0x7A99A97FU);
    }
    std::optional<std::wstring_view> Dagger() const
    {
        return Text(0xB08A7453U);
    }
    std::optional<std::wstring_view> Staff() const
    {
        return Text(0x38AC7186U);
    }
    std::optional<std::wstring_view> BareHands() const
    {
        return Text(0xF504DC92U);
    }
    std::optional<std::wstring_view> Claws() const
    {
        return Text(0x6F4C0C1EU);
    }
    std::optional<std::wstring_view> Arrow() const
    {
        return Text(0x760B357BU);
    }
    std::optional<std::wstring_view> Rock() const
    {
        return Text(0xDD6F1E8AU);
    }
    std::optional<std::wstring_view> Shield_Wood() const
    {
        return Text(0x9E44EC85U);
    }
    std::optional<std::wstring_view> Shield_Steel() const
    {
        return Text(0x56C4A349U);
    }
};

// A row of Properties (properties*, worksheet 98).
class Properties : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(Sheet::Properties);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<Properties> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<Properties>(Properties(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Stat() const
    {
        return Text(0x1F872A59U);
    }
    std::optional<std::wstring_view> Type() const
    {
        return Text(0x12A02374U);
    }
    std::optional<float> Min() const
    {
        return Real<float>(0xB8F84387U);
    }
    std::optional<float> Max() const
    {
        return Real<float>(0x61D4DEBFU);
    }
    std::optional<std::int32_t> UIMapping() const
    {
        return Whole<std::int32_t>(0xA2C45164U);
    }
    std::optional<std::int32_t> NameStrID() const
    {
        return Whole<std::int32_t>(0xB133F6F9U);
    }
    std::optional<std::int32_t> NameLowerStrID() const
    {
        return Whole<std::int32_t>(0xAFEA8EFBU);
    }
    std::optional<std::int32_t> DescStrID() const
    {
        return Whole<std::int32_t>(0x3593B3FDU);
    }
    std::optional<std::int32_t> Desc2StrID() const
    {
        return Whole<std::int32_t>(0xA73C37EDU);
    }
    std::optional<std::int32_t> NonCombatEffect() const
    {
        return Whole<std::int32_t>(0x7A3DF160U);
    }
    std::optional<std::int32_t> CombatEffect() const
    {
        return Whole<std::int32_t>(0xE24B074DU);
    }
    std::optional<std::int32_t> EngineLink() const
    {
        return Whole<std::int32_t>(0x106CCDD5U);
    }
    std::optional<std::wstring_view> ToolsetName() const
    {
        return Text(0xCDB01DC0U);
    }
    std::optional<std::int32_t> BShowInToolset() const
    {
        return Whole<std::int32_t>(0xB3A02EACU);
    }
    std::optional<std::int32_t> BShowInPartyPicker() const
    {
        return Whole<std::int32_t>(0xFD26E8AFU);
    }
    std::optional<std::int32_t> Precision() const
    {
        return Whole<std::int32_t>(0x19EAD4E4U);
    }
};

// A row of Item Stat Defines (itemstatdefines*, worksheet 109).
class ItemStatDefines : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(Sheet::ItemStatDefines);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<ItemStatDefines> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<ItemStatDefines>(ItemStatDefines(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::int32_t> NameStrRef() const
    {
        return Whole<std::int32_t>(0x3273F70DU);
    }
    std::optional<std::int32_t> CreatureProp() const
    {
        return Whole<std::int32_t>(0xD6505AA1U);
    }
    std::optional<std::int32_t> BShowInUI() const
    {
        return Whole<std::int32_t>(0x97F9C208U);
    }
    std::optional<std::int32_t> Precision() const
    {
        return Whole<std::int32_t>(0x19EAD4E4U);
    }
};

// A row of Item Stats (itemstats*, worksheet 110).
class Itemstats : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(dao::script::Table::Itemstats);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<Itemstats> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<Itemstats>(Itemstats(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<float> Attack() const
    {
        return Real<float>(0x73A86AFEU);
    }
    std::optional<float> Damage() const
    {
        return Real<float>(0x5899C79BU);
    }
    std::optional<float> DamageRange() const
    {
        return Real<float>(0x44EAB611U);
    }
    std::optional<float> Armor() const
    {
        return Real<float>(0xF9FC7170U);
    }
    std::optional<float> ArmorPenetration() const
    {
        return Real<float>(0x779A0C4AU);
    }
    std::optional<float> Defense() const
    {
        return Real<float>(0xE33746BFU);
    }
    std::optional<float> BaseAimDelay() const
    {
        return Real<float>(0xD55AFED5U);
    }
    std::optional<float> DSpeed() const
    {
        return Real<float>(0xB4FF0968U);
    }
    std::optional<float> CritChanceModifier() const
    {
        return Real<float>(0x60A9CF00U);
    }
    std::optional<float> AbilityCostModifier() const
    {
        return Real<float>(0x8E15E921U);
    }
    std::optional<float> MissileDeflection() const
    {
        return Real<float>(0x2F98611FU);
    }
    std::optional<float> OptimumRange() const
    {
        return Real<float>(0x675DEA5DU);
    }
    std::optional<float> SpellPower() const
    {
        return Real<float>(0x96EA6098U);
    }
    std::optional<float> RngdPenalty() const
    {
        return Real<float>(0x5C513D85U);
    }
    std::optional<float> AttributeMod() const
    {
        return Real<float>(0x848D6C51U);
    }
    std::optional<std::int32_t> Attribute0() const
    {
        return Whole<std::int32_t>(0x4074F8DBU);
    }
    std::optional<std::int32_t> Attribute1() const
    {
        return Whole<std::int32_t>(0x596FC99AU);
    }
};

// A row of Effects (effects*, worksheet 136).
class Effects : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(dao::script::Table::Effects);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<Effects> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<Effects>(Effects(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::int32_t> Type() const
    {
        return Whole<std::int32_t>(0x12A02374U);
    }
    std::optional<std::int32_t> CancelOnPlotEvent() const
    {
        return Whole<std::int32_t>(0x2C217D4DU);
    }
    std::optional<std::int32_t> BConsiderHostile() const
    {
        return Whole<std::int32_t>(0x1F420294U);
    }
    std::optional<std::int32_t> BDropStealth() const
    {
        return Whole<std::int32_t>(0xDBDC8830U);
    }
    std::optional<std::int32_t> AllowPermanent() const
    {
        return Whole<std::int32_t>(0x3920511EU);
    }
    std::optional<std::int32_t> AllowInstant() const
    {
        return Whole<std::int32_t>(0x174ACBDDU);
    }
    std::optional<std::int32_t> AllowTemp() const
    {
        return Whole<std::int32_t>(0x8FCACB84U);
    }
    std::optional<std::int32_t> AffectImmortal() const
    {
        return Whole<std::int32_t>(0xF0CEA932U);
    }
    std::optional<std::int32_t> AffectPlot() const
    {
        return Whole<std::int32_t>(0x95B1BF28U);
    }
    std::optional<std::int32_t> Flags() const
    {
        return Whole<std::int32_t>(0xD97C90C5U);
    }
    std::optional<std::int32_t> Priority() const
    {
        return Whole<std::int32_t>(0x76E5BBCBU);
    }
    std::optional<std::int32_t> Animation() const
    {
        return Whole<std::int32_t>(0x43C8D895U);
    }
    std::optional<std::int32_t> IgnoreDeath() const
    {
        return Whole<std::int32_t>(0xB2B84870U);
    }
    std::optional<std::int32_t> SimpleEffect() const
    {
        return Whole<std::int32_t>(0x36518A28U);
    }
    std::optional<std::uint8_t> CameraFollow() const
    {
        return Whole<std::uint8_t>(0xB996F185U);
    }
};

// A row of PRJ_base (prj_base*, worksheet 1006).
class Projectiles : public RowView
{
  public:
    using RowView::RowView;
    static constexpr std::int64_t kId = static_cast<std::int64_t>(dao::script::Table::Projectiles);

    // Returns the row the game takes for an ID; none for an ID no row has.
    static std::optional<Projectiles> Of(const m2da::Merged &table, std::int64_t id)
    {
        const m2da::Row *row = m2da::Taken(table, id);
        return row != nullptr ? std::optional<Projectiles>(Projectiles(table, *row)) : std::nullopt;
    }

    std::optional<std::int32_t> ID() const
    {
        return Whole<std::int32_t>(0x66FBC936U);
    }
    std::optional<std::wstring_view> Label() const
    {
        return Text(0xB49CA166U);
    }
    std::optional<std::wstring_view> ModelName() const
    {
        return Text(0xFD8403D5U);
    }
    std::optional<std::uint8_t> Simulation() const
    {
        return Whole<std::uint8_t>(0xA2166177U);
    }
    std::optional<float> Mass() const
    {
        return Real<float>(0x90FC93BBU);
    }
    std::optional<float> Acceleration() const
    {
        return Real<float>(0xFD480C46U);
    }
    std::optional<float> InitialSpeed() const
    {
        return Real<float>(0x883650AAU);
    }
    std::optional<float> MinSpeed() const
    {
        return Real<float>(0xDB26D384U);
    }
    std::optional<float> TopSpeed() const
    {
        return Real<float>(0xFD919690U);
    }
    std::optional<std::uint8_t> StaticCollisions() const
    {
        return Whole<std::uint8_t>(0x73E07C85U);
    }
    std::optional<std::uint8_t> DynamicCollisions() const
    {
        return Whole<std::uint8_t>(0x97CA5911U);
    }
    std::optional<std::uint8_t> HideLaunch() const
    {
        return Whole<std::uint8_t>(0x06484B80U);
    }
    std::optional<float> ImpactTimeout() const
    {
        return Real<float>(0x0796E13BU);
    }
    std::optional<std::uint8_t> AlphaFade() const
    {
        return Whole<std::uint8_t>(0x7217A33BU);
    }
    std::optional<std::wstring_view> SoundFlying() const
    {
        return Text(0xE9D96D07U);
    }
    std::optional<std::int32_t> SoundMatType() const
    {
        return Whole<std::int32_t>(0x35DB27AAU);
    }
    std::optional<std::int32_t> DamageType() const
    {
        return Whole<std::int32_t>(0x5BFAE957U);
    }
    std::optional<float> DamageBonus() const
    {
        return Real<float>(0x90460F08U);
    }
};

} // namespace dao::core::tables
