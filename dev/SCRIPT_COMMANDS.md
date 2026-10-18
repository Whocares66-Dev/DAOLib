# The script command table

Dragon Age: Origins scripts (the toolset's `.nss`, compiled to `.ncs`) reach the engine through numbered commands, the way Neverwinter Nights scripts do. The exe registers each one at start-up. One function, `CVirtualMachineCommands::InitializeCommands` (`0x00675210`, slot 1 of the class's vtable, as in NWN), makes 827 calls, each pushing the command's name, the function that implements it, and its number:

```
push  offset "COMMAND_TOGGLEGAMEPAUSE"
push  offset 0x0066A7A0        ; the implementation, or 0
push  188                      ; the command's number
mov   ecx, esi                 ; the CVirtualMachineCommands object
call  CVirtualMachineCommands::Register ; 0x0064DD40, table[id] = function
```

`tools/script_commands.py` walks `.text` for that shape and writes `dev/script_commands.csv`: `id,name,function_va,registered_at_va,script_name,signature`, one row per command. As read on 2026-09-29, 827 commands are registered and 692 have an implementation. The other 135 are registered with a null function. Examples are `COMMAND_ADDSPELL`, `COMMAND_BANPLAYER` and the `COMMAND_COMMAND*` creature-command family. The client exe knows their numbers and names but implements them elsewhere, or not at all.

## Each command's handler, by group

