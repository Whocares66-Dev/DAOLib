# Items as the engine keeps them

Read 2026-10-18 by lib-agent-2 (three subagents in parallel) in the GOG exe, the toolset's scripts and the game's tables, for DAOParagonEngine's next batch (items as world objects), at paragon-agent-2's request. How a creature's items are made from its template and by script, how equipping works and what it changes natively, and where the stats the combat scripts read come from. Builds on `dev/CHARACTERS.md` (the equipment and appearance slots), `src/dao/Equipment.cpp`, `dev/PROPERTIES.md` (the property modifiers equipping adds to) and the item-tint reading (materialtypes, MaterialProgress). Each point says what was read and what is inferred. Not yet seen in play.

## Items made and given

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe, with `default_hm_noble.utc` read as GFF3 for its struct IDs. "Read" means seen in the code; "inferred" is marked.

### A creature's template items: `LoadItemsFromTemplate` (340) and spawning

- `LoadItemsFromTemplate(object oCreature, string sTemplate, int bReplaceInventory)` (`0x0065d8a0`) pops the object, the string and the int, finds the creature, and calls `CCCreature::ForScript_LoadItemsFromTemplate`. Read.
- That loads the template as a GFF3 of resource type `0x7EB` (a `.utc`, inferred from the type). If it loaded (`[+0x5C]` non-zero), it saves and clears the creature's byte +0x7B4 for the duration, then destroys what the creature holds before loading the template's. Read.
  - Every item in the list at creature+0xBE0, the equipped items (inferred from what follows), gets item slot 0x198 with 0: `CCItem` slot 103 (`0x00752c90`), which removes it from its holder (`ForScript_RemoveItem`) and destroys it (`ForScript_DestroyObject`). This happens whatever `bReplaceInventory` says. Read.
  - With `bReplaceInventory`, every item in the list at creature+0xBF0, the backpack, is destroyed the same way. Read.
- Then `0x006bcc30(gff, struct, bReplaceInventory)` loads the template's lists, and `SetItemHeraldry` refreshes the appearance. Read.
- `0x006bcc30`, only while the creature's +0xC58 is 0:
  - **`Equip_ItemList`.** For each entry, a new `CCItem` is loaded from it (`0x00753800`, below). The equip slot is the entry's **GFF3 struct ID**, not a field and not `bitm_base`'s `EquippableSlots` (`0x00a36960`, the struct record's first dword). The weapon set is the entry's `SetNumber` field (−1 when absent). Read.
  - `0x006af630(item, slot, &reason)` checks the slot. If it allows it, `CCCreature::Equip(item, slot, SetNumber)`. If not, with reason 0 or 2 the item goes into the backpack list (creature+0xBF0); with any other reason it's neither equipped nor stored (inferred: lost). Read.
  - **`ItemList`**, the backpack, is loaded only when `bReplaceInventory` is set (`0x00761ec0` on the +0xBF0 list). So `LoadItemsFromTemplate(o, t, FALSE)` replaces only the equipped items and leaves the backpack alone. Read.
- `default_hm_noble.utc`'s `Equip_ItemList` struct IDs are the `INVENTORY_SLOT_*` numbers (`script.ldf`: MAIN 0, OFFHAND 1, RANGEDAMMO 2, CHEST 4, HEAD 5, BOOTS 6, GLOVES 7, CLOAK 8, NECK 11, BELT 12, BITE 13):
  - the wooden shield is ID 1 (OFFHAND) with `SetNumber` 0;
  - the longsword is ID 0 (MAIN) with `SetNumber` 0;
  - the chest is ID 4, the boots 6, the gloves 7;
  - the shortbow is ID 0 (MAIN) with `SetNumber` 1, the second weapon set.
  - Its `ItemList` entries all have struct ID 255 (`INVENTORY_SLOT_INVALID`). Read in the file.
  - Each entry's fields are `TemplateResRef`, `Stealable` and `Droppable`, plus `SetNumber` on the weapons.
- A creature's own template load (`0x006c0b40`, inside the load at `0x006c06f0`) calls the same `0x006bcc30` with `bReplaceInventory` = 1, so a spawned creature gets both lists the same way. Read. A second caller, `0x006c1fb3`, is not read (inferred: the save's load).
- An entry's item (`0x00753800`): its `TemplateResRef` (or, if that's empty, `InventoryRes`) names the `.uti`, loaded through item slot 0x1C0. Then item slot 0x1D0 reads the entry's own fields (`Stealable`, `Droppable` and the like) over it. Read; which `.uti` fields slot 0x1C0 copies is not listed here.

### `CreateItemOnObject` (212)

- `CreateItemOnObject(resource rItemFileName, object oTarget, int nStackSize = 1, string sTag = "", int bSuppressNotification = FALSE, int bDroppable = TRUE)` (`0x00673630`) pops in that order; the sixth only when given. Read.
- It does nothing and answers `OBJECT_INVALID` unless the implementer is the server's (`[this+4]` is 1, inferred), `nStackSize` > 0, and the target has an inventory (`0x005d5c20(target)`). Read.
- The items (`0x007595a0(list, tag, stack)`):
  - `0x00757eb0` makes one `CCItem` from the `.uti` (item slot 0x1C0, as above) and, if `sTag` isn't empty, sets its tag (`0x00696590`). Read.
  - If the item stacks (`0x00753c60`), the stack is split by the maximum stack size (`0x00753070`, inferred: `bitm_base`'s `StackSize`): full stacks as copies (`CCItem`'s copy constructor), then the rest. Each item's stack is set through its slot 116. Read.
  - If it doesn't stack, `nStackSize` separate copies are made. Read.
  - Every item gets `bDroppable` at +0x4B0. Read.
