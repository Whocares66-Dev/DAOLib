# Script commands, group "placeables": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts (checked unless marked):
- Handlers are `__thiscall (commandId, paramCount)`, `ret 8`, on `CVirtualMachineCommands`. A failed pop returns -639 (0xFFFFFD81) and a failed push -638 (0xFFFFFD82); either aborts the script (`dev/script_commands/commands.md`). The class-rank pair swaps these codes (below).
- Every placeable getter here pops the object, looks it up with `CClientExoApp::GetPlaceableById` (0x005D5B40: the object array's lookup, then the type byte +0x14 compared with the placeable type at 0x00B59400), reads one field and pushes it. **A non-placeable or invalid object gets the default: 0, or -1 for the state, or an empty string.** None of them fails the script for a wrong object.
- **Where the fields come from.** The placeable's template load (0x00750370, the `.utp`'s fields by label, `GffReader` getters 0xA36E90 bool, 0xA37050 int, 0xA371F0 string, 0xA37300 localized string) fills: `AutoRemoveKey` → +0x5E8, `KeyName` → +0x574 (a `std::wstring`), `LocPopupText` → +0x5B8 (a localized string), `PickLock` → +0x5E0, `KeyRequired` → +0x5E4, `InitialState` → +0x658 (u16), `Appearance` → +0x4F0 (u16), `Rank` → +0x6BC, `TreasureCategory` → +0x6C0. Then from `placeable_types` (table 93) by Appearance: `BaseType` (hash 0x5010EDC3) → +0x638 (a byte), and the state controller (hash 0x9A320A51) → +0x654. No script command writes any of these except the state (`SetPlaceableState` 287, `SetPlaceableActionResult` 151). (checked)
- `dev/PLACEABLES.md` has the states, the `statecnt_*` tables, `SetPlaceableState` (0x0074F9D0), the walk-grid patch and "not interactive" vs inactive.

### GetPlaceableState (286)
- **Handler:** 0x0065F590 (not shared). `GetPlaceableById`. (checked)
- **Does:** pushes the u16 at +0x658, zero-extended: the row of the type's `statecnt_*` table (`PLC_STATE_*`). (checked)
- **State:** +0x658, set by `InitialState` at load, by `SetPlaceableState`, and restored from `SavegameAreaPlaceableState`; `dev/PLACEABLES.md` "The initial state". (checked)
- **Edge cases:** a non-placeable or invalid object pushes **-1** (the only getter here whose default is not 0). The field is unsigned, so it never reads negative for a placeable; `SetPlaceableState` stores an out-of-range state as 0 (`dev/PLACEABLES.md`). (checked)