Every live function of `script.ldf`, by its `@addtogroup`: what its handler does, the engine state it touches, and its edge cases, under the ldf's exact name. Read by lib-agent-2 from 2026-10-18 for DAOParagonEngine's baseline (its #27), one file per group; a function a subsystem note already covers points there.

- [Action system](script_commands/actionsys.md)
- [Areas of effect](script_commands/aoe.md)
- [Commands](script_commands/commands.md)
- [Effect access](script_commands/effect_access.md)
- [Events](script_commands/events.md)
- [Inventory](script_commands/inventory.md)
- [Items](script_commands/items.md)
- [Locations](script_commands/locations.md)
- [Party](script_commands/Party.md)
- [Placeables](script_commands/placeables.md)
- [Reaction](script_commands/reaction.md)
- [Spells and combat](script_commands/spells_combat.md)
- [The threat system](script_commands/threatsystem.md)
- [Walk-grid patches](script_commands/waypatches.md)

## Cross-checked against script.ldf

The game ships the toolset compiler's declaration of every engine function: `script.ldf`, in `packages\core\data\scripts.erf`. Each declaration carries its command number:

```
void ToggleGamePause(int bPause = TRUE) = 188;
```

`tools/script_commands.py` reads it straight out of the ERF (`DAO_GAME_DIR` overrides the GOG path) and fills `script_name` and `signature`:

| script.ldf, 2026-09-29 | |
|---|---|
| Declarations | 674 (672 until 2026-10-17, when the pattern learned an array written with a space, `object []`: `GetCreaturesInAOE` 337 and `GetAbilitiesDueToAOEs` 389, DAOLib #167) |
| Same name as the registration, same number | 585 |
| Another name, same number (`GetDistanceBetween` = `COMMAND_GETDISTANCEBETWEENOBJECTS`) | 86 |
| Declared, not registered in this exe | 3 (`SetDead` 729, `GetPlayerMapPatch` 651, `GetPlayerMapPin` 654) |
| Registered, not declared | 156, most of them the 135 with no implementation |

So a registration number is a script function number. The table is verified against a second, independent source, not only read off the code. `cscript.ldf` beside it declares a second interface of 160 functions for the client and console scripts (`ConsoleCommand`, `ShutDown`, ...). Their registration is a different shape, not yet read.

### Commands script.ldf leaves out, read in the exe

Read 2026-10-18 by lib-agent-2, for DAOParagonEngine's script engine; the CSV's signature column is script.ldf's, so these are kept here.

- 396 `COMMAND_ADDTOPARTY`: `void AddToParty(object oCreatureToAdd, object oPlayer)`, commented out in script.ldf (line 7123). It pops two objects, ignores the second, and calls `ForScript_ADDTOPARTY(creature, 1)`.
- 397 `COMMAND_REMOVEFROMPARTY`: `void RemoveFromParty(object oCreature)`, commented out (line 7139); `ForScript_ADDTOPARTY(creature, 0)`.
- 859 `COMMAND_GETABILITYLIST`: not in script.ldf. It pops an object, an int and an int and pushes an `int[]` (tag 0x40) of ability ids: type 0 takes the abilities of types 1 to 3, 1 to 3 one type each, 4 none; a third int not 0 keeps only those still cooling down.
- 10: not registered. `InitializeCommands`' 827 registrations skip it, and `ExecuteCommand` answers -642, which fails the script at that `ACTION`. Five shipped `.ncs` call it.

## Why this table is the first map of the engine

- **Every function a script can call is named.** Each row's function is named in the address library's table (`tools/names.py --give`), and `tools/names.py` gives the names to Ghidra (`ScriptCommand::TOGGLEGAMEPAUSE` at `0x0066A7A0`), so the decompiler shows the engine's vocabulary instead of `FUN_0066a7a0`.
- **Each implementation is a worked example of the engine's types.** A command reads its arguments off the script VM's stack, calls into the engine, and pushes a result. With the signature beside it, the argument reads are identified before the body is read.
- **The registry is the natural place to add commands.** Registering our own number, name and function through `CVirtualMachineCommands::Register`, plus a matching declaration in a `script.ldf` the toolset compiles against, would put a new command in reach of scripts, as NWNX did for Neverwinter Nights. `dao/Script.h` sets a slot of the table directly, which is the same thing after the fact; about 150 of the 863 slots are empty. Not yet tried with a new command.

## The engine behind the commands

`tools/script_backends.py` follows each command's body one step down, into the engine functions it does its work with, and names them after the command. Run 2026-09-30:

| Found | Count | How |
|---|---|---|
| Object lookups by id | 10 | The shape of NWN's `GetCreatureByGameObjectID`: the application's object array (`+0xCC`), `CGameObjectArray::GetGameObject`, a type byte at `+0x14`, and a cast through a virtual slot. A cast slot's class is the one whose implementation of it is `mov eax, ecx; ret`. So `0x005D58C0` is `CClientExoApp::GetCreatureById`, and there are lookups for areas, area-of-effect objects, placeables, modules, projectiles and waypoints, the uncast `GetGameObjectById`, and `GetObjectById` for any object. The area's and the area-of-effect object's were missed until 2026-10-01: their casts are slots 2 and 3, and the disassembler writes an offset below ten without `0x`. |
| Methods of a looked-up object | 38 | A direct call with the object as `this`, when it is the only method the command calls on it: `CCModule::ForScript_DoAreaTransition`, `CCCreature::ForScript_EndHeartbeat`, `CCObject::ForScript_IsDead`. |
| Methods through a vtable | 6 | The same, through the object's vtable. |
| A command's sole callee | 180 | The one direct call left after the VM, the lookups and shared helpers are set aside: `ScriptBackend::ForScript_GetRemainingCooldown`, `ScriptBackend::ForScript_GetLocalEffect`. |
| Candidates, unnamed | 25 | One of several methods a command calls on its object: the command's purpose is shared out among them. |

The tables are `dev/object_lookups.csv` and `dev/script_backends.csv`. A function with a row in `Addresses.h` is left out of the second: it has the engine's own name by then, so the counts fall as the library grows. `tools/names.py --give` writes their names into this checkout's copy of the address library's table and prints a line for each to send to the library's session, which commits them; Ghidra has them from the table, and a name set by hand wins. `ForScript_X` means the engine function behind script command X. It says what the function does for scripts, not the engine's own name for it.

## Fields

Most commands call no engine function at all: they look the object up and read or write its fields inline. `tools/script_fields.py` follows the looked-up object through registers and x87 loads, and records every field read or written through it, and every field whose value the command pushes back to the script. That last gives the field its meaning and its script type. Run 2026-09-30: 61 fields over 7 classes, 18 of them returned to a script (`dev/script_fields.csv`).

| Class | Offset | Width | What, by the command that returns it | Script type |
|---|---|---|---|---|
| `CCCreature` | `+0x790` | 4 | `GetCreatureBodyBag` | object |
| `CCCreature` | `+0x7E8` | 4 | `GetPackage` | int |
| `CCCreature` | `+0x7EC` | 4 | `GetPackageAI` | int |
| `CCCreature` | `+0xBD0` | 4 | `GetCreatureTreasureCategory` | int |
| `CCCreature` | `+0xBD4` | 4 | `GetCombatantType` | int |
| `CCCreature` | `+0xC60`, then `+0x0` | 4 | `GetCombatState` | int |
| `CCObject` | `+0x2B0` | 4 | `GetAILevel` | int |
| `CCObject` | `+0x3C0` | 4 | `GetGroupId` | int |
| `CCObject` | `+0x3C4` | 4 | `GetTeamId`, written by `SetTeamId` | int |
| `CCPlaceable` | `+0x5E0` to `+0x6C0` | 1-4 | pick-lock level, key required, auto-remove key, base type, state, action, treasure rank and category | int |
| `CCWayPoint` | `+0x444` | 4 | `GetMapPinState` | int |

Fields only written are leads too. `CCCreature+0xC60` points to the creature's combat data: combat state at `+0x0`, and at `+0x60` what `SetAttackDuration` and `SetAimLoopDuration` write, the attack timing a turn-based mode will want. `CCObject` has `+0x2BC` destroyable, `+0x2C6` always visible, `+0x3D4` the command result `SetCommandResult` writes, and `+0x421` active.

A field read is a byte range the engine reads, not yet a type: a pointer followed further is marked by its path (`+0xc60 +0x60`), and the width is what the instruction read. Each one still wants the probe's eyes in play before a class header relies on it.
