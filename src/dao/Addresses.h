// Every address this library reads from the game, each read from the
// executable dao/core/Fingerprint.h names (the GOG 1.05.13263.0 build) in
// Ghidra, as a preferred-base virtual address: what the Ghidra listing
// shows. Go through dao::Module::Address, which relocates it.
//
// DAO_ADDRESS is code, DAO_DATA a global. The second argument is the
// engine's name for it, as it is named in Ghidra: tools/names.py reads this file and tools/ghidra-mcp.ps1 applies
// those names to the analysis on every launch, so a finding recorded here
// is read back beside the code it was made from. `Class::Method` becomes
// the namespace Class.
//
// Add a row with what it is, how it was found, and the date. An address
// with no story is one nobody can check.
#pragma once

#include <cstdint>

#define DAO_ADDRESS(name, engineName, va) inline constexpr std::uintptr_t name = (va);
#define DAO_DATA(name, engineName, va) inline constexpr std::uintptr_t name = (va);

namespace dao::addr
{

// The exe's entry point (mainCRTStartup), from IMAGE_OPTIONAL_HEADER, which
// the loader detours to run plugins on the main thread before the game.
DAO_ADDRESS(kEntryPoint, "entry", 0x008E3024)

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
DAO_ADDRESS(kCVirtualMachineCommands_InitializeCommands, "CVirtualMachineCommands::InitializeCommands", 0x00675210)
// `table = this->[+0xC]; table[id] = fn`, `ret 0xC`, the name unused.
DAO_ADDRESS(kCVirtualMachineCommands_Register, "CVirtualMachineCommands::Register", 0x0064DD40)
// Slot 2: an id of 0x35F or more, or an empty slot, is error -642
// (0xFFFFFD7E); otherwise `table[id](id, paramCount)` with the commands
// object in ecx, its result returned.
DAO_ADDRESS(kCVirtualMachineCommands_ExecuteCommand, "CVirtualMachineCommands::ExecuteCommand", 0x0064DD60)

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
DAO_ADDRESS(kGameModeController_HandleUserTogglePause, "GameModeController::HandleMessage_UserTogglePauseMessage",
            0x006403C0)
// The application object: allocated 0x2F0 bytes and constructed at
// 0x005DCC4A (constructor 0x005DC510, whose vftable stores name the class
// CClientExoApp), stored here, cleared at 0x005D42D3.
DAO_DATA(kClientExoApp, "g_clientExoApp", 0x00C327EC)
DAO_ADDRESS(kCClientExoApp_Constructor, "CClientExoApp::CClientExoApp", 0x005DC510)
// Pause or unpause. __thiscall void(bool paused, bool second), `ret 8`. Does
// nothing if the flag already says `paused`. Otherwise it sets the flag (an
// object at +0xD8 whose byte at +0xC is the value, set through its vtable
// slot +0x54), sets a second such object at +0xE8 from `second`, keeps the
// pause's start time at +0xF8 and a running total of paused time at +0xFC,
// and between those reacts to the game mode in (this+0x114)->+0x78 (modes
// 0-5, UI updates per mode). Named SetPaused by what it does; the engine's
// name is not in the exe. What `second` means is not yet read: the pause
// key passes 1.
DAO_ADDRESS(kCClientExoApp_SetPaused, "CClientExoApp::SetPaused", 0x005D6740)
// The game mode controller, CClientExoApp+0x114 (dao/GameModes.h), read
// 2026-09-30: its constructor, from the application's initialisation
// (0x005DC052), and SetGameMode(mode, 0, 0), __thiscall, which its
// handler for SetGameModeMessage (0x006416E0) calls with the message's
// field.
DAO_ADDRESS(kGameModeController_Constructor, "GameModeController::GameModeController", 0x00641370)
DAO_ADDRESS(kGameModeController_SetGameMode, "GameModeController::SetGameMode", 0x006414A0)

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
DAO_ADDRESS(kGameModeExplore_CastCursorRay, "GameModeExplore::CastCursorRay", 0x0070B590)
DAO_ADDRESS(kGameModeExplore_UpdateCursor, "GameModeExplore::UpdateCursor", 0x0070F270)
DAO_ADDRESS(kGameModeExplore_CanMoveTo, "GameModeExplore::CanMoveTo", 0x0070C840)
DAO_ADDRESS(kGameModeExplore_MoveTo, "GameModeExplore::MoveTo", 0x0070BE80)
DAO_ADDRESS(kWorld_Raycast, "World::Raycast", 0x006497A0)
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
DAO_DATA(kWorld_RaycastResults, "World::g_raycastResults", 0x00C347B0)
DAO_ADDRESS(kWorld_CurrentScene, "World::CurrentScene", 0x00648B30)
DAO_ADDRESS(kCCArea_LineOfSight, "CCArea::LineOfSight", 0x0067C3F0)
DAO_ADDRESS(kCCPlaceable_BlocksSight, "CCPlaceable::BlocksSight", 0x0067A670)
DAO_ADDRESS(kCCObject_Area, "CCObject::Area", 0x00695180)
DAO_ADDRESS(kCCModule_CurrentArea, "CCModule::CurrentArea", 0x005FE470)
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
DAO_ADDRESS(kCCArea_RoomsSeen, "CCArea::RoomsSeen", 0x006886F0)
// Which room a thing is in, read 2026-10-01 from how the party's rooms are
// gathered. An object keeps its room at +0x41C and the room's number at
// +0x150 (-1 in none), set by its own refresh (0x00695D70), which asks its
// area for the room at its position raised by its height: RoomAt,
// __thiscall on the area (const Vector4 *), `ret 4`, which hands the point
// to the layout (its slot 2) and answers the room or null.
DAO_ADDRESS(kCCArea_RoomAt, "CCArea::RoomAt", 0x0067B3A0)
DAO_ADDRESS(kAreaLayout_Update, "AreaLayout::Update", 0x005CA1B0)
DAO_ADDRESS(kIndoorStreamPolicy_Decide, "EC::IndoorStreamPolicy::Decide", 0x005D3FA0)
DAO_ADDRESS(kCRoom_SetState, "CRoom::SetState", 0x005D31D0)
DAO_ADDRESS(kBounds_DistanceSquared, "Bounds::DistanceSquared", 0x004D2D30)

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
DAO_ADDRESS(kCCCreatureStats_Cooldowns, "CCCreatureStats::Cooldowns", 0x0071CA00)
DAO_ADDRESS(kCCCreatureStats_SetCooldown, "CCCreatureStats::SetCooldown", 0x0071DCE0)
DAO_ADDRESS(kCooldownManager_Find, "CooldownManager::Find", 0x0071C250)
DAO_ADDRESS(kCooldownManager_Tick, "CooldownManager::Tick", 0x0071C840)
DAO_ADDRESS(kCCCreature_Movement, "CCCreature::Movement", 0x006A2570)
DAO_ADDRESS(kCCObject_RequestPath, "CCObject::RequestPath", 0x0073EA80)
DAO_ADDRESS(kMovementController_Step, "MovementController::Step", 0x00A62640)
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
DAO_ADDRESS(kCCCreature_PathBlocker, "CCCreature::PathBlocker", 0x006A7E10)
DAO_ADDRESS(kCCPlaceable_Covers, "CCPlaceable::Covers", 0x0074DC40)
DAO_ADDRESS(kMovementParameters_Constructor, "MovementParameters::MovementParameters", 0x00A62830)
DAO_ADDRESS(kMovementController_Constructor, "MovementController::MovementController", 0x00A62B20)
DAO_ADDRESS(kMovementController_Free, "MovementController::Free", 0x00A625B0)
DAO_ADDRESS(kMovementController_IsFree, "MovementController::IsFree", 0x00A62A90)
DAO_ADDRESS(kMovementController_SetStart, "MovementController::SetStart", 0x00A62990)
DAO_ADDRESS(kMovementController_SetGoal, "MovementController::SetGoal", 0x00A629C0)
DAO_ADDRESS(kMovementController_LiftObstacle, "MovementController::LiftObstacle", 0x00A627D0)
DAO_ADDRESS(kMovementController_Direct, "MovementController::Direct", 0x00A626F0)
DAO_ADDRESS(kCCCreature_PlaceObstacle, "CCCreature::PlaceObstacle", 0x006A3BC0)
DAO_ADDRESS(kCCArea_NearestFree, "CCArea::NearestFree", 0x0073E7F0)
// Lets a finished search go before a new one (__thiscall, no arguments):
// the move update calls it first, then puts the mover's id in the
// controller's parameters (+0x10, its first word), the target's id at
// +0x94 when the command has one, and the range, 0.05 m, at +0x90.
DAO_ADDRESS(kMovementController_Cancel, "MovementController::Cancel", 0x00A62960)
DAO_ADDRESS(kMovementController_NextWaypoint, "MovementController::NextWaypoint", 0x00A625E0)
DAO_ADDRESS(kPathSearch_Waypoint, "PathSearch::Waypoint", 0x00A47620)

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
DAO_ADDRESS(kVfxManager_Get, "VfxManager::Get", 0x0079E820)
DAO_ADDRESS(kVfxManager_Place, "VfxManager::Place", 0x0079EDC0)
DAO_ADDRESS(kVfxManager_Find, "VfxManager::Find", 0x0079D780)
DAO_ADDRESS(kVfxManager_Remove, "VfxManager::Remove", 0x0079EAE0)
DAO_ADDRESS(kCCObject_SetPosition, "CCObject::SetPosition", 0x00695540)
DAO_ADDRESS(kGameModeExplore_AimRingEffect, "GameModeExplore::AimRingEffect", 0x0070AB20)
DAO_ADDRESS(kGameModeExplore_UpdateAim, "GameModeExplore::UpdateAim", 0x00710F80)
DAO_DATA(kGameModeExplore_AimRing, "GameModeExplore::g_aimRing", 0x00C0679C)

// Creature actions (dev/TURN_BASED.md). Read 2026-09-30 from AddCommand's
// implementation after the probe showed it running about five times a
// second in combat; not yet verified in play.
//
// An object by its id, on the application: the script command AddCommand
// pops an object id and passes it here, `mov ecx, [g_clientExoApp]`, one
// argument, and gets the object back in eax.
DAO_ADDRESS(kCClientExoApp_GetObjectById, "CClientExoApp::GetObjectById", 0x005D5BE0)
// The typed lookups beside it, found 2026-09-30 by tools/script_backends.py
// (dev/object_lookups.csv): the same shape, then a cast through the virtual
// slot that only the named class answers with `this`. Each __thiscall, the
// id, `ret 4`. GetGameObjectById does not cast: the object as the array
// holds it. The creature, placeable and object lookups were checked in play
// 2026-09-30 by the probe's script checks (dev/LIBRARY.md): each found the
// object the engine's getters answered for, on every call. The waypoint's
// was not called, and GetGameObjectById is not used by the checks.
DAO_ADDRESS(kCClientExoApp_GetGameObjectById, "CClientExoApp::GetGameObjectById", 0x005D5840)
DAO_ADDRESS(kCClientExoApp_GetCreatureById, "CClientExoApp::GetCreatureById", 0x005D58C0)
DAO_ADDRESS(kCClientExoApp_GetPlaceableById, "CClientExoApp::GetPlaceableById", 0x005D5B40)
DAO_ADDRESS(kCClientExoApp_GetWayPointById, "CClientExoApp::GetWayPointById", 0x005D5C60)
// The module, read 2026-09-30: the object manager (+0xCC) looks up the id
// at +0x2B8 and the result is cast through slot 7, CCModule's.
DAO_ADDRESS(kCClientExoApp_GetModule, "CClientExoApp::GetModule", 0x005D4FC0)
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
DAO_ADDRESS(kCCObject_QueueCommand, "CCObject::QueueCommand", 0x0073CD10)

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
DAO_ADDRESS(kMessages_Send, "Messages::Send", 0x004D20C0)
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
DAO_ADDRESS(kMessages_FromText, "Messages::FromText", 0x004D2320)
DAO_ADDRESS(kMessages_FromBindingText, "Messages::FromBindingText", 0x007957D0)
DAO_ADDRESS(kMessages_FromCallbackText, "Messages::FromCallbackText", 0x00524BE0)
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
DAO_ADDRESS(kGroupedInputMappings_Get, "Eclipse::GroupedInputMappings::Get", 0x00895DC0)
DAO_DATA(kGroupedInputMappings, "g_groupedInputMappings", 0x00C3B9D8)
DAO_ADDRESS(kGroupedInputMappings_AddAction, "Eclipse::GroupedInputMappings::AddAction", 0x00894530)
DAO_ADDRESS(kGroupedInputMappings_AddMapping, "Eclipse::GroupedInputMappings::AddMapping", 0x00894BB0)
DAO_ADDRESS(kGroupedInputMappings_SetKey, "Eclipse::GroupedInputMappings::SetKey", 0x00894CC0)
DAO_ADDRESS(kInput_RegisterActions, "Input::RegisterActions", 0x006E30D0)
DAO_ADDRESS(kInput_ResetKeys, "Input::ResetKeys", 0x006EA9F0)
DAO_ADDRESS(kInput_LoadKeyBindings, "Input::LoadKeyBindings", 0x006EA8F0)
DAO_ADDRESS(kInput_SaveKeyBindings, "Input::SaveKeyBindings", 0x006EA970)
DAO_ADDRESS(kInputModes_Get, "InputModes::Get", 0x00891010)
DAO_DATA(kInputModes, "g_inputModes", 0x00C3B800)
DAO_ADDRESS(kInputModes_Group, "InputModes::Group", 0x00890C60)
DAO_ADDRESS(kInputModeGroup_Allow, "InputModeGroup::Allow", 0x0088F810)
DAO_ADDRESS(kInputModes_Allowed, "InputModes::Allowed", 0x0088F430)
DAO_ADDRESS(kMessageRegistry_Get, "MessageRegistry::Get", 0x004740B0)
DAO_DATA(kMessageRegistry, "g_messageRegistry", 0x00C2C09C)
DAO_ADDRESS(kMessageRegistry_Dispatch, "MessageRegistry::Dispatch", 0x004D8330)
DAO_ADDRESS(kMessageRegistry_Find, "MessageRegistry::Find", 0x004D82A0)
DAO_ADDRESS(kMessageRegistry_Register, "MessageRegistry::Register", 0x004D9190)

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
DAO_DATA(kGuiManager, "g_scaleformManager", 0x00C31264)
DAO_ADDRESS(kEclipseGFxFSCommandHandler_Callback, "EclipseGFxFSCommandHandler::Callback", 0x00511030)
DAO_ADDRESS(kEclipseGFxExternalInterface_Callback, "EclipseGFxExternalInterface::Callback", 0x00511050)
DAO_ADDRESS(kGuiCommands_DispatchFSCommand, "GuiCommands::DispatchFSCommand", 0x0051F5F0)
DAO_ADDRESS(kGuiCommands_DispatchExternalInterface, "GuiCommands::DispatchExternalInterface", 0x0051F640)
DAO_ADDRESS(kGuiCommands_Find, "GuiCommands::Find", 0x0051F4F0)
DAO_ADDRESS(kGuiCommands_RegisterFSCommand, "GuiCommands::RegisterFSCommand", 0x005201C0)
DAO_ADDRESS(kGuiCommands_RegisterExternalInterface, "GuiCommands::RegisterExternalInterface", 0x00520280)
DAO_ADDRESS(kCrc32, "Crc32", 0x004CA960)
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
DAO_ADDRESS(kGuiMovieSet_Begin, "GuiMovieSet::Begin", 0x006AD480)
DAO_ADDRESS(kGuiMovieSet_Next, "GuiMovieSet::Next", 0x006AD4D0)
DAO_ADDRESS(kScaleformManager_FindMovie, "GUI::ScaleformManager::FindMovie", 0x00511730)
DAO_ADDRESS(kGuiMovie_Invoke, "GuiMovie::Invoke", 0x00518850)
DAO_ADDRESS(kDragonAgeGUISystem_IsMovieOnStage, "GUI::DragonAgeGUISystem::IsMovieOnStage", 0x0060C920)
DAO_ADDRESS(kDragonAgeGUISystem_CallFunctionOnMovie, "GUI::DragonAgeGUISystem::CallFunctionOnMovie", 0x0060C990)
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
DAO_ADDRESS(kScaleformManager_Initialize, "GUI::ScaleformManager::Initialize", 0x00515FE0)
DAO_ADDRESS(kScaleformManager_GetMovieDef, "GUI::ScaleformManager::GetMovieDef", 0x005146E0)
DAO_ADDRESS(kScaleformManager_CreateMovieView, "GUI::ScaleformManager::CreateMovieView", 0x00514940)
DAO_ADDRESS(kScaleformManager_OpenMovie, "GUI::ScaleformManager::OpenMovie", 0x00514B10)
DAO_ADDRESS(kGuiMovie_Construct, "GuiMovie::GuiMovie", 0x0051A040)
DAO_ADDRESS(kGUISystem_OpenScreen, "GUI::GUISystem::OpenScreen", 0x00523BF0)
DAO_ADDRESS(kGFxLoader_CreateMovie, "GFxLoader::CreateMovie", 0x0090C470)
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
DAO_ADDRESS(kGUIScene_Constructor, "GUI::GUIScene::GUIScene", 0x00A55AC0)
DAO_ADDRESS(kGUISystem_SceneIdByName, "GUI::GUISystem::SceneIdByName", 0x00523ED0)
DAO_ADDRESS(kGUISystem_Scene, "GUI::GUISystem::Scene", 0x00523FD0)
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
DAO_ADDRESS(kGUISystem_AddScene, "GUI::GUISystem::AddScene", 0x005240D0)
DAO_ADDRESS(kGUISystem_CloseScreen, "GUI::GUISystem::CloseScreen", 0x00523FF0)
DAO_ADDRESS(kGUISystem_HideScreen, "GUI::GUISystem::HideScreen", 0x00523CB0)
DAO_ADDRESS(kCClientExoApp_SetGUISystem, "CClientExoApp::SetGUISystem", 0x005D4F20)
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
DAO_ADDRESS(kEclipseGFxFileOpener_OpenFile, "EclipseGFxFileOpener::OpenFile", 0x005113A0)
DAO_ADDRESS(kScaleformResourceFile_Constructor, "GUI::ScaleformResourceFile::ScaleformResourceFile", 0x00523220)
DAO_ADDRESS(kResources_Find, "Resources::Find", 0x004D5BA0)
DAO_ADDRESS(kResources_Release, "Resources::Release", 0x004D5860)

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
DAO_ADDRESS(kSaveGame_SlotFolder, "SaveGame::SlotFolder", 0x00643DD0)
DAO_ADDRESS(kSaveGame_Write, "SaveGame::Write", 0x00646280)
DAO_ADDRESS(kSaveGame_FindSave, "SaveGame::FindSave", 0x00645E00)
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
DAO_ADDRESS(kGameModeLoading_Worker, "GameModeLoading::Worker", 0x006C6520)
DAO_ADDRESS(kSaveLoadProxy_ScreenshotPath, "SaveLoadProxy::ScreenshotPath", 0x006CB7A0)

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
DAO_ADDRESS(kParty_Select, "Party::Select", 0x00639E50)
DAO_ADDRESS(kCameraControllerExplore_SnapToTarget, "CameraControllerExplore::SnapToTarget", 0x0076FCE0)
DAO_ADDRESS(kCameraControllerExplore_Update, "CameraControllerExplore::Update", 0x007714B0)
DAO_ADDRESS(kCameraControllerExplore_OnTargetChanged, "CameraControllerExplore::OnTargetChanged", 0x007709B0)
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
DAO_ADDRESS(kCameraControllerExplore_TacticalStep, "CameraControllerExplore::TacticalStep", 0x00771060)
DAO_ADDRESS(kCameraControllerExplore_PullToParty, "CameraControllerExplore::PullToParty", 0x0076FDD0)
DAO_ADDRESS(kCameraControllerExplore_PlayerPan, "CameraControllerExplore::PlayerPan", 0x007703F0)
DAO_ADDRESS(kCameraControllerExplore_KeepInView, "CameraControllerExplore::KeepInView", 0x00770BB0)
DAO_ADDRESS(kCameraControllerBase_ReadInput, "CameraControllerBase::ReadInput", 0x007EDC60)
DAO_ADDRESS(kCameraControllerBase_SetTarget, "CameraControllerBase::SetTarget", 0x007EE840)
DAO_ADDRESS(kCameraControllerBase_Zoom, "CameraControllerBase::Zoom", 0x007EE7E0)
DAO_ADDRESS(kCameraControllerBase_TurnBehind, "CameraControllerBase::TurnBehind", 0x007EE880)
DAO_ADDRESS(kSceneCameraManager_Update, "SceneCameraManager::Update", 0x006C4C60)
DAO_ADDRESS(kSceneCamera_SetPitch, "SceneCamera::SetPitch", 0x006C2BC0)
DAO_ADDRESS(kSceneCamera_Update, "SceneCamera::Update", 0x006C2C20)
DAO_ADDRESS(kSceneCamera_SetYaw, "SceneCamera::SetYaw", 0x006C2B90)

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
DAO_ADDRESS(kCCCreature_UpdatePortrait, "CCCreature::UpdatePortrait", 0x006AE2D0)
// Its pair, read 2026-09-30: ReleasePortrait, a CCCreature method with no
// arguments that returns 1, destroys the renderer (0x007F3D00, which
// destroys the clone at +0x20 with ~CCCreature), frees it and clears the
// creature's +0x78C. The engine calls it in four places, each beside one
// of UpdatePortrait's callers: a screen that shows a portrait renders it
// and releases it again. Otherwise ~CCCreature destroys a renderer still
// held, when the creature goes with its area.
DAO_ADDRESS(kCCCreature_ReleasePortrait, "CCCreature::ReleasePortrait", 0x0069F470)
DAO_ADDRESS(kPortraitRenderer_Constructor, "PortraitRenderer::PortraitRenderer", 0x007F3D80)
DAO_ADDRESS(kPortraitRenderer_Setup, "PortraitRenderer::Setup", 0x007F3E20)
DAO_ADDRESS(kPortraitRenderer_Render, "PortraitRenderer::Render", 0x007F46E0)
DAO_ADDRESS(kEclipseGFxImageLoader_LoadImage, "EclipseGFxImageLoader::LoadImage", 0x00516A20)

// The talk tables (dao/Tlk.h), read 2026-09-30 from GetTlkTableString: the
// engine's CTlkTable in this global; GetString, __thiscall (strref,
// std::wstring &out, flag 0), `ret 0xC`, with the gender to use set at the
// table's +0x1C first (0 male, 1 female).
DAO_DATA(kTlkTable, "g_tlkTable", 0x00C1D08C)
DAO_ADDRESS(kCTlkTable_GetString, "CTlkTable::GetString", 0x004E4990)

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
DAO_ADDRESS(kCreatureProperties_GetTotal, "CreatureProperties::GetTotal", 0x006435C0)
DAO_ADDRESS(kCreatureProperties_GetBase, "CreatureProperties::GetBase", 0x006436B0)
DAO_ADDRESS(kCreatureProperties_GetCurrent, "CreatureProperties::GetCurrent", 0x006437A0)
DAO_ADDRESS(kCreatureProperties_GetModifier, "CreatureProperties::GetModifier", 0x00643890)
DAO_ADDRESS(kCreatureStats_SetBase, "CCCreatureStats::SetBase", 0x0071B230)
DAO_ADDRESS(kCreatureStats_SetCurrent, "CCCreatureStats::SetCurrent", 0x0071B320)
DAO_ADDRESS(kCreatureStats_SetModifier, "CCCreatureStats::SetModifier", 0x0071B280)

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
DAO_ADDRESS(kTokenManager_Get, "SmartData::TokenManager::Get", 0x00455BF0)
DAO_DATA(kTokenManager, "g_tokenManager", 0x00C325B0)
DAO_ADDRESS(kTokenManager_Find, "SmartData::TokenManager::Find", 0x005C6E80)
DAO_DATA(kNullAttribute, "SmartData::g_nullAttribute", 0x00BF99E0)
DAO_ADDRESS(kGuiExternal_GetValue, "GuiExternal::GetValue", 0x0051EC60)
DAO_ADDRESS(kGuiExternal_GetNumber, "GuiExternal::GetNumber", 0x0051CFD0)
DAO_ADDRESS(kGuiExternal_AddTokenObserver, "GuiExternal::AddTokenObserver", 0x0051CD50)
DAO_ADDRESS(kGuiExternal_SendMessage, "GuiExternal::SendMessage", 0x0051C800)
// The plain attributes' own vtables (RTTI), by which an attribute found
// by path says its type: SmartData::Attribute<T, NoMemberLookup,
// InternalStorage, ObserverNotification> for each T, and float's custom
// storage beside. Each one's slot 21 is Set(T), __thiscall, `ret 4`: it
// stores the value and, if it changed, notifies the attribute's observers,
// which is how a bound movie learns of it (read 2026-09-30 from
// Attribute<int>'s: compare, store, notify). Slot 14 does not tell types
// apart: the linker folded it to two shared bodies.
DAO_DATA(kAttributeBool_Vftable, "SmartData::Attribute<bool>::vftable", 0x00AEA6A4)
DAO_DATA(kAttributeInt_Vftable, "SmartData::Attribute<int>::vftable", 0x00ADE4E4)
DAO_DATA(kAttributeUnsignedInt_Vftable, "SmartData::Attribute<unsigned int>::vftable", 0x00AED6AC)
DAO_DATA(kAttributeShort_Vftable, "SmartData::Attribute<short>::vftable", 0x00AEC43C)
DAO_DATA(kAttributeByte_Vftable, "SmartData::Attribute<unsigned char>::vftable", 0x00B1B0BC)
DAO_DATA(kAttributeFloat_Vftable, "SmartData::Attribute<float>::vftable", 0x00B00CC4)
DAO_DATA(kAttributeFloatCustom_Vftable, "SmartData::Attribute<float,CustomStorage>::vftable", 0x00B1B12C)
// The engine's way to make and queue a command: __thiscall(type, parameters
// (a CDataArrays), addToFront, static, playerIssued, override, the +8
// word). Called from 13 places: effects (ExecuteEffect, from CCObject and
// CCCreature), drawing and sheathing weapons with their animation (from
// SetCombatState among others), modal abilities' animations, jumps, moves
// and conversations, and QueuePlayerCommand. The names are ours.
DAO_ADDRESS(kCCObject_QueueNewCommand, "CCObject::QueueNewCommand", 0x0073D000)
// QueueNewCommand with playerIssued 1, not static: the player's orders, as
// explore mode's PerformUseObjectMessage handler and the functions beside
// it (0x007F6410, 0x007F64C0, 0x007F6690) queue them.
DAO_ADDRESS(kCCObject_QueuePlayerCommand, "CCObject::QueuePlayerCommand", 0x0073D060)
// The player's orders (dao/Orders.h), read 2026-09-30: QueuePlayerCommand's
// three callers, each __stdcall, each signalling OrderReceived (__cdecl
// (CCCreature *, target), EVENT_TYPE_ON_ORDER_RECEIVED to the creature)
// after. Attack (creature, target) `ret 8`; UseObject (creature, object,
// action) `ret 0xC`; UseAbility (ability, creature, item, target, const
// Vector4 *point) `ret 0x14`. QueuePlayerCommand itself is __thiscall
// (type, CDataArrays *parameters from operator new, addToFront), `ret 0xC`.
// The names are ours.
DAO_ADDRESS(kOrders_Attack, "Orders::Attack", 0x007F6410)
DAO_ADDRESS(kOrders_UseObject, "Orders::UseObject", 0x007F6550)
DAO_ADDRESS(kOrders_UseAbility, "Orders::UseAbility", 0x007F6690)
DAO_ADDRESS(kOrders_OrderReceived, "Orders::OrderReceived", 0x007F6370)
// The object's queue, read 2026-09-30 from GetCurrentCommand,
// GetCommandByIndex and GetCommandQueueSize: a list at +0x2CC, its count
// at +0x2D4 (the list's +8), and at +0x2DC a pointer to the slot holding
// the current command, the queue's head. GetCurrentCommand is that read,
// __thiscall with no arguments; GetCommandByIndex walks the list,
// __thiscall(index), `ret 4`, null past its end.
DAO_ADDRESS(kCCObject_GetCurrentCommand, "CCObject::GetCurrentCommand", 0x0073C5F0)
DAO_ADDRESS(kCCObject_GetCommandByIndex, "CCObject::GetCommandByIndex", 0x0073C6D0)
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
DAO_ADDRESS(kCCObject_StepCommands, "CCObject::StepCommands", 0x0073D320)
DAO_ADDRESS(kCCObject_StartNextCommand, "CCObject::StartNextCommand", 0x0073C7D0)
DAO_ADDRESS(kCCObject_ClearCurrentCommand, "CCObject::ClearCurrentCommand", 0x0073C730)
DAO_ADDRESS(kCommands_Start, "Commands::Start", 0x0064A500)
DAO_ADDRESS(kCommands_End, "Commands::End", 0x0064A510)

// A second script command table, beside CVirtualMachineCommands: RTTI's
// CClientVirtualMachineCommands, a CVirtualMachineCmdImplementer, made by
// the same start-up function (0x005DB7F0). Its InitializeCommands
// registers 103 handlers of its own, among them an AddCommand and a
// SetActive that queue commands as the others do. dao::script's table and
// intercepts are the other table's. Whether single-player runs a script on
// this one is not known yet: the probe counts its AddCommand's calls.
// Read 2026-09-30.
DAO_ADDRESS(kCClientVirtualMachineCommands_Constructor, "CClientVirtualMachineCommands::CClientVirtualMachineCommands",
            0x006D3A00)
DAO_ADDRESS(kCClientVirtualMachineCommands_InitializeCommands, "CClientVirtualMachineCommands::InitializeCommands",
            0x006DB420)
DAO_ADDRESS(kCClientVirtualMachineCommands_AddCommand, "CClientVirtualMachineCommands::AddCommand", 0x006DA1F0)
DAO_ADDRESS(kCClientVirtualMachineCommands_SetActive, "CClientVirtualMachineCommands::SetActive", 0x006D67A0)

// The game's update, as a task on the engine's worker threads: from the
// deliberate crash of 2026-09-30, whose stack ran CClientExoApp's start-up
// through the message dispatch down to Eclipse::UpdateTask's slot 3, which
// calls the per-frame update at 0x00412EB0, on a Multithreading::Thread.
// The probe counts a stack sample as game logic when this function's frame
// is on it; the range is Ghidra's.
DAO_ADDRESS(kEclipseUpdateTask_Execute, "Eclipse::UpdateTask::vfunction3", 0x00413310)
// What that slot calls each frame (dao/Frame.h), read 2026-09-30:
// __thiscall, no arguments, true to go on running, on the object in the
// global below (0x280 bytes, made by 0x0040DCB0 at start-up; no vtable, so
// no RTTI name: named here for what it runs). It returns false at once if
// its byte at +0xCD is set, a quit flag by how the task uses the answer.
DAO_DATA(kEclipseGame, "g_eclipseGame", 0x00C1D0A0)
DAO_ADDRESS(kEclipseGame_Update, "EclipseGame::Update", 0x00412EB0)
inline constexpr std::uintptr_t kEclipseUpdateTask_ExecuteEnd = 0x00413368;

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
DAO_DATA(kVirtualMachine, "g_pVirtualMachine", 0x00C1D080)
DAO_ADDRESS(kCVirtualMachine_StackPopInteger, "CVirtualMachine::StackPopInteger", 0x00A2F3C0) // 453/464
DAO_ADDRESS(kCVirtualMachine_StackPopFloat, "CVirtualMachine::StackPopFloat", 0x00A2F410)     // 72/74
DAO_ADDRESS(kCVirtualMachine_StackPopVector, "CVirtualMachine::StackPopVector", 0x00A2F460)   // 18/19
DAO_ADDRESS(kCVirtualMachine_StackPopString, "CVirtualMachine::StackPopString", 0x00A2F4C0)   // 136/145
DAO_ADDRESS(kCVirtualMachine_StackPopArray, "CVirtualMachine::StackPopArray", 0x00A2F520)     // 4/5, arrays of any type
DAO_ADDRESS(kCVirtualMachine_StackPopResource, "CVirtualMachine::StackPopResource", 0x00A2F5F0) // 15/15
DAO_ADDRESS(kCVirtualMachine_StackPopEngineStructure, "CVirtualMachine::StackPopEngineStructure",
            0x00A2F650) // 99/99: effect, location, event, command
DAO_ADDRESS(kCVirtualMachine_StackPopObject, "CVirtualMachine::StackPopObject", 0x00A2F6D0)     // 415/426
DAO_ADDRESS(kCVirtualMachine_StackPushInteger, "CVirtualMachine::StackPushInteger", 0x00A2D650) // 77/77
DAO_ADDRESS(kCVirtualMachine_StackPushFloat, "CVirtualMachine::StackPushFloat", 0x00A2D690)     // 1/1, and the order
DAO_ADDRESS(kCVirtualMachine_StackPushVector, "CVirtualMachine::StackPushVector", 0x00A2D6D0)   // 2/2
DAO_ADDRESS(kCVirtualMachine_StackPushString, "CVirtualMachine::StackPushString", 0x00A2D730)   // 5/5
DAO_ADDRESS(kCVirtualMachine_StackPushEngineStructure, "CVirtualMachine::StackPushEngineStructure",
            0x00A2D920) // 31/31: command, effect, location, event
DAO_ADDRESS(kCVirtualMachine_StackPushObject, "CVirtualMachine::StackPushObject", 0x00A2D980) // 3/3
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
DAO_ADDRESS(kCVirtualMachineCommands_CreateGameDefinedStructure, "CVirtualMachineCommands::CreateGameDefinedStructure",
            0x00652D60)
DAO_ADDRESS(kCVirtualMachineCommands_DestroyGameDefinedStructure,
            "CVirtualMachineCommands::DestroyGameDefinedStructure", 0x0066DFB0)
DAO_ADDRESS(kCVirtualMachineCommands_CopyGameDefinedStructure, "CVirtualMachineCommands::CopyGameDefinedStructure",
            0x00674AF0)
// The structures' own constructors, from Create and Copy.
DAO_ADDRESS(kCScriptEvent_Constructor, "CScriptEvent::CScriptEvent", 0x0069BAC0)
// CScriptEvent's vtable, which its constructor stores (RTTI): how
// dao/Events.h tells a script event's data without reading its RTTI.
DAO_DATA(kCScriptEvent_Vftable, "CScriptEvent::vftable", 0x00B0F12C)
DAO_ADDRESS(kCScriptEvent_CopyConstructor, "CScriptEvent::CScriptEvent_copy", 0x0069CB10)
DAO_ADDRESS(kCScriptLocation_Constructor, "CScriptLocation::CScriptLocation", 0x007ABD00)
DAO_ADDRESS(kCScriptLocation_CopyConstructor, "CScriptLocation::CScriptLocation_copy", 0x0064DD90)
DAO_ADDRESS(kCGameEffect_Constructor, "CGameEffect::CGameEffect", 0x00A652A0)
DAO_ADDRESS(kCGameEffect_CopyConstructor, "CGameEffect::CGameEffect_copy", 0x00A65130)
// The clocks, read 2026-09-30 (dev/TODO.md, item 4). The engine's
// microsecond clock (the performance counter over its frequency) on a
// global timer, read through a thunk from 31 places, world code among
// them; Scaleform's GTimer keeps its clock function in a global (the
// performance counter's in milliseconds, or GetTickCount's) that the UI
// reads through a jump. Which clock gives the world its frame time is not
// yet pinned. The names are ours.
DAO_ADDRESS(kClock_Microseconds, "Clock::Microseconds", 0x004BFFF0)
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
DAO_ADDRESS(kWorldTimer_Constructor, "EC::WorldTimer::WorldTimer", 0x00A35F30)
// The game's time scale (dao/Clock.h, SetSpeed), read 2026-09-30. The
// application keeps it at +0x2C8; SetTimeScale, __thiscall (scale, seconds,
// owner id or -1), `ret 0xC`, sets it at once for 0 seconds, else records
// the scale it had (+0x2BC), the one asked (+0x2C0), the time (+0x2C4) and
// how far it is (+0x2D0) for its update to ease (+0x2D8 set). The frame
// update (0x005D7560) advances the world timer (0x00A35A10), then, when the
// scale is 0 or more, writes it to the world timer's +0x54 and gives it to
// the scene (0x008107F0, slot 6, as a double). The names are ours.
DAO_ADDRESS(kCClientExoApp_SetTimeScale, "CClientExoApp::SetTimeScale", 0x005D81C0)
DAO_ADDRESS(kCClientExoApp_FrameUpdate, "CClientExoApp::FrameUpdate", 0x005D7560)
// The frame's clock (dao/Clock.h): the Eclipse game object, a global the
// update task reads (0x00401F30 returns it), keeps two EC::ITimer at +0xB4
// and +0xB8 (its constructor, 0x00404260). ITimer::Update, __thiscall
// (float fixed frames per second), `ret 4`: the microsecond clock's
// seconds since +0x08 (or one over the fixed rate) into +0x10, added to
// the double at +0x18. EclipseGame's frame step (0x004025C0) updates the
// +0xB4 timer, keeps its seconds at the game's +0xBC, and gives its total
// to the scene (0x008107F0). The game object is kEclipseGame above.
DAO_ADDRESS(kITimer_Update, "EC::ITimer::Update", 0x004CB160)
DAO_ADDRESS(kEclipseGame_FrameStep, "EclipseGame::FrameStep", 0x004025C0)
DAO_ADDRESS(kWorldTimer_Update, "EC::WorldTimer::Update", 0x00A358F0)
DAO_ADDRESS(kWorldTimer_SinceUpdate, "EC::WorldTimer::SinceUpdate", 0x00A35980)
DAO_ADDRESS(kWorldTimer_WorldTime, "EC::WorldTimer::WorldTime", 0x00A35AF0)
DAO_ADDRESS(kObjectTimer_Update, "ObjectTimer::Update", 0x006FA1D0)
DAO_ADDRESS(kCCCreature_Update, "CCCreature::Update", 0x006BE4B0)
DAO_DATA(kClock_Timer, "g_clockTimer", 0x00C2BDF8)
DAO_DATA(kGTimer_Clock, "GTimer::g_clock", 0x00C1DAA8)

// Input, read 2026-09-30 (dev/TODO.md, item 3). The key bindings' actions
// name a message as text per game mode, registered at start-up by
// RegisterInputMessages (one large function), and a key's message is built
// by name and delivered. RawInputMessage's handler in the input mappings
// runs while the controls screen captures a key. The names are ours.
DAO_ADDRESS(kInput_RegisterInputMessages, "Input::RegisterInputMessages", 0x006DC900)
DAO_ADDRESS(kGroupedInputMappings_HandleRawInput, "Eclipse::GroupedInputMappings::HandleRawInput", 0x00894ED0)

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
DAO_ADDRESS(kGameModeExplore_UseAbility, "GameModeExplore::UseAbility", 0x00711680)
DAO_ADDRESS(kGameModeExplore_BeginTargeting, "GameModeExplore::BeginTargeting", 0x0070BA30)
DAO_ADDRESS(kGameModeExplore_AttackTarget, "GameModeExplore::AttackTarget", 0x0070B980)
DAO_ADDRESS(kGameModeExplore_ValidTarget, "GameModeExplore::ValidTarget", 0x0070D550)
DAO_DATA(kSelectedTarget, "g_selectedTarget", 0x00C06794)
// The one setter of the selected target: __thiscall on explore mode (id),
// `ret 4`; it stores the id, tells the target's GUI and moves the
// highlight (dao/GameModes.h, SelectTarget). Read 2026-09-30.
DAO_ADDRESS(kGameModeExplore_SelectTarget, "GameModeExplore::SelectTarget", 0x0070D030)

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
DAO_ADDRESS(kCGameEffect_AddElapsed, "CGameEffect::AddElapsed", 0x0079F2A0)
DAO_ADDRESS(kEffectsHolder_Update, "SEffectsHolder::Update", 0x007A1500)
DAO_ADDRESS(kEffectsHolder_AgeEffect, "SEffectsHolder::AgeEffect", 0x007A0B10)
DAO_ADDRESS(kCGameEffect_Prepare, "CGameEffect::Prepare", 0x0065C760)
// Regeneration, read 2026-09-30 (dao/Clock.h). UpdateRegeneration,
// __thiscall on the creature's stats (ms, bool in combat), `ret 8`, adds
// the milliseconds to the stats' +0x58 and, each 3000 of them out of
// combat or 2000 in combat, has every property regenerate (RegenerateAll,
// 0x00795BA0), the living only. A depletable property's regeneration step
// (CreaturePropertyDepletable's slot 13, 0x0079CAD0) skips if its +0x74
// is set, else adds the total of the property its +0x64 (out of combat) or
// +0x6C (in combat) names: properties.gda's NonCombatEffect and
// CombatEffect. The names are ours.
DAO_ADDRESS(kCCCreatureStats_UpdateRegeneration, "CCCreatureStats::UpdateRegeneration", 0x0071B0E0)
DAO_ADDRESS(kCCCreatureStats_RegenerateAll, "CCCreatureStats::RegenerateAll", 0x00795BA0)
DAO_ADDRESS(kCreaturePropertyDepletable_Regenerate, "CreaturePropertyDepletable::Regenerate", 0x0079CAD0)
// The parameters by index of CDataArrays, the base CGameEffect and
// CScriptEvent share at +0 (the RTTI's base class descriptors), from
// GetEffectInteger and its three siblings (one body, 0x00651270) and
// GetEventInteger and its kin, which call the same four: __thiscall, the
// index, `ret 4`. Each indexes a std::vector the structure points to (+0x4
// ints, +0x8 floats, +0x10 objects, +0x14 strings), bounds-checked; out of
// range, 0, 0.0, -1, and a static empty string. GetString returns a
// pointer to the string.
DAO_ADDRESS(kCDataArrays_GetInteger, "CDataArrays::GetInteger", 0x00A5A220)
DAO_ADDRESS(kCDataArrays_GetFloat, "CDataArrays::GetFloat", 0x00A5A250)
DAO_ADDRESS(kCDataArrays_GetObject, "CDataArrays::GetObject", 0x00A5A2B0)
DAO_ADDRESS(kCDataArrays_GetString, "CDataArrays::GetString", 0x00A5A2E0)
// A fifth list at +0xC, read 2026-09-30 from the script commands that build
// commands (CommandMoveToLocation and its kin), which store their
// booleans in it: the getter has the other four's shape and answers 0 out
// of range. No script command reads it. The name is ours.
DAO_ADDRESS(kCDataArrays_GetFlag, "CDataArrays::GetFlag", 0x00A5A280)
// The setters the same constructors call, __thiscall (index, value),
// `ret 8`: each makes its list if there is none, grows it to the index and
// stores. The vector setter takes a pointer to 16 bytes, three floats and
// padding, copied with movaps; its list at +0x18 is not a std::vector but
// {data, size, capacity} of 16-byte elements (its accessor, 0x005728B0,
// is data + index * 16). The string setter takes a pointer to the engine's
// std::wstring.
DAO_ADDRESS(kCDataArrays_SetInteger, "CDataArrays::SetInteger", 0x00A5B710)
DAO_ADDRESS(kCDataArrays_SetFloat, "CDataArrays::SetFloat", 0x00A5B7D0)
DAO_ADDRESS(kCDataArrays_SetFlag, "CDataArrays::SetFlag", 0x00A5B890)
DAO_ADDRESS(kCDataArrays_SetObject, "CDataArrays::SetObject", 0x00A5B950)
DAO_ADDRESS(kCDataArrays_SetString, "CDataArrays::SetString", 0x00A5BA10)
DAO_ADDRESS(kCDataArrays_SetVector, "CDataArrays::SetVector", 0x00A5A9B0)

// Commands (dao/Objects.h, Command), read 2026-09-30 from the 17 script
// commands that build one and from AddCommand. Each takes a command from
// a pool the AI master keeps (the object at AIMaster+0x6C) with New, which
// numbers it (the counter at the pool's +0x48), makes a CDataArrays for its
// parameters, fills it and sets the type. The copy the VM makes on a push
// or pop (CopyGameDefinedStructure, structure 2) is a new command from the
// pool with the fields copied by CopyFrom, not a shared reference; destroy
// (structure 2) is Release, which puts it back. The names are ours.
DAO_ADDRESS(kCommandPool_New, "CommandPool::New", 0x007468C0)
DAO_ADDRESS(kCommandPool_Release, "CommandPool::Release", 0x0066DE30)
DAO_ADDRESS(kCommand_Constructor, "Command::Command", 0x00741F10)
DAO_ADDRESS(kCommand_CopyFrom, "Command::CopyFrom", 0x00741FC0)
DAO_ADDRESS(kCommand_SetType, "Command::SetType", 0x00742090)

// Script arrays (dao/core/ScriptArray.h), read 2026-09-30. The value is
// the tag and a shared payload; these make, copy, measure and free one,
// __thiscall each. StackPopArray (above) makes the popper a copy with
// operator new and CopyConstructor; StackPushArray copies the one it is
// given, so the caller keeps and frees its own, as GetTeam (0x0066E570)
// does: Destructor, then operator delete. The engine's name for the class
// is not in the exe.
DAO_ADDRESS(kScriptArray_Constructor, "ScriptArray::ScriptArray", 0x00A2E920) // (tag): empty
DAO_ADDRESS(kScriptArray_CopyConstructor, "ScriptArray::ScriptArray_copy", 0x00A2D260)
DAO_ADDRESS(kScriptArray_Destructor, "ScriptArray::~ScriptArray", 0x00A2E830)
DAO_ADDRESS(kScriptArray_Size, "ScriptArray::Size", 0x00A2DB80)
DAO_ADDRESS(kCVirtualMachine_StackPushArray, "CVirtualMachine::StackPushArray", 0x00A2D7C0)
// A resource on the stack is a std::wstring under tag 0x60 (StackPopResource
// copies it out with the string's operator=): this pushes one, as
// StackPushString does a string under tag 5. Read 2026-09-30.
DAO_ADDRESS(kCVirtualMachine_StackPushResource, "CVirtualMachine::StackPushResource", 0x00A2D890)

// The engine's std::wstring, MSVC 2005's (dao/core/Msvc8.h), read
// 2026-09-30 from GetStringLength (0x00657CC0), which makes one on its
// stack, pops into it and destroys it. __thiscall each. The C runtime is
// linked into the exe, so these have no import names, and memory they
// allocate is the engine's heap's: only the engine's destructor frees it.
DAO_ADDRESS(kStdWString_Constructor, "std::wstring::wstring", 0x004C0D90)
DAO_ADDRESS(kStdWString_Destructor, "std::wstring::~wstring", 0x004C0DB0)
DAO_ADDRESS(kStdWString_Assign, "std::wstring::operator=", 0x004C2AB0)   // assign(other, 0, npos)
DAO_ADDRESS(kStdWString_AssignRange, "std::wstring::assign", 0x004C1270) // (other, position, count)
// The allocator every engine allocation above goes through: operator new
// calls the engine's memory manager (0x004B92C0) with a tag of -1.
DAO_ADDRESS(kOperatorNew, "operator_new", 0x00413660)
DAO_ADDRESS(kOperatorDelete, "operator_delete", 0x004136C0)

// The VM's stack as a whole, read 2026-09-30 for calling commands from C++
// (dao/Script.h, Invoke): an object at VM+0x1738 whose first word is the
// number of values on it, types at VM+0x1744 and values at VM+0x1748 (the
// pops index both with it). Truncate, __thiscall on that object with the
// new depth, `ret 4`, destroys every value above the depth and sets it;
// each pop ends with it.
DAO_ADDRESS(kCVirtualMachineStack_Truncate, "CVirtualMachineStack::Truncate", 0x00A2F390)

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
DAO_ADDRESS(kAIMaster_AddEventDeltaTime, "AIMaster::AddEventDeltaTime", 0x0064ACE0)
DAO_ADDRESS(kAIMaster_AddEventNow, "AIMaster::AddEventNow", 0x0064A630)
DAO_ADDRESS(kAIMaster_AddEventLater, "AIMaster::AddEventLater", 0x0064A740)
DAO_ADDRESS(kAIMaster_ProcessEvent, "AIMaster::ProcessEvent", 0x0064A8C0) // (CAIEvent **), __thiscall
DAO_ADDRESS(kAIMaster_ClearEvents, "AIMaster::ClearEvents", 0x0064AC80)
DAO_ADDRESS(kAIMaster_DeleteEventData, "AIMaster::DeleteEventData", 0x0064A520) // (id, data)
DAO_ADDRESS(kCAIEvent_Constructor, "CAIEvent::CAIEvent", 0x0079BDA0)
// Where an object's script meets its event (dao/Events.h): __thiscall
// (CScriptEvent **), `ret 4`. CCObject's handler (slot 50, 0x00698A50,
// shared with the triggers, sounds, waypoints and more; CCCreature's,
// CCPlaceable's, CCItem's and CCAreaOfEffectObject's chain to it) calls it
// for a script event (handler id 10), and 17 other places call it
// straight, delivering at once: a creature's death and update, the command
// queue, effects, Party::Select, the orders. Read 2026-09-30.
DAO_ADDRESS(kCCObject_RunEventScript, "CCObject::RunEventScript", 0x00698960)
DAO_ADDRESS(kCCObject_EventHandler, "CCObject::EventHandler", 0x00698A50)
DAO_ADDRESS(kCCArea_EventHandler, "CCArea::EventHandler", 0x0067F180)

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
DAO_ADDRESS(kConversation_Start, "Conversation::Start", 0x007F2210)
DAO_ADDRESS(kConversation_IsLineAvailable, "Conversation::IsLineAvailable", 0x007F0D30)
DAO_ADDRESS(kConversation_FirstAvailable, "Conversation::FirstAvailable", 0x007F1100)
DAO_ADDRESS(kConversation_PlayLine, "Conversation::PlayLine", 0x007F1EF0)
DAO_ADDRESS(kConversation_OfferReplies, "Conversation::OfferReplies", 0x007F1A40)
DAO_ADDRESS(kConversation_Advance, "Conversation::Advance", 0x007F1910)
DAO_ADDRESS(kConversation_End, "Conversation::End", 0x007F0F50)
DAO_ADDRESS(kGameModeDialog_ShowResponses, "GameModeDialog::ShowResponses", 0x00690B70)
DAO_ADDRESS(kGameModeDialog_AddResponse, "GameModeDialog::AddResponse", 0x00690810)
DAO_ADDRESS(kConversationData_Constructor, "ConversationData::ConversationData", 0x0068FF10)
DAO_ADDRESS(kConversationData_AddResponse, "ConversationData::AddResponse", 0x00690670)
DAO_ADDRESS(kResponse_Constructor, "Response::Response", 0x0068D2A0)
DAO_ADDRESS(kCCModule_EventHandler, "CCModule::EventHandler", 0x005FF850)

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
DAO_DATA(kGdaTables, "g_gdaTables", 0x00C1D090)
DAO_ADDRESS(kGdaTables_GetInt, "GdaTables::GetInt", 0x00A2CCC0)
DAO_ADDRESS(kGdaTables_GetFloat, "GdaTables::GetFloat", 0x00A2CC80)
DAO_ADDRESS(kGdaTables_GetString, "GdaTables::GetString", 0x00A2CC40)
DAO_ADDRESS(kGdaTables_GetBool, "GdaTables::GetBool", 0x00A2CD00)
// The store's lookup of a table by number, __thiscall, `ret 4`, null if it
// has none: what every getter above starts with. A table's row count is
// its slot 5 and its column count its slot 4, no arguments, as
// GetM2DARows and GetM2DAColumns call them (M2DA's +0x44 and +0x40, G2DA's
// +0x30 and +0x2C). The name is ours.
DAO_ADDRESS(kGdaStore_FindTable, "GdaStore::FindTable", 0x004DF580)

// The object array's lookup: NWN's `uint8_t CGameObjectArray::GetGameObject(
// OBJECT_ID, CGameObject**)`, the same shape, called by GetObjectById on the
// application's array at +0xCC.
DAO_ADDRESS(kCGameObjectArray_GetGameObject, "CGameObjectArray::GetGameObject", 0x00A58190)
// SetCombatState's engine side: the script command looks the creature up
// (0x005D58C0) and calls this on it with the state. How a creature enters
// and leaves combat, for a turn-based mode.
DAO_ADDRESS(kCCCreature_SetCombatState, "CCCreature::SetCombatState", 0x006AE0A0)

} // namespace dao::addr
