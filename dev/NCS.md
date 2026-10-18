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
- The gap is filled with 0 (inferred): object id 0, not OBJECT_INVALID; null strings.

Read 2026-10-18 by lib-agent-2 for DAOParagonEngine, not seen in play.
