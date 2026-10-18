# Script commands, group "vars": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm` and a raw call scan, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not.

Shared facts (checked unless marked), which hold for `GetLocalInt`/`SetLocalInt` (63/64, 0x00655FA0/0x00656120) too:
- **Every handler has one shape.** Pop oObject (`StackPopObject` 0x00A2F6D0), then sVarName (`StackPopString` 0x00A2F4C0), then for a setter the value. A failed pop returns −639, a failed push −638. Look the object up with `CClientExoApp::GetGameObjectById` (0x005D5840), any type. Pick its variable table by the type byte (+0x14):
  - Area (4): vtable slot +0x08 (`return this` on `CCArea`), table at **area+0x260**.
  - Module (3): slot +0x1C (`return this` on `CCModule`), table at **module+0x394**.
  - Anything else: slot +0x18, the `CCObject` cast, table at **object+0x2E0**. Creature, placeable, item, trigger (every kind), waypoint, store, sound, area of effect, projectile and visual effect all answer it.
  - The party (`CCParty`), GUI, tile, map and map patch answer 0 there, so they have no locals.
  - The name becomes an interned name (0x004E8960 on the VM's string pool). The handler then calls `ScriptBackend::ForScript_GetLocal<T>` (0x00702E90…) or `ForScript_SetLocal<T>` (0x00703DB0…) on the table.
- **The table** (constructor 0x00703D80; built at 0x00604623 for the module, 0x00688E4C for the area and 0x00699974 for a `CCObject`) is 13 MSVC8 vectors of 16 bytes, one per type and indexed by it, then a dword at +0xD0. The types are 1 int, 2 float, 3 string, 4 object, 5 location, 6 player, 8 event, 9 command, 10 effect, 11 itemproperty and 12 resource. Types 0 and 7 are unused. These match the template `VarTable`'s `Type`.
  - Each record is 0x14 bytes: +0 the interned name (8 bytes), +8 the type, +0xC the value, +0x10 a "changed" byte.
  - The value at +0xC is inline for int, float, object and player. For the others it is a pointer to a heap copy: a wstring (string, resource), a 0x30 location, a 0x50 event, a 0x14 command, or a 0x7C effect (effect, itemproperty).
  - Lookup (0x00702D50) is linear within the type's vector. So **one name can exist once per type, independently**: an int "X" and a float "X" are two variables.
  - Names match when they are the same pool entry or have equal hashes (0x00702970). Case sensitivity was not read.
- **A name must be declared, or nothing happens.** Every handler passes create = 0 and changed = 1 to the setter backend: `0x00703BF0(name, type, create)` finds the record and calls the adder 0x00703A00 only when create is non-zero.
  - **A Set of an undeclared name, or of a declared name under another type, is a silent no-op.** A Get of one returns the type's default. Neither fails the script.
  - The only code that creates records is the template loader 0x00704240. It reads the GFF3 `VarTable` list (`Name`, `Type`, `Value`; the only reference to the "VarTable" string), with create = 1.
  - It runs for the module (0x00602461, into +0x394), the area (`CCArea::ReadFromSave` 0x00688329, into +0x260) and each `CCObject` (0x0069B1F1, into +0x2E0).
  - The template's `VarTable` holds every variable of its kind's variable table. `gen00fl_alistair.utc` declares 97, read with ghilana.
  - In a template, **an object-typed value always loads as OBJECT_INVALID** (the loader passes −1 at 0x00704520).
- **Defaults on a miss, or for an object with no table:**
  - int: 0.
  - float: 0.0.
  - string and resource: "" (0x00C2BE00, a static the program fills at run time: inferred empty).
  - object: `[0x00B5940C]` = 0xFFFFFFFF, OBJECT_INVALID.
  - location: a new default location (0x007ABD00: area −1, vectors zero).
  - player: 0.
  - event: a default event (0x0069BAC0).
  - command: a null command (type 0).
  - effect: a default effect (0x00A652A0(0)).
- **Saves.**
  - The writer (0x00705A70, called by the module 0x005FEC00, the area 0x00683C80 and `CCObject` 0x00697EE0) puts records into the GFF4 list `ScriptVarTable` (17000: `…Name` 17001, `…Type` 17002, `…Value` 17003). It **skips any record whose changed byte is 0**, so only variables a script has set are written (checked).
  - The readers (0x00705E90 → 0x007053D0) run after the template load, with create = 0 and changed = 1. So **a saved variable the template no longer declares is dropped**.
  - The readers set only int, float, string, object and resource (checked). Location, player, event, command, effect and itemproperty locals are not restored from a save (inferred, as the writer's type filter was not read). `dev/SAVES.md` lists the same five types.
  - **This disagrees with `dev/AI.md`'s play report** that `CREATURE_RULES_FLAG0` set to 8 read 0 after a load. By this reading a script-set int should survive. Not resolved: possibly the creature's `CCObject` load (0x00698ED0) does not get the save's struct, or the load order differs (not read).

### GetLocalFloat (66)
- **Handler:** 0x00656290 (not shared). It calls `ForScript_GetLocalFloat` 0x00702EC0, which looks up type 2. (checked)
- **Does:** pops oObject and sVarName and pushes the record's float (`StackPushFloat` 0x00A2D690). (checked)
- **State:** the object's table (shared facts). (checked)
- **Edge cases:** an invalid object, an object with no table, or an undeclared name gives 0.0 and the script goes on. A name declared only as an int gives 0.0 too, since the types are separate. (checked)

### SetLocalFloat (67)
- **Handler:** 0x00656420 (not shared). It calls `StackPopFloat` (0x00A2F410) and `ForScript_SetLocalFloat` 0x00703DF0, with type 2, create 0 and changed 1. (checked)
- **Does:** pops oObject, sVarName and fValue. If a type-2 record of that name exists, it stores the value and sets +0x10 = 1. (checked)
- **Edge cases:** an undeclared name, an invalid object or one with no table does nothing, silently. A failed pop of the value returns −639. No NaN or range check. (checked)

### GetLocalString (69)
- **Handler:** 0x00656590 (not shared). It calls `ForScript_GetLocalString` 0x00702EF0 (type 3) and `StackPushString` (0x00A2D730). (checked)
- **Does:** copies the record's wstring and pushes it. (checked)
- **Edge cases:** a miss or no table gives "" (the static at 0xC2BE00). (checked; that it is empty is inferred)

### SetLocalString (70)
- **Handler:** 0x00656750 (not shared). It calls `ForScript_SetLocalString` 0x00703E30 (type 3, create 0, changed 1). (checked)
- **Does:** pops oObject, sVarName and sValue, then assigns the record's existing wstring (0x004C2AB0). (checked)
- **Edge cases:** an undeclared name does nothing. Any length is accepted. (checked)

### GetLocalObject (72)
- **Handler:** 0x006568D0 (not shared). It calls `ForScript_GetLocalObject` 0x00702FB0 (type 4) and `StackPushObject` (0x00A2D980). (checked)
- **Does:** pushes the stored id raw. (checked)
- **Edge cases:** a miss gives OBJECT_INVALID. **The stored id is not checked**, so an object destroyed since the set still comes back as its old id. A template's object locals start as OBJECT_INVALID. (checked)

### SetLocalObject (73)
- **Handler:** 0x00656A50 (not shared). It calls `ForScript_SetLocalObject` 0x00703E70 (type 4). (checked)
- **Does:** pops oObject, sVarName and oValue (`StackPopObject`) and stores the id raw. (checked)
- **Edge cases:** oValue is not looked up, so any id, OBJECT_INVALID included, is stored. An undeclared name does nothing. (checked)

### GetLocalLocation (75)
- **Handler:** 0x00656BC0 (not shared). It calls `ForScript_GetLocalLocation` 0x00702FE0 (type 5) and `StackPushEngineStructure(1)` (0x00A2D920). It frees its copy (0x005BDCF0, then `operator delete`). (checked)
- **Does:** copies the stored location (position, orientation, area; the pads are not copied) into a new 0x30 block and pushes it. (checked)
- **Edge cases:** a miss gives a default location (area −1, zero vectors), which `IsLocationValid` calls invalid. That matches script.ldf's "invalid location on error". No game script calls it (grep of the toolset scripts). (checked)

### SetLocalLocation (76)
- **Handler:** 0x00656D60 (not shared). It calls `StackPopEngineStructure(1)` (0x00A2F650) and `ForScript_SetLocalLocation` 0x00703EB0 (type 5). It frees the popped copy. (checked)
- **Does:** if the record has no location yet, it allocates 0x30 and copies the fields. Otherwise it copies 0x24 bytes over the old one. Sets changed. (checked)
- **Edge cases:** an undeclared name does nothing. A null location (an array element past the end) is read without a check and would fault (inferred). Not restored from a save (shared facts). (checked apart from that)

### GetLocalPlayer (78)
- **Handler:** 0x00656EF0 (not shared). It calls `ForScript_GetLocalPlayer` 0x007030A0 (type 6) and `StackPushEngineStructure(5)` with the raw dword. (checked)
- **Does:** pushes the stored player value. (checked)
- **Edge cases:** a miss gives 0. No game script uses the player locals. (checked)

### SetLocalPlayer (79)
- **Handler:** 0x00657070 (not shared). It calls `StackPopEngineStructure(5)` and `ForScript_SetLocalPlayer` 0x00703F50 (type 6). (checked)
- **Does:** stores the popped dword and sets changed. (checked)
- **Edge cases:** an undeclared name does nothing. Not restored from a save (inferred). (checked)

### GetLocalEvent (84)
- **Handler:** 0x006571F0 (not shared). It calls `ForScript_GetLocalEvent` 0x007030C0 (type 8): a new 0x50 event, either a copy (0x0069CB10) or a default (0x0069BAC0). It pushes `StackPushEngineStructure(0)` and frees its copy through the virtual destructor. (checked)
- **Edge cases:** a miss gives a default event (type 0, inferred). No game script calls it. (checked)

### SetLocalEvent (85)
- **Handler:** 0x00657380 (not shared). It calls `StackPopEngineStructure(0)` and `ForScript_SetLocalEvent` 0x00703F80 (type 8). It frees the popped copy. (checked)
- **Does:** if the record holds no event, it allocates one by copy-construction. Otherwise it copies into the existing one (0x00702650). Sets changed. (checked)
- **Edge cases:** an undeclared name does nothing. A null event would fault in the copy (inferred). Not restored from a save (inferred).

### GetLocalCommand (87)
- **Handler:** 0x00657500 (not shared). It calls `ForScript_GetLocalCommand` 0x00703170 (type 9): `new` 0x14 and a copy (0x00741F30). It pushes `StackPushEngineStructure(2)`. (checked)
- **Edge cases:** a miss pushes a null command (type 0). **The handler never frees the copy it pushed**, and the push stores its own copy (`commands.md`). So each hit leaks a command record and its `CDataArrays` (no free: checked; the leak: inferred). No game script calls it.

### SetLocalCommand (88)
- **Handler:** 0x0066E3F0 (not shared). It calls `StackPopEngineStructure(2)` and `ForScript_SetLocalCommand` 0x00704020 (type 9). It returns the popped copy to the pool (0x0066E3A0). (checked)
- **Does:** if the record holds no command, it allocates 0x14 and copy-constructs (0x00741F30). Otherwise `Command::CopyFrom` (0x00741FC0). Sets changed. (checked)
- **Edge cases:** an undeclared name does nothing. A null command would fault in the copy (inferred). Storing it does not queue it.

### GetLocalEffect (90)
- **Handler:** 0x00657670 (not shared). It calls `ForScript_GetLocalEffect` 0x00703210 (type 10): `new` 0x7C, either a copy (0x00A65130) or a default (0x00A652A0(0)). It pushes `StackPushEngineStructure(3)` and frees its copy through the virtual destructor. (checked)
- **Edge cases:** a miss gives a default effect (type 0, EFFECT_TYPE_INVALID: inferred). No game script calls it. (checked)

### SetLocalEffect (91)
- **Handler:** 0x00657800 (not shared). It calls `StackPopEngineStructure(3)` and `ForScript_SetLocalEffect` 0x007040C0 (type 10). It frees the popped copy. (checked)
- **Does:** copy-constructs a new effect into the record, or copies over the old one (0x007023D0). Sets changed. Storing it applies nothing. (checked)
- **Edge cases:** an undeclared name does nothing. Not restored from a save (inferred).

### GetLocalItemProperty (93)
- **Handler:** **none**. 0x00675A9C registers command 0x5D with a null function. `CVirtualMachineCommands::ExecuteCommand` (0x0064DD60) answers −642 (0xFFFFFD7E) for a null entry, which **aborts the script**. (checked)
- **State:** the backend exists, though: 0x007032C0 is a getter for type 11 that copies the 0x7C effect-shaped record, or gives a default. It is reached only from the generic table code (0x00705760). (checked)
- **Edge cases:** no game script calls it. (checked)

### SetLocalItemProperty (94)
- **Handler:** **none**. 0x00675AAC registers 0x5E with a null function, so it returns −642 and the script aborts. (checked)
- **State:** the backend 0x00704160 (type 11) exists, and the template loader fills type-11 variables from a `VarTable`. (checked)
- **Edge cases:** no game script calls it. (checked)

### GetLocalResource (736)
- **Handler:** 0x00657980 (not shared). It calls `ForScript_GetLocalResource` 0x00703370 (type 12) and `StackPushResource` (0x00A2D890; the name is inferred from its use). (checked)
- **Does:** pushes the stored resource name as a string. (checked)
- **Edge cases:** a miss gives "". 13 game scripts use it. (checked)

### SetLocalResource (737)
- **Handler:** 0x00657B40 (not shared). It calls the resource pop (0x00A2F5F0) and `ForScript_SetLocalResource` 0x00704200 (type 12, create 0, changed 1). (checked)
- **Does:** assigns the record's wstring. (checked)
- **Edge cases:** an undeclared name does nothing. The name is not checked against any resource. It is restored from a save. (checked)

## Not read
- How names compare: the interned pool (0x004E8750) and the +0x28 branch of 0x00702970, so whether "foo" matches "FOO".
- The writer's type filter (0x00704BA0 / 0x00705A70 in full) and the meaning of `CCObject`'s load flag (bit 0 of 0x0069AAD0's third argument, which picks the changed mark for template values). Hence why `dev/AI.md` saw a set int lost across a load.
- The generic table copy 0x00705760 (all twelve types; called from 0x007589D0), and the dword at table+0xD0.
- Who else calls `ForScript_SetLocalInt` (0x0066C046, 0x006BC631, 0x006D66E0, 0x007BC4DB, 0x007F0810, …) and with what create flag. One may create records outside a template (the PHYSICS_DISABLED key in `effect_access.md`).
