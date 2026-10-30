// Every function and global this library reaches in the game, each as an
// ID: a number from DAOAddressLibrary (extern/DAOAddressLibrary) that means
// the same function or global in every build of DAOrigins.exe. Where it is
// in the build that is running is looked up at run time, by
// dao::Module::Address, in that build's file (dao/Module.h), so nothing
// here, and no plugin, is tied to one build.
//
// A row is a constant and its ID, and nothing else about the thing: what
// the engine calls it, and where it is in each build, are written down once,
// in the address library's table (its data/addresses.csv), and looked up
// there. `python tools/address_ids.py kName`, or an ID, or an address, says
// what a row is: its name and its address in every build. tools/names.py
// gives the table's names to Ghidra on every launch (tools/ghidra-mcp.ps1),
// so a finding is read back beside the code it was made from.
//
// DAO_ADDRESS is code, DAO_DATA a global. DAO_VFTABLE, inside a class,
// gives it kVftable: its vftable's ID, by which a message or an attribute
// is told from another.
//
// Each was read in Ghidra in the GOG 1.05.13263.0 build, and the stories in
// the comments are all told in that build's addresses, as the listing shows
// them (preferred-base virtual addresses). To add one, write it as it was
// read, with the engine's name for it:
//
//   DAO_ADDRESS(kCClientExoApp_SetPaused, "CClientExoApp::SetPaused", 0x005D6740)
//
// and run tools/address_ids.py, which finds its ID, puts the name in the
// table and leaves the row as the ID alone. Until then the row does not
// compile. Add it with what it is, how it was found, and the date: an
// address with no story is one nobody can check.
#pragma once

#include "daoaddr/Library.h"

#include <cstdint>

namespace dao
{

// A function's or a global's number in the address library. Its own type,
// so an address cannot be passed where one is wanted.
using AddressId = daoaddr::Id;

} // namespace dao

#define DAO_ADDRESS(name, id) inline constexpr ::dao::AddressId name{id};
#define DAO_DATA(name, id) inline constexpr ::dao::AddressId name{id};
#define DAO_VFTABLE(id)                                                                                                \
    static constexpr ::dao::AddressId kVftable                                                                         \
    {                                                                                                                  \
        id                                                                                                             \
    }

