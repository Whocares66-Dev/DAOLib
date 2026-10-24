# Script commands, group "actionsys": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts, all checked:
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`, on `CVirtualMachineCommands`; none of these is shared with another command. It returns 0, -639 (0xFFFFFD81) on a failed pop, -638 (0xFFFFFD82) on a failed push (`dev/script_commands/commands.md`). The VM is `g_pVirtualMachine` (0x00C1D080); pops are `StackPopInteger` 0x00A2F3C0, `StackPopString` 0x00A2F4C0 (a `resource` is popped as a string), `StackPopObject` 0x00A2F6D0; pushes `StackPushInteger` 0x00A2D650, `StackPushString` 0x00A2D730, `StackPushResource` 0x00A2D890.
- **The data tables** are `g_gdaTables` (0x00C1D090), its store at +4; `GdaStore::FindTable` (0x004DF580, by number, null if none) and 0x004E0590 (by name: the name trimmed of " \t" (0x00ADEE0C), lowercased, CRC-hashed, looked up in the map at +0x14). A table's vtable (M2DA 0x00AF15EC, G2DA 0x00AF164C) has slot 8 (+0x20) the string getter and slot 10 (+0x28) the int getter, each `(rowId, columnHash, out)`: **the row is the row's ID, not its index**, whatever script.ldf says (`src/dao/Gda.h`, `src/dao/Addresses.h` "The game's data tables"). The column hash is `core::GdaColumnHash` (CRC32 of the lowercased name as UTF-16LE).
- **The by-name fallback** (`s2DA`) is used only when `n2DA` is -1; any other number goes to `FindTable` and the name is ignored (checked in 13 and 856).
- **The running script's name** is kept per run level: the VM's level is +0x64, and each level's record is 0x240 bytes from +0x288, its first field a `std::wstring` written by the script load (0x00A2DC10 at 0x00A2DD99) from the name `RunScript` was given; the file opened is `"%ls%ls"` (0x00B0675C) of that name and the VM's +8 string (inferred: the extension). (checked apart from the extension)

### Deprecated_ExecuteScript (8)
- **Handler:** 0x00653DE0. It calls `CClientExoApp::GetGameObjectById` (0x005D5840) and `CVirtualMachine::RunScript` (0x00A31E70). (checked)
- **Does:** pops rScript, then oTarget. If oTarget is a game object of any kind, `RunScript(rScript, oTarget, 1, -1)`; otherwise `RunScript(rScript, OBJECT_INVALID, 0, -1)`. The script runs **at once, nested**, at the next run level, before the caller's next instruction; its OBJECT_SELF is oTarget (the third argument is stored as the level's "OBJECT_SELF valid" at VM+0x16E8, the object at +0x1710, 0x00A31F35). (checked)
- **State:** the VM's run level +0x64 and the per-level records above. Nothing is queued. (checked)
- **Edge cases:** script.ldf says the call "will fail" for an object that does not exist; the engine **still runs the script**, with OBJECT_SELF invalid. An empty name returns from `RunScript` at once (length +0x14 = 0, 0x00A31EA8); a script that fails to load returns early too (0x00A31EEE, inferred: the load's error). `RunScript`'s result is ignored; the handler returns 0 unless a pop fails. (checked)

### GetM2DAResource (13)
- **Handler:** 0x00654150. It calls `FindTable` or 0x004E0590, then the table's string getter (slot 8, +0x20). (checked)
- **Does:** pops n2DA, sColumn, nRow, and s2DA when paramCount > 3. The column is trimmed of " \t", lowercased (0x004C3970) and hashed (0x004C1150), then the cell is read as text and **pushed as a resource** (`StackPushResource`). It is `GetM2DAString` (9, 0x00653EF0, the same body) with a resource push. (checked)
- **State:** the loaded tables; nothing written. (checked)
- **Edge cases:** no such table, row ID or column pushes the empty resource "" (the out string starts empty and the getter is skipped or leaves it). nRow is a **row ID**, not the "0 based index" script.ldf says. s2DA is read only for n2DA = -1. A failed push returns -638. (checked; the getter leaving the out empty on a miss is inferred)

### GetHashedM2DAInt (841)
- **Handler:** 0x00654700. It calls `FindTable` and the table's int getter (slot 10, +0x28). (checked)
- **Does:** pops n2DA, nColumnHash, nRow; pushes `table->GetInt(nRow, nColumnHash)`. The column is given as its hash (`core::GdaColumnHash`), so no string work. (checked)
- **State:** the loaded tables. (checked)
- **Edge cases:** the out starts at **0**, so no table, row or column gives 0, as script.ldf says. n2DA = -1 is not a by-name lookup here (there is no s2DA): it skips the lookup and pushes 0. nRow is a row ID (as GetM2DAInt), not an index. A failed push returns -638 (`neg/sbb/and 0x27E/add -638`). (checked)

### GetM2DARowIdFromRowIndex (856)
- **Handler:** 0x006543B0. It calls `FindTable` or 0x004E0590, then the table's slot 18 (+0x48, 0x004ECD50 for both M2DA and G2DA). (checked)
- **Does:** pops n2DA, nRowIndex, and s2DA when paramCount > 2. Slot 18 is `(rowIndex, columnHash, out)`: it finds the column's index by hash (slot 6) and reads the cell **by row index** (slot 14; M2DA 0x004EC420). The handler always passes the hash **0x66FBC936**, which is `GdaColumnHash("ID")`, so it answers the ID column of the row at that position. (checked)
- **State:** the loaded tables. (checked)
- **Edge cases:** the out starts at **-1**: no table, no ID column (slot 6 answering -1, 0x004ECD61) or an index ≥ the row count (M2DA, signed `jge` at 0x004EC455) pushes -1. A negative index passes that signed test and fails the unsigned one, which reads a static empty row (0x00C30240) (checked); that it then answers -1 is inferred. A failed push returns -638. (checked)

### GetTlkTableString (14)
- **Handler:** 0x00654B50. It calls `CTlkTable::GetString` (0x004E4990) on `g_tlkTable` (0x00C1D08C), then `std::wstring::operator=`. (checked)
- **Does:** pops nStrRef, then nGender (always: the compiler fills the default). nGender is kept only if it is 0 or 1 (the bytes at 0x00AF1110/0x00AF1111), **otherwise it becomes 0**; it is written to the table's +0x1C and **left there**. `GetString(strref, out, 0)` walks the talk tables in the vector at +0x28 (12-byte entries), first hit wins, and the text is pushed. (checked)
- **State:** the global talk table's gender byte +0x1C is changed for every later lookup (`src/dao/Tlk.cpp` puts it back for its own calls). (checked)
- **Edge cases:** an unknown strref pushes "" (GetString writes the out only on a hit, 0x004E4A00). script.ldf's `GENDER_MALE` is 1 and `GENDER_FEMALE` 2, so the default writes 1 and `GENDER_FEMALE` writes **0**. `src/dao/Tlk.cpp` reads 1 as the female form, which would make the scripts' constants backwards; `GetString`'s walk itself does not read +0x1C, so where the byte picks a form is not read (inferred, not settled). No null check on `g_tlkTable` (it would fault before the talk tables exist). (checked apart from the gender's meaning)

### GetGameDifficulty (15)
- **Handler:** 0x00654CB0. It calls `SmartData::Attribute::Get` (0x0053CC40, `mov eax,[ecx+0xC]`). (checked)
- **Does:** pushes the attribute at `ClientOptions` (`[0x00C32938]`) **+0x68**, so the int at +0x74. (checked) That it is the options' `DifficultyLevel` is inferred from the command and the option's name (0x00B01458).
- **State:** the client options; the same value the options screen's Difficulty picker sets (`dev/GUI.md`, inferred). (checked for the read)
- **Edge cases:** no range check: whatever is stored is pushed. The scripts compare it against 0..3 (`GetGameDifficulty() == 3` for Nightmare; `achievement_core_h` defines only CASUAL 0, NORMAL 1, HARD 2). No null check on the options pointer. A failed push returns -638. (checked)

### GetGraphicsDetailLevel (851)
- **Handler:** 0x00654CE0. Same as 15 with the attribute at `ClientOptions` **+0x4F0** (the int at +0x4FC). (checked)
- **Does:** pushes that value; the option's name `GraphicsDetailLevel` is read to be this attribute [C, read 2026-10-23 by lib-agent-2]: the options' reader and writer load +0x4F0 at 0x005EBDEC and 0x005EBE33, beside the string (0x005EBDF7, 0x005EBE5A). Its one script reader is `effect_dot2_h.nss`:154 (_EffectDOT), which adds the DOT's crust vfx (fire 10, electricity 1005, cold 1008) only when the level is above 0.
- **Edge cases:** no `GRAPHICS_DETAIL_LEVEL_*` constants exist in script.ldf or the scripts despite its doc comment; no range check. A failed push returns -638. (checked)

### DebugSpawnScriptDebugger (16)
- **Handler:** 0x00654D10. It calls `CClientExoApp::GetModule` (0x005D4FC0) and `ScriptBackend::ForScript_DebugSpawnScriptDebugger` (0x00A2D230). (checked)
- **Does:** only if there is a module and its byte **+0x194** is set, it sets the VM's +0x196C to 1 (the debugger requested; what reads it not followed). Otherwise nothing. (checked)
- **Edge cases:** no parameters; void; always returns 0. What the module's +0x194 is (inferred: a debug or toolset-run flag) and whether it is ever set in a shipped game is not read. (checked)

### DEBUG_ConsoleCommand (704)
- **Handler:** 0x0064E990. It calls 0x008445D0 (the console's singleton at 0x00C39B20, built on first use by 0x00843F90; vtable 0x00B2B174, no RTTI) and its slot 4 (0x008425C0). (checked)
- **Does:** pops sArg, calls `console->Execute(sArg, 0)` and pushes the string it returns. Slot 4 (checked in part): an empty line answers "" (the static result 0x00C39C78); otherwise the line is split at the first whitespace (0x00AF029C), the command word lowercased and looked up in the console's map at +4 (0x006003E0). An unknown command with the flag 0 answers `ERROR: Cannot execute function "…"` (pieces 0x00B2B128, 0x00B2B16C; the exact text built inferred). What a known command does is not read.
- **State:** the console's static result string, overwritten per call. (checked)
- **Edge cases:** the debug scripts use it for "textfield", "textbutton … runscript …", "ButtonGroup", "removebuttongroup"; whether those commands are registered in the shipped exe is not read. A failed push returns -638. (checked)

### GetCurrentScriptName (145)
- **Handler:** 0x00654D40. (checked)
- **Does:** pushes the `std::wstring` at VM + 0x288 + level × 0x240 (level = VM+0x64): the name the running script was started with, as `RunScript` was given it. (checked) Inferred: without extension, e.g. `creature_core`.
- **Edge cases:** inside a script run by `Deprecated_ExecuteScript` or another nested run it is the inner script's name. A failed push returns -638. (checked)

### GetCurrentScriptResource (308)
- **Handler:** 0x00654D70. (checked)
- **Does:** the same name as 145, with **".ncs"** (0x00B00A38) appended (`std::operator+` 0x004C3420), pushed as a resource. (checked)
- **Edge cases:** if the name already had an extension it would be doubled (inferred not to happen, since the load appends the extension itself). A failed push returns -638. (checked)

## Not read
- What a known console command does in slot 4 of the console, and which commands the shipped exe registers.
- Where the talk table's gender byte +0x1C selects the male or female form, and so which of the scripts' GENDER_* values gives which text.
- The module's +0x194 and who reads the VM's +0x196C (the debugger request).
- That `ClientOptions`' +0x68 and +0x4F0 are the attributes named `DifficultyLevel` and `GraphicsDetailLevel` (the names are where the options are saved, not where the attributes are built).
- G2DA's cell-by-index (0x004ED6F0) bounds on a row index; only M2DA's was read.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe (`tools\daoexe.py --disasm`). Not seen in play. Shared facts (table lookup, row IDs, the by-name fallback, -639/-638): the top of `actionsys.md`. Master's `dev/LIBRARY.md` (the "trap" line) and `dev/FILES.md` (seen in play) also use these.

### GetM2DAString (9)
- **Handler:** 0x00653EF0 (not shared). Same body as `GetM2DAResource` (13, 0x00654150) with `StackPushString` 0xA2D730 for the push.
- **Described in:** `dev/script_commands/actionsys.md` ("GetM2DAResource", which says it is this body); `dev/LIBRARY.md` ("A trap").
- **Does:** pops n2DA, sColumn, nRow, and s2DA only when paramCount > 3; reads the cell as text through the table's string getter (vtable +0x20) and pushes it as a string.
- **Edge cases:** the only getter of the five that tests `g_gdaTables` (0x00C1D090) for null (0x0065400D, checked); on null, no table, no row ID or no column the pushed string is "" (checked). nRow is a row ID, not an index. s2DA is read only for n2DA = -1. On an integer column it pushes bytes read from memory (master's LIBRARY.md). A failed push returns -638.

### GetM2DAInt (667)
- **Handler:** 0x006544F0 (not shared). Same shape, the table's int getter (vtable +0x28), `StackPushInteger`.
- **Described in:** `dev/script_commands/actionsys.md` ("GetHashedM2DAInt" is its hash-column twin); `dev/FILES.md` (seen in play), `dev/LIBRARY.md`.
- **Does:** pops n2DA, sColumn (trimmed of " \t", lowercased, hashed), nRow, s2DA (only when paramCount > 3); pushes the cell as an int.
- **Edge cases:** the result starts at **0** (checked, `edi` zeroed at 0x00654548), so a missing table, row or column gives 0 and no failure. No null check on `g_gdaTables`: it is read at once (0x006545E6, checked), so before the tables exist this would fault (inferred from that). nRow is a row ID. s2DA only for n2DA = -1.

### GetM2DAFloat (138)
- **Handler:** 0x006547C0 (not shared). Same shape with the table's slot +0x24 (called at 0x0065492A), `StackPushFloat` 0xA2D690.
- **Described in:** `dev/LIBRARY.md` (the table through `dao::gda`); `dev/ITEMS.md` for a use.
- **Does:** as `GetM2DAInt` but pushes a float. The result starts at 0.0 (`fldz`, 0x00654807, checked).
- **Edge cases:** a missing table, row or column gives 0.0 and no failure. No null check on `g_gdaTables` (checked). The slot +0x24 is read as the float getter by its position between the string (+0x20) and int (+0x28) getters; what the M2DA and G2DA bodies do with a column of another type was not read.

### GetM2DARows (11)
- **Handler:** 0x006549D0 (not shared). The table's vtable slot +0x14 (called at 0x00654A92), `StackPushInteger`.
- **Described in:** `dev/LIBRARY.md` (697 rows for abilities seen in play), `dev/FILES.md`.
- **Does:** pops n2DA, and s2DA only when paramCount > 1; finds the table (by number, by name when n2DA = -1) and pushes its row count.
- **Edge cases:** `g_gdaTables` is **not** tested for null (0x00654A64, checked); a missing table gives **0** (the count starts at 0). That slot +0x14 is the row count is inferred from its use and from the seen counts. The count is the number of rows, not the highest row ID, so with sparse IDs it is not a bound for `nRow`.

### GetM2DAColumns (12)
- **Handler:** 0x00654AE0 (not shared). The table's vtable slot +0x10 (0x00654B28), `StackPushInteger`.
- **Described in:** `dev/LIBRARY.md` (51 columns for abilities, seen in play).
- **Does:** pops n2DA only (script.ldf has no s2DA here) and pushes the column count.
- **Edge cases:** n2DA is looked up by `FindTable` at once, so **-1 is not a by-name lookup** here; it finds no table and pushes **0** (checked: no branch on -1 in this handler). No null check on `g_gdaTables`. That slot +0x10 is the column count is inferred from its use and the seen count.

## Not read
- What the M2DA and G2DA getters do inside (slots +0x10, +0x14, +0x20, +0x24, +0x28) beyond `actionsys.md` and `src/dao/Gda.h`.
- Why the hash helper 0x004C1150 takes a string pointer (the output string); the call shape is the one `actionsys.md` describes.
