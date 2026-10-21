// The labels of a saved game's fields (dev/gff4_fields.csv), by the name xoreos gives them without "Savegame",
// which the reader (Savegame.cpp) and the writer (SavegameWrite.cpp) both go by. Private to the core.
#pragma once

#include <cstdint>

namespace dao::core::savegame::L
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
} // namespace dao::core::savegame::L
