# The script command table

Dragon Age: Origins scripts (the toolset's `.nss`, compiled to `.ncs`) reach the engine through numbered commands, the way Neverwinter Nights scripts do. The exe registers each one at start-up. One function makes 827 calls, each pushing the command's name, the function that implements it, and its number:

```
push  offset "COMMAND_TOGGLEGAMEPAUSE"
push  offset 0x0066A7A0        ; the implementation, or 0
push  188                      ; the command's number
mov   ecx, esi                 ; the registry (its class not yet known)
call  ScriptCommands::Register ; 0x0064DD40
```

`tools/script_commands.py` walks `.text` for that shape and writes `dev/script_commands.csv`: `id,name,function_va,registered_at_va,script_name,signature`, one row per command. As read on 2026-09-29, 827 commands are registered and 692 have an implementation. The other 135 are registered with a null function. Examples are `COMMAND_ADDSPELL`, `COMMAND_BANPLAYER` and the `COMMAND_COMMAND*` creature-command family. The client exe knows their numbers and names but implements them elsewhere, or not at all.

## Cross-checked against script.ldf

The game ships the toolset compiler's declaration of every engine function: `script.ldf`, in `packages\core\data\scripts.erf`. Each declaration carries its command number:

```
void ToggleGamePause(int bPause = TRUE) = 188;
```

`tools/script_commands.py` reads it straight out of the ERF (`DAO_GAME_DIR` overrides the GOG path) and fills `script_name` and `signature`:

| script.ldf, 2026-09-29 | |
|---|---|
| Declarations | 672 |
| Same name as the registration, same number | 583 |
| Another name, same number (`GetDistanceBetween` = `COMMAND_GETDISTANCEBETWEENOBJECTS`) | 86 |
| Declared, not registered in this exe | 3 (`SetDead` 729, `GetPlayerMapPatch` 651, `GetPlayerMapPin` 654) |
| Registered, not declared | 158, most of them the 135 with no implementation |

So a registration number is a script function number. The table is verified against a second, independent source, not only read off the code. `cscript.ldf` beside it declares a second interface of 160 functions for the client and console scripts (`ConsoleCommand`, `ShutDown`, ...). Their registration is a different shape, not yet read.

## Why this table is the first map of the engine

- **Every function a script can call is named.** `tools/names.py` turns the rows into Ghidra names (`ScriptCommand::TOGGLEGAMEPAUSE` at `0x0066A7A0`), so the decompiler shows the engine's vocabulary instead of `FUN_0066a7a0`.
- **Each implementation is a worked example of the engine's types.** A command reads its arguments off the script VM's stack, calls into the engine, and pushes a result. With the signature beside it, the argument reads are identified before the body is read.
- **The registry is the natural place to add commands.** Registering our own number, name and function through `ScriptCommands::Register`, plus a matching declaration in a `script.ldf` the toolset compiles against, would put a new command in reach of scripts, as NWNX did for Neverwinter Nights. Not yet tried.