### GetPlaceableBaseType (153)
- **Handler:** 0x0065EFC0 (not shared). `GetPlaceableById`. (checked)
- **Does:** pushes the byte at +0x638, zero-extended. (checked)
- **State:** +0x638 is `placeable_types`' `BaseType` for the placeable's Appearance row, read once at template load (0x00750852–0x0075087D). If the row or column is missing, the byte keeps its earlier value (the constructor's, not read). (checked; the default inferred)
- **Edge cases:** a non-placeable pushes 0. A `BaseType` above 255 would be truncated to its low byte (the table's values fit; `placeable_h` compares with 220). The type is not re-read when the appearance changes later (no other writer found; inferred). (checked apart from the marked)

### GetPlaceableAutoRemoveKey (154)
- **Handler:** 0x0065F020 (not shared). `GetPlaceableById`. (checked)
- **Does:** pushes the int at +0x5E8, the template's `AutoRemoveKey` (a bool, so 0 or 1). (checked)
- **State:** +0x5E8, read only. `placeable_h` (line ~329) uses it to take the key from the user after an unlock; the engine does not act on it here. (checked; script use by grep)
- **Edge cases:** a non-placeable pushes 0. (checked)

### GetPlaceableKeyRequired (155)
- **Handler:** 0x0065F080 (not shared). `GetPlaceableById`. (checked)
- **Does:** pushes the int at +0x5E4, the template's `KeyRequired` (a bool). (checked)
- **State:** +0x5E4, read only; the lock logic is the script's (`placeable_h`). (checked)
- **Edge cases:** a non-placeable pushes 0. (checked)

### GetPlaceableKeyTag (156)
- **Handler:** 0x0065F0E0 (not shared). `GetPlaceableById`, `std::wstring::operator=` (0x004C2AB0), `StackPushString` (0xA2D730). (checked)
- **Does:** copies the `std::wstring` at +0x574 into a local and pushes it. The template field is **`KeyName`**, not "KeyTag". (checked)
- **State:** +0x574, read only. (checked)
- **Edge cases:** a non-placeable pushes "" (the local starts empty). A failed pop returns -639, a failed push -638, the local freed on each path. (checked)

### GetPlaceablePickLockLevel (157)
- **Handler:** 0x0065F1C0 (not shared). `GetPlaceableById`. (checked)
- **Does:** pushes the int at +0x5E0, the template's `PickLock`. (checked)
- **State:** +0x5E0, read only here. A save has `SavegameObjectPickLock` (16258), which is probably this field (inferred). (checked)
- **Edge cases:** a non-placeable pushes 0. No clamping. (checked)

### GetPlaceableTreasureRank (818)
- **Handler:** 0x0065F220 (not shared). `GetPlaceableById`. (checked)
- **Does:** pushes the int at +0x6BC, the template's **`Rank`** field. (checked)
- **State:** +0x6BC, read only. `sys_treasure_h` and `sys_traps_h` use it. (checked; scripts by grep)
- **Edge cases:** a non-placeable pushes 0. (checked)

### GetPlaceableTreasureCategory (817)
- **Handler:** 0x0065F280 (not shared). `GetPlaceableById`. (checked)
- **Does:** pushes the int at +0x6C0, the template's `TreasureCategory`. (checked)
- **State:** +0x6C0, read only. (checked)
- **Edge cases:** a non-placeable pushes 0. The creature's own `TreasureCategory` (`GetCreatureTreasureCategory` 315) is another field, read by another load (0x006C08B2). (checked)

### GetPlaceablePopupText (356)
- **Handler:** 0x0065F2E0 (not shared). `GetPlaceableById`, then 0x004D1EB0 on +0x5B8, then `std::wstring::operator=` and `StackPushString`. (checked)
- **Does:** pushes the text of the localized string at +0x5B8, the template's `LocPopupText`. 0x004D1EB0 resolves it on first use: if its byte +0 is clear it calls 0x004D1D50, which looks the string id (+4) up with `CTlkTable::GetString` (0x004E4990) and stores the text at +8; it then returns +8. (checked; the flag being set afterwards inferred)
- **State:** +0x5B8, a cached localized string; the talk table. (checked)
- **Edge cases:** a non-placeable pushes "". A string id the talk table lacks leaves the text empty (inferred). The text is the current language's, cached at the first read (inferred). Failed pop -639, failed push -638. (checked)

### SetClassRank (325)
- **Handler:** 0x0065F490 (not shared). `CClientExoApp::GetCreatureById` (0x005D58C0), `CCCreature::ForScript_SetClassRank` (0x006B6120). (checked)
- **Does:** pops the creature, nClass, then nRank, and calls `ForScript_SetClassRank(nClass, nRank)`. That locks the creature's class list (+0x544, a SmartData list of 0x78-byte records, ctor 0x006A9D20; the record's +0x58 attribute is the class, +0x68 the rank) and looks for nClass:
  - found and nRank ≠ 0: sets the record's rank (attribute slot +0x54);
  - found and nRank = 0: deletes the record (its slot +0x10(1)) and erases it from the list (0x004A62E0);
  - not found and nRank ≠ 0: makes a record with class and rank and appends it (0x006F1340);
  - not found and nRank = 0: nothing. Then it unlocks (slot +0x24). (checked)
- **State:** creature +0x544. Saved as `SavegameCreatureClassRankList` with `…ClassID`/`…ClassRank` (GFF4 16464–16466; inferred to be this list). No stat, level or ability is recomputed and no event is sent. (checked: the native touches only the list)
- **Edge cases:** a non-creature or invalid object does nothing. nClass and nRank are not range-checked (negative ranks stored). **A failed pop of any argument returns -638**, not -639. Returns 0. No game script calls it. (checked; scripts by grep)

### GetClassRank (623)
- **Handler:** 0x0065F510 (not shared). `GetCreatureById`, `CCCreature::ForScript_GetClassRank` (0x006B1560). (checked)
- **Does:** pops the creature, then nClass. The native copies the class list (+0x544, through 0x00610000) and returns the rank attribute (+0x68) of the first record whose class (+0x58) equals nClass, else 0. (checked)
- **State:** creature +0x544, read. (checked)
- **Edge cases:** a non-creature, invalid object or a class not in the list pushes 0. **The codes are swapped:** a failed pop returns -638 and a failed push -639. No game script calls it. (checked)

## Not read
- The placeable constructor's defaults for +0x638 and the other fields when a template lacks them (only that the load keeps the earlier value).
- The save path's own reads of these fields (only the label ids found in `dev/gff4_fields.csv`).
- Who else reads the creature class list +0x544 (level-up, the character sheet), and the SmartData attribute setter's observers.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Shared facts (checked): the VM is `[0xC1D080]`, the app `[0xC327EC]`. Object pop is 0xA2F6D0, int pop 0xA2F3C0; a failed pop returns -639 (0xFFFFFD81). `GetPlaceableById` is 0x005D5B40, `GetCreatureById` 0x005D58C0. A push failure returns -638.

### SetPlaceableState (287)
- **Handler:** 0x0065EE60 (not shared); `GetPlaceableById`, then `CCPlaceable::SetState` 0x0074F9D0(nState, 0).
- **Described in:** `dev/PLACEABLES.md` ("The use, the animation and EVENT_TYPE_USE").
- **Does:** plays the [old][new] transition and the new idle, stores the state and swaps the walk-grid patch. The second argument to 0x0074F9D0 is always 0 from the script.
- **Edge cases:** an invalid or non-placeable object does nothing and returns 0, no failure (checked). nState is passed unchecked at the command (inferred: a state past the table's columns is the state function's problem). A failed pop of either argument returns -639 (checked).

### SetPlaceableActionResult (151)
- **Handler:** 0x0065EEC0 (not shared); `GetPlaceableById`, then 0x0074FC10(nAction, bSuccess, bVariation).
- **Described in:** `dev/PLACEABLES.md` ("The use, the animation and EVENT_TYPE_USE").
- **Does:** on success sets the state row's SuccessN (SuccessNa with bVariation) through `SetPlaceableState`; it always clears +0x68C.
- **Edge cases:** pops object, nAction, bSuccess and bVariation; all four are popped even when the script left bVariation at its default FALSE (the VM pushes defaults), so a short stack is -639 (checked). bSuccess and bVariation are reduced to bools (`!= 0`); nAction passes as given (checked). An invalid or non-placeable object does nothing and returns 0 (checked).

### GetPlaceableAction (152)
- **Handler:** 0x0065EF60 (not shared); `GetPlaceableById`, reads the **word** at +0x68C.
- **Described in:** `dev/PLACEABLES.md` ("The use, the animation and EVENT_TYPE_USE", step 5).
- **Does:** pushes the action stored by the last use (sub-action 21); `SetPlaceableActionResult` clears it.
- **Edge cases:** an invalid or non-placeable object pushes 0, not an error (checked). The value is zero-extended from 16 bits (`movzx word`), so it never reads negative (checked). The next handler 0x0065EFC0 is the same shape over the byte at +0x638.

### GetCreatureTreasureCategory (315)
- **Handler:** 0x0065F430 (not shared); `GetCreatureById`, reads the int at +0xBD0 (`CCCreature`).
- **Described in:** `dev/SCRIPT_COMMANDS.md` (the `CCCreature` field table), `dev/script_commands/placeables.md` (under `GetPlaceableTreasureCategory`, 817).
- **Does:** pushes the creature template's `TreasureCategory`; the placeable's own is `GetPlaceableTreasureCategory` (+0x6C0).
- **Edge cases:** an invalid or non-creature object pushes 0 (checked). The pop failing is -639; the push failing is -638 (checked).

## Not read

- What state values each placeable type's `statecnt_*` table accepts; `SetPlaceableState` does not validate nState at the command (inferred, state function not re-read).
