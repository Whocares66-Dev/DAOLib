# The compiled scripts (.ncs) as the exe runs them

A note from lib-agent-2, 2026-10-17, for lib-agent to keep, move or fold into a core reader (`dev/PARAGON_NCS.md`, DAOParagonEngine #8). Read in the GOG exe: the loader's header check, `CVirtualMachine`'s execute loop and its stack functions; the names of DA:O's own opcodes come from the toolset compiler's disassembler strings (`toolset\extract\tools\Engine.dll`, around file offset 0x7AB000). Checked against the files: all 2,764 `.ncs` the game takes (ghilana, every source but the enciphered add-ins) decode with the layouts below to their last byte, none left over. The scratch decoder that did it is `%TEMP%\lib-agent-2-ncsdec.py`.

## The file

- The loader (`0x00a2cf10`) wants at least 13 bytes, `NCS V`, a digit, `.`, a digit. It makes the version (major × 10 + minor); for any version other than 1.0 byte 8 must be `B`, for 1.0 it is not looked at. The big-endian size at bytes 9 to 12 is not read.
- The code starts at byte 13, and the program counter starts there (the loader sets the counter's field to 13): an instruction's offset is its offset in the file. `JMP`, `JSR`, `JZ`, `JNZ` and `STORE_STATE` count from the instruction's own offset, so either origin decodes the same.
- Every operand is big-endian.

## The loop

`0x00a30230`: each turn reads the opcode byte and the type byte at the counter, counts the instruction (the run fails at 0x20000 instructions), and switches on opcode − 1 through a table of 54 entries at `0x00a31c24` (opcodes 1 to 0x36; anything else fails with -640). NWN's 45 opcodes keep their numbers; DA:O adds 0x2E to 0x36.

The stack is of 4-byte cells: the operands' offsets and sizes are in bytes and the loop divides them by 4. Each cell has a type tag beside its value (the VM's +0x1744 tags, +0x1748 values); SP and BP are cell counts (+0x1738, +0x173C). Strings, resources, engine structures and arrays are one cell each, holding a pointer; a vector is three float cells.

## Type bytes

| Type | Meaning | Where |
|---|---|---|
| 0x00 | none | `ACTION`, the jumps, `MOVSP`, `RETN`, `SAVEBP`, `RESTOREBP`, `NOP`, and DA:O's 0x2E to 0x36 |
| 0x01 | stack | `CPDOWNSP`, `CPTOPSP`, `CPDOWNBP`, `CPTOPBP`, `DESTRUCT`, and 0x30 to 0x33 |
| 0x03 | int | also the tag of an int cell |
| 0x04 | float | |
| 0x05 | string | |
| 0x06 | object | |
| 0x10 + n | engine structure n, as `script.ldf` numbers them: 0x10 event, 0x11 location, 0x12 command, 0x13 effect, 0x14 itemproperty, 0x15 player. `RSADD` accepts 0x10 to 0x19 | |
| 0x20 | int, int | |
| 0x21 | float, float | |
| 0x22 | object, object | |
| 0x23 | string, string | |
| 0x24 | structure, structure (EQUAL, NEQUAL: a u16 size follows) | not in the shipped files |
| 0x25 | int, float | |
| 0x26 | float, int | |
| 0x30 + n | engine structure n, both sides (EQUAL, NEQUAL) | not in the shipped files |
| 0x3A | vector, vector | |
| 0x3B | vector, float | |
| 0x3C | float, vector | |
| 0x40 to 0x44 | an array of int, float, string, object, resource (`dao/core/ScriptArray.h`) | `RSADD` |
| 0x50 + n | an array of engine structure n | `RSADD` (0x51 location[] and 0x53 effect[] in the files) |
| 0x60 | resource (a resref, held as a string) | `CONST`, `RSADD`, `EQUAL` compares two like strings |

`RSADD` lets 0x40 to 0x59 through whole (0x45 to 0x4F too); the array's constructor (`ScriptArray`) is what knows the groups. An array tag maps to its element type at `0x00a2d010` (0x40 → 3, 0x41 → 4, 0x42 → 5, ...).

## Opcodes

Length is the whole instruction, opcode and type byte included. Handler addresses are in GOG's exe.

| Op | Name | Length | Operands after the type byte | Handler | Notes |
|---|---|---|---|---|---|
| 0x01 | CPDOWNSP | 8 | i32 offset, u16 size | `0x00a3057b` | |
| 0x02 | RSADD | 2 | | `0x00a30700` | types 3, 4, 5, 6, 0x60, 0x10–0x19 (a new structure), 0x40–0x59 (an empty array, `0x00a2f140`) |
| 0x03 | CPTOPSP | 8 | i32 offset, u16 size | `0x00a307fd` | |
| 0x04 | CONST | 6, or 4 + n | type 3: i32; 4: f32; 6: i32 object (0 is OBJECT_SELF, read from the run's context, 1 is OBJECT_INVALID); 5 and 0x60: u16 n, then n bytes | `0x00a302ee` | strings are bytes widened one to one into the VM's wide string (all 290,285 in the files are ASCII) |
| 0x05 | ACTION | 5 | u16 command, u8 argument count | `0x00a30962` | calls the implementer's `ExecuteCommand` (slot 2) |
| 0x06–0x0A | LOGAND, LOGOR, INCOR, EXCOR, BOOLAND | 2 | | | type 0x20 |
| 0x0B, 0x0C | EQUAL, NEQUAL | 2, or 4 for 0x24 (u16 size) | | `0x00a30a85` | 0x20–0x24, 0x30–0x39; strings and resources by text, structures through the implementer's slot 7 |
| 0x0D–0x10 | GEQ, GT, LT, LEQ | 2 | | `0x00a310a7` | 0x20, 0x21 |
| 0x11–0x13 | SHLEFT, SHRIGHT, USHRIGHT | 2 | | | 0x20 |
| 0x14 | ADD | 2 | | `0x00a30c6a` | 0x20, 0x21, 0x23 (concatenation), 0x25, 0x26, 0x3A |
| 0x15–0x17 | SUB, MUL, DIV | 2 | | `0x00a3132e` | 0x20, 0x21, 0x25, 0x26, 0x3A, 0x3B, 0x3C |
| 0x18 | MOD | 2 | | | 0x20 |
| 0x19 | NEG | 2 | | | 3, 4 |
| 0x1A | COMP | 2 | | | 3 |
| 0x1B | MOVSP | 6 | i32 offset | `0x00a30531` | |
| 0x1C | STORE_STATEALL | 2 | | `0x00a317c2` | the type byte is the resume point's distance from this instruction |
| 0x1D | JMP | 6 | i32 relative | `0x00a309f5` | |
| 0x1E | JSR | 6 | i32 relative | `0x00a30a3d` | pushes the return address (+6) on the return stack |
| 0x1F | JZ | 6 | i32 relative | `0x00a309d0` | pops an int |
| 0x20 | RETN | 2 | | `0x00a30a23` | |
| 0x21 | DESTRUCT | 8 | u16 size to remove, i16 offset of the element to keep, u16 its size | `0x00a3186e` | |
| 0x22 | NOTI | 2 | | | 3 |
| 0x23, 0x24 | DECISP, INCISP | 6 | i32 offset | `0x00a31950` | type 3 |
| 0x25 | JNZ | 6 | i32 relative | `0x00a31844` | pops an int |
| 0x26 | CPDOWNBP | 8 | i32 offset, u16 size | `0x00a3057b` | the CPDOWNSP handler, from BP |
| 0x27 | CPTOPBP | 8 | i32 offset, u16 size | `0x00a307fd` | the CPTOPSP handler, from BP |
| 0x28, 0x29 | DECIBP, INCIBP | 6 | i32 offset | `0x00a31950` | |
| 0x2A | SAVEBP | 2 | | `0x00a30f5a` | |
| 0x2B | RESTOREBP | 2 | | `0x00a30f7c` | |
| 0x2C | STORE_STATE | 10 | i32, i32 (two stack sizes, kept at the VM's +0x1964 and +0x1960) | `0x00a317e2` | the type byte is the resume point's distance (0x0E in the files: the code after the next `RETN`) |
| 0x2D | NOP | 2 | | `0x00a307f5` | |
| 0x2E | WAITFOR | 2 | | `0x00a319e6` | DA:O. Pops a float (seconds) and the state the `STORE_STATE` before it saved, and hands both to the implementer's slot 11. The server's implementer returns 0 there (`0x00a21f90`) and the state is freed; the client's (`CClientVirtualMachineCommands`, `0x006d3b70`) schedules it (`0x0064a230`, kind 0x1C). Twice in the files: `combateveryoneimmortal.ncs`, `CONST 30.0; STORE_STATE; WAITFOR; RETN` |
| 0x2F | WAITUNTIL | 2 | | `0x00a31a63` | DA:O. Pops an int, adds the implementer's slot 13 (500 in both), pushes it, pops a saved state, asks slot 12 with both (client `0x006d3bd0`), then slot 16; pushes 0 or 1. Not in the files |
| 0x30 | CPSPTOAL | 8 | i32 offset, u16 size | `0x00a30638` | DA:O. Pops an int index (the top cell must be an int); stores the value of the given size at the stack's top into element *index* of the array at SP + offset (`0x00a2ec20`); the offset is counted from SP before the index is popped, so it counts the index's cell (`ability_core.ncs` at 4614 to 4693 confirms it) |
| 0x31 | CPBPTOAL | 8 | i32 offset, u16 size | `0x00a30638` | the same, the array at BP + offset. Not in the files |
| 0x32 | CPALTOPSP | 8 | i32 offset, u16 size | `0x00a308a5` | DA:O. Pops an int index; pushes element *index* of the array at SP + offset (`0x00a2f840`); the offset is counted from SP before the index is popped, so it counts the index's cell (`ability_core.ncs` at 4614 to 4693 confirms it) |
| 0x33 | CPALTOPBP | 8 | i32 offset, u16 size | `0x00a308a5` | the same from BP. Not in the files |
| 0x34 | STORE_FULL_STATE | 2 | | `0x00a317c2` | DA:O; the `STORE_STATEALL` handler. Not in the files |
| 0x35 | ABORTSCRIPT | 2 | | `0x00a31bf3` | DA:O. Ends the run with success (0). Not in the files |
| 0x36 | STOREWAITUNTILTIMER | 2 | | `0x00a31b30` | DA:O. Pops an int and a float (seconds), hands the float × 1000 as milliseconds to the implementer's slot 15 (client `0x00423ee0`), pushes the int back. Not in the files |

The names of 0x2E, 0x2F, 0x34, 0x35 and 0x36 are the compiler's strings (`WAITFOR`, `WAITUNTIL`, `STORE_FULL_STATE`, `ABORTSCRIPT`, `STOREWAITUNTILTIMER`, and its sequence `STOREWAITUNTILTIMER`, `WAITUNTIL`, `JNZ PAST_ABORTSCRIPT`, `ABORTSCRIPT`) matched to the handlers by what each does; only WAITFOR is seen in a shipped file. 0x30 to 0x33 are certain by their handlers and the compiler's `CPSPTOAL`, `CPBPTOAL`, `CPALTOPSP`, `CPALTOPBP`.

**BP** (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C unless marked]:

- **SAVEBP** (0x00A30F5A) [C]: it reads BP ([vm+0x173C]) and SP ([vm+0x1738], a cell count), pushes the old BP (0x00A2D650), then sets BP to the SP read **before** that push. So BP is the index of the saved-BP cell, and the cells below it are what was on the stack: the globals, in a script's global block.
- **RESTOREBP** (0x00A30F7C) [C] pops an int (0x00A2F3C0) into BP.
- **The BP-relative ops** [C for CPTOPBP, from the draft for the others]:
  - CPTOPBP (0x27) shares CPTOPSP's handler 0x00A307FD. Its cell index is `base − (−offset >> 2)`, with base = BP for 0x27 and SP for 0x03 (0x00A30837–0x00A30854). So `CPTOPBP −4` reads cell BP − 1, the last global, not the saved BP.
  - CPDOWNBP (0x00A30620), INCIBP and DECIBP (0x00A3198E), and CPBPTOAL and CPALTOPBP (0x00A3067F, 0x00A308EC) take the same base, [I: from paragon-agent-2's reading, not re-read here].
- **STORE_STATE's two sizes** (+0x1964 the globals', +0x1960 the locals') go with the stack through `CVirtualMachine::StackPopCommand` 0x00A2FD10 to 0x00A2FB90.
  - It divides both by 4 (0x00A2FBA9, 0x00A2FBAC). When both are 0 it takes the whole stack: BP's size from the stack's +4, and SP − BP for the rest (0x00A2FBBF–0x00A2FBCB) [C].
  - It copies n = the globals' size cells from below the old BP (the saved BP not among them), then the locals' cells from below SP, and makes the new stack's BP = n [I for the copy and the new BP: 0x00A2FB90 was read only to 0x00A2FBDE].
- **Why it matters:** with BP one cell high, every BP-relative read is off by one, reading the global above the one meant, or the saved BP for −4. Ten shipped scripts use BP-relative ops: arl101ev/tr_fire_trap, auto_addition, cir000ip_fade_fire_base, den600pt_bigfight, intfloatconversions, mgb_test1, monster_talent_web, zz_bdnpt_debug and zz_crafting (a census of the shipped scripts).

## What the shipped files use

Of the 54 opcodes, 48 pairs of opcode and type appear; none of 0x2F, 0x31, 0x33, 0x34, 0x35, 0x36, `EQUAL`/`NEQUAL` 0x24 or 0x30–0x39, `SHRIGHT`, `USHRIGHT`, `STORE_STATEALL`, or engine structures 0x14 and 0x15. The most common: `CPTOPSP` (1.2 million), `MOVSP`, `CONST` int, `ACTION`, `CPDOWNSP`.

## How a run ends

The loop returns 0 for success (`ABORTSCRIPT`, or the program counter set to -1). Its failures, as the handlers return them: -639 a stack check or pop failed, -642 a division or modulo by zero (int or float), -640 an opcode out of range, -644 a type byte a handler does not take, -646 the counter past the code's end, -632 the 0x20000-instruction limit, -638 a push failed, -649 an array copy failed, -647 no command implementer, -648 an engine structure number out of range.

## A script array read or written past its end

**Reading** (`CPALTOPSP`/`CPALTOPBP`, 0x32/0x33; the handler 0x00A2F840, from the dispatch at 0x00A308A5):
- The destination cell is pushed first, set to the element type's default: int 0, float 0.0, object `0xFFFFFFFF` (OBJECT_INVALID), and a null pointer for strings, resources and structures.
- **An index above 1024, or negative** (`cmp edi, 0x400; ja`, unsigned, 0x00A2F8C8) fails the script with **-649** (0x00A31BC1) and ends the run.
- **An index from 0 to 1024 that is past the end, element 0 of an empty array included,** is no error: the run goes on with the default (each type's `cmp edi, size; jae`, e.g. 0x00A2F913). A string or resource reads as an empty one (made from L""); an engine structure as a null pointer.
- A missing array gives the same default.

**Writing** (`CPSPTOAL`/`CPBPTOAL`, 0x30/0x31; 0x00A2EC20):
- The same 1024 cap (0x00A2EC7A) fails the script with -649.
- An index past the end **grows the array** to index + 1 and stores the element.
- The gap is filled with 0 (read 2026-10-22, below): object id 0, not OBJECT_INVALID; null strings.
  - [C] (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) The array store function 0x00A2EC20 (behind CPSPTOAL): it returns 0 (a failed run) for an index above 0x400 (1024) and for a store whose element type differs from the array's (the tag mapped by 0x00A2D010). With a valid index it grows the element vector when its size is not above the index, by the resize helper with size index + 1, then writes the element.
  - [C] For the string (5), object (6), resource (0x60) and engine-structure (0x10-0x19) arrays the helper is 0x0063D330, which is 0x005D26E0(n, 0): a std::vector resize with the fill value 0. So the gap elements of an object array are 0 (object id 0), and those of the string, resource and structure arrays are 0 (a null pointer, no string object), not OBJECT_INVALID.
  - [C] The element written for a string, resource or structure array is a new copy (0x004C2510, 0x0069BBD0, the structure manager's slot 0x20); a non-null old element is destroyed first.
  - [I] The int (3) and float (4) arrays use other resize helpers (0x005AE960 and 0x00534B10), not read; by their callers' shape they are the same resize with a 0 fill.

Read 2026-10-18 by lib-agent-2 for DAOParagonEngine, not seen in play.

## A script named with ".nss": the name stripped, `<stem>.ncs` loaded

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm` and byte scans of `.text`, for DAOParagonEngine. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not.

- **The compiled script keeps the extension.** `sp_module_item_acq.ncs` (extracted with ghilana) holds the resource constants as the text `preev_item_acquired.nss`, `arlev_item_acquired.nss`, …; `arl101cr_undead.ncs` holds `cli000cr_army_ds.ncs`. A resource is a string cell in the VM (above). (checked)
- **`HandleEvent` (121, handler 0x006586D0) cuts the name at its first '.':** it pops the event, then the resource (`StackPopResource` 0x00A2F5F0, which does no parsing), copies it, takes `std::wstring::find(L'.', 0)` (0x004C1000, at 0x0065878A) and keeps the part before it (0x004C30F0, left(n), with npos keeping the whole). An empty name then takes the object's default script (CCObject +0x26C, area +0x38C, module +0x1F8); else the stem is passed on: CCObject 0x006985F0, area 0x0067BFB0, module 0x005FE840. (checked) Multi-dot names keep only the part before the first dot (checked from the code).
- **The VM then appends ".ncs" itself.** `CVirtualMachine::RunScript` (0x00A31E70, ID 32573; corrected 2026-10-23 by lib-agent-2: it was named ExecuteScript, which is the script command 0x00653DE0 that calls it; called from 0x006985F0 with (name, self, 1, event type)) loads through 0x00A2DC10, which formats `"%ls%ls"` (0x00B0675C) of the name and the VM's +4 wstring, and calls `Resources::Find(out, name, 3)` (0x004D5BA0). The VM's three strings are set by its constructor 0x00A2FF60, called at 0x005DBDA7 with **".ncs"** (+4), ".nss" (+0x20) and ".ndb" (+0x3C) (strings at 0x00B00A38/44/50). A miss returns -634 (0xFFFFFD86) and the script does not run; past 10 nested runs, -633 (0xFFFFFD87) (checked; both codes exist only inside the loader 0x00A2DC10, see below). 
- **What RunScript returns and its depth** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
  - **The name:** 0x00A31E70 is `CVirtualMachine::RunScript`, not `ExecuteScript`. ExecuteScript is the script command 0x00653DE0, which calls it.
  - **RunScript returns 0 on every path** [C], a miss and the depth failure included (`jne 0x00A32087` is `xor eax, eax`).
    - The codes −634 (not found) and −633 (too deep) exist only inside the loader 0x00A2DC10. No caller of RunScript sees them.
    - Neither failure writes a log line: only strings are built and freed.
  - **An empty name, or a `Resources::Find` miss, runs nothing** [C].
    - Find's type argument is the dword at 0x00AF046C, which holds 3, as the note says [C].
  - **The depth:** the nesting field is +0x64, starting at −1; the loader adds 1 and refuses at 10 (`cmp eax, 0xA; jl`, 0x00A2DC53). So 10 nested runs are allowed (levels 0–9) and the 11th fails inside the loader (corrected 2026-10-23 by lib-agent-2: it is not 100; 0x64 is hexadecimal).
- **So the mapping is "by stem", and only where a command strips it.** The commands that strip at the first '.' (every `find(L'.')` call in `.text` scanned): **HandleEvent (121)**, **SetEventScript (122**, 0x00658B55), BeginConversation (440, 0x0065DA64) and BeginSlideshow (632, 0x0065DCB5). The resource type never comes from the name's extension: the VM always asks for `<stem>.ncs`. (checked)
- **Where nothing strips, an extension fails:** `Deprecated_ExecuteScript` (8, 0x00653DE0) passes its string straight to `RunScript`, so `"x.nss"` becomes `"x.nss.ncs"`, which no source holds (inferred: the resource manager finds files by their full name, `dev/FILES.md`); `HandleEvent_String` (133) and `EffectAreaOfEffect`'s rScript (634) do not appear among the strippers (not read further). Scripts use extensions only with HandleEvent (`sp_module_item_acq`, `arl000tr_talk`, `arl101cr_undead`, constants in `bdc/bdn/orz/epi/proving` headers); no shipped script passes an extension to SetEventScript, EffectAreaOfEffect or Deprecated_ExecuteScript (grepped).
- **Where the module passes `R"preev_item_acquired.nss"`:** `_Main_Plot/sp_module_item_acq.nss` picks `RESOURCE_SCRIPT_<PREFIX>_ITEM_ACQUIRED` by the item tag's prefix (line 506 on) and calls `HandleEvent(evEvent, rScript)` (line 528); it is included by `approval_h`, `global_objects_h` and `genpt_morrigan_main` (grepped).
- **For a port:** strip at the first '.' in HandleEvent/SetEventScript (and the conversation/slideshow commands), then look up `<stem>.ncs`; do not derive the type from the extension.

- **`EffectAreaOfEffect` removes the first ".ncs" from its script name** (`dev/script_commands/effect_access.md`; read 2026-10-18 by lib-agent-2).

Not read: how `Resources::Find` treats a name with two extensions (`x.nss.ncs`), beyond finding nothing; the case-sensitivity of resource names; `HandleEvent_String` (133); the loaders' own dot searches at 0x005DE1C1, 0x006033A4, 0x0068F53B, 0x006A153D/0x006A15DC, 0x00747BD7.
