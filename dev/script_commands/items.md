# Script commands: items (DAOParagonEngine #27 baseline)

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

### GetMaxItemStackSize (299)
- **Handler:** 0x0066A810, shared with 300 (`cmp [esp+0x10], 0x12B` picks 299). Calls `0x00753C60` (stacks?) and `0x00753070` (`bitm_base.StackSize`).
- **Does:** pushes `StackSize` of the item's base item row if the item stacks, else 1. Checked.
- **State:** item+0x464; GDA table 6 column `StackSize`. Checked.
- **Edge cases:** no item or not an item (creature, placeable): 0. A non-stacking item answers 1 even where `StackSize` is 0 (runes). An item being destroyed answers 1. Checked.

### GetItemStackSize (300)
- **Handler:** 0x0066A810 (shared with 299, the non-299 branch at 0x0066A869), `SmartData::Attribute::Get` on item+0x5F0.
- **Does:** pushes the item's current stack size if it stacks, else 1. Checked.
- **State:** item+0x5F0. Checked.
- **Edge cases:** no item or not an item: 0; non-stacking or being destroyed: 1, whatever +0x5F0 holds. An "infinite" item (+0x650) answers its stored count. Checked.

### SetItemStackSize (301)
- **Handler:** 0x0066A8A0. Pops the item, then the int.
- **Does:** only for an item that stacks: n is clamped to at most `StackSize`, a negative n becomes 0; if that differs from +0x5F0, item vtable +0x1CC(n) sets it. **If the asked n was ≤ 0, the item is then destroyed** (vtable +0x198(0): out of its holder and destroyed). Checked. `script.ldf` says "clamps between 1 and MaxStackSize"; the code does not clamp to 1.
- **State:** item+0x5F0 (through +0x1CC). Checked.
- **Edge cases:** no item, not an item, a non-stacking item, an item being destroyed: nothing. No merge or split with other stacks; nothing re-checks the holder's slot count (inferred: the +0x1CC setter not read past the call). Checked except as marked.

### GetItemAttackRating (134)
- **Handler:** 0x0065E230; `GdaTables::GetFloat(6, base item, 0xA2CE7725 "CombatRating")`.
- **Does:** pushes `bitm_base.CombatRating` at the item's base item row (longsword 2.0). No material, no properties. Checked.
- **State:** item+0x464. Checked.
- **Edge cases:** no item or not an item: 0.0; a missing cell leaves 0.0. Checked.

### GetBaseItemType (135)
- **Handler:** 0x0065E2C0.
- **Does:** pushes the u16 at item+0x464 (the `bitm_base` row, `BASE_ITEM_TYPE_*`). Checked.
- **Edge cases:** no item or not an item: 0 (`bitm_base` row 0 is INVALID). Checked.

### GetItemMaterialType (136)
- **Handler:** 0x00669F20; `SmartData::Attribute::Get` on item+0x4FC.
- **Does:** pushes the item's MaterialType (a `materialtypes` row; `dev/ITEMS.md` has what it scales). Checked.
- **Edge cases:** no item or not an item: 0. Checked.

### GetItemAbilityId (190)
- **Handler:** 0x00668A20, the shared object getter (also 141, 160, 411, 413 among others); for a creature the switch (table 0x00668CE8/0x00668CFC) sends 190 to the default; the item branch is at 0x00668C2D: `Attribute::Get` on item+0x660.
- **Does:** pushes the item's ability id (an `abi_base` row). Checked.
- **State:** item+0x660. Checked.
- **Edge cases:** a creature, placeable or anything not an item: 0 (`ABILITY_INVALID`). No object: 0. Checked. `CCCreature::ForScript_CanUseItem` (0x006AE8B0) treats ability 200000 specially (not read further).

### SetItemAbilityId (191)
- **Handler:** 0x0065E330 → `ScriptBackend::ForScript_SetItemAbilityId` (0x00752D80).
- **Does:** pops the item and `nAbilityId` only; sets item+0x660 (attribute vtable +0x54), then, if the item has a holder (+0x4E8), the holder's vtable +0x28(item) (inferred: "item changed"; a no-op `ret 4` in `Inventory`), then item vtable +0x64 (not read). Checked.
- **Edge cases:** **`nPower` is never popped**, and the VM's ACTION opcode (0x00A30962) does not clean the stack after a command, so a three-argument call leaves an int behind (checked; no shipped script calls 191). An equipped item's granted ability is not re-applied (inferred; `script.ldf`: "Do not use on equipped items"). No item: nothing. Checked.

## Not read

- `0x00762E90` (move all) and `0x00762010` (add at a slot): whether stacks merge into existing ones and how the slot count is enforced.
- The `Equipment` location's remove/add callbacks on a move (assumed: the unequip/equip of `dev/ITEMS.md`).
- What leaving the party pool does to a creature's +0xBF0 (no other caller of `0x006A8C10`).
- The readers of creature+0x5F4 other than `ForScript_CanUseItem`; the GUI's use of it.
- `0x00A46E60` and `0x0074E640` in full (the trap patch's effect on paths).
- The money attribute's setter (a clamp there); `0x00629690`, the party's money notice.
- `0x0062EE50` (what the follower map's slot and set words are exactly); the iteration order of `0x007613A0` (first match).
- The default backpack size (set from a template or the party at start).

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

### SetItemMaterialType (137)
- **Handler:** 0x00669F90 (not shared); `GetItemById` 0x005D5870, then item `SetMaterialType` 0x00756540(item, nMaterialType).
- **Described in:** `dev/ITEMS.md` ("What SetItemMaterialType changes").
- **Does:** takes the item's stats off the wearer, sets MaterialType (item +0x4FC), puts them back, refreshes the model, trims runes and recomputes value.
- **Edge cases:** an invalid or non-item object does nothing and returns 0 (checked). **The command does nothing at all if any slot of the item's rune vector (+0x4BC) holds a value other than -1, that is, if a rune is set in it**: the handler walks the vector and returns before calling 0x00756540 (checked, 0x0066A013-0x0066A048). The ITEMS.md note's "runes past the count are destroyed" step is therefore reached only for an item with no runes in it (the vector holds empty slots). nMaterialType is not range-checked here (inferred). A failed pop returns -639 (checked).

## Not read

- The rune vector's element meaning past "-1 is empty" (taken from the compare against the sentinel at 0x00C313C0).
