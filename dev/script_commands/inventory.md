# Script commands: inventory (DAOParagonEngine #27 baseline)

Read 2026-10-18 by lib-agent-2 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with capstone, `bitm_base` through ghilana, `script.ldf` and the toolset's scripts. Builds on `dev/ITEMS.md`, `dev/PLACEABLES.md` and `dev/PARTY.md`. "Checked" is read in the disassembly; "inferred" is marked. Not seen in play.

## Common ground

- **Lookups** (`g_clientExoApp` methods): `0x005D5870` item by id (game-object type byte 7, then cast slot +0x14 `CCItem`); `0x005D58C0` `GetCreatureById`; `0x005D5B40` `GetPlaceableById`; `0x005D5BE0` `GetObjectById`; `0x005D5C20` item possessor by id (cast slot +0x34 `ItemPossessor`: `CCCreature`/`CCPlaceable` at +0x440, `CCParty` at +0x70, `Store`). The wrong kind of object fails the lookup, so a handler takes it as "no object". Checked.
- **Handler return:** every handler here returns 0, or −0x27F (−639, a failed pop: the script fails) when an argument cannot be popped, or −0x27E when the push fails. No handler fails because the object is wrong; a getter pushes its default. Checked.
- **Item fields:** +0x464 u16 the base item row (`bitm_base`, GDA table 6); +0x4FC MaterialType; +0x5F0 the stack size; +0x660 the ability id; +0x2C4 byte plot (`IsPlot`); +0x4E8 the `ItemLocation*` that holds it (null when none); attribute +0x5D0 (its bool at +0x5DC): an item being destroyed (as in `dev/ITEMS.md`); attribute +0x650: an "infinite stack" flag (inferred from its use: such a stack counts as 0x7FFFFFFF when taken from). Item vtable +0x1CC sets the stack size, +0x198(0) takes the item out of its holder and destroys it (`dev/ITEMS.md`). Checked.
- **Stacks:** an item stacks (`0x00753C60`) when not being destroyed and `bitm_base.StackSize` (hash 0x39974122, `0x00753070`) > 1. Checked. `StackSize` in the shipped table: 99 arrows, bolts, quick items (potions, poultices), herbalism, poison and trap-making items, Generic Stackable; 10 gems; 1,000,000 copper (row 17) and crafting recipes; 0 runes and Shale's arms; 1 the rest.
- **The party's inventory is one object.** `CCParty` +0x20C is an `Inventory` (vtable 0x00B1B974). When a creature joins the pool (`0x0063A020` → `CCCreature 0x006A8C10` at 0x0063A10E), its own `Inventory` (attribute creature+0xBF0) is replaced by the party's: its items are moved in (`0x00762E90`), its money is added to the party's, the pointer is set, and the old `Inventory` is deleted. So every pool member's +0xBF0 is the same shared backpack and purse. Checked. What leaving the pool does to +0xBF0 was not found (no other caller of 0x006A8C10).
- **An `Inventory`** has three `ItemLocation`s: +0x8 the backpack (a `ListItemLocation`, vtable 0x00B2308C; its max slot count is attribute +0x28), +0x18 plot items (an item with +0x2C4 set goes here), +0x28 money (base item 17). Checked in `Inventory`'s remove (`0x00740300`). Equipped items are not in it: they are in the `Equipment` at creature+0xBE0 (attribute), per creature. A placeable's inventory is attribute +0x678.
- **Money location** (`Inventory`+0x28): attribute +0x90 the total in copper, with +0x60 gold (/10000), +0x70 silver, +0x80 copper derived by `0x007C9F50`. Add `0x007C9FE0(n)`, subtract `0x007CA020(n)`: no bounds on either. Checked.
- **Taking an item from its holder:** `ScriptBackend::ForScript_RemoveItem` (`0x00762100`, `(result*, item, n)`). With no holder (+0x4E8 null) the result is the item itself. Otherwise the holder's vtable slot 2: for a list (`0x007619E0`), n = −1 or n > stack takes the whole stack; 0 < n < stack splits off a copy of n (`0x00759740`) and takes that; **any other n (0, −2, …) takes the whole item**. The taken item's +0x4E8 is cleared and the holder's owner is told (possessor slot +0x20). `Inventory`'s slot 2 routes plot items to +0x18 and copper to the money location. Checked.

## The GUI and messaging part: shared facts

Read 2026-10-18 in GOG's `DAOrigins.exe` 1.05 (base 0x400000) with capstone, for the Paragon port's baseline. "Checked" is read in the disassembly; "inferred" is not. Shared conventions: every handler is `__stdcall (command, argCount)`, `ret 8`; a failed pop returns -639 (0xFFFFFD81), which fails the script. `app` is `g_clientExoApp` (0x00C327EC), `gui` is `app+0x158` (`GUI::DragonAgeGUISystem`), `party` is `app->0x005D57C0()` (CCParty, may be null), `module` is `CClientExoApp::GetModule` (0x005D4FC0). Messages go out through `Messages::Send` (0x004D20C0). "Full-screen GUI" means `SetFullScreenGUIMessage(name)` to `gui` (handler 0x0060B260): the scene id by name (gui slot 12), an unknown name does nothing, then `SetGameMode(3)` (GUI mode, whose enter pauses: dev/TURN_BASED.md) and `GameModeGUI` 0x007183C0 opens the scene, which signals `EVENT_TYPE_GUI_OPENED` (84) with the id (dev/GUI.md). The floaty manager is a lazily made singleton (getter 0x00523880, global 0x00C343C0) that feeds the `FloatyLayer` movie through its `Batch*` calls.

There is no dev/CAMERA.md in DAOLib; the SetTrainingMode reading reused here is `%TEMP%\lib-agent-2-fade.md`. Correction to it: SetTrainingMode's object is `module+0x498` (the module's tutorial control), not `app+0x498` (0x0066371F-0x00663751, checked).