namespace dao::addr
{

// The exe's entry point (mainCRTStartup), from IMAGE_OPTIONAL_HEADER, which
// the loader detours to run plugins on the main thread before the game.
DAO_ADDRESS(kEntryPoint, 25795)

// The script command table (dev/SCRIPT_COMMANDS.md, dao/Script.h), on the
// CVirtualMachineCommands object at CClientExoApp+0x1E4. Its vtable runs
// in the order of NWN's CVirtualMachineCmdImplementer (NWN:EE's
// CVirtualMachineCmdImplementer.hpp): destructor, InitializeCommands,
// ExecuteCommand.
//
// InitializeCommands, slot 1: allocates the table, 0xD7C bytes (863
// handlers), zeroes it, then makes 827 calls to Register(id, function,
// "COMMAND_NAME"), one per command a script can call, and returns (`ret`).
// tools/script_commands.py reads the table out of it. The VM calls it when
// it is given the object (0x00A2D240, which keeps it at VM+0x1968), just
// after the application stores it. Read 2026-09-30; until then this row
// said the registrations began at 0x00674E90, which is ReadIniEntry's
// handler, one of the functions registered. Seen in play 2026-09-30: the
// probe's detour ran once, five seconds into start-up, on the commands
// object the application keeps, and on the thread every script command
// then ran on.
DAO_ADDRESS(kCVirtualMachineCommands_InitializeCommands, 13533)
// `table = this->[+0xC]; table[id] = fn`, `ret 0xC`, the name unused.
DAO_ADDRESS(kCVirtualMachineCommands_Register, 12839)
// Slot 2: an id of 0x35F or more, or an empty slot, is error -642
// (0xFFFFFD7E); otherwise `table[id](id, paramCount)` with the commands
// object in ecx, its result returned.
DAO_ADDRESS(kCVirtualMachineCommands_ExecuteCommand, 12840)

// Pause (dev/TURN_BASED.md). Read 2026-09-29 with tools/daoexe.py; not yet
// verified in play.
//
// The pause key's handler. Its address is the member function pointer the
// handler object MemberFunctionMessageHandler<GameModeController,
// UserTogglePauseMessage, &GameModeController::HandleMessage> is built with
// (at 0x006407CA: vftable 0x00B06648, +8 = this address, this-adjust 0);
// tools/message_handlers.py reads all 314 such handlers the same way. A
// __thiscall bool(const UserTogglePauseMessage&), `ret 4`, always true. It
// ignores its controller: it reads the app's paused flag and calls SetPaused
// with the opposite, and 1 as the second argument.
DAO_ADDRESS(kGameModeController_HandleUserTogglePause, 12587)
// The application object: allocated 0x2F0 bytes and constructed at
// 0x005DCC4A (constructor 0x005DC510, whose vftable stores name the class
// CClientExoApp), stored here, cleared at 0x005D42D3.
DAO_DATA(kClientExoApp, 100835)
DAO_ADDRESS(kCClientExoApp_Constructor, 10640)
// Pause or unpause. __thiscall void(bool paused, bool second), `ret 8`. Does
// nothing if the flag already says `paused`. Otherwise it sets the flag (an
// object at +0xD8 whose byte at +0xC is the value, set through its vtable
// slot +0x54), sets a second such object at +0xE8 from `second`, keeps the
// pause's start time at +0xF8 and a running total of paused time at +0xFC,
// and between those reacts to the game mode in (this+0x114)->+0x78 (modes
// 0-5, UI updates per mode). Named SetPaused by what it does; the engine's
// name is not in the exe. What `second` means is not yet read: the pause
// key passes 1.
DAO_ADDRESS(kCClientExoApp_SetPaused, 10547)
// GFx's advance of a movie, read 2026-10-15 for DAOLib #134: GFxMovieRoot's
// slot 30 (its vftable 0x00B4CA6C), __thiscall (float seconds, unsigned
// frames to catch up), `ret 8`, answering the time to the next frame in
// st(0); 3.6 KB, beside Display (slot 31). Slot 33 (0x0095B1C0), taken
// first for it, is SetBackgroundAlpha: __thiscall (float), `ret 4`, a byte
// at +0x8F7 of the alpha times 255, which the manager sets to 0 on a new
// movie (CreateMovieView, 0x00514A9E). Who calls Advance each frame is the
// probe's to see (src/plugins/probe/Movies.cpp).
DAO_ADDRESS(kGFxMovieRoot_Advance, 28507)
// Each movie's step, read 2026-10-15 for DAOLib #134 after the probe saw
// one caller advance every movie each frame (0x0051AE44, the update
// thread), paused or not: the manager's update (its slot 4, 0x005166C0)
// goes through its records (+0x70) and calls this, __thiscall on a record
// (float time, float other time, bool restricted), `ret 0xC`. A record
// shown (+0x28) is advanced unless restricted (a full menu up) and its
// flags (+0x24) lack 0x10; by the second time where its flags have 8, else
// the first, times its speed (+0x30). Seen with the probe (DAOTurnBased's
// and DAOPhotoCam's games, 2026-10-15): the first is the frame's time and
// the second the game's, 0 while paused; the HUD's records and a plugin's
// screen (AddScreen) have flags 0, so they are advanced with the frame's
// time paused or not, in fly cam mode too, and a full menu narrows the
// records advanced to its own. The manager also sets the byte at +0x9CB of
// every new movie (GFxMovieRoot's slot 35, read back by slot 37, 0x00514A8F):
// nothing else in the exe reads it. The name is ours.
DAO_ADDRESS(kMovieRecord_Step, 6791)
// The game mode controller, CClientExoApp+0x114 (dao/GameModes.h), read
// 2026-09-30: its constructor, from the application's initialisation
// (0x005DC052), and SetGameMode(mode, 0, 0), __thiscall, which its
// handler for SetGameModeMessage (0x006416E0) calls with the message's
// field.
DAO_ADDRESS(kGameModeController_Constructor, 12617)
DAO_ADDRESS(kGameModeController_SetGameMode, 12618)
// The controller's switch (dao/GameModes.h, OnModeChanged): __fastcall on the controller, no
// stack arguments, plain `ret`, from its update on the game thread. It takes a queued request
// (0x00640C60), runs the old mode's Leave (slot 7), writes the new mode at +0x78 and the old at
// +0x88, runs the new mode's Enter (slot 6), has the GUI show or hide the HUD (slot 77) and
// delivers EVENT_TYPE_GAMEMODE_CHANGE (60) to the module; no request taken, +0x78 stays. A load
// ends here, the loading mode (9) left in its last state (6), the party placed and the load
// scripts run before it. Read 2026-10-24 (DAOLib #255), dev/CUTSCENES.md.
DAO_ADDRESS(kGameModeController_SwitchMode, 12624)

// What is under the mouse cursor, read 2026-09-30 (dao/GameModes.h,
// GameModeExplore, has the fields). The mode's update re-picks 15 times a
// second: CastCursorRay asks the camera for the ray through the cursor
// and collects up to 32 hits along it with the world's Raycast, nearest
// first; UpdateCursor chooses the object or the ground among them, and
// CanMoveTo judges the ground. A move click goes through MoveTo
// (__thiscall (creature id, const Vector4 *point, -1), `ret 0xC`), which
// records the point and has the creature walk. Raycast is __thiscall on
// the application's +0x10C, which it does not read, (hits, max, const
// Vector4 *origin, const Vector4 *unit direction, collision mask, flag,
// physics scene or null for the current area's), `ret 0x1C`, and returns
// the count. The names are ours.
DAO_ADDRESS(kGameModeExplore_CastCursorRay, 16179)
DAO_ADDRESS(kGameModeExplore_UpdateCursor, 16280)
DAO_ADDRESS(kGameModeExplore_CanMoveTo, 16220)
DAO_ADDRESS(kGameModeExplore_MoveTo, 16197)
DAO_ADDRESS(kWorld_Raycast, 12713)
// World queries (dao/World.h), read 2026-09-30. Raycast collects its hits
// in a static vector (below) and, with its flag, adds the selection group
// 0x20000 to the mask and keeps an object's body and volume both; it uses
// the current area's scene when given none (0x00648B30: the module's
// current area, its +0x62C, whose +0xCC is the scene, 0x00537D60). The
// area's LineOfSight is __thiscall (const Vector4 *from, const Vector4
// *to, Vector4 *where blocked or null, ignored object id, mask), `ret
// 0x14`, and returns true when nothing blocks: the scene's slot 4 finds
// the nearest shape on the segment, and a placeable hit blocks only if
// its base type is 1 (a door) or 8 (0x0067A670). An object's area is its
// slot 40, cast by slot 2 (0x00695180); the module's current area is its
// +0x388 once loaded, else the controlled creature's (0x005FE470). The
// names are ours.
DAO_DATA(kWorld_RaycastResults, 101265)
// The world on the screen, read 2026-10-01 (dao/World.h, ToScreen). The
// cursor's ray cast (GameModeExplore::CastCursorRay, 0x0070B590) takes the
// cursor's place in pixels, asks this global's object for the scene being
// drawn (its slot 9, no arguments: an Eclipse::IGraphicsInternal's +0x58),
// whose camera is at its +0xC0: a GraphicsStructures::SceneCamera (28
// slots), which wraps the renderer's camera at its +4, a
// Renderer::PerspectiveCamera (42 slots), and passes its slots 16 to 19
// on to it. It has the camera turn the pixel into two points of the
// world, near and far (its
// slot 17, (x, y, out near, out far), `ret 0x10`); the ray from the first
// toward the second is cast with the mask 0x2607 and the selection
// volumes. The camera's slot 19, (const point, out x, out y, out bool),
// `ret 0x10`, goes the other way: the point through the view matrix
// (+0x80) and the projection (+0x40, remade first if the flag at +0x30
// says so), the bool whether it came out in front, and x and y in pixels
// of the view, whose width and height the object at the renderer's
// camera's +4 answers (its slots 5 and 6): the renderer's camera's, not
// the wrapper's, whose +4 is the renderer's camera itself. The points are
// four floats, aligned for SSE. The names are ours.
DAO_DATA(kGraphics, 100485)
DAO_DATA(kPerspectiveCamera_Vftable, 74603)
// A model's box (dao/World.h, ModelBoxOf), read 2026-10-04 (DAOLib #21):
// a GraphicsStructures::Object's Graphics::IRenderable, at its +4, answers
// the model's box by its slot 5 (0x00549B80, ID 7935); only an interface
// with this table is asked.
DAO_DATA(kGraphicsStructures_Object_RenderableVftable, 63977)
// The scene's objects and nodes changing (dao/Scene.h, read 2026-10-22,
// dev/SCENE.md): a Graphics::ISceneInstance takes a GraphicsStructures::Object
// into its list (+0x90) by AddObject and out of it by RemoveObject, each
// __thiscall (the Object's +8 interface, a second word), `ret 8`; a node takes
// a child by AddChild, __thiscall (child), `ret 4`, into its vector at +0x10,
// setting the child's parent at +0x170.
DAO_ADDRESS(kISceneInstance_AddObject, 7474)
DAO_ADDRESS(kISceneInstance_RemoveObject, 7458)
DAO_ADDRESS(kObjectNode_AddChild, 8352)
DAO_ADDRESS(kWorld_CurrentScene, 12699)
DAO_ADDRESS(kCCArea_LineOfSight, 13635)
DAO_ADDRESS(kCCPlaceable_BlocksSight, 13576)
DAO_ADDRESS(kCCObject_Area, 14059)
DAO_ADDRESS(kCCModule_CurrentArea, 11018)
// Paths, read 2026-09-30 (dao/World.h, CurrentPath; dev/TODO.md, item 5). A
// creature's MovementController is its +0xA40 (Movement); the move
// commands' update (0x007A9130) asks for a path through RequestPath
// (__thiscall on the creature's area (creature, const Vector4 *to, const
// Vector4 *from or null), `ret 0xC`; 1 found, 5 failed, 8 no controller,
// else still searching, asked again the next frame),
// which sets the controller's start (+0x60) and destination (+0x30) and
// advances the search a slice of time (Step, 0x00A62640, through
// 0x00A473F0 and the Pathfinding search environments). The search, at the
// controller's +0x14, keeps its waypoints in a std::vector at +0x70
// (Waypoint reads one) and the next one's index at +0x88. The names are
// ours.
// Rooms and their streaming (dao/World.h, Rooms), read 2026-09-30 for the
// turn-based mod, whose camera panned into rooms drawn black. An area is
// laid out in rooms (CRoom, vftable 0x00AFFFF4, over IRoomLayout), each
// loaded from its own file, <area>_<room>.rim, by a RoomLoadingTask. The
// area's layout is its +0x658: its rooms a pointer array at +0x424 with
// the count at +0x428, the scene it draws into at +0x4E0, and the
// streaming's reference point at +0x550. Its update (LayoutUpdate,
// __thiscall, no arguments, each frame from the area's 0x0067B210) takes
// the scene's camera's position as that point (unless the layout's +0x560
// says to take a listed object's), then asks its policy what each room
// should be and tells the room (its slot 7, SetState, __thiscall (state,
// layout, 0)). When the point has moved more than 30 m in a frame, in
// explore mode, it pauses the game until the rooms are in. The indoor
// policy (EC::IndoorStreamPolicy, 0x00B00078; Decide, its slot 1,
// __thiscall (layout, ?, room, int *state), `ret 0x10`) answers 4 for a
// room the scene's +0x68 object names (its slot 21, by the room's number;
// what that object is, is not read), else goes by the squared distance from
// the point to the room's bounds (BoundsDistanceSquared, __thiscall on the
// bounds (const Vector4 *), `ret 4`): under its +8, 5; under its +0xC, 4;
// up to its +0x10, 1, which changes nothing; farther, 3. A room keeps its
// number at +0x14, its bounds at +0x120, its load at +0x264 (2 not loaded,
// 0 a load asked for, 1 loaded), the state last asked at +0x268 and a load
// in the background under way at +0x371. State 3 unloads it; 4 loads it in
// the background if it is not loaded; 5 loads it at once, or, its load
// already asked for, puts its number in the layout's list at +0x4D0
// (0x005C9A50; 4 takes it out again, 0x005C92B0), a list kept under a lock
// (+0x4B0) for the loading thread: the rooms wanted first. In play,
// 2026-10-01, in Castle Cousland: 61 rooms, 1 in state 5, 53 in state 4 and
// 7 in state 3, the controlled creature's own room in state 4 with its
// load 1. So the states are how much a room is wanted, and the load is
// whether it is there.
//
// What draws a room black is another thing, read 2026-10-01: whether the
// party has seen it. The area's update (0x0068AF30) gathers the rooms the
// party's members stand in (0x00635FD0, on the party, into the area's
// +0x61C) and, when they have changed, runs RoomsSeen (__thiscall on the
// area (const std::vector<int> &rooms), `ret 4`): those rooms, and each
// neighbour of one that passes a test of the two (0x00687EC0: a room the
// first sees, through its connects where it has them, and no door whose
// state blocks the view; read 2026-10-20, dev/COLLISION.md "Rooms seen";
// seen in play the same day, a door opened made the room behind it seen
// within 1.5 s), are in view; each is added to the area's list of rooms seen
// (a std::vector<int> at +0x460, which a save keeps as
// SavegameAreaRoomsViewed, 16018); then every room is told which it is
// (its slot 22, SetSeen, which keeps it at the room's +0x100: 1 in view, 2
// seen before, 0 never) and whether to show (its slot 31, with false for
// one never seen). The names are ours.
DAO_ADDRESS(kCCArea_RoomsSeen, 13805)
// Which room a thing is in, read 2026-10-01 from how the party's rooms are
// gathered. An object keeps its room at +0x41C and the room's number at
// +0x150 (-1 in none), set by its own refresh (0x00695D70), which asks its
// area for the room at its position raised by its height: RoomAt,
// __thiscall on the area (const Vector4 *), `ret 4`, which hands the point
// to the layout (its slot 2) and answers the room or null.
DAO_ADDRESS(kCCArea_RoomAt, 13596)
DAO_ADDRESS(kAreaLayout_Update, 10254)
DAO_ADDRESS(kIndoorStreamPolicy_Decide, 10447)
DAO_ADDRESS(kCRoom_SetState, 10429)
DAO_ADDRESS(kBounds_DistanceSquared, 4918)

// Cooldowns (dao/Abilities.h), read 2026-09-30 from the scripts'
// SetCooldown and GetRemainingCooldown. A creature's stats keep a cooldown
// manager at +0xF4, made on first use (Cooldowns, __thiscall, no
// arguments). Find, __thiscall on the manager (ability, const std::wstring
// &item tag), `ret 8`, gives an ability's record or null. A record is 0x44
// bytes: the ability, the tag of the item it came from (a std::wstring at
// +4), the seconds it was last set to at +0x20, and two float attributes,
// the seconds left at +0x24 and their share of +0x20 at +0x34 (each value
// at the attribute's +0xC). A record is made when the creature gains the
// ability (0x0071EAC0, through 0x0071D6D0) and handed to the creature's
// ability list for the interface (the stats' +0x1B4, 0x0062D760).
// SetCooldown, __thiscall on the stats (ability, float seconds, const
// std::wstring &tag), `ret 0xC`, sets +0x20 and the seconds left to the
// time, the share to 1, and puts the record among those that tick; it does
// nothing for an ability with no record, nor for a record outside the
// manager's first list (its +0, the creature's own abilities' records):
// an item's record, which only the ticking list (+0x10) holds, is left
// as it was (0x0071D630, read 2026-10-06, DAOLib #38). Find looks in
// both lists. Tick (0x0071C840, __thiscall on
// the manager (float seconds), from Cooldowns' own caller) takes the time
// from each, sets the share to what is left over +0x20, and drops a record
// that has run out from the ticking ones. The names are ours.
DAO_ADDRESS(kCCCreatureStats_Cooldowns, 16534)
DAO_ADDRESS(kCCCreatureStats_SetCooldown, 16554)
DAO_ADDRESS(kCooldownManager_Find, 16526)
DAO_ADDRESS(kCooldownManager_Tick, 16532)
// Whether a creature can use an ability now (dao/Abilities.h), read
// 2026-10-03: the check the quickbar greys a button by (0x0071D1B0 asks it
// for each slot of the page, the reason at -1 first) and the player's order
// makes before it issues a command (GameModeExplore's PerformAbilityMessage
// handler, 0x00711C38, and the function after it, 0x00711EF9); the
// use-ability command's builder asks it too (0x00744166). __thiscall on the creature's stats (ability, int
// *reason, const std::wstring *itemTag), `ret 0xC`, true if usable, else
// the reason a string reference. In order: an item's ability needs the item
// still held (244342); an ability in a set (0x008663A0, the active modal
// ones, so a sustained one can be turned off) is usable with no more said;
// disabled by effects (0x00A64740's flags, 223888); the cost, ABI_BASE's
// "cost" times 1 + fatigue (property 41) / 100, against property 8's current
// value (376268 stamina, 376267 mana), blood magic (10700) excusing it; the
// ability's conditions (CanUseConditionedAbility); its cooldown; and not
// the ability being cast already (395779). One failing path (0x0071CF69)
// gives 0 for the reason. The name is ours.
DAO_ADDRESS(kCCCreatureStats_CanUseAbility, 16539)
// The level-up screen's tests (dao/LevelUp.h): IsAbilityAvailable, __thiscall on the stats (ability),
// `ret 4`, a bool, the screen's and the script command's (13096 calls it at 0x0065A6FA); and
// AbilityChains::Find, cdecl (stats, kind, mode), which builds the party data's lists of offers.
// Read 2026-10-24 (DAOLib #259).
DAO_ADDRESS(kCCCreatureStats_IsAbilityAvailable, 16521)
DAO_ADDRESS(kAbilityChains_Find, 12208)
// The party picker's choice (dao/PartyPicker.h): SelectEntry, __thiscall (entry), `ret 4`, the
// select branch of HandleMessage_SelectPartyMemberUnderCursorMessage (18342, at 0x00768B50), whose
// own check is only fewer than four chosen. Read 2026-10-24 (DAOLib #259).
DAO_ADDRESS(kGameModePartyPicker_SelectEntry, 18307)
// A store's prices and its test of a sale (dao/Store.h), each __thiscall on the store object:
// GetBuyPrice and GetSellPrice (item, count), `ret 8`, what it charges and pays, by MarkUp or
// MarkDown as the item was just traded; WillBuy (item, seller creature, count), `ret 0xC`, a
// bool, its WillNotBuy and WillOnlyBuy lists, its money and MaxBuyPrice. Read 2026-10-25 (DAOLib
// #259).
DAO_ADDRESS(kStore_GetBuyPrice, 21193)
DAO_ADDRESS(kStore_GetSellPrice, 21192)
DAO_ADDRESS(kStore_WillBuy, 21206)
// Crafting's party (dao/Crafting.h): GetParty, __thiscall on the app with no argument, returns
// the party (the app's +0xD0, its +0x58), whose inventory at +0x20C knows the recipes. Read
// 2026-10-25 (DAOLib #259).
DAO_ADDRESS(kCClientExoApp_GetParty, 10504)
// An item's requirements (dao/Items.h), each __thiscall: on the item, IgnoresRequirements (a bool),
// GetRequirementCount (0 to 2) and GetRequirement (index, int* property, float* value, CCCreature*),
// `ret 0x10`; on the creature, MeetsItemRequirement (CCItem*, index), `ret 8`, a bool. Read
// 2026-10-25 (DAOLib #259).
DAO_ADDRESS(kCCItem_IgnoresRequirements, 18012)
DAO_ADDRESS(kCCItem_GetRequirementCount, 17983)
DAO_ADDRESS(kCCItem_GetRequirement, 18004)
DAO_ADDRESS(kCCCreature_MeetsItemRequirement, 14705)
// A follower's tactics (dao/Tactics.h), each cdecl: PartyTactics::Of (creature id) and EntryOf
// (creature id, row from 0), null for none; and the screen's lists, each filling the engine's
// std::vector<unsigned> and answering a bool: ListTargets (creature, types, objects),
// ListBaseConditions (target, bases), ListConditions (base, target, conditions, objects),
// ListCommands and ListAbilities (creature, target, base, out), ListModalAbilities (creature,
// out), ListItems (target, out). Read 2026-10-25 (DAOLib #259).
DAO_ADDRESS(kPartyTactics_Of, 19942)
DAO_ADDRESS(kPartyTactics_EntryOf, 19956)
DAO_ADDRESS(kPartyTactics_ListTargets, 20010)
DAO_ADDRESS(kPartyTactics_ListBaseConditions, 19983)
DAO_ADDRESS(kPartyTactics_ListConditions, 19973)
DAO_ADDRESS(kPartyTactics_ListCommands, 19993)
DAO_ADDRESS(kPartyTactics_ListAbilities, 19974)
DAO_ADDRESS(kPartyTactics_ListModalAbilities, 19975)
DAO_ADDRESS(kPartyTactics_ListItems, 19984)
// The Chanter's board (dao/Board.h): GetJournal, __thiscall on the app, no argument; the journal's
// BuildChanterBoard (board), `ret 4`, and ClearChanterBoard, no argument, each __thiscall. Read
// 2026-10-25 (DAOLib #259).
DAO_ADDRESS(kCClientExoApp_GetJournal, 10506)
DAO_ADDRESS(kJournal_BuildChanterBoard, 15737)
DAO_ADDRESS(kJournal_ClearChanterBoard, 15698)
// And its test of a recipe known, as Inventory::CraftRecipe makes it (0x0073FE45 to 0x0073FE6B):
// the id wrapped by the int attribute's constructor, __thiscall (const int*), `ret 4`; Contains,
// __thiscall on the vector (the inventory's +0x38 attribute's value) with the wrapped id, `ret 4`,
// a bool; the wrapper's destructor, __thiscall with no argument.
DAO_ADDRESS(kSmartDataAttributeInt_Attribute, 119)
DAO_ADDRESS(kECDynArray_Contains, 17611)
DAO_ADDRESS(kSmartDataAttributeInt_Destroy, 143)
// CCCreature's vtable slot 44 (+0xB0), __thiscall (anim_base row, float speed, float blend, byte
// flag), `ret 0x10`, no return value (every caller overwrites eax), read 2026-10-21 (dev/ANIMATION.md). It plays an
// animation on the base layer through 0x0069B3D0. Only CCCreature's vtable holds it.
DAO_ADDRESS(kCCCreature_Play, 14670)
// Where an animation is in its cycle, read 2026-10-15 for DAOLib #131
// (dao/Animation.h, SetPhase) and checked 2026-10-23. An object's AnimationTask (its slot 2,
// 0x008154D0) takes the frame's time from the game's clock (none while
// paused) times the object's speed (+0x2E8) and steps the object (its slot
// 25, 0x00546B90) by phases; phase 1 (slot 48, 0x00547580) advances each
// of its animations (a vector at +0x17C, first +0x180, last +0x184) by
// their slot 9. A ModelAnimationTree's (0x00814260) walks its nodes (a
// vector at +0x10, first +0x14, last +0x18, 0x34 bytes each): a node's
// place in its cycle is a 16-bit word at +0x2E, 0x10000 the whole cycle,
// advanced by its rate (+0x20) times the time and wrapping (0x00811BE0);
// the tree keeps the last at +0x30 before it advances.
DAO_DATA(kModelAnimationTree_Vftable, 69616)
DAO_ADDRESS(kCCCreature_Movement, 14455)
// A creature's animations (dao/Animation.h), read 2026-10-02: its
// animation set (+0x154) answers whether it has an animation and how long
// it is in milliseconds, __thiscall (id), `ret 4` each; the attack swing's
// step (0x006A41A0) asks both. The names are ours.
DAO_ADDRESS(kCCCreature_AnimationLength, 14048)
DAO_ADDRESS(kCCCreature_HasAnimation, 14049)
// How long a command gives an animation it plays as a step (dao/Animation.h,
// StepLength), read 2026-10-10 (DAOLib #51): the animation sub-action's
// handler (37, 0x007A4390) asks it for its duration, __thiscall (id), `ret
// 4`, milliseconds. An animation that loops (0x00695390) answers its own
// length. Another answers its own and that of each it passes into by
// anim_base's TransitionTo (0x005CA6B0), until the next would loop or five
// are counted. One with an animation event 31 or 32 (events.gda's deathblow
// attack and death merges; 0x00A38780 answers an event's time) answers its
// own and that of one more, not walking TransitionTo: the deathblow's
// attack for the creature's weapon style (0x006A9140) or the reaction for
// its killer's (0x006BF8A0), as the address library's session read them,
// 2026-10-10. The handler divides the answer by the
// step's speed (its float 3, where above 0), takes a blend off where its
// flag 3 is set, and multiplies a looping one by its count. The name is
// ours.
DAO_ADDRESS(kCCCreature_AnimationStepLength, 14096)
// A creature's animation events, read 2026-10-12 (DAOLib #88): its vtable's
// slot 46, __thiscall (event record, bool), `ret 8`, a bool in al. The
// record's first word is the event's number (events.gda's); number 1 is an
// attack's impact, which goes to 0x006A6230 (it reads the record's name, a
// string at +0x1C, for "Left", the off hand's weapon) and on to
// 0x006A6080, the one place a melee impact's script event, 29, is made (a
// projectile's is made by 0x007E87C0). With the bool set the record only
// goes to 0x006B6290. What it does not take itself goes to CCObject's. The
// attack's sub-action waits the animation's length and sends no impact
// itself, so an animation whose events are not raised is a blow that never
// lands. The name is ours.
DAO_ADDRESS(kCCCreature_OnAnimationEvent, 14964)
// Where a creature's animation events come from, and what stops them far
// from the camera, read and seen in play 2026-10-12 (DAOLib #88, with the
// probe). Each update a CCObject (0x0069A600, under the AI master's update,
// on the game thread) asks the Animation::AnimationManager (a static
// object, its getter 0x008107F0) for the events waiting under its id (the
// manager's slot 15, 0x0080F380) and hands each to its slot 46. The manager
// is given them (AddEvent, its slot 14, __thiscall (event record, flags),
// `ret 8`, under its lock) by the animations as they are advanced: each
// graphics object (GraphicsStructures::Object) owns an Animation::
// AnimationTask, whose run advances the object's animation, and the tasks
// of the objects in the scene run on the task threads as a group the
// scene's animation manager owns (Animation::AnimSceneManagerDefault: its
// slot 5, 0x00810DB0, takes an object in and its slot 7, 0x00810E00, takes
// one out). So an object not in the scene has no animation and raises no
// event.
//
// What takes a creature's object out of the scene is the game's own show
// and hide by distance. 0x006184E0 goes through the area's objects by their
// distance from the camera and tells each whether it is shown (its slot 79,
// 0x0069A9xx for a CCObject, 0x006AA890 a creature's own): a creature
// nearer than CreatureShowDistance is, and one farther is not, unless it is
// under 60 m and its +0xC60 names a follower; any other object is shown
// nearer than ObjectShowDistance; the creatures 0x005D5600 answers true
// for are left as they are. Both distances are 60 in the image. The
// creatures' the game writes itself whenever it applies the graphics
// detail (0x00458EB0, by the level: 30, 35, 50 or 60; four callers, read
// by the address library's session); nothing writes the objects'. Hidden in explore
// or combat mode, an object fades (0x00695A90, half a second, its target
// the byte at +0x18D set by 0x00695C00) and at nothing its slot 66 takes
// its model out of the scene. The same pass gives each object its band
// (its slot 81), which a creature's AI level is worked out from.
//
// In play: with the camera 82 m from a fight no impact was raised at all
// (and so none handled), where 40 to 55 were in 14 s with the camera on the
// fight; as the camera left, some forty objects were taken out of the
// scene in one frame, from 0x00695A90 under the AI master's update. Three
// things read first were not it, each tried in play: the rooms' streaming
// around the camera, the creature's AI level, and the scene's fade of far
// objects (0x00536010, whose distances are an object's own times a factor).
// The names are ours.
DAO_ADDRESS(kAnimationManager_AddEvent, 21851)
DAO_DATA(kCreatureShowDistance, 96798)
DAO_DATA(kObjectShowDistance, 96799)
DAO_ADDRESS(kCCObject_RequestPath, 17573)
DAO_ADDRESS(kMovementPath_Step, 33560)
// A search apart from any creature's (dao/World.h, FindPath), read
// 2026-09-30. The controller is a plain record, about 0xA0 bytes: the
// pathfinding world at +0x0C (the application's +0x118), the mover's
// parameters at +0x10 (a record of 0x50 bytes: its id, its radius at +4, a
// size class at +0xC, then the obstacle it stands as), its path at +0x14 (a
// 0x90-byte object, 0x00A482D0, which holds the search's own state), the
// destination at +0x30 with a direction at +0x40 and the area's number in
// the pathfinding world at +0x50, the start at +0x60 with the mover's facing
// at +0x70 and the area's number again at +0x80, and flags at +0x9C and
// +0x9D (failed, found). The script command IsLocationSafe (0x0065AF..)
// makes both records on its stack: the parameters' constructor, then the
// controller's (__thiscall (world, parameters), `ret 8`, which makes the
// path), and frees the path after (Free). RequestPath is the order of a
// search: IsFree (__thiscall (const Vector4 *, area number, 0, 0), `ret
// 0x10`) asks whether the mover fits at the destination, and if not the
// area's NearestFree (__thiscall (const Vector4 *at, const Vector4
// *direction, float reach, controller, 0, Vector4 *out), `ret 0x18`) finds
// a place within 2 m back toward the start; SetStart and SetGoal
// (__thiscall (area number, const Vector4 *, const Vector4 *), `ret 0xC`)
// store the ends; LiftObstacle takes the mover's own obstacle out of the
// pathfinding world, and the creature's PlaceObstacle puts it back after;
// Direct (__thiscall (0, 0), `ret 8`) asks whether a straight walk will do,
// and else Step advances the search (1 found, 5 none, 8 no mesh, others
// still searching). The area's number is an attribute at the area's +4 (its
// value at +0x10).
//
// The grid the searches run on, read 2026-10-01. A cell has a kind
// (0x00A49070). IsFree (0x00A46FC0): kinds 8 and up, and kind 7, block, and
// stamps never do. The coarse search passes kinds 0 to 8, the fine search
// steps into a cell of kind under 8, and stamps 5 and 6 matter only to
// movers of the same class (+0xC90) (corrected 2026-10-18 by lib-agent-2;
// dev/PATHFINDING.md); and counts of what crowds it (0x00A49160, 0x00A49240). A
// creature's obstacle (0x00A469C0, stamped by PlaceObstacle and taken out
// by LiftObstacle from a record of the cells it changed) makes the cells
// under it kind 6 and adds to the count of those within a metre round it.
// IsFree goes by the fixed kinds alone, so a creature does not make a place
// unfree. Direct (0x00A4BB80) fails on any crowded cell and on a stamp, so
// a straight walk is refused beside standing creatures. The fine search
// (Pathfinding::MapSearchEnvironment, 0x00B592E0) walks through stamps and
// crowding at a cost (0x00A45AA0), but for a mover wider than a cell takes
// only cells with room about them (0x00A461A0). RequestPath begins by
// telling the pathfinding world about its mover (0x0073EB24): at the
// world's +0xC4 whether the creature's +0xC90 is not 0, which decides
// which kind of stamp stops it in the straight-walk check, and at +0xC8
// the creature's size (its slot 39).
//
// What is in the way, read 2026-10-01. When a creature's own search
// answers 6, the creature's PathBlocker (0x006A7E10) takes its path's last
// waypoint and goes through the area's objects (ids, a vector at the
// area's +0x690) for a placeable within 5 m of it whose footprint holds
// the point (CCPlaceable::Covers, __thiscall (const Vector4 *), `ret 4`),
// and answers its id: the object the scripts are then told to use. So what
// bars a path is a placeable, a shut door in play. The point it looks
// about is the path's own note of the barred cell, a pointer at the path's
// +0x84 to a point made on first use (0x00A476B0), which the search's
// refining writes when it answers 6 (0x00A4D1F0). The names are ours.
DAO_ADDRESS(kCCCreature_PathBlocker, 14603)
DAO_ADDRESS(kCCPlaceable_Covers, 17891)
DAO_ADDRESS(kMovementParameters_Constructor, 33566)
DAO_ADDRESS(kMovementPath_Constructor, 33575)
DAO_ADDRESS(kMovementPath_Free, 33555)
DAO_ADDRESS(kMovementPath_IsFree, 33573)
DAO_ADDRESS(kMovementPath_SetStart, 33570)
DAO_ADDRESS(kMovementPath_SetGoal, 33571)
DAO_ADDRESS(kMovementPath_LiftObstacle, 33563)
DAO_ADDRESS(kMovementPath_Direct, 33561)
DAO_ADDRESS(kCCCreature_PlaceObstacle, 14494)
DAO_ADDRESS(kCCArea_NearestFree, 17572)
// Lets a finished search go before a new one (__thiscall, no arguments):
// the move update calls it first, then puts the mover's id in the
// controller's parameters (+0x10, its first word), the target's id at
// +0x94 when the command has one, and the range, 0.05 m, at +0x90.
DAO_ADDRESS(kMovementPath_Cancel, 33569)
DAO_ADDRESS(kMovementPath_NextWaypoint, 33556)
DAO_ADDRESS(kPathSearch_Waypoint, 33026)

// Visual effects in the world (dao/Vfx.h), read 2026-09-30 from explore
// mode's aiming (0x00710F80), which keeps its ring's id in a global and
// places, moves and removes it each frame. The manager is a singleton made
// on first use (Get, __cdecl, its pointer at 0x00C389C8). Place is
// __thiscall (const {row, area id, scale, 0} *, the id it had or -1, mode,
// const Vector4 *at, const Vector4 *facing or null, attach to or 0), `ret
// 0x18`, and answers the effect's id, the one it had if that still lives;
// Find (id) `ret 4` answers its object; Remove (id, 0) `ret 8`. An effect
// is moved by CCObject::SetPosition (const Vector4 *, flag), `ret 8`.
// AimRingEffect (area type, out of reach) answers the ring's vfx_base row,
// 30017 to 30019. The names are ours.
DAO_ADDRESS(kVfxManager_Get, 19701)
DAO_ADDRESS(kVfxManager_Place, 19710)
DAO_ADDRESS(kVfxManager_Find, 19676)
DAO_ADDRESS(kVfxManager_Remove, 19704)
// A placed effect's size and colour after it is placed, read 2026-10-01
// (dao/Vfx.h): the two calls the effect's set-up (0x007EA550) makes on
// the effect once its model is there. __thiscall on the effect's object,
// (float scale) and (const colour *, four floats), `ret 4` each: they find
// the model and call its slots 43 and 69. Nothing is done for an effect
// whose model is not loaded yet. The names are ours.
DAO_ADDRESS(kVfxObject_SetScale, 21152)
DAO_ADDRESS(kVfxObject_SetTint, 21153)
DAO_ADDRESS(kCCObject_SetPosition, 14084)
DAO_ADDRESS(kGameModeExplore_AimRingEffect, 16149)
DAO_ADDRESS(kGameModeExplore_UpdateAim, 16294)
DAO_DATA(kGameModeExplore_AimRing, 97481)

// Creature actions (dev/TURN_BASED.md). Read 2026-09-30 from AddCommand's
// implementation after the probe showed it running about five times a
// second in combat; not yet verified in play.
//
// An object by its id, on the application: the script command AddCommand
// pops an object id and passes it here, `mov ecx, [g_clientExoApp]`, one
// argument, and gets the object back in eax.
DAO_ADDRESS(kCClientExoApp_GetObjectById, 10519)
// The typed lookups beside it, found 2026-09-30 by tools/script_backends.py
// (dev/object_lookups.csv): the same shape, then a cast through the virtual
// slot that only the named class answers with `this`. Each __thiscall, the
// id, `ret 4`. GetGameObjectById does not cast: the object as the array
// holds it. The creature, placeable and object lookups were checked in play
// 2026-09-30 by the probe's script checks (dev/LIBRARY.md): each found the
// object the engine's getters answered for, on every call. The waypoint's
// was not called, and GetGameObjectById is not used by the checks.
DAO_ADDRESS(kCClientExoApp_GetGameObjectById, 10507)
DAO_ADDRESS(kCClientExoApp_GetCreatureById, 10509)
DAO_ADDRESS(kCClientExoApp_GetPlaceableById, 10517)
DAO_ADDRESS(kCClientExoApp_GetWayPointById, 10521)
// The module, read 2026-09-30: the object manager (+0xCC) looks up the id
// at +0x2B8 and the result is cast through slot 7, CCModule's.
DAO_ADDRESS(kCClientExoApp_GetModule, 10493)
// Queue a command on an object: __thiscall(command, addToFront, override),
// `ret 0xC`. Seen in play 2026-09-30 (dev/TURN_BASED.md): about 200
// commands in a fight, all on CCCreature. Read 2026-09-30: it returns 0
// only for a null command; any other it puts in the queue, at the front
// (0x0073CCD0) or the back (0x00698C30), and returns 1, so the queue always
// takes the command and no caller frees one after. Before that: a
// player-issued command (+0x10) signals EVENT_TYPE_PLAYER_COMMAND_ADDED
// (94) to the object, with the command's type as int 0; an override of -1
// is looked up in the commands table (TABLE_COMMANDS, commands.gda) by the
// command's type, its AddBehaviorSelf column if the object's current
// command has the same type, else AddBehavior; then 1 clears the queue
// (ClearAllCommands, soft), 2 clears it hard, and 3 clears it, soft or hard
// by whether the new command's object 0 is not the creature's current
// target; 0 just adds. After, a UseAbility queued on a creature is passed
// on with its ability (0x0071DEE0).
//
// Its ten calls, by the address they return to, as a hook on it sees them:
//   0x0066E1B4  the scripts' AddCommand
//   0x006DA2C4  the client command table's AddCommand (below)
//   0x0073D054  QueueNewCommand, the engine's own: the player's orders
//               (QueuePlayerCommand, player issued) and the engine's
//               reactions (effects, weapon draws, animations, jumps, moves)
//   0x006A22AE  drive, the player steering the character directly
//               (player issued)
//   0x006A23C5  turn
//   0x006A3A19  interact, for CCCreature::InteractWithObject
//   0x00661A7E, 0x00661B1B  the script command SetActive: an animation and
//               a wait at the front, static
//   0x006D694C, 0x006D69CC  the client table's SetActive, the same
// So whose a command is shows in the command itself: Command::PlayerIssued.
DAO_ADDRESS(kCCObject_QueueCommand, 17550)

// Sending a message (dao/Messages.h), read 2026-09-30. Handlers are kept by
// a registry, a singleton of 0x28 bytes made on first use (Get), in a table
// indexed by the receiver's class id (its vtable slot 0, a number given to
// each class on its first registration) and then the message's type id
// (the message's slot 0): Register(&classId, &typeId, handler), which every
// receiver's handler setup calls, 314 handlers in all
// (dev/MESSAGE_HANDLERS.md). Dispatch(receiver, message) asks the
// receiver's +4 byte whether it takes messages, finds the handler (Find)
// and returns its answer, false for none. A class id is a record
// {number, parent's record}, and Find walks the receiver's chain and the
// message's, so a class inherits its bases' handlers: GameModeCombat,
// which registers none, answers with GameModeExplore's (its slot 0,
// 0x0076E7F0, gives a record whose parent is explore's, 0x00C06988). Send is the free function around
// it that 74 places call, __cdecl (message, receiver), a no-op for a null
// receiver: synchronous, on the caller's thread, and the message stays the
// caller's. The names are ours.
DAO_ADDRESS(kMessages_Send, 4886)
// Messages from text, read 2026-10-01 (dao/Messages.h, SendText). A movie's
// button and a key's binding name a message as text, and the engine makes
// the message from it. FromText, __cdecl (const std::wstring &), reads the
// text through a Messages::InputStringDataStream (0x00AF1474): its first
// word names the class in the registry of message makers (0x00489BC0), the
// message is made (0x004D22B0, by the name's hash) and reads its own
// fields from the rest (its slot 5), and is answered, or null for a name
// the registry has not. FromBindingText is the same over the input
// bindings' registry (0x00613A30), which the input mapper uses for a
// pressed key (0x0087C950). FromCallbackText, __cdecl (out {receiver,
// message}, const std::wstring &), takes "Receiver.Message args": the
// receiver's name up to the dot, found in the data tree's tokens and asked
// for its message receiver (its slot 19), and the rest to FromText. The
// movies' ExternalInterface SendMessage (GuiExternal::SendMessage,
// 0x0051C800) does the same from Flash values, sends the message
// (Messages::Send) and deletes it (its slot 4, with 1). A pressed key
// goes the same way: the input's dispatcher (0x0087D690) finds the
// bindings an input matches, has each one's text made into a message
// (0x0087C950, for the texts the current mode's group allows) and sends
// them all to the one receiver it was given, the game mode controller,
// the receiver RawInputMessage has too. The names are ours.
DAO_ADDRESS(kMessages_FromText, 4890)
DAO_ADDRESS(kMessages_FromBindingText, 19413)
DAO_ADDRESS(kMessages_FromCallbackText, 6984)
// The player's keys, read 2026-10-01 (dao/Input.h). The game mode
// controller's start (0x00641870) calls, in this order:
// Input::RegisterInputMessages (0x006DC900), which fills the modes' groups
// of texts; RegisterActions; ResetKeys; LoadKeyBindings.
//
// The actions are Eclipse::GroupedInputMappings' (its one object made by
// Get, kept at g_groupedInputMappings), each an Eclipse::
// GroupedInputMappings::GroupedInputs of 0x13C bytes:
// - AddAction, __thiscall (const std::wstring &name, const TalkString
//   &label, 0, 0), `ret 0x10`: makes the action unless the name is there
//   (the container at +0x58 asked, its slots 6 and 2). The label is the
//   engine's talk string, 0x24 bytes: a flag that its text is there, a
//   string reference, the text (0x004D1C70 makes one from a reference,
//   0x004D1EB0 reads the text, asking the talk table if the flag is 0).
// - AddMapping, __thiscall (const std::wstring &name, const uint32
//   *trigger, const std::wstring &text, const bool *), `ret 0x10`: what
//   the action's two keys send. The trigger is a device and an event with
//   no key (0x10000000 the keyboard; 0x20000 for the screenshot's and the
//   console's actions, 0x40000 and 0x50000 for others); the text names a
//   message ("TakeScreenshotMessage").
// - SetKey, __thiscall (const std::wstring &name, const uint32 *input,
//   int which of its two keys, bool), `ret 0x10`: the input is a code, the
//   modifiers and the device in one word (dao/core/InputCode.h). The game's
//   defaults pass 0 last; with 1 it goes on to look at the actions that
//   have the key already.
// - RegisterActions, __cdecl (): 53 AddAction calls with the labels'
//   string references (373290 on), then the mappings.
// - ResetKeys, __cdecl (): clears the keys (0x00893D70), sets the game's
//   defaults (0x006E9510, 56 SetKey calls) and reads keymap.gda's for the
//   language. Also called from 0x005E5F40.
// - LoadKeyBindings and SaveKeyBindings, __cdecl (): KeyBindings.ini in
//   the user's folder, read (0x00895580) and written (0x00895980).
//
// The modes' groups are another object (made by InputModes::Get, kept at
// g_inputModes): Group, __thiscall (const std::wstring &name), `ret 4`,
// answers the group of that name, the names being the modes' classes'
// ("GameModeExplore") and "DisableAll"; a group has a set of the texts it
// allows (at +0x30; Allow, __thiscall (const std::wstring &text), `ret 4`,
// adds one), a set it bars (+0x5C), and a flag (+0) that it bars all it
// does not allow. Allowed, __thiscall (const std::wstring &text), `ret
// 4`, is asked by the input mapper for each text a pressed key has
// (0x0087C950): true if a group that is on allows the text and none bars
// it. The names are ours.
DAO_ADDRESS(kGroupedInputMappings_Get, 24389)
DAO_DATA(kGroupedInputMappings, 102772)
DAO_ADDRESS(kGroupedInputMappings_AddAction, 24363)
DAO_ADDRESS(kGroupedInputMappings_AddMapping, 24375)
DAO_ADDRESS(kGroupedInputMappings_SetKey, 24377)
DAO_ADDRESS(kInput_RegisterActions, 15454)
DAO_ADDRESS(kInput_ResetKeys, 15458)
DAO_ADDRESS(kInput_LoadKeyBindings, 15456)
DAO_ADDRESS(kInput_SaveKeyBindings, 15457)
DAO_ADDRESS(kInputModes_Get, 24303)
// Where the game reads the mouse's place for its world pick, read
// 2026-10-02: GameModeExplore::CastCursorRay asks the input system (a
// singleton at 0x00C3B8E4, made by 0x00891C30) for input 43, an axis pair,
// through this: __thiscall (int out[2], const int *input), returning out,
// both 0 if the input is not allowed now. Its two axes are read from the
// devices' state (0x0087DA50), not from Windows: the exe's one
// GetCursorPos (Eclipse::PlatformWin32's slot 17, 0x004139C0) only says
// whether the cursor is at the window's edge, for scrolling. The name is
// ours.
DAO_ADDRESS(kInput_AxisPair, 24340)
// The cursor hidden and held, read 2026-10-15 for DAOLib #132:
// Eclipse::PlatformWin32, the game object's (kEclipseGame) at its +0xC8.
// Its slot 13 (0x00414630), __thiscall (), unless its flag at +0x5A is set:
// keeps the cursor's clip (GetClipCursor), clips it to one pixel where it
// is, tells the input system (0x0087DF00), hides it (ShowCursor(FALSE))
// and sets the flag. Slot 14 (0x00414700) undoes it all where the flag is
// set: the clip given back, ShowCursor(TRUE), the flag cleared. The explore
// camera's input read calls them for its right-drag orbit (0x007EDFFB,
// 0x007EE023), and the fly camera's look each frame (0x0076D620): 13 while
// its look is held, else 14.
DAO_DATA(kPlatformWin32_Vftable, 60745)
// Whether an input is down (dao/Input.h, Hold; DAOLib #95), read 2026-10-12
// from the drive command back. The player steering the controlled creature
// is a steering object's update each frame (0x00764150): it makes a
// direction (0x00763CD0, through 0x00763A00) and, while that is not zero,
// drives the creature by it and the camera's angle (0x007636A0, to
// 0x006ADCF0, which changes the creature's drive command or issues one,
// 0x006A21C0: type 0x21, player issued); when it is zero and the creature
// is driving, it stops it (0x00763810). The direction comes of asking the
// input system of numbered inputs whether each is down: forward is input 2
// (or 6), backward 3, and across is input 5 (or 27 with 14) less input 4
// (or 26 with 14); each counts 1 with the creature's run on (its +0x4A8)
// and 0.5 without. IsDown, __thiscall on the input system (const int
// *input), `ret 4`, a bool: false at once where the input modes do not
// allow the input now, else whether any key bound to it is down in the
// devices' state. AllowsInput, __thiscall on the modes (InputModes::Get's;
// const int *input), `ret 4`, a bool: true if a group that is on allows the
// input and none bars it, as Allowed answers for a text. Every one of
// IsDown's 84 calls asks AllowsInput first through it (the address
// library's session, reading GOG and EA the same day). The names are ours.
DAO_ADDRESS(kInput_IsDown, 24337)
DAO_ADDRESS(kInputModes_AllowsInput, 24258)
DAO_DATA(kInputModes, 102761)
DAO_ADDRESS(kInputModes_Group, 24295)
DAO_ADDRESS(kInputModeGroup_Allow, 24272)
DAO_ADDRESS(kInputModes_Allowed, 24264)
// One key to one action, and what an action answers to, read 2026-10-15 for
// DAOLib #130 (dao/Input.cpp, keys shared by actions whose modes do not
// overlap). SetKey (above) and LoadKeyBindings (0x0089573A, 0x00895863)
// first call TakeKey, __thiscall on the mappings (std::wstring *out name,
// const uint32 *key), `ret 8`: for every action's record (the mappings'
// +0xDC) and both its keys (+0x110, +0x114, 0x00893810), a key equal to the
// new one is cleared through the record's SetKey with 0, and the action's
// name kept for the controls screen's "already bound" line; then they give
// the action being bound the key through its record's SetKey, __thiscall
// (const uint32 *key, int which), `ret 8`, which writes the word and binds
// the key to each of the action's mappings. Those are at the record's +0xF0
// for the first key (+0x100 for the second), a vector (first +4, last +8)
// of 8 bytes each: a flag at +0, and at +4 either a numbered input (the
// flag 0; dao/Input.h's Withhold) or a handle into the input mapper's
// texts. The mapper (Get, __cdecl (), made on first use, kept at
// g_inputMapper) keeps the texts as a vector at its +4 (first +8, last
// +0xC) of 0x28 bytes each, the text a std::wstring at +8 (0x0087B630).
// A mode's group (InputModes::Group) answers for a text and a numbered
// input alike, each __thiscall (const std::wstring *) or (const int *),
// `ret 4`, a bool: whether it allows it (InputModeGroup::AllowsText and
// AllowsInput) and whether it bars it (BarsText, BarsInput); InputModes::
// Allowed and AllowsInput ask them of every group that is on. The names
// are ours.
DAO_ADDRESS(kGroupedInputMappings_TakeKey, 24351)
DAO_ADDRESS(kGroupedInputs_SetKey, 24350)
DAO_ADDRESS(kInputMapper_Get, 24119)
DAO_DATA(kInputMapper, 102721)
// A numbered input's mapping is not the input but a binding of the input
// system's (Get, __cdecl (), kept at g_input), made for it once a key
// (0x00892210): an index into its vector at +4 (first +4, last +8) of 12
// bytes each, the key word first, the input's list of them in a map at
// +0x10, which IsDown reads. Bind, __thiscall (const int *binding, const
// uint32 *key), `ret 8`, makes one again for a key; the record's SetKey
// calls it for each of the action's numbered inputs (0x00894C40 made them).
// Read 2026-10-15 after DAOTurnBased's test of #130 found the first
// reading, which took the binding for the input, shadowed nothing.
DAO_ADDRESS(kInput_Get, 24332)
DAO_DATA(kInput, 102766)
DAO_ADDRESS(kInput_Bind, 24310)
DAO_ADDRESS(kMessageRegistry_Get, 3100)
DAO_DATA(kMessageRegistry, 100247)
DAO_ADDRESS(kMessageRegistry_Dispatch, 5090)
DAO_ADDRESS(kMessageRegistry_Find, 5089)
DAO_ADDRESS(kMessageRegistry_Register, 5110)

// The Flash UI's calls into the game (dao/Gui.h), read 2026-09-30. GFx
// calls the game's FSCommand handler and ExternalInterface handler, slot 1
// of each: __thiscall (movie, command, args), `ret 0xC`, and (movie,
// method, args, count), `ret 0x10`. Each passes the call on to the object
// in this global, most likely GUI::ScaleformManager by where its methods
// are (0x005166C0 is one; to confirm by its RTTI in play), which takes the
// movie's user data (GFxMovie's slot 43), the game's object owning the
// movie, and hands owner, name and arguments to a registry at its +0xA8:
// a map at +0 of FSCommand handlers, __cdecl (owner, args), and one at
// +0x14 of ExternalInterface handlers, __cdecl (owner, args, count), each
// keyed by the name's CRC-32 (0x004CA960: the standard CRC-32 of the
// name's bytes, case-sensitive). The two Dispatch functions look the name
// up (Find) and call the handler; the two Register functions, __thiscall
// (const std::wstring &name, handler), `ret 8`, add one, as the game does
// 67 FSCommands and 17 ExternalInterface calls at start-up (0x00521AF0,
// 0x00716540). An ExternalInterface handler answers through the movie,
// its owner's +0x1C, slot 42 (GFx's SetExternalInterfaceRetVal). The
// names are ours but for the GFx classes'.
DAO_DATA(kGuiManager, 100415)
// Whether the mouse is the interface's, read 2026-10-01 (dao/Gui.h,
// MouseOverInterface): __thiscall on the manager, no arguments. False
// before the manager is set up (its +0x15); else true unless the set of
// movies the mouse is over (+0x80) is empty and no movie holds a mouse
// press (+0xA0). Explore mode's chooser asks it through the GUI system's
// slot 71 before it picks (dao/GameModes.h). The name is ours.
DAO_ADDRESS(kScaleformManager_MouseOverInterface, 6593)
DAO_ADDRESS(kEclipseGFxFSCommandHandler_Callback, 6584)
DAO_ADDRESS(kEclipseGFxExternalInterface_Callback, 6585)
DAO_ADDRESS(kGuiCommands_DispatchFSCommand, 6866)
DAO_ADDRESS(kGuiCommands_DispatchExternalInterface, 6867)
DAO_ADDRESS(kGuiCommands_Find, 6864)
DAO_ADDRESS(kGuiCommands_RegisterFSCommand, 6878)
DAO_ADDRESS(kGuiCommands_RegisterExternalInterface, 6879)
DAO_ADDRESS(kCrc32, 4715)
// The game's movies, read 2026-09-30. The owner the calls above carry is
// the manager's record of a movie: its name, an engine std::wstring at +0
// (0x1C bytes), then the movie at +0x1C, GFx's GFxMovieView. The manager
// keeps its records in a std::set at +0x70 (MSVC 2005's tree: the head at
// +4, the count at +8, the record's pointer at a node's +0xC), walked by
// the engine's own checked iterator: Begin and Next, each __thiscall
// (set, iterator {set, node, valid}), `ret 4`. FindMovie, __thiscall
// (const std::wstring &name), `ret 4`, walks it for a record of that name;
// DragonAgeGUISystem, which holds the manager at +0x120, finds a movie so
// for its slots 80 and 81 (IsMovieOnStage and CallFunctionOnMovie, run by
// the ActionScript of MovieUtils). InvokeOnRecord is a record's own
// Invoke: a jump to its movie's slot 18. The names are ours.
DAO_ADDRESS(kGuiMovieSet_Begin, 14716)
DAO_ADDRESS(kGuiMovieSet_Next, 14717)
DAO_ADDRESS(kScaleformManager_FindMovie, 6603)
DAO_ADDRESS(kGuiMovie_Invoke, 6741)
DAO_ADDRESS(kDragonAgeGUISystem_IsMovieOnStage, 11443)
DAO_ADDRESS(kDragonAgeGUISystem_CallFunctionOnMovie, 11444)
// Loading a movie (dev/GUI.md), read 2026-09-30. GetMovieDef, __thiscall
// (out GFxMovieDef*, const std::wstring &name), `ret 8`: the name's .swf
// becomes .gfx, a definition already loaded comes from the manager's cache
// at +0x18, and a new one from GFxLoader::CreateMovie (filename, flags).
// CreateMovieView makes a view of it: an image visitor over the
// definition, its slot 17 (GFx's CreateInstance), the view set up through
// four of its slots, and cached at +0x44. OpenMovie, __thiscall (screen,
// const std::wstring &name, layer, flags), `ret 0x10`: FindMovie, or a
// view and a new 0x108-byte record (GuiMovie_Construct) with the third
// argument at +0x2C, put in the set; then shown and activated. The GUI
// system's OpenScreen (its slot 17) calls it with a screen's descriptor's
// movie name (+0x74), +0x9C and +0xA4, and copies the descriptor's +0xBF
// to the record's +0x100. What screen, layer and flags mean is for the
// probe to show (it logs every call). The manager is made and its font
// library loaded by Initialize, from the GUI system's slot 5.
DAO_ADDRESS(kScaleformManager_Initialize, 6697)
DAO_ADDRESS(kScaleformManager_GetMovieDef, 6673)
DAO_ADDRESS(kScaleformManager_CreateMovieView, 6674)
DAO_ADDRESS(kScaleformManager_OpenMovie, 6676)
DAO_ADDRESS(kGuiMovie_Construct, 6782)
DAO_ADDRESS(kGUISystem_OpenScreen, 6954)
// The HUD's seven scenes (0x2F, 0x28, 0x1C, 0x2A, 0x2B, 0x29, 0x2C), read 2026-10-23 for
// DAOLib #83: DragonAgeGUISystem's slot 28 closes them, each through its slot 14 (the
// scene's slot 6, which drops its movie), and slot 29 hides them and keeps their movies,
// each through its slot 15 (HideScreen); both __thiscall with nothing, `ret`, each first
// calling its slot 41 (+0xA4). Slot 27 shows them (slot 13 each). The GUI's game mode change
// (its slot 77, 0x0060BC60, called by the switch at 0x00641ED4) shows them for explore and
// combat, hides them for the GUI mode, keeps them up when the loading mode is in state 3,
// and closes them for every other mode, a door's loading among them (seen in play: from
// 0x00641ED6, the loading mode in state 7). The names are ours.
DAO_ADDRESS(kDragonAgeGUISystem_CloseHud, 11204)
DAO_ADDRESS(kDragonAgeGUISystem_HideHud, 11205)
DAO_ADDRESS(kGFxLoader_CreateMovie, 26910)
// Every screen is a GUI::GUIScene (the base of GUIMain, GUIInventory,
// GUIPopupLayer and the rest, a SmartObject), made by
// GUIScene(name, filename, layer, b, flags) and kept by the GUI system in
// a map at +0x58, id to scene, found by name through its slot 12. Its own
// fields from +0x4C: the filename, a SmartData string at +0x74 (member
// "Filename"), layer +0x9C, b +0xA0, flags +0xA4, "Cached" +0xA8, bytes
// +0xBC-0xBF. OpenScreen passes a scene's filename, layer and flags to
// OpenMovie with the scene's id first. The layer is the stacking order,
// by the values the game gives (read 2026-09-30, from the 42 scenes the
// GUI system's constructor and the scenes' own make): 0 the floaty text,
// 1 the HUD (GUIMain) and army control, 2-3 the in-game menus (map,
// journal, skills, crafting, party picker), 4 item examine, 5
// conversation, cutscene, death, main menu, save and load, 6 options,
// login, credits, 7 popups and achievements, 8 the loading screens. Flags:
// 0x20 on the modal ones (options, popups, main menu, credits), 0x10 on
// loading screens, 0x04 on skills and spells, 0x02 on screenshots. b is 0
// for most, 10 for the character record, journal, map, skills and spells.
// PushGUIScreenMessage (a screen's name) pushes a scene by name.
DAO_ADDRESS(kGUIScene_Constructor, 33297)
DAO_ADDRESS(kGUISystem_SceneIdByName, 6961)
DAO_ADDRESS(kGUISystem_Scene, 6962)
// A scene's registration, read 2026-09-30: the GUI system's constructor
// makes 61 scenes, 0xC0 bytes each from operator new, and adds each with
// AddScene, __thiscall (id, scene), `ret 8`: the ids are 0-0x3F; a new id
// goes into the map, the scene's +0xB8 gets the GUI system, and the scene's
// slot 2 runs; an id already there is ignored. The GUI system is the
// application's +0x158 (SetGUISystem, 0x005D4F20, from the start-up that
// makes it, 0x278 bytes). By name, from its vtable: slot 17 OpenScreen
// (above), slot 18 closes (the manager drops the record, and the cached
// definition too if asked), slot 19 hides (the record deactivated and its
// shown flag cleared, the movie kept), each __thiscall (const
// std::wstring &name), `ret 4`.
DAO_ADDRESS(kGUISystem_AddScene, 6965)
DAO_ADDRESS(kGUISystem_CloseScreen, 6963)
DAO_ADDRESS(kGUISystem_HideScreen, 6956)
DAO_ADDRESS(kCClientExoApp_SetGUISystem, 10491)
// Where a movie's file comes from, read 2026-09-30. GetMovieDef gives GFx a
// scene's filename with ".swf" put on a bare name; GFx asks the game's
// EclipseGFxFileOpener, whose OpenFile (its slot 1) makes a
// GUI::ScaleformResourceFile, a GFile over the engine's resource system:
// it looks the name up as a resource of type 3 (FindResource, __cdecl
// (out handle, const std::wstring &name, type), the handle one word, freed
// by ReleaseResource, __thiscall on it), and failing a name ending ".swf"
// tries it with ".gfx". So a scene "turnorder" is the resource
// "turnorder.gfx", found wherever the resource system finds the game's
// own (its override folders and archives; not read here).
DAO_ADDRESS(kEclipseGFxFileOpener_OpenFile, 6592)
DAO_ADDRESS(kScaleformResourceFile_Constructor, 6930)
// The file itself, read 2026-10-01 (dao/Gui.h, OnMovieLoad): 0x38 bytes, a
// GFile of 19 slots. It reads from the resource's bytes in memory: where
// they begin is at +0x30 and where it has read to at +0x34 (its Tell,
// slot 4, is their difference; a file that found no resource has 0 at
// +0x30), and the resource's handle is at +0x2C. Its length (slot 6,
// __thiscall, no arguments) asks the handle. Read (slot 10, (buffer,
// count), `ret 8`) copies the lesser of the count and what is left, which
// it gets through its own slots, so a file whose two pointers and length
// slot are another's reads another's bytes. GFx takes "GFX" and "FWS" as
// well as the compressed "CFX" and "CWS" (the four signatures are tested
// at 0x00913FF4 on).
DAO_DATA(kScaleformResourceFile_Vftable, 63755)
// A movie told of the data tree's changes, read 2026-10-01 (dao/Options.h).
// A movie asks to be told a token's value with ExternalInterface's
// AddTokenObserver(label, "", token) (GuiExternal::AddTokenObserver,
// 0x0051CD50), which makes a GUI::ScaleformTokenObserver on the movie's
// record (0x0051A3C0) and answers its id: the record's counter at +0x98,
// counting down, so the engine's ids run down from 0xFFFFFFFF. (A movie
// can also name an id of its own, AddTokenObserverWithID, and those count
// up from 0.) The movie's script keeps the id's callback.
//
// DeliverTokens, __thiscall on the movie's record (bool), `ret 4`, is run
// for every record by 0x00511CA0. For the changes waiting (ids at +0xA8,
// the values as strings at +0xB8) it sets two arrays in the movie through
// its slot 13, "TokenManager.BatchTokenReplacementCallbackIDs" of ints
// and "TokenManager.BatchTokenReplacementValues" of wide strings, and
// invokes "TokenManager.PerformBatchTokenReplacements", the movie's own
// function that calls each id's callback with its value. The name is ours.
DAO_ADDRESS(kGuiMovie_DeliverTokens, 6777)
DAO_ADDRESS(kResources_Find, 5034)
// The resource manager's sources, read 2026-10-09 (DAOLib #43). At start
// SetUpSources (__cdecl, no arguments; answers in al whether the manager
// is set up, and ECWinApp's Initialize ends the game if not) adds the
// game's folders and
// archives to the manager, each by an alias and a kind and a priority,
// a lower priority looked in first: WRITE_OVERRIDE: 0 and OVERRIDE: 1
// (packages\core\override, kind 1, a folder walked with its
// subfolders), PATCH: 100000, DATA: 100005. Manager (__cdecl, no
// arguments) answers the manager, made on first use. AddSource,
// __thiscall on it (const std::wstring *path, kind, priority, 0, void
// **source or null, 1, 0) as OVERRIDE: is added, `ret 0x1C`: a path it
// has already is that source again, its count one more (released by
// name through 0x004C80F0, one count a call); otherwise it makes the
// source by the kind's factory (kind 1, a DirectorySource, which walks
// its folder as it is made), files it by priority and indexes its
// resources at once (0x004C7C50, ID 4670). True for a source made or
// joined; false for an empty path or a kind with no factory. Read in
// full by the address library's session. Its fourth argument is a list
// of names to skip (a std::vector<std::wstring> *, null for none), its
// sixth whether the folder is watched for changes (re-listed every fifth
// frame); kind 1 takes in subfolders and makes any .erf or .rim inside a
// source of its own, kind 0 (ENV: alone) loose files only. A tie in
// priority is broken by the source's slot 8. The names are ours.
//
// The order, read 2026-10-12 (DAOLib #106, dev/FILES.md): the manager's
// list of its sources (its +8 the head of a std::list, +0xC its count, a
// node's source at +8) is kept by AddSource's insert, which puts a new source
// before the first for which 0x004E6660 answers true: that one's priority
// (+0x60) is higher, or equal and its slot 8 higher (a DirectorySource 0, or
// 1 with its +0xC0 set; a RIMSource 2; an ErfSource 3), or both equal and its
// name (+4) earlier. Its index (0x004C7C50) keeps each name's sources in the
// same order, and a file comes from the first. The add-ins' priorities: the
// loader (0x005EF460) counts down from 9000 by 7 an add-in, and 0x005ECB30
// adds a folder under ADDINS: at the priority given, or under BUILD_ADDINS:
// at 10000 more.
DAO_ADDRESS(kResMan_SetUpSources, 343)
DAO_ADDRESS(kResMan_Manager, 4667)
DAO_ADDRESS(kResMan_AddSource, 4671)
// A source released by the name it was added with, read 2026-10-12 (DAOLib
// #85): __thiscall on the manager (const std::wstring *name), `ret 4`. Under
// the manager's lock it finds the source of that name that is not already
// going (its +0x68 clear) and takes one count from it (0x004C7A10, its one
// caller): at none left the source is closed (its slot 7) and, with none
// of its resources held (+0x2C), taken out of the manager's list and its
// index (0x004C6330) and deleted by a task of the manager's own
// (ECResMan::DeleteSourceTask). True if there was such a source. AddSource
// and this are a pair the game uses while it runs, some fifty callers
// each, so neither belongs to start alone. The name is ours.
DAO_ADDRESS(kResMan_ReleaseSource, 4672)
// The names the manager has, by type (dao/Resources.h, DAOLib #96), read
// 2026-10-12 from the game's own use of it: gathering its merged tables, it
// asks for every file of the type GDA (0x006FBAD9, the list then matched
// against a prefix, PRCSCR_). ListNames, __thiscall on the manager (out
// list, const std::wstring &source, const std::wstring &type, char loaded,
// const std::wstring &without), `ret 0x14`, the last source's answer, and
// true too where the lock was not had and nothing was listed (the address
// library's session, reading GOG and EA the same day), so true says nothing
// of the list: under the manager's lock (0x004C5390, 0x004C53B0) it goes through every
// source that is not closed (+0x68), only the one named `source` if that is
// not empty, none whose name holds `without` if that is not empty, and has
// each add its names to the list by its slot 5 (ResourceSource's 0x004E6A90,
// DirectorySource's 0x004CED00, `ret 0x10`): the type in lower case, a '.'
// put before it if it has none, every resource not marked (+0x3A) whose name
// ends with it, each name once (0x004E66D0), and with `loaded` 1 only those
// whose state is 2 (any other value but 0 takes none). An empty type takes
// every name. The list is 0x28 bytes: a std::list<std::wstring> (its
// head at +4, its size at +8; a node's next at +0 and its string at +8) and
// a std::wstring after it (+0xC). NameList's constructor, __thiscall
// (std::wstring by value, which it copies into +0xC and destroys), `ret
// 0x1C`, and its destructor, __thiscall with no arguments, are how the game
// makes and frees one on its stack around the call. The names are ours.
DAO_ADDRESS(kResMan_ListNames, 4632)
DAO_ADDRESS(kNameList_Construct, 5272)
DAO_ADDRESS(kNameList_Destroy, 5273)
DAO_ADDRESS(kResources_Release, 5021)
// A resource's bytes (dao/Resources.h), read 2026-10-02 from the movie
// file's constructor (its call at 0x00523412) and its length (slot 6,
// 0x00523140, a tail-jump into Size). The handle Find gives is one word, a
// pointer to the resource; Data and Size, each __thiscall on the handle
// with no arguments, lock the resource (0x004D5540), read where its bytes
// are (Data, through 0x004BF060) or how many (Size, the resource's +0x30),
// and unlock it (0x004D5570). Both answer 0 when the lock is refused, so a
// Size of 0 alone does not say the file is empty; and Data, finding no
// bytes, first builds a message chosen by the resource's state
// (0x004BEBC0), most likely a line in the game's log (the address
// library's session, reading GOG and EA the same day). The third argument of Find is a mode, not a file
// type: the file's name carries its extension, and the movie file asks
// with 3 (the global at 0x00AF046C, beside 1 and 2), which has its bytes
// there on return, except on the main thread exploring or in a fight
// (below). The type numbers of the engine's older table
// (0x004C9E3E, ncs 2010, 2da 2017: NWN's) are not what Find takes. The
// names are ours.
DAO_ADDRESS(kResources_Data, 5018)
DAO_ADDRESS(kResources_Size, 5019)
// A file's bytes from its source and not from the resource (dao/Resources.h,
// DAOLib #100), read 2026-10-12. What Data answers, the resource's +0x20, is
// the file only until the resource's first use: ReadOnFirstUse (below) then
// hands the bytes to the processor of the file's type, frees them and puts
// the processor's own buffer there (0x004BEA24, 0x004BEA30). The processors
// are a table by extension (0x00C2C0DC, looked up at 0x004E6F30 with what
// follows the name's last '.', 0x004E6EE0) with one entry, made at
// 0x008471D3: "dds", TexManager::DDSResourceHelper (vftable 0x00B2B2A4). So
// a texture the game has used answers bytes that are not its file, of the
// file's length (+0x30), and every other type answers its file.
//
// The engine's own fetch cannot be asked again for a buffer of ours: a
// source's slot 1 (ErfSource 0x004D0930 with the archive's one FileAccess,
// its +0xB8; DirectorySource 0x004CEC00 with the path, its folder's at +0xA4
// and the resource's name after it, 0x004C3360 being the two strings joined)
// makes a ReadRequestTask whose Run seeks to the resource's +0x28, allocates
// its +0x2C bytes and stores that buffer in the resource's +0x20
// (0x004E9E20), over what the game draws from. RIMSource's slot 1
// (0x004CC1B0) fetches nothing: a RIM is read whole by its own task
// (0x004CE970), and its resources are given places in that block, the
// block's address less its file offset (+0xE8, +0x100) plus the resource's
// +0x28 (0x004CE8E0), so +0x28 is an offset in the file for a RIM too.
//
// A source's slot 6, __thiscall (bytes, size), `ret 8`, true when done, is
// what ReadOnFirstUse runs over the fetched bytes first: ErfSource's
// (0x004D0790) a cipher it holds at +0xA8 with eight bytes of key at +0xB0,
// which is how an add-in's enciphered archive reads as plain; the others'
// (0x00821560) answer true. Lock, __thiscall (bool: 0 waits, 1 only
// tries), `ret 4`, true when held, and Unlock, __thiscall with no
// arguments, are the source's lock (the critical section at its +0x6C,
// 0x004CB2B0 and 0x004CB300), which ReadOnFirstUse holds around slot 6.
// The three vftables tell a source's kind; a RIMSource is an ErfSource (its
// base at +0, with Multithreading::Task at +0xD0). A source's +4 is the
// engine's name for it, an alias and a file (TEXTURES:texturepack.erf,
// DATA:guiexport.erf) for an archive and the folder's path for a folder,
// seen in play 2026-10-12: no path to open an archive by. Its one open
// file is the FileAccess at +0xB8 (the vftable below, the handle at its
// +4), which slot 1 uses only with the source's +0x68 clear and +0x69 set.
// The names of the two functions are ours.
DAO_ADDRESS(kResourceSource_Lock, 5439)
DAO_ADDRESS(kResourceSource_Unlock, 5440)
DAO_DATA(kErfSource_Vftable, 63047)
DAO_DATA(kDirectorySource_Vftable, 63048)
DAO_DATA(kRimSource_Vftable, 63045)
DAO_DATA(kFileAccess_Vftable, 62906)
// A texture made from a .dds (dao/Resources.h, DAOLib #228), read 2026-10-21.
// The processor's slot 1 (TexManager::DDSResourceHelper, 0x00847670,
// __thiscall (bytes, size, out buffer, out size, ?), `ret 0x14`) asks the
// texture manager (0x008472C0, its singleton) to make the texture
// (0x008456D0), which calls MakeTexture, __cdecl (bytes, size, the driver's
// slot 7, a flag the processor sets from the byte at +0xC8 of 0x00C31650),
// answering the texture or null: it checks "DDS " and a header of 0x7C
// bytes, then makes a 2D, cube or volume texture through the driver,
// D3DDriver::D3DTexture2D, D3DTextureCubeMap or D3DTextureVolume, whose
// IDirect3D*Texture9 is at +0x4C, +0x40 and +0x48 (each destructor's
// Release, slot 2, of it). So a texture is made within ReadOnFirstUse, on
// the thread that first reads its file, under its source's lock. A
// texture's D3DResetable (the 2D one's at +0x38) releases its +0x4C and
// makes it again on a device reset, unless the object's +0x5C is 1. The
// function's name is ours; the classes' are the game's.
DAO_ADDRESS(kTexManager_MakeTexture, 23042)
DAO_ADDRESS(kD3DTexture2D_Destructor, 23915)
DAO_ADDRESS(kD3DTextureCubeMap_Destructor, 23910)
DAO_ADDRESS(kD3DTextureVolume_Destructor, 23923)
DAO_DATA(kD3DTexture2D_Vftable, 70050)
DAO_DATA(kD3DTextureCubeMap_Vftable, 70046)
DAO_DATA(kD3DTextureVolume_Vftable, 70054)
// A RIM's file (dao/Resources.h, DAOLib #102), read 2026-10-12. A RIMSource
// holds no open file once it has loaded: its task (0x004CE970) reads
// everything after the directory into one block (0x004CE5B0: a seek to the
// first file's offset, the source's +0x100, and +0x104 bytes read into
// +0xEC), then deletes the source's FileAccess and clears +0xB8 (0x004CE739).
// A RIM whose +0x108 is not zero is unpacked first (0x004CC370), and its
// files' offsets are then into the unpacked block and not the file. One
// with bit 0x10 of its flags (+0x10C) frees the block when its load is done
// (0x004CEAB1), so nothing of the file is in memory after.
//
// So the file is opened again by its path, which the engine's own opener of
// an archive (0x004D0800) makes from the source's name, an alias and a file,
// with the alias table's resolver: __thiscall (std::wstring *out, const
// std::wstring &name), `ret 8`, on what Get answers (a __cdecl with no
// arguments; null before the table is made). It constructs `out`, and gives
// it the path the alias before the name's ':' stands for and, straight
// after it, what follows the ':' in lower case; an alias the table has not
// is put back before its ':', which is how a full path passes through, and
// a name with no ':' is given back as it is. The table is a list (alias at
// +0, path at +0x1C, next at +0x38) made at start-up; Path takes no lock.
// The address library's session read both in the three builds, 2026-10-12:
// the same code in each. The two names are ours.
DAO_ADDRESS(kFileAliases_Get, 4467)
DAO_ADDRESS(kFileAliases_Path, 4712)
// Find's fast way, read 2026-10-03 for the model-load crash (DAOLib issue #3,
// issue #3). On the main thread (the id in 0x00C2B590, set at
// start-up) with the game exploring or in a fight (GameModeController's
// mode 0 or 1, through 0x005D5E70), Find looks the file up by name
// (0x004D5A90) and hands it back as it is, its mode not passed on. On every
// other thread and in every other mode it waits: WaitLoaded, __cdecl
// (handle *), loops while the resource's State is 1, loading, pumping
// 0x004E6160, and answers whether it reached 2, loaded. State, __thiscall on
// the handle, answers 0 for none; the engine's own words for its states
// (the table at 0x004D5798) are Not Loaded, Loading, Loaded, Error On Load,
// the state at the resource's +0x34. LoadModelHierarchy, __thiscall (const
// std::wstring *name), `ret 4`, true if it read the model (called by
// GraphicsStructuresManager's slot 39, 0x0057A6F0, for a model not yet
// cached), parses what Size and Data answer (0x00596830, which refuses null
// bytes, too few, or no "GFF V4.0") without asking the state. Its view of
// the bytes (0x004D5360) is zeroed when the header's data offset (+0x18) is
// 0 or past the size, and nothing after checks it: the fault at 0x004D53B4
// reading 0x24 is that zeroed view, so the bytes there began "GFF V4.0" but
// the header was not whole (read by the address library's session,
// 2026-10-03). A file read while it streams in fits; so does one freed under
// the parse. The names are ours.
DAO_ADDRESS(kGraphicsStructures_LoadModelHierarchy, 9345)
DAO_ADDRESS(kResources_WaitLoaded, 5028)
DAO_ADDRESS(kResources_State, 5013)
// A resource's first read, read 2026-10-03 (issue #3):
// ReadOnFirstUse, __thiscall on the resource, run by Data (through
// 0x004BF060) when the state is 2. If the read flag (+0x3D) is 0 it takes
// the archive's lock (0x004E6580 on [res+0x1C], waiting; a critical
// section that refuses a second entry by its holder unless its +0x1C
// says so), reads the bytes into +0x20 through the archive's slot 6 (data,
// size +0x2C), finds a processor for the type (0x004E6EE0, 0x004E6F30),
// which may replace +0x20 with a buffer of its own, deleting the old
// unless +0x3C, then sets the flag and unlocks (0x004E65A0). The flag is not
// checked again under the lock: two threads asking for one unread resource
// both read it, unless both come through Resources::Data, which takes the
// same lock around it; ReplaceBuffer calls it holding the lock too. The name
// is ours.
DAO_ADDRESS(kResource_ReadOnFirstUse, 4435)
// A resource's buffer replaced, read by the address library's session
// 2026-10-03 (issue #3): ReplaceBuffer, __thiscall on the
// resource with three arguments, `ret 0xC`. Under the archive's lock
// (0x004BEF83) it frees +0x20 and installs a new buffer with the read flag
// cleared, when +0x38 and +0x3C are 0, then reads it (ReadOnFirstUse at
// 0x004BEFCD); called by IDs 4860, 4861 and 5438. Resources::Data takes the
// same lock around ReadOnFirstUse but gives it back before its caller uses
// the bytes, so a parse reads them unlocked. The name is ours.
DAO_ADDRESS(kResource_ReplaceBuffer, 4441)
// A resource's handles, read by the address library's session 2026-10-03:
// the word at +0x38 counts them (raised by the manager's lookup, which
// Find's fast way goes through too, and by a handle's copy; lowered by
// 0x004BF010, never below 0). At 0 it runs FreeWhenUnused, __thiscall on
// the resource, no arguments: under the source's lock, with the count 0 and
// +0x3C 0, it cancels a load still pending or frees +0x20 and sets the state
// to 0. A held handle is the pin: ReplaceBuffer and this refuse while the
// count is above 0, and LoadModelHierarchy holds its handle across the
// parse. The source (+0x1C) is an ECResMan::ResourceSource, its lock built
// re-entrant (0x004E6C50). The name is ours.
DAO_ADDRESS(kResource_FreeWhenUnused, 4440)
// A resource moved in its array, read 2026-10-03 (issue #3). A
// resource is 0x40 bytes: its name, the engine's std::wstring, at +0 (the
// constructor 0x004BE890), the source +0x1C, the buffer +0x20, a pending
// load +0x24, the size +0x2C, the state +0x34, the handles +0x38, the flags
// +0x3A to +0x3D. It is held by value in a std::vector at the source's
// +0xBC, whose helpers copy it field by field, all __cdecl (first, last,
// third): std::copy (third the destination: an erase), std::copy_backward
// (the destination's end: an insert), std::fill (a value) and
// std::_Uninit_copy (new storage: a regrowth). The names are ours.
DAO_ADDRESS(kResourceArray_Copy, 4769)
DAO_ADDRESS(kResourceArray_CopyBackward, 4770)
DAO_ADDRESS(kResourceArray_Fill, 4778)
DAO_ADDRESS(kResourceArray_CopyToNew, 4786)
// A resource destroyed and a source closed, read by the address library's
// session 2026-10-03 (issue #3). ~Resource, __thiscall, no
// arguments: stops or drops a load at +0x24, then frees the buffer unless
// +0x3C, whatever the count of handles, which it leaves as it was. An
// ErfSource (RIMSource derives from it) holds its resources in a std::vector
// at +0xBC, built once from the archive's table, and its name, a
// std::wstring, at +4. Close (slot 7, __thiscall, no arguments) sets +0x68,
// which makes the manager's lookup skip the source; it and Empty (slot 3,
// bool __thiscall (bool, bool), `ret 8`, the arguments unread, true on every
// path) erase each resource without handles, or with handles but Loading,
// and resize the array to 0 unless one was kept, destroying them all. Add
// (slot 4, bool __thiscall (const std::wstring *name, void *), `ret 8`)
// grows the array by one for a name not in the source's map.
DAO_ADDRESS(kResource_Destroy, 4437)
DAO_ADDRESS(kErfSource_Close, 4861)
DAO_ADDRESS(kErfSource_Empty, 4860)
DAO_ADDRESS(kErfSource_Add, 4864)
// A resource's bytes read from its archive, read 2026-10-03
// (src/fix/ArchiveRead.h): ReadRequestTask::Run, slot 2 of its vftable
// (0x00AF1418), __thiscall (float, unread), `ret 4`, a status (3 read, 5
// failed, 4 stopped). Under only its own load's lock (0x004CB2B0 on
// [[this+8]+8]) it seeks the file at +0x18, the source's, shared (or one it
// opens itself through 0x004C9170 and deletes), to the resource's +0x28
// (0x004E9F05), allocates +0x2C bytes into its +0x20 and reads them
// (0x004E9F2C). FileAccess (the vftable 0x00AF0474) holds a handle at +4:
// Seek, __thiscall (uint8 origin, int32 low, int32 high), `ret 0xC`,
// SetFilePointer, the new position's low half or -1; Read, __thiscall (void
// *buffer, uint32 size, uint32 count), `ret 0xC`, ReadFile at the file's
// position with no OVERLAPPED, the bytes read, counted at 0x00C2BE50.
DAO_ADDRESS(kReadRequestTask_Run, 5555)
DAO_ADDRESS(kFileAccess_Seek, 4678)
DAO_ADDRESS(kFileAccess_Read, 4681)

// Saving and loading (dev/SAVES.md), read 2026-09-30. SaveLoadProxy's
// SaveGameMessage and LoadGameMessage handlers take a slot and a kind (0 a
// manual save, slots to 500; 1 an autosave, 1-4; 2 the quicksave; 3 one
// more, by the checks). SlotFolder, __cdecl (out, const std::wstring
// &character, slot, kind), formats "%ls%ls\\Saves\\%ls%d": the documents
// root, the character, the kind's prefix (Slot_, AutoSave_, QuickSave_)
// and the slot. Write, __thiscall (slot, kind, const std::wstring &name,
// ?), `ret 0x10`, on an object whose +0x18 is the character's name: the
// slot's folder, then <name>.das and <name>.das.met opened with
// ECFileAccess and written ("savegame" for an empty name). FindSave,
// __cdecl, looks for "*.das" in a slot's folder, for loading. In play,
// 2026-09-30: FindSave was called for a load (character, slot 2, kind 0);
// a quicksave asked for slot -1, kind 2, and Write was called with the slot
// resolved (1, kind 2) and returned 1. An earlier quicksave had not called
// it, for a reason not read (the worker's check below). The names are ours.
DAO_ADDRESS(kSaveGame_SlotFolder, 12666)
DAO_ADDRESS(kSaveGame_Write, 12680)
DAO_ADDRESS(kSaveGame_FindSave, 12676)
// Where a save is done, read 2026-09-30: in the loading game mode, whose
// slot 8 calls the Worker, a state machine in numbered steps behind the
// SaveScreen movie. Write runs only when a check (0x004C9B10) passes. Step
// 10 asks for the save's screenshot path. ScreenshotPath is a
// SaveLoadProxy method (the application's +0x150), __thiscall (out path,
// const std::wstring &character, slot, kind, bool mustExist), `ret 0x14`:
// the slot's folder and screen.dds, or an empty path if mustExist and the
// file is not there (it opens it to see). In play, 2026-09-30, the save and
// load screen called it for every slot of every character as it filled
// (mustExist), and a save once, after Write (not mustExist).
DAO_ADDRESS(kGameModeLoading_Worker, 15080)
DAO_ADDRESS(kSaveLoadProxy_ScreenshotPath, 15140)
// The loading mode's states, read 2026-10-10 for DAOLib #64 (could the
// areas behind a map's doors be loaded ahead). The mode's update (Update,
// its slot 8, __thiscall (float seconds), `ret 4`) runs one state a frame,
// by the word at its +0x84 (its slot 13 reads it, its slot 14, 0x006C63F0,
// sets it), then its base's update (0x00694640). Each state is a machine in
// numbered steps, kept in the mode's data (a GameModeLoadingData, the
// mode's +0x130; its +0 an error, which stops them all): 2 and 8
// (0x006C76F0, not read; step at +0x64), 3 the Worker above (+0x70), 4
// (0x006C8800, +0x6C), 5 (0x006C8420, +0x6C), 6 (0x006C8EF0, +0x74) and 7
// (0x006C7380, no steps).
//
// DoAreaTransition (CCModule::ForScript_DoAreaTransition, 0x00600A40) makes
// the data with kind 7 and sets the game mode to loading. State 7 finds the
// area asked for (its tag a std::wstring at the mode's +0xD4, the waypoint's
// at +0xF0; 0x006C7080) and goes to 5 if the area is in the area list in
// memory, to 4 if not. The engine's own log calls them "Area Transition"
// and "Area List Swap". An area list is a file, modules\<module>\data\
// al_*.rim; of the single-player module's 190, 140 hold one area.
//
// State 5: 1 pauses; 2 tells the graphics (its slot 60); 10 (0x006C5C50)
// unloads the area left (0x0067B390: every room of its layout, slot 8),
// makes the new area's scene (its +0x62C) the graphics', and has the new
// area's layout load around the arrival point (0x0067B210, which reads the
// area's .bin and hands the point to the layout, 0x005CA590, and so to
// AreaLayout::Update); 13 waits while 0x006C6EE0 answers true: a party
// member's model still loading (the creature's +0x7BC), or, for any area of
// the list, a placeable (the area's vector at +0x244) or a creature
// (+0x254) not yet in (0x0067F420); then 0x006C73C0 puts the party at the
// point; 14 unpauses and sets state 6.
//
// State 4: 1 pauses; 3 and 4 save each area of the list left, one a frame
// (0x006FB3A0, 0x006FA5C0); 5 unloads it; 6 loads the new list
// (0x006FBFF0); 7 and 8 wait on its files (0x00A590C0); 9 builds its
// areas, some each frame (0x006FABB0: 0 going, 1 done, else failed); 10 to
// 14 as state 5's 10 to 14.
//
// State 6 follows each: 1 asks for explore, SetGameMode(0, 1, 0) at 0x006C8F91,
// whatever mode the loading began in (corrected 2026-10-18 by lib-agent-2;
// a second load request of kind 4, 5 or 7 replaces a waiting one); 4 sends
// the hero's area EVENT_TYPE_AREALOAD_PRELOADEXIT (38; 42 after a save's
// load) and runs the event queue at once (AIMaster::Update), so the area's
// own script runs inside the step; 6 runs AreaLayout::Update each frame and
// waits, paused, while rooms still load (0x005C84D0, the same count
// AreaLayout::Update unpauses on); 7 and 8 send the POSTLOADEXIT event (39
// or 43) and leave the mode.
//
// In play, 2026-10-10 (the probe's loading watch, dev/PROBE.md, file cache
// warm): Redcliffe village to its tavern, in the same list, 0.5 and 0.7 s,
// of it 0.24 and 0.39 s in state 5's step 10, which ran through 13 in the
// one update each time; back out to the village 1.5 and 2.4 s, of it 0.70
// and 0.85 s in state 6's step 4, one update, and 0.45 and 1.17 s in its
// step 6 (18 and 57 updates); village to castle and back by the world map,
// list swaps, 1.8 and 2.1 s, of it 0.32 to 0.46 s in state 4's step 6 and
// 0.25 to 0.62 s in its step 9. The name is ours.
DAO_ADDRESS(kGameModeLoading_Update, 15094)
// The module's area list, read 2026-10-10 for DAOLib #64 (could a second
// list be kept ready). The module owns one, a CSAreaList (0xE0 bytes, over
// CResLST; constructor 0x006FBE50, taking the list's name by value) at its
// +0x2CC. DropAreaList, __thiscall, no arguments, calls the list's deleting
// destructor, its only virtual, and nulls the field; the destructor
// (0x006FBEE0) tears down (0x00684760) and deletes each of its areas, found
// by ID in the application's table. 0x005FE170, __thiscall (the name), makes
// a list only if the field is null. ReadAreas, __thiscall (int, 1 from the
// swap), `ret 4`, empties the list's entries and reads them again, from the
// resource manager's `are` files or its own Area_List of Area_ID and
// Area_Name, answering 1 or 0: 0x48 bytes each, the area's ID first, in a
// vector at +0xB8, with the next to save or build at +0xC8. 0x006FABB0,
// which the engine's own text calls CSAreaList::LoadAreas(), builds one
// CCArea a call from that cursor (0 more to come, 1 all built, 0x12 an area
// the application already has: "Trying to load an already loaded area!").
// FindArea, __thiscall (const std::wstring &tag), `ret 4`, answers the
// list's area of that tag or null. The swap (state 4 above) calls
// DropAreaList at its step 5, makes and reads the list at 6, and builds at
// 9; its step 10 unloads the rooms of the area the party's hero is still in
// (0x0062FD10 on the party). A teardown also unloads a RIM named for the
// area from the resource manager (0x004C8330). The names but LoadAreas are
// ours.
DAO_ADDRESS(kCCModule_DropAreaList, 11006)
DAO_ADDRESS(kCSAreaList_ReadAreas, 15833)
DAO_ADDRESS(kCSAreaList_FindArea, 15814)
// LoadAreas itself (the engine's own name for it, above), __thiscall on the
// list with no arguments, a plain `ret`: for dao/ObjectIds.h (DAOLib #87),
// which follows a build of a list's areas by it, from the first call that
// finds none going to the first that answers other than 0.
DAO_ADDRESS(kCSAreaList_LoadAreas, 15817)
// The list's RIM trees, read 2026-10-10 after the first try at keeping a
// list (DAOLib #64) waited ten seconds at every swap. The application's
// +0xC0 is a ModuleResourceManager (vftable 0x00B00958, over RIMTreeManager,
// constructor 0x005DB6F0), with the tree loaded for the module's list at its
// +0x7C. The swap's step 7 hands it the tag of the area asked for
// (0x005FBAB0): the area's node is found (0x00A58F30), and if it is not
// loaded already (0x00A587E0), the current tree is unloaded when it is not
// the node's own (Unload, __thiscall (tree), `ret 4`), the node's (its
// +0x58) is loaded (0x00A59770) and made the current one. Step 8 calls the
// manager's update each frame (0x00A590C0, __thiscall (float seconds)),
// which drops from its vector at +0x40 each tree that has finished
// unloading (0x00A58B00), and while any is left adds the seconds to a
// count at +0x4C and answers false until that reaches 10.0; then it goes on
// regardless. In play, 2026-10-10, with a list kept and so its areas alive:
// step 8 took 10.0 s at each of three swaps, and the fourth crashed inside
// that update (0x004E6677, reading 0x23, under 0x00A58A29). The name is
// ours.
DAO_ADDRESS(kRIMTreeManager_Unload, 33383)
// FindNode, __thiscall (const std::wstring &area tag), `ret 4`, is the
// lookup 0x005FBAB0 begins with: the area's node, its tree at +0x58, or
// null. The manager's current tree is also what tells a transition which
// list the party is in: 0x005FBBD0 makes the list's name from it, and the
// loading mode's state 7 (0x006C7080) compares that with the list of the
// area asked for, taking the same name for a door within the list. In
// play, 2026-10-10, with the current tree left stale by a list kept: the
// trip back was taken for such a door, the area was not in the module's
// list, and the loading ended at once with the party where it stood. The
// name is ours.
DAO_ADDRESS(kRIMTreeManager_FindNode, 33377)
// UnloadRooms, __thiscall on an area, no arguments: has the area's layout
// (+0x658) unload every room (0x005C81D0: each room's slot 8 with the
// layout and 1). Its one caller is a transition's step 10 (0x006C5C9D in
// 0x006C5C50), for the area the party's hero is in (0x0062FD10 on the
// party), before the layout of the area arrived in loads; it takes nothing
// back from the call. Read 2026-10-10 for DAOLib #64, used for #72. The
// name is ours.
DAO_ADDRESS(kCCArea_UnloadRooms, 13595)
// GetAreaById, __thiscall on the application (area id), `ret 4`: the area,
// or null. A list's entry begins with the id (0x006FA910 walks them so).
// Read 2026-10-11 for DAOLib #72, with what a layout needs before a room of
// it can load: the layout is given its scene and marked ready (its +0x510)
// by 0x005C9690, a virtual of its own (it also makes the layout the current
// one, the global at 0x00C326CC, which AreaLayout::Update sets again at its
// start every frame); 0x005CA590, the arrival's load around a point (from
// the area's 0x0067B210), does nothing for a layout not ready; 0x005C8670
// unloads every room and marks it not ready. A layout's +0x7C is 1 for an
// outdoor area (it gets a SpeedTree library). CRoom::SetState with 4 asks
// for a load in the background where the room's load is neither done nor
// asked for (its slot 6 with 1, the layout and true). A room's load is
// then finished by the room's slot 24 (0x005D2E60, __thiscall (float
// seconds, layout), `ret 8`), which AreaLayout::Update calls for every
// room of its layout each frame: where the room is waiting on its objects
// (its byte at +0x368, set by 0x005D2B80 once it has queued a
// ConstructObjectTask for each) and the last of them has signalled, it
// takes the objects in and the room is loaded. So a room asked for in an
// area the party is not in is built and never finished, since no update
// runs for that area's layout: in play, 2026-10-11, the chantry's one room,
// asked for from the village, was still not loaded a minute later, and
// was two seconds after the party had been in. Indoors an area is one
// room (the chantry and two others of Redcliffe village's list; the
// village itself has 80), and each area of a list has its layout ready
// with a scene of its own before the party has been in it.
DAO_ADDRESS(kCClientExoApp_GetAreaById, 10511)
// A saved file's name, read 2026-10-11 for DAOLib #78, after a crash as a
// screenshot was asked for. Renderer::ThreadedGraphicsDriver (vftable
// 0x00B5834C) records the graphics driver's calls as commands for the
// render thread. Its slots 84 and 85, SaveToFile and SaveToFileOther
// (__thiscall (object, const std::wstring *name, eight more arguments or
// six), `ret 0x28` and `ret 0x20`), record commands 0x27 and 0x28: an
// object to be saved to a file of that name (the real driver's slot 84,
// 0x0053AC70, calls the object's slot 4 with the name's characters and
// the rest). Each copies the name into the command as 0x400 bytes from the
// string's buffer, whatever its length (`push 0x400; push eax; push edx;
// call 0x008E24A0` at 0x00A23D38 and 0x00A23E97, the exe's only two copies
// of that length): up to a kilobyte past the end of a short name. The crash:
// an access violation in the copy at 0x008E24FA, reading 0x4FCA6000 with
// its source at 0x4FCA5D60, from 0x00A23D3F, the string the engine's own
// (Eluvian's screenshot sends TakeScreenshotMessage and no name). The names
// are ours.
DAO_ADDRESS(kThreadedGraphicsDriver_SaveToFile, 32291)
DAO_ADDRESS(kThreadedGraphicsDriver_SaveToFileOther, 32292)
// The mouse's records (fix/MouseRecords.h, DAOLib #105), read 2026-10-12
// after a crash at 0x008803A6. ReadMouse is __thiscall on the
// ECInput::RawInputWin32, called from its slot 1 (0x008804F0) after the
// keyboard's (0x0087FCF0) with the same this. It asks DirectInput how many
// buffered records the mouse has (GetDeviceData, the device at +0x24, 0x14
// bytes a record), allocates that many (0x008800BA, operator new), reads
// them and walks them, the walk's index the local at [esp+0x24]. After the
// walk, behind a byte for each axis that moved ([esp+0x1B], [esp+0x1A]), it
// makes the movement's event from the dwords at +8 and +0xC, the time stamp
// and sequence, of the record at the index, which by then is the count: one
// past the last (+0x460 and +0x4C8 from the function's start, the four
// reads at +0x466, +0x469, +0x4C8 and +0x4CF). The walk is entered only
// with a count above zero, so the record before always exists. The address
// library's session read the function in the three builds: 0x589 bytes in
// each, the same instructions at the same offsets, GOG's and EA's differing
// only in calls' targets and one global's address. The name is ours.
DAO_ADDRESS(kRawInputWin32_ReadMouse, 24180)
// A flickering light (fix/LightFlicker.h, DAOLib #150), read 2026-10-17 by the
// address library's session and checked here. Update is slot 1 of the
// controller, __thiscall (double dt), `ret 8`: it adds dt to the cycle's time
// (+0x28, a double) and, past the cycle's length (+0x20), starts a new cycle
// from the engine's own generator (Animation::g_randomWaveSeed, 0x00C39778);
// within it, sets the light's intensity (the float at +4 points at) between
// +0x18 and +0x1C by a smoothstep of t / cycle whose clamp, 0x0081578A to
// 0x008157BB, always leaves 0. EA's is the same instructions, Steam's is GOG's.
DAO_ADDRESS(kRandomWaveController_Update, 22011)
// The frame's present, read 2026-10-11 for DAOLib #73, after a profile (#68)
// found the arrival's wait for rooms held to the monitor. Present is
// D3DDriver::D3DGraphicsDriver's slot 10 (vftable 0x00B2E174), __thiscall,
// no arguments: the device's Present (the object at the driver's +0x100,
// its slot 17) with four nulls, answering in al whether the result is
// anything but D3DERR_DEVICELOST (0x88760868). The driver calls the device
// itself for resources too, from whatever thread asks: CreateIndexBuffer
// in its slot 22 (0x0086A3F0) and CreateTexture in its slot 43
// (0x00868FB0), with the device at its +0xFC. In play the streaming thread
// spent over half of the wait under those two in ntdll's
// ZwWaitForAlertByThreadId, a lock's wait, and the wait was 337 to 672 ms
// into Redcliffe village with vertical sync on and 190 to 227 with it off.
// The name is ours.
DAO_ADDRESS(kD3DGraphicsDriver_Present, 23543)
// The other present, read 2026-10-11 after the first try at #73 skipped
// nothing: D3DDriver::D3DSwapChain's slot 2 (vftable 0x00B2E0D8), __thiscall,
// no arguments, the same shape: the swap chain's Present (the object at its
// +0x10, its slot 3) with five nulls, answering in al whether the result is
// anything but D3DERR_DEVICELOST. In play, windowed, the frames of the
// arrival's wait did not come through the driver's Present at all, while
// the update thread spent half the wait under d3d9.dll and dwmapi.dll in a
// wait; the loading screen's own task (15 presents in a load from the menu)
// did come through the driver's. The name is ours.
DAO_ADDRESS(kD3DSwapChain_Present, 24028)
// A batch's draw, read 2026-10-23 for DAOLib #247: D3DDriver::D3DBatchIndex's
// slot 15 (vftable 0x00B2E6DC), __thiscall with five words, `ret 0x14`. It sets
// the batch's streams and indices on the driver's context (0x00870B50,
// 0x0086EFA0), then draws them indexed (0x0086F470: the context's state
// flushed, 0x0086F030, and the device's DrawIndexedPrimitive, slot 82). The
// one place a batch's indexed draw reaches the device from. The name is ours.
DAO_ADDRESS(kD3DBatchIndex_Draw, 23770)
// The draw's record, read 2026-10-23 for DAOLib #247: the frame thread does
// not draw. Renderer::ThreadedGraphicsDriver (vftable 0x00B5834C) stands in
// for the driver there; its slot 28, __thiscall with five words like the
// driver's (D3DGraphicsDriver slot 28, 0x008688E0), writes opcode 17 and the
// five words into its command buffer ([this+8]: the write pointer at
// +0x600000, the commands counted at +0x600010 by InterlockedIncrement). The
// render thread plays the buffer back (0x00A1EDA0, a switch of 41 opcodes on
// the read pointer at +0x600004, from 0x00A1F9C0), opcode 17 calling the
// driver's slot 28, which draws the batch (kD3DBatchIndex_Draw). The name is
// ours.
DAO_ADDRESS(kThreadedGraphicsDriver_DrawBatch, 32259)
// What a draw is (dao/Render.h, Draw::node and its names; DAOLib #247, read
// 2026-10-23 in the probe's record and batch watches). A node's render (its
// +0x180 interface's slot 5 or 17) writes the node into the render context's
// +0xE4 (the mesh's at 0x00559D9C and 0x00559DED) and hands the context, as
// its second word, to its material instance's render: slot 2, __thiscall with
// three words, `ret 0xC`, or the one slot 17 calls directly, five words,
// `ret 0x14`. The batches those record are played on the render thread one
// command at a time by 0x00A1EDA0 (__thiscall, the buffer, `ret 4`), whose
// opcodes 17, 18 and 35 call D3DGraphicsDriver's slots 28 (five words,
// `ret 0x14`), 29 (seven, `ret 0x1C`) and 30 (ten, `ret 0x28`). The frame
// thread also calls those slots itself, at once, inside a material render
// (from MaterialPass slot 3, 0x00506672). Seen in Redcliffe castle: 1,141,796
// played commands, every one its record's in order. The names are ours.
// What sets the three apart (read 2026-10-23): slot 28 draws the batch
// through its own Draw, its vftable's slot 15, which for a D3DBatch
// (0x00870D40) is a DrawPrimitive and for a D3DBatchIndex (0x00870DE0) a
// DrawIndexedPrimitive, each over the range the batch keeps (+0x60 on).
// Slot 29 calls D3DBatch::DrawRange (0x00870D90), a DrawPrimitive over the
// vertex start and count its two last words give; slot 30 calls
// D3DBatchIndex::DrawRange (0x00870E70), the index buffer set and a
// DrawIndexedPrimitive over its five last words. The device's two draws are
// 0x0086F440 (its +0x144, DrawPrimitive) and 0x0086F470 (+0x148,
// DrawIndexedPrimitive), on the driver's +0x1AC.
DAO_ADDRESS(kMaterialObjectInstance_Render, 5842)
DAO_ADDRESS(kMaterialObjectInstance_RenderPasses, 5824)
DAO_ADDRESS(kD3DGraphicsDriver_DrawBatch, 23557)
DAO_ADDRESS(kD3DGraphicsDriver_DrawBatchRange, 23558)
DAO_ADDRESS(kD3DGraphicsDriver_DrawBatchIndexedRange, 23559)
DAO_ADDRESS(kRenderer_PlayCommand, 32141)
// The loading screen's task, read 2026-10-11 for DAOLib #71, after a profile
// (#68) found a loading screen's time in waits, a frame at a time. While a
// loading screen is up it is drawn by Eclipse::UpdateAndRenderLoadingGUITask
// (vftable 0x00AF6C80), a task of kind 2: the render thread's. Execute, its
// slot 2 (__thiscall (float seconds), `ret 4`, answering 2 or 3 in eax),
// begins every turn with a sleep
// of 0x42, 66 ms (`push 0x42` at its +1, the call to the sleep's wrapper
// 0x004E6170 at +5), then, while the graphics' slot 61 says the loading
// screen is up, updates the GUI (the application's +0x158, its slot 8) by a
// fixed step, the float 0.066 at Step, which nothing else reads, draws and
// presents, and answers 2 to be run again; when the screen is no longer up
// it sets its byte at +0x18 and answers 3. The graphics' slot 60 starts and
// stops it, and to stop it spins on that byte (0x005272D0: `while (!done)
// SwitchToThread()`), which the loading mode's state 6 calls at the top of
// every update. In the profile the game's own frame, while loading, waited
// on the render thread (WaitForSingleObject, 0x004E6120, under the
// graphics' slot 20) 70 to 80 ms a frame, and the spin took 75 to 90 ms a
// screen: the loading screen's turn. The names are ours.
DAO_ADDRESS(kLoadingGUITask_Execute, 7121)
DAO_DATA(kLoadingGUITask_Step, 63796)
// The engine's sleep, read 2026-10-11 for DAOLib #75: __cdecl (ms), four
// instructions over the system's Sleep (`mov eax, [esp+4]; push eax; call
// [Sleep]; ret`). The loading screen's task calls it with 0x42 at its
// Execute's +5, the sleep each of its turns begins with; fast load answers
// that one call shorter (perf/fastload/LoadingScreen.h). The name is ours.
DAO_ADDRESS(kMultithreading_Sleep, 5425)
// The engine's threads, read 2026-10-11 for DAOLib #68 and #71, to say
// which of them a loading screen waits for. Eclipse::DAThreadManager
// (vftable 0x00ADD8F0, over Multithreading::ThreadManager) makes them in
// its slot 2 (0x004017D0), each by 0x004BE1A0 (__thiscall (name, pooled,
// kind, ?)): "DragonAge::Update" of kind 1, "DragonAge::Render" 2,
// "DragonAge::Streaming" 5, "DragonAge::FileIO" 3, "DragonAge::Network" 4,
// "DragonAge::StackDump" 6, one of each, and then
// "DragonAge::WorkerThread"s of no kind, by its loop one for every
// processor but one; in play, 2026-10-11, on twenty cores, one thread gave
// itself that name. A
// thread's run (Multithreading::Thread's slot 1, 0x004E63A0) first gives
// itself its name, the manager's SetThreadName (__thiscall (thread id or -1
// for this one, const char *name), `ret 8` at 0x004BBDFA: the plain `ret`
// before it, at 0x004BBDDE, is its exception filter's, and no caller cleans
// the stack; the manager is the global at 0x00C2B588, which 0x004BBE00
// returns), which raises the debugger's exception 0x406D1388 and nothing
// else; the name is the caller's temporary, freed after the call. Then it
// takes tasks for ever: the manager's slot 1
// (0x004BC450) waits for the next of its queue, the task's slot 2 runs it,
// and an answer of 2 puts it back. A task (Multithreading::Task,
// constructor 0x004C4DA0) keeps its kind at +0xC, and 0x004BE610 queues it
// for that kind's thread, or runs it where it stands when the kind has
// none (the task's slot 1, which calls its slot 2 with a sixtieth of a
// second). The kinds, from the push before each class's call to the
// constructor: 5, the streaming thread, for ConstructObjectTask,
// ConstructMergeObjectTask, ConstructCrustObjectTask, TextureLoadTask,
// RIMLoadTask, LoadVFXTask and LoadModuleResourcesTask; 3, the file thread,
// for ECResMan::ReadRequestTask; 2, the render thread, for
// RenderCommandProcessorTask and for the loading screen's own
// UpdateAndRenderLoadingGUITask, whose 66 ms of sleep a turn is therefore
// the render thread's; 1, the update thread, for RoomLoadingTask,
// ModelDeleteTask, EmitterUpdateTask and ComposeMorphTask. A
// ConstructObjectTask's run (0x00572C40) builds its object under one lock
// (0x004CB2B0 to 0x004CB300 around the factory's slot 4), so two of them
// would not run side by side on two threads. In play, 2026-10-11, through
// the arrival's wait for rooms: the file thread waiting 99% of the time,
// the streaming thread 42%, the rest of it under Direct3D's CreateIndexBuffer
// and CreateTexture (the driver's slots 22 and 43, 0x0086A3F0 and
// 0x00868FB0, which call the device themselves). A room's load
// (RoomLoadingTask, vftable 0x00AFFFC0, kind 1) is
// what the resources' read of the room's file is told to run when it is
// done (0x004C8550, from 0x005D2FF2): on the update thread, where it makes a
// GraphicsStructures::ConstructObjectTask for each object of the room
// (0x005D2B80). The names of the threads are the game's; the function's is
// ours.
DAO_ADDRESS(kThreadManager_SetThreadName, 4373)
// The engine's pools, read 2026-10-11 for DAOLib #68 and #69, after a
// profile of loading found its thread in FindFit for over a third of the
// steps that build an area list and swap a layout. The engine's memory is
// its own: operator new (0x00413660) calls 0x004B92C0 (size, alignment's
// shift or -1 for the pool's own), which picks a pool, and operator delete
// (0x004136C0) asks 0x004B8B90 which pool owns the pointer and calls that
// pool's slot 6. The pools are ECPrivate::Pool (vftable 0x00AEF424, 21
// slots) and two classes over it, ECPrivate::BlockPool (0x00AEFAC4) and
// ECPrivate::StringPool (0x00AEF7F4).
//
// Pool's slot 4 (0x004BA880) allocates: under the pool's lock (the critical
// section at its +0x40: 0x004C4D00 and 0x004C4D50 take it, 0x004C4D30 lets
// it go) it rounds the size up to 4, finds a free block (0x004B9940),
// unlinks it (slot 11), splits off what is left over when that is at least
// the pool's smallest block (+0xEC) and links the remainder (slot 10), and
// writes the block's header. Slot 5 (0x004BA9D0) reallocates, growing in
// place over the free blocks that follow. Both read the header straight
// from the pointer: the block's size at -0x10, its alignment's shift at
// -0xA, a used flag at -9, and the size asked for just before the data.
//
// The free lists are twenty, at the pool's +4 on, by the size's bit length
// (everything of 256 KiB and over in the last). 0x004B9940 starts at the
// size's own list and goes up one at a time until one answers. FindFit,
// __thiscall (first block of a list, size, alignment's shift), `ret 0xC`,
// not reading `this`, searches one list: it walks all of it by the block's
// +8, the room a block gives being its size (+0) less 0x18 and what the
// alignment takes at that address; a block whose room is the size is the
// answer at once, and otherwise the smallest block that fits, the first of
// equals, at the list's end. So an allocation costs the length of its size
// class's list. The name is ours.
DAO_ADDRESS(kPool_FindFit, 4322)
// The pools from outside, read 2026-10-11 (an audit for DAOLib #69: could
// the pools be replaced whole). Everything outside the pool classes reaches
// them through three functions, all __cdecl: Allocate (size, alignment's
// shift or -1 for the pool's own; null for a size of 0), called by operator
// new and three thunks (0x004136DA, 0x0041373F, 0x004694C7); Free
// (pointer), operator delete's jump and 0x004694E5; Reallocate (pointer,
// size), from 0x004136BA. Each builds the one manager on its first call
// (0x004B8DA0): 0x4D4 bytes at g_memoryManager holding four pools, of which
// two are set up, "Main Pool" and "Strings", each one HeapAlloc from the
// process heap, never grown and never returned: 0x3700000 for Strings and
// 0x31B00000 for Main, a MiB less at each try that fails. So the engine's
// heap is 850 MiB, committed from the start, and no more. The pool is
// chosen by a code on a per-thread stack (static TLS; 0x004B8B20 pushes,
// 0x004B8B60 pops, an empty stack is the Main pool), and a pointer's owner
// is found by address range (each pool's slot 15, by 0x004B8B90). A free of
// a pointer no pool owns, or of a block already free, returns without a
// word; a reallocation of a pointer no pool owns returns null, one to the
// size already asked for returns the pointer. No code outside the classes
// calls a pool's slot, the header's helpers have no caller outside them, and
// the allocators above (Scaleform's, PhysX's, SpeedTree's, Umbra's) go
// through operator new and its kin. FreeSubPool, __cdecl (code), frees a
// BlockPool's sub-pool whole (its slot 20), from FreeLODMemoryTask and an
// area layout's destructor; nothing calls the slot that makes one, and the
// codes written are -1. Statistics, __cdecl (out, out, pool or -1), has one
// caller, PhysX's manager, for a pool that is not there. The names are ours.
// The loader replaces the three, and answers the two, before the first
// allocation (src/perf/EngineHeap.h), so in a game with the loader none of
// the pools' own code above runs and the manager's pointer stays null.
DAO_ADDRESS(kEngineMemory_Allocate, 4313)
DAO_ADDRESS(kEngineMemory_Free, 4308)
DAO_ADDRESS(kEngineMemory_Reallocate, 4314)
DAO_ADDRESS(kEngineMemory_FreeSubPool, 4312)
DAO_ADDRESS(kEngineMemory_Statistics, 4311)
DAO_DATA(kEngineMemoryManager, 100163)

// The camera and the party's selection, read 2026-09-30 from
// CenterOnObjectMessage's handler (dao/Messages.h). Party::Select,
// __thiscall (id, int), on the application's +0xD0's +0x58 (0x005D57C0
// returns it; its class is not read, and Party is our guess from the
// selection it keeps). With 0 for the int, as the handler passes it: for a
// creature whose +0x7F9 (controllable) is
// set and that passes its vtable's check at +0x1D0, it puts it in the
// selection (a vector of ids at +0x184; 0x006368F0) and signals it
// EVENT_TYPE_ON_SELECT (89); for anything else, nothing. The game mode
// controller's +0x60 is the explore camera's controller
// (CameraControllerExplore, vftable 0x00B1D954, 25 slots, built by
// 0x0076F890): the id of what it follows at +0x08 and its focus point at
// +0x70. Its slot 8, SnapToTarget, copies the target's position (the
// graphics object's +0x120) into the focus and hands the camera the new
// state at once: no easing. Between snaps the controller follows its
// target rigidly, and eases only when the target changes (dao/Camera.h
// has the whole reading): SetTarget, Zoom and Update below. The names are
// ours.
DAO_ADDRESS(kParty_Select, 12478)
DAO_ADDRESS(kCameraControllerExplore_SnapToTarget, 18510)
DAO_ADDRESS(kCameraControllerExplore_Update, 18526)
DAO_ADDRESS(kCameraControllerExplore_OnTargetChanged, 18518)
// The tactical view's frame, read 2026-09-30 for a pan that stopped short
// (dao/Camera.h). The explore controller's tactical step
// (0x00771060, __thiscall (seconds, target, party list), `ret 0xC`, from
// its Update) first pulls the camera back toward the party (PullToParty,
// __thiscall (float seconds, bool inView, party list), `ret 0xC`): while
// the offset is not nothing and neither the target nor any party member is
// in view, a speed at +0xEC grows by 30 m/s each second (0x00C0E038) and
// moves the offset by it toward the nearest party member; once one is in
// view it dies away within a frame or two. Then, if one is in view, it
// reads the player's input (ReadInput, 0x007EDC60: the pan keys, a held
// mouse button's drag, the cursor at the screen's edge) and either pans
// by it (PlayerPan, __thiscall (float seconds, const PanInput *, target
// object, party list), `ret 0x10`, the input {int x, int y, bool keys or
// edge}, which moves the offsets at +0xF0 and +0x100) or, with none, keeps
// the party in view (KeepInView, 0x00770BB0, `ret 8`). Each has the one
// caller. The names are ours.
DAO_ADDRESS(kCameraControllerExplore_TacticalStep, 18524)
DAO_ADDRESS(kCameraControllerExplore_PullToParty, 18513)
DAO_ADDRESS(kCameraControllerExplore_PlayerPan, 18516)
DAO_ADDRESS(kCameraControllerExplore_KeepInView, 18520)
DAO_ADDRESS(kCameraControllerBase_ReadInput, 21234)
DAO_ADDRESS(kCameraControllerBase_SetTarget, 21241)
DAO_ADDRESS(kCameraControllerBase_Zoom, 21239)
DAO_ADDRESS(kCameraControllerBase_TurnBehind, 21242)
DAO_ADDRESS(kSceneCameraManager_Update, 15038)
// The game's fly camera, read 2026-10-15 for DAOLib #125 (dao/Camera.h,
// SetFreeView). GameModeFlyCam (GM_FLYCAM, 6) is a mode the scripts'
// SetGameMode enters. Its entry (slot 6, 0x0076D4A0) turns on the input
// group "GameModeFlyCam" and sets the renderer's near and far to 0.01 and
// 4000, and its exit (slot 7, 0x0076D560) turns it off and sets 0.25 and
// 4000 (corrected 2026-10-19, DAOLib #205: entry and exit were swapped
// here); its update (slot 8, 0x0076DDF0) reads inputs 30 to 35
// (FlyCam_MoveLeft ... FlyCam_MoveSlow, dao/Input.cpp's list) and moves the
// camera at the mode's +0x58, set by the base GameMode's constructor, by
// 15 m/s through its orientation: SetPosition, __thiscall (const Vector4
// *where, float seconds, ?), `ret 0xC`, the camera's place at its +0x70.
// The look (0x0076D620) adds the mouse to the orientation's yaw and pitch in
// degrees (QuaternionYaw and QuaternionPitch, __cdecl (const quaternion
// *), a float; the quaternion at the camera's +0x90), clamps the pitch to
// +-90, makes the quaternion again (QuaternionFromYawPitchRoll, __cdecl (out
// quaternion *, yaw, pitch, roll), degrees) and gives it to SetOrientation,
// __thiscall (const quaternion *, float seconds, ?), `ret 0xC`. The names
// are ours.
DAO_ADDRESS(kSceneCamera_SetPosition, 15033)
DAO_ADDRESS(kSceneCamera_SetOrientation, 15039)
DAO_ADDRESS(kQuaternion_FromYawPitchRoll, 5699)
DAO_ADDRESS(kQuaternion_Yaw, 5700)
DAO_ADDRESS(kQuaternion_Pitch, 5701)
DAO_ADDRESS(kSceneCamera_SetPitch, 15001)
DAO_ADDRESS(kSceneCamera_Update, 15003)
DAO_ADDRESS(kSceneCamera_SetYaw, 15000)
// The camera's distances, read 2026-10-04 (dao/Camera.h,
// SetCloseDistance; DAOLib #17). The orbit (SceneCamera's update) takes its
// radius each frame as zoom x close + (1 - zoom) x far (0x006C2360; mode
// 1 blends the pitch the same way, the behaviour's +0x48 with the camera's
// +0x50). Close and far are the orbit behaviour's +0x40 and +0x30, set
// every frame by SetOrbit (__thiscall (const Vector4 *focus, float close,
// float far, float pitch, const Vector4 *), `ret 0x14`) from the explore
// controller's +0x1C, +0x0C and +0x20 (ApplyOrbit, ID 18490), which its
// Update loads first from two globals: g_closeDistance, 3.0, and
// g_farDistance, 20.0. Nothing else writes the globals. The orbit
// behaviour alone tests no collision; in the close view the scene camera's
// manager (0x006C4C60) runs a pull-in after it (mode 1 -> 0x006C3710 at
// 0x006C4E1A: a 0.25 m sphere cast from the pivot, mask 0x1024), a release
// (0x006C35A0) and a near-camera fade (0x006C4130) (corrected 2026-10-18,
// lib-agent-2; dev/CAMERA.md). The names are ours.
DAO_DATA(kCameraControllerExplore_CloseDistance, 98108)
DAO_DATA(kCameraControllerExplore_FarDistance, 98107)

// Portraits, read 2026-09-30. A creature's portrait is rendered live into
// a texture of its own, as the party's are. UpdatePortrait, a CCCreature
// method with no arguments (the creature in ecx), makes the creature's
// portrait renderer if it has none (0xD70 bytes, its constructor
// PortraitRenderer, kept at the creature's +0x78C), then renders it
// (Render, __thiscall (bool, bool, bool), called with 1, 1, 1) and returns
// 1. It is what the party, the paperdoll and character creation call.
// The renderer holds a CCCreature of its own at +0x20 (built with id -1,
// the model the portrait shows), the source creature at +0xD40, a set-up
// flag at +0xD44, and the texture's name at +0xD4C, an engine
// std::wstring: "portrait_", the creature's tag and a counter (in play,
// 2026-09-30: portrait_player0, portrait_bhn200cr_howe_landra33), made on
// its first render
// (Setup: a 146x146 render target, a SceneCamera, the lights
// "portrait_lightrig" and "el_lr_new_day2"; the camera from table 7, by
// the creature's appearance). A movie shows a texture as "img://<name>"
// (EclipseGFxImageLoader, whose slot 1 ends in LoadImage: "img://" or
// "imgps://", then the engine's texture of that name, else a file; the
// URL may add ?Width=, &Height= and ?Async=). The names are ours.
DAO_ADDRESS(kCCCreature_UpdatePortrait, 14737)
// Its pair, read 2026-09-30: ReleasePortrait, a CCCreature method with no
// arguments that returns 1, destroys the renderer (0x007F3D00, which
// destroys the clone at +0x20 with ~CCCreature), frees it and clears the
// creature's +0x78C. The engine calls it in four places, each beside one
// of UpdatePortrait's callers: a screen that shows a portrait renders it
// and releases it again. Otherwise ~CCCreature destroys a renderer still
// held, when the creature goes with its area.
DAO_ADDRESS(kCCCreature_ReleasePortrait, 14365)
DAO_ADDRESS(kPortraitRenderer_Constructor, 21346)
DAO_ADDRESS(kPortraitRenderer_Setup, 21347)
DAO_ADDRESS(kPortraitRenderer_Render, 21350)
// Where the portrait's camera goes, read 2026-10-02 (dao/Portraits.h):
// PlaceCamera, __thiscall on the renderer (pitch, rotation, distance, Z
// adjust, X adjust, light X, light Y, light Z), `ret 0x20`, Render's one
// call. An argument of 65505.0, the "unset" Render passes for an ordinary
// creature, is read from the portraits table (table 7, by the columns'
// hashes) at the renderer's row (+0xD48, by the creature's tag, else the
// Default row 1); any other value is used as given, unless PlaceCamera
// forces the table: for a creature on the Default row whose appearance is
// not 15 and whose tag is not the second of two strings it tests (and for
// one whose appearance has changed), it reads every value from the table,
// at the row whose Appearance column is the creature's if there is one.
// So an ogre on the Default row is framed from the table whatever it is
// given (seen in play 2026-10-02: a distance of 1.6 given, the portrait
// unchanged; 0x004C0E50 is the strings' inequality, not their equality).
// Render passes the source creature's (+0xD40) own settings (+0x598)
// instead of the "unset" for a creature whose tag is either string, the
// one character creation frames. The camera is set
// about the model's head: the clone's (+0x20, a CCCreature) node "head",
// found by CCCreature::HeadTransform (0x006A1EF0, ID 14447, named by the
// address library), which every creature's skeleton read has as "Head" (people,
// the ogre, genlocks, shrieks, hurlocks, skeletons, werewolves), turned by
// the rotation and pitch and backed off by the distance. The name is ours.
DAO_ADDRESS(kPortraitRenderer_PlaceCamera, 21341)
DAO_ADDRESS(kCCCreature_HeadTransform, 14447)
// A creature's equipment changing (dao/Equipment.h), read 2026-10-04
// (DAOLib #22): every path to equip an item meets in the equip,
// __thiscall (CCItem *item, slot, weapon set), `ret 0xC`, success in al,
// which EquipItemMessage's handler (0x006BE330) and the scripts' EquipItem
// (0x0065CDA0) call; and every path to take one off in the unequip,
// __thiscall (Result *out, CCItem *item), `ret 8`, answering out with its
// first byte the success, which the scripts' UnequipItem calls and the
// equip calls for what it displaces. The names are ours.
DAO_ADDRESS(kCCCreature_Equip, 14937)
DAO_ADDRESS(kCCCreature_Unequip, 14877)
DAO_ADDRESS(kScaleformManager_LoadImage, 6704)

// The talk tables (dao/Tlk.h), read 2026-09-30 from GetTlkTableString: the
// engine's CTlkTable in this global; GetString, __thiscall (strref,
// std::wstring &out, flag 0), `ret 0xC`. The +0x1C GetTlkTableString sets
// is read by no lookup (2026-10-24, #106): the gender is the tables' order.
DAO_DATA(kTlkTable, 99738)
DAO_ADDRESS(kCTlkTable_GetString, 5384)
// Talk-table text as the screens show it (dao/Tlk.h), read 2026-10-03:
// ReplaceDesignerTags, __cdecl (std::wstring *text), rewriting the string in
// place. The engine registers it under that name as an ExternalInterface
// call and an FSCommand (0x0051F2F0, 0x0051F410), which every movie's
// ExternalCommands.ReplaceDesignerTags reaches: the conversation screen,
// the journal, tooltips and popups ask it for bound text that holds a "<".
// First the markup of the dialog_tags table (143) becomes Flash HTML
// (<emp> to <b>, <desc> to <i>(, ...); then each <...> token is looked up
// by its CRC-32 (0x0051D620): <FirstName/>, <LastName/>, <FullName/> and
// <DogName/> from the party's object ([[app+0xD0]+0x58], its +0x454,
// +0x470, +0x48C), the keys bound to actions (<InventoryKey/>,
// <PauseKey/>, ...), and the mouse's wording; any other comes back as it
// was. No creature and no gender: the gender's form is chosen when the line
// is read (CTlkTable_GetString).
DAO_ADDRESS(kReplaceDesignerTags, 6861)

// A creature's properties, read 2026-09-30 from GetCreatureProperty and
// SetCreatureProperty: the creature's +0x7C4 is a SmartData attribute
// whose value (+0xC, so the creature's +0x7D0) is its CCCreatureStats.
// The stats' +0x190 is the properties by id (a std::vector at its +4, ids
// below 1000 there). The getters are on that list, not on the stats:
// GetCreatureProperty passes the stats plus 0x190 (0x0066D002), and a
// getter called on the stats themselves reads the wrong vector and
// crashed the game in play, 2026-09-30. They are __thiscall (id), `ret 4`,
// a float, one per value: total, base, current, modifier
// (PROPERTY_VALUE_* 1-4). The setters are on the stats, and add 0x190
// themselves (0x0071B239): __thiscall (id, float value, 1), `ret 0xC`,
// base (for PROPERTY_VALUE_* 1 and 2), current, modifier. The ids are the
// scripts' PROPERTY_*. The names are ours.
DAO_ADDRESS(kCreatureProperties_GetTotal, 12658)
DAO_ADDRESS(kCreatureProperties_GetBase, 12659)
DAO_ADDRESS(kCreatureProperties_GetCurrent, 12660)
DAO_ADDRESS(kCreatureProperties_GetModifier, 12661)
DAO_ADDRESS(kCreatureStats_SetBase, 16510)
DAO_ADDRESS(kCreatureStats_SetCurrent, 16513)
DAO_ADDRESS(kCreatureStats_SetModifier, 16511)

// The engine's data tree (dao/Data.h), read 2026-09-30 from the UI's
// ExternalInterface handlers GetValue and GetNumber. Get, __cdecl, no
// arguments, returns the root, SmartData::TokenManager, made on first use
// (0xA8 bytes, its constructor 0x005C7470) and kept in the global. Find,
// __thiscall (const std::wstring &path, flag), `ret 8`, splits the path on
// '.' and walks it to an IAttribute; with none there it answers the shared
// null attribute, which GetValue compares with. An attribute's slot 12
// returns its value as a std::wstring by value, its slot 13 as a double;
// GetValue passes flag 0. AddTokenObserver registers a movie's interest in
// a path (three strings, answering an id). The handlers' names are the
// game's registered ones; the rest ours.
DAO_ADDRESS(kTokenManager_Get, 2312)
DAO_DATA(kTokenManager, 100796)
DAO_ADDRESS(kTokenManager_Find, 10161)
DAO_DATA(kNullAttribute, 96499)
DAO_ADDRESS(kGuiExternal_GetValue, 6856)
DAO_ADDRESS(kGuiExternal_GetNumber, 6846)
DAO_ADDRESS(kGuiExternal_AddTokenObserver, 6844)
DAO_ADDRESS(kGuiExternal_SendMessage, 6830)
// The plain attributes' own vtables (RTTI), by which an attribute found
// by path says its type: SmartData::Attribute<T, NoMemberLookup,
// InternalStorage, ObserverNotification> for each T, and float's custom
// storage beside. Each one's slot 21 is Set(T), __thiscall, `ret 4`: it
// stores the value and, if it changed, notifies the attribute's observers,
// which is how a bound movie learns of it (read 2026-09-30 from
// Attribute<int>'s: compare, store, notify). Slot 14 does not tell types
// apart: the linker folded it to two shared bodies.
DAO_DATA(kAttributeBool_Vftable, 62231)
DAO_DATA(kAttributeInt_Vftable, 60440)
DAO_DATA(kAttributeUnsignedInt_Vftable, 62558)
DAO_DATA(kAttributeShort_Vftable, 62416)
DAO_DATA(kAttributeByte_Vftable, 68350)
DAO_DATA(kAttributeFloat_Vftable, 64977)
DAO_DATA(kAttributeFloatCustom_Vftable, 68352)
// The engine's way to make and queue a command: __thiscall(type, parameters
// (a CDataArrays), addToFront, static, playerIssued, override, the +8
// word). Called from 13 places: effects (ExecuteEffect, from CCObject and
// CCCreature), drawing and sheathing weapons with their animation (from
// SetCombatState among others), modal abilities' animations, jumps, moves
// and conversations, and QueuePlayerCommand. The names are ours.
DAO_ADDRESS(kCCObject_QueueNewCommand, 17553)
// QueueNewCommand with playerIssued 1, not static: the player's orders, as
// explore mode's PerformUseObjectMessage handler and the functions beside
// it (0x007F6410, 0x007F64C0, 0x007F6690) queue them.
DAO_ADDRESS(kCCObject_QueuePlayerCommand, 17554)
// The player's orders (dao/Orders.h), read 2026-09-30: QueuePlayerCommand's
// three callers, each __stdcall, each signalling OrderReceived (__cdecl
// (CCCreature *, target), EVENT_TYPE_ON_ORDER_RECEIVED to the creature)
// after. Attack (creature, target) `ret 8`; UseObject (creature, object,
// action) `ret 0xC`; UseAbility (ability, creature, item, target, const
// Vector4 *point) `ret 0x14`. QueuePlayerCommand itself is __thiscall
// (type, CDataArrays *parameters from operator new, addToFront), `ret 0xC`.
// The names are ours.
DAO_ADDRESS(kOrders_Attack, 21386)
DAO_ADDRESS(kOrders_UseObject, 21388)
DAO_ADDRESS(kOrders_UseAbility, 21389)
DAO_ADDRESS(kOrders_OrderReceived, 21385)
// The object's queue, read 2026-09-30 from GetCurrentCommand,
// GetCommandByIndex and GetCommandQueueSize: a list at +0x2CC, its count
// at +0x2D4 (the list's +8), and at +0x2DC a pointer to the slot holding
// the current command, the queue's head. GetCurrentCommand is that read,
// __thiscall with no arguments; GetCommandByIndex walks the list,
// __thiscall(index), `ret 4`, null past its end.
DAO_ADDRESS(kCCObject_GetCurrentCommand, 17535)
DAO_ADDRESS(kCCObject_GetCommandByIndex, 17539)
// A command's start and end (dao/CommandEvents.h), read 2026-09-30. The
// object's command step, each frame (__thiscall, `ret 8`), ends the head
// command when done or failed and starts the next (StartNext, __thiscall,
// no arguments). ClearCurrentCommand (__thiscall (result, ?, flag, flag),
// `ret 0x10`; 1 success, -2 cleared, else COMMAND_FAILED_*) is the one
// way a command ends: the step, ClearAllCommands and the script command
// all call it. Each tells the commands system (the application's +0x1AC)
// through a thunk to its +0x6C's vtable: Start, slot 2 (object, command),
// and End, slot 3 (object, command, result, flag); each thunk has one
// caller. The names are ours.
DAO_ADDRESS(kCCObject_StepCommands, 17558)
// A command's run each update, read 2026-10-02 (dev/TURN_BASED.md, "A
// command's timed phases"): RunCurrent, __thiscall (?, ?, ms), `ret 0xC`,
// StepCommands' one call, given the object's own milliseconds since its
// last update, walks the current command's sub-actions (a list reached
// through the object's +0x2DC record) and spends the milliseconds on each
// one's delay (+0x18) and then, for a timed one (a duration at +0x14), on
// its elapsed time (+0xC), never past the duration; then it has the
// sub-action handler (the event queue's +0x70, a CSSubActionHandler) run
// it through its slot 2, Execute, __thiscall (object, sub-action), `ret
// 8`, the sub-action's first word its type, the index of its handler in
// the handler's table at +0x48. Once the milliseconds are spent no
// sub-action runs that update. Seen in play the same day, the probe's
// conjure watch: a UseAbility's sub-actions are types 15, 39, 50, 7 (the
// conjure, timed: 3,866 ms for Chain Lightning, its elapsed time counting
// the caster's milliseconds), 8 (the cost taken) and 37 (the cast, timed);
// IsConjuring answers the running sub-action's type (dev/TURN_BASED.md).
// The names are ours.
DAO_ADDRESS(kCCObject_RunCurrentCommand, 17548)
DAO_ADDRESS(kCSSubActionHandler_Execute, 19864)
DAO_ADDRESS(kCCCreature_IsConjuring, 17541)
// An attack's reach (dao/Attack.h), read 2026-10-04 for the turn-based mod
// (DAOLib #16). The attack command's start (AttackStart) asks whether the
// weapon is ranged (HasRangedWeapon, __thiscall, no arguments: a creature passing
// 0x006A25A0, not read, whose main-hand base item's Type, hash
// 0x12A02374, is 5, a staff, or 6, a bow or crossbow; corrected, first
// read as the wield type) and queues an approach and then
// the attack, sub-action 50; it tests no reach itself. Ranged: sub-action
// 15 with the weapon's Range (bitm_base's, hash 0x81F2A384, from the main
// hand: the creature's +0xBE0 is a SmartData::Attribute<Equipment *>, its
// value (Attribute::Get, `mov eax, [ecx+0xC]`) the Equipment, whose +0x24
// is a SlotItemLocation; ItemIn (0) on that, the item's base row its word at +0x464; 65505 without
// one) capped at 60 (0x0069F330), and both personal spaces (PersonalSpaces,
// the sum of each one's slot 39, PersonalSpace). Its handler
// (ApproachRanged) ends at once when WithinBand (the distance between min
// - 0.05 and max + 0.05) holds for the range and the spaces and
// LineOfSight from the attacker to the target holds; else it walks toward
// the target, its stop distance Range less the spaces. Melee: sub-action
// 4 with the attacker's space; its handler (ApproachMelee) asks the target
// for the attacker's place on its melee ring (MeleeRingSpot, slot 38,
// __thiscall (attacker id, out, unread, claim, unread), `ret 0x14`, the
// ring given the attacker's own PersonalSpace, its obstacle lifted for it; claim set,
// as the handler sets it, lets go of the attacker's places (0x007CC060)
// and claims the one found (0x007CC2B0 writes the attacker's id into the
// ring's entries); 0 only finds it), then the target's InMeleeReach (slot
// 33, __thiscall (attacker, place), `ret 8`: the centres at most both
// spaces and 1.2 apart in x, y and z, and the attacker within 1.25 of the
// place in x and y alone, WithinBand being flat; the band passed is 1.2f at
// 0xAE7540 and WithinBand adds 0.05, so 1.25: checked 2026-10-18 by lib-agent-2), and
// ends at once if so, else walks to the place. Corrected 2026-10-18 by
// lib-agent-2: the walk itself ends the approach, and that is the usual end:
// the move update 0x007A9130 (stop 0.05, flags 7) answers 1 within float 2
// (unset, so 0) + 0.05 m of the place in x and y; InMeleeReach is only an
// early-out, on the first run (0x007AA749) and before the re-path check
// (0x007AA8CF). The place's radius includes the attacker's own ring-0
// MeleeRingOffset (0x006A8EE0 to 0x007CC2B0), so for an attacker with an
// offset InMeleeReach is false at its place by that offset, and
// InStrikingReach's limit (PersonalSpaces + 1.2 + 1.0, 0x00ADE218, checked)
// still allows the blow. -9 from the move update is PATH_ACTION_REQUIRED (a
// door), not arrival: the approach asks the ring again with claim, runs
// 0x007A6CA0 again, sets int 3 = 1500 ms and goes on. The swing, sub-action 1
// (Swing), tests on its first run, in order: the target found and not
// dead; the attacker's InStrikingReach (below); the direction to the
// target within 60 degrees of level (Vector::Elevation); and line of
// sight (Combat::LineOfSight, the target's id the one hit it lets
// through). Then it sends EVENT_TYPE_COMMAND_PENDING (SignalAttackPending:
// integers 1 and 1, objects self and target) unless the sub-action's
// integer 2 is set. It answers -8 at once where the attacker's disabled
// flags (0x00A64740 on its +0x70) have bit 4. There is no facing test in
// the swing: the turn before it is sub-action 39's. InStrikingReach
// (0x006A3DC0), __thiscall (target), `ret 4`: true unless the target is a
// creature (type byte 6) farther in x and y from the attacker's place
// than PersonalSpaces + 1.2 + the constant at 0x00ADE218, to the target's
// slot 29 point.
// CSSubActionHandler keeps its handlers, its own __thiscall (object,
// sub-action) members, `ret 8`, by type at +0x48 (filled by 0x007AB8D0).
// Corrected by the address library's session, 2026-10-04. The names are
// ours.
DAO_ADDRESS(kCommands_AttackStart, 17685)
DAO_ADDRESS(kCCCreature_InMeleeReach, 14499)
DAO_ADDRESS(kCCCreature_MeleeRingSpot, 14620)
DAO_ADDRESS(kCCCreature_PersonalSpace, 14451)
DAO_ADDRESS(kCCCreature_HasRangedWeapon, 14466)
DAO_ADDRESS(kCCObject_PersonalSpaces, 14072)
DAO_ADDRESS(kSubActions_WithinBand, 19820)
DAO_ADDRESS(kCombat_LineOfSight, 13639)
DAO_ADDRESS(kSmartDataAttribute_Get, 7596)
DAO_ADDRESS(kSlotItemLocation_ItemIn, 21362)
DAO_ADDRESS(kCSSubActionHandler_ApproachMelee, 19889)
DAO_ADDRESS(kCSSubActionHandler_ApproachRanged, 19887)
DAO_ADDRESS(kCSSubActionHandler_Swing, 19879)
DAO_ADDRESS(kCCCreature_SignalAttackPending, 14503)
DAO_ADDRESS(kCCCreature_InStrikingReach, 14500)
// A creature's melee rings kept up to date, read 2026-10-04 (DAOLib #19;
// corrected by the address library's session): CCCreature::Update calls
// RefreshMeleeRing (__thiscall (ring index), `ret 4`) for each of its
// rings every frame. It places every ring's centre about the creature
// (0x006A3060, for each ring on each call); then, unless the ring has a
// claimant (the count at +0x38 of its claimants' list at +0x30: a claim
// adds the attacker, 0x00698C30, a release takes it away, 0x00745230) or
// its byte at +0x28 is set, it empties the ring (MeleeRing::Clear: flags
// 0, holders none, +0x10 65505.0) and stops. Else it marks the places a
// wall (0x00A471E0) or a slope over 60 degrees cuts off as near-only
// (+9), and records the creatures this creature perceives (its perception
// list, the std::set of ids at its +0xC18, which IsPerceiving and
// GetPerceivedCreatureList read) as standing on the places their bodies
// cover (0x007CC870: a place keeps the nearer creature, its id at +0xC
// and distance at +0x10; +0x14, +0x18 and +0x1C set to -1, so any claim
// there is gone; a run each side through 0x007CB920). So a ring nobody
// has claimed is empty, and once claimed it fills with who stands round it
// and is perceived. The rings are made from the appearance row (0x006AC2A0,
// ID 14700): ring 0 from apr_base, rings 1 to 9 from meleering1 to 9 where
// they have the row; each ring's radius at +0x10 its PersonalSpace (ring 0)
// or Radius, plus 1.2, and 0x007CB400 adds 0.1 more (the double at 0xAE7D70), so a
// ring's places stand at that plus 1.3 (corrected 2026-10-18 by lib-agent-2); its +0x28 the
// row's MeleeRingUpdateAlways, a
// ring always filled. Their places by 0x007CB400; the ring at an index,
// made if absent, by MeleeRings::At (0x007CCC30, __thiscall (index) on
// the creature's +0xB44, the vector of rings, `ret 4`). Corrected by the address
// library's session, 2026-10-04. The names are ours.
DAO_ADDRESS(kCCCreature_RefreshMeleeRing, 14701)
DAO_ADDRESS(kMeleeRing_Clear, 20604)
DAO_ADDRESS(kMeleeRings_At, 20622)
DAO_ADDRESS(kScriptBackend_ForScript_VectorToAngle, 5703)
// The angles an attack's turn and blow test, read 2026-10-04 (DAOLib #19):
// the turn (TurnToward, which sub-actions 39, 40 and 42 call with a
// direction of their own; 39's, 0x007A69E0, to the target's slot 29
// point for melee) is done at once when the
// difference of two VectorToAngle angles, wrapped to -180..180 by Wrap
// (__cdecl (angle, min, max), adding or taking 360), is at most 1 degree;
// the blow tests the attacker-to-target direction's Elevation (__cdecl
// (const Vector4 *), atan2 of z against the length in x and y, in degrees)
// against 60. The names are ours.
DAO_ADDRESS(kAngle_Wrap, 5693)
DAO_ADDRESS(kVector_Elevation, 5704)
// The two facings an attack's turn reads, read 2026-10-05 (DAOLib #25, #32).
// The creature's own (+0x130) is TurnToward's; ForScript_SetOrientation
// (__thiscall (const Vector4 *) on the creature, `ret 4`) writes it and
// tells the model (the creature's vtable slot 32). The movement controller
// (creature+0xA30, MovementController, vtable 0x00B596C0) keeps a facing
// of its own at +0xA0, written only by its update (0x00A644E0) while its
// byte +0x75 is set, and a wanted facing at +0x90 that SetDesiredFacing
// (__thiscall (const Vector4 *), `ret 4`) writes. That starts a turn
// (state 4, +0xF0) only when the wanted facing is more than about 1
// degree from the controller's own (0.99999899 degrees, as AngleBetween
// measures it: __cdecl (const Vector4 *, const Vector4 *), the unsigned
// angle on the ground between two directions in degrees, x and y only),
// the state is 0, and the motion record is not busy (the byte at +0x74 set
// and +0xC0 of [[+0x70]+0x10] nonzero); read in full by the address
// library's session, which counts nine callers. The scripts' SetFacing
// commands go through SetDesiredFacing for a live creature, and so turn
// nothing when the controller's facing is already on the aim. The names
// SetDesiredFacing and AngleBetween are ours.
DAO_ADDRESS(kScriptBackend_ForScript_SetOrientation, 14042)
DAO_ADDRESS(kMovementController_SetDesiredFacing, 33602)
DAO_ADDRESS(kVector_AngleBetween, 4976)
DAO_ADDRESS(kCCObject_StartNextCommand, 17542)
DAO_ADDRESS(kCCObject_ClearCurrentCommand, 17540)
DAO_ADDRESS(kCommands_Start, 12731)
DAO_ADDRESS(kCommands_End, 12732)

// A second script command table, beside CVirtualMachineCommands: RTTI's
// CClientVirtualMachineCommands, a CVirtualMachineCmdImplementer, made by
// the same start-up function (0x005DB7F0). Its InitializeCommands
// registers 103 handlers of its own, among them an AddCommand and a
// SetActive that queue commands as the others do. dao::script's table and
// intercepts are the other table's. Whether single-player runs a script on
// this one is not known yet: the probe counts its AddCommand's calls.
// Read 2026-09-30.
DAO_ADDRESS(kCClientVirtualMachineCommands_Constructor, 15293)
DAO_ADDRESS(kCClientVirtualMachineCommands_InitializeCommands, 15424)
DAO_ADDRESS(kCClientVirtualMachineCommands_AddCommand, 15416)
DAO_ADDRESS(kCClientVirtualMachineCommands_SetActive, 15348)

// The game's update, as a task on the engine's worker threads: from the
// deliberate crash of 2026-09-30, whose stack ran CClientExoApp's start-up
// through the message dispatch down to Eclipse::UpdateTask's slot 3, which
// calls the per-frame update at 0x00412EB0, on a Multithreading::Thread.
// The probe counts a stack sample as game logic when this function's frame
// is on it; the range is Ghidra's.
DAO_ADDRESS(kEclipseUpdateTask_Execute, 382)
// What that slot calls each frame (dao/Frame.h), read 2026-09-30:
// __thiscall, no arguments, true to go on running, on the object in the
// global below (0x280 bytes, made by 0x0040DCB0 at start-up; no vtable, so
// no RTTI name: named here for what it runs). It returns false at once if
// its byte at +0xCD is set, a quit flag by how the task uses the answer.
DAO_DATA(kEclipseGame, 99742)
DAO_ADDRESS(kEclipseGame_Update, 378)
// Eclipse::UpdateTask's slot 3 is this long (0x00413310 to 0x00413368 in the
// listing): an address in it is its ID's address and less than this more.
inline constexpr std::uint32_t kEclipseUpdateTask_ExecuteSize = 0x58;

// The script machine's typed stack, named after Neverwinter Nights: NWN:EE's
// headers (C:\Modding\unified, NWNXLib/API/API/CVirtualMachine.hpp) declare
// `BOOL StackPopInteger(int32_t*)` and its kin on CVirtualMachine, and every
// script command here calls the same shapes on the object at kVirtualMachine:
// `mov ecx, [0x00C1D080]; lea eax, [esp]; push eax; call`. Which is which
// was voted on 2026-09-30 across the 665 implemented commands with a
// script.ldf signature: each command's calls into the VM, in order, against
// its argument types in order (the pops), and, for commands with one other
// VM call, against its return type (the pushes). Votes for the winner, of
// all cast, beside each. The addresses run in NWN's declaration order --
// integer, float, vector, string, then DAO's own resource and array types,
// engine structure, object -- pops and pushes each together. Strings and
// engine structures (locations, effects) were round-tripped in play too,
// 2026-09-30, by the probe's value-type checks (dev/LIBRARY.md). The int and
// object pops and pushes were verified in play 2026-09-30: the probe's
// script checks popped and pushed back each getter's argument and answer
// on about 2,200 calls, and the scripts ran on unchanged. The rest are not
// yet exercised.
DAO_DATA(kVirtualMachine, 99736)
DAO_ADDRESS(kCVirtualMachine_StackPopInteger, 32550)          // 453/464
DAO_ADDRESS(kCVirtualMachine_StackPopFloat, 32551)            // 72/74
DAO_ADDRESS(kCVirtualMachine_StackPopVector, 32552)           // 18/19
DAO_ADDRESS(kCVirtualMachine_StackPopString, 32553)           // 136/145
DAO_ADDRESS(kCVirtualMachine_StackPopArray, 32554)            // 4/5, arrays of any type
DAO_ADDRESS(kCVirtualMachine_StackPopResource, 32555)         // 15/15
DAO_ADDRESS(kCVirtualMachine_StackPopEngineStructure, 32556)  // 99/99: effect, location, event, command
DAO_ADDRESS(kCVirtualMachine_StackPopObject, 32557)           // 415/426
DAO_ADDRESS(kCVirtualMachine_StackPushInteger, 32504)         // 77/77
DAO_ADDRESS(kCVirtualMachine_StackPushFloat, 32505)           // 1/1, and the order
DAO_ADDRESS(kCVirtualMachine_StackPushVector, 32506)          // 2/2
DAO_ADDRESS(kCVirtualMachine_StackPushString, 32507)          // 5/5
DAO_ADDRESS(kCVirtualMachine_StackPushEngineStructure, 32510) // 31/31: command, effect, location, event
DAO_ADDRESS(kCVirtualMachine_StackPushObject, 32511)          // 3/3
// The commands object's engine-structure functions (dao/Structures.h), read
// 2026-09-30: each __thiscall on CVirtualMachineCommands with the
// structure's number first (script.ldf's ENGINE_STRUCTURE_n), `ret 8`.
// Create (slot 5) allocates each type: event 0x50 bytes, location 0x30,
// command from a pool the application keeps, effect and item property 0x7C
// each. Destroy (slot 6) deletes one: through its vtable, or for a location
// with operator delete. Copy (slot 8) makes a new one from another; the
// VM's StackPushEngineStructure and StackPopEngineStructure both call it,
// so a pop hands the command a copy of its own to destroy, and a push
// copies. NWN has the same three a slot later, after a GetEqual.
DAO_ADDRESS(kCVirtualMachineCommands_CreateGameDefinedStructure, 12971)
DAO_ADDRESS(kCVirtualMachineCommands_DestroyGameDefinedStructure, 13453)
DAO_ADDRESS(kCVirtualMachineCommands_CopyGameDefinedStructure, 13530)
// The structures' own constructors, from Create and Copy.
DAO_ADDRESS(kCScriptEvent_Constructor, 14241)
// CScriptEvent's vtable, which its constructor stores (RTTI): how
// dao/Events.h tells a script event's data without reading its RTTI.
DAO_DATA(kCScriptEvent_Vftable, 66962)
DAO_ADDRESS(kCScriptEvent_CopyConstructor, 14272)
DAO_ADDRESS(kCScriptLocation_Constructor, 19899)
DAO_ADDRESS(kCScriptLocation_CopyConstructor, 12841)
DAO_ADDRESS(kCGameEffect_Constructor, 33640)
DAO_ADDRESS(kCGameEffect_CopyConstructor, 33636)
// The clocks, read 2026-09-30 (dev/TODO.md, item 4). The engine's
// microsecond clock (the performance counter over its frequency) on a
// global timer, read through a thunk from 31 places, world code among
// them; Scaleform's GTimer keeps its clock function in a global (the
// performance counter's in milliseconds, or GetTickCount's) that the UI
// reads through a jump. Which clock gives the world its frame time is not
// yet pinned. The names are ours.
DAO_ADDRESS(kClock_Microseconds, 4466)
// The world's time (dao/Clock.h, SetSpeed): EC::WorldTimer (vftable
// 0x00B58C44), the application's +0xD4, over EC::ITimer. Its update (slot
// 6) takes the microsecond clock's seconds since the last, times its scale
// at +0x54; its slot 4 gives the seconds since the update, times the same;
// both give 0 while its byte at +0x58 is set (its slot 7 sets it). Its
// constructor sets the scale to 1, and nothing else writes it. An object's
// timer (ObjectTimer::Update, 0x006FA1D0, the object's +0x2B8 a record
// whose +0x08 is the milliseconds since its last update) asks it for the
// world time (0x00A35AF0), and CCCreature's update (slot 87, 0x006BE4B0)
// hands those milliseconds to regeneration and its effects. The names are
// ours.
DAO_ADDRESS(kWorldTimer_Constructor, 32707)
// The game's time scale (dao/Clock.h, SetSpeed), read 2026-09-30. The
// application keeps it at +0x2C8; SetTimeScale, __thiscall (scale, seconds,
// owner id or -1), `ret 0xC`, sets it at once for 0 seconds, else records
// the scale it had (+0x2BC), the one asked (+0x2C0), the time (+0x2C4) and
// how far it is (+0x2D0) for its update to ease (+0x2D8 set). The frame
// update (0x005D7560) advances the world timer (0x00A35A10), then, when the
// scale is 0 or more, writes it to the world timer's +0x54 and gives it to
// the scene (0x008107F0, slot 6, as a double). The names are ours.
DAO_ADDRESS(kCClientExoApp_SetTimeScale, 10573)
DAO_ADDRESS(kCClientExoApp_FrameUpdate, 10566)
// The frame's clock (dao/Clock.h): the Eclipse game object, a global the
// update task reads (0x00401F30 returns it), keeps two EC::ITimer at +0xB4
// and +0xB8 (its constructor, 0x00404260). ITimer::Update, __thiscall
// (float fixed frames per second), `ret 4`: the microsecond clock's
// seconds since +0x08 (or one over the fixed rate) into +0x10, added to
// the double at +0x18. EclipseGame's frame step (0x004025C0) updates the
// +0xB4 timer, keeps its seconds at the game's +0xBC, and gives its total
// to the scene (0x008107F0). The game object is kEclipseGame above.
DAO_ADDRESS(kITimer_Update, 4728)
DAO_ADDRESS(kEclipseGame_FrameStep, 74)
DAO_ADDRESS(kWorldTimer_Update, 32689)
DAO_ADDRESS(kWorldTimer_SinceUpdate, 32690)
DAO_ADDRESS(kWorldTimer_WorldTime, 32695)
DAO_ADDRESS(kObjectTimer_Update, 15799)
DAO_ADDRESS(kCCCreature_Update, 14951)
DAO_DATA(kClock_Timer, 100176)
DAO_DATA(kGTimer_Clock, 99837)

// Input, read 2026-09-30 (dev/TODO.md, item 3). The key bindings' actions
// name a message as text per game mode, registered at start-up by
// RegisterInputMessages (one large function), and a key's message is built
// by name and delivered. RawInputMessage's handler in the input mappings
// runs while the controls screen captures a key. The names are ours.
DAO_ADDRESS(kInput_RegisterInputMessages, 15452)
DAO_ADDRESS(kGroupedInputMappings_HandleRawInput, 24378)

// Using an ability from the quickbar, read 2026-09-30 from
// PerformAbilityMessage's handler (dao/Messages.h says how it chooses a
// target). UseAbility (ability, caster, the message's target, the target chosen,
// a position), `ret 0x14`, queues the ability's command;
// BeginTargeting, __thiscall on the mode (ability, caster, target), `ret
// 0xC`, asks the player for a target: unless an input check fails (then a
// "Target Required" floaty), it keeps the ability, its TargetType, the
// caster and the target in globals from 0x00C35FA0, turns on the targeting
// cursor, and in explore or combat mode enters targeting (the mode's slot
// 28). RequestTargetMessage has no handler: only its factory (0x00776070)
// and clone build one, so sending it does nothing;
// the attack target is what the caster fights (by id); a global holds the
// selected target's id, set only through 0x0070D030, which the messages
// that cycle targets call; ValidTarget (object, TargetType) says whether the
// ability may be used on it. The names are ours.
DAO_ADDRESS(kGameModeExplore_UseAbility, 16297)
DAO_ADDRESS(kGameModeExplore_BeginTargeting, 16186)
DAO_ADDRESS(kGameModeExplore_AttackTarget, 16183)
DAO_ADDRESS(kGameModeExplore_ValidTarget, 16234)
// What BeginTargeting keeps, read 2026-10-02: one object, the "globals from
// 0x00C35FA0" above being its fields. BeginTargeting resets it (0x0070AB60)
// and then sets +0x40 the ability, +0x44 its AbilityType and +0x4C its
// TargetType (abi_base), +0x48 the target passed, +0x50 the caster, and
// through its slot 21 +0x14, 1 while the targeting cursor is up; the
// reset puts 0 in +0x14, +0x40 and +0x44 and -1 in +0x50. Explore mode's
// aiming (0x00711054) tests +0x40 not 0 and +0x14 1. The name is ours.
DAO_DATA(kAbilityTargeting, 101587)
DAO_DATA(kSelectedTarget, 97479)
// The one setter of the selected target: __thiscall on explore mode (id),
// `ret 4`; it stores the id, tells the target's GUI and moves the
// highlight (dao/GameModes.h, SelectTarget). Read 2026-09-30.
DAO_ADDRESS(kGameModeExplore_SelectTarget, 16228)

// How a temporary effect runs out, read 2026-09-30 (dao/Effects.h). Each
// object's update ages its effects through its effects holder (a virtual
// on every object class, 0x007A1800, calling Update, 0x007A1500), which
// walks them (0x0079F5D0) with AgeEffect (0x007A0B10) for each. For a
// temporary effect that has been applied, AgeEffect asks the owner (its
// vtable slot 11) for this update's figures and passes their +0x08, the
// milliseconds since the last, to AddElapsed (__thiscall (ms), `ret 4`),
// which adds them to the effect's +0x30, never past its duration at +0x2C.
// Then, if the duration less the elapsed is nothing, it ends the effect.
// AddElapsed has no other caller. The names are ours.
DAO_ADDRESS(kCGameEffect_AddElapsed, 19723)
DAO_ADDRESS(kEffectsHolder_Update, 19775)
DAO_ADDRESS(kEffectsHolder_AgeEffect, 19766)
DAO_ADDRESS(kCGameEffect_Prepare, 13143)
// Regeneration, read 2026-09-30 (dao/Clock.h). UpdateRegeneration,
// __thiscall on the creature's stats (ms, bool in combat), `ret 8`, adds
// the milliseconds to the stats' +0x58 and, each 3000 of them out of
// combat or 2000 in combat, has every property regenerate (RegenerateAll,
// 0x00795BA0), the living only. A depletable property's regeneration step
// (CreaturePropertyDepletable's slot 13, 0x0079CAD0) skips if its +0x74
// is set, else adds the total of the property its +0x64 (out of combat) or
// +0x6C (in combat) names: properties.gda's NonCombatEffect and
// CombatEffect. The names are ours.
DAO_ADDRESS(kCCCreatureStats_UpdateRegeneration, 16509)
DAO_ADDRESS(kCCCreatureStats_RegenerateAll, 19430)
DAO_ADDRESS(kCreaturePropertyDepletable_Regenerate, 19652)
// The parameters by index of CDataArrays, the base CGameEffect and
// CScriptEvent share at +0 (the RTTI's base class descriptors), from
// GetEffectInteger and its three siblings (one body, 0x00651270) and
// GetEventInteger and its kin, which call the same four: __thiscall, the
// index, `ret 4`. Each indexes a std::vector the structure points to (+0x4
// ints, +0x8 floats, +0x10 objects, +0x14 strings), bounds-checked; out of
// range, 0, 0.0, -1, and a static empty string. GetString returns a
// pointer to the string.
DAO_ADDRESS(kCDataArrays_GetInteger, 33398)
DAO_ADDRESS(kCDataArrays_GetFloat, 33399)
DAO_ADDRESS(kCDataArrays_GetObject, 33401)
DAO_ADDRESS(kCDataArrays_GetString, 33402)
// A fifth list at +0xC, read 2026-09-30 from the script commands that build
// commands (CommandMoveToLocation and its kin), which store their
// booleans in it: the getter has the other four's shape and answers 0 out
// of range. No script command reads it. The name is ours.
DAO_ADDRESS(kCDataArrays_GetFlag, 33400)
// The setters the same constructors call, __thiscall (index, value),
// `ret 8`: each makes its list if there is none, grows it to the index and
// stores. The vector setter takes a pointer to 16 bytes, three floats and
// padding, copied with movaps; its list at +0x18 is not a std::vector but
// {data, size, capacity} of 16-byte elements (its accessor, 0x005728B0,
// is data + index * 16). The string setter takes a pointer to the engine's
// std::wstring.
DAO_ADDRESS(kCDataArrays_SetInteger, 33416)
DAO_ADDRESS(kCDataArrays_SetFloat, 33417)
DAO_ADDRESS(kCDataArrays_SetFlag, 33418)
DAO_ADDRESS(kCDataArrays_SetObject, 33419)
DAO_ADDRESS(kCDataArrays_SetString, 33420)
DAO_ADDRESS(kCDataArrays_SetVector, 33409)

// Commands (dao/Objects.h, Command), read 2026-09-30 from the 17 script
// commands that build one and from AddCommand. Each takes a command from
// a pool the AI master keeps (the object at AIMaster+0x6C) with New, which
// numbers it (the counter at the pool's +0x48), makes a CDataArrays for its
// parameters, fills it and sets the type. The copy the VM makes on a push
// or pop (CopyGameDefinedStructure, structure 2) is a new command from the
// pool with the fields copied by CopyFrom, not a shared reference; destroy
// (structure 2) is Release, which puts it back. The names are ours.
DAO_ADDRESS(kCommandPool_New, 17726)
DAO_ADDRESS(kCommandPool_Release, 13451)
DAO_ADDRESS(kCommand_Constructor, 17647)
DAO_ADDRESS(kCommand_CopyFrom, 17649)
DAO_ADDRESS(kCommand_SetType, 17652)

// Script arrays (dao/core/ScriptArray.h), read 2026-09-30. The value is
// the tag and a shared payload; these make, copy, measure and free one,
// __thiscall each. StackPopArray (above) makes the popper a copy with
// operator new and CopyConstructor; StackPushArray copies the one it is
// given, so the caller keeps and frees its own, as GetTeam (0x0066E570)
// does: Destructor, then operator delete. The engine's name for the class
// is not in the exe.
DAO_ADDRESS(kScriptArray_Constructor, 32538) // (tag): empty
DAO_ADDRESS(kScriptArray_CopyConstructor, 32494)
DAO_ADDRESS(kScriptArray_Destructor, 32537)
DAO_ADDRESS(kScriptArray_Size, 32519)
DAO_ADDRESS(kCVirtualMachine_StackPushArray, 32508)
// A resource on the stack is a std::wstring under tag 0x60 (StackPopResource
// copies it out with the string's operator=): this pushes one, as
// StackPushString does a string under tag 5. Read 2026-09-30.
DAO_ADDRESS(kCVirtualMachine_StackPushResource, 32509)

// The engine's std::wstring, MSVC 2005's (dao/core/Msvc8.h), read
// 2026-09-30 from GetStringLength (0x00657CC0), which makes one on its
// stack, pops into it and destroys it. __thiscall each. The C runtime is
// linked into the exe, so these have no import names, and memory they
// allocate is the engine's heap's: only the engine's destructor frees it.
DAO_ADDRESS(kStdWString_Constructor, 4497)
DAO_ADDRESS(kStdWString_Destructor, 4498)
DAO_ADDRESS(kStdWString_Assign, 4546)      // assign(other, 0, npos)
DAO_ADDRESS(kStdWString_AssignRange, 4517) // (other, position, count)
// The allocator every engine allocation above goes through: operator new
// calls the engine's memory manager (0x004B92C0) with a tag of -1.
DAO_ADDRESS(kOperatorNew, 392)
DAO_ADDRESS(kOperatorDelete, 397)

// The VM's stack as a whole, read 2026-09-30 for calling commands from C++
// (dao/Script.h, Invoke): an object at VM+0x1738 whose first word is the
// number of values on it, types at VM+0x1744 and values at VM+0x1748 (the
// pops index both with it). Truncate, __thiscall on that object with the
// new depth, `ret 4`, destroys every value above the depth and sets it;
// each pop ends with it.
DAO_ADDRESS(kCVirtualMachineStack_Truncate, 32549)

// The engine's event queue, read 2026-09-30 from SignalEvent (0x00657E40).
// The object at CClientExoApp+0x1AC keeps it; it has no vtable and so no
// RTTI name, and plays the part of NWN's CServerAIMaster, whose calls these
// match: SignalEvent hands AddEventDeltaTime (0, 0, caller, target, 10,
// the CScriptEvent), 10 being NWN's EVENT_SIGNAL_EVENT, and it reads the
// world clock (EC::WorldTimer, CClientExoApp+0xD4) and queues a CAIEvent
// (RTTI; NWN's CServerAIEventNode) now or later, in a list at +0x64 of the
// master. ProcessEvent takes one due event, frees the node, and hands the
// event to its target: a CCObject's slot 50 (vtable +0xC8), an area's or
// the module's handler below; with no target it frees the event's data
// through DeleteEventData, which ClearEvents also uses. Not yet seen in
// play: which events pass through here (the probe counts them).
DAO_ADDRESS(kAIMaster_AddEventDeltaTime, 12754)
DAO_ADDRESS(kAIMaster_AddEventNow, 12745)
DAO_ADDRESS(kAIMaster_AddEventLater, 12746)
DAO_ADDRESS(kAIMaster_ProcessEvent, 12749) // (CAIEvent **), __thiscall
DAO_ADDRESS(kAIMaster_ClearEvents, 12753)
DAO_ADDRESS(kAIMaster_DeleteEventData, 12733) // (id, data)
DAO_ADDRESS(kCAIEvent_Constructor, 19600)
// The queue as it runs, read 2026-10-02 (dao/Timers.h). Update, __thiscall
// (bool), `ret 4`, reads the world clock and has 0x0064AE30 deliver every
// event due by then, earliest first, within a time budget; nine places
// call it. Events wait in a linked list at the master's +0x64 (a pointer
// to its head, the earliest, at +0, its tail at +4, the count at +8; a
// node's +0 the one before it, +8 its CAIEvent), kept in order by
// AddEventLater, which walks it from the tail. AddEventDeltaTime is the
// one way in, from 50 places (DelayEvent among them): __thiscall (days,
// ms, caller, target, id, data), `ret 0x18`, 1 when queued, it adds the
// delay to the world time (WorldTimer::WorldTime, __thiscall (day *, ms
// *), `ret 8`) and hands it to AddEventLater, or to AddEventNow for none,
// each called from nowhere else. DelayEvent writes its seconds as whole
// days of the world timer's +0x50 seconds and milliseconds
// (dao/core/HeldEvents.h). DeliverNow, __thiscall (caller, target, id,
// data), `ret 0x10`, builds a node due at 0 and hands it to ProcessEvent
// at once: what SignalEvent does when told to process it immediately, and
// 43 other places. An area of effect's first EVENT_TYPE_AOE_HEARTBEAT is
// not queued: its update (0x007E4B6A) makes it once, flagged at the
// object's +0x4D8, and runs its script with it straight away; every later
// one is the script's own DelayEvent. The names are ours.
DAO_ADDRESS(kAIMaster_Update, 12757)
DAO_ADDRESS(kAIMaster_DeliverNow, 12755)
// Where an object's script meets its event (dao/Events.h): __thiscall
// (CScriptEvent **), `ret 4`. CCObject's handler (slot 50, 0x00698A50,
// shared with the triggers, sounds, waypoints and more; CCCreature's,
// CCPlaceable's, CCItem's and CCAreaOfEffectObject's chain to it) calls it
// for a script event (handler id 10), and 17 other places call it
// straight, delivering at once: a creature's death and update, the command
// queue, effects, Party::Select, the orders. Read 2026-09-30.
DAO_ADDRESS(kCCObject_RunEventScript, 14176)
// A creature's health changed (dao/Creatures.h, OnDeath and OnRevive): __thiscall (float old
// health, byte flag), `ret 8`, void, called through the stats' change notice (0x00719A00, for
// property 7) from every setter of the current health. Old above 0 and new at or below is a
// death; with the flag and a mortal creature it applies the death effect (0x007A1A40) itself,
// as a script's SetCurrentHealth(0) does; a fatal blow's death effect lowers the health on its
// ExecuteEffect's first tick (0x007AB313), the flag 0. Old at or below 0 and new above is a
// revive. Read 2026-10-24 (DAOLib #255), dev/COMBAT.md "Health and IsDead".
DAO_ADDRESS(kCCCreature_OnHealthChanged, 14935)
DAO_ADDRESS(kCCObject_EventHandler, 14177)
DAO_ADDRESS(kCCArea_EventHandler, 13685)

// Conversations (dao/Dialogue.h), read 2026-09-30. The engine's
// conversation has no vtable; its file (GFF4 "CONV") is at +0x20, its size
// at +0x24, the line spoken last at +0x38, and each function below is
// __thiscall on it with a line's index, `ret 4`. Start checks the file's
// header and speaks the first line; IsLineAvailable is the test every
// line offered passes; FirstAvailable (__thiscall (count, const int
// *lines)) gives the first NPC line of a list to pass it; PlayLine speaks
// an NPC line and offers its replies (OfferReplies, which keeps the
// children that are replies and pass); Advance goes on from a chosen
// reply; End frees the participants. Dialogue mode keeps the conversation
// on the screen at +0x1C8, the NPC line at +0x2C0 and the replies shown
// at +0x1A8 (a std::vector of line indices, in the order shown, a
// fast-path reply first): ShowResponses (__thiscall (list, flag), `ret
// 8`) fills it, AddResponse (__thiscall (line), `ret 4`) adds one to the
// screen through ConversationData::AddResponse (__thiscall (text, icon),
// `ret 8`), which makes a Response (0x90 bytes: Text, Icon) in the data
// tree's Conversation.Responses. The names are ours.
DAO_ADDRESS(kConversation_Start, 21329)
DAO_ADDRESS(kConversation_IsLineAvailable, 21310)
DAO_ADDRESS(kConversation_FirstAvailable, 21313)
DAO_ADDRESS(kConversation_PlayLine, 21328)
DAO_ADDRESS(kConversation_OfferReplies, 21326)
DAO_ADDRESS(kConversation_Advance, 21324)
DAO_ADDRESS(kConversation_End, 21312)
// Who speaks a line and to whom (dao/Dialogue.h), read 2026-10-03:
// LineSpeaker and LineListener, __thiscall on the conversation (line),
// `ret 4`, answering an object's id. Each reads the line's field from the
// file (ConversationLineSpeaker 12202, ConversationLineListener 12203) and
// resolves it (0x007F2D80, __cdecl (const std::wstring *text, owner, ...)):
// OWNER is the conversation's owner, its +0x28; PLAYER the party's +0x1E8;
// HENCH:<tag> and HENCH<n> the party's members; any other text a tag,
// looked for about the owner (0x006973B0). What resolves to nothing (-1)
// answers, for the speaker, the owner (+0x28), and for the listener the
// conversation's +0x2C, the other party. Read by the address library's
// session, 2026-10-03: the scripts' BeginConversation(oInitiator, oTarget,
// rDialogFile) calls 0x006976B0 on oTarget's object with oInitiator, which
// loads the conversation (0x007EF310, __thiscall (id, id, name)) with its
// own id first, to +0x28, and oInitiator second, to +0x2C; with no name it
// takes the object's own conversation (+0x3D8). Where oInitiator is
// oTarget, the character the player controls takes its place. So +0x28 is
// oTarget, the owner, and +0x2C oInitiator. Called by PlayLine as it speaks a line, which the camera and
// lip sync follow. The names are ours (the address library, bf39c18).
DAO_ADDRESS(kConversation_LineSpeaker, 21297)
DAO_ADDRESS(kConversation_LineListener, 21298)
DAO_ADDRESS(kGameModeDialog_ShowResponses, 13949)
DAO_ADDRESS(kGameModeDialog_AddResponse, 13948)
DAO_ADDRESS(kConversationData_Constructor, 13933)
DAO_ADDRESS(kConversationData_AddResponse, 13944)
DAO_ADDRESS(kResponse_Constructor, 13885)
DAO_ADDRESS(kCCModule_EventHandler, 11051)

// The game's data tables (dao/Gda.h), read 2026-09-30. The object in this
// global keeps them; +4 of it is its store, made on first use (0x00A2CBC0).
// Its getters, __thiscall (table, row, column hash, out), `ret 0x10`, find
// the table by number (0x004DF580 on the store) and call that table's
// typed getter, C2DASource's slot 10 for an int (as GetM2DAInt does) and
// slot 9 for a float (as GetM2DAFloat does). Beside them, the same shape:
// slot 8 for a string, which GetM2DAString calls itself on the table
// (0x0065404D-0x00654088: the column trimmed of " \t", lowercased and
// hashed, the out a std::wstring), and slot 11 for a bool, whose cell
// reader (M2DA's slot 15, 0x004EC4D0) writes one byte. Read 2026-09-30.
DAO_DATA(kGdaTables, 99739)
DAO_ADDRESS(kGdaTables_GetInt, 32473)
DAO_ADDRESS(kGdaTables_GetFloat, 32472)
DAO_ADDRESS(kGdaTables_GetString, 32471)
DAO_ADDRESS(kGdaTables_GetBool, 32474)
// The store's lookup of a table by number, __thiscall, `ret 4`, null if it
// has none: what every getter above starts with. A table's row count is
// its slot 5 and its column count its slot 4, no arguments, as
// GetM2DARows and GetM2DAColumns call them (M2DA's +0x44 and +0x40, G2DA's
// +0x30 and +0x2C). The name is ours.
DAO_ADDRESS(kGdaStore_FindTable, 5262)

// The object array's lookup: NWN's `uint8_t CGameObjectArray::GetGameObject(
// OBJECT_ID, CGameObject**)`, the same shape, called by GetObjectById on the
// application's array at +0xCC.
DAO_ADDRESS(kCGameObjectArray_GetGameObject, 33354)
// How an object gets its id, read 2026-10-12 (DAOLib #86, dao/ObjectIds.h).
//
// AddObject, __thiscall on the same array ((id asked for, object), `ret
// 8`, the id given in eax; it never reads ecx, and a null object answers
// -1), has the world database make an EC::GameObjectProxy for the object
// and register it (0x004D6760, ID 5050, whose one call is to 0x004D6610,
// ID 5047), then tells the object the id it got (its slot 1) and returns
// it. EA's is the same code, its first five bytes the same and free of
// anything relative (the address library's session, 2026-10-12). Six
// callers. One is CCObject's constructor (0x00699670, ID 14185: it sets
// the AI level at +0x2B0 to -1 just after), so a creature, a placeable
// and an item each have their id from the moment they are constructed;
// the other five (IDs 13809, 20441, 21400, 12488, 11132) are other
// kinds, not read. The id asked for is the one the constructor was
// given: the scripts' CreateObject (0x00669550) comes to 0x006861F0 (ID
// 13782), which does `new CCCreature(-1)` or `new CCPlaceable(-1)`, loads
// the template into it (0x006A2900 for a creature) and puts it in the
// area; a save's objects are constructed with their saved ids. A
// template with a pool (the scripts' CreatePool) is not constructed at
// all: 0x00687830 hands out one of the pool's creatures, which has its
// id already.
//
// The world database (the save's own word: SavegameWorldDatabase) is the
// object at this global, 0x3C bytes, made by 0x004C52E0. It has three
// pools of ids, pointers at its +0x30, +0x34 and +0x38, each 0x10 bytes:
// the least id, the most, the last given, and a byte set once the pool
// has wrapped. Their ranges are fixed (0x004D59A0): 1 to 0x3FFFFFFF,
// 0x40000000 to 0x7FFFFFFF, 0x80000000 to 0xFFFFFFFE. An object that
// asks for -1 gets the first pool's next id (0x004D5DC0, ID 5038): the
// last given plus one, with no look at what is in use until the pool has
// wrapped. One that asks for an id is of the pool whose range holds it,
// and TakeId (__thiscall on the database, (id, pool), `ret 8`, a bool)
// says whether it may have it: no if an object has it (the database's
// map, 0x008170B0), and then the object gets the pool's next instead,
// with no sign but the id returned; yes otherwise, and an id past the
// pool's last given becomes the last given. So an id asked for above
// the counter moves the counter up to it, and TakeId alone moves the
// counter and registers nothing.
//
// A save keeps each pool's last given (0x004D5D00 reads the list
// SavegameWorldDatabase, field 16500, each entry a SavegameWorldDBIDgroup,
// 16501, and a SavegameWorldDBLastID, 16502), so two games loaded from
// one save number their new objects from the same id. The names are ours.
DAO_ADDRESS(kCGameObjectArray_AddObject, 33352)
DAO_DATA(kWorldDatabase, 100181)
DAO_ADDRESS(kWorldDatabase_TakeId, 5039)
// SetCombatState's engine side: the script command looks the creature up
// (0x005D58C0) and calls this on it with the state. How a creature enters
// and leaves combat, for a turn-based mode.
DAO_ADDRESS(kCCCreature_SetCombatState, 14734)

// SetAtmosphere's engine side, __thiscall on the party, no arguments: the
// controlled creature's area as the sky's commands want it, whose +0x658
// is the scene that holds the sky (dao/Atmosphere.h); null with no members
// or nobody controlled. Read 2026-10-10 from the command's handler
// (0x00664A10), DAOLib #62.
DAO_ADDRESS(kScriptBackend_ForScript_SetAtmosphere, 12271)

// The trees' light (dao/Atmosphere.h). SpeedTree's shaders take
// g_vLightDir and g_vLightDiffuse, which the tree renderer binds by name to
// its own +0xBA8 and +0xBB8 (0x0052FB60 on); AreaLayout::Update copies them
// into every tree renderer each frame (0x00532A10, 0x00530DB0) from these
// two globals, four floats each. The area's load (0x005C89B0) sets them
// once from the scene's sun (0x004625D0): the direction through the setter,
// __cdecl (const float *direction, three floats), which normalises it,
// stores it with a fourth of 1 and works out the light's angles at
// 0x00C31688; the diffuse as it is. Without a sun, (0.577, 0.577, 0.577)
// and nothing. Read 2026-10-16, DAOLib #148.
DAO_DATA(kTreeLight_Direction, 100506)
DAO_DATA(kTreeLight_Diffuse, 100509)
DAO_ADDRESS(kTreeLight_SetDirection, 7246)

// The framebuffer's effects (dao/Framebuffer.h), read 2026-10-10 from
// FB_SetEffectResource's and FB_SetEffectEnabled's handlers (0x00651880,
// 0x006519B0), DAOLib #62. The global holds the object that keeps them, a
// std::vector of Renderer::FrameBufferEffect pointers at its +0; its class
// has no RTTI, and the name is ours. Find, __thiscall on it (const
// std::wstring &name), `ret 4`: the effect of that name, or null.
// FindResource, __thiscall on an effect (const std::wstring &name),
// `ret 4`: the effect's value of that name, a Renderer::FrameBufferResource,
// or null.
DAO_DATA(kFrameBufferEffects, 99917)
DAO_ADDRESS(kFrameBufferEffects_Find, 2349)
DAO_ADDRESS(kFrameBufferEffect_FindResource, 2591)

// The sun's shadow maps asked for, __cdecl (const Partition *, count): the
// engine's parallel-split shadow maps, each {float distance, int size, int
// format}, 12 bytes, copied and their textures made (0x00454F60, which names
// them ShadowTexture%d). Called from 0x00455580 with two, {10.0, size, format}
// and {sqrt of 0x00BF20B4's 900, size, format}, the size 0x00BF20C4's, which
// the shadow quality (0x00BF20E0) sets to 512 or 1024. Read 2026-10-10 from
// the string's use, DAOLib #65. The class has no RTTI; the name is ours.
DAO_ADDRESS(kShadowMaps_SetPartitions, 2295)

// How many lights the scene's light manager holds, by kind: four ints that
// Renderer::LightManagerDefault's sixth virtual function (0x00464E30, a
// node's lights taken in) adds to and its eighth (0x00465190) takes from,
// for every light whose ILight says it is not of kind 1 (its +0xC; kind 1
// is taken in only as a point light, and not counted). Point lights (type
// 1) go to the first or the second by 0x0053C8D0's answer, type 2 to the
// third and type 3 to the fourth; ObjectNodePointLight's type is 1
// (0x0079B310). Which of ambient lights and light probes are 2 and 3 is not
// read yet. Read 2026-10-10, DAOLib #65. The names are ours.
DAO_DATA(kLightManager_PointLights, 99991)
DAO_DATA(kLightManager_PointLightsOther, 99992)
DAO_DATA(kLightManager_LightsOfType2, 99993)
DAO_DATA(kLightManager_LightsOfType3, 99994)

// The renderer's light manager taking a node's lights into its lists and
// letting them go (its sixth and eighth virtuals: __thiscall, the node's
// lights' owner, `ret 4`), one list's insert and erase of a light's node
// (__thiscall on the list, the node, `ret 4`), and the stamp at the manager's
// +0xD0 that has objects choose their lights again (__thiscall, no
// arguments). A point light goes by its ILight's kind (the ILight's +0xC) to
// one of two pairs of lists, the manager's +0x6C and +0x84 for the things
// that move and +0x24 and +0xC for the level's own pieces, the first of each
// pair for a light whose blueprint has a flag; an object takes the best
// three of the first and two of the second, by each light's score against
// it. Read 2026-10-11, DAOLib #65: examples/graphics/Graphics.cpp has the
// reading and what uses it. LightList has no RTTI; the name is ours.
DAO_ADDRESS(kLightManager_AddLights, 2701)
DAO_ADDRESS(kLightManager_RemoveLights, 2702)
DAO_ADDRESS(kLightManager_MarkChanged, 2621)
// The light manager's quality (dao/Lights.h, SetObjectsTakeUnflagged): ApplyQuality, __thiscall on the
// manager, no arguments, sets each of its six lists' most-per-object by the graphics quality
// (0x00BF22C8): at 0 the two unflagged point lists 0 and the rest 1, else unflagged 2 and flagged 3,
// probes and ambient 1; only the manager's constructor (0x00464BE0, at 0x00464D8E) calls it.
// SetLimits, __thiscall (most, chooser), `ret 8`, writes a list's most at +4 and its chooser at +0.
// Read 2026-10-24 (DAOLib #256).
DAO_ADDRESS(kLightManagerDefault_ApplyQuality, 2632)
DAO_ADDRESS(kLightList_SetLimits, 2619)
DAO_DATA(kLightManagerDefault_vftable, 61964)
DAO_ADDRESS(kLightList_Insert, 2698)
DAO_ADDRESS(kLightList_Erase, 2699)

// The chooser of the lights of a node that moves (the manager's lists +0x84
// and +0x6C; __thiscall on the manager, four arguments, `ret 0x10`), the
// choice from one list that it and the level's chooser both make (__thiscall
// on the list: the object's place, its size as a float, the object's group
// of chosen lights and six more, `ret 0x24`), and a
// point light's score against an object (its virtual at +0x84: __thiscall on
// the node, the object's place, its size, its room and another, `ret 0x10`;
// the score is left in the node's +0x184: strength times what is left of the
// light's reach there, nothing for an object of a room the light is not of).
// Read 2026-10-11, DAOLib #65.
DAO_ADDRESS(kLightManager_ChooseForMoving, 2694)
DAO_ADDRESS(kLightList_ChooseFor, 2691)
DAO_ADDRESS(kObjectNodePointLight_Score, 7628)

// A light probe's score against an object, as a point light's is (the same
// virtual, +0x84, and the same arguments): one over the distance between
// them, left in the node's +0x184, nothing for an object of a room the
// probe is not of. And the function of an object's light state (the object's
// groups of chosen lights and what the shaders are handed of them; no RTTI,
// the name is ours) that fills the second from the first when a material
// first asks in a frame: __thiscall with one argument, `ret 4`. The point
// lights go to the state's +0x40 (g_vLightParams), the chosen probe's cube
// map to +0x114 and its three irradiance matrices to +0x130, +0x170 and
// +0x1B0 (g_mLightParams), these copied only when the probe is not the one
// kept at +0x110. Read 2026-10-11, DAOLib #65: examples/graphics/Graphics.cpp
// blends a figure's probes there.
DAO_ADDRESS(kObjectNodeLightProbe_Score, 7629)
DAO_ADDRESS(kLightState_Fill, 2848)

// The ambient light each draw is given (dao/Lights.h). Fill (above, its one
// argument a pointer to a float4) sets the state's +0x80, g_vLightParams[4],
// to that argument plus the colour of each ambient light in the state's list
// at +0x30 (begin +0x34, end +0x38; an entry's *entry + 0x18 is the node), w
// 1. The argument is +0xC0 of the per-draw render context (the globals
// 0x00C1E930 and 0x00C1EC00, made by 0x00466120, which zeroes it; no other
// store to it was found), handed in by the shader parameters' provider
// (0x004F8A10: 0x11 is g_vLightParams through 0x0046BAF0, 0x1B the probe's
// cube through 0x0046BC40). A light's colour, __thiscall on the node (float4
// *out), `ret 4`: the node's +0x1A0, times the float4 at *(+0x154) + 0x2D4
// where the byte at +0x196 is set; Fill takes it for point and ambient lights
// alike. Ambient lights (ObjectNodeAmbientLight, ILight kind 2, 0x004CCAE0)
// go to the manager's +0x54 (AddLights), and the level's chooser takes them
// into the state's +0x30 only for an object whose flags (its vtable's +0xA4)
// have 0x10000: __thiscall on the manager (node **, the sun's direction, its
// colour, char ambient lights, char one more), `ret 0x14`. The chooser for
// what moves (kLightManager_ChooseForMoving) takes them only if its fourth
// argument says so, and the manager's own call (0x004654E0, for a node with
// 0x4000 in its +0x24) says not. Either finds the state by the node's
// vtable's +0x68. Read 2026-10-16, DAOLib #147.
DAO_ADDRESS(kLightManager_ChooseForLevel, 2693)
DAO_ADDRESS(kObjectNodeLight_Color, 7605)
// A point light's reach in metres: `fld [ecx+0x1B0]; ret`, __thiscall, no arguments. Read 2026-10-24
// (DAOLib #256).
DAO_ADDRESS(kObjectNodePointLight_Reach, 7567)

// A point light's node, as the light colour above is handed it (dao/Lights.h,
// OnPointLight; DAOLib #223, read 2026-10-21). Its vftable tells it from an
// ambient light (ObjectNodeAmbientLight's vftable, 0x00AF7CAC). ObjectNode's
// constructor (0x0055ED20) keeps its owner at +0x154 and its blueprint at
// +0x174; the blueprint's +0xC0 is a std::wstring (0x004C0DB0 tears it down
// with the one at +0xDC), which its slot 10 (0x0055D760, 0x00571510) hands
// out: the node's name. The owner's model's name is dao/GraphicsNodes.h's
// (seen in play, dao/Lights.cpp). The light's ILight is at +0x180, and its +0xC (the node's
// +0x18C, 0 from the constructor at 0x0053CAF0) is what AddLights
// (0x00464E30, through the ILight's slot 5, 0x0053CC40) sends to the lists
// of what moves when 1 and to the level pieces' otherwise; the blueprint's
// byte +0x15C (0x0053C8D0) picks the first list of either pair. Every light
// seen in Redcliffe had 0 and 1 there, the portraits' rig among them.
DAO_DATA(kObjectNodePointLight_vftable, 63928)
DAO_DATA(kObjectNodePointLight_ILight_vftable, 63924) // its ILight's, at the node's +0x180

// An object's light state easing its share of the sun toward a target
// (dao/Lights.h, OnSunShare; DAOLib #230, read 2026-10-22): __thiscall on
// the state, the target as a float and a char, `ret 8`. With the char 0 the
// state's +0x200 moves toward the target by at most the frame's time
// (*0x00C31650 + 0x3C) times a rate (0x00BF29AC); with it set, or once there,
// it is the target. Its one caller (0x00462470, __thiscall on the light
// manager: the state, the walk grid's sun subset for the object, the sun's
// direction and colour, and the char, `ret 0x14`; called from both choosers)
// passes 1.0, or 0.1 (0x00AE7BD8) when the manager's byte +0x120 is set and
// the subset's first entry is not -1; the subset is 0x004622C0's (the
// manager's +0x124's +0x4A0, eight bytes an entry, at the object's +0x28;
// src/dao/core/Grid.h). It then hands the state the sun's direction and its
// colour times +0x200 (0x0046BB80, to +0x60 and +0x70). The char is the
// chooser's: set when the state's +0x214 is not the manager's +0xD4.
DAO_ADDRESS(kLightState_EaseSun, 2824)

// The resource manager making a source of files of one kind without putting
// it in its list (ECResMan::ResMan::AddSource does that): __thiscall, the
// source's path as a std::wstring in the engine's own names for its folders
// ("ENV:lak303d\lak303d.rim", "DATA:global.rim"), its kind (the manager's
// +0x7C has a maker for each: 0 and 1 a directory, 2 a RIM, 3 an ERF), its
// priority and two more, `ret 0x14`; it returns the source, or nothing for
// an empty path or a kind with no maker. Its one caller asks for kind 2: a
// level's own .rim and each of its rooms' come through here as the level
// loads (logged 2026-10-11, at priority 100007). Read 2026-10-11, DAOLib
// #65: examples/graphics learns by it which level the player has come into.
DAO_ADDRESS(kResMan_MakeSource, 4641)

// What Full Detail LOD (a Nexus mod, mods/7058) patches in the exe file, for
// a plugin to change in memory with dao::Patch (DAOLib #220, examples/patch).
// The order a model's levels of detail are tried in: 16 ints, a row of four
// for each level asked for, read by 0x00617020 (the row for a level, -1
// taken as 0): 0,1,2,3 / 1,0,2,3 / 2,1,0,3 / 3,2,1,0. The inventory's and
// the character record's OnShow (their vtables' slot 5) each make the preview their character is
// drawn into, `push 0x1CB; push 0x10E; mov ecx,eax; call 0x007BFE10`: 270 by
// 459, its height pushed first, at +0x6E and +0x140. Read 2026-10-21; EA's
// build has the table and both pushes at the same offsets. The names are ours.
DAO_DATA(kModelLodOrder, 65393)
DAO_ADDRESS(kGUIInventory_OnShow, 17210)
DAO_ADDRESS(kGUICharacterRecord_OnShow, 17530)

// What fills a sprite emitter's particles for a frame, the emitter's slot +0xAC
// (DAOLib #224, read 2026-10-21; dao/Vfx.h, OnEmitterColor): __thiscall, no
// arguments, `ret`. A plain sprite's (Sprite<SpriteParticle> and
// Sprite<PhysicsSpriteParticle> both) goes on, if its opacity (+0x4E0) is above
// 0, to the fill of 32-byte vertices (slot +0xB0: 0x00550860, 0x00551450) or of
// 68-byte ones (+0xB4: 0x00550D80, 0x00551990) by its +0x510; a linked sprite,
// a ribbon, has its own (0x005850E0 for SpriteParticle, 0x00585940 for
// PhysicsSpriteParticle; the plain sprites' one function serves both of their
// vtables, and is named for the first). Each multiplies every particle's age-map colour by the
// emitter's live colour multiplier, +0x39C (rgba; dev/EFFECTS.md): of every read
// of +0x39C in the exe, these six fills are the only ones at draw. A mesh
// emitter has no such multiplier and none of these. dao/GraphicsNodes.h reads
// an emitter's model.
DAO_ADDRESS(kEmitterSprite_Fill, 8074)
DAO_ADDRESS(kEmitterRibbon_Fill, 9125)
DAO_ADDRESS(kEmitterRibbonPhysics_Fill, 9127)

// The draw distances (DAOLib #266, for DAOElgaraShaders; read by the address
// library's session, its issue #2, named in its d7275ce, 2026-10-29). An outdoor
// chunk tile chooses its level of detail by the viewer's distance
// (EC::ChunkTile::ChooseDetail, 0x005CCFE0, __thiscall (tile, viewer, layout),
// `ret 8`); LodManager::BandDistance (0x006170F0) gives a band's distance. The
// emitters' three level-of-detail distances (0x00BF22CC..0x00BF22D4) are floats
// in metres, and the trees' factor (0x00BF22D8) a float. After a change to the
// trees', CClientExoApp::RemakeTreeBounds (0x005D6C20, __thiscall (app)) remakes
// their bounds.
DAO_ADDRESS(kEC_ChunkTile_ChooseDetail, 10319)
DAO_ADDRESS(kLodManager_BandDistance, 11669)
DAO_DATA(kEmitterLodDistance0, 95756)
DAO_DATA(kEmitterLodDistance1, 95757)
DAO_DATA(kEmitterLodDistance2, 95758)
DAO_DATA(kTreeLodFactor, 95759)
DAO_ADDRESS(kCClientExoApp_RemakeTreeBounds, 10551)
} // namespace dao::addr
