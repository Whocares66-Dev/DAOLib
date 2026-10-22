# Script commands, group "stores": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" is read in the disassembly; "(inferred)" is not.

Shared facts, all checked:
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`. It returns 0, -639 (0xFFFFFD81) on a failed pop, -638 on a failed push; either fails the script. Pops are `CVirtualMachine::StackPopObject` 0x00A2F6D0 and `StackPopInteger` 0x00A2F3C0, the push `StackPushInteger` 0x00A2D650.
- **The store lookup:** `CClientExoApp::GetObjectById` (0x005D5BE0), then the object's vtable +0x50, cast slot 20, which only `Store` answers (`dev/object_casts.csv`). Anything else (a creature, a placeable merchant, OBJECT_INVALID) is "no store": a getter pushes its default, a setter does nothing, the script goes on. (checked)
- **The CSV's shared handlers are right.** The registrations pass the same function: 448 and 449 both 0x0065A980 (0x0067787C, 0x00677892); 452 and 313 both 0x0065AB50 (0x006778D4, 0x00677900). None of these handlers reads the command number, so the pairs behave identically. (checked)

**The `Store` object** (vftable 0x00B242D4; bases `CCObject` at 0, `ItemPossessor` at +0x440). Fields, from the constructor 0x007ED840, the copy constructor 0x007EC4F0, the template load 0x007EBDD0 (UTM field names in quotes) and the save/load 0x007EB510/0x007EC0B0 (GFF4 ids):
- +0x444 the store's items (an `Inventory`-like container; `0x00762C40` and `0x00763040` move items in and out). (checked that they are used so; its class inferred)
- +0x454 MarkUp, a `SmartData` attribute (read with `Attribute::Get` 0x0053CC40, set through its vtable +0x54); "MarkUp", 16151 SavegameStoreMarkUp; default 100. (checked)
- +0x464 MarkDown, the same kind; "MarkDown", 16150; default 100. (checked)
- +0x474 money in copper, a plain int; "StoreGold", 16152; default -1, meaning infinite. (checked)
- +0x478 MaxBuyPrice, a plain int; "MaxBuyPrice", 16153; default -1, meaning no limit. (checked)
- +0x47C WillNotBuy and +0x48C WillOnlyBuy, two lists of base item rows (each element's "BaseItem"); 16154, 16155; the items are 16156 SavegameStoreItemList. (checked)

**Prices** (native, checked). An item's value is its attribute +0x5A8 (what `GetItemValue` 291 pushes). Item attribute +0x5C0 is a trade state: 0 none, 1 bought from a store, 2 sold to a store.
- The store pays (`0x007EB6F0(item, n)`): `trunc(pct / 100.0 * value) * max(n, 1)`, with pct = MarkUp if the item's state is 1 (a just-bought item sells back at the price paid), else MarkDown.
- The store charges (`0x007EB770(item, n)`): the same, with pct = MarkDown if the state is 2 (buying back what was sold, at the price received), else MarkUp.
- The truncation is `0x008E34F0` (the CRT's float-to-int, inferred). The per-item price is truncated before the multiply by n. **No skill, attribute or relationship enters either price**: the only terms are the item's value, the two percentages and n. Coercion has no native effect on prices; any haggling is scripts changing MarkUp/MarkDown (as `den200pt_generic` sets MarkDown 50) or opening a different store.
- **Will the store buy** (`0x007EBD00(item, creature, n)`): if WillNotBuy is non-empty, the item's base item row (word item+0x464) must not be in it, **and WillOnlyBuy is then ignored**; otherwise, if WillOnlyBuy is non-empty, the row must be in it. Then the total price must be ≤ the store's money (unless -1) and ≤ MaxBuyPrice (unless -1). MaxBuyPrice is compared with the **total** for the n items, not the price of one. (checked; that 0x0086BEE0 is "empty" is inferred from its use)
- **Selling** (`Store::HandleMessage(SellItemMessage)` 0x007EC740 → 0x007EC2C0): after the check, the item goes into the store (+0x444), its state becomes 0 if it was 1, else 2; the store's money drops by the price (unless -1), the seller's purse (`Inventory`+0x28, `dev/script_commands/inventory.md`) gains it (0x007C9FE0). A stack of n > 1 that stacks goes in one call; otherwise items are sold one at a time. (checked)
- **Buying** (`Store::HandleMessage(BuyItemMessage)` 0x007ED340 → 0x007ECF00): refused if the price exceeds the buyer's purse (`Inventory`+0x28 → attribute +0x90); the store has no check of its own. The taken item's infinite-stack flag (+0x650) is cleared and its state becomes 0 if it was 2, else 1; the store's money rises by the price (unless -1), the purse falls (0x007CA020). If the item cannot be placed with the buyer, an infinite-stack copy is destroyed, any other goes back to the store with its state restored. (checked)
- The state is on the item, not per store (inferred consequence): an item bought from one store sells to another at that store's MarkUp.

### OpenStore (447)
- **Handler:** 0x0065A8B0 (not shared). It sends an `OpenStoreMessage` (vftable 0x00B07C10) with `Messages::Send` (0x004D20C0) to the store's receiver (store+0x18), handled by `Store::HandleMessage(OpenStoreMessage)` 0x007EB9D0. (checked)
- **Does:** the message handler binds the store to the `SmartData` token "Store" (`TokenManager::Get` 0x00455BF0, then 0x005C6650 with the hash of "Store"; that this binds the GUI's data source is inferred), then sends `SetFullScreenGUIMessage("Store")` to the GUI system (app+0x158). That opens the full-screen GUI `Store` in the GUI game mode, which pauses (`dev/GUI.md`, `dev/TURN_BASED.md`; the shared "Full-screen GUI" fact in `dev/script_commands/inventory.md`). (checked apart from the binding's meaning)
- **State:** none of the store's fields; the GUI reads them while open. Buying and selling are `BuyItemMessage`/`SellItemMessage` to the store (above). (checked)
- **Edge cases:** no store: nothing, returns 0. No check of the party, combat, an open conversation or another full-screen GUI: the GUI handler decides (not read). (checked)

### GetStoreMoney (448)
- **Handler:** 0x0065A980 (shared with SetStoreMoney 449, no case: identical). (checked)
- **Does:** pushes the store's money, +0x474, in copper. (checked)
- **Edge cases:** no store pushes -1, which reads as "infinite" per script.ldf's own convention. A store with infinite money also gives -1. (checked)

### SetStoreMoney (449)
- **Handler:** 0x0065A980, **the getter's handler**. (checked)
- **Does: nothing to the store.** It pops the object, pushes the store's money as an int and returns. nMoney is never popped. (checked)
- **Edge cases:** the VM's ACTION handler (0x00A30962) checks only the handler's sign, not the stack's depth (checked), so after a call the stack holds two extra int cells (nMoney and the pushed money) where the compiled script expects none. The script's later stack-relative reads are then off by two cells: wrong values, or a failed stack check (-639) further on (inferred). No shipped script calls it (checked: grep of the toolset's scripts). Differs from script.ldf, which documents a setter.

### GetStoreMaxBuyPrice (450)
- **Handler:** 0x0065A9F0 (not shared). (checked)
- **Does:** pushes +0x478. (checked)
- **Edge cases:** no store pushes -1. -1 is also the stored "no limit". (checked)

### SetStoreMaxBuyPrice (451)
- **Handler:** 0x0065AA60 (not shared). (checked)
- **Does:** pops the object, then the int, and writes it to +0x478. (checked)
- **Edge cases:** no clamp: -1 means no limit, other negatives make the store buy nothing (every price ≥ 0 is > it; inferred from the check above), 0 lets it take only free items. A failed int pop returns -639. No store: nothing. Takes effect at the next sale (it is read per sale). (checked)

### GetStoreMarkUp (452)
- **Handler:** 0x0065AB50 (shared with GetStoreMarkDown 313, no case: identical). (checked)
- **Does: pushes MaxBuyPrice (+0x478), not the markup.** The MarkUp attribute (+0x454) is never read. (checked)
- **Edge cases:** no store pushes 100. A store with no MaxBuyPrice gives -1. Differs from script.ldf. No shipped script calls it (checked). (checked)

### SetStoreMarkUp (453)
- **Handler:** 0x0065AAD0 (not shared). (checked)
- **Does:** pops the object, then the int, and sets the MarkUp attribute (+0x454, its vtable +0x54). It is the percentage of the value charged for an item the store sells, and paid for an item the player just bought from a store (prices above). (checked)
- **Edge cases:** no clamp (0 makes items free; negative gives negative prices, which the purse check then always passes; inferred). The attribute's setter may notify observers (the open GUI; inferred). A failed int pop returns -639. No store: nothing. (checked)

### GetStoreMarkDown (313)
- **Handler:** 0x0065AB50, the same function as GetStoreMarkUp. (checked)
- **Does: pushes MaxBuyPrice (+0x478)**, not the markdown (+0x464). (checked)
- **Edge cases:** no store pushes 100. Differs from script.ldf. No shipped script calls it. (checked)

### SetStoreMarkDown (314)
- **Handler:** 0x0065ABC0 (not shared). (checked)
- **Does:** pops the object, then the int, and sets the MarkDown attribute (+0x464, its vtable +0x54): the percentage of the value the store pays, and charges to buy back what the player sold it. (checked)
- **Edge cases:** as SetStoreMarkUp. The one shipped call is `den200pt_generic.nss` (MarkDown 50). (checked)

### OpenItemUpgradeGUI (636)
- **Handler:** 0x0065AC40 (not shared). (checked)
- **Does:** pops nothing; builds `SetFullScreenGUIMessage("ItemUpgrade")` (0x00605DB0, vftable 0x00B022B8) and sends it to the GUI system (app+0x158): the full-screen GUI `ItemUpgrade` (the rune screen), as the shared fact in `dev/script_commands/inventory.md`. (checked)
- **Edge cases:** no arguments, always returns 0. Which item or creature the screen shows is the GUI's own choice (not read). Called by `gen00pt_generic_actions.nss` and the debug `zz_upgrade.nss`. (checked)

An item's value and the prices the store screen shows: see ITEMS.md, "An item's value".

## Not read
- 0x005C6650's meaning (the "Store" token binding), `GUI::GUIStore`'s handlers, `CloseStoreMessage`, and what closing does to the token.
- Whether `SetFullScreenGUIMessage` refuses while another full-screen GUI or a conversation is up.
- The `WillNotBuy`/`WillOnlyBuy` container's class (0x0086BEE0, 0x008663A0); the buying loop's branch for n > 1 non-stacking items (0x007ED4A0) in detail.
- The SetStoreMoney stack imbalance's exact effect in a running script.
