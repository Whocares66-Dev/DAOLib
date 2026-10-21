#include "dao/core/Savegame.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <type_traits>
#include <utility>

// Notes on the reading, 2026-10-20 (DAOLib #202).
//
// The labels are xoreos's (dev/gff4_fields.csv) and the types the file's own templates give (78 of them,
// the same in every save seen: tools/gff4.py --templates). Seen in the player's saves, a Dalish start at
// level 1 and the game's end: the counts of tests/test_savegame.cpp against tools/gff4.py's dump.
//
// What each part rests on, read in the GOG exe (1.05.13263) by lib-agent-2 (its maps of the loaders) and
// set beside the code below as [exe 0x...] "checked" (the loader reads it so), [file] "seen in saves only",
// [raw] "kept as it is, no meaning known". The engine's getters read the raw width at the field's offset,
// with no conversion, and an absent label gives the caller's default (nearly always the object's own value);
// "present" is the label being in the struct's template. The order of a save's load: world database, clock
// and effect counter (0x006C7912), the campaign (0x005FF8B0), the party object, the hero (0x0069DBB0), the
// party (0x00637D50), the areas and their objects (0x006FABB0, 0x006882C0), then the event queue
// (0x0064AAD0). The game mode is not saved. A part without a mark is [file].

namespace dao::core::savegame
{

namespace
{

using gff4::Struct;
using gff4::Type;

// -- The labels (dev/gff4_fields.csv), by the name xoreos gives without "Savegame"
namespace L
{
// The top struct
constexpr std::uint32_t kCampaign = 16000, kAreaList = 16001, kPlayerchar = 16002, kPartyList = 16003, kVersion = 16004,
                        kGameState = 16005;
// Shared
constexpr std::uint32_t kTemplateResRef = 3, kPosition = 4, kOrientation = 5, kObjectId = 23, kTag = 1;
constexpr std::uint32_t kObjectActive = 16201, kObjectImmortal = 16220, kObjectEventScript = 16221, kObjectTag = 16222,
                        kItems = 16223, kMaxItems = 16226, kObjectPlot = 16250, kObjectHealth = 16251,
                        kObjectMaxHealth = 16252, kObjectRank = 16253, kObjectTreasureGroup = 16254,
                        kObjectName = 16255, kLootableAppearance = 16257, kObjectPickLock = 16258,
                        kTrapDetected = 16259, kDetectCheck = 16260, kDisarmCheck = 16261, kInteractionRadius = 16262,
                        kIsPlotGiver = 16455, kTeamId = 16453;
constexpr std::uint32_t kEffectList = 16623, kCommandList = 16721, kCurrentActionQueue = 16740, kVariables = 17000;
// Script variables
constexpr std::uint32_t kVarName = 17001, kVarValue = 17003;
// Data arrays and effects
constexpr std::uint32_t kArrInt = 16641, kArrFloat = 16642, kArrBool = 16643, kArrOid = 16644, kArrString = 16645,
                        kArrVector = 16646, kArrQuaternion = 16647;
constexpr std::uint32_t kEffId = 16613, kEffType = 16614, kEffDurationType = 16615, kEffDuration = 16616,
                        kEffTimeIndex = 16618, kEffAnimation = 16619, kEffPriority = 16620, kEffCreator = 16621,
                        kEffAbility = 16622, kEffEngineData = 16624, kEffResource2 = 16625, kEffFlags = 16627,
                        kEffDataarray = 16640;
// Commands
constexpr std::uint32_t kCmdCommandId = 16722, kCmdId = 16723, kCmdStatic = 16724, kCmdData = 16725,
                        kCmdPlayerIssued = 16726;
constexpr std::uint32_t kSubId = 16731, kSubCore = 16732, kSubInterruptable = 16733, kSubTimeIndex = 16734,
                        kSubLastTimeIndex = 16735, kSubLength = 16736, kSubStartTime = 16737, kSubData = 16738;
constexpr std::uint32_t kCurrentCommand = 16720, kSubactionList = 16730;
// Items
constexpr std::uint32_t kItemCost = 1001, kItemStackSize = 1002, kItemAbilityId = 1011, kItemProperties = 1013,
                        kItemPowers = 1014, kItemEffectIds = 1015, kItemVfxIds = 1018, kItemSubitems = 1019,
                        kItemRecipe = 1020, kItemBaseCost = 1021, kItemDroppable = 16224, kItemDamaged = 16225,
                        kItemIrremovable = 16228, kItemIndestructible = 16229, kItemMaterial = 16230,
                        kItemStealable = 16231, kItemInfinite = 16232, kItemVfxPropertyId = 16233,
                        kItemVfxPropertyPower = 16234, kItemUnnamed = 16237;
// Stats
constexpr std::uint32_t kSpellList = 16305, kTalentList = 16306, kSkillList = 16307, kQuickslots = 16308,
                        kQuickslots1 = 16313, kQuickslots2 = 16314, kQuickslots3 = 16315, kQuickslots4 = 16316,
                        kCurrentQbar = 16317, kLockQbar = 16318, kAutoLevelUp = 16329, kUnnamed16339 = 16339,
                        kUnnamed16340 = 16340, kStatList = 16350, kHeroicStatList = 16351, kHeroicPartyStatList = 16352,
                        kModalAbilities = 16468;
constexpr std::uint32_t kStatBase = 16300, kStatModifier = 16301, kStatCurrent = 16302, kStatComRegen = 16303,
                        kStatRegen = 16304, kStatIndex = 16353;
constexpr std::uint32_t kQsAbility = 16311, kQsItemTag = 16312, kQsTemplate = 16319, kQsNumber = 16331;
// Appearance
constexpr std::uint32_t kAppType = 16321, kAppGender = 16322, kAppGore = 16324, kAppDecapitated = 16325,
                        kAppHeraldry = 16326, kAppOriginalType = 16327, kAppMorphName = 16328;
// Creatures
constexpr std::uint32_t kPartyMembers = 16203, kCreatureStats = 16209, kBackpack = 16210, kPlotItems = 16211,
                        kMoney = 16212, kEquipmentActiveSet = 16218, kEquipmentItems = 16219, kCraftingRecipes = 16227,
                        kLoopingAnimation = 16256, kImportance = 16263, kNonPartyMembers = 16279,
                        kPartySubstate = 16280, kPartyLocked = 16281, kPartyFollow = 16282, kAppearance = 16320,
                        kGroupId = 16451, kStealth = 16454, kCanLevelUp = 16456, kTrackable = 16457,
                        kControllable = 16458, kInteractive = 16459, kRace = 16460, kCanChangeEquipment = 16463,
                        kClassRankList = 16464, kIsGhost = 16467, kShowAsAllyOnMap = 16469, kIsStatue = 16470,
                        kMinimizedSkills = 16471, kMinimizedTalents = 16472, kItemsScaled = 16474, kHeartbeat = 16475,
                        kRoamRadius = 16476, kRoamCenter = 16477, kPoolName = 16478, kPoolAvailable = 16479,
                        kNoPermDeath = 16480, kTimeSinceDeath = 16481, kBodybagId = 16600, kCreatureRank = 16612;
constexpr std::uint32_t kClassId = 16465, kClassRank = 16466, kHeaderId = 16473;
constexpr std::uint32_t kPortraitPitch = 16332, kPortraitYaw = 16333, kPortraitTint = 16334,
                        kPortraitExpression = 16335, kPortraitDistance = 16336, kPortraitPositionH = 16337,
                        kPortraitPositionV = 16338;
constexpr std::uint32_t kTacticsHasTable = 16821, kTacticsTable = 16822, kPlayerMorph = 16950, kPlayerSoundSet = 16951,
                        kDefaultSoundSet = 16952;
constexpr std::uint32_t kTacticsEnabled = 16823, kTacticsList = 16824, kTacticsDirty = 16832,
                        kTacticsPresetType = 16833, kTacticsPresetIndex = 16834, kTacticsPresetList = 16835,
                        kTacticsCustomList = 16836;
constexpr std::uint32_t kTeTargetObject = 16818, kTeConditionObject = 16819, kTeEnabled = 16825, kTeTarget = 16826,
                        kTeCondition = 16827, kTeCommand = 16828, kTeParam = 16829, kTeItemTag = 16837,
                        kTeItemResRef = 16838;
// Placeables, triggers and the rest of an area
constexpr std::uint32_t kPlaceableState = 16100, kPlaceableUseable = 16102, kIsBodybag = 16601, kLootableObject = 16602;
constexpr std::uint32_t kTrigGeometry = 16101, kTrigDetectable = 16103, kTrigDisarmable = 16104,
                        kTrigDetectCheck = 16105, kTrigDisarmCheck = 16106, kTrigLastDisarmed = 16107,
                        kTrigReverb = 16108, kTrigLoadScreen = 16110, kTrigSounds = 16111, kTrigType = 16112,
                        kTrigMusicEnter = 16113, kTrigMusicExit = 16114, kTrigMusicEnterDelay = 16115,
                        kTrigMusicExitDelay = 16116;
constexpr std::uint32_t kAoeId = 16603, kAoeShape = 16604, kAoeRadius = 16605, kAoeWidth = 16606, kAoeLength = 16607,
                        kAoeCreator = 16608, kAoeDuration = 16609, kAoeDurationType = 16610, kAoeLinked = 16611,
                        kAoeAbility = 16750, kAoeFlags = 16751, kAoeStationary = 16752;
constexpr std::uint32_t kWpMapNote = 16710, kWpEnabled = 16711, kWpText = 16712, kWpType = 16713, kWpLocText = 16714;
constexpr std::uint32_t kStoreMarkDown = 16150, kStoreMarkUp = 16151, kStoreGold = 16152, kStoreMaxBuy = 16153,
                        kStoreWillNotBuy = 16154, kStoreWillOnlyBuy = 16155, kStoreItemList = 16156;
constexpr std::uint32_t kSoundTag = 16900, kSoundActive = 16901, kSoundName = 16902, kSoundX = 16903, kSoundY = 16904,
                        kSoundZ = 16905, kSoundOx = 16906, kSoundOy = 16907, kSoundOz = 16908, kSoundOw = 16909,
                        kSoundVolume = 16910, kSoundPitch = 16911, kSoundFadeIn = 16912, kSoundFadeOut = 16913,
                        kSoundMaxDistance = 16914, kSoundConeInside = 16915, kSoundConeOutside = 16916,
                        kSoundConeVolume = 16917, kSoundPriority = 16918, kSoundOccludable = 16919;
constexpr std::uint32_t kAreaPlaceables = 16010, kAreaCreatures = 16011, kAreaTriggers = 16012, kAreaAoes = 16013,
                        kAreaWaypoints = 16015, kAreaMap = 16016, kAreaStores = 16017, kAreaRoomsViewed = 16018,
                        kAreaSounds = 16019, kAreaMinImportance = 16020;
// The campaign
constexpr std::uint32_t kCampaignResource = 16014, kGroupList = 16450, kTimeBeforeDecay = 16499, kJournal = 16504,
                        kAmbientDialogs = 16530, kWorldmap = 16780, kPlotActions = 16840, kEntryAreaList = 17100,
                        kEntryArea = 17101, kEntryPosition = 17102, kEntryOrientation = 17103;
constexpr std::uint32_t kGroupHostiles = 16452;
constexpr std::uint32_t kJActive = 16505, kJComplete = 16506, kJResRef = 16510, kJStoryText = 16511,
                        kJPlotDestinations = 16513, kJConversations = 16516, kJOrphans = 16522, kJGroups = 16525;
constexpr std::uint32_t kQTitle = 16507, kQText = 16508, kQParent = 16509, kQAreaTag = 16512, kQCompleted = 16523,
                        kQGroup = 16524, kQUpdated = 16540, kQOffer = 16541;
constexpr std::uint32_t kPdTag = 16514, kPdGuids = 16515;
constexpr std::uint32_t kConvLines = 16517, kClSpeaker = 16518, kClText = 16519, kClReply = 16520;
constexpr std::uint32_t kJgResRef = 16526, kJgCurrent = 16527, kJgCompleted = 16528, kJgPriority = 16529;
constexpr std::uint32_t kAmbOwner = 16531, kAmbSpeaker = 16532, kAmbResRef = 16533, kAmbLine = 16534;
constexpr std::uint32_t kWmPrimary = 16781, kWmSecondary = 16782, kWmMaps = 16783, kWmGui = 16790, kWmLastPin = 16791,
                        kWmTag = 16784, kWmPlayerLoc = 16785, kWmPins = 16786, kWmPathBefore = 16796,
                        kWmPathAfter = 16797, kWmPinTag = 16787, kWmPinState = 16788, kWmPinRecent = 16789,
                        kWmPinPrevious = 16792, kWmPinLast = 16793, kWmPinName = 16798, kWmPosX = 16794,
                        kWmPosY = 16795;
constexpr std::uint32_t kPaEnabled = 16841, kPaCurrentSet = 16842, kPaList = 16843, kPaId = 16844, kPaState = 16845,
                        kPaCount = 16846, kPaUpdated = 16847;
// The party
constexpr std::uint32_t kPoolMembers = 16204, kSelected = 16270, kPickerGui = 16274, kApprovals = 16275,
                        kApprovalId = 16276, kApprovalLevel = 16277, kLeader = 16278, kItemStorage = 16288,
                        kNewItems = 16290, kAutoLevelDefault = 16291, kQuickbarLocked = 16292, kHoldPositions = 16293,
                        kRunInDriveMode = 16294, kApprovalDesc = 16297, kBackpackSort = 16299, kQbarExpansion = 16310,
                        kPlotManager = 16400, kSeenLines = 16503;
constexpr std::uint32_t kStItem = 16284, kStOwner = 16285, kStSlot = 16286, kStWeaponset = 16287, kNewItemId = 16289;
constexpr std::uint32_t kPlotList = 16401, kPlotGuid = 16402, kPlotFlags1 = 16403, kUnreadCodex = 16521;
// The game's state
constexpr std::uint32_t kEffectStartingId = 16626, kAiMaster = 16636, kWorldTimer = 16700, kWorldDay = 16701,
                        kWorldTime = 16702, kEventQueue = 16630, kEventDay = 16631, kEventTime = 16632,
                        kEventCaller = 16633, kEventTarget = 16634, kEventId = 16635, kEventScript = 16650,
                        kSeType = 16670, kSeCreator = 16671, kSeTarget = 16672, kSeData = 16673, kSeScriptName = 16674,
                        kSeResources = 16675;
constexpr std::uint32_t kWorldDatabase = 16500, kWdGroup = 16501, kWdLastId = 16502;
constexpr std::uint32_t kCheatUsed = 16007, kBuildNumber = 16770, kSaveVersion = 16771;
constexpr std::uint32_t kPlayerChar = 16208, kMapZoom = 16295, kMapLegend = 16296, kTimePlayed = 16298;
} // namespace L

// -- Reading a field into a member, leaving the member's default where the file lacks it

// A whole number or a float. A whole number of the other signedness at the same width is taken too (the
// ids are S32 in most structs and U32 in others), never a wider one: the engine's getters read the raw
// width at the field's offset and convert nothing. True if the field was read.
template <class T>
    requires std::is_arithmetic_v<T>
bool Take(const Struct &s, std::uint32_t label, T &out)
{
    if (const auto v = s.Get<T>(label))
    {
        out = *v;
        return true;
    }
    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
    {
        using Other = std::conditional_t<std::is_signed_v<T>, std::make_unsigned_t<T>, std::make_signed_t<T>>;
        if (const auto v = s.Get<Other>(label))
        {
            out = static_cast<T>(*v);
            return true;
        }
    }
    return false;
}

bool Take(const Struct &s, std::uint32_t label, std::wstring &out)
{
    if (auto v = s.String(label))
    {
        out = std::move(*v);
        return true;
    }
    return false;
}

bool Take(const Struct &s, std::uint32_t label, Vec3 &out)
{
    const auto f = s.Floats(label);
    if (f.size() != 3)
    {
        return false;
    }
    out = {f[0], f[1], f[2]};
    return true;
}

bool Take(const Struct &s, std::uint32_t label, Quat &out)
{
    const auto f = s.Floats(label);
    if (f.size() != 4)
    {
        return false;
    }
    out = {f[0], f[1], f[2], f[3]};
    return true;
}

// Into a field that stays none where the save lacks it.
template <class T> bool Take(const Struct &s, std::uint32_t label, std::optional<T> &out)
{
    T value{};
    if (!Take(s, label, value))
    {
        return false;
    }
    out = std::move(value);
    return true;
}

// A talk-table string's reference.
void TakeStrref(const Struct &s, std::uint32_t label, std::optional<std::uint32_t> &out)
{
    if (const auto v = s.Tlk(label))
    {
        out = v->strref;
    }
}

void TakeStrref(const Struct &s, std::uint32_t label, std::uint32_t &out)
{
    if (const auto v = s.Tlk(label))
    {
        out = v->strref;
    }
}

// Whether the struct's template has the field: "present" for the engine's getters, which then read it.
bool Has(const Struct &s, std::uint32_t label)
{
    return s.Shape().Find(label) != nullptr;
}

void TakeNumbers(const Struct &s, std::uint32_t label, std::vector<std::uint32_t> &out)
{
    out = s.Numbers(label);
}

void TakeNumbers(const Struct &s, std::uint32_t label, std::vector<std::int32_t> &out)
{
    const auto v = s.Numbers(label);
    out.assign(v.begin(), v.end());
}

void TakeNumbers(const Struct &s, std::uint32_t label, std::optional<std::vector<std::int32_t>> &out)
{
    if (Has(s, label))
    {
        std::vector<std::int32_t> v;
        TakeNumbers(s, label, v);
        out = std::move(v);
    }
}

template <class T> std::vector<T> Elements(std::span<const std::byte> bytes)
{
    std::vector<T> out(bytes.size() / sizeof(T));
    if (!out.empty())
    {
        std::memcpy(out.data(), bytes.data(), out.size() * sizeof(T));
    }
    return out;
}

// A list of 16-byte elements as vectors, the fourth float of each dropped (a Vector3 list's is junk).
std::vector<Vec3> Vectors(std::span<const std::byte> bytes)
{
    std::vector<Vec3> out;
    for (std::size_t at = 0; at + 16 <= bytes.size(); at += 16)
    {
        float f[3];
        std::memcpy(f, bytes.data() + at, sizeof f);
        out.push_back({f[0], f[1], f[2]});
    }
    return out;
}

std::vector<Quat> Quaternions(std::span<const std::byte> bytes)
{
    std::vector<Quat> out;
    for (std::size_t at = 0; at + 16 <= bytes.size(); at += 16)
    {
        float f[4];
        std::memcpy(f, bytes.data() + at, sizeof f);
        out.push_back({f[0], f[1], f[2], f[3]});
    }
    return out;
}

// -- Filling a struct from a GFF struct, and a list of them

void Fill(const Struct &s, DataArrays &out);
void Fill(const Struct &s, Effect &out);
void Fill(const Struct &s, Command &out);
void Fill(const Struct &s, Subaction &out);
void Fill(const Struct &s, ActionQueue &out);
void Fill(const Struct &s, Item &out);
void Fill(const Struct &s, Stat &out);
void Fill(const Struct &s, Quickslot &out);
void Fill(const Struct &s, Stats &out);
void Fill(const Struct &s, Appearance &out);
void Fill(const Struct &s, ClassRank &out);
void Fill(const Struct &s, TacticEntry &out);
void Fill(const Struct &s, Tactics &out);
void Fill(const Struct &s, Creature &out);
void Fill(const Struct &s, Placeable &out);
void Fill(const Struct &s, Sound &out);
void Fill(const Struct &s, Trigger &out);
void Fill(const Struct &s, AreaOfEffect &out);
void Fill(const Struct &s, Waypoint &out);
void Fill(const Struct &s, Store &out);
void Fill(const Struct &s, Area &out);
void Fill(const Struct &s, Quest &out);

template <class T> void List(const Struct &s, std::uint32_t label, std::vector<T> &out)
{
    for (const Struct &child : s.Structs(label))
    {
        T value;
        Fill(child, value);
        out.push_back(std::move(value));
    }
}

template <class T> void List(const Struct &s, std::uint32_t label, std::optional<std::vector<T>> &out)
{
    if (Has(s, label))
    {
        std::vector<T> v;
        List(s, label, v);
        out = std::move(v);
    }
}

template <class T> std::optional<T> One(const Struct &s, std::uint32_t label)
{
    const auto child = s.Child(label);
    if (!child)
    {
        return std::nullopt;
    }
    T value;
    Fill(*child, value);
    return value;
}

// A ScriptVarTable: a generic list whose elements' templates say the type (SVI1 int, SVF1 float, SVS1
// string, SVO1 object, SVR1 resource). [file]
void Variables(const Struct &s, std::vector<Variable> &out)
{
    for (const Struct &v : s.Structs(L::kVariables))
    {
        const std::string &kind = v.Label();
        Variable variable;
        Take(v, L::kVarName, variable.name);
        if (kind == "SVI1")
        {
            std::int32_t value = 0;
            Take(v, L::kVarValue, value);
            variable.value = value;
        }
        else if (kind == "SVF1")
        {
            float value = 0;
            Take(v, L::kVarValue, value);
            variable.value = value;
        }
        else if (kind == "SVS1")
        {
            std::wstring value;
            Take(v, L::kVarValue, value);
            variable.value = std::move(value);
        }
        else if (kind == "SVO1")
        {
            ObjectId value;
            Take(v, L::kVarValue, value.id);
            variable.value = value;
        }
        else if (kind == "SVR1")
        {
            ResRef value;
            Take(v, L::kVarValue, value.name);
            variable.value = std::move(value);
        }
        else
        {
            continue;
        }
        out.push_back(std::move(variable));
    }
}

// A DTA1 [exe 0x00A5BAD0]: each list is read only when present with elements, in the order ints, floats,
// bools, object ids, quaternions, vectors, strings; the arrays of the effect, command or event are emptied
// and refilled. [raw] for an effect's engine data and an event's data: what the slots mean is the type's.
void Fill(const Struct &s, DataArrays &out)
{
    TakeNumbers(s, L::kArrInt, out.ints);
    out.floats = Elements<float>(s.ListBytes(L::kArrFloat, Type::F32));
    out.bools = Elements<std::uint8_t>(s.ListBytes(L::kArrBool, Type::U8));
    TakeNumbers(s, L::kArrOid, out.objects);
    out.strings = s.Strings(L::kArrString);
    out.vectors = Vectors(s.ListBytes(L::kArrVector, Type::Vector3));
    out.quaternions = Quaternions(s.ListBytes(L::kArrQuaternion, Type::Quaternion));
}

// A GME1 [exe 0x00A656F0, held by 0x00A64B10]: a new effect (ctor 0x00A652A0, creator -1, resource2 -1)
// with the save laid over it; nothing is applied and the object's own list is not cleared first.
void Fill(const Struct &s, Effect &out)
{
    Take(s, L::kEffId, out.id);
    Take(s, L::kEffType, out.type);
    Take(s, L::kEffDurationType, out.durationType);
    Take(s, L::kEffDuration, out.duration);
    Take(s, L::kEffTimeIndex, out.timeIndex);
    Take(s, L::kEffAnimation, out.animation);
    Take(s, L::kEffPriority, out.priority);
    Take(s, L::kEffCreator, out.creator);
    Take(s, L::kEffAbility, out.abilityId);
    if (const auto d = s.Child(L::kEffEngineData))
    {
        Fill(*d, out.engineData);
    }
    Take(s, L::kEffResource2, out.resource2);
    Take(s, L::kEffFlags, out.flags);
    if (const auto d = s.Child(L::kEffDataarray))
    {
        Fill(*d, out.data);
    }
}

// A CMD1 [exe 0x00744DF0]. SavegameCommandID (16723) is not read: the command gets a new id. An object's
// commands and action queue are restored only on a saved game's load [exe 0x00698ED0, the loading state's
// byte +0x68], not when an area is returned to.
void Fill(const Struct &s, Command &out)
{
    Take(s, L::kCmdCommandId, out.commandId);
    Take(s, L::kCmdId, out.id);
    Take(s, L::kCmdStatic, out.isStatic);
    if (const auto d = s.Child(L::kCmdData))
    {
        Fill(*d, out.data);
    }
    Take(s, L::kCmdPlayerIssued, out.playerIssued);
}

// A SAC1 [exe 0x00744FC0], every default the sub-action's own value.
void Fill(const Struct &s, Subaction &out)
{
    Take(s, L::kSubId, out.id);
    Take(s, L::kSubCore, out.coreSubaction);
    Take(s, L::kSubInterruptable, out.coreInterruptable);
    Take(s, L::kSubTimeIndex, out.timeIndex);
    Take(s, L::kSubLastTimeIndex, out.lastTimeIndex);
    Take(s, L::kSubLength, out.length);
    Take(s, L::kSubStartTime, out.startTime);
    if (const auto d = s.Child(L::kSubData))
    {
        Fill(*d, out.data);
    }
}

void Fill(const Struct &s, ActionQueue &out)
{
    out.current = One<Command>(s, L::kCurrentCommand);
    List(s, L::kSubactionList, out.subactions);
}

// An ITL1 [exe 0x00758340, listed by 0x007626C0]: an item is rebuilt from its ObjectID and TemplateResRef
// (an entry without a resref is skipped) and the save laid over its template. The property lists are read
// only inside ItemAbilityID's presence, and ItemSubitemsResRefs makes new items from its resrefs. The
// Position is the item's place: (slot, weapon set, 0) for equipment [exe 0x00797420, truncated to whole
// numbers; the slot is an INVENTORY_SLOT_* id], and for a backpack or plot item x and y with slot 0xFF
// (x counts from 0, y is the item's attribute at +0x600, not identified: [raw]). 16237 [raw]: the item's
// byte at +0x46C, values 0, 4, 26, 36, 38, 45 seen.
void Fill(const Struct &s, Item &out)
{
    Take(s, L::kTemplateResRef, out.templateResRef);
    Take(s, L::kPosition, out.position);
    Take(s, L::kObjectId, out.id);
    Take(s, L::kItemCost, out.cost);
    Take(s, L::kItemStackSize, out.stackSize);
    // The property lists are read only when ItemAbilityID is present [exe 0x00758340].
    if (Has(s, L::kItemAbilityId))
    {
        Take(s, L::kItemAbilityId, out.abilityId);
        TakeNumbers(s, L::kItemProperties, out.properties);
        TakeNumbers(s, L::kItemPowers, out.propertyPowers);
        TakeNumbers(s, L::kItemEffectIds, out.propertyEffectIds);
        TakeNumbers(s, L::kItemVfxIds, out.propertyVfxIds);
    }
    out.subitemResRefs = s.Strings(L::kItemSubitems);
    Take(s, L::kItemRecipe, out.craftingRecipeId);
    Take(s, L::kItemBaseCost, out.baseCost);
    Take(s, L::kObjectTag, out.tag);
    Take(s, L::kItemDroppable, out.droppable);
    Take(s, L::kItemDamaged, out.damaged);
    Take(s, L::kItemIrremovable, out.irremovable);
    Take(s, L::kItemIndestructible, out.indestructible);
    Take(s, L::kItemMaterial, out.materialType);
    Take(s, L::kItemStealable, out.stealable);
    Take(s, L::kItemInfinite, out.infinite);
    Take(s, L::kItemVfxPropertyId, out.currentVfxPropertyId);
    Take(s, L::kItemVfxPropertyPower, out.currentVfxPropertyPower);
    Take(s, L::kItemUnnamed, out.field16237);
    Take(s, L::kObjectPlot, out.plot);
    List(s, L::kEffectList, out.effects);
    Variables(s, out.variables);
}

// An SMPL, ATTR or DPLT, told apart by the struct's label; each reads the fields its template has.
// An SMPL, ATTR or DPLT [exe 0x00795E30 into 0x0079D1E0, 0x0079D200, 0x0079D230]: the engine restores the
// element into the property of its row of properties.gda (16353, u16), and that property's class decides
// which fields it reads, the label never consulted. A property not in the list keeps the template's value.
void Fill(const Struct &s, Stat &out)
{
    const std::string &kind = s.Label();
    out.kind = Stat::Kind::Simple;
    if (kind == "DPLT")
    {
        out.kind = Stat::Kind::Depletable;
    }
    else if (kind == "ATTR")
    {
        out.kind = Stat::Kind::Attribute;
    }
    Take(s, L::kStatIndex, out.index);
    Take(s, L::kStatBase, out.base);
    Take(s, L::kStatModifier, out.modifier);
    Take(s, L::kStatCurrent, out.current);
    Take(s, L::kStatComRegen, out.combatRegen);
    Take(s, L::kStatRegen, out.regen);
}

void Fill(const Struct &s, Quickslot &out)
{
    Take(s, L::kQsAbility, out.ability);
    Take(s, L::kQsItemTag, out.itemTag);
    Take(s, L::kQsTemplate, out.templateResRef);
    Take(s, L::kQsNumber, out.number);
}

// A CRS1 or CPS1 [exe 0x0071EDC0]: the property lists 16350 (all elements), 16351 (heroic) and 16352 (the
// party-wide, read for the hero only), then talents, spells and skills (added as the template's are, with no
// prerequisite check), then the modal abilities (replacing the set), then the quick bar [exe 0x0062C3D0]
// of a CPS1. 16339 and 16340 are read at 0x0062C661 and 0x0062C6B6 and [raw].
void Fill(const Struct &s, Stats &out)
{
    TakeNumbers(s, L::kSpellList, out.spells);
    TakeNumbers(s, L::kTalentList, out.talents);
    TakeNumbers(s, L::kSkillList, out.skills);
    List(s, L::kStatList, out.stats);
    List(s, L::kHeroicStatList, out.heroicStats);
    List(s, L::kHeroicPartyStatList, out.heroicPartyStats);
    TakeNumbers(s, L::kModalAbilities, out.modalAbilities);
    constexpr std::uint32_t slots[] = {L::kQuickslots, L::kQuickslots1, L::kQuickslots2, L::kQuickslots3,
                                       L::kQuickslots4};
    for (std::size_t i = 0; i < out.quickslots.size(); ++i)
    {
        List(s, slots[i], out.quickslots[i]);
    }
    out.hasQuickbar = s.Label() == "CPS1";
    Take(s, L::kCurrentQbar, out.currentQbar);
    Take(s, L::kLockQbar, out.lockQbar);
    Take(s, L::kAutoLevelUp, out.autoLevelUp);
    Take(s, L::kUnnamed16339, out.field16339);
    Take(s, L::kUnnamed16340, out.field16340);
}

// An APP1, APL1 or APC1 [exe 0x0074A890]: whatever labels exist are read; the heraldry list sets seven
// slots whatever its length; the morph name is the hero's. The model is built later [exe 0x006C0DE0].
void Fill(const Struct &s, Appearance &out)
{
    out.label = s.Label();
    Take(s, L::kAppType, out.type);
    Take(s, L::kAppGender, out.gender);
    Take(s, L::kAppGore, out.gore);
    Take(s, L::kAppDecapitated, out.decapitated);
    if (Has(s, L::kAppHeraldry))
    {
        out.heraldryVariation = Elements<std::uint8_t>(s.ListBytes(L::kAppHeraldry, Type::U8));
    }
    Take(s, L::kAppOriginalType, out.originalType);
    Take(s, L::kAppMorphName, out.morphName);
}

void Fill(const Struct &s, ClassRank &out)
{
    Take(s, L::kClassId, out.classId);
    Take(s, L::kClassRank, out.rank);
}

// A PTE1 [exe 0x007ADD20]: the item tag and resref are read together, only if the tag's field is present;
// the condition object defaults to -1. Not read by the engine: 16820, 16830, 16831.
void Fill(const Struct &s, TacticEntry &out)
{
    Take(s, L::kTeTargetObject, out.targetObject);
    Take(s, L::kTeConditionObject, out.conditionObject);
    Take(s, L::kTeEnabled, out.enabled);
    Take(s, L::kTeTarget, out.target);
    Take(s, L::kTeCondition, out.condition);
    Take(s, L::kTeCommand, out.command);
    Take(s, L::kTeParam, out.commandParam);
    Take(s, L::kTeItemTag, out.commandItemTag);
    Take(s, L::kTeItemResRef, out.commandItemResRef);
}

// A PTT1 [exe 0x007AFBF0]: read only when the creature's SavegameTacticsHasTable is non-zero; the entry list
// replaces the table's, and the block of dirty, preset and custom lists is skipped when 16832 is absent.
void Fill(const Struct &s, Tactics &out)
{
    Take(s, L::kTacticsEnabled, out.enabled);
    List(s, L::kTacticsList, out.entries);
    Take(s, L::kTacticsDirty, out.dirty);
    Take(s, L::kTacticsPresetType, out.presetType);
    Take(s, L::kTacticsPresetIndex, out.presetIndex);
    out.presetList = Elements<std::uint8_t>(s.ListBytes(L::kTacticsPresetList, Type::U8));
    for (const Struct &list : s.Structs(L::kTacticsCustomList))
    {
        std::vector<TacticEntry> entries;
        List(list, L::kTacticsList, entries);
        out.custom.push_back(std::move(entries));
    }
}

// The header ids of a CMA1 list (a minimised skill or talent group).
std::vector<std::int16_t> Headers(const Struct &s, std::uint32_t label)
{
    std::vector<std::int16_t> out;
    for (const Struct &h : s.Structs(label))
    {
        std::int16_t id = 0;
        Take(h, L::kHeaderId, id);
        out.push_back(id);
    }
    return out;
}

void TakePortrait(const Struct &s, Creature &out)
{
    if (!s.Get<float>(L::kPortraitPitch))
    {
        return;
    }
    Portrait p;
    Take(s, L::kPortraitPitch, p.pitch);
    Take(s, L::kPortraitYaw, p.yaw);
    Take(s, L::kPortraitTint, p.tint);
    Take(s, L::kPortraitExpression, p.expression);
    Take(s, L::kPortraitDistance, p.distance);
    Take(s, L::kPortraitPositionH, p.positionH);
    Take(s, L::kPortraitPositionV, p.positionV);
    out.portrait = p;
}

// The creature [exe 0x006ABDC0 loads the whole .utc, then 0x006BE980 restores the save over it, 0x00698ED0
// first]. The order in the engine: the common read (Position, Orientation, looping animation, effects,
// Active, Immortal, Plot, variables, event script, name, Tag, TeamID, interaction radius, commands, action
// queue), importance (16263, filtered against the area's 16020 by 0x0067A170), the party fields (16203
// joins the party by follower state, 16279, 16280 to 16282 read together), stats (only if the template's
// Combatant, +0xBD4, is 0), tactics, then group, stealth, plot giver, ghost, statue, items scaled,
// trackable, race, interactive, controllable, class ranks, equipment (16218, 16219), backpack (16210,
// 16211, 16212, 16226), body bag, rank, treasure group, appearance, sound sets and portrait,
// can-change-equipment, map ally, minimised headers, heartbeat (only outside a save's load), roam centre
// and radius, pool name and availability, no-permanent-death and time since death (raw into +0xC88, ms
// of a corpse). The hero's morph (16950) is read by the model builder [exe 0x006C0DE0] from the PlayerChar
// struct, not by the restore. Absent: the template's value.
//
// The creature's struct is CRP1, CRL1 or CRE1: one reader, a field the template lacks keeping its default.
// The hero's and a pool member's differ from an area's in the party fields, the name, the tactics and (the
// hero) the morph, portrait and sound sets.
void Fill(const Struct &s, Creature &out)
{
    const std::string &kind = s.Label();
    out.kind = Creature::Kind::Area;
    if (kind == "CRP1")
    {
        out.kind = Creature::Kind::Hero;
    }
    else if (kind == "CRL1")
    {
        out.kind = Creature::Kind::Pool;
    }
    Take(s, L::kTemplateResRef, out.templateResRef);
    Take(s, L::kObjectTag, out.tag);
    Take(s, L::kPosition, out.position);
    Take(s, L::kOrientation, out.orientation);
    Take(s, L::kObjectId, out.id);
    Take(s, L::kObjectActive, out.active);
    Take(s, L::kPartyMembers, out.partyMembers);
    Take(s, L::kNonPartyMembers, out.nonPartyMembers);
    Take(s, L::kPartySubstate, out.partySubstate);
    Take(s, L::kPartyLocked, out.partyLocked);
    Take(s, L::kPartyFollow, out.partyFollow);
    out.stats = One<Stats>(s, L::kCreatureStats);
    List(s, L::kBackpack, out.backpack);
    List(s, L::kPlotItems, out.plotItems);
    Take(s, L::kMoney, out.money);
    Take(s, L::kEquipmentActiveSet, out.activeSet);
    List(s, L::kEquipmentItems, out.equipment);
    Take(s, L::kObjectImmortal, out.immortal);
    Take(s, L::kObjectEventScript, out.eventScript);
    Take(s, L::kMaxItems, out.maxItems);
    TakeNumbers(s, L::kCraftingRecipes, out.craftingRecipes);
    Take(s, L::kObjectPlot, out.plot);
    Take(s, L::kObjectTreasureGroup, out.treasureGroup);
    TakeStrref(s, L::kObjectName, out.nameStrref);
    Take(s, L::kLoopingAnimation, out.loopingAnimation);
    Take(s, L::kInteractionRadius, out.interactionRadius);
    Take(s, L::kImportance, out.importance);
    out.appearance = One<Appearance>(s, L::kAppearance);
    TakePortrait(s, out);
    Take(s, L::kGroupId, out.groupId);
    Take(s, L::kTeamId, out.teamId);
    Take(s, L::kStealth, out.stealth);
    Take(s, L::kIsPlotGiver, out.plotGiver);
    Take(s, L::kCanLevelUp, out.canLevelUp);
    Take(s, L::kTrackable, out.trackable);
    Take(s, L::kControllable, out.controllable);
    Take(s, L::kInteractive, out.interactive);
    Take(s, L::kRace, out.race);
    Take(s, L::kCanChangeEquipment, out.canChangeEquipment);
    List(s, L::kClassRankList, out.classRanks);
    Take(s, L::kIsGhost, out.isGhost);
    Take(s, L::kShowAsAllyOnMap, out.showAsAllyOnMap);
    Take(s, L::kIsStatue, out.isStatue);
    out.minimizedSkillHeaders = Headers(s, L::kMinimizedSkills);
    out.minimizedTalentHeaders = Headers(s, L::kMinimizedTalents);
    Take(s, L::kItemsScaled, out.itemsScaled);
    Take(s, L::kHeartbeat, out.heartbeatInterval);
    Take(s, L::kRoamRadius, out.roamRadius);
    Take(s, L::kRoamCenter, out.roamCenter);
    Take(s, L::kPoolName, out.poolName);
    Take(s, L::kPoolAvailable, out.poolAvailable);
    Take(s, L::kNoPermDeath, out.noPermDeath);
    Take(s, L::kTimeSinceDeath, out.timeSinceDeath);
    Take(s, L::kBodybagId, out.bodybagId);
    Take(s, L::kCreatureRank, out.rank);
    List(s, L::kEffectList, out.effects);
    List(s, L::kCommandList, out.commands);
    out.actionQueue = One<ActionQueue>(s, L::kCurrentActionQueue);
    Take(s, L::kTacticsHasTable, out.tacticsHasTable);
    out.tactics = One<Tactics>(s, L::kTacticsTable);
    out.playerMorph = Elements<std::uint8_t>(s.ListBytes(L::kPlayerMorph, Type::U8));
    Take(s, L::kPlayerSoundSet, out.playerSoundSet);
    Take(s, L::kDefaultSoundSet, out.defaultSoundSet);
    Variables(s, out.variables);
}

// A PLC1 [exe 0x00683380 lists, 0x0074EFE0 loads the .utp, 0x0074FE30 restores]: the state (16100) is stored
// first and applied after the template's post-load; max health before health; the items through the
// container [exe 0x007CA990]. RoomID1 and RoomID2 are not saved: the engine takes them from the .are's
// PlaceableList by matching X and Y.
void Fill(const Struct &s, Placeable &out)
{
    Take(s, L::kTemplateResRef, out.templateResRef);
    Take(s, L::kObjectTag, out.tag);
    Take(s, L::kPosition, out.position);
    Take(s, L::kOrientation, out.orientation);
    Take(s, L::kObjectId, out.id);
    Take(s, L::kPlaceableState, out.state);
    Take(s, L::kPlaceableUseable, out.useable);
    Take(s, L::kObjectActive, out.active);
    Take(s, L::kObjectImmortal, out.immortal);
    Take(s, L::kObjectEventScript, out.eventScript);
    List(s, L::kItems, out.items);
    Take(s, L::kMaxItems, out.maxItems);
    Take(s, L::kObjectPlot, out.plot);
    Take(s, L::kObjectHealth, out.health);
    Take(s, L::kObjectMaxHealth, out.maxHealth);
    Take(s, L::kObjectRank, out.rank);
    Take(s, L::kObjectTreasureGroup, out.treasureGroup);
    TakeStrref(s, L::kObjectName, out.nameStrref);
    Take(s, L::kLootableAppearance, out.lootableCreatureAppearanceType);
    Take(s, L::kObjectPickLock, out.pickLock);
    Take(s, L::kTrapDetected, out.trapDetected);
    Take(s, L::kDetectCheck, out.detectCheck);
    Take(s, L::kDisarmCheck, out.disarmCheck);
    Take(s, L::kInteractionRadius, out.interactionRadius);
    Take(s, L::kTeamId, out.teamId);
    Take(s, L::kIsPlotGiver, out.plotGiver);
    Take(s, L::kIsBodybag, out.isBodybag);
    Take(s, L::kLootableObject, out.lootableObjectId);
    List(s, L::kEffectList, out.effects);
    List(s, L::kCommandList, out.commands);
    out.actionQueue = One<ActionQueue>(s, L::kCurrentActionQueue);
    Variables(s, out.variables);
}

// An SND1 [exe 0x007C2BE0, listed by 0x0067DA40]: no template, every default 0, the three position floats
// override the base Position and SoundTag overrides ObjectTag.
void Fill(const Struct &s, Sound &out)
{
    Take(s, L::kPosition, out.position);
    Take(s, L::kObjectId, out.id);
    Take(s, L::kObjectTag, out.tag);
    Take(s, L::kSoundTag, out.soundTag);
    Take(s, L::kSoundActive, out.active);
    Take(s, L::kSoundName, out.name);
    Take(s, L::kSoundX, out.soundPosition.x);
    Take(s, L::kSoundY, out.soundPosition.y);
    Take(s, L::kSoundZ, out.soundPosition.z);
    Take(s, L::kSoundOx, out.soundOrientation.x);
    Take(s, L::kSoundOy, out.soundOrientation.y);
    Take(s, L::kSoundOz, out.soundOrientation.z);
    Take(s, L::kSoundOw, out.soundOrientation.w);
    Take(s, L::kSoundVolume, out.volume);
    Take(s, L::kSoundPitch, out.pitch);
    Take(s, L::kSoundFadeIn, out.fadeIn);
    Take(s, L::kSoundFadeOut, out.fadeOut);
    Take(s, L::kSoundMaxDistance, out.maxDistanceMult);
    Take(s, L::kSoundConeInside, out.coneInside);
    Take(s, L::kSoundConeOutside, out.coneOutside);
    Take(s, L::kSoundConeVolume, out.coneVolume);
    Take(s, L::kSoundPriority, out.priority);
    Take(s, L::kSoundOccludable, out.occludable);
}

// A trigger [exe 0x00687AC0 lists, the class by SavegameAreaTriggerType 16112 through 0x0067A690, then
// 0x0079A7E0 and kin]: the geometry points get the saved Position added by the engine; reverb preset for
// TRAR and TRAV, the sounds for TRAV, the music states for TRAM. Written and never read: ObjectID, 16103 to
// 16107 and 16110.
void Fill(const Struct &s, Trigger &out)
{
    out.label = s.Label();
    Take(s, L::kTemplateResRef, out.templateResRef);
    Take(s, L::kObjectTag, out.tag);
    Take(s, L::kPosition, out.position);
    Take(s, L::kOrientation, out.orientation);
    Take(s, L::kObjectId, out.id);
    out.geometry = Vectors(s.ListBytes(L::kTrigGeometry, Type::Vector3));
    Take(s, L::kTrigType, out.type);
    Take(s, L::kObjectActive, out.active);
    Take(s, L::kObjectEventScript, out.eventScript);
    List(s, L::kCommandList, out.commands);
    out.actionQueue = One<ActionQueue>(s, L::kCurrentActionQueue);
    Variables(s, out.variables);
    Take(s, L::kTrigDetectable, out.detectable);
    Take(s, L::kTrigDisarmable, out.disarmable);
    Take(s, L::kTrigDetectCheck, out.detectCheck);
    Take(s, L::kTrigDisarmCheck, out.disarmCheck);
    Take(s, L::kTrigLastDisarmed, out.lastDisarmed);
    Take(s, L::kTrigLoadScreen, out.loadScreen);
    Take(s, L::kTrigReverb, out.reverbPreset);
    List(s, L::kTrigSounds, out.sounds);
    Take(s, L::kTrigMusicEnter, out.musicEnterState);
    Take(s, L::kTrigMusicExit, out.musicExitState);
    Take(s, L::kTrigMusicEnterDelay, out.musicEnterDelay);
    Take(s, L::kTrigMusicExitDelay, out.musicExitDelay);
}

// An AOE1 [exe 0x007E5060, listed by 0x0067E0D0]: no template; an absent Flags takes the AbilityID the
// engine has just read (a quirk of its code).
void Fill(const Struct &s, AreaOfEffect &out)
{
    Take(s, L::kPosition, out.position);
    Take(s, L::kOrientation, out.orientation);
    Take(s, L::kObjectId, out.id);
    Take(s, L::kObjectEventScript, out.eventScript);
    Take(s, L::kObjectTag, out.tag);
    Take(s, L::kAoeId, out.aoeId);
    Take(s, L::kAoeShape, out.shape);
    Take(s, L::kAoeRadius, out.radius);
    Take(s, L::kAoeWidth, out.width);
    Take(s, L::kAoeLength, out.length);
    Take(s, L::kAoeCreator, out.creator);
    Take(s, L::kAoeDuration, out.duration);
    Take(s, L::kAoeDurationType, out.durationType);
    Take(s, L::kAoeLinked, out.linked);
    Take(s, L::kAoeAbility, out.abilityId);
    // Flags defaults to the member the AbilityID was just read into [exe 0x007E5060], so an absent Flags
    // takes the AbilityID.
    if (!Take(s, L::kAoeFlags, out.flags))
    {
        out.flags = out.abilityId;
    }
    Take(s, L::kAoeStationary, out.stationary);
}

// A WPT1 [exe 0x007C4520, listed by 0x0067DE40]: nothing after MapNote is read when it is 0; the note's
// text is the tlkstring if present and not null, else the string (16712).
void Fill(const Struct &s, Waypoint &out)
{
    Take(s, L::kPosition, out.position);
    Take(s, L::kOrientation, out.orientation);
    Take(s, L::kObjectId, out.id);
    Take(s, L::kObjectActive, out.active);
    Take(s, L::kObjectTag, out.tag);
    Take(s, L::kWpMapNote, out.mapNote);
    Take(s, L::kWpEnabled, out.mapNoteEnabled);
    Take(s, L::kWpText, out.mapNoteText);
    Take(s, L::kWpType, out.mapNoteType);
    TakeStrref(s, L::kWpLocText, out.mapNoteTextStrref);
}

// An STO1 [exe 0x007EC0B0, listed by 0x0067AC00]: the ObjectID is not read (a store gets a new id); the
// will-not-buy and will-only-buy ids are appended to the template's.
void Fill(const Struct &s, Store &out)
{
    Take(s, L::kTemplateResRef, out.templateResRef);
    Take(s, L::kPosition, out.position);
    Take(s, L::kObjectId, out.id);
    Take(s, L::kStoreMarkDown, out.markDown);
    Take(s, L::kStoreMarkUp, out.markUp);
    Take(s, L::kStoreGold, out.gold);
    Take(s, L::kStoreMaxBuy, out.maxBuyPrice);
    TakeNumbers(s, L::kStoreWillNotBuy, out.willNotBuy);
    TakeNumbers(s, L::kStoreWillOnlyBuy, out.willOnlyBuy);
    List(s, L::kStoreItemList, out.items);
    Take(s, L::kObjectEventScript, out.eventScript);
    Take(s, L::kObjectTag, out.tag);
    Take(s, L::kMaxItems, out.maxItems);
}

// An ARE1 [exe 0x006FABB0 finds it by Tag, 0x0068B1B0 and 0x006882C0 load it]: an ObjectID of -1 or absent
// makes the engine ignore the struct (Area::Used). The lists are loaded in the order waypoints,
// placeables, triggers, creatures, sounds, stores, AOEs, effects; rooms viewed go into the area's seen
// list; the map (16016) is taken only if its size is the grid's [exe 0x007C9620] and is merged with the
// areas linked by LinkAreaMiniMaps; 16020 is read by the creature loader [exe 0x00683820].
void Fill(const Struct &s, Area &out)
{
    Take(s, L::kTag, out.tag);
    Take(s, L::kObjectId, out.id);
    List(s, L::kAreaPlaceables, out.placeables);
    List(s, L::kAreaCreatures, out.creatures);
    List(s, L::kAreaTriggers, out.triggers);
    List(s, L::kAreaAoes, out.aoes);
    List(s, L::kAreaWaypoints, out.waypoints);
    out.map.bits = Elements<std::uint8_t>(s.ListBytes(L::kAreaMap, Type::U8));
    List(s, L::kAreaStores, out.stores);
    TakeNumbers(s, L::kAreaRoomsViewed, out.roomsViewed);
    List(s, L::kAreaSounds, out.sounds);
    Take(s, L::kAreaMinImportance, out.minCreatureImportance);
    List(s, L::kEffectList, out.effects);
    Variables(s, out.variables);
}

// -- The campaign

void Fill(const Struct &s, Quest &out)
{
    TakeStrref(s, L::kQTitle, out.titleStrref);
    TakeStrref(s, L::kQText, out.textStrref);
    Take(s, L::kQParent, out.parentPlot);
    Take(s, L::kJResRef, out.resRef);
    Take(s, L::kQAreaTag, out.areaTag);
    Take(s, L::kQCompleted, out.completed);
    Take(s, L::kQGroup, out.group);
    Take(s, L::kQUpdated, out.updated);
    Take(s, L::kQOffer, out.offerId);
}

// A JRNL [exe 0x006F7290]: the groups first, then the active quests (AddPlot), the completed (AddPlot with
// completed forced, then CompletePlot) and the orphans, the plot destinations, the conversations in reverse
// list order, the story text, and last SetActivePlot with SavegameJournalResRef (16510, at 0x006F81F5), which
// picks the entry of that ResRef or none. Quest.updated is read for active quests only.
void Fill(const Struct &s, Journal &out)
{
    List(s, L::kJActive, out.active);
    List(s, L::kJComplete, out.completed);
    List(s, L::kJOrphans, out.orphans);
    Take(s, L::kJResRef, out.activeResRef);
    TakeStrref(s, L::kJStoryText, out.storyTextStrref);
    for (const Struct &d : s.Structs(L::kJPlotDestinations))
    {
        PlotDestination destination;
        Take(d, L::kPdTag, destination.tag);
        destination.guids = d.Strings(L::kPdGuids);
        out.plotDestinations.push_back(std::move(destination));
    }
    for (const Struct &c : s.Structs(L::kJConversations))
    {
        std::vector<ConversationLine> lines;
        for (const Struct &line : c.Structs(L::kConvLines))
        {
            ConversationLine l;
            Take(line, L::kClSpeaker, l.speaker);
            Take(line, L::kClText, l.text);
            Take(line, L::kClReply, l.reply);
            lines.push_back(l);
        }
        out.conversations.push_back(std::move(lines));
    }
    for (const Struct &g : s.Structs(L::kJGroups))
    {
        JournalGroup group;
        Take(g, L::kJgResRef, group.resRef);
        Take(g, L::kJgCurrent, group.openInCurrent);
        Take(g, L::kJgCompleted, group.openInCompleted);
        Take(g, L::kJgPriority, group.priority);
        out.groups.push_back(std::move(group));
    }
}

std::vector<std::pair<float, float>> TravelPath(const Struct &s, std::uint32_t label)
{
    std::vector<std::pair<float, float>> out;
    for (const Struct &p : s.Structs(label))
    {
        float x = 0;
        float y = 0;
        Take(p, L::kWmPosX, x);
        Take(p, L::kWmPosY, y);
        out.emplace_back(x, y);
    }
    return out;
}

// A WRLD [exe 0x00701250]: a map or pin whose tag matches none of the game's is dropped by the engine.
void Fill(const Struct &s, WorldMap &out)
{
    Take(s, L::kWmPrimary, out.primaryMap);
    Take(s, L::kWmSecondary, out.secondaryMap);
    for (const Struct &m : s.Structs(L::kWmMaps))
    {
        WorldMapMap map;
        Take(m, L::kObjectId, map.id);
        Take(m, L::kWmTag, map.tag);
        Take(m, L::kWmPlayerLoc, map.playerLocation);
        for (const Struct &p : m.Structs(L::kWmPins))
        {
            WorldMapPin pin;
            Take(p, L::kObjectId, pin.id);
            Take(p, L::kWmPinTag, pin.tag);
            Take(p, L::kWmPinState, pin.state);
            Take(p, L::kWmPinRecent, pin.recentlyActivated);
            Take(p, L::kWmPinPrevious, pin.activatedPreviously);
            Take(p, L::kWmPinLast, pin.lastState);
            TakeStrref(p, L::kWmPinName, pin.nameStrref);
            map.pins.push_back(std::move(pin));
        }
        map.travelPathBefore = TravelPath(m, L::kWmPathBefore);
        map.travelPathAfter = TravelPath(m, L::kWmPathAfter);
        out.maps.push_back(std::move(map));
    }
    Take(s, L::kWmGui, out.guiStatus);
    Take(s, L::kWmLastPin, out.lastPinClicked);
}

// A PTAA [exe 0x006FDCF0]: an action whose id is not known is skipped.
void Fill(const Struct &s, PlotActions &out)
{
    Take(s, L::kPaEnabled, out.enabled);
    Take(s, L::kPaCurrentSet, out.currentSet);
    for (const Struct &a : s.Structs(L::kPaList))
    {
        PlotAction action;
        Take(a, L::kPaId, action.id);
        Take(a, L::kPaState, action.state);
        Take(a, L::kPaCount, action.count);
        Take(a, L::kPaUpdated, action.updated);
        out.list.push_back(action);
    }
}

// A MODU [exe 0x005FF8B0, in the order entry area list, area, position, orientation, time before decay
// (60000 where absent), variables, groups (replacing all), journal, world map, plot actions, resource,
// ambient dialogs]. The module's own ObjectID (23) is written and not read. The campaign resource decides
// whether the load counts as a saved game's.
void Fill(const Struct &s, Campaign &out)
{
    Take(s, L::kObjectId, out.id);
    Take(s, L::kCampaignResource, out.resource);
    for (const Struct &g : s.Structs(L::kGroupList))
    {
        Group group;
        Take(g, L::kGroupId, group.id);
        TakeNumbers(g, L::kGroupHostiles, group.hostiles);
        out.groups.push_back(std::move(group));
    }
    Take(s, L::kTimeBeforeDecay, out.creatureTimeBeforeDecay);
    if (const auto j = s.Child(L::kJournal))
    {
        Fill(*j, out.journal);
    }
    for (const Struct &a : s.Structs(L::kAmbientDialogs))
    {
        AmbientDialog dialog;
        Take(a, L::kAmbOwner, dialog.owner);
        Take(a, L::kAmbSpeaker, dialog.speaker);
        Take(a, L::kAmbResRef, dialog.resRef);
        Take(a, L::kAmbLine, dialog.line);
        out.ambientDialogs.push_back(std::move(dialog));
    }
    if (const auto w = s.Child(L::kWorldmap))
    {
        Fill(*w, out.worldMap);
    }
    if (const auto p = s.Child(L::kPlotActions))
    {
        Fill(*p, out.plotActions);
    }
    Variables(s, out.variables);
    Take(s, L::kEntryAreaList, out.entryAreaList);
    Take(s, L::kEntryArea, out.entryArea);
    Take(s, L::kEntryPosition, out.entryPosition);
    Take(s, L::kEntryOrientation, out.entryOrientation);
}

// -- The party and the game's state

// A PRTY [exe 0x00637D50]: the pool (a creature whose id the world has already is skipped), leader (applied
// only if that creature exists), selected character (applied after the party is placed), the plot manager
// [exe 0x0075E300], the inventory [exe 0x00740790: MaxItems, backpack, plot items, money, recipes], seen
// lines, the flags, qbar expansion, picker status (2 where absent), backpack sort, approvals, item storage,
// new items. The party is fresh at the load, so nothing is cleared first. The plot manager's keys are
// GUIDs, 32 upper-case hex digits.
void Fill(const Struct &s, Party &out)
{
    Take(s, L::kObjectId, out.id);
    List(s, L::kPoolMembers, out.pool);
    List(s, L::kBackpack, out.backpack);
    List(s, L::kPlotItems, out.plotItems);
    Take(s, L::kMoney, out.money);
    Take(s, L::kMaxItems, out.maxItems);
    TakeNumbers(s, L::kCraftingRecipes, out.craftingRecipes);
    Take(s, L::kSelected, out.selectedCharacter);
    Take(s, L::kPickerGui, out.pickerGuiStatus);
    for (const Struct &a : s.Structs(L::kApprovals))
    {
        Approval approval;
        Take(a, L::kApprovalId, approval.id);
        Take(a, L::kApprovalLevel, approval.level);
        TakeStrref(a, L::kApprovalDesc, approval.descStrref);
        out.approvals.push_back(approval);
    }
    Take(s, L::kLeader, out.leader);
    for (const Struct &i : s.Structs(L::kItemStorage))
    {
        StoredItem stored;
        Take(i, L::kStItem, stored.item);
        Take(i, L::kStOwner, stored.owner);
        Take(i, L::kStSlot, stored.slot);
        Take(i, L::kStWeaponset, stored.weaponSet);
        out.itemStorage.push_back(stored);
    }
    for (const Struct &n : s.Structs(L::kNewItems))
    {
        std::uint32_t id = 0;
        Take(n, L::kNewItemId, id);
        out.newItems.push_back(id);
    }
    Take(s, L::kAutoLevelDefault, out.autoLevelDefault);
    Take(s, L::kQuickbarLocked, out.quickbarLocked);
    Take(s, L::kHoldPositions, out.holdPositions);
    Take(s, L::kRunInDriveMode, out.runInDriveMode);
    Take(s, L::kBackpackSort, out.backpackSort);
    Take(s, L::kQbarExpansion, out.qbarExpansion);
    if (const auto manager = s.Child(L::kPlotManager))
    {
        for (const Struct &p : manager->Structs(L::kPlotList))
        {
            PlotEntry plot;
            Take(p, L::kPlotGuid, plot.guid);
            for (std::size_t i = 0; i < plot.flags.size(); ++i)
            {
                Take(p, L::kPlotFlags1 + static_cast<std::uint32_t>(i), plot.flags[i]);
            }
            out.plots.push_back(std::move(plot));
        }
        out.unreadCodex = manager->Strings(L::kUnreadCodex);
    }
    TakeNumbers(s, L::kSeenLines, out.seenLines);
}

void Fill(const Struct &s, ScriptEvent &out)
{
    Take(s, L::kSeType, out.type);
    Take(s, L::kSeCreator, out.creator);
    Take(s, L::kSeTarget, out.target);
    if (const auto d = s.Child(L::kSeData))
    {
        Fill(*d, out.data);
    }
    Take(s, L::kSeScriptName, out.scriptName);
    out.resources = s.Strings(L::kSeResources);
}

// A GMST: the effect counter [exe 0x006C79CB: 1 where the label is absent], the event queue [exe
// 0x0064AAD0, 0x0079C0C0: by SavegameEventID, 10 and 24 script events, 6, 7, 11, 12, 13, 16 and 27 plain, any
// other dropped; appended to the queue in order] and the world timer [exe 0x00A36010].
void Fill(const Struct &s, GameState &out)
{
    Take(s, L::kEffectStartingId, out.effectStartingId);
    if (const auto master = s.Child(L::kAiMaster))
    {
        for (const Struct &e : master->Structs(L::kEventQueue))
        {
            QueuedEvent event;
            event.label = e.Label();
            Take(e, L::kEventDay, event.day);
            Take(e, L::kEventTime, event.time);
            Take(e, L::kEventCaller, event.caller);
            Take(e, L::kEventTarget, event.target);
            Take(e, L::kEventId, event.id);
            if (const auto script = e.Child(L::kEventScript))
            {
                ScriptEvent payload;
                Fill(*script, payload);
                event.script = std::move(payload);
            }
            out.eventQueue.push_back(std::move(event));
        }
    }
    if (const auto timer = s.Child(L::kWorldTimer))
    {
        Take(*timer, L::kWorldDay, out.worldDay);
        Take(*timer, L::kWorldTime, out.worldTimeMs);
    }
}

} // namespace

bool DataArrays::Empty() const
{
    return ints.empty() && floats.empty() && bools.empty() && objects.empty() && strings.empty() && vectors.empty() &&
           quaternions.empty();
}

const Stat *Stats::Find(std::uint16_t row) const
{
    const auto it = std::ranges::find_if(stats, [&](const Stat &st) { return st.index == row; });
    return it == stats.end() ? nullptr : &*it;
}

bool AreaMap::SetSize(std::uint32_t w, std::uint32_t h)
{
    const std::uint64_t cells = std::uint64_t{w} * h;
    if ((cells / 8) + 1 != bits.size())
    {
        return false;
    }
    width = w;
    height = h;
    return true;
}

bool AreaMap::Explored(std::uint32_t x, std::uint32_t y) const
{
    if (x >= width || y >= height)
    {
        return false;
    }
    const std::uint64_t bit = (std::uint64_t{y} * width) + x;
    const std::uint64_t byte = bit / 8;
    return byte < bits.size() && ((bits[static_cast<std::size_t>(byte)] >> (bit % 8)) & 1U) != 0;
}

std::size_t AreaMap::ExploredCount() const
{
    std::size_t count = 0;
    for (const std::uint8_t b : bits)
    {
        count += static_cast<std::size_t>(std::popcount(b));
    }
    return count;
}

const PlotEntry *Party::FindPlot(std::wstring_view guid) const
{
    const auto it = std::ranges::find_if(plots, [&](const PlotEntry &p) { return p.guid == guid; });
    return it == plots.end() ? nullptr : &*it;
}

const Area *Save::FindArea(std::wstring_view tag) const
{
    const auto it = std::ranges::find_if(areas, [&](const Area &a) { return a.tag == tag; });
    return it == areas.end() ? nullptr : &*it;
}

const Creature *Save::FindCreature(std::uint32_t id) const
{
    if (hero && hero->id == id)
    {
        return &*hero;
    }
    for (const Creature &c : party.pool)
    {
        if (c.id == id)
        {
            return &c;
        }
    }
    for (const Area &a : areas)
    {
        for (const Creature &c : a.creatures)
        {
            if (c.id == id)
            {
                return &c;
            }
        }
    }
    return nullptr;
}

std::optional<Save> Read(const gff4::File &das)
{
    if (das.Type() != "SAVE")
    {
        return std::nullopt;
    }
    const auto top = das.Root();
    if (!top)
    {
        return std::nullopt;
    }
    Save save;
    if (const auto c = top->Child(L::kCampaign))
    {
        Fill(*c, save.campaign);
    }
    List(*top, L::kAreaList, save.areas);
    if (const auto p = top->Child(L::kPlayerchar))
    {
        save.hero = One<Creature>(*p, L::kPlayerChar);
        Take(*p, L::kMapZoom, save.mapZoom);
        Take(*p, L::kMapLegend, save.mapLegend);
        Take(*p, L::kTimePlayed, save.timePlayedSeconds);
    }
    if (const auto p = top->Child(L::kPartyList))
    {
        Fill(*p, save.party);
    }
    if (const auto v = top->Child(L::kVersion))
    {
        Take(*v, L::kCheatUsed, save.version.cheatUsed);
        Take(*v, L::kBuildNumber, save.version.buildNumber);
        Take(*v, L::kSaveVersion, save.version.saveVersion);
    }
    for (const Struct &e : top->Structs(L::kWorldDatabase))
    {
        IdPool pool;
        Take(e, L::kWdGroup, pool.group);
        Take(e, L::kWdLastId, pool.lastId);
        save.idPools.push_back(pool);
    }
    if (const auto g = top->Child(L::kGameState))
    {
        Fill(*g, save.state);
    }
    return save;
}

std::optional<Save> Read(std::span<const std::byte> bytes)
{
    const auto file = gff4::File::Parse(bytes);
    return file ? Read(*file) : std::nullopt;
}

} // namespace dao::core::savegame
