# Script commands, group "item_prop": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (subagent b3c and a fork of it) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py`, a capstone scan of `.text` for field offsets, the game's tables (`tools\gda.py`, ghilana) and the toolset's scripts, for DAOParagonEngine #27. Not seen in play. "Checked" is read in the disassembly; "inferred" is not. `script.ldf`'s "item_prop" group holds many functions that have nothing to do with items; its name is kept.

Shared facts (checked unless marked):
- Handlers are `__thiscall (command, argCount)`, `ret 8`, on `CVirtualMachineCommands`. Pops: object 0x00A2F6D0, int 0x00A2F3C0, string 0x00A2F4C0, float 0x00A2F410; push int 0x00A2D650, string 0x00A2D730. A failed pop returns -639 (the script fails), a failed push -638. Lookups as in master's `dev/script_commands/items.md`: `0x005D5870` item by id, `GetCreatureById` 0x005D58C0, `GetObjectById` 0x005D5BE0. A lookup of the wrong kind fails, and the handler then does nothing or pushes its default.
- **The shared setter 0x0066BE50** (296, 328, 329, 333, 335, 642, 643, 645, 676, 698, 758, 764, 805, 813, 839) pops the object, then the int only when argCount > 1 (else 0). It tries the creature cast (vtable +0x10) and its cases, then the item cast (+0x14) with 296 and 645, then +0x0C (335), then the placeable cast (+0x24, 698), then **676 for any object**. The creature cases are 328/329/333 (master's `inventory.md`), 642 (0x0066BF89), 643 (0x0066BF72), 758 (0x0066BFA4), and through the table at 0x0066C188/0x0066C19C: 764 (+0xB94), 805, 813 and 839. The returns are always 0 past the pops.
- **The shared getter 0x00668A20** (also 141, 160, 190, 295, 411, 413 and more) pops the object, then up to two ints (default -1) that these commands ignore. 677 is tested first, for any object. Then come the creature switch, the item branch (0x00668BF7: 190, 295, 368, 641, 646), the placeable branch and +0x0C (334). A miss pushes 0.
- **Item fields** (beyond master's `items.md`): byte +0x4B0 droppable, byte +0x4B1 stealable. +0x4A8 is the **wearer's id**: set by the creature's item-equipped handler (0x006C17B0, in 0x006C16E0) and reset to -1 by the unequipped one (0x006C1A28); -1 when the item is not equipped. +0x4BC is the vector of sub-item ids (runes). Item properties are parallel vectors: +0x680 the property ids (`itemprps`, table 107), +0x69C the powers, +0x6B8 and +0x6C8 two ints. Attributes: +0x50C material progression, +0x5E0 damaged, +0x610 unique, +0x630 irremovable. +0x6D8 is a property id the item marks (200000 when none; its role is inferred). +0x6EC is a vector of creature ids (see CanUseItem). The template loader (0x00757870 and 0x00753740) reads `Droppable`→+0x4B0, `Stealable`→+0x4B1, `Damaged`→0x00753250, `Unique`→0x00752BD0 (+0x610), `Cost`, and `MaterialProgress`→+0x50C (0x00757570).
- **The plot-action control** is `module+0x494` (`GetModule` 0x005D4FC0). None of its six handlers checks that a module exists: with none, `[0+0x494]` faults (the missing test is checked; the crash is inferred). They do check +0x494 itself. Its actions are a vector of entry pointers at +0x5C/+0x60. Each entry holds its set (+0x58, inferred `plotactiondefines.ActionSet`), id (attribute +0x5C), state (+0x6C), count (+0x7C) and a bool (+0x8C). The control holds the current set (+0x84), enabled (bool +0x94), a bool +0xA4, the shown slot list (+0x68) and a placeholder entry (+0xB4). Lookup `0x006FCA50` returns the **last** entry with the id, or null. `plotactiondefines` (table 142): ids 1 and 3–8 are set 1, the army; 9–12 are set 2, the Fade shapes.

### GetItemPropertyPower (784)
- **Handler:** 0x00665570 (own) → item 0x007556D0(property, &sum, bIncludeSubItems != 0).
- **Does:** pops the item, nPropertyId, then bIncludeSubItems. It sums the power (+0x69C) of every +0x680 entry equal to the id. With bIncludeSubItems, it recurses into each rune in +0x4BC, and the recursion always includes the runes' own sub-items. It pushes the sum. Checked.
- **State:** reads only.
- **Edge cases:** no item, or not an item: 0. A property not on the item: 0. A rune id that no longer resolves is skipped (checked). Since AddItemProperty refuses duplicates, the sum is one entry's power unless runes add theirs (inferred).

### AddItemProperty (303)
- **Handler:** 0x00665290 (own). It pops the item, nProperty, then nPower.
- **Does:** only when nProperty > 0 and nPower > 0. **If the item is worn** (+0x4A8 resolves to a creature), it calls the creature's 0x006AC9A0, which adds the property through the item's 0x00756800 and, if added, applies the new last entry to the wearer (0x00759190, by `itemprps.IPType`). Otherwise it calls the item's 0x00756800(prop, power, -1, -1) alone. Checked.
- **0x00756800:** if the id is already in +0x680, it does nothing and returns false. Otherwise it appends the id, the power, -1 and -1 to the four vectors and notifies the holder (+0x4E8, vtable +0x28). Either way it then calls item vtable +0x64. Checked.
- **Edge cases:** **an existing property keeps its old power**: there is no update. nProperty or nPower ≤ 0: nothing. The id is not checked against `itemprps`. Runes are not touched. Checked.

### RemoveItemProperty (304)
- **Handler:** 0x00665370 (own). It pops the item, then nProperty.
- **Does:** for a worn item, the wearer's 0x006B3540 finds the property's index, takes its effect off the wearer (0x00757CF0), removes it (0x007562E0), and, when the id equals item +0x6D8, calls 0x00759880 (inferred: the item's marked property re-chosen). For an unworn item, 0x007562E0 alone: it erases the first matching entry from all four vectors, notifies the holder and calls vtable +0x64. Checked.
- **Edge cases:** a property not on the item: nothing. Runes' properties are not touched. No item: nothing. Checked.

### SetItemDroppable (296)
- **Handler:** 0x0066BE50 (shared setter), item case at 0x0066C0D5.
- **Does:** writes byte item+0x4B0 = (bDroppable != 0). There is no notify and no holder refresh. Checked.
- **State:** +0x4B0 is also set by `CreateItemOnObject`'s bDroppable and the template's `Droppable` (master's `dev/ITEMS.md`). The body bag at death takes the creature's droppable items (master's `dev/PLACEABLES.md`).
- **Edge cases:** not an item: nothing. bDroppable is omitted when only one argument is given: 0. Checked.

### IsItemStealable (641)
- **Handler:** 0x00668A20 (shared getter), item case at 0x00668C6C.
- **Does:** pushes (byte item+0x4B1 != 0). Checked.
- **State:** set only from the template's or inventory entry's `Stealable` (0x007578BC, 0x007537A5); no command sets it. Checked.
- **Edge cases:** not an item, or no object: 0. Checked.

### ReadIniEntry (809)
- **Handler:** 0x00674E90 (own), which reads with 0x004E1BF0 (`GetPrivateProfileStringW`, a 512-character buffer, an empty default).
- **Does:** pops sHeadingLabel, then sValueLabel. **The heading `OnlineOptions` (case-sensitive) answers `"Access Denied"`** without reading. Any other heading reads `[heading] value` from `ClientOptions(0x00C32938)+0x550` + `DragonAge.ini` and pushes the text. Checked. Inferred: +0x550 is the user's `Settings\` folder.
- **State:** the file on disk, read at each call; nothing is cached. Checked.
- **Edge cases:** a missing key, an empty value, or a value of exactly 254 or 255 characters gives `""` (0x004E1C66 treats 0xFE/0xFF as failure). So does a value equal to the string at `[0x00BF4890]` (not read). If `g_eclipseGame` (0x00C1D0A0) byte +0x251 is set, `("DebugOptions", "SkipCharGen")` answers `"1"` (inferred: a command-line skip). Checked except as marked.

### GetPlatform (694)
- **Handler:** 0x00651AB0 (own).
- **Does:** pushes the constant 1, `PLATFORM_PC`. Checked.
- **Edge cases:** none; a failed push returns -638.

### SetPlotActionSet (810)
- **Handler:** 0x006658E0 → `ScriptBackend::ForScript_SetPlotActionSet` 0x006FDB40 on the control.
- **Does:** stores the set at +0x84 (notified). With 0, it clears the shown list (+0x68) and stops. With any other set, it sets bool +0xA4 to 1 and clears the list. It then appends every entry whose state is not 0 and whose +0x58 equals the set, in vector order, and pads the list to 5 slots with the placeholder. Checked.
- **State:** control +0x84, +0xA4, +0x68. The `ArmyControl` movie draws the list (inferred).
- **Edge cases:** more than 5 matches are all kept: there is no truncation. An unknown set gives 5 placeholders. 0 leaves +0xA4 as it was. There is no range check, and no module means a crash (shared facts). Checked.

### SetPlotActionsEnabled (795)
- **Handler:** 0x00665930 → `ScriptBackend::ForScript_SetPlotActionsEnabled` 0x006FC450.
- **Does:** sets the control's bool +0x94 to (nEnabled != 0) and notifies it. Checked.
- **Edge cases:** the states and the list are untouched. That a disabled control blocks clicks is the movie's doing (inferred). No control: nothing.

### SetPlotActionState (796)
- **Handler:** 0x00665980 → `ScriptBackend::ForScript_SetPlotActionState` 0x006FD150. It pops the id, then the state.
- **Does:** stores the state at entry +0x6C as given and notifies it. From an old state of 0 or 2 to 1, it sets the entry's +0x8C to 1 (inferred: the "updated" flash). From 0 to 1, if the entry is not shown, it goes into the first placeholder slot. To 0, the placeholder is put back in its slot. Checked in outline.
- **Edge cases:** an unknown id: nothing. A state outside 0–3 is stored, and only 0, 1 and 2 drive the list. With 5 slots in use, a newly enabled action is not shown. The slot is filled whatever the current set is (+0x58 is not compared). 0→3 does not add it to the list (inferred from the branches).

### GetPlotActionState (797)
- **Handler:** 0x006659E0 → `ScriptBackend::ForScript_GetPlotActionState` 0x006FD020.
- **Does:** pushes the entry's state (+0x6C). Checked.
- **Edge cases:** an unknown id, or no control: 0, which is the same as "Invalid". Checked.

### SetPlotActionCount (798)
- **Handler:** 0x00665A50 → 0x006FD050(id, count).
- **Does:** stores the count at entry +0x7C and notifies it. Checked.
- **Edge cases:** it is not clamped to `plotactiondefines.MaxCount`, and a negative count is stored. An unknown id: nothing. Checked.

### GetPlotActionCount (799)
- **Handler:** 0x00665AB0 → 0x006FD080.
- **Does:** pushes entry +0x7C. Checked.
- **Edge cases:** an unknown id, or no control: 0. Checked.

### CanUseItem (812)
- **Handler:** 0x00665710 (own) → `CCCreature::ForScript_CanUseItem` 0x006AE8B0(item). It pops oCreature, then oItem, and pushes the bool. Each step below that fails answers FALSE. Checked.
- **1. Damage:** a damaged item (+0x5E0 ≠ 0) fails.
- **2. Quickbar lock:** an item whose ability (+0x660) is not `INVALID_ITEM_ABILITY` (200000) fails when the creature's party data is locked (`CCCreatureStats` +0x1B4, its bool at +0, which is `LockQuickslotBar`'s flag). Creature +0x5F4 (`SetCanChangeEquipment`) set skips this test.
- **3. Requirements:** unless the item has the property `globals` row 18 `ITEM_PROPERTY_IGNORE_ITEM_REQUIREMENTS` (10000; 0x00755E50), each of the base item's `ReqProp1`/`ReqProp2` (0x007531C0 counts them) goes through 0x006ACEB0. That reads the requirement through 0x00755270 (`bitm_base` and `materialrules`; how the material changes it is inferred) and the creature's property. **It is enforced only for a creature in its party's pool** (`CCCreature::GetParty`, `CCParty::IsInPartyPool`); others pass. It passes when the creature's value is < 1e-6, or ≥ the requirement.
- **4. Restriction scripts:** for each of the item's properties whose `itemprps.IPType` is 3 (the "Required: …" rows 6000+), it sets creature +0xC70 = 0. It builds an event of type 70 (object 0 the item, object 1 the creature, int 0 the property) and runs the row's `ConditionScript` (e.g. `ip_restrict_ass`) on the creature at once (0x006985F0; inferred synchronous from the read right after). A +0xC70 still 0 fails.
- **5. Slots:** a base item with `EquippableSlots` ≠ 0 must share a bit with creature +0xCB0 (0x0069F110).
- **6. Reservations:** when item +0x6EC already holds as many creature ids as the stack size (+0x5F0), the creature must be among them (inferred: users reserving a stack, added by 0x006AEC10).
- **Edge cases:** no creature or no item: FALSE (no call). Checked.

### SetCanUseItem (813)
- **Handler:** 0x0066BE50 (shared setter), the creature case through the table (index 0x31) at 0x0066C008.
- **Does:** writes creature int +0xC70 = value, as given and not made bool. Checked.
- **State:** +0xC70 is reset to 0 by CanUseItem before each restriction script, and read right after as == 0 (0x006AEB4C). Also read elsewhere at 0x006AD0E6 (`cmp 1`) and reset at 0x006ACFE5 (not read). Checked.
- **Edge cases:** outside a restriction script it only leaves a value that the next test resets. Not a creature: nothing. With one argument: 0. Checked.

### SetObjectInteractive (676)
- **Handler:** 0x0066BE50 (shared setter), the last case at 0x0066C163, for any object → `CCObject` 0x0069BA70(value != 0). Read in master's `dev/PLACEABLES.md` ("Not interactive", ID 14239).
- **Does:** writes byte +0x42C, then asks the object's vtable +0x94 (for a creature, 0x006BA840: interactive, a test 0x006A90D0, and slot +0x1D0 == 1). True calls 0x0069B900, false 0x0069B830. Both walk the effects holder for `EFFECT_TYPE_AOE` (5) effects matching two filters (inferred: an AoE aura switched on and off with interactivity). Checked except as marked.
- **Edge cases:** any object type, items and triggers included, gets the byte. No object: nothing. A placeable's state change can rewrite it (`SetPlaceableState`, PLACEABLES.md). Checked.

### GetObjectInteractive (677)
- **Handler:** 0x00668A20 (shared getter), the first test at 0x00668AC8.
- **Does:** pushes (byte +0x42C != 0) for any object. Checked.
- **Edge cases:** no object: 0. Checked.

### SetItemDamaged (645)
- **Handler:** 0x0066BE50 (shared setter), item case at 0x0066C0BE → item 0x00753250(value).
- **Does:** **only when the item is not worn** (+0x4A8 == -1): sets attribute +0x5E0 to the value as given (not made bool), notifies the holder (+0x4E8 vtable +0x28) and calls item vtable +0x64. Checked.
- **State:** +0x5E0; CanUseItem refuses a damaged item. The template's `Damaged` goes through the same setter. Checked.
- **Edge cases:** **on an equipped item it does nothing**, unlike `script.ldf`'s plain "sets" (checked). Not an item: nothing. With one argument: 0, which clears it. Checked.

### GetItemDamaged (646)
- **Handler:** 0x00668A20 (shared getter), item case at 0x00668C5D.
- **Does:** pushes attribute +0x5E0 as stored (any int the setter was given). Checked.
- **Edge cases:** not an item: 0. Checked.

### GetItemUnique (368)
- **Handler:** 0x00668A20 (shared getter), item case at 0x00668C3F.
- **Does:** pushes attribute +0x610, loaded from the template's `Unique` (0x00752BD0). No command sets it. Checked.
- **Edge cases:** not an item: 0. Checked.

### GetItemMaterialProgression (369)
- **Handler:** 0x0066A060 (own).
- **Does:** pushes item attribute +0x50C, loaded from the template's `MaterialProgress` (0x00757570, the old value as default). Checked.
- **Edge cases:** not an item: 0. Checked. It is used by `ScaleEquippedItems` (386; inferred, not read).

### DEBUG_SendRawInputEvent (338)
- **Handler:** 0x00651AD0 (own).
- **Does:** pops nFrameNumber, then sRawEventName, and returns 0. **A no-op in this build.** Checked.
- **Edge cases:** only a failed pop (-639).

### DEBUG_SetCursorPosition (339)
- **Handler:** 0x00651B90 (own).
- **Does:** pops x, then y, and returns. **A no-op in this build.** Checked.
- **Edge cases:** only a failed pop (-639).

### GetAutoLevelFollowers (341)
- **Handler:** 0x00666300 (own): the party (0x005D57C0), its bool attribute +0x84 (getter 0x00838210).
- **Does:** pushes the party's "auto level party members" option, 0 or 1. Checked. A creature joining the party copies this flag unless its own auto-level is 2 (master's `inventory.md`, GetAutoLevelUp).
- **Edge cases:** **there is no null check of the party** (checked). With no party it reads near address 0x90 and faults (inferred).

### GetShowSpecialMoveFloaties (601)
- **Handler:** 0x00666340 (own).
- **Does:** pushes the bool at ClientOptions (0x00C32938) +0xF8 (byte +0x104). Checked. Inferred: this is the `ShowSpecialMoveFloaties` option, registered at 0x005E6E00.
- **Edge cases:** none. It is an options value, not part of a save. `combat_h` reads it before a special-move floaty.

### SetCreatureIsGhost (643)
- **Handler:** 0x0066BE50 (shared setter), creature case at 0x0066BF72 → 0x006BBFC0(value).
- **Does:** when the value differs from int +0xC90, it changes the weapons' visual effect on the items in slots 0, 1 and 2 (main, off hand, ranged ammo; 0x006A0ED0):
  - on: 0x006A6690 applies to each item a `CGameEffect` of type 4 (`EFFECT_TYPE_VISUAL_EFFECT`), int 0 = 3007 (`GHOST_VFX`), ability 150004 (`ABILITY_TRAIT_GHOST`).
  - off: 0x006BBF10 removes those.
  - Then it calls `MovementController::LiftObstacle` (creature +0xA40). Checked.
- **Then** it stores the value at +0xC90. On, with a physics controller (+0x770, `SetPhysicsController`'s), it destroys the controller (0x0069EDC0). Off, with none, it makes one (0x006B1ED0(-1, -1)). Either way it calls `CCCreature::PlaceObstacle` (0x006A3BC0), which passes (+0xC90 ≠ 0) to the pathfinder. Checked.
- **State:** +0xC90 is read by `CCObject::RequestPath` (0x0073EB30: the pathfinder's +0xC4 flag), by the item-equipped handler (0x006C1889: a newly equipped weapon gets the visual), and by the save (GFF field 0x4053, 0x006B27CB). Checked. That ghosts pass through others is the pathfinder's doing (inferred; `script.ldf`).
- **Edge cases:** the value is stored raw. So 1 then 2 counts as a change, and the visual is applied a second time (inferred). Ghost off creates a physics controller even for a creature that had none before (checked). Not a creature: nothing.

### SetCreatureIsStatue (642)
- **Handler:** 0x0066BE50 (shared setter), creature case at 0x0066BF89 → 0x0069F4A0(value != 0).
- **Does:** writes byte +0xC94.
  - On: `ClearAllCommands(1, 1)` (0x0073C930, a hard clear). Effects holder slot +0x4C(1) pauses the animation (`CCObject::SetAnimationPaused`, `CCreatureAnimController::Pause`). Creature slot +0xB0 replays the current animation ([+0x154]+0x140) as (1.0, -2.0, 1). Slot +0x58 sets float +0xB04 to 0.0 (inferred: the animation speed).
  - Off: resumes the animation and sets +0xB04 to 1.0. Checked.
- **State:** +0xC94 is read by `CCCreature::Update` (0x006BE7A9: a statue skips a block of the per-frame update, inferred to be perception and AI), by the cursor (0x0070CE74: "examine" in place of "converse"), and by the save (GFF field 0x4056, 0x006B27F6). Checked except as marked.
- **Edge cases:** off on a creature that was never a statue still resets the speed to 1.0, whatever it was (inferred). Commands queued while it is a statue are not refused here (not read). Not a creature: nothing.

### InitHeartbeat (342)
- **Handler:** 0x00666380 (own) → `ScriptBackend::ForScript_InitHeartbeat` 0x0069F440. It pops the object, then fInterval.
- **Does:** sets creature float +0xC98 (the interval) to fInterval and +0xC9C (elapsed) to 0. Checked.
- **Tick** (`CCCreature::Update` 0x006BE4B0, at 0x006BE889): with an interval > 0, the frame delta is added to +0xC9C. Once that is **strictly greater** than the interval, it sends `EVENT_TYPE_HEARTBEAT2` (81) to the creature, with float 0 the elapsed time, through `AIMaster::AddEventDeltaTime` (0x0064ACE0). It then resets to 0, dropping the overshoot. Checked.
- **State:** +0xC98 is saved as GFF field 16475 (0x006B2E2E, read at 0x006BF4F5). +0xC9C is not saved (inferred).
- **Edge cases:** an interval ≤ 0, or NaN, never fires. Not a creature: nothing, though fInterval is still popped. A second call restarts the count. Checked.

### EndHeartbeat (343)
- **Handler:** 0x00666400 (own) → 0x0069F460.
- **Does:** sets +0xC98 and +0xC9C to 0.0, which stops the heartbeat. Checked.
- **Edge cases:** not a creature: nothing. An event already queued stays queued (inferred).

### LinkAreaMiniMaps (357)
- **Handler:** 0x0065F7C0 (own) → `CCModule::ForScript_LinkAreaMiniMaps` 0x00602AF0.
- **Does:** pops sFirstArea, then sSecondArea. It appends the pair of strings to the module's vector at +0x340 and **always pushes TRUE**. Checked.
- **State:** module +0x340. Its reader (the shared fog) and whether it is saved were not read.
- **Edge cases:** the areas are not checked to exist. Repeated or reversed pairs are appended again. `script.ldf`'s "FALSE otherwise" never happens. There is no null check of `GetModule` (a crash is inferred). Checked except as marked.

### GetItemEquipSlot (358)
- **Handler:** 0x00665410 (own).
- **Does:** if the item's wearer (+0x4A8) resolves to a creature, it asks the creature's `Equipment` (+0xBE0), slot +0x4C (0x00797C80, through 0x007612E0), for the item's place. That searches the active slots (+0x24) first, then every weapon set (+0x48, 0x20 bytes each). It pushes the slot. Checked.
- **Edge cases:** not equipped, no item, or a wearer that no longer resolves: 255 (`INVENTORY_SLOT_INVALID`). An item in the **inactive** weapon set still answers its slot; the set is not returned. Checked.

### SetItemIrremovable (375)
- **Handler:** 0x006654B0 (own) → item 0x00752BF0(bIrremovable != 0).
- **Does:** sets the bool attribute +0x630 and notifies the holder (+0x4E8 vtable +0x28). Checked.
- **State:** read by the equip test 0x006AF630 (an irremovable item in the target slot, or in the off hand for a two-hander, blocks equipping over it) and by `Equipment`'s slot search 0x00797EC0, which skips such slots. Also read by the item's GUI data at 0x007144B8 (inferred: the paperdoll's lock). No template field sets it (no `Irremovable` string in the exe); it is kept by the item's copy at 0x00756C80 (inferred: saves). Checked except as marked.
- **Edge cases:** it works on an unequipped item too, where it takes effect once worn. Not an item: nothing. That scripts can still unequip it is `script.ldf`'s (not read). Checked.

### IsItemIrremovable (376)
- **Handler:** 0x0066BD70 (own).
- **Does:** pushes the bool +0x630 (0x00838210). Checked.
- **Edge cases:** not an item: 0. Checked.


## The second part's shared facts

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm` and capstone scans, `script.ldf` and the toolset's scripts, for DAOParagonEngine #27. Not seen in play. "Checked" is read in the disassembly; "inferred" is marked. `script.ldf`'s `item_prop` group holds many unrelated functions; the name is kept.

Shared facts (checked unless marked):
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`, and returns 0, −639 (0xFFFFFD81) when a pop fails, or −638 when a push fails; both fail the script (master's `dev/script_commands/commands.md`). No handler here fails because an object is wrong or missing: a getter pushes its default.
- Lookups as in master's `dev/script_commands/items.md`: `0x005D5870` item by id, `CClientExoApp::GetCreatureById` `0x005D58C0`, `GetObjectById` `0x005D5BE0`. Cast slot +0x10 of an object's vtable is `CCCreature` (`src/dao/generated/ObjectCasts.h`).
- A `SmartData` attribute's bool is at its +0xC (`0x00838210` reads it); `SmartData::Attribute::Get` is `0x0053CC40`; its vtable +0x54 sets it.
- **The online features object** is a static at `0x00C23998` (got by `0x00402900`; its code's assert names `.\OnlineFeatures.cpp`). The handlers reach it only when `g_eclipseGame` (`[0x00C1D0A0]`, `0x00401F30`) is non-null, which it is in play (inferred). Its +0xDE byte is "signed in to the online service" (inferred: set by the one-line `0x004749A0` and at `0x0048C14D` after a login; every online call tests it first). Its +0x6EF0 is a `std::vector` of achievement records of 0x124 bytes, loaded from GDA table 268 (`achievements`, `0x0047EC00` finds table 0x10C). A record: +0x00 the name (`std::wstring`, its length at +0x14), +0x10C unlocked, +0x10E/+0x10F flags (+0x10F "changed", inferred: cleared by `0x0047A260`), +0x110 the count to unlock at (1 by default), +0x114 the count. An id is the record's index.
- **The online service** is `[0x00C1D9E4]` (`0x0041E250`), a 0x764-byte object made once at `0x00430B20`, EA's online SDK (its strings: `AchievementManager`, `TelemetryManager`, "...not signed in to Xbox Live", "...not defined on the server"). **In the GOG build nothing is stubbed**: the code is the retail code. With BioWare's and EA's DA:O online services shut down, no sign-in succeeds, so +0xDE stays 0 and every path gated on it does nothing (inferred, not seen in play). The achievements themselves are kept locally in the records above and still unlock.
- `ClientOptions` is `[0x00C32938]` (master's `dev/CAMERA.md`).

### SetItemIndestructible (377)
- **Handler:** 0x00665510 (not shared). Pops oItem, then the int; `0x005D5870`; `ScriptBackend::ForScript_SetItemIndestructible` (0x00752C30). Checked.
- **Does:** sets the item's attribute at +0x640 to `bIndestructible != 0` (vtable +0x54), then if the item has a holder (+0x4E8) calls the holder's vtable +0x28(item), then the item's vtable +0x64: the same tail as `SetItemAbilityId` (master's `items.md`). Checked.
- **State:** item+0x640 (its bool at +0x64C). It is one of the 17 attributes `0x007141F0` publishes to the GUI as `_root.ReturnedItemAttributes`; read also in the item code at 0x00754020–0x007589D0. Checked; that the inventory screen hides its destroy option for it is inferred (`script.ldf`: "cannot be destroyed by the player").
- **Edge cases:** no item or not an item: nothing, returns 0. Any non-zero int is TRUE. Checked.

### IsItemIndestructible (378)
- **Handler:** 0x0066BDE0 (not shared). Pops oItem; `0x005D5870`; `0x00838210` on item+0x640. Checked.
- **Does:** pushes 1 or 0 from item+0x640's bool. Checked.
- **Edge cases:** no item or not an item: 0. Checked. No shipped script calls it.

### IsHumanoid (363)
- **Handler:** 0x00666450 (not shared). Pops oObject; `GetObjectById`; cast slot +0x10 (`CCCreature`); `ScriptBackend::ForScript_IsHumanoid` (0x006A25A0) → `0x0069F030`. Checked.
- **Does:** takes the creature's appearance record (+0x7B0), its row (u16 at +0x58), reads `apr_base` (GDA table 2) column `CombatModelTree` (hash 0xDE80B418) as a string, and answers whether it contains the narrow text `combat_humanoid` (0x00B0F318; `std::wstring::find`, case-sensitive). Checked.
- **State:** creature+0x7B0; `apr_base.CombatModelTree`. Checked.
- **Edge cases:** no object, or not a creature: 0; no appearance record (+0x7B0 null) or a missing cell: 0. `PlayHumanoidAnimations` is not consulted. Checked.

### ShowAsAllyOnMap (364)
- **Handler:** 0x006665C0 (not shared). Pops oCreature (`GetCreatureById`), then the int. Checked.
- **Does:** sets the creature's attribute at +0xCB4 to `bEnable != 0` (vtable +0x54). Checked.
- **State:** creature+0xCB4. The map's walk of creatures (`0x00633E10`) takes a creature when `0x006A2B80` answers or this bool is set (checked at 0x00633FC5); that the result is the minimap's blue dots is inferred. Also read and written by the creature's save and load (0x006B2C52, 0x006BF2D0; inferred).
- **Edge cases:** no creature: nothing, but the int is still popped. Checked.

### GetBackgroundDefaultsIndex (637)
- **Handler:** 0x0066C500 (not shared). Pops oCreature; `GetCreatureById`; the creature's stats (attribute +0x7C4); `ScriptBackend::ForScript_GetBackgroundDefaultsIndex` (0x0071BE90). Checked.
- **Does:** pushes race × 1000 + class × 100 + background. Race is the stats' attribute +0xB0 (`0x00719EA0`); class is property 27 (`PROPERTY_SIMPLE_CURRENT_CLASS`, `0x0071B010`) and background property 37 (`PROPERTY_SIMPLE_BACKGROUND`, `0x0071AFB0`), each the property's vtable +0x14 value as a float, truncated (`_ftol`). Checked. That the result is a row id of `background_defaults` (table 1001) is `script.ldf`'s.
- **Edge cases:** no creature, or no stats: −1. A property list too short to hold 27 or 37: that part is 0. Nothing checks the row exists. Checked. No shipped script calls it.

### SetZoomLevel (849)
- **Handler:** 0x00666790. Already read in master's `dev/CAMERA.md` ("The script's `SetZoomLevel`"): the explore camera controller's (`app+0x114` → +0x60) +0x50 = clamp(z, 0, 1) × 2 − 1; no controller: nothing. Checked.

### UnlockAchievement (638)
- **Handler:** 0x00651BE0, shared with 616 (`cmp commandId, 0x27E` picks 638). Pops the string; then `0x0047D7C0` on the online features object. Checked.
- **Does:** finds the record whose name equals the string (`0x004C0DE0`, exact, case-sensitive) and does what `UnlockAchievementByID` does to it (sets +0x10C and +0x10F, awards it through the service, `0x00474770`). Checked in outline; the tail after the award (0x0047D9F2 on) not read line by line.
- **Edge cases:** no record of that name, or already unlocked: nothing. Checked. `script.ldf` calls it slow: it is a linear walk.

### UnlockAchievementByID (616)
- **Handler:** 0x00651BE0 (shared with 638, the 0x268 branch). Pops the int; `0x0047DAC0`. Checked.
- **Does:** for an id inside the records, not yet unlocked, with a non-empty name: sets unlocked (+0x10C) and +0x10F, then `0x00474770(name, 1)` hands the award to the service's achievement manager (service vtable +0x3C, then its +0x54): signed in (+0xDE) with the user's id from the service, else with none (the SDK's offline award, inferred). If that call was made, it sends telemetry `GAME`/`ACHI` with the record's +0x3C (`SendOnlineTelemetry`'s backend, gated as there). Checked.
- **State:** the record's +0x10C, +0x10F. Where the records are kept between sessions was not read.
- **Edge cases:** id negative or past the table (compared unsigned): nothing. Already unlocked: nothing more. No service: unlocked locally, nothing sent. The handler ignores the result. Checked.

### GetHasAchievement (629)
- **Handler:** 0x00651CF0, shared with 603 (`cmp commandId, 0x275` picks 629). Pops the string; `0x0047A610`. Checked.
- **Does:** pushes the unlocked byte (+0x10C) of the first record whose name equals the string (exact compare). Checked.
- **Edge cases:** no such record: 0. Local state only: no online call. Checked.

### GetHasAchievementByID (603)
- **Handler:** 0x00651CF0 (shared with 629, the 0x25B branch); `0x0047A5C0`. Checked.
- **Does:** pushes record[id]+0x10C. Checked.
- **Edge cases:** id out of range (unsigned compare): 0. Local only. Checked. 11 shipped scripts call it.

### GetAchievementCountByID (618)
- **Handler:** 0x00666810 (not shared). Pops nID; copies the record (`0x0047A700`) and pushes its +0x114. Checked.
- **Edge cases:** id negative or past the table: a default record (`0x00474290`: count 0, target 1), so 0. With no `g_eclipseGame`: −1 (not in play, inferred). Checked. No shipped script calls it.

### IncrementAchievementCountByID (617)
- **Handler:** 0x00651E20 (not shared). Pops nID, then nIncrement (its default 1 is the compiler's); `ScriptBackend::ForScript_IncrementAchievementCountByID` (0x0047EB30). Checked.
- **Does:** count (+0x114) += nIncrement, even on an unlocked record. If count ≥ target (+0x110), count = target, and if the record was not unlocked before, it is unlocked (`0x0047DAC0`) and 1 is pushed. Otherwise 0. Checked.
- **Edge cases:** id out of range (unsigned): 0. A negative increment lowers the count with no floor. Pushes 1 when the unlock was attempted, whatever `0x0047DAC0` did (an empty name). No `g_eclipseGame`: −1. Checked.

### UpdateOnlineStatistic (639)
- **Handler:** 0x00651EC0. Pops sStatName, nUpdateType, nUpdatePeriod, fStatValue, sStatText; `ScriptBackend::ForScript_UpdateOnlineStatistic` (0x0047F5F0). Checked.
- **Does:** only when signed in (+0xDE): truncates the name to 64 and the text to 255 characters (logging "...too long. Truncating..."), and hands a stat record to the service (vtable +0x48 → +0x28 → +0x08 for the user, then on). Checked to the request's build; the send not read.
- **Edge cases:** not signed in: nothing (so nothing in the GOG build, inferred). No shipped script calls it. Checked.

### GetOnlineStatistic (640)
- **Handler:** 0x00652050. Pops sStatName, nPeriodType; calls `ScriptBackend::ForScript_GetOnlineStatistic` (0x00480980, `ret 0x20`, gated on +0xDE), then **always pushes 0.0** (`fldz` at 0x00652110): the backend's request gives the script nothing. Checked.
- **Edge cases:** always 0.0, signed in or not. Checked.

### SendOnlineTelemetry (630)
- **Handler:** 0x00652170. Pops nModuleID, nGroupID, sTelemetryText; `ScriptBackend::ForScript_SendOnlineTelemetry` (0x0047D4D0)(module, group, &text, 0). Checked.
- **Does:** sends a record (formatted `%d+%s`) through the service's telemetry manager (vtable +0x40) only if: `0x0054F810` on the game answers false, `ClientOptions`+0x260 is set (inferred: `EnableOnlineTelemetry`, the Online tab's switch), signed in (+0xDE), +0xDC set, the manager exists, and the fourth argument + 1 ≤ 99. Checked; the meaning of `0x0054F810` and +0xDC not read.
- **Edge cases:** any gate false: nothing (the GOG build: nothing, inferred). The engine itself also calls it (achievements, deaths, cutscene skips: master's `dev/COMBAT.md`, `dev/CUTSCENES.md`). No shipped script calls it. Checked.

### SendOnlineScreenShot (631)
- **Handler:** 0x0066C580. Pops sFileName, sShortDescription, sLongDescription; builds a 0x68-byte upload record (`0x00491300`) with the file name and **the long description only**; two of its flags set to 1; `0x004E2AC0` on the online features object. Checked.
- **Does:** uploads only when signed in (+0xDE) and an uploader exists (+0x6D18, `0x00494400`). Checked.
- **Edge cases:** sShortDescription is popped and dropped. Not signed in: nothing (the backend's −1 is ignored). No shipped script calls it. Checked.

### IsMoving (388)
- **Handler:** 0x00666550 (not shared). Pops oCreature; `GetCreatureById`. Checked.
- **Does:** pushes 1 if the creature's +0xB20 is non-zero, else 0. Checked.
- **State:** creature+0xB20, a movement state: 0 still; the engine compares it with 1, 2 and 4 elsewhere (0x006A3BDA, 0x00760A19). Its writer was not found by offset (inferred: written through a sub-object). Checked except as marked.
- **Edge cases:** no creature or not a creature: 0. Used by `spell_aoe_duration.nss`. Checked.

### SetCheatUsedFlag (627)
- **Handler:** 0x00665890 (not shared, no arguments). The application's +0x14C; `ScriptBackend::ForScript_SetCheatUsedFlag` (0x00643570). Checked.
- **Does:** in that object's GFF4 (its +0x10, root via `0x004DAAA0`), finds the struct `SavegameVersion` (16004); if it has `SavegameCheatUsed` (16007), sets it to 1 (`0x00478340`). Checked; the field names are xoreos's (`dev/gff4_fields.csv`), and that the object is the game's save record kept in memory is inferred.
- **Edge cases:** no +0x14C object, no GFF, no struct or no field: nothing. No way to clear it. 30 shipped (cheat) scripts call it. Checked.

### ShowAllCodexEntries (619)
- **Handler:** 0x006658B0 (not shared, no arguments). The party (`0x005D57C0`: app+0xD0 → +0x58), its +0x210; `ScriptBackend::ForScript_ShowAllCodexEntries` (0x00760250). Checked.
- **Does:** party+0x210 is a 0x6C-byte object built from every `.plo` resource (`0x0075DAC0`/`0x0075DB80`, made at 0x0062EBE0). The backend walks its list, loads each plot's resource and, for entries with an index other than −1, calls `0x0075F7E0` with a name (checked in outline); that this sets each codex plot's flags so the journal shows them is `script.ldf`'s, inferred.
- **Edge cases:** no party or no plot table: nothing. A debug command; no shipped script calls it.

### SetRoamLocation (824)
- **Handler:** 0x00666A10 (not shared). Pops oCreature, then the location (`StackPopEngineStructure(1)`); `GetCreatureById`. Checked.
- **Does:** copies the location's position (x, y, z) into creature+0xCD0 (a 16-byte store; its fourth float is stale stack). **The area and orientation are dropped.** Frees the popped location. Checked.
- **Edge cases:** no creature: nothing. Checked. Only `creature_core.nss` calls it, at spawn, with the creature's own position when the local `ROAM_DISTANCE` > 25.

### SetRoamRadius (825)
- **Handler:** 0x00666AB0 (not shared). Pops oCreature, then the float; writes it to creature+0xCE0. Checked.
- **Does:** the engine's check (`0x006A2D30`, per creature): when the radius is non-zero, every 2,000 ms (counter +0xCE8, fed by the frame's milliseconds, inferred), it measures the distance from +0xCD0 to the position (+0x120); beyond the radius, and only once until back inside (latch +0xCE4), it signals `EVENT_TYPE_ROAM_DIST_EXCEEDED` (97) to the creature (`0x0069BAC0`, then `0x0064ACE0` on app+0x1AC); inside, it clears the latch. `creature_core.nss` answers it by running home at 1.5× speed. Checked.
- **Edge cases:** no creature: nothing. Radius 0 turns the check off. A negative radius makes every distance "beyond" (inferred from the compare). Area is not considered. Checked.

### GetRoamLocation (826)
- **Handler:** 0x006668D0 (not shared). Pops oCreature; `GetCreatureById`. Checked.
- **Does:** pushes a location (`CScriptLocation`, master's `dev/script_commands/locations.md`) with position = creature+0xCD0..+0xCD8, orientation (0, 0, 0), area = the creature's current area (+0x254), not the one it was given. Checked.
- **Edge cases:** no creature: the default location (position 0, area OBJECT_INVALID). Never set: position (0, 0, 0) in the creature's area (inferred: +0xCD0 is zeroed at creation, 0x006BA703). Checked except as marked.

### GetGamePlotAssist (602)
- **Handler:** 0x00666B10 (not shared, no arguments). Pushes `ClientOptions`+0xA8 (`SmartData::Attribute::Get`). Checked. That +0xA8 is the Game tab's `PlotAssist` option is inferred from the command's name and `GAME_PLOT_ASSIST_*`.
- **Edge cases:** no check of `[0x00C32938]`: a null options object would fault (it is made at start, inferred). Used by `achievement_core_h.nss`.

### GetPRCEnabled (828)
- **Handler:** 0x0066D6D0 (not shared). Pops the string (its default "" is the compiler's). Checked.
- **Does:** walks the add-in list (`0x004028F0` → `0x00C32DC0`, records of 0x2A4 bytes, count `0x00474FB0`) for the record whose UID (+0x1C, `std::wstring`) equals the string exactly; pushes its +0x15D, cleared if +0x15E is set and +0x15F is not, or if +0x175 is set and +0x176 is not. Checked. The bytes' meanings are inferred (+0x15D enabled; the pairs "needs authorisation / authorised" and a second requirement / met, after `script.ldf`'s "installed, enabled and authorized").
- **Edge cases:** no such add-in, or "": 0. Offers are not searched. Checked.

### GetPlayTime (830)
- **Handler:** 0x00666B40 (not shared, no arguments). app+0xD0; `ScriptBackend::ForScript_GetPlayTime` (0x0069D340). Checked.
- **Does:** reads the game clock (`g_eclipseGame`+0xB4, vtable +0x14, a float truncated to whole seconds), adds the change since the last sample (+0x84) to the total (+0x88), stores the sample and pushes the total. Checked. The play time accumulates only when asked for; that the clock is wall time and +0x88 is saved is inferred.
- **Edge cases:** no application or no app+0xD0: −1. Checked. `LogStoryEvent` also calls the backend. Used by `stats_core_h.nss`.

### BuyDownload (831)
- **Handler:** 0x00666B80 (not shared). Pops the string. Checked.
- **Does:** if the string is non-empty and the online features object's +0x6D24 is set, sends a `BuyDownloadMessage` (vtable 0x00AEE5E0; `PRCGUIShared` handles it, `dev/messages.csv`) carrying the offer id to +0x6D24 (`Messages::Send`), and pushes 1; else 0. Checked.
- **Edge cases:** "" or no receiver: 0. 1 means sent, not bought. No shipped script calls it.

### SaveGamePostCampaign (832)
- **Handler:** 0x00660BE0 (not shared, no arguments). Sends a `SaveGameMessage` (0x00B00098) with slot −1, kind 3 and no name to `SaveLoadProxy` (app+0x150). Checked. Kind 3 is the "fourth kind" of master's `dev/SAVES.md` ("Asking"); what the proxy does with it was not read. Called once, by `epi100ar_coronation.nss`.

### IsPRCAvailable (836)
- **Handler:** 0x0066D930 (not shared, no arguments). Checked.
- **Does:** pushes 1 if the add-in list (`0x00C32DC0`) has any record, or the online features object's offer list (+0x6D1C, records of 0x1D0 bytes) has any; else 0. Checked.
- **Edge cases:** no `g_eclipseGame`: 0. Any add-in counts, enabled or not, the core DLC included. No shipped script calls it. Checked.

### LogStoryEvent (837)
- **Handler:** 0x00666DA0 (not shared). Pops nEventID; app+0x14C; `ScriptBackend::ForScript_LogStoryEvent` (0x00643BF0)(id, L""). Checked.
- **Does:** in the in-memory save record's GFF4, appends a struct to `SavegameStorySoFarEventList` (16970) with `EventID` (16971) = the id, `GameTime` (16972) = `GetPlayTime`'s total, `UTC` (16973) = `_time64`, `Screenshot` (16974) = "". Checked; the parent struct got through `0x006431B0` (`SavegameStorySoFar`, 16008, inferred).
- **Edge cases:** nEventID ≤ 0, or no app+0x14C: nothing. No dedup. Three shipped scripts call it. Checked.

### CloseQuestGroup (846)
- **Handler:** 0x00663F00 (not shared). Pops the string; the journal (`0x005D5810`: the module's +0x490). Checked.
- **Does:** if the string names a plot group (`0x006EBAC0`: the journal's +0xA0 list, each entry's name at +0x58), `0x006F5DA0(name, 0)` closes the group; else `0x006F5380(name, 0)` closes one plot. Checked; that "close" moves the quest(s) to the completed list is `script.ldf`'s, inferred (the two not read in full).
- **Edge cases:** no module or journal: nothing. An unknown name goes to the single-plot path (its miss not read). Checked.

### RemoveQuest (850)
- **Handler:** 0x00663FE0 (not shared). Pops the string; the journal as above. Checked.
- **Does:** if the string is **not** a plot group, `0x006EF5E0(name)` removes the plot from the journal; a group's name does nothing. Checked; the removal itself not read.
- **Edge cases:** no journal: nothing. No shipped script calls it. Checked.

### OpenJournalPRCTab (852)
- **Handler:** 0x00666CB0 (not shared). **Pops the string only when paramCount > 0**; sends `GUI::GoToJournalPRCTabMessage` (0x00B02098) with it to the GUI system (app+0x158). Checked.
- **Edge cases:** no check of the offer id or of the GUI pointer. No shipped script calls it. Checked.

### IsPRC (857)
- **Handler:** 0x00652260 (not shared). Pops the string and the int and pushes 0. Checked: deprecated, as `script.ldf` says.

## Not read
- What reads module +0x340 (the minimap link), and whether it is saved.
- How the plot-action entries are made, what +0xA4 and +0x8C mean, and the ArmyControl movie's use of the list.
- `ClientOptions+0x550`, the string at `[0x00BF4890]`, and `g_eclipseGame+0x251`.
- 0x00759190 and 0x00757CF0 (a property applied to and taken off the wearer, by IPType), 0x00759880, and item +0x6D8's meaning.
- 0x00755270's exact requirement arithmetic (materialrules' ReqPropNMod), and who fills item +0x6EC (0x006AEC10's callers).
- 0x006AD0E6/0x006ACFE5, the other reader of creature +0xC70.
- 0x0069B900/0x0069B830's filters (the AoE effects switched with interactivity), and creature 0x006A90D0.
- What the statue's skipped update block holds, and whether a statue refuses new commands. The physics controller made by 0x006B1ED0.
- `AIMaster::AddEventDeltaTime`'s arguments in full.

- Where the achievement records' unlocked bytes and counts are kept between sessions (a profile file, or the SDK's offline award data); the loader `0x0047EC00` past the table lookup; `0x0047A2B0` (a reset of all records).
- The online service's classes (no RTTI on `[0x00C1D9E4]` or its interfaces); the sign-in that sets +0xDE; `0x0054F810` and +0xDC in the telemetry gate.
- `0x006F5DA0`, `0x006F5380`, `0x006EF5E0` (the journal's close and remove) and `0x0075F7E0` (what `ShowAllCodexEntries` sets).
- What `SaveLoadProxy` does with save kind 3.
- The writer of creature+0xB20 (`IsMoving`); who calls the roam check `0x006A2D30`.
- `0x006A2B80`, the map's other test beside `ShowAsAllyOnMap`'s bool; the item code that honours +0x640.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

### IsItemDroppable (295)
- **Handler:** 0x00668A20, the large getter handler shared by many commands (switch on the command number, which is its second parameter); case 295 (0x127) is the jump table's default, 0x00668BF7 (checked: byte table at 0x668CFC, index 0x87 is entry 4 = 0x668BF7).
- **Described in:** `dev/COMBAT.md` (the loot paragraph: "the item's +0x4B0, `IsItemDroppable`'s byte").
- **Does:** `GetObjectById`; if the object answers as an item (virtual +0x14), pushes `byte [item + 0x4B0] != 0`.
- **Edge cases:** an invalid object pushes 0 (checked). An object that is not an item falls through to the other kinds' cases and, for 295, none match, so it pushes 0 (checked). The handler pops only the object for this command (extra pops happen only for the numbers that take more arguments). The same slot with +0x4B1 is another command (case 0x281).

### GetFollowerWouldBeLocation (728)
- **Handler:** 0x00666650 (not shared). It gets the party object (`[CClientExoApp+0xD0]+0x58`, 0x005D57C0) and calls 0x00632BB0(followerId, &position), which fills in the leader id (`app+0xD0` +0x5C, 0x005D5CA0) and calls 0x00632640, which calls 0x00760810 with arrival = 0.
- **Described in:** `dev/PARTY.md` (the follow formation, "`GetFollowerWouldBeLocation` ... is the walking answer"), `dev/LOAD_EVENTS.md` ("Walking follow").
- **Does:** builds a new script location (0x30 bytes). If 0x00632BB0 succeeds, it holds the position, the area id (leader creature +0x254) and the orientation (leader +0x130); pushed as engine structure type 1.
- **Edge cases:** if the follower is the leader (`0x00632640` compares the two ids first), or it fails, the location is the default one, a zero position, and no error (checked: 0x006326AA returns false). The orientation and area are copied from the leader creature (`GetCreatureById` of the leader id, 0x005D5CC0) with no null test, so with no leader this would read a null (inferred, not exercised). The location is freed after the push; a push failure returns -638 (checked). A failed pop returns -639 (checked).

### GetCreatureBodyBag (827)
- **Handler:** 0x0065F3D0 (not shared); `GetCreatureById`, reads the dword at +0x790, pushes it as an object (0xA2D980).
- **Described in:** `dev/COMBAT.md` ("The body bag": creature +0x790 = the bag), `dev/SCRIPT_COMMANDS.md` (the `CCCreature` field table).
- **Does:** returns the creature's body bag placeable, linked by `SpawnBodyBag`.
- **Edge cases:** an invalid or non-creature object pushes OBJECT_INVALID, -1 (checked, esi starts at -1). A creature that has no bag (never died) holds the field's own value, not necessarily -1 (inferred from the field; not read in play). Push failure is -638 (checked).

## Not read

- The loot-system callers of +0x4B0; which engine paths set a body bag's link before death.
