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
// The game mode controller, CClientExoApp+0x114 (dao/GameModes.h), read
// 2026-09-30: its constructor, from the application's initialisation
// (0x005DC052), and SetGameMode(mode, 0, 0), __thiscall, which its
// handler for SetGameModeMessage (0x006416E0) calls with the message's
// field.
DAO_ADDRESS(kGameModeController_Constructor, 12617)
DAO_ADDRESS(kGameModeController_SetGameMode, 12618)

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
// neighbour of one that passes a test of the two (0x00687EC0, not read:
// in play a room behind a door just opened stayed unseen until the hero
// walked in), are in view; each is added to the area's list of rooms seen
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
// nothing for an ability with no record. Tick (0x0071C840, __thiscall on
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
DAO_ADDRESS(kCCCreature_Movement, 14455)
// A creature's animations (dao/Animation.h), read 2026-10-02: its
// animation set (+0x154) answers whether it has an animation and how long
// it is in milliseconds, __thiscall (id), `ret 4` each; the attack swing's
// step (0x006A41A0) asks both. The names are ours.
DAO_ADDRESS(kCCCreature_AnimationLength, 14048)
DAO_ADDRESS(kCCCreature_HasAnimation, 14049)
DAO_ADDRESS(kCCObject_RequestPath, 17573)
DAO_ADDRESS(kMovementController_Step, 33560)
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
// (0x00A49070): under 5 walkable, 5 and 6 a creature's stamp, 7 and up not
// walkable; and counts of what crowds it (0x00A49160, 0x00A49240). A
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
DAO_ADDRESS(kMovementController_Constructor, 33575)
DAO_ADDRESS(kMovementController_Free, 33555)
DAO_ADDRESS(kMovementController_IsFree, 33573)
DAO_ADDRESS(kMovementController_SetStart, 33570)
DAO_ADDRESS(kMovementController_SetGoal, 33571)
DAO_ADDRESS(kMovementController_LiftObstacle, 33563)
DAO_ADDRESS(kMovementController_Direct, 33561)
DAO_ADDRESS(kCCCreature_PlaceObstacle, 14494)
DAO_ADDRESS(kCCArea_NearestFree, 17572)
// Lets a finished search go before a new one (__thiscall, no arguments):
// the move update calls it first, then puts the mover's id in the
// controller's parameters (+0x10, its first word), the target's id at
// +0x94 when the command has one, and the range, 0.05 m, at +0x90.
DAO_ADDRESS(kMovementController_Cancel, 33569)
DAO_ADDRESS(kMovementController_NextWaypoint, 33556)
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
DAO_DATA(kInputModes, 102761)
DAO_ADDRESS(kInputModes_Group, 24295)
DAO_ADDRESS(kInputModeGroup_Allow, 24272)
DAO_ADDRESS(kInputModes_Allowed, 24264)
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
// there on return. The type numbers of the engine's older table
// (0x004C9E3E, ncs 2010, 2da 2017: NWN's) are not what Find takes. The
// names are ours.
DAO_ADDRESS(kResources_Data, 5018)
DAO_ADDRESS(kResources_Size, 5019)

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
// (dao/CameraMoves.h). The explore controller's tactical step
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
DAO_ADDRESS(kSceneCamera_SetPitch, 15001)
DAO_ADDRESS(kSceneCamera_Update, 15003)
DAO_ADDRESS(kSceneCamera_SetYaw, 15000)

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
DAO_ADDRESS(kEclipseGFxImageLoader_LoadImage, 6704)

// The talk tables (dao/Tlk.h), read 2026-09-30 from GetTlkTableString: the
// engine's CTlkTable in this global; GetString, __thiscall (strref,
// std::wstring &out, flag 0), `ret 0xC`, with the gender to use set at the
// table's +0x1C first (0 male, 1 female).
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
// SetCombatState's engine side: the script command looks the creature up
// (0x005D58C0) and calls this on it with the state. How a creature enters
// and leaves combat, for a turn-based mode.
DAO_ADDRESS(kCCCreature_SetCombatState, 14734)

} // namespace dao::addr