### RemoveItem (403)
- **Handler:** 0x0066C400 → `ScriptBackend::ForScript_RemoveItem` (0x00762100), then the taken item's vtable +0x198(0).
- **Does:** takes n of the item from wherever it is (the common "taking" above) and **destroys what was taken**: the whole item, or the split-off copy, the rest staying in its stack. Checked.
- **State:** the holder's list, item+0x4E8, item+0x5F0 when split. Checked.
- **Edge cases:** n defaults to −1 (popped only with two arguments); n = 0 or < −1 destroys the whole stack; n ≥ stack the whole stack. An item being destroyed (+0x5D0): nothing. An item on no holder: destroyed. An equipped item: taken from the `Equipment` location (inferred: the unequip, its stats and `EVENT_TYPE_UNEQUIP`, through the location; `dev/ITEMS.md`). A plot item: no exemption. Checked except as marked.

### RemoveItemsByTag (379)
- **Handler:** 0x006660C0. Pops the possessor, the tag, n (−1 unless three arguments). Calls the possessor's vtable slot 2 (+0x08), then `ScriptBackend::ForScript_RemoveItemsByTag` (0x007621E0) destroys every taken item (+0x198(0)).
- **Does:** a creature (0x006B3410): the backpack (+0xBF0, `Inventory` slot 3 0x00740430) first, then, **only when n ≠ −1** and fewer than n were taken, the creature's equipment (+0xBE0) for the rest. `Inventory` slot 3: the backpack, then the plot items with the same n when n = −1 or the backpack gave nothing. A placeable or the party (0x00763070): its inventory's slot 3. Checked.
- **State:** the possessor's lists; for a party member the shared party backpack. Checked.
- **Edge cases:** `RemoveItemsByTag(o, tag)` never takes equipped items; with an explicit n it can. Stacks count by their size and are split at the end (inferred from `CountItemsByTag` and the list's take). Not a possessor (an item, a waypoint): nothing. Checked except as marked.

### CountItemsByTag (380)
- **Handler:** 0x00666210; possessor vtable slot 3 (+0x0C), which counts with `0x007615F0`.
- **Does:** sums, over items whose tag (+0xB8) equals sTag, the stack size for a stacking item and 1 otherwise. A creature (0x006A2EA0) counts its backpack (+0xBF0) **plus its equipment** (+0xBE0); a placeable or party (0x007630D0) its inventory. Checked.
- **Edge cases:** not a possessor: 0. For a party member the count is of the whole party's backpack, plus that one creature's equipped items. Whether the `Inventory` walk includes the plot and money locations is inferred (yes, through its iterator). Checked except as marked.

### GetCreatureMoney (408)
- **Handler:** 0x0066A980.
- **Does:** creature+0xBF0 → `Inventory`+0x28 (money location) → attribute +0x90, the copper total. Checked.
- **State:** for a party member the shared party purse (`CCParty`+0x20C). Checked.
- **Edge cases:** not a creature (a placeable, an item): 0. Default `OBJECT_SELF` is the compiler's. Checked.

### SetCreatureMoney (409)
- **Handler:** 0x0066AA00. Pops nCoppers, the creature, bNotify.
- **Does:** reads the old total, subtracts it (`0x007CA020`) and adds nCoppers (`0x007C9FE0`) to the money location: the total becomes nCoppers, gold/silver/copper re-derived. Then, when the creature is in the active party (`CCParty::IsInParty`, 0x00631830), bNotify is set and the total changed, the party (+0x330, `0x00629690(new − old)`) is told (inferred: the on-screen money notice). Checked.
- **Edge cases:** no clamp: a negative total is stored (checked in the add/subtract; the attribute's own setter not read). Not a creature: nothing. A pool member not active: changed silently. Checked.

### GetItemPossessedBy (675)
- **Handler:** 0x0065CBA0. Only when the implementer's +4 is 1 (inferred: the server's instance; otherwise `OBJECT_INVALID`). Possessor vtable slot 1 (+0x04), find by tag (`0x007613A0`); pushes the item's id (attribute +4).
- **Does:** a creature (0x006A2E60): the first match in its backpack (+0xBF0), else in its equipment (+0xBE0). A placeable (0x0074CFA0): its inventory (+0x678). The party (0x0062EC80): the party inventory. Checked.
- **Edge cases:** no match or not a possessor: `OBJECT_INVALID`. For a party member the backpack searched is the party's, so any member finds an item another member carries in the backpack; equipped items only on that creature. First match is the list's order (not read). Checked except as marked.

### GetActiveWeaponSet (167)
- **Handler:** 0x0065CD20 → `ScriptBackend::ForScript_GetActiveWeaponSet` (0x00797410): `Equipment`(+0xBE0)+0x10, an attribute.
- **Does:** pushes the active weapon set (0 or 1). Checked.
- **Edge cases:** not a creature, or no `Equipment`: 0, as `script.ldf` warns. Checked.

### SetCanChangeEquipment (391)
- **Handler:** 0x00665C80.
- **Does:** sets creature attribute +0x5F4 (its bool at +0x600) to `bCanChange != 0` (vtable +0x54). Checked.
- **State:** creature+0x5F4. Read by `CCCreature::ForScript_CanUseItem` (0x006AE91D, where set it skips a check against the creature's stats +0x7C4) and five other creature functions (0x006B23B0, 0x006B4070, 0x006B9ED0, 0x006BA890, 0x006BAFE0), not read. What the GUI does with it is inferred (the paperdoll locked).
- **Edge cases:** not a creature: nothing. Equip/Unequip by script were not seen to test it (not read). Checked except as marked.

### GetMaxInventorySize (404)
- **Handler:** 0x0066C2A0.
- **Does:** creature `OBJECT_INVALID` (the default) becomes `[app+0xD0]+0x5C` (inferred: the player's creature). Pushes creature+0xBF0 → `Inventory`+0x8 (backpack) → attribute +0x28. Checked.
- **Edge cases:** not a creature, or no backpack: −1. For a party member it is the shared backpack's size. Checked.

### SetMaxInventorySize (405)
- **Handler:** 0x0066C340 → `ScriptBackend::ForScript_SetMaxInventorySize` (0x007CA4F0): backpack attribute +0x28 set.
- **Does:** clamps nMaxSize to 1..125 (≥ 0x7D → 125, ≤ 1 → 1) and sets it. Same default creature as 404. Checked.
- **Edge cases:** no items are dropped or moved when the size falls below the count (checked: the setter only sets the attribute; its observers not read). `module_core` adds 10 per `gen_im_misc_backpack` acquired by a follower. Not a creature: nothing. Checked.

### StorePartyInventory (345)
- **Handler:** 0x00665CF0 → `ScriptBackend::ForScript_StorePartyInventory` (0x00638D10), `this` the party (`[app+0xD0]+0x58`).
- **Does:** for each item in the party backpack (`CCParty`+0x20C → `Inventory`+0x8), records in the party's map +0x434 (item id → {party's id, 0, 0}); then, with global flag 0x00C371B4 | 0x100, moves the whole backpack into the placeable's inventory (+0x678; `0x00762E90`), refreshes both (vtable +0x24) and `CCObject::SetObjectInteractive(placeable, 1)`. Checked.
- **Edge cases:** **plot items (+0x18), money and every equipped item stay with the party.** Target not a placeable: nothing. Stacks may merge with the placeable's (inferred, `0x00762E90` not read). Checked except as marked.

### StoreFollowerInventory (346)
- **Handler:** 0x00665D50 → `ScriptBackend::ForScript_StoreFollowerInventory` (0x00638EB0). Pops oFollower then oTarget, a placeable.
- **Does:** **moves the follower's equipment** (+0xBE0), not a backpack: each item recorded in the party's map +0x434 as {follower's id, slot, set} (from `0x0062EE50`, inferred: the item's place in the `Equipment`), then all moved into the placeable (+0x678), both refreshed, the placeable made interactive. Checked.
- **Edge cases:** the follower's backpack is the party's and is untouched. oFollower not a creature or target not a placeable: nothing. Unequip events and stat removal come with the move (inferred: through `Equipment`'s location callbacks). Checked except as marked.

### RestorePartyInventory (347)
- **Handler:** 0x00665DF0 → `ScriptBackend::ForScript_RestorePartyInventory` (0x00639070).
- **Does:** collects the placeable's items whose map entry (party+0x434) names the party, then moves each (`0x00762C40`, the item's own slot) into the party backpack; flag 0x100 meanwhile; both refreshed. Checked.
- **Edge cases:** items the player added to the chest, or a member's stored equipment, stay. The map entries are not erased (checked: no erase in the function), so a later restore from another chest would also take those items if they were there. An item missing from the chest is simply not restored. Checked.

### RestoreFollowerInventory (348)
- **Handler:** 0x00665E50 → `ScriptBackend::ForScript_RestoreFollowerInventory` (0x00639290).
- **Does:** for the placeable's items whose map entry names this follower, places each back at its recorded (slot, set) in the follower's `Equipment` (`0x00762990`). Checked.
- **Edge cases:** the slot is not re-validated beyond `0x00762990`'s `0x007611A0` (can the location take it); if refused the item stays in the chest (checked in 0x00762990). The map entries are not erased. `Prelude`: `StoreFollowerInventory(oPC, footlocker)` and its restore use this for the hero. Checked.

### MoveItem (349)
- **Handler:** 0x00665EF0 → the target possessor's `ScriptBackend::ForScript_MoveItem` (0x007633F0). Pops oSource, oTarget, oItem.
- **Does:** both oSource and oTarget must be possessors and oItem an item; then the item goes to the target's inventory (possessor slot +0x18: creature +0xBF0, placeable +0x678, party +0x20C) through `0x00762C40`: a slot chosen (`0x00761340`), the **whole stack** taken from its actual holder (`ForScript_RemoveItem(item, −1)`) and added (`0x00762010`), the target's owner told. Global flag | 0x10 meanwhile. Checked.
- **Edge cases:** **oSource is only checked to exist**: the item is taken from wherever it is, not from oSource. Moving to a party member puts it in the shared backpack. If the target cannot take it (`0x007611A0`), nothing moves. An equipped item is taken out of its `Equipment` (inferred: unequipped). Checked except as marked.

### GetWeaponsUnsheathedStatus (160)
- **Handler:** 0x00668A20 (shared), creature branch case 0 at 0x00668B41 → `0x007C34C0` on the combat block (creature+0xC60).
- **Does:** by the block's +0x1C (inferred: the weapon style): −1, 0 or 2 → byte +0x08; 1 or 3 → +0x08 and +0x09 both; anything else 0. Inferred: +0x08 the main hand drawn, +0x09 the off hand, and 1/3 the dual-wield styles. Checked except as marked.
- **Edge cases:** not a creature, or no combat block: 0. Checked.

### GetTrapDetectionDifficulty (411)
- **Handler:** 0x00668A20 (shared), placeable branch at 0x00668C9F: `CCPlaceable`+0x644.
- **Does:** pushes the int at +0x644, loaded from the template's `TrapDetection` (0x007505E6). Checked.
- **Edge cases:** not a placeable (a trigger, a creature): 0. `SetTrapDetectionDifficulty` (412) has no handler in this exe, so it is the template's value. Checked.

### GetTrapDisarmDifficulty (413)
- **Handler:** 0x00668A20 (shared), placeable branch at 0x00668C97: `CCPlaceable`+0x648.
- **Does:** pushes +0x648, from the template's `TrapDisarm` (0x007505C7). Checked.
- **Edge cases:** not a placeable: 0. 414 is not implemented either. Checked.

### SetTrapDetected (416)
- **Handler:** 0x0065A740 → `CCPlaceable::ForScript_SetTrapDetected(bDetected, 0)` (0x0074FBD0).
- **Does:** if the value differs from +0x5EC (or the second argument forces it), stores bDetected as given (not normalised to 0/1), `0x006965E0` (inferred: replicate the change), then with a nonzero value `0x0074E5E0`, else `0x0074E640`. Those hand the current state's walk-grid patch (+0x690[+0x658], `dev/PLACEABLES.md`) and the position to the pathfinder (`app+0x118`, `0x00A46E60`). Inferred: a detected trap's patch is marked so paths avoid it, and taken out again. Checked except as marked.
- **Edge cases:** not a placeable (a trigger trap): nothing. No event raised here (checked). `IsTrapDetected` (415) has no handler. Checked.

### DisplayPortraitMessage (770)
- **Handler:** 0x00661F40 (own). Builds `GUI::InvokeCallbackMessage` (ctor 0x00652570) and sends it to `gui`; its handler 0x0060ED40 finds the movie and `GuiMovie::Invoke`s.
- **Does:** pops object, string, colour (default 0xFFFFFF). Calls `SharedLibrary.Portrait.DisplayPortraitMessage` in the movie `Portraits` (scene 0x2B, layer 1) with one argument string `"<object id> <colour> <message>"` (decimal, space separated). Checked.
- **State:** none in the engine; the movie keeps the message. Duration and fade are the movie's ActionScript (not read).
- **Edge cases:** no check of the object: an invalid or non-party object goes to the movie as its id, which picks (or finds no) portrait (inferred). No `gui` → nothing. `Portraits` not loaded → nothing (FindMovie fails, checked). No mode check (checked).

### DisplayFloatyMessage (744)
- **Handler:** 0x00662180 (own). `DisplayFloatyMessage` message (ctor 0x0064F5D0: +4 id, +8 text, +0x24 style, +0x28 colour, +0x2C duration) sent to the object's +0x18; `CCCreature` handles it at 0x006A73E0, `CCPlaceable` at 0x0074BE30; both call the floaty manager's add (0x0063FAF0).
- **Does:** pops object, string, style (default 0), colour (default 0xFFFFFF), and the duration only when 5 arguments came (else 0.0, not ldf's 0.5; compiled calls always push 5, inferred). Checked.
- **Shown only when** (0x007C4AE0, checked): the object's byte +0x1AC is set; for a creature, its byte +0xCFC too (meaning not read); and its position (+0x120) is within 50 m, 2D (squared distance ≤ 2500), of the current area layout's streaming reference point (layout +0x550, the camera's position: Addresses.h). Otherwise dropped silently.
- **Creature:** styles 1 and 2 (FLOATY_HIT, FLOATY_CRITICAL_HIT) are shown for a party member when the option `ShowPartyDamageNumbers` (ClientOptions +0xD8) is on, else only when the creature's bool attribute +0x4E8 is set and `ShowHostileDamageNumbers` (+0xE8) is on. Style 0 always passes. Checked. **Placeable:** the style is forced to 4 (checked).
- **Manager add** (checked): at most 6 floaty messages (floaty type 2) per object at once, a 7th is dropped; style 4 is dropped while the object already has a style-4 one. Duration is passed to the movie; for styles 1/2 the movie ignores it (ldf).
- **Edge cases:** invalid object or object types other than creature/placeable → nothing (no handler, inferred). ClientOptions is the global 0x00C32938.

### DisplayStatusMessage (747)
- **Handler:** 0x006623E0 (own) → gui slot 23 (0x005239D0) → floaty manager 0x0063C260.
- **Does:** pops string and colour (default 0xFFFFFF). Invokes `FloatyLayer.DisplayStatusMessage(text, colour)` on the movie named by the global at 0x00C3729C (the floaty layer, scene 0x2F, layer 0); the colour goes as a number, made unsigned (2^32 added) when negative. Checked.
- **State:** none in the engine. Position (screen centre) and duration are the movie's (not read).
- **Edge cases:** movie not loaded → nothing (checked). No mode check, no queue in the engine (checked).

### SetQuickslot (748)
- **Handler:** 0x0066B690 (own) → `CCCreatureStats` (creature +0x7C4) 0x0071F230 (slot, ability, tag, item reference).
- **Does:** pops object, slot, ability, and the tag only with 4 arguments. Works on the stats' party data (+0x1B4; made when the creature joins the party, 0x0071CCD0 from CCParty 0x0063A020). Ability 0 clears the slot (0x0062C0F0). A tag finds the item in the creature's inventory (0x006A6400 on +0x5C, by ability and tag); not found → nothing. An ability whose `ABI_base` `UseType` is 3 (passive) is refused. Slot -1: nothing if the ability (and item) is already on the bar, else the first free slot (0x0062AD80), searched from slot 9 for skills (`abilitytype` 3). A slot above 49 (unsigned) is refused. Writes the entry into the current bar's map (party data +0x14C + 20 × bar), sets the dirty byte +0x2C. Checked.
- **State:** party data bars (5 bars × 50 slots); with a tag and a tutorial control, signals training event 1064 (`EVENT_TYPE_TRAINING_QBAR_ITEM_EQUIPPED`, 0x00693290). For an item ability (type 4) not yet linked, 0x0071EF00 runs (not read).
- **Edge cases:** non-creature or no party data (never in the party) → nothing (checked). Writes the current bar only.

### GetQuickslot (330)
- **Handler:** 0x0066B800 (own) → `ForScript_GetQuickslot` 0x0071C620.
- **Does:** returns the ability id in slot n of the current bar (party data +0x1B0), 0 if empty. Checked.
- **State:** reads only; the lookup is the map's find 0x005D0AB0 (whether it inserts an empty entry is not read).
- **Edge cases:** invalid object, non-creature, no party data → 0 (checked). Slot out of range → 0 (no entry, inferred). Other bars are not reachable.

### SetQuickslotBar (328)
- **Handler:** 0x0066BE50, shared with 296, 329, 333, 335, 642, 643, 645, 676, 698, 758, 764, 805, 813, 839; case 0x148 at 0x0066BF1B → 0x00719AE0.
- **Does:** sets the party data's current bar (+0x1B0) and the dirty byte, when the bar differs and is below 5. Checked.
- **State:** party data +0x1B0, +0x2C.
- **Edge cases:** the test is signed (`jge 5`): a negative bar is stored, and later reads index outside the bars (checked the test; consequence inferred). Object default 0 and int default 0 (popped only with 2 args). Non-creature → the handler's placeable branch, which has no case 328 (nothing). No party data → nothing.

### LockQuickslotBar (329)
- **Handler:** 0x0066BE50 (shared, above), case 0x149 at 0x0066BEF6 → 0x00719AC0.
- **Does:** sets the party data's own attribute (its +0, slot 21 Set) to `bLock > 0`. Checked.
- **State:** party data +0 (observers notified; the quickbar movie reads it, inferred).
- **Edge cases:** negative bLock unlocks (checked). No party data → nothing.

### ShowFloatyIcon (749)
- **Handler:** 0x00662DB0 (own) → floaty manager 0x0063FCF0 (creature, icon).
- **Does:** removes the creature's existing icon floaty (type 3, 0x0063DD00), then, for a non-empty name, makes a new type-3 floaty and sets its texture (0x0063E530, 0x007F4F60). Checked in outline.
- **State:** floaty manager's list; one icon per creature.
- **Edge cases:** empty string hides (checked). Non-creature → nothing (GetCreatureById, checked). No 50 m test here, unlike messages (checked: none called).

### GetCanLevelUp (633)
- **Handler:** 0x0066B8A0 (own).
- **Does:** returns the creature's bool attribute +0xB84 (its byte +0xC). Checked. It is written by `CCCreature::UpdateCanLevelUp` (0x006ACA10), reached from command 0x00662EA0's handler (inferred to be SetCanLevelUp).
- **State:** read only.
- **Edge cases:** invalid or non-creature → 0 (checked). A failed pop returns -638 here, not -639 (checked).

### GetAutoLevelUp (384)
- **Handler:** 0x0066B980 (own) → `CCCreatureStats::GetAutoLevelUp` 0x0071A1F0.
- **Does:** returns party data +0x30 (an int attribute). Checked.
- **State:** read only.
- **Edge cases:** invalid, non-creature, or no party data → 0 (checked). Values: 0 off, 1 on, 2 forced (scripts set 2 on followers; joining the party keeps 2 and otherwise copies the party's own setting, party +0x84, 0x0063A09F, checked).

### SetAutoLevelUp (385)
- **Handler:** 0x0066B900 (own) → `ForScript_SetAutoLevelUp` 0x00719B10.
- **Does:** sets party data +0x30 to the int as given (Set, notifies). Checked.
- **State:** party data +0x30.
- **Edge cases:** no range check (checked). No party data → nothing, so it does not stick on a creature never in the party (checked; on joining, party +0x84 decides unless 2, but the data is made after that copy, 0x0063A11C, so a first join takes the creator's defaults, inferred).

### GetMaxLevel (840)
- **Handler:** 0x00662F00 (own).
- **Does:** returns party +0xC4. Checked. CCParty's constructor (0x0063A3C0) sets it once (0x0063B765-0x0063B801): the cap is `properties.gda` (table 98) row 19 (`PROPERTY_SIMPLE_EXPERIENCE`) column `Max`; the value is the last row index of `TABLE_EXPERIENCE` (164) whose `XP` is at most the cap. Checked; whether the row index equals the level is the table's (inferred).
- **State:** read only.
- **Edge cases:** no party → 0 (checked).

### ShowChantersGUI (779)
- **Handler:** 0x00662F40 (own) → the journal (0x005D5810) 0x006F46A0(board), then full-screen GUI `Chanters`.
- **Does:** rebuilds the board's list: table 115 by the board's row (`Texture`, `Label`, `HeaderStrID`), then table 116's quests with that `BoardID` (`VisibleFlag`, `AcceptedFlag`, `PlotRef`) into the data tree. Then opens the `Chanters` scene: GUI mode 3, paused, `EVENT_TYPE_GUI_OPENED`. Checked (columns by their hashes; the filter by flags inferred).
- **State:** journal +0xE8 list; game mode.
- **Edge cases:** an unknown board leaves an empty list (inferred). Accepting goes through `Journal::AcceptChantersQuestMessage` (0x006EB850). No close event read.

### ShowGeneralScoreboardGUI (788)
- **Handler:** 0x00663030 (own).
- **Does:** full-screen GUI `GeneralScoreboard` (scene 0x18, `GUI::GUIGeneralScoreboard`): GUI mode 3, paused, `EVENT_TYPE_GUI_OPENED`. Checked.
- **State:** game mode, the GUI vector.
- **Edge cases:** no argument; no close event read (closing is `GUI.HideFullScreenGUI`, dev/GUI.md).

### ShowChampionshipScoreboardGUI (789)
- **Handler:** 0x006630D0 (own).
- **Does:** as 788 with `ChampionshipScoreboard` (scene 0x19, `GUI::GUIChampionshipScoreboard`). Checked.
- **State / Edge cases:** as 788.

### ShowTournamentScoreboardGUI (790)
- **Handler:** 0x00663170 (own).
- **Does:** as 788 with `TournamentScoreboard` (scene 0x1A, `GUI::GUITournamentScoreboard`). Checked.
- **State / Edge cases:** as 788.

### ShowPartyPickerGUI (794)
- **Handler:** 0x006635E0 (own). Sends `ShowPartyPickerMessage` to the current game mode (the controller's current mode object).
- **Does:** only `GameModeExplore` and `GameModeGUI` handle it (0x007183A0): `SetGameMode(12)` (PartyPicker). Checked. After its fade (`FadeToBlackLoading`, scene 0x22), the mode delivers `EVENT_TYPE_PARTYPICKER_INIT` (104) to the module at once and shows scene 0x1B `PartyPicker` (0x0076C7C4-0x0076C878, checked).
- **Closing:** Submit (`SubmitPartyChangesMessage`, 0x00767060) queues `EVENT_TYPE_PARTYPICKER_CLOSED` (71) to the module (0x00766C10), then exits; Exit alone (0x00765960) only shows the fade and sets the leaving flag, no event (checked).
- **Edge cases:** in combat, dialogue, cutscene and the rest the message has no handler and is ignored (inferred from the handler table). Whether mode 12 pauses is not read.

### SetPartyPickerGUIStatus (311)
- **Handler:** 0x00663660 (own).
- **Does:** sets party +0x74 (int attribute, Set, notifies) to the status. Checked.
- **State:** party +0x74 (0 no use, 1 read-only, 2 usable; the HUD's button reads it, inferred).
- **Edge cases:** no range check (checked). No null check of the party: with no party it reads through a null pointer (checked the missing test; crash inferred).

### GetPartyPickerGUIStatus (312)
- **Handler:** 0x006636B0 (own).
- **Does:** returns party +0x74. Checked.
- **Edge cases:** no party: same missing null check as 311.

### ShowTutorial (310)
- **Handler:** 0x00651780 (own).
- **Does:** pops the int and returns. Nothing else. Checked.
- **State:** none. **Edge cases:** a no-op in this build for every value.

### SetTrainingMode (842)
- **Handler:** 0x006636F0 (own) → `ForScript_SetTrainingMode` 0x00692E90 on `module+0x498`.
- **Does** (the fade note's reading, reused, with the reset read here): a session is dropped when `ShowTutorials` (ClientOptions +0x98) is off or its bit is set in `CompletedTraining` (+0x1C8, kept in the options); sessions above 32 are dropped. A change resets the step (+0x8C) and the expected event and argument (+0x9C, +0xA0), marks the old session done, clears every GUI highlight (0x0060EAE0 from 0x0069307B, checked), pauses in explore or combat and raises `EVENT_TYPE_TRAINING_BEGIN` (1059) through `genev_tutorial`.
- **Edge cases:** no module or tutorial control → nothing (checked). 0 ends the session.

### BeginTrainingStep (843)
- **Handler:** 0x00663760 (own) → `ForScript_BeginTrainingStep` 0x00693190 on `module+0x498`.
- **Does:** sets the expected event (+0x9C) and its argument (+0xA0), sets the step attribute (+0x8C, Set, notifies the popup), then clears every GUI highlight (`gui` 0x0060EAE0(0)). Checked. The step's text and images are the movie's (`PopupLayer.RefreshTrainingTutorial`, 0x006931E0; not read).
- **Later:** the engine's training signal (0x00693290) raises the event only when a session is on, the event equals +0x9C and the argument is 0 or equals +0xA0; it goes to `genev_tutorial` with the session as int 0 (checked).
- **Edge cases:** argument default 0 (popped only with 3 args, checked). No session check here (checked).

### AddGeneralScoreboardEntry (791)
- **Handler:** 0x00663210 (own) → scene 0x18 (gui slot 24) `ForScript_AddGeneralScoreboardEntry` 0x0072D860.
- **Does:** makes a new entry (0xD0 bytes, ctor 0x0072CDD0) with name, victories, defeats, fatalities, qualified and dead (the last two made bool), appends it to the scene's array (+0xC0) and notifies. Checked.
- **State:** scene 0x18's list.
- **Edge cases:** no cap and no command clears the list (clearing by the scene not read). No `gui` or scene → nothing (checked).

### SetChampionshipScoreboardEntry (792)
- **Handler:** 0x00663380 (own) → scene 0x19 `ForScript_SetChampionshipScoreboardEntry` 0x0072C210.
- **Does:** for entry 0–30 (unsigned), sets the fixed entry's name, winner and is-player (bools). Checked.
- **Edge cases:** id ≥ 31 or negative → nothing (checked). An id past the array's end reads the fallback pointer at 0x00C36828 (checked; the array is made by the scene, inferred to hold 31).

### SetTournamentScoreboardEntry (793)
- **Handler:** 0x006634B0 (own) → scene 0x1A `ForScript_SetTournamentScoreboardEntry` 0x0072B690.
- **Does:** for entry 0–14 (unsigned), sets the entry's name, defeated and is-player. Checked.
- **Edge cases:** id ≥ 15 or negative → nothing (checked).

### ShowCraftingGUI (355)
- **Handler:** 0x00663AB0 (own; it tests the command number is 355). `ShowCraftingGUIMessage(skill)` to `gui`, handled at 0x0060B300.
- **Does:** sets the crafting scene's (0x26) skill attribute (+0xC0); unless the scene's byte +0xBC is set, opens the full-screen GUI `Crafting`: GUI mode 3, paused, `EVENT_TYPE_GUI_OPENED`. Checked.
- **Edge cases:** skill 0 → nothing (checked). Any other id is passed on unchecked (the movie and `ABI_base` decide, inferred). Crafting itself is `GUICrafting::CraftItemMessage` (0x00726430), `EVENT_TYPE_CRAFT_ITEM` (102) not read.

### ShowSpecUnlockedNotification (722)
- **Handler:** 0x00663B80 (own) → `ForScript_ShowSpecUnlockedNotification` 0x00628E10 on party +0x330.
- **Does:** makes a notification (0x180 bytes, ctor 0x00627DD0) of type 11 with the text of `TABLE_RULES_CLASSES` (3) row nSpecID's `NameStrRef` and its `Icon`, appends it to the party's notification list and notifies (the `Notifications` scene, 0x2C, shows it, inferred). Checked.
- **Edge cases:** a row not in the table leaves empty text and icon (checked: the getter's failure skips the copy). No party null check (crash inferred).

### ShowAreaUnlockedNotification (723)
- **Handler:** 0x00663BD0 (own) → `ForScript_ShowAreaUnlockedNotification` 0x00629360 on party +0x330.
- **Does:** a notification of type 10 with the area name and the image (popped only with 2 args; else empty), appended as above. Checked.
- **Edge cases:** strings are used as given (no lookup); empty image is left to the movie's default (inferred). No party null check.

### SetGameCompletionPercentage (844)
- **Handler:** 0x00663CD0 (own) → `ForScript_SetGameCompletionPercentage` 0x0073BF80 on scene 0 (`CharacterRecord`).
- **Does:** clamps to [0, 100] and sets the scene's float attribute +0xC4 (notifies). Checked.
- **State:** the character record scene only; not kept in the save by this path (inferred).
- **Edge cases:** NaN takes 100 (the unordered compare falls to the constant, checked).

### HighlightGuiElement (848)
- **Handler:** 0x00663D30 (own) → `gui` `ForScript_HighlightGuiElement` 0x0060EA60(element, 1, arg).
- **Does:** for an element below the count of `gui`'s highlight array (+0x1B4), sets its on flag (+0x58) to 1 and its argument (+0x68) to nHighlightArg, then notifies the array. Checked.
- **State:** `gui` +0x1B4 (GUI_HIGHLIGHT_* 0–7).
- **Edge cases:** out of range or negative → nothing (unsigned test, checked). The command never turns one off: only `BeginTrainingStep`, `SetTrainingMode` and one other caller (0x0060EBE0) clear all (0x0060EAE0). Argument default 0.

## Not read

- `0x00762E90` (move all) and `0x00762010` (add at a slot): whether stacks merge into existing ones and how the slot count is enforced.
- The `Equipment` location's remove/add callbacks on a move (assumed: the unequip/equip of `dev/ITEMS.md`).
- What leaving the party pool does to a creature's +0xBF0 (no other caller of `0x006A8C10`).
- The readers of creature+0x5F4 other than `ForScript_CanUseItem`; the GUI's use of it.
- `0x00A46E60` and `0x0074E640` in full (the trap patch's effect on paths).
- The money attribute's setter (a clamp there); `0x00629690`, the party's money notice.
- `0x0062EE50` (what the follower map's slot and set words are exactly); the iteration order of `0x007613A0` (first match).
- The default backpack size (set from a template or the party at start).
## Not read (the GUI and messaging part)
- The movies' ActionScript: the portrait message's and status message's duration and placement, FloatyLayer's handling of styles, the PartyPicker, scoreboard and chanters movies, `PopupLayer.RefreshTrainingTutorial`.
- What closes each full-screen GUI raises to the scripts (only the party picker's submit was found).
- Whether `GameModePartyPicker` pauses; what scene 0x22's fade does exactly.
- The meaning of object byte +0x1AC, creature byte +0xCFC and creature attribute +0x4E8 in the floaty test.
- `0x0071EF00` (an item ability linked on SetQuickslot) and whether the bar map's find inserts.
- Clearing of the general scoreboard; the champion/tournament arrays' construction.
- `0x005D5CC0`, the object the training event is sent to.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Shared facts (checked): the VM stack object is `[0xC1D080]`; `0x00A2F6D0` pops an object id, `0x00A2F3C0` an int, `0x00A2D980` pushes an object, `0x00A2D650` pushes an int. A failed pop returns −639 (`0xFFFFFD81`); the int push returns 0, or −638 when the push fails (`neg/sbb/and 0x27E` then `+0xFFFFFD82`). `0x005D58C0` is GetCreatureById, `0x005D5BE0` GetObjectById, `[0xC327EC]` the client app.

### GetItemInEquipSlot (401)
- **Handler:** 0x00669C70 (sole).
- **Described in:** `dev/ITEMS.md` (the item commands list): pops, the BITE rule for slot 0, the destroyed-item check.
- **Does:** answers the item id in (slot, weapon set) of the creature's equipment (+0xBE0), pushed as an object.
- **Edge cases:** the slot is popped first, then the creature; the weapon set is popped only when the argument count is 3 or more, else −1 (checked). A failed pop of slot or creature fails the script (−639). A creature that is not found, is not a creature (type check against byte `0xB59409`, then the object's virtual at +0x10 gives the creature), has an empty equipment, or an empty slot gives the value read from `0xB5940C` (OBJECT_INVALID) and the script goes on (checked). A destroyed item (`+0x5D0` test, `0x00838210`) is also OBJECT_INVALID (checked). The item id is read at item+4 (checked).

### AddCreatureMoney (406)
- **Handler:** 0x0066AB30 (sole; command name COMMAND_ADDGOLD).
- **Described in:** `dev/PLACEABLES.md` (the container's OPEN_INVENTORY step) only mentions it, for a body bag with only copper.
- **Does:** adds nCoppers to the creature's money; the script's `bNotify` is popped and used to show the gain.
- **Edge cases:** the pops are coppers (int), creature (object), then bNotify (int, defaults seeded as 0 and 1 on the handler's frame); a failed pop is −639 (checked). A creature not found (`GetCreatureById` null) does nothing and returns 0 (checked). The amount is added through the creature's money object at +0xBF0 and the notify path goes through the client app's message object only when both coppers and the creature id are non-zero (checked as control flow; the exact money function not followed, inferred). The command pushes nothing, so the script sees no value.

### OpenInventory (410)
- **Handler:** 0x0065FA40 (sole; GUI::ShowContainerGUIMessage via `0x0064DC00`).
- **Described in:** `dev/PLACEABLES.md` (a container opened): USE with action 11, then OPENED, then `OpenInventory(self, user)`.
- **Does:** posts the container panel for oObject, opened by oPlayer, to the GUI.
- **Edge cases:** pops the object, then the player, then bCanAddItems only if the argument count is above 2, else it stays 0 (checked). Either object not found (`GetObjectById`, `GetCreatureById`), no client app GUI object (`app+0xD0` null), or object virtuals at +0x34 and +0x24 failing: nothing is shown, returns 0 with no script failure (checked). The result of the +0x24 virtual sets the object's flag at +0x618 to `bCanAddItems == 0` (checked, meaning of the flag inferred: the panel is read-only). Pushes nothing.

### SwitchWeaponSet (273)
- **Handler:** 0x0065CCC0 (sole), calls `CCCreature::ForScript_SwitchWeaponSet` 0x006A8FA0.
- **Described in:** `dev/ITEMS.md` (weapon-set switch): the old set's items out, the new set's in via `0x006BC7F0`.
- **Does:** changes the creature's active weapon set.
- **Edge cases:** pops the object, then the set (checked; the set always comes off the stack, so the default INVALID_WEAPON_SET (−1) is pushed by the compiled script). A creature not found does nothing and returns 0, no failure (checked). A failed pop is −639. The set is passed unchecked, its range is left to `0x006A8FA0` (not read). Pushes nothing.

### EquipItem (392)
- **Handler:** 0x0065CDA0 (shared with UnequipItem 393; EquipItem is the case with command number 0x188 on the stack, the else side is 393).
- **Described in:** `dev/ITEMS.md` (the item commands list): calls `CCCreature::Equip(item, slot, set)`.
- **Does:** equips oItem on oObject into nEquipSlot and weapon set; pushes 1 on success, 0 otherwise.
- **Edge cases:** pops the creature and the item, then slot and set together through `0x00651150` (both ints must pop, else −639; the defaults 255 and −1 are the compiled script's; checked). The creature is looked up with GetCreatureById and the item with `0x005D5870` (an item lookup, so a non-item id also fails); either null leaves the result 0 and the script goes on (checked). The result is the byte `0x006BC7F0` returns (checked). An int push failure is −638.

### UnequipItem (393)
- **Handler:** 0x0065CDA0 (shared with EquipItem 392, the other side of the compare with 0x188).
- **Described in:** `dev/ITEMS.md` (the item commands list): calls `CCCreature::Unequip(item)`.
- **Does:** takes oItem off oObject; pushes 1 on success, 0 otherwise.
- **Edge cases:** pops only the creature and the item (checked). Creature or item not found: result 0, no script failure. The result is `*0x006B7690(creature, item)` as a byte, and a temporary freed after with `0x007616C0` (checked, the temporary's meaning inferred: the equipment-slot result). Whether the item is actually in the creature's equipment is left to `0x006B7690` (not read).

## Not read
- The money function behind AddCreatureMoney (`0x00629690`, `0x007C9FE0`) and the exact notify.
- The range check on SwitchWeaponSet's set inside `0x006A8FA0`.
- What `0x006BC7F0` and `0x006B7690` refuse (slot rules, two-handed, class limits).


Shared: every handler pops with the VM stack helpers 0xA2F6D0 (object) and 0xA2F3C0 (int); a failed pop returns -639 (0xFFFFFD81) (checked). All six return 0 on every other path, so a bad object is a silent no-op, not a script failure (checked).

### MoveAllItems (350)
- **Handler:** 0x00666020 (own), `ScriptBackend::ForScript_MoveAllItems` 0x00763570.
- **Described in:** `dev/PLACEABLES.md` (a container's loot: OPEN_INVENTORY), one line: `_autoloot` containers get `MoveAllItems` instead of the loot screen.
- **Does:** pops oSource then oTarget, resolves each with `GetObjectById` 0x005D5C20, and calls `ForScript_MoveAllItems` on the target object with the source's id; a temporary list is destroyed after (0x007616C0). Checked.
- **Edge cases:** either id unresolved (OBJECT_INVALID or no such object) -> nothing moves, returns 0, no failure (checked). It uses `GetObjectById`, not `GetCreatureById`, so any object type resolves here (a container, a creature, a placeable); what a non-inventory object does is in the backend (not read). Argument order: source is popped first (checked).

### SignalDamage (359)
- **Handler:** 0x00662310 (own).
- **Described in:** `dev/COMBAT.md` (Damage and death): `Effects_ApplyInstantEffectDamage` calls `SignalDamage(oDamager, oTarget)` after setting health.
- **Does:** both ids resolved with `GetCreatureById` 0x005D58C0, then gated; if the gates pass it calls 0x00523880 on the source and 0x0063FB80 on the result. That call is inferred to be the damage notification to the target's AI (perception/threat), not read further.
- **Edge cases:** either not a creature (invalid, or another object type) -> returns 0, nothing happens (checked). Gates, all checked as code, meaning inferred: the byte at +0xC of the fields at +0x4F8 (target) and +0x4E8 (source) must both be non-zero; `0x007C4D20(target,0,0,0)` must be true and the same call on the source must be false. Otherwise a silent no-op. Never fails the script apart from a failed pop (-639).

### StartCharGen (746)
- **Handler:** 0x0066B0E0 (own).
- **Described in:** `dev/CHARGEN.md` (The creature): mode 0 opens creation (GUI screen 1, game mode 0xB), mode 1 the level-up screen (0x2E) or `AutoLevelUpMessage`.
- **Does:** pops oCreature (object), nMode and bImportEnabled (two ints), resolves the creature with `GetCreatureById`, then branches on mode; see the note for the rest.
- **Edge cases:** the pops are all required here (no argument-count test), though script.ldf gives nMode and nImportEnabled defaults; the compiler fills them in (checked, no count read). Unresolved creature -> returns 0 and opens nothing (checked). With nMode 0 only, three flags must be clear first or the handler returns 0 without opening the screen: byte +0x250 and byte +0x251 of the object `0x00402CA0` returns, and the bool at +0x5F4 of the object at `[0xC32938]` (checked as code; meaning not read, inferred to be "already in creation / a screen is up"). nMode 1 has no such gate. nMode values other than 0 and 1 are not distinguished in the part read (not read).

### PreloadCharGen (390)
- **Handler:** 0x00651770 (own), `ScriptBackend::ForScript_PreloadCharGen` 0x007C0480.
- **Described in:** `dev/CHARGEN.md` (The creature): "only calls `ForScript_PreloadCharGen`; not read further". `sp_module_start.nss` calls it at the module's start.
- **Does:** calls the backend, then returns 0 (checked). It pops nothing (the stub is `call 0x7C0480; xor eax,eax; ret 8`).
- **Edge cases:** no arguments, no failure path, no object (checked). What the backend preloads is not read.

### SetCanLevelUp (760)
- **Handler:** 0x00662EA0 (own), `CCCreature::UpdateCanLevelUp` 0x006ACA10.
- **Described in:** `dev/CHARGEN.md` (CHARGEN_AUTOLEVEL: `AL_DoAutoLevelUp`, the tactics count, `SetCanLevelUp`) and `dev/script_commands/inventory.md` (GetCanLevelUp, which reads the flag it writes).
- **Does:** pops the creature, and bCanLevelUp only if the argument count is 2 or more; then resolves the creature with `GetCreatureById` and calls `UpdateCanLevelUp` on it. Checked.
- **Edge cases:** the bool is popped but, as read, not passed to `UpdateCanLevelUp` (the call takes no argument; the flag is recomputed by the engine from the creature's state) - so the script's value is discarded (checked as code; the effect on the flag +0xB84 not re-read). Without the argument the pop is skipped and the default is not used either. Unresolved creature -> no-op, returns 0 (checked).

### ShowPopup (323)
- **Handler:** 0x00663810 (own).
- **Described in:** no master note describes the command; `dev/LIBRARY.md` and `dev/GUI.md` cover the popup layer (scene `PopupLayer`, `dao::gui::CurrentPopup`).
- **Does:** pops nMessageStrRef (int), nPopupType (int), oOwner (object), then bShowInputField and nDefaultInputStrRef only if the argument count is at least 4 and 5. If the type is non-zero, gets the GUI object `0xCF` from the data tree, sets tokens on it (the strref, the type, the input field as a bool, the default strref, and others by hash: 0x335F4422, 0x187217E1, 0x016926A0, 0x4E28B067), and if all took, copies four strings (loop to 4) and asks the GUI system (client app +0x158) to open it via 0x00606BD0. Checked as code; the tokens' meaning is inferred.
- **Edge cases:** nPopupType 0 -> nothing is shown, returns 0 (checked). GUI object 0xCF missing -> no popup, returns 0. Missing optional arguments default to 0 (checked), as the ldf says. oOwner is popped but its use was not followed (not read).

## Not read
The backends of MoveAllItems and PreloadCharGen; what the gates in SignalDamage (+0x4F8/+0x4E8, 0x007C4D20) and StartCharGen (+0x250/+0x251, +0x5F4) are named; the string-copy loop and tokens of ShowPopup; whether `UpdateCanLevelUp` reads its argument from the VM stack.