- While they're added, the global inventory flags `0x00C371B4` get 0x10, or 0x14 with `bSuppressNotification` (inferred: 0x4 suppresses the on-screen notice), and are put back after. `0x007634b0(list, inventory)` adds the items to the target's inventory. The answer is the first item's id. If the target's slot 0x24 answers (inferred: a placeable or container), it's set active. Read.
- **`EVENT_TYPE_INVENTORY_ADDED` (6)** (`0x00763100`, from the inventory's add):
  - It's not raised while the global flag 0x2 is set, when event 6 is in the owner's disabled set, +0x28C (`0x00698490(6)`; only `EnablevEvent` changes it and no shipped script calls it; corrected 2026-10-18 by lib-agent-2, `dev/LOAD_EVENTS.md`), or when the item's +0x5D0 check answers (not understood). Read.
  - Creator and target: the inventory's owner. Object 0: the item.
  - With the global flag 0x400, int 0 is 1 and it's delivered at once (`DeliverNow`). Otherwise int 0 is 0 and it's queued with no delay. Read.
  - `0x00763240` raises 7, `INVENTORY_REMOVED`, the same way, to the inventory that lost the item (checked 2026-10-18 by lib-agent-2, "Looting" below).

## Looting (lib-agent-2, for DAOParagonEngine)

### Taking loot: AcquireAllItems, AcquireItem (loota, GOG exe, read 2026-10-18)

#### Short answers

1. **Take All.** The panel sends `SelectedChar.AcquireAllItems <Container.ID>` (checked, Container.as TakeAllItems). The text becomes an `AcquireAllItemsMessage` (8 bytes: vtable 0xB1E3F4, object id at +4, made at 0x0077C9D0, name wide string at 0xB1F2D8; checked) and goes to the selected creature, `CCCreature::HandleMessage` 0x006A1FD0 (checked). The handler does no filtering. It calls `ForScript_MoveAllItems` 0x00763570 on the creature's ItemPossessor (creature+0x440) with the container id. That function finds the container's object (0x005D5C20, id to object, vcall +0x34, then +0x18 gives its item location, an `Inventory`), copies **every item of the container's list, in list order** (index 0..n-1, snapshotted into a vector first), and moves each one separately into the creature's own location (the `Backpack*` at creature+0xBF0, see below; corrected 2026-10-19 by lib-agent-2: the ItemPossessor slot 0x006BA800 is `add ecx, 0x7B0` on the +0x440 subobject, so +0xBF0, not +0xBFC). No droppable, stolen or plot test is made at this level (checked: the loop 0x00762E90 pushes each item unconditionally; the only per-item tests are CanRemove/CanAdd below). Which items are in a body bag was decided earlier by the use (PLACEABLES.md). Money is an ordinary item (base item row 17, `bitm` Label "Money - Copper", StackSize 1,000,000, Type 1; checked via ghilana), so it is moved like the others; it is not a separate "container copper" number at this layer.
2. **Full pack.** The destination is a `Backpack` (vtable 0xB26464, a `ListItemLocation`); its `CanAdd` is vtable slot 12, 0x007CA890. Capacity is the **number of entries (stacks count as one)** against an attribute at Backpack+0x28 (global flag 0x10 lets an item past `Backpack::CanAdd`; the generic CanAdd/CanRemove wrappers 0x007611A0 / 0x007611E0 bypass on 0x100 instead, read 2026-10-19 by lib-agent-2), which the base constructor `ListItemLocation` 0x007CAAE0 sets to **0x60 = 96** (checked; whether the party's Backpack is later given another value was not found). The movie reads `Party.BackpackSize` (attribute +0x18) and `Party.BackpackMaxSize` (+0x28) (strings 0xB060E0 / 0xB060C0, used at 0x0063AEA8 / 0x0063AF2B; checked). When it is reached mid-take: items that cannot fit are **skipped, the rest are still tried** (a stackable item that merges into an existing stack still fits). Nothing is removed from the container for a skipped item. If any item failed, the handler tells the GUI `Inventory.FlashFullOverlay` ("inventory" movie; strings 0xB0F4C8/0xB0F4B4; checked). The panel's own logic (checked): after sending, `full = BackpackSize >= BackpackMaxSize` and `containerNotEmpty = count(Container.Inventory) != 0`; if (containerNotEmpty and full) it calls `Inventory.FlashFullOverlay()` (plays ST_INVENTORY_FULL, overlay for 1.5 s) and **stays open**; otherwise `Close()`.
3. **Single item.** The loot list's double click, the TakeItem action and the radial menu send `GUI.SelectedChar.AcquireItem <item id>` (checked, ContainerItemList.as lines 92, 101; RadialMenu.as 245). That is taken by `CCCreature::HandleMessage(AcquireItemMessage)` 0x006A70E0 and moves the item **into the selected creature's pack**. The reverse, dropping a pack item onto the container list or radial menu entry, sends `Container.AcquireItem <item id>` (ContainerItemList.as 68/82/..., PlayerItemList.as 182) and is `CCPlaceable::HandleMessage(AcquireItemMessage)` 0x0074FCC0: it moves the item **into the placeable's own inventory**. Same message class (0xB1E3C4), the two handlers differ by receiver (checked). Which receiver the text reaches is decided by the text's prefix, resolved to an object by `Messages::FromCallbackText` 0x00524BE0 (data tree path, then vcall +0x4C; inferred, not followed).

#### Pseudo-code

```
// text "SelectedChar.AcquireAllItems <id>"  -> CCCreature::Handle(AcquireAllItemsMessage) 0x006A1FD0
HandleAcquireAll(creature, msg):
    savedFlags = g_invFlags;  g_invFlags |= 0x424        // 0x400 deliver events at once, 0x4 no on-screen notice, 0x20 (not read)
    result = MoveAllItems(creature.ItemPossessor /*+0x440*/, msg.id)   // 0x00763570
    if !result.allMoved:
        gui.Call("Inventory.FlashFullOverlay")            // string 0xB0F4C8 via 0x004D20C0 to the client
    g_invFlags = savedFlags
    return result.allMoved                                // al

MoveAllItems(me, containerId):                             // 0x00763570
    cont = GetObjectById(containerId)                      // 0x005D5C20; null -> src = null
    src  = cont ? cont.ItemLocation() : null               // vcall +0x18 of its ItemPossessor part
    dst  = me.ItemLocation()                               // vcall +0x18 = *(creature + 0xBF0), the Backpack
    if !src || !dst: return {allMoved=true, moved=[]}      // (initialised "ok")
    list = []
    for it in src (iterator: valid/current/next):  list.push(it)   // 0x00762E90, all items, no filter
    ok = true; moved = []
    for it in list (index order):                          // 0x00762D60
        r = MoveItem(it, dst)                              // 0x00762C40
        if !r.ok: ok = false
        if r.movedRecord non-empty: moved.push(r.movedRecord)
    return {ok, moved}

MoveItem(item, dst):                                       // 0x00762C40 -> 0x00762990
    loc = dst.FindLocationFor(item)                        // 0x00761340, dst vcall +0x50 (Backpack 0x007CA5E0: a stack to merge onto, else new)
    if item.holder (+0x4E8) == dst:  relocate within dst   // dst vcall +0x20
    else:
        canRemove = item.holder == null || item.holder.CanRemove(item)   // vcall +0x34, 0x007611E0
        canAdd    = dst.CanAdd(item, loc)                  // vcall +0x30, 0x007611A0
        if canRemove && canAdd:
            removed = item.holder.Remove(item)             // 0x00762100
            dst.Add(item, loc)                             // 0x00762010 -> dst vcall +4 (Backpack 0x00762240: stack merge or insert,
                                                           //   raises EVENT_TYPE_INVENTORY_ADDED 0x00763100; the old holder gets INVENTORY_REMOVED)
            ok = true
        else: nothing moves, ok = false

Backpack::CanAdd(item, loc):                               // 0x007CA890
    if g_invFlags & 0x10: return true                      // CreateItemOnObject sets it: scripted adds ignore the limit
    count = (end - begin)/4 of the item vector (+0xC/+0x10)
    max   = attribute at +0x28                             // default 96
    full  = count >= max && max != -1
    if item.stacks() && (existing = itemAt(loc)) && CanStack(item, existing): return true   // 0x00753C60, 0x00755CB0
    return !full

// text "GUI.SelectedChar.AcquireItem <itemId>" -> CCCreature::Handle(AcquireItemMessage) 0x006A70E0
HandleAcquireItem(creature, msg):
    item = GetItemById(msg.id)                             // 0x005D5870; null -> return false
    PlayGuiSound("Click")                                  // 0x00510CA0(0x12, L"Click")
    saved = g_invFlags; g_invFlags |= 0x424
    prevHolder = item.holder                               // +0x4E8
    r = MoveItem(item, creature.ItemLocation())            // via 0x007633F0
    if !r.ok:
        gui.Call("Inventory.FlashFullOverlay")
        if r.moved non-empty: MoveBack(r.moved, prevHolder)  // 0x00762FB0
    g_invFlags = saved
    return true

// "Container.AcquireItem <itemId>" -> CCPlaceable::Handle(AcquireItemMessage) 0x0074FCC0
//   same, into the placeable's own location; no sound, no overlay (calls 0x007633F0 on placeable+0x440, rolls back with 0x00762FB0)
```

#### Evidence

- Checked in disassembly: 0x006A1FD0 (flags |= 0x424 at 0x006A2018, call 0x00763570 at 0x006A2045 with ecx=this+0x440 and the id at msg+4, failure branch strings 0xB0F4C8 `Inventory.FlashFullOverlay` and 0xB0F4B4 `inventory`, sent with 0x004D20C0); 0x00763570; 0x00762E90; 0x00762D60 (per-item 0x00762C40, `ok=0` on a failed item, loop continues); 0x00762C40; 0x00762990 (CanRemove, CanAdd, then remove and add); 0x00762010; 0x007633F0; 0x006A70E0 (sound "Click", rollback 0x00762FB0); 0x0074FCC0 (same shape); Backpack vtable 0xB26464 and `CanAdd` 0x007CA890; `ListItemLocation` ctor 0x007CAAE0 (+0x18 = 0, +0x28 = 0x60, +0x38 = 0); ItemPossessor slot at creature+0x440 is 0x006BA800, which returns `*(creature+0xBF0)`.
- Checked in the movie AS (`%TEMP%\lib-agent-2-guicmd-x\allgfx\as_inventory\scripts\__Packages`): Container.as `TakeAllItems`, `Close`; ContainerItemList.as; PlayerItemList.as; Inventory.as `FlashFullOverlay`.
- Checked since ("The party's shared inventory" below; `dev/script_commands/inventory.md`): `*(creature+0xBF0)` is one Inventory shared by all the pool's members. Inferred: that the 0x20 bit of the flags (in 0x424) means nothing here; the routing from `SelectedChar.` / `GUI.SelectedChar.` to the creature; the container's own item location is `Inventory` (vtable 0xB1B974), whose CanAdd/CanRemove 0x0073F600/0x0073F640 let anything with `item+0x2C4` set, or base item 0x11 (17, copper) through (checked in code; meaning of +0x2C4 inferred as a plot/undroppable flag).
- Events: each move raises INVENTORY_REMOVED (7) on the old holder and INVENTORY_ADDED (6) on the new (ITEMS.md; 0x00763100); the module's item-acquired script (sp_module_item_acq) is not read here.

#### Not read

- Whether the party Backpack's max (+0x28) is ever changed from 96 (the Backpack constructor 0x0080D220 caller was not found), and what `Party.BackpackSize` (+0x18) is updated from.
- The body of `Backpack::Add` 0x00762240 in full (stack merge details, and whether copper becomes a normal pack entry or is added to a money counter on the way in).
- The text router's exact path for `SelectedChar.` versus `GUI.SelectedChar.`, and what the 0x20 flag bit does.
- `PartyBackpackChangedMessage`/`ContainerChangedMessage` senders (what tells the open panel to refresh).
- The module item-acquired script chain and the Wilds' blood quest.

### Loot events (lootb): the take's events, the darkspawn blood flag, the bag's end (read 2026-10-18)

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe and the toolset's scripts. "checked" is read in disassembly, data or script text; "inferred" is marked.

#### Short answers

1. **The events a take fires** (checked):
   - `EVENT_TYPE_INVENTORY_ADDED` = 6 and `EVENT_TYPE_INVENTORY_REMOVED` = 7 (`events_h.nss`:175-176). Each goes to the **owner of the inventory that changed**, which is also the creator: ADDED to the receiving creature, REMOVED to the container (the placeable). Not to the item, not to the module.
   - Fields: object 0 = the item (`CDataArrays::SetObject(0, item id)`, 0x00A5B950). Int 0 = 1 when the global flag 0x400 is set (delivered at once), else 0; the scripts read it as `bProcessImmediate`. REMOVED also gets **flag 0** = bit 5 (0x20) of the global flag word (0x00A5B890 = SetFlag; the scripts do not read it). No other fields. There is no "old owner" or "new owner" field and no stack count.
   - ADDED is raised by 0x00763100, REMOVED by 0x00763240 (`ret 4` at 0x0076338D). Both: not while global flag 0x2 is set, not if the event type is in the owner's disabled set (0x00698490), not if the item is being destroyed (+0x5D0). With 0x400 the event is created and delivered at once (0x0064AD80, `DeliverNow`); otherwise queued (0x0064ACE0, handler id 10, no delay).
   - Wrappers: 0x00763390(item) sets the item's +0x440 to the inventory owner's id and raises ADDED; 0x007633C0(item) sets +0x440 to -1 and raises REMOVED. Callers: creature's inventory callbacks 0x006A2EE0 (add) and 0x006A2FA0 (remove), the placeable's add 0x0074BE08 and remove 0x0074FC60 (calls 0x007633C0 at 0x0074FC6A), and 0x006313AA / 0x00631415 (another add path, which raises ADDED itself through 0x00763100).
2. **The module's item-acquired event is a script event, not an engine one** (checked in the scripts):
   - The engine never sends it. The creature's script `player_core.nss` (`HandleEvent_InventoryEvent`, :268, dispatched at :515-518) receives the engine's `EVENT_TYPE_INVENTORY_ADDED`. If the item has the local int `ITEM_SEND_ACQUIRED_EVENT` != 0 it calls `SendEventCampaignItemAcquired(GetModule(), oItem, GetEventInteger(ev,0))` (`events_h.nss`:1012): `Event(EVENT_TYPE_CAMPAIGN_ITEM_ACQUIRED)`, object 0 = the item, `SignalEvent(module, ev, bProcessImmediate)`. The event's creator is the signalling creature (OBJECT_SELF), that is the acquirer; `sp_module_item_acq` reads it with `GetEventCreator`.
   - `EVENT_TYPE_CAMPAIGN_ITEM_ACQUIRED` = **1020** (`events_h.nss`:59; no `EVENT_TYPE_MODULE_ITEM_ACQUIRED` exists).
   - Chain: module script `sp_module.nss`:114 (case 1020, comment "Sent by: player_core") -> `HandleEvent(ev, RESOURCE_SCRIPT_MODULE_ITEM_ACQUIRED)` = `sp_module_item_acq.ncs` (`global_objects_h.nss`:20) -> case 1020 (:99) -> not the special cases, `ITEM_CODEX_FLAG == -1` -> `_PassEventToPlotHandler(tag, ev)` (:261, :498) -> first three letters of the item tag pick the plot's script: "pre" -> `preev_item_acquired.ncs`.
   - `preev_item_acquired.nss`:45-71: case 1020; `oItem = GetEventObject(ev,0)`; `if (GetTag(oItem) == "pre200im_darkspawn_blood") WR_SetPlotFlag(PLT_PRE100PT_DARKSPN_BLOOD, PRE_BLOOD_PC_ACQUIRED_DARKSPAWN_BLOOD, TRUE, TRUE)`. (`PRE_IT_DARKSPAWN_BLOOD`, `pre_objects_h.nss`:128.)
   - **Nothing tests that the acquirer is in the party or is the PC**: the condition is the item's tag alone, plus `ITEM_SEND_ACQUIRED_EVENT` on the item. `pre200im_darkspawn_blood.uti` (designeritems.erf) has Tag `pre200im_darkspawn_blood`, BaseItem 50, Plot 1, and the var `ITEM_SEND_ACQUIRED_EVENT` = 1 (checked). `ITEM_CODEX_FLAG` = -1 there, so it goes to the plot handler. The only "party" condition is that the creature running `player_core` gets the ADDED event (the hero and followers; which creatures use `player_core` was not listed, inferred from its name and comments: "Any party member").
   - The flag's own script `pre100pt_darkspn_blood.nss`:41: on SET of that flag with value 1, if `UT_CountItemInInventory(PRE_IM_DARKSPAWN_BLOOD) >= 3` and `PRE_BLOOD_PC_HAS_DARKSPAWN_BLOOD` is not set, it sets `PRE_BLOOD_PC_HAS_DARKSPAWN_BLOOD`. So **the item must already be in the acquirer's inventory when the event runs** (ADDED comes after the add, and the count is by the hero's inventory), which is why the events must follow the move and not precede it. (`pre_functions_h.nss`:25 `PRE_ItemAcquired` is the same test, not called from this chain.)
3. **After the take** (checked in the exe):
   - Each removal from a placeable ends in 0x0074FC60: REMOVED (7) to the placeable, then the container GUI refresh (`[this+0x238]` slot 9 and the GUI object 0x00636250, inferred ContainerChanged), then **0x0074E710**, the same function `Container.InventoryClosedMessage` runs (0x0074E8D0 calls it, returns true). So the "emptied" handling runs after **every** removal as well as at close.
   - 0x0074E710: if `+0x618` (cannot add items: a loot-only container, which the OpenInventory native sets) and the inventory (+0x678) is empty: make not interactive (0x0069BA70(0)); if `+0x664` (the body bag's creature id) is not -1: the creature is handled by 0x006A93F0(creature, isCoinBag) (0x0069B830 and 0x0069BA70(0) on the creature, and for a coin bag its +0x790 body-bag id cleared, then 0x006A3AD0 / 0x0069F360(0), inferred "start its decay"); isCoinBag = the placeable's model name (0x0074CB80) equals `plc_bagcoin03_0` (string at 0x00B1BD68); if so the placeable is destroyed (vslot 0x198, arg 0). Then if `+0x614` is set the placeable is destroyed the same way. At the end, if the inventory is still not empty, 0x0069B830 runs.
   - So an emptied body bag: made non-interactive at once, its dead creature handled; destroyed by the engine only if it is a `plc_bagcoin03_0` bag or has +0x614 (not found who sets +0x614; not read). `gen_ip_bodybag.utp` is Appearance 16 whose `placeable_types` row has an empty ModelName, so it is **not** destroyed by this; it stays, non-interactive (ghilana: Body Bag, StateCnt_Bodybag, RemoveWhenDead 0). The creature body bags' decay is the script's: `creature_core.nss`:254-262 `SetBodybagDecayDelay(bag, 3000)`, `SetObjectInteractive(bag,FALSE)`, `DestroyObject(bag, 20000)` after `SpawnBodyBag` (that is for one death case; other cases :267, :293 plain `SpawnBodyBag`).
   - No script reacts to the bag's inventory events: `placeable_core.nss`:55 -> `Placeable_HandleInventory` (`placeable_h.nss`:576) has empty ADDED and REMOVED cases. Specific placeables (chests with their own scripts, e.g. `arl120ip_crate`) have cases but no body-bag script does. No `INVENTORY_CLOSED` or `PLACEABLE_ONCLOSE` script event exists in the scripts (grep of the extract: none).
   - **The panel does not close by itself in the engine as far as read**: the movie's Take All closes it unless the pack is full with items left (`dev/GUI.md`:39). Close sends `Container.InventoryClosedMessage` (runs 0x0074E710 again, a no-op if already done), `GameModeController.PerformUseObject <char> <container> 2` (USE_OBJECT with CLOSE: a body bag in Closed state offers only OPEN_INVENTORY, so it fails -1 at the action check and the script gets no USE event; before the check, 0x006B9620 moves the bag's creature's remaining droppables in, if any), and `GUI.HideFullScreenGUI`.

#### Pseudo-code for Take All of a body bag with the blood

```
// precondition: the bag is open, the panel is showing; SelectedChar = receiver (a party member)
// message "SelectedChar.AcquireAllItems <bag id>"  -> CCCreature AcquireAllItemsMessage 0x006A1FD0
saved = G.flags; G.flags |= 0x424;            // 0x400 deliver events at once, 0x20 (REMOVED flag 0 = 1), 0x4 no on-screen notice
MoveAll(receiver.inventory, bag.inventory):   // 0x00763570 -> list 0x00762E90 -> per item 0x00762D60/0x00762C40/0x00762990
  for item in bag.items:                       // money stacks included
      bag.inventory.Remove(item):              // item.owner = -1
          raise 7 to bag:      ev7{creator=bag, obj0=item, int0=1, flag0=1}   // delivered now
          bag script placeable_core -> Placeable_HandleInventory (empty)
          GUI ContainerChanged(bag)
          EmptiedCheck(bag)                    // 0x0074E710: only acts when the bag is empty, see below
      receiver.inventory.Add(item):            // item.owner = receiver.id (+0x440)
          raise 6 to receiver: ev6{creator=receiver, obj0=item, int0=1}        // delivered now
          receiver's script (player_core) HandleEvent_InventoryEvent:
              if item.ITEM_SEND_ACQUIRED_EVENT != 0:
                  SignalEvent(module, ev{type 1020, creator=receiver, obj0=item}, immediate=int0)
                  module sp_module -> sp_module_item_acq -> tag prefix "pre" -> preev_item_acquired:
                      if tag == "pre200im_darkspawn_blood":
                          SetPlotFlag(PLT_PRE100PT_DARKSPN_BLOOD, PRE_BLOOD_PC_ACQUIRED_DARKSPAWN_BLOOD, 1)
                          // flag script: if count(blood in hero inventory) >= 3 -> set PRE_BLOOD_PC_HAS_DARKSPAWN_BLOOD
              if base item is BASE_ITEM_TYPE_QUICK/GIFT: tutorial flags
G.flags = saved;
EmptiedCheck(bag) when bag.cannotAdd && bag.inventory.empty:
    bag.SetInteractive(false)
    if bag.creature != -1: HandleDeadCreature(bag.creature, isCoinBag = bag.model == "plc_bagcoin03_0")
    if isCoinBag or bag.destroyWhenEmpty(+0x614): Destroy(bag)
// then the movie closes the panel (unless the pack is full with items left):
send Container.InventoryClosedMessage -> EmptiedCheck(bag) again
send GameModeController.PerformUseObject receiver bag 2  -> fails -1 (no CLOSE action in the bag's state), no script event
GUI.HideFullScreenGUI
```

A single-item take (`AcquireItemMessage` 0x006A70E0) is the same Remove then Add per item without the 0x424 flags (so the events are queued, int 0 = 0, no on-screen suppression); not read beyond that its add and remove land in the same callbacks 0x006A2EE0 / 0x0074FC60 (inferred).

#### Evidence

- Event 6 body 0x00763123-0x00763220: gates (flag 0x2, 0x00698490(6)==1, item +0x5D0 via 0x00838210), event allocated 0x50 bytes, `[ev+0x20]=6`, `[ev+0x24]=owner` (vcall slot 0x14 of the inventory), SetObject(0, item +4), then 0x400 test: queued via `0x0064ACE0(0,0,owner,owner,0xA,ev)` or delivered via `0x0064AD80(owner,owner,0xA,ev)` after SetInteger(0,1) (0x00A5B710(0,1)). Event 7 body 0x00763240-0x0076338D: same plus 0x00A5B890(0, bit 5 of 0x00C371B4) before the int. `CDataArrays` vectors: +4 ints, +0xC flags, +0x10 objects (`dev/script_commands/commands.md`:92, `events.md`).
- `AcquireAllItems` 0x006A1FD0 sets 0x00C371B4 |= 0x424 (0x006A2018), calls 0x00763570 on `this+0x440` with the container id `[msg+4]`, restores the word at 0x006A2118. The result (a bool at +0xA0) false path builds a message (strings 0x00B0F4C8, 0x00B0F4B4; inferred a "pack full" notice). Not read further.
- 0x00763570: resolves the container object by id (0x005D5C20), its inventory (vcall 0x18), own inventory (vcall 0x18), builds the move list 0x00762E90 and executes it 0x006B2320; per item 0x00762C40 sets flag 0x8 and calls 0x00762990. Inferred that the item-level add/remove reach 0x006A2EE0 / 0x0074FC60 through the inventories' owner callbacks; the call from 0x00762990 down to them was not traced.
- Scripts: `sp_module.nss`:110-119, `sp_module_item_acq.nss`:99-263 and :498-530, `preev_item_acquired.nss`:56-79, `player_core.nss`:268-324 and :515-518, `placeable_h.nss`:416-473 and :576-592, `creature_core.nss`:254-293, `pre100pt_darkspn_blood.nss`:41-52.

#### Not read

- Which creatures run `player_core` (that followers do is inferred); whether non-party NPCs picking up the item would also raise 1020 if they run it.
- Who sets the placeable's +0x614 (destroy when emptied) and +0x618 from a template (0x618 is set by the OpenInventory native, per PLACEABLES.md); what 0x006A93F0's callees do exactly.
- The trace from 0x00762990 to the owner callbacks, the "pack full" path of AcquireAllItems, and the single-item take's flags.
- Whether the movie closes the panel after Take All on its own (GUI.md says so; the AS was not re-read here).
- Which 0x00631xxx function holds the second add path (0x006313A0) and what calls it.

## The party's shared inventory (lib-agent-2, for DAOParagonEngine)

### The party's shared Inventory: who gets INVENTORY_ADDED / REMOVED, leaving the pool, the join's move (pinva, 2026-10-19)

GOG exe, read in disassembly (`tools/daoexe.py`), plus ghilana and the toolset scripts. "checked" = read in disassembly/data; "inferred" = marked. Prior notes used: dev/ITEMS.md:145 and :200-207 (the wrappers, "the call from 0x00762990 down to the owner callbacks was not traced"), dev/script_commands/inventory.md:11 ("leaving the pool: not found"), :78, :98, dev/PARTY.md:402 (0x005D5CA0 = the controlled creature, `[[app+0xD0]+0x5C]`).

#### Short answers

1. **A take into a pool member's (= the party's) Inventory raises ADDED on TWO objects, in this order: the party object (`CCParty`, id `[party+4]`), then the MAIN CONTROLLED creature (`0x005D5CA0`, CCPlayer+0x5C), NOT the creature that took the item.** (checked) Same for REMOVED (0x00763240): the party, then the controlled creature. So Take All by a selected follower who is not the controlled creature still fires the event at the controlled one. The chain is Inventory::Add -> Backpack::Add 0x00762240 -> object `[backpack+4]` (= the owner id the Inventory was built with = the PARTY's id) -> its ItemPossessor slot 0x1C = `CCParty` 0x006313A0. The ItemPossessor `ecx` of 0x00763100 is not the Inventory: slot 0x14 of the **possessor** (party: 0x00639DA0 = `[party+4]`; creature: 0x007EBCF0 = `[creature+4]`) returns the owner id, which is `[ev+0x24]` and the event's target/creator.
   - 1020 (CAMPAIGN_ITEM_ACQUIRED) therefore comes from whoever is the controlled creature **if it runs player_core**. player_core is the EventScript of the hero template (`default_player.utc` and the other `default_*.utc` origin templates have `EventScript` = `player_core`, ghilana: 14 templates, Tag `player`) and of every follower while in the active party (`wrappers_h.nss`:343-353, `WR_SetFollowerSubState` ACTIVE/LOCKEDACTIVE: `SetEventScript(oCreature, RESOURCE_SCRIPT_PLAYER_CORE)`; leaving sets `creature_core`, :414). The controlled creature is always a party member in practice (hero or active follower), so it runs player_core and 1020 fires, once per ADDED, with `GetEventCreator` = the controlled creature. The party object's own ADDED goes to the party (no script known; inferred nothing handles it).
   - The creature-own callback `CCCreature` 0x006A2EE0 / 0x006A2FA0 is NOT used for pool members: the shared Inventory's owner is the party, so the owner lookup returns the party's possessor. 0x006A2EE0 is used only by a creature's own (non-shared) Inventory, i.e. a creature outside the pool.
2. **Leaving the pool: a NEW EMPTY Inventory is created for the creature; the party's is untouched.** SetFollowerState state 0 -> `0x0063B8B0(id, force)`: erases the map entry (0x0063A1D0), then `0x006A29A0(creature)`: `new Inventory(owner = creature id, 9, 0xC)` (0x007405E0) and sets `creature+0xBF0` to it (attribute setter 0x004A97D0 just stores the pointer, deletes nothing). Items are NOT given back: they stay in the party backpack. A creature destroyed while in the pool: its destructor 0x006BAFE0 first calls party 0x006377E0(id,1) and `0x0063B8B0(id, 1)` (0x006BB120/0x006BB131, forced, skips the "last member" refusal), so it too gets a fresh Inventory, then 0x006BB26B deletes the object at +0xBF0 (the fresh one). The shared Inventory is never deleted by a creature. (checked)
3. **The join's move merges stacks, respects the 96 limit, and raises no events.** (checked) 0x006A8C10 sets global flag 0x2 only (`or eax,2` at 0x006A8C67, restored at 0x006A8CE2), NOT 0x10 and not 0x100, then `0x00762E90(&r, oldInventory, partyInventory)`. With flag 0x2 set, 0x00763100/0x00763240 return at once: **no ADDED/REMOVED for the join's items** (the owner callbacks still run: the item's `+0x440` owner is set). Bypass flags are not set, so CanAdd/CanRemove apply: items that do not fit stay in the old Inventory and are not moved (see Not read for what the delete then does).

#### Pseudo-code

```
// Backpack::Add 0x00762240 (after merge or placing; both paths end here)
item.location(+0x4E8) = backpack
backpack.vslot[0x54]()                      // dirty/notify
owner = GetObjectById(backpack.ownerId /*+4*/)   // 0x005D5C20, returns the ItemPossessor via vslot 0x34
owner.possessor.vslot[0x1C](item)           // OnItemAdded; REMOVE: Backpack::Remove 0x007619E0 -> vslot 0x20 (0x00761AF0-0x00761AFE)

// party possessor (CCParty+0x70) OnItemAdded 0x006313A0
SetItemOwner(item, party.id); Raise(ADDED, target = party.id)        // 0x00763390 -> 0x00763100
if (!(G.flags & 2)) Raise(ADDED, target = GetMainControlled().id)    // 0x006313E1-0x00631415, the controlled creature's possessor
if (!(G.flags & 4)) party.NewItemNotice(item)  // 0x00629620
if (!(G.flags & 0x40)) party.+0x620 attribute.set(1)
// REMOVED 0x00631470: SetItemOwner(item,-1); Raise(REMOVED, party); Raise(REMOVED, controlled) (0x763240 does its own flag-2 gate)

// creature's own possessor (not in pool / own Inventory) 0x006A2EE0
if (party && party.members.contains(creature.id)) { SetItemOwner(item, creature.id); Raise(ADDED, creature) }
else { G.flags |= 2; SetItemOwner(item, creature.id) /*no event*/; restore }

// Leaving the pool 0x0063B8B0 (SetFollowerState 0, and creature destructor with force)
members.erase(id); creature.+0xBF0 = new Inventory(owner=creature.id, 9, 12)   // empty; party backpack unchanged

// Join 0x006A8C10 (creature joins the pool)
G.flags |= 2; MoveAll(old -> partyInventory)  // 0x00762E90: list old items, then per item 0x00762C40 (flag 8, slot 0x00761340) -> 0x00762990
partyMoney += oldMoney; creature.+0xBF0 = partyInventory; delete old; G.flags = saved
```

#### Evidence

- Possessor vtables (RTTI scan, `tools/daoexe.py` vtables): ItemPossessor 0x00B1C258 (slot 5 = purecall 0x008E17D6, slots 7/8 = 0x00763390/0x007633C0); `CCParty` at +0x70: 0x00B05DBC (slots 5 = 0x00639DA0 `add ecx,-0x6C; jmp 0x53CC40` = party id at +4, 7 = 0x006313A0, 8 = 0x00631470); `CCCreature` at +0x440: 0x00B10334 (5 = 0x007EBCF0 `add ecx,-0x43C` = creature id, 6 = 0x006BA800, 7 = 0x006A2EE0, 8 = 0x006A2FA0); `CCPlaceable` at +0x440: 0x00B1BF30 (7 = 0x0074BE00, 8 = 0x0074FC60). Party object's primary vslot 13 (0x34) = 0x00639D40 `lea eax,[ecx+0x70]`; creature's = 0x00750EC0 `lea eax,[ecx+0x440]`. 0x005D5C20 = id -> object -> vslot 0x34 (the possessor).
- Correction to dev/ITEMS.md:122: ItemPossessor slot 6 for a creature (0x006BA800) is `add ecx,0x7B0` on the possessor (= creature+0x440) so it returns the Inventory attribute at **creature+0xBF0**, not +0xBFC.
- Party ctor 0x0063A3C0: `[esi+0x20C] = 0x007405E0(new 0x50 bytes, ownerId = [esi+4] via 0x53CC40, 9, 0xC)` (0x0063A98D-0x0063A9AC). 0x007405E0 sets vtable 0xB1B974 and builds sub-locations; the Backpack is `0x0080D220(owner, 9, 0xC)` -> `0x007CAAE0(owner)` -> `0x00761170`, which stores `[loc+4] = owner` (0x0076117C). Backpack max (+0x28) = 0x60 (0x007CAB47).
- 0x00762240 tail 0x00762558-0x00762598: `[item+0x4E8] = this`, vslot 0x54, `GetObjectById([this+4])`, vslot 0x1C(item). Stack merge branch 0x0076232B-0x00762365 (0x00755CB0/0x00755D10 can-merge, then destroys the moved item with vslot 0x198(0)); skipped if global flag 1 is set (0x007622E4 `and al,1`, 0x00762316).
- 0x00740950 (Inventory::Add) routes before the Backpack: items with `+0x2C4` -> sub-location +0x18; base-item type word `+0x464` == 0x11 (money) and inventory owner == `[app->party]+4` -> amount `[item+0x5F0]` added to the money location (+0x28, 0x007C9FE0) and the item deleted (so party money never takes a slot); `+0x4EC` == 0x6C -> 0x007408B0; else Backpack at +8. CanAdd on the Inventory 0x0073F600 is true for `+0x2C4` items and money, else Backpack::CanAdd.
- Backpack::CanAdd 0x007CA890: flag **0x10** (`test [0xC371B4],0x10`) returns true; else true if size < max (+0x28) (max -1 = unlimited), or the item is stackable (0x00753C60) and an existing stack found (0x00761260 -> vslot 0x48) accepts it (0x00755CB0). The generic wrappers 0x007611A0 (CanAdd) / 0x007611E0 (CanRemove) skip the check when flag **0x100** is set, else call location vslot 0x30/0x34.
- 0x00762990: item's location (`+0x4E8`) == dst -> a slot move; otherwise `CanRemove(old)` and `CanAdd(dst, slot)`, then `0x00762100` (Remove, `-1` = whole stack) and `0x00762010` (Add); either check failing falls to 0x00762B30 and nothing moves. 0x00762C40 sets flag 0x8 and picks the slot with 0x00761340. 0x00762D60 loops 0x00762C40 over the list and ANDs the result into `*out` (0x00762E29).
- 0x006A8C10: 0x006A8C49 `lea ebp,[ecx+0xBF0]`; flag 2 at 0x006A8C67-0x006A8C6E; 0x00762E90 at 0x006A8C85; money 0x006A8C8D-0x006A8CB3 (`old+0x28 -> +0x90` into `party+0x28`'s money via 0x007C9FE0); setter vslot 0x54 at 0x006A8CC1; old deleted `vslot 0 (1)` at 0x006A8CC9-0x006A8CCF; flags restored 0x006A8CE2.
- Whole-.text scan for `[reg+0xBF0]` (disp32 `F0 0B 00 00`): 36 hits, no direct store; all uses are `lea ecx,[reg+0xBF0]`. The only calls of the attribute setter (vslot 0x54 -> 0x004A97D0) on it in CCCreature: 0x006A29A0 (new Inventory; only caller 0x0063B950 in 0x0063B8B0), 0x006B4070 (the creature's initialisation, 0x006B449D: new Inventory; only caller 0x006BA75B in 0x006B9ED0), 0x006A8C10 (the join), and the destructor 0x006BAFE0 (0x006BB26B: delete + set null). Hit at 0x004F11C0 is an unrelated stack constant.
- 0x00763100 gates: flag 0x2, `0x00698490(obj, 6)` (the owner's disabled-event set), item `+0x5D0`; `[ev+0x20]=6`, `[ev+0x24]=owner`; delivered to the owner (queued or at once with 0x400). 0x006313E1-0x00631415: if flag 2 clear, `0x005D5CA0` (controlled id, or -1) -> `0x005D58C0` (creature by id) -> `lea ecx,[creature+0x440]; call 0x00763100(item)`.
- 0x006A2EE0: `0x006A2CC0` = `[[app+0xD0]+0x58]` (party); `0x00631830(party, creature id)` tests the party's map `+0x234`; in the pool: 0x00763390 (event); outside: flag 2 around 0x00763390 (owner set, no event).
- Scripts: `player_core.nss`:268-300 (`HandleEvent_InventoryEvent`: `GetEventCreator` = oOwner, item = object 0, ADDED -> `SendEventCampaignItemAcquired` if `ITEM_SEND_ACQUIRED_EVENT`), `wrappers_h.nss`:343-353 and :414 (event script per follower state), `cam000ar_all_camps.nss`:118 and `cli400ar_city_gates.nss`:72 (scripts setting player_core on party members).

#### Not read

- What deleting the old Inventory (0x006A8CC9) does to items that did not fit and stayed in it (the Backpack destructor 0x0080D200 and the ItemLocation's list ownership were not read). With a full party backpack a joining member's leftover items may be destroyed.
- Whether anything handles the ADDED/REMOVED delivered to the party object itself (the party's script, if any; 0x00698490's disabled set for it).
- The Remove path's body beyond the vslot 0x20 call (0x007619E0 read only for the callback and the 0x54 notify), and the exact REMOVED flag-0 values for the party raise.
- Who calls 0x006B9ED0 / 0x006B4070's init for a loaded save (the save path restoring a pool member's +0xBF0 pointer to the party's: join on load via 0x0063A020 is inferred from dev/LOAD_EVENTS.md/PARTY.md, not re-read).
- The case where a non-pool creature (outside the pool, own Inventory) is the target: only the callback 0x006A2EE0 was read (no event raised if the creature is not in the party's map).

### A template's Gold (pinvb) (2026-10-19)

#### Answers

1. **LoadItemsFromTemplate does not read Gold, and neither does any other engine path.** The creature-from-template loader and the item-list loader (`0x006bcc30`, below) never touch money. The label "Gold" has no string of its own in the exe and nothing refers to its tail-merged copy (checked). Every call of the money add (`0x007C9FE0`) and subtract (`0x007CA020`) is listed below, and none is a template or creature load. So `Gold = 75` in default_player.utc is dead data: the engine ignores it.
2. **There is no other place where it reaches money.** The hero's creation (`0x0069D910` / `0x0069D790`) makes no money call. The pool join moves the hero's own purse, which is 0, into the party's. Chargen's scripts never touch money.
3. **A new game's starting money is 0 copper from the engine and chargen's scripts.** Whatever the party has at the first controllable frame comes from the module or prologue scripts, or from treasure. The port should start at 0 and not 75. The 75 on default_player.utc is unused; copying it would be a deviation from vanilla.
   - I did not find a vanilla record of the money seen at a new game. The peer's "0 gold" is consistent with the code.
   - Not played: I did not read the first prologue module's scripts for a grant. That is the only place a non-zero start could come from (see Not read).

#### Order a port can copy

```
hero = create_creature(from "default_player.utc")   // 0x0069D910; Gold field ignored; purse = 0
hero.purse = 0                                        // own Inventory (+0xBF0), money location empty
join_pool(hero)                                       // SetFollowerState(hero,1) 0x0069E015 -> 0x0063A020 -> 0x006A8C10
    party.money += hero.purse        // 0x007C9FE0 at 0x006A8CB3, ADD
    hero.inventory = party.inventory
chargen script: Chargen_InitInventory(hero, class, idx)
    if IsPartyMember: Chargen_ClearInventory (destroys items; no money call)
    LoadItemsFromTemplate(hero, <starting equipment or default_<class>.utc>, TRUE)
        // destroys equipped items and the backpack list; loads Equip_ItemList and ItemList; money untouched
=> party money 0
```

LoadItemsFromTemplate with bReplaceInventory: money is neither reset nor changed. It destroys the items of the `Equipment` list and the backpack `ListItemLocation`, and the money location (Inventory+0x28) is a different location (dev/script_commands/inventory.md:12). An `ItemList` entry that is the copper base item (17) would be added to the money location by the Inventory add (`0x00740a3d` -> `0x007C9FE0`), but default_player.utc's `ItemList` is empty (checked) and default_warrior.utc's `ItemList` is `[]` too.

#### Evidence

- default_player.utc (ghilana, `packages/core/data/chargen.rim`): `Gold` = 75, `ItemList` = [], `Equip_ItemList` = [gen_im_cth_com_a01]. GFF3 label `Gold`, an int field. default_warrior.utc: `Gold` = 0. (checked)
- Creature template field reads, the exe's own list: the object part at `0x0069ad60..` reads TemplateResRef, Conversation, InteractRadius, AILevel, TeamId, GroupId, GroupForceHsti, GroupOverridden, LastHostileActor, Immortal, Plot, IsDestroyable, EventScript(s); the creature part at `0x006c0850..` reads TreasureCategory (-> creature+0xBD0), CharType, NoPermDeath and the rest. Neither reads "Gold". (checked)
- The label's text: the only `Gold\0` in the exe is the tail of `StoreGold` at 0xb244c4 (its tail would be 0xb244c9). A scan of all E8/E9/68 operands in .text for 0xb244c9 finds nothing. StoreGold is a store field, unrelated. (checked)
- Every caller of `0x007C9FE0` (money add), found by scanning E8/E9 operands in .text: `0x0066aacd`, `0x0066abbc` (the AddGold / SetCreatureMoney family of script commands, by the command table; inferred from addresses 0x0066aa..), `0x006a8cb3` (the pool join, dev/script_commands/inventory.md:11), `0x00740a3d` (Inventory add of a copper item), `0x007ec444` (a store sale: reads creature+0xBF0, money location +0x28). Of `0x007CA020` (subtract): `0x0066aab0`, `0x006b98af`, `0x007403bb`, `0x007ed0ae`. None is a load from a template. (checked as call sites; the callers' functions beyond 0x006a8cb3 / 0x007ec444 not read in full)
- "Money" (0xb06a00) is referenced at `0x00644827` and `0x007b9642`; the first is a save-side writer that pairs it with "CurrentHealth", i.e. the save, not the template. (checked for 0x00644827; 0x007b9642 not read)
- Scripts: `Chargen_InitInventory` (`sys_chargen_h.nss:423`) calls `LoadItemsFromTemplate(oCreature, sTemplate, TRUE)` and `_AddAbility` only; `Chargen_ClearInventory` destroys items. No `AddCreatureMoney` / `SetCreatureMoney` / `GetCreatureMoney` occurs in `sys_chargen*.nss`. The only `AddCreatureMoney` callers in the scripts are `placeable_h.nss:446`, `sys_rewards_h.nss:336,502`, `sys_treasure_h.nss:329,411,478`. (checked)
- Hero creation path: dev/PARTY.md:572 (0x0069DF10 -> 0x0069D910 -> `SetFollowerState(hero,1)` at 0x0069E015); `0x0069D790`'s calls (disassembled: 0x413660, 0xa388f0, 0xa361b0, 0x6abdc0, 0x4c.... string helpers, 0x53cc40) include no money add. (checked)
- Money layout and join: dev/script_commands/inventory.md:11-13, :39-47, :322-326; items.md:11-13.
- Other records: DAOEluvianMCP dev/TESTING.md:366 shows `Party.Money.Total` 78 at a store after play, not a start value. DAOTurnBased has nothing on starting money (grep of dev/ finds only the poultice-cost remark in REBALANCE.md:165).

#### Not read

- The prologue modules' scripts (origin starts, Ostagar) for a grant of starting money or a SetCreatureMoney through a different command. Only the 0 from the engine and chargen is established.
- The functions around `0x0066aacd` / `0x0066abbc` beyond identifying them as the gold script commands; `0x007b9642`.
- Whether a save of the freshly created hero stores "Money" as 0 or 75: the save writer at `0x00644800` reads the live purse, not a template (inferred).
- A real new game's money seen in play; no record found.

## Equipping

Read 2026-10-18 in the GOG exe by lib-agent-2 (a subagent), for DAOParagonEngine's items batch, building on `src/dao/Equipment.cpp` (the equip `0x006BC7F0` and unequip `0x006B7690` every path meets in, DAOLib #22, seen in play 2026-10-04) and `dev/CHARACTERS.md` (the appearance's slots). "Read" is seen in the code; "inferred" is marked. Slots are `script.ldf`'s: `INVENTORY_SLOT_MAIN` 0, `OFFHAND` 1, `RANGEDAMMO` 2, `CHEST` 4, `HEAD` 5, `BOOTS` 6, `GLOVES` 7, `CLOAK` 8, `RING1` 9, `RING2` 10, `NECK` 11, `BELT` 12, `BITE` 13 (a creature's natural weapon), 14–19 Shale's and the dog's; `INVENTORY_SLOT_INVALID` 255; `INVALID_WEAPON_SET` −1. An item's base item row is its word at +0x464; its `bitm_base` `Type` is cached at +0x6E0 (`0x00752C70`): 1 misc, 2 melee weapon, 3 shield, 4 armour and clothing, 5 staff, 6 bow or crossbow, 7 ammunition, 8 rune.

### The script commands

- `EquipItem` (392) and `UnequipItem` (393) share `0x0065CDA0`. It pops the creature and the item (objects); for `EquipItem` it then pops the slot and the weapon set with their defaults, 255 and −1 (`ForScript_CommandEquipItem`). It calls `CCCreature::Equip(item, slot, set)` or `CCCreature::Unequip(item)` and pushes the success as an int (0 when the creature or the item is not found). Read.
- `GetItemInEquipSlot` (401, `0x00669C70`) pops the slot (int) and the creature (object), and the weapon set (int) only when called with three arguments, else −1. For slot 0 (`MAIN`) it first looks in slot 13 (`BITE`, set −1) and answers a natural weapon there if there is one; otherwise it looks in (slot, set) of the creature's equipment (+0xBE0 → `0x00761260`). An item being destroyed (+0x5D0) answers `OBJECT_INVALID`. Read. That set −1 means the active weapon set is inferred (`SwitchWeaponSet`, 273, changes it).

### The equip, `CCCreature::Equip` (`0x006BC7F0`, `__thiscall (item, slot, set)`)

- Slot 255 lets the item choose its own slot (`0x00762C40`, inferred: its first fitting `EquippableSlots`); any other slot goes to (slot, set) (`0x00762990`), which validates the item against the slot (not read further). What was in the slot is moved to the inventory by the equip itself, not through the unequip (seen in play 2026-10-04, `src/dao/Equipment.cpp`). Read.
- Ammunition (Type 7): before equipping, the weapons in the set's main and off hands are looked up and, when the main hand's `bitm_base` `Ammo` column (hash `0xE6203B7E`) is 0 or not the ammunition's base item, the hands' items are unequipped. Read in outline; the decompiled variables are tangled, so which item is tested against which is inferred.
- On success the item leaves the inventory (`0x00762FB0` on +0xBF0) unless `0x0086BEE0` answers. Then, for a melee weapon, shield or staff (Type 2, 3, 5), whatever is in `RANGEDAMMO` (slot 2) of that set is unequipped: ammunition that no longer fits (read; `src/dao/Equipment.cpp`: "three places"). For a follower in a party the module is told (`0x00693290(0x42F, …)`, not read). Last, unless the game mode is 9, `0x006951A0`, and the stats object's `0x0071D4A0` (inferred: recompute). Read.

### What equipping does to the creature (`0x006C16E0`, `__thiscall (item, slot)`; inferred to be the slot's on-equip, from what it does)

- A two-handed weapon (`0x00A664C0`) unequips what the other hand holds; equipping the off hand (slot 1) while a two-hander is in the main hand unequips the main hand. Read.
- The item's +0x4A8 is set to the creature's id (its owner). Read.
- **Stats:** unless the creature's +0xC58 is set, `0x0071B3C0(item, +1.0)` adds the item's stats to the creature's property modifiers: for each row of `itemstatdefines` (109) with a creature property, the stat named by its `Label` is read from `itemstats` (110) at the item's base item row and multiplied by the same stat of `materialrules` (159) at the item's material (`materialtypes`' `Material`, from the item's MaterialType +0x4FC; 1 when not above 0), times +1.0; `0x0071B2D0(property, value)` adds it. Then `0x00759A40(creature, item)` (inferred: the item's own properties). Unequipping calls the same with −1.0. Read in outline; which factor is the material's is inferred from the tables named.
- The appearance's slot (`0x00748DA0(slot, item)`), heraldry for slots 0–2, the wield (`0x006A6640(slot)`), and the stats recomputed (`0x0071B970`). Read.
- **`EVENT_TYPE_EQUIP` (11):** when event 11 is not in the creature's disabled set, +0x28C (`0x00698490`; only `EnablevEvent` changes it and no shipped script calls it; corrected 2026-10-18 by lib-agent-2, `dev/LOAD_EVENTS.md`) and the item is not being destroyed, delivered at once (`AIMaster::DeliverNow`) to the creature (caller the item): creator the creature, object 0 the item, int 0 the slot, int 1 a bit of a global flag word (`DAT_00C371B4` bit 7; the equip sets 0x52 and 0x10 there, meaning not read). Read.
- If the creature is the hero, the party's +0x4AA is set (inferred: the inventory screen to refresh). Read.

### Unequipping (`0x006C19F0`, the matching on-unequip)

- The item's owner (+0x4A8) back to −1, the equipment updated, `0x00757B90`, the stats taken off (`0x0071B3C0(item, −1.0)`), and unless `0x006AB100` answers, the appearance's slot cleared (`0x007489C0`), heraldry and the wield as above, then `0x0071B970`. Read.
- **`EVENT_TYPE_UNEQUIP` (12):** as EQUIP, to the creature (caller the item): creator the creature, object 0 the item, int 0 the slot. Read.

### Read for DAOParagonEngine #169 (items)

Read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169. All addresses are GOG 1.05. Names are the address library's where it has one; "(unnamed)" where it has none.

- **The on-equip and on-unequip have one caller each, and it is the Equipment location's add/remove callbacks, for the ACTIVE weapon set only** [C]. `0x006C16E0` (on-equip) has a single xref, `0x00797D5A`, inside `0x00797D10` (Equipment vtable `0xB1FB8C` slot 21, +0x54; unnamed); `0x006C19F0` (on-unequip) is called from `0x00797E67` inside `0x00797E20` (slot 22, +0x58; unnamed). Both do the same: the entry's set number (entry+0x10) is replaced by the active set when it is not below the set count (`0x007977B0`); if it equals the active set (attribute at Equipment+0x10) and the byte at entry+0x30 is 0, the creature's on-equip/on-unequip runs (stats, EQUIP/UNEQUIP event, appearance, weapon style `0x006B8D40`). For any other set nothing runs: `0x00797D10` only stores the set's location into the item's +0x4E8 (`0x007974D0(set)`), so an item put into an inactive set gets no stats and no EQUIP event, and an item taken out of one gets no UNEQUIP. The new set's equips on a switch are in the next bullet.
- **The weapon-set switch, `0x007977D0` (unnamed; called by `0x006A8FA0`, SwitchWeaponSet's creature side, which then calls `0x006951A0`)** [C for the order, I for the stats of the new set]. Does nothing when the set is not below the count or is already the active one. Otherwise: byte +0x54 = 1; global flag `0x00C371B4 |= 0x202` for the whole switch (with 0x2 no ADDED/REMOVED inventory event is raised, ITEMS.md:41; what 0x200 does was not read); phase 1, for each slot of the OLD set (the active attribute is still the old one): `0x00761260` finds the item, `ForScript_RemoveItem(item, -1)` (`0x00762100`) takes it out of its slot, `0x00762010` adds it into the set's own storage location (Equipment+0x48 + set*0x20), so the Equipment remove callback fires with set == active: on-unequip runs for each item (UNEQUIP events, stats off). Phase 2, for each item stored in the NEW set's location: `CCCreature::Equip` (`0x006BC7F0`) with the item's slot and the new set number; if that returns false the item is put into the creature's backpack (`0x00762C40` on `*(creature+0xBF0)`, flag 0x10 set). Only then (`0x00797A6B`) the active-set attribute is set to the new set (attribute vtable +0x54) and byte +0x54 cleared. Because the attribute changes after phase 2, the add callback in phase 2 sees set != active and runs no on-equip; whether the stats and EQUIP events of the new set are applied by an observer of the active-set attribute was not read [I].
- **`set = -1` means the active set** [C]. `0x007F4EA0(slot)` (the address library names it SlotItemLocation::ItemIn) builds `{slot, 0, 0, -1, -1, -1}` and calls the location's vtable +0x48 (`0x00761260`). The Equipment lookup `0x00797AA0` (unnamed) takes the set from entry+0xC, replaces it by the active attribute (Equipment+0x10) when it is not below the set count (an unsigned compare, so -1 always is), and when that equals the active set answers from the live slots (sub-location Equipment+0x24), else from the stored set (+0x48 + set*0x20).
- **The two-handed test `0x00A664C0` is `EquippableSlots == 1` exactly, with no `Type` test** [C]: it calls `0x00A66440` (the `bitm_base` column `EquippableSlots`, hash `0xD5B0AE22`) and compares with `0x00A66470(0)` (1<<0). ITEMS.md:517 had it; for the port: a bow, crossbow, greatsword, maul, battleaxe (slots 1) are two-handed; a staff or any item whose slots word is 3, 2, 8192 ... is not, whatever its Type.
- **`0x006AB100` (unnamed; proposed CCCreature::HasLootableBodyBag): what it asks** [C for the shape, I for two meanings]. True when: the creature's vtable +0x1D0 answers non-zero (dead; the slot's meaning not read, inferred from COMBAT.md:480); its body bag (`creature+0x790`) resolves to a placeable (`CClientExoApp::GetPlaceableById` `0x005D5B40`); the bag's vtable +0x100(0) is non-zero (not read); and either `0x006A90D0(creature)` is true (unnamed; the creature still has something to take: money at `*(+0xBF0)`+0x28 -> +0x90 above 0, or an item of its equipment (+0xBE0) or backpack (+0xBF0) for which the predicate `0x006A4440` holds, the predicate not read) or the bag's inventory (+0x678) has items. In the on-unequip `0x006C19F0`, when it is true `0x006B8D40` (the weapon style) is NOT called (`0x006C1A6F` then `0x006C1A8B`; ITEMS.md:544). So a corpse that is being looted keeps its style while its gear is taken out; one with nothing left recomputes. So the exe skips only the style recompute, and only while the corpse has a bag with something in it.
- **Flag 0x100 alone does not silence the inventory events** [C]. `ForScript_StorePartyInventory` (`0x00638D10`) and `ForScript_StoreFollowerInventory` (`0x00638EB0`) set exactly `0x00C371B4 |= 0x100` (`0x00638E2E`, `0x00638FF3`), call the move-all `0x00762E90`, refresh both locations (vtable +0x24), `SetObjectInteractive(placeable, 1)` (`0x0069BA70`) and restore the flag. Per ITEMS.md:53 and :259, 0x100 only makes the generic CanAdd/CanRemove wrappers `0x007611A0`/`0x007611E0` pass; the ADDED/REMOVED events are suppressed by 0x2 alone (ITEMS.md:41, :223), which these commands do not set. So the exe raises REMOVED (from the source holder) and ADDED (to the placeable's inventory) for every item stowed, as a script's MoveItem would. For a follower's equipment the source holder is the Equipment location, whose remove callback (`0x00797E20`, above) runs on-unequip only for items of the ACTIVE set: items of the other sets are carried out with no UNEQUIP and no stats change (they had none on). So a stowed follower is unequipped as its items go for the active set's items [C, by the callback]; whether the move-all iterates the inactive sets' stored items too was not read [I].
- **The same-holder case of a move: `0x00762990` (unnamed; the remove-then-add of one item from its holder into a location at a slot)** [C for the structure]. First test: `item+0x4E8 == destination location`; then it calls the location's vtable +0x20 (a move within the location, slot from `0x007612E0`) and, if that answers true, finishes at once (`0x00762A86`) without `ForScript_RemoveItem`/add. Otherwise it needs the holder's CanRemove (`0x007611E0`) and the destination's CanAdd (`0x007611A0`), then removes (`0x00762100(item, -1)`) and adds (`0x00762010`). `MoveItem`'s `0x00762C40` runs it with global flag `|= 8` set meanwhile (`0x00762CA2`; script_commands/inventory.md:98 says 0x10, which is what the callers that bypass CanAdd set, such as the weapon-set switch above), choosing the slot first (`0x00761340`). So an item already in the target Inventory is moved WITHIN it (its slot may change), not skipped; whether that raises any event, and what Backpack's vtable +0x20 does, was not read [I]. The party's backpack is one Inventory shared by its members (ITEMS.md:124, :222), so a move between two members is this case. The check is [C]; that nothing is seen of such a move is [I].
- **MoveAllItems (350) takes the backpack only** [C]. `ForScript_MoveAllItems` (`0x00763570`, ITEMS.md:52, :62-:90; script_commands/inventory.md:360-363) walks the list of the SOURCE's item location (possessor slot 0x18 = `*(creature+0xBF0)` for a creature, +0x678 for a placeable); a creature's equipment is the separate location at +0xBE0, so what is equipped stays. No plot, stolen or droppable test per item (ITEMS.md:52).
- **`LoadItemsFromTemplate` with `bReplaceInventory` destroys the list at `creature+0xBF0`, which for a pool member is the party's shared Inventory** [C]. ITEMS.md:14 (the list at +0xBF0 destroyed) with :124 and :222 (+0xBF0 is the one shared Inventory while the creature is in the pool, so the destroy takes the party's backpack).
- **AddItemProperty's appender `0x00756800`** [C]: refuses an id already in +0x680 and returns false (script_commands/item_prop.md:21); `AddItemProperty` only calls it for nProperty > 0 and nPower > 0 (item_prop.md:20). So for a script's add an id is never doubled [C]; for the .uti list it is [I]: that the template loader (`0x007572D0`) goes through `0x00756800` rather than filling +0x680 directly was not read.

### What the scripts add

- The armour and weapon numbers the combat scripts read come through the creature's property modifiers this adds (armour, attack, defense and the rest named in `itemstatdefines`), so a fight needs no script on equip to have them; the scripts' own handlers of 11 and 12 (in `creature_core` and the item scripts) add what they add on top. Inferred; the scripts' handlers not read here.

## Item stats

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe, the toolset's scripts and the game's tables. "Read" is what the code shows; "inferred" is marked. An item's base item type (bitm_base's row) is the u16 at item+0x464, its MaterialType (materialtypes' row, which `ScaleEquippedItems` sets by level) the attribute at item+0x4FC.

### GetItemStat (305), the stat the combat scripts read

- `ScriptCommand::GETITEMSTAT` (`0x00665670`) pops the item and the stat type and pushes `ScriptBackend::ForScript_GetItemStat(item, type)` (0.0 for no item). Read.
- `ForScript_GetItemStat`: the stat type is a row of `itemstatdefines.gda` (table 109), whose `Label` names a column (`0x007530A0` reads it and hashes it). The value is `itemstats.gda` (table 110) at the item's base item row, that column, times `materialrules.gda` (table 159) at the material's row, the same column; the material's row is `materialtypes.gda`'s `Material` for the item's MaterialType (`0x00753C90`). A multiplier of 0 or less, or a missing column, counts as 1. Read. So: stat = itemstats[base item][stat] × materialrules[materialtypes[MaterialType].Material][stat].
- The stat types (`itemstatdefines`): 1 Armor, 2 Damage, 3 Defense, 4 Attack, 5 CritChanceModifier, 6 ArmorPenetration, 7 AbilityCostModifier, 8 MissileDeflection, 9 OptimumRange, 10 Spellpower, 11 RngdPenalty, 12 AttributeMod. `itemstats` has these as columns (and DamageRange, DSpeed, BaseAimDelay, Attribute0/1); a longsword's row: Damage 7, DamageRange 1.5, ArmorPenetration 2, CritChanceModifier 2, DSpeed −0.1. `materialrules` scales Damage, Armor, CritChanceModifier, ArmorPenetration and others: Iron 1.0, Grey Iron 1.1/1.2/1.1/1.15, Steel 1.2/1.4/1.2/1.3, Veridium 1.3/1.6/1.3/1.45.
- Not counted: the item's own properties (the .uti's PropertiesList); `GetItemStat` reads only the base item's row and the material. Item properties: see "An item's own properties" below.
- The scripts' use: `combat_h` attack rating adds `GetItemStat(weapon, ATTACK)`; critical chance `CRIT_CHANCE_MODIFIER`; ranged penalty distance `OPTIMUM_RANGE`; `core_h`'s `DmgGetWeaponBaseDamage` is `GetItemStat(weapon, DAMAGE)` (unarmed `COMBAT_DEFAULT_UNARMED_DAMAGE`), its max damage that times `itemstats`' DamageRange; the attribute factor `ATTRIBUTE_MOD`; `combat_damage_h`'s armour penetration `GetItemStat(weapon, ARMOR_PENETRATION) + GetCreatureProperty(attacker, PROPERTY_ATTRIBUTE_AP)`. Read in the scripts.

### The armour value, and stats that become creature properties

- The scripts read a defender's armour as a creature property: `combat_damage_h`'s `DmgGetArmorRating` is `GetCreatureProperty(defender, PROPERTY_ATTRIBUTE_ARMOR)` (11), rolled 70 % fixed and 30 % random. Read.
- The engine puts equipment's stats into those properties. `itemstatdefines`' `CreatureProp` column maps a stat to a creature property: Armor → 11, Defense → 10, AbilityCostModifier → 41, MissileDeflection → 22, Spellpower → 13 (the others 0, none). `0x0071B3C0(item, sign)` walks the stats with a property and adds sign × (itemstats × materialrules, as `GetItemStat`) to that property's MODIFIER (`0x0071B2D0` keeps the current value: `CreatureProperties::GetCurrent`, then the property's slot 7, which adds to +0x54, the modifier (`0x00795A30`), then the current put back). Read.
- Its callers: the creature's item-equipped handler (`0x006C16E0`, from `Equipment`'s ItemLocation slot 22, `0x00797D5A`) with +1, unless the creature's +0xC58 is set; the item-unequipped one (`0x006C19F0`, from `0x00797E67`) with −1 (inferred by symmetry; its call at `0x006C1A5D` read); and the scripts' `SetItemMaterialType` (`0x00756540`) with −1 then +1 around the change, on the wearer. Read. So a creature's armour total is its base (properties.gda) plus every equipped item's Armor stat, scaled by material, plus effects' modifiers.

### The four weapon commands (one implementation)

- `GetWeaponAttackRating` (693), `GetWeaponBaseDamage` (699), `GetWeaponStrengthModifier` (701) and `GetWeaponArmorPenetration` (703) share `0x0066A3B0`, which switches on the command. Each pops the attacker and an int (`nRightHandWeapon`, 1 the main hand, otherwise the off hand; 703 also pops the target first), takes the hand's weapon from the creature's equipment (`0x006A2B40`: its slot-0 and slot-1 item), and reads one `bitm_base` column at the weapon's base item: 693 `CombatRating` (float, `0x006A8680`), 699 `BaseDamage` (int, `0x006A86F0`), 701 would read `StrengthModifier` (`0x006A8770`) but never reaches it: its case (`0x0066A4B6`) looks up a target handle it never pops (−1, always null) and so always pushes the default −0.1f (`0x00B0839C`; corrected 2026-10-18 by lib-agent-2, "The weapon style" below), 703 `ArmourPenetration` (`0x006A87E0`, British spelling). No material, no attributes. Read.
- `bitm_base` has `CombatRating`, `StrengthModifier` and `BaseDamage`, but no `ArmourPenetration` column, so 703 answers 0.0 for every weapon (read: the table's columns; the read fails and the default 0 stays). The target 703 pops is not used.
- The shipped scripts barely use these four: `rules_h`'s `Rules_GetWeaponAttackRating`, `Rules_GetWeaponArmorPenetration` and `Rules_GetWeaponBaseDamage` wrap them, and nothing calls those wrappers (grep of the toolset's scripts). Combat goes through `GetItemStat` and the creature properties above.

## An item's own properties (read 2026-10-21 by lib-agent-2 with two subagents, for DAOParagonEngine)

- **Where they are** [C]: the item's +0x680 holds the property ids (`itemprps.gda` rows) and +0x69C their powers. They come from the .uti's PropertiesList and from `AddItemProperty` (`dev/script_commands/item_prop.md`). A rune's properties are under the item's +0x4BC.
- **On equip, each IPType-1 property becomes an effect on the wearer** [C]:
  - Who: the item-equipped handler 0x006C16E0 calls the item's 0x00759A40 at 0x006C17ED, after the base stats (0x0071B3C0), and only while creature +0xC58 is 0. 0x00759A40 also does the runes'.
  - The effect, made by 0x00759190 per entry:
    - its type is `itemprps.Effect`: 1012 `EFFECT_TYPE_MODIFY_PROPERTY` for the attribute, resistance and armour rows;
    - int 0 is `Int0`, the properties.gda id;
    - the float that `ScalingVector` names (`Float0` or `Float1`) is multiplied by the entry's power;
    - the creator is the item, and the duration type is 4 (not a script constant).
  - It goes on the creature's effect list (+0x70), and the scripts' `ApplyEffectModifyProperty` adds it to the property's modifier. Scripts see it in the effect list and in `GetCreatureProperty`'s total.
- **On unequip they are removed** [C for the call, I for the match]: 0x006C19F0 calls 0x00757B90 at 0x006C1A3E, which takes off the effects the item created. `RemoveItemProperty` on a worn item takes one off through 0x00757CF0.
- **Example, the Apprentice Robes** [C from data]: `gen_im_cth_mag_app.uti` has PropertiesList [{1002, power 1}]. Row 1002 "Increase Willpower" is Effect 1012, Int0 3, Float0 1, ScalingVector Float0. So the robes give +1 Willpower, and max mana rises by 5 (5 per point of Willpower).
- **IPType 2 is on-hit; the engine only raises an event** [C]:
  - **The event**: one `EVENT_TYPE_ITEM_ONHIT` (65) per landed blow and per IPType-2 id (0x00757FC0, queued by 0x00758180, runes included). Int 0 is the id, int 1 the power, object 0 the attacker and object 1 the item.
  - **Melee** (0x006A6080): from the attacker's weapon (slot 13 bite, else main hand, or off hand when the swing's animation name has `|Left`), and from the target's chest slot (4).
  - **Ranged**: built from the RANGEDAMMO item at the shot (0x006B6F60) and sent at impact (0x007E87C0).
  - **The scripts do the rest** (`rules_core.nss`:241–283, only in GM_COMBAT): they roll `itemprps.ProcChance` and call `ItemProp_DoEffect` (`sys_itemprps_h.nss`:89–466).
  - **The damage rows** deal `Float0` × power of damage type `Int0`. Elemental damage is further multiplied by 1 + property 55..59 / 100.
- **IPTypes 3 and 4** (requirements and flags) apply no effect.
- **The passive combat bonuses reach the scripts as creature properties** [C, the scripts]: flat damage 39 (`combat_damage_h.nss`:318, 359), armour penetration 53 (:143), critical damage 54 (:172), elemental percentages 55–59 (`effect_damage_h.nss`:256–290). Critical chance is 21 and 23, inside the native `GetCreatureCriticalHitModifier` [I: not read].
- **The script commands** [C]:
  - `GetItemProperties` (316, 0x00674260) returns the ids at +0x680, without duplicates, with the runes' when asked, and no powers.
  - `GetItemPropertyPower` (784) is the only reader of a power.
  - `GetItemStat` ignores properties.
  - `HasItemProperty` (302) and `Get/SetLocalItemProperty` (93/94) have no handler, and there is no `IsItemPropertyValid`.
- **Not read**: what 0x00757B90's filter matches on; how a loaded save restores worn items' effects; who receives the queued on-hit event.

## An item's value (read 2026-10-21 by lib-agent-2 with a subagent, for DAOParagonEngine)

Read 2026-10-21 by lib-agent-2 (subagent), GOG `game\DAOrigins.exe` (base 0x400000), `tools\daoexe.py --disasm`, ghilana (uti 3000-cost glove), and store.gfx decompiled with FFDec 26.3 (`java -jar ffdec.jar -cli -export script`; scratch `%TEMP%\lib-agent-2-ui-store\out\scripts\__Packages`, made from the CFX->CWS-renamed `lib-agent-2-guigfx-x\store.gfx`; inventory/popuplayer from `lib-agent-2-guiext-as`). C = read in code/data, I = inferred. Not seen in play.

### Short answers

1. **Item value** (the attribute at item+0x5A8; ItemCost 1001 in saves; what `GetItemValue` 291 pushes and the GUI token `<item>.Cost`), computed by one function, `0x00755430` (ID 18005, no name), in this order (C):
   `v = trunc( Cost * materialrules.CostMultiplier[ materialtypes.Material[MaterialType] ] )`
   `v += sum over item properties of itemprps.BaseCost[propertyId] * power` (only when the id list +0x680 and the power list +0x69C have the same length)
   `if base item Type == 7 (ammunition) and v > 0: v = max(1, trunc(v * 0.33))`
   `v += value (+0x5A8) of each rune item in the list at +0x4BC`
   `Cost` is the .uti's `Cost` field (read into +0x5B8, the "BaseCost" 1021 in saves). The stack size (+0x5F0) is **not** in it: +0x5A8 is a per-unit value. No base item column enters except `Type` (ammunition) and, in the callers, `RuneCount`; there is **no** `SetItemValue` script command and no script writer (the only value commands are `GetItemValue` 291 and the stack-size ones).
2. **The store screen computes its own price display**; it does not call 0x007EB6F0/0x007EB770. The Flash movie reads `<item>.Cost` (= +0x5A8) and `Store.MarkUp`/`Store.MarkDown` and applies `Math.floor(Cost * pct / 100)` itself, with the same rules (flag 1/2) as the native functions; the native pair (0x007EB6F0 pay, 0x007EB770 charge) is called only by the sell/buy message handlers, which move the money. The two agree, so the display equals the transaction. Inventory has no price display; the sell value appears only in the store's and the popup's own code.

### 1. The formula, with addresses

- **Single function:** `0x00755430` (this = item). Callers, all four (C, direct-call scan): 0x007566DC (`SetItemMaterialType` 0x00756540), 0x0075795C (template load 0x007572D0), 0x00759BF3 and 0x00759DCE (rune attach / detach). So the value is recomputed on template load, material change and rune change; **not** by `AddItemProperty`/`RemoveItemProperty` as far as the callers show (no other direct caller), nor by stack changes (C for the callers; whether properties added by script call 0x00755430 by another route, e.g. a virtual call, was not searched).
- **Step 1, cost x material** (0x00755430-0x007554D1, C): `materialtypes` (table 0x59 = 89) row `MaterialType` (item+0x4FC), column `Material` (hash 0x56BCE3D9) gives a row of `materialrules` (table 0x9F = 159); its `CostMultiplier` (hash 0x74E8EA95) is a float, **default 1.0** (fld1 at 0x00755433; a missing row leaves 1.0). `fild [item+0x5B8]` x multiplier, converted by the CRT float-to-int 0x008E34F0 (truncation, `cvttsd2si`), stored through the attribute's vtable +0x54 into +0x5A8.
- **Step 2, properties** (0x007554D3-0x007555E1, C): the two vectors at +0x680 (property ids) and +0x69C (powers), element size 16 (SmartData). If their sizes are equal, for each i: `itemprps` (table 0x6B = 107) row id, column `BaseCost` (hash 0xB28DB889) x power, added to +0x5A8 with 0x00621ED0 (value += arg). Unequal sizes: skipped.
- **Step 3, ammunition** (0x007555E3-0x00755642, C): `0x00752C70` (base item `Type`, per arrays.md; the test is `== 7`). If the value is above 0: `trunc(value * 0.33)` (double 0.33 at 0x00B1C4D8); if that is above 1 it is set to `trunc(value * 0.33)`, else to 1. This is applied to cost x material **plus properties**.
- **Step 4, runes** (0x00755644-0x007556C6, C): for each id in the vector at +0x4BC (not -1), `GetObjectById` (0x005D5870), add that item's own +0x5A8. A rune's own value is the same computation on the rune item.
- **Writers of +0x5A8** (scan for displacement 0x5A8 in code, then read each; C): 0x00755430 (above); the default/clear routine 0x00752FC8 (sets +0x5A8 and +0x5B8, +0x6D8 to 0 / 0x30D40...; ctor-like reset); the constructor/copy registration 0x00756BDC / 0x00756F4B (attribute init); the **savegame loader 0x007586B0**, which reads field 1001 straight into +0x5A8 (and 1021 into +0x5B8 if present), so **a loaded save keeps the saved value; it is not recomputed** (nothing at that point calls 0x00755430; C for this function, a save's items are not re-valued unless a recompute caller above runs later; I). Readers: the save writer 0x007544B1 (1001), the token/attribute-name table 0x007546F0 (ID 18003: names "Cost" -> +0x5A8 at 0x00754CE6 and "StoreFlag" -> +0x5C0 at 0x00754D4D, which is how the movies' `<item>.Cost` / `.StoreFlag` resolve), a comparison 0x007F845F (sorting by value, I), the store functions 0x007EB6F9 / 0x007EB779, GetItemValue 0x0066BD35.
- **Template load** 0x007572D0 (C): reads "Cost" (string 0x00B1C7F8, `Field` helper 0x00A37050) into +0x5B8 at 0x007578E3-0x007578FF; the properties list is read earlier in the same function (0x0075767x-0x00757808 region, I); then 0x007546F0 (attribute table) and 0x00755430 at 0x0075795C.
- **Check against data (real item):** `gen_im_arm_glv_lgt_ap5.uti` (ghilana): Cost 3000, Material 32, one property id 1504 power 5. `materialtypes` row 32: Material 21; `materialrules` row 21 "Drakeskin": CostMultiplier 18; `itemprps` 1504 BaseCost 2500. Formula: 3000 x 18 + 2500 x 5 = **66500** copper. I could not read the live value (no game); not compared. The save-derived data point in `SAVE_WRITER.md:328` (34 arrows: ItemCost 1320, BaseCost 1) fits the formula and shows the stack is not a factor: 1320 = 4000 x 0.33 (properties of 4000 summed before the 0.33; C for the arithmetic, I that those arrows carry such a property).
- **Not read:** whether `AddItemProperty`/`RemoveItemProperty` (script commands) reach 0x00755430 or leave +0x5A8 stale; the base-item `Type` hash at 0x00752C70 in this pass (taken from arrays.md); rune value use of `RuneCount`; the enciphered add-in tables (`itemprps_*`, `materialtypes_gwb`) that add rows.

### 2. Who prices what

#### Native (C, `stores.md:19-24` confirmed)
- 0x007EB6F0 (pays): `trunc( (double)pct / 100.0 * (double)value )` then `* max(n,1)`, pct = MarkUp (store+0x454) when the item's +0x5C0 state is 1, else MarkDown (+0x464). Disassembly shows `fild pct; fdiv [0xAED110]; fimul value; call 0x008E34F0`: a double divide by 100.0, truncation toward zero. 0x007EB770 (charges): the mirror, MarkDown when state is 2.
- Callers (direct-call scan, C): 0x007EB6F0 from 0x007EBD6D (will-buy check 0x007EBD00) and 0x007EC358 (sell, inside 0x007EC2C0); 0x007EB770 from 0x007ECB7F and 0x007ECF64 (buy, inside 0x007ECF00). 0x007EBD00 from 0x007EC329. **No GUI or movie path calls them**; nothing else reads the markups for display.

#### The store movie (C, store.gfx decompiled)
- `Store.as:58-60` (`__Packages/Store.as`): when it opens, `Store.m_nMarkDown = Number(GetValue("Store.MarkDown"))`, `Store.m_nMarkUp = Number(GetValue("Store.MarkUp"))`, and `ItemUtils.ms_nStoreMarkDown = Store.m_nMarkDown`. (The tokens' binding to store+0x464 / +0x454 is I, from the "Store" token binding in OpenStore; the native name hashing was not read.)
- Each list entry (`SharedLibrary/StoreItemEntry.as`, `GetCost`), with `Cost = GetValue(<item>.Cost)` (+0x5A8) and `StoreFlag` (+0x5C0, `STORE_FLAG_JUST_BOUGHT = 1`, `JUST_SOLD = 2` in `ItemUtils.as:6-7`):
  - store-owned entry: `JUST_SOLD` -> `floor(Cost * (MarkDown/100))`, else `floor(Cost * (MarkUp/100))`;
  - player's entry: `JUST_BOUGHT` -> `floor(Cost * (MarkUp/100))`, else `floor(Cost * (MarkDown/100))`.
  Identical to the native rules. It is shown per unit through `ItemUtils.FormatMoneyString` (100 copper = 1 silver, 100 silver = 1 gold; `ItemUtils.as:3-5,16-40`, the silver-per-gold constant on line 4 is not shown above, I from `COPPERS_PER_GOLD = 100 x SILVERS_PER_GOLD`) and is **not multiplied by the stack size** in the list. A store entry is greyed ("red") if `GetCost() > Party.Money.Total` (`IsTooExpensive`, store-owned entries only; unit price vs purse, so a stack's total is not checked there).
- The split popup (`popuplayer`, `PopupLayer.as:646-681`): on `GUIPopupLayer.ItemSplitAmount` it reads `GUIPopupLayer.ItemStack.Cost`, `.StoreFlag`, `Store.MarkUp`, `Store.MarkDown`, `Party.Money.Total`, and shows `amount x floor(Cost x pct/100)` with the same flag rules (Buy label: JUST_SOLD -> MarkDown else MarkUp; Sell: JUST_BOUGHT -> MarkUp else MarkDown), disabling Buy when the total exceeds the purse. That is the same as the native `unit_trunc x n`.
- Transactions: `Store.as:246-280` sends `Store.BuyItem(buyer, item, qty)` / `Store.SellItem(seller, item, qty)` (`SendMessageArgs`) and judges success by comparing `Party.Money.Total` before and after (sound only); the money moved is the native price, not a number the movie sends.
- **The tooltip's "Sells for"** (`ItemUtils.SellValue`, `ItemUtils.as:516-527`, used by the item tooltips in the inventory movie's shared code): `if ms_nStoreMarkDown > 0: floor(Cost * ms_nStoreMarkDown / 100)`; this ignores `StoreFlag`, so a just-bought item's tooltip says MarkDown while the native sale-back pays MarkUp. `ms_nStoreMarkDown` is set only by `Store.as:60`, and is 0 in the inventory movie, so the inventory screen has no value line (no one else assigns it: grep of inventory/popuplayer/store AS, C).

#### Inventory and selling elsewhere
- The inventory movie reads `<item>.Cost` into its `Item` (`SharedLibrary/Item.as:98`) but `ItemEntry.GetCost()` returns it unmodified and `InitMoney` runs only for entries with a money clip (store entries). Outside a store the player's screens show no price (C for the AS; the inventory scene's use of `InitMoney` is not traced to a movie clip, I).
- Selling uses the native handler 0x007EC2C0 (price from 0x007EB6F0; will-buy check includes the same price vs `MaxBuyPrice`/store money, `stores.md:22`); buying 0x007ECF00 (price from 0x007EB770).

### Not read
- Whether script-side `AddItemProperty`/`RemoveItemProperty` recompute +0x5A8 (all four direct callers of 0x00755430 are material, load and runes).
- The item `Type` read at 0x00752C70 and the properties-list read in the template loader, in this pass.
- The native binding of the tokens "Store.MarkUp" / "Store.MarkDown" (inferred).
- A live comparison of the 66500 figure, or of any item's displayed price, in play.
- The enciphered add-ins' `itemprps_*` / `materialtypes_gwb` rows.

## The store's and inventory's tab filters

Read 2026-10-21 by lib-agent-2, for DAOParagonEngine. Vanilla filters by `bitm_base.EquippableSlots` (a mask of 1 << `INVENTORY_SLOT_*`), not by `StorePanel`, and crafting by the item's name.

- **The message** [C]:
  - `GUIStore` `SetItemListFilterMessage` (0x00729A00) sets the filter on both of its lists (+0xC0, +0xD0); `GUIInventory`'s (0x00731B10) sets it on its list (+0x1FC) and keeps the value at +0x1F4.
  - Each list's `FilteredGroupedItemView` looks the value up in its map at +0x74 (0x007FBBD0), stores the filter at +0x5C (0x007FBBB0) and rebuilds the list (0x007FB570).
  - The rebuild skips each item whose filter's slot 1 returns false. It also skips a crafting recipe (the item's base item 0x6C, 108) that the party's list at +0x20C+0x38 already holds (0x0073FD60) [C for the test, I that this means "already learned"].
- **The map**, filled at 0x007FAF5C..0x007FB0ED [C]:

| Value | Class (vtable) | An item passes when |
|---|---|---|
| 0 | `ItemFilterAll` (0xB248F4) | always (0x00722AE0) |
| 1 | `ItemFilterWeapons` (0xB24900) | `EquippableSlots & 0x10005`: main hand, ammunition, Shale's right arm (0x0080E010) |
| 2 | `ItemFilterArmour` (0xB2490C) | `EquippableSlots & 0xC80F0` (chest, head, boots, gloves, Shale's chest, the dog's warpaint and collar), or `EquippableSlots == 2`, off hand only, so shields (0x0080E030) |
| 3 | `ItemFilterAccessories` (0xB24918) | `EquippableSlots & 0x1E00`: the rings, the amulet, the belt (0x0080E070) |
| 4 | `ItemFilterOther` (0xB24924) | `EquippableSlots & 0xD9EF7` is 0 and the item is not crafting (0x0080E0C0) |
| 5 | `ItemFilterCrafting` (0xB24948) | the item's tag (+0xB8) has `_im_cft` first at character 3 (string 0xB26568; 0x0080E090) |
| 6 | `ItemFilterPlot` (0xB24930) | the byte at item+0x2C4 (the plot flag) (0x0080E100) |
| 7 | `ItemFilterBuybackSellback` (0xB2493C) | the value at item+0x5C0 is non-zero (0x0080E120) |

- **What that gives with vanilla's `bitm_base`** [C for the table's values, read with ghilana]:
  - Weapons: every weapon (main-hand mask 1 or 3), arrows, bolts and staff projectiles (4), and Shale's right arm (65536). Shale's left arm (131072) is not a weapon.
  - Armour: body armour and clothing (16), helmets (32), boots of every weight (64), gloves (128), shields (2), Shale's chest (32768), and the dog's collar and warpaint. Shale's body (16384) is not armour.
  - Accessories: rings (1536), amulets (2048) and belts (4096).
  - Crafting: the tags `gen_im_cft_*` and `gxa_im_cft_*`, both the recipes (base item 108) and the reagents, herbs, poisons, traps and Awakening's runes. The `StorePanel` of the recipes' base item (4) plays no part [I that item tags match their template names, as all 210 such templates do].
  - Other: what has none of those slots and is not crafting: gems, keys, gifts, quick items, codex items, and also Shale's body and left arm, the creature weapons (8192) and the ballista bolts (0).
- **A value not in the map** [I]: the lookup is the map's `operator[]` (0x004DFE40), which yields a null filter, and a null filter skips no item, as 0 does.

## The weapon style, and GetWeaponStrengthModifier

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG, base 0x400000) with `tools\daoexe.py --disasm/--refs`, a scratch rel32 call scan, and `bitm_base` from `packages\core\data\2da.erf` (`tools\gda.py --dump bitm_base`; base table, add-ins' M2DA rows not merged). No Ghidra, no game. **Checked** = read in the disassembly or the table; **inferred** = a reading of purpose.

### 1. Weapon style (GetWeaponStyle, 354)

#### The chain (checked)

- `GetWeaponStyle` → `0x006A3C30`: `[creature+0xC60]` (combat data), returns its `+0x5C`; 0 when there is no combat data.
- `+0x5C` is written only by `0x007C3600` (`__thiscall` on the combat data, one arg: a set record). It copies the record (`+0x1C` id, `+0x20` and `+0x3C` the two .anb names) and maps the id:

| Record | id | Trees (UTF-16 strings) | Style written to `+0x5C` |
|---|---|---|---|
| `0x00C38C04` (default, ctor at `0x00AD2F20`; also built on the stack by `0x006A4450`) | **−1** | `""`, `""` | **0** |
| `0x00C38C40` (`0x00AD2FC0`) | 0 | `OW.anb`, `OWOverlay.anb` | 1 |
| `0x00C38C7C` (`0x00AD3060`) | 1 | `WS.anb`, `WSOverlay.anb` | 1 |
| `0x00C38CB8` (`0x00AD3100`) | 2 | `TW.anb`, `TWOverlay.anb` | 3 |
| `0x00C38CF4` (`0x00AD31A0`) | 3 | `DW.anb`, `DWOverlay.anb` | 2 |

  The code: id == [C7C] or [C40] → 1; == [CF4] → 2; else `(id != [CB8]) - 1 & 3` → 3 for TW, 0 for anything else. These five are the only set records in the exe (refs to the four statics: `0x006A9167`, `0x006BF98E`, the chooser, the setter, the static ctors/dtors). **There is no ranged, staff or unarmed set**: those creatures get the id −1 record with no trees.

- `0x006B8D40` (CCCreature, no args) is the only caller of both the chooser and the setter:
  1. Gate `0x006A25A0` → `0x0069F030`: the appearance (`[+0x7B0]+0x58`, `apr_base` = table 2) column **`CombatModelTree`** (hash `0xDE80B418`) must contain `"combat_humanoid"` (`0x00B0F318`). Otherwise nothing is chosen and the style stays what it was (0 from construction, inferred).
  2. The chooser `0x007C3500` (on the combat data; args: out record, the creature's equipment `+0xBE0`) picks a record (below).
  3. If the new id differs from the current (`0x00616C20` reads it), `0x007C3600` stores it, then `0x006B5B30` is called four times: (2, old base tree), (2, old overlay), (0, new base), (0, new overlay) (checked; inferred: the old trees taken out of the animation player and the new put in).

#### The decision table (the chooser `0x007C3500`, checked)

Items are read through `0x007F4EA0(slot)` → `0x00761260({slot, set −1})`, slot 0 MAIN and slot 1 OFFHAND of the active weapon set (set −1 = active, checked 2026-10-22: ITEMS.md, "Read for DAOParagonEngine #169 (items)"). An item's `+0x6E0` is `bitm_base` **`Type`** (hash `0x12A02374`, stored at load by `0x007547FF`; checked). The two-handed test `0x00A664C0` on the item's base-item record (`item+0x440`, its `+0x24` the bitm_base row, so `item+0x464`) is `bitm_base` **`EquippableSlots`** (hash `0xD5B0AE22`, `0x00A66440`) **== 1** (`0x00A66470(0)` = 1<<0, the MAIN slot's bit alone). `WeaponWield` (`0x1DE44006`) is **not** read.

| Main hand | Off hand | Record | GetWeaponStyle |
|---|---|---|---|
| empty | anything (incl. a dagger) | default (−1) | **0** |
| `Type` ≠ 2 (bow/crossbow 6, staff 5, …) | anything | default (−1) | **0** |
| `Type` 2 and `EquippableSlots` == 1 | anything (not looked at) | TW (2) | **3** |
| `Type` 2, `EquippableSlots` ≠ 1 | empty | OW (0) | **1** |
| `Type` 2, `EquippableSlots` ≠ 1 | `Type` 3 (shield) | WS (1) | **1** |
| `Type` 2, `EquippableSlots` ≠ 1 | `Type` 2 (melee weapon) | DW (3) | **2** |
| `Type` 2, `EquippableSlots` ≠ 1 | any other `Type` | default (−1) | **0** |

`bitm_base` rows that matter (checked): Longsword 2, Dagger 3, Mace 4, Waraxe 49: Type 2, EquippableSlots 3 (one-handed). **Greatsword 5, Maul 36, Battleaxe 40: Type 2, EquippableSlots 1 → TW → 3** (two-handed = 3 confirmed from the code; not played). Shields 6/7/8/13/14: Type 3, EquippableSlots 2. Shortbow 19, Longbow 20, Crossbow 21: Type 6, EquippableSlots 1. Staff 16: Type 5. Creature Weapon 23/56/57: Type 2, EquippableSlots 8192 (→ OW in MAIN). The `BITE` slot (13) is not looked at.

#### The play results explained (checked against the table)

- **Bow → 0**: main `Type` 6 is not 2, so the chooser returns the default record (id −1, no trees); not a "ranged set", there is none. (Bow animations come from the appearance's own trees, inferred.)
- **Two daggers → 2**: main Type 2, EquippableSlots 3; off Type 2 → DW.
- **One dagger → 1**: off empty → OW.
- **Nothing → 0**: main empty → default.
- **Off-hand dagger alone → 0**: the chooser reads MAIN first and returns the default when it is empty; the off hand is never read.
- **Sword and shield → 1**: off Type 3 → WS.

#### When it is recomputed (checked: the three direct calls to `0x006B8D40`)

- `0x006BE1F8`, in `0x006BDC70`, the creature's animation player set up with its trees (COMBAT.md), gated by the same `0x006A25A0`: at spawn/load/appearance change.
- `0x006C1808`, in the on-equip `0x006C16E0` (after the appearance's slot `0x00748DA0`).
- `0x006C1A8B`, in the on-unequip `0x006C19F0`, **skipped when `0x006AB100` answers** (the creature is dead, vtable `+0x1D0`, and something about its `+0x790` object; inferred: a corpse being looted keeps its set).
- **Weapon-set switch** (`SwitchWeaponSet` 273 `0x0065CCC0` → `0x006A8FA0` → equipment `0x007977D0`): no direct call to `0x006B8D40`; the switch takes the old set's items out of their slots (`0x00762100(…, item, −1)`) and equips the new set's through `0x006BC7F0`. The recompute then comes from the on-unequip/on-equip handlers, reached virtually (`Equipment`'s ItemLocation, ITEMS.md: `0x00797D5A` → on-equip, `0x00797E67` → on-unequip). Read 2026-10-22 (ITEMS.md, "Read for DAOParagonEngine #169 (items)"): the remove side is checked, each old item's on-unequip runs as the active set's; the new set's on-equip is still inferred, since the active set changes only after its items go in.
- No other writer of `+0x5C`; no per-frame recompute. Non-humanoid appearances (CombatModelTree without `combat_humanoid`) never change it.

(Other readers of the record id, not needed here: `0x006A9140` maps OW/WS → 0x150, TW → 0x151, DW → 0x152; `0x006BF960` maps them → 0x16C/0x16D/0x16E.)

### 2. GetWeaponStrengthModifier (701) and DEPRECATED_GetCreatureStrengthModifier (700)

#### The handler (checked)

`0x0066A3B0`, shared by 693/699/700/701/703; `eax = cmd − 693`, byte table `0x0066A590`, jump table `0x0066A578`: 693 → `0x0066A424`, 699 → `0x0066A455`, 700 → `0x0066A490`, **701 → `0x0066A4B6`**, 703 → `0x0066A4EC`.

Prologue (all cases): the float result is initialised from **`[0x00B0839C]` = −0.1f** (`0xBDCCCCCD`), the int (nRightHandWeapon) to 1, the target object slot and `edi` to −1; the attacker is popped and looked up (`GetCreatureById` `0x005D58C0` → esi).

Case 701 (`0x0066A4B6`):
1. pops `nRightHandWeapon`;
2. calls **`GetCreatureById(edi)` with edi = −1**, a target lookup copied from 703's case; 701 never pops a target;
3. if the attacker **or that lookup** is null → `0x0066A54A`: pushes the initial float, **−0.1**;
4. only otherwise would it call the backend `0x006A8770(hand)`.

`GetCreatureById(0xFFFFFFFF)` is always null (checked): `0x00A58190` → `0x004D5EB0` returns 0 at once for handle −1. **So 701 returns −0.1 for every call**: any weapon, either hand, no weapon, even an invalid attacker. Only a failed int pop differs (−639, script error). That is the −0.1 seen in play for the longsword and the daggers.

The unreachable backend `0x006A8770` (checked): `0x006A2B40` gives the creature's slot-0 and slot-1 items; hand == 1 → main, else off; `GetItemById` `0x005D5870`; reads `bitm_base` (table 6) **`StrengthModifier`** (hash `0x1772174C`) at the item's base row `item+0x464`; 0.0 when there is no item or the read fails. It would have answered 1.0 for a longsword or dagger (bitm_base: Longsword 1.0, Dagger 1.0, Longbow 0.0, Staff 0.0). No `−1.1` or other arithmetic anywhere: the −0.1 is purely the handler's default constant.

#### Compared with 700 (checked)

Case 700 (`0x0066A490`) pops nothing more, pushes an **int**: 0 if the creature exists and its stats attribute `+0x7C4` is non-null, else −1 (`edi`'s initial −1). Neither command reads a weapon or an attribute: 700 is a stub returning 0, 701 is a broken case returning its default −0.1.

#### Side effects of the same default (checked)

- 693 `GetWeaponAttackRating` with an invalid attacker → −0.1 (not 0); its backend `0x006A8680` reads `CombatRating` (`0xA2CE7725`) without checking the read, 0.0 with no weapon.
- 703 `GetWeaponArmorPenetration` with an invalid attacker or target → −0.1.
- 699 `GetWeaponBaseDamage` pushes an int, −1 for an invalid attacker.

#### What the scripts use instead (checked)

No script in `toolset\extract\scripts` calls 701 or 700. The damage scripts' strength factor is `GetWeaponAttributeBonusFactor` (`core_h.nss`): `GetItemStat(oWeapon, ITEM_STAT_ATTRIBUTE_MOD)`, or `UNARMED_ATTRIBUTE_BONUS_FACTOR` when there is no weapon (used in `combat_damage_h.nss:289`, `core_h.nss:2860`). `2da_data_h.nss`'s `GetItemStrengthModifierBaseValue` (bitm_base `StrengthModifier` through `GetM2DAFloat`) is defined but not called.

This corrects "The four weapon commands" above, which said 701 reads `StrengthModifier`.

## ScaleEquippedItems (386): a creature's equipment set to a level's material

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, a call scanner over `.text`, `tools\gda.py`, and ghilana's merged tables, for DAOParagonEngine. Not seen in play. "(checked)" means read in the disassembly or the table; "(inferred)" means not. Builds on `dev/ITEMS.md` (item stats, `SetItemMaterialType`), `dev/PROPERTIES.md` (property slots), `dev/COMBAT.md` (0x0071EAC0, ability add), and `dev/script_commands/stats.md` (254, 333).

### The chain
- **Handler** `ScriptCommand::SCALEEQUIPPEDITEMS` 0x006664D0 (not shared): pops the object, then the int `nTargetLevel`; `GetObjectById` 0x005D5BE0, slot +0x10 (as a creature); a non-creature or invalid id does nothing; then `ScriptBackend::ForScript_ScaleEquippedItems` 0x006A88F0 (ID 14612), `__thiscall(creature, level)`, `ret 4`. Returns 0 (checked).
- **Script caller**: `Core Scripts\creature_core.nss:469-473`, `EVENT_TYPE_SPAWN`: `nLevelToScale = AS_GetCreatureLevelToScale(OBJECT_SELF, AB_GetAreaTargetLevel(OBJECT_SELF))`, `AS_InitCreature`, then `ScaleEquippedItems(OBJECT_SELF, nLevelToScale)`. Also `_Debug\active.nss:29` (debug). `script.ldf:8122-8133` says "area must have a valid area_id… creature must not be a party member" (checked).

### 0x006A88F0, step by step (checked)
1. `if (creature+0xCC4 != 0 && level == 0) return;` creature+0xCC4 is the byte **ItemsScaled**: saved and loaded as GFF4 field 16474 `SavegameCreatureItemsScaled` (save 0x006B2825, load 0x006BEDE5), 0 in the constructor (0x006BA6FD).
2. `if (creature+0xBD4 != 0) return;` +0xBD4 is the template's `Combatant` (`GetCombatantType` 763 reads it): `CREATURE_TYPE_NON_COMBATANT` (1) creatures are never scaled.
3. `if (CCParty::IsInParty(creatureId)) return;` (0x00631830 on the party from 0x005D57C0): **party members are never scaled**.
4. `creature+0xCC4 = 1` (set before any item is looked at, whether or not anything changes).
5. Area: `CCObject::GetArea` (slot +0xA0, 0x00695CD0); `areaId = area+0x6B4` (0 with no area). area+0x6B4 is the area's predefined variable 2, `AREA_ID` (written at area load 0x0068835C through the variable lookup 0x00702E90 with index 2; index 2 is registered with the UTF-16 string "AREA_ID" at 0x004E8608 → 0x00AF13B4).
6. `if (areaId == 0 && level <= 0) return;`
7. Level used: `L = level` if `level > 0`; else `L = areadata[areaId].TargetLevel` (table 225 `areadata`, column hash 0x39E0446F = `TargetLevel`, through 0x00A2CCC0), **−1 if the row/cell cannot be read**. So the area is needed only for `level <= 0`; a positive level never reads it.
8. `col = L / 3` as **signed integer division truncating toward zero** (`imul 0x55555556`, high word + sign bit). `if ((unsigned)col > 7) return;` **No clamp**: a column outside 0..7 (L ≥ 24, or L ≤ −3) makes the whole call do nothing.
9. For each of 12 equipment slots, in the order of the dword list at 0x00B0F880..0x00B0F8AC: **0 MAIN, 1 OFFHAND, 2 RANGEDAMMO, 4 CHEST, 5 HEAD, 6 BOOTS, 7 GLOVES, 9 RING1, 10 RING2, 11 NECK, 12 BELT, 13 BITE** (CLOAK 8 and the Shale/dog slots 14-19 are not visited):
   - item = equipment(creature+0xBE0)+0x24 → 0x007F4EA0(slot), which looks the slot up with weapon set −1, the active set (ITEMS.md, "Read for DAOParagonEngine #169 (items)"). Skip if none.
   - skip unless 0x00752C80: `item+0x6E4 != 0`, which is `bitm_base` (table 6) column **`ShowMaterial`** (hash 0x66F63962) at the item's base item, loaded at 0x00754822. ShowMaterial is 1 for weapons (longsword, dagger, mace, greatsword, maul, battleaxe, waraxe, bows, crossbow, staff), shields, armour, gloves, boots and helmets (32 base items); 0 for rings, amulets, belts, ammunition, clothing, mage hoods, creature weapons (bite), runes (checked in bitm_base). So rings/neck/belt/bite/ammo slots are visited but never change.
   - skip unless `item+0x50C` (the item's **MaterialProgress**: the template field "MaterialProgress", string 0x00B1C880, read at 0x00757571; also what `GetItemMaterialProgression` 369 returns) is non-zero.
   - `mat = ts_material[MaterialProgress].Material{col+1}`: table **199 `ts_material`** ("Treasure System Material"), the column hash from the 8-entry table at 0x00B0F860 = `Material1`…`Material8` (checked by hash). Default 0; **if the read fails or the cell is 0, the item is skipped** (no change).
   - otherwise `ScriptBackend::ForScript_SetItemMaterialType(item, mat)` 0x00756540 (ID 18027), **even if mat equals the current material** (no comparison).

### Column from level (checked)
| Level L | col (0-based) | ts_material column |
|---|---|---|
| ≤ −3 (only via an area TargetLevel) | negative | none: call does nothing |
| −2..2 | 0 | Material1 |
| 3..5 | 1 | Material2 |
| 6..8 | 2 | Material3 |
| 9..11 | 3 | Material4 |
| 12..14 | 4 | Material5 |
| 15..17 | 5 | Material6 |
| 18..20 | 6 | Material7 |
| 21..23 | 7 | Material8 |
| ≥ 24 | ≥ 8 | none: call does nothing |

So, in the peer's 1-based terms, column = L ÷ 3 + 1 with integer division, valid 1..8; **Material9..Material15 are never read by the engine** (only 8 hashes in the exe; checked). In the shipped `ts_material` (62 rows, core only; the one enciphered add-in file `ts_material_kcc` unread) the progressive rows are 43-48 and 49-53, 59; every row's Material8 equals Material9..15 except 59 "Robe, Cloth" (Material8 86 = Material9+). E.g. row 46 "Armor, All Metal": 1, 2, 8, 4, 5, 3, 6, 6 (materialtypes rows).

### Matching the peer's measurements
- "Above 23 leaves items alone": step 8 (col 8+ returns). Checked.
- "A column past 8 changes nothing": same bound; and the data's Material9+ equal Material8 anyway.
- "Below 1 leaves items alone": the engine does **not** use a level ≤ 0 as given. Level 0 with ItemsScaled already 1 (every spawned non-party creature: the spawn call sets it) returns at step 1. A negative level, or 0 on a never-scaled creature, scales to the **area's** `areadata.TargetLevel` (step 7), or does nothing if the area has no `AREA_ID` (step 6). Since the spawn scaling used a level derived from the same TargetLevel, re-scaling to it usually lands on the column already applied, which looks like "no change" (inferred). A creature in an area whose TargetLevel lies in a different column than its current material would change at level −1.
- A party member or a non-combatant is never changed at any level (steps 2-3).

### What SetItemMaterialType changes (0x00756540, checked in outline)
1. On the wearer (creature id at item+0x4A8, `GetCreatureById`): `0x0071B3C0(item, −1.0)` takes the item's stats off the creature's property modifiers (constant −1.0 at 0x00AE2E00).
2. Sets MaterialType (item+0x4FC, attribute slot +0x54).
3. `0x0071B3C0(item, +1.0)` puts them back with the new material: per `itemstatdefines` row with a `CreatureProp`, `itemstats[base][stat] × materialrules[materialtypes[Mat].Material][stat]` (ITEMS.md "Item stats"). So Armor/Defense/Spellpower/… modifiers are recomputed; `GetItemStat` (Damage, Attack, crit, AP read by the combat scripts) follows the new material live.
4. If the wearer has an appearance (creature+0x7B0) and the item's equipped slot maps to an appearance slot < 7 (0x007480D0): the model's material is changed (0x00747DD0), the part reloaded (0x006C1400), 0x006A8580(1,1) (inferred: appearance refresh).
5. Runes: compares the item's rune vector size (item+0x4BC) with 0x00753D90 (bitm_base `RuneCount` hash 0x017132EC, with a materialrules factor; inferred: the new rune-slot count); if fewer slots now, the runes past the count are destroyed (object slot 0 with 1), and the vector resized.
6. The holder (item+0x4E8) is notified (slot +0x28), 0x00755430 recomputes the item's value (`materialtypes.Material` hash 0x56BCE3D9 → `materialrules.CostMultiplier` 0x74E8EA95), then item slot +0x64 (not read).

### Not read

- Item slot +0x64 at the end of SetItemMaterialType; 0x00753D90 in full.
- Whether the enciphered add-in `ts_material_kcc` adds rows (the running game reads it; Eluvian can).
