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
// handler, one of the functions registered.
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

// Creature actions (dev/TURN_BASED.md). Read 2026-09-30 from AddCommand's
// implementation after the probe showed it running about five times a
// second in combat; not yet verified in play.
//
// An object by its id, on the application: the script command AddCommand
// pops an object id and passes it here, `mov ecx, [g_clientExoApp]`, one
// argument, and gets the object back in eax.
DAO_ADDRESS(kCClientExoApp_GetObject, "CClientExoApp::GetObjectById", 0x005D5BE0)
// Queue a command on an object: __thiscall(command, addToFront, override),
// `ret 0xC`, returns 1. Seen in play 2026-09-30 (dev/TURN_BASED.md): about
// 200 commands in a fight, all on CCCreature, from the scripts'
// AddCommand, from FUN_006A21C0 (the player's direct control, "drive"
// commands) and from FUN_0073D000 (the engine's own, effects among them).
// The command's first word is a global sequence number in its low 16 bits
// and the command type in bits 16-23: the script's COMMAND_TYPE_* numbers.
// addToFront is AddCommand's bAddToFront; override its nOverrideAddBehavior,
// -1 by default.
DAO_ADDRESS(kObject_QueueCommand, "GameObject::QueueCommand", 0x0073CD10)

// The game's update, as a task on the engine's worker threads: from the
// deliberate crash of 2026-09-30, whose stack ran CClientExoApp's start-up
// through the message dispatch down to Eclipse::UpdateTask's slot 3, which
// calls the per-frame update at 0x00412EB0, on a Multithreading::Thread.
// The probe counts a stack sample as game logic when this function's frame
// is on it; the range is Ghidra's.
DAO_ADDRESS(kEclipseUpdateTask_Execute, "Eclipse::UpdateTask::vfunction3", 0x00413310)
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
// engine structure, object -- pops and pushes each together. Not yet
// verified in play.
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
// The object array's lookup: NWN's `uint8_t CGameObjectArray::GetGameObject(
// OBJECT_ID, CGameObject**)`, the same shape, called by GetObjectById on the
// application's array at +0xCC.
DAO_ADDRESS(kCGameObjectArray_GetGameObject, "CGameObjectArray::GetGameObject", 0x00A58190)
// SetCombatState's engine side: the script command looks the creature up
// (0x005D58C0) and calls this on it with the state. How a creature enters
// and leaves combat, for a turn-based mode.
DAO_ADDRESS(kCCCreature_SetCombatState, "CCCreature::SetCombatState", 0x006AE0A0)

} // namespace dao::addr
