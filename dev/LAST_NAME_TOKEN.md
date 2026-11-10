# Last-name tokens after loading a save

Investigation of [DAOLib #328](https://github.com/Whocares66-Dev/DAOLib/issues/328), 2026-11-10. Read in the local GOG 1.05 executable with Ghidra's read-only decompiler and checked against its x86 disassembly. The issue's author reproduced the symptom in a popup and through `dao::tlk::Resolve`. A before-and-after GOG test with a temporary probe reproduced the empty surname and verified the fix on an existing save and a newly written save.

## Finding

The engine computes the party's cached surname before restoring the hero's saved stats, then never refreshes it. Character creation has an extra refresh after the name-selection script event, which explains why a newly created character has a surname. This is an engine load-order defect; `dao::tlk::Resolve` directly calls the same engine resolver as the UI.

The order of calls and the missing refresh are checked in the executable and in play. Loading Lyna's existing save called SetHero with background-defaults index **3000**, leaving the surname empty. After the saved stats restored, the added refresh used **2301** and resolved **Mahariel**. This identifies the premature row for this character; it does not imply that every affected character selects the same incorrect row.

## Where the text comes from

- `ReplaceDesignerTags` calls `ExpandDesignerTag` (0x0051D620). Its `<LastName/>` case, hash 0xCAAD80AD, copies `CCParty::GetLastNameString` (0x0062ECB0), a getter returning the wide string at party +0x470. `<FirstName/>` uses +0x454 through 0x0062ECA0. `<FullName/>` calls 0x0062ECC0, which concatenates the first name, a space and the last name. These cases do not consult `dialog_tokens` or `background_defaults` during expansion.
- `CCParty::SetHero` (0x00630240) copies the creature's name into +0x454 at 0x00630295, sets the party's hero-id attribute at +0x1F8, then obtains the creature's stats through +0x7C4. `GetBackgroundDefaultsIndex` (0x0071BE90) computes `race * 1000 + class * 100 + background`. SetHero looks up that row of `background_defaults`, reads `LastName` (column hash 0x1873D3E9), resolves the returned talk-table reference through `LocString`, and assigns party +0x470 at 0x00630347. If the table lookup fails, it leaves the cached surname as it was.
- The party constructor (0x0063A3C0) initializes all three name strings empty, including the surname at 0x0063A888. Neither `CCParty::WriteToSave` (0x006343A0) nor `CCParty::Load` (0x00637D50) serializes/restores the cached first name or surname. They are reconstructed from the hero instead.

The installed core `2da.erf` was read through `dao_files`/`tools/archive.py`. Its `background_defaults.gda` has row 0 with LastName 0 and twelve playable rows: 2101/2301 -> 363648, 2103/2303 -> 363649, 1102/1302 -> 363650, 1105/1305 -> 363651, 3105/3305 -> 363652, 2204 -> 363653, 3204 -> 363654. A fix should use the engine's merged table and localized lookup, as SetHero does, rather than embed those values.

## The incorrect order on load

`CCCreature::LoadFromStruct` (0x006BE980, vtable slot 34) does the following:

1. Calls the base object's restore, `CCObject::LoadFromStruct` (0x00698ED0), which restores the name before party membership. This explains why the first name can be correct.
2. Reads `SavegamePartyMembers` (16203 / 0x3F4B) and calls `SetFollowerState` (0x0063B9A0) at **0x006BEAA1**.
3. That reaches `CCParty::AddToPool` (0x0063A020). For a creature with the hero byte +0x7D4 equal to 1, AddToPool calls **SetHero at 0x0063A159**, including its surname lookup. The saved stats have not been applied yet.
4. Only afterwards reads `SavegameCreatureStats` (16209 / 0x3F51) and calls **`CCCreatureStats::LoadFromStruct` (0x0071EDC0) at 0x006BEB97**. That loader starts by calling `CreatureProperties::Load` (0x00795E30); class and background are properties 27 and 37, read by `GetClass` (0x0071B010) and `GetBackground` (0x0071AFB0). Race is obtained by `GetRacialType` (0x00719EA0).
5. There is no later SetHero call in the creature restore or party restore to rebuild the surname from the restored stats.

Ghidra finds only two direct callers of SetHero: AddToPool above, and `GUICharGen::HandleMessage_CharGenSelectNameMessage` (0x00736F00), at **0x00737025**. The latter first delivers the character-name script event synchronously, then calls SetHero. A scan of party code 0x00628000-0x00640000 for +0x470 operands also finds constructors, destruction and the getter, but no other normal surname assignment besides SetHero.

Do not move the whole SetFollowerState operation below the stats restore: it also establishes party data needed by that restore (`PROPERTIES.md`, the shared heroic-party property array). Refreshing the derived name afterwards avoids changing that prerequisite.

## Fix

`src/fix/LastName.cpp`, installed by the loader before plugins load, detours `CCCreature::LoadFromStruct`, calls the original, then, after a successful restore and for the party's hero alone, calls the engine's SetHero on that party and creature. The hero is identified through `CClientExoApp::GetParty` and `CCParty::GetHero`, compared with the restored creature's id; no build-specific hero-byte offset is needed.

Both functions are `__thiscall`, take one stack argument and return with `ret 4`. **The creature restore returns success in EAX**: 0x006BF6DE sets 1, the failure path at 0x006BF6EB sets 0. Ghidra's existing void signature is incorrect. The hook preserves that return value and does not refresh after a failed restore.

This reuses the engine's localization and table lookup, refreshes both name caches together, and repairs existing saves because the surname is derived from the saved character choices. It also covers UI text and `dao::tlk::Resolve` through their common source. A change confined to `Tlk.cpp` would leave the UI path unfixed. SetHero also assigns the hero-id attribute. The tested loads, including companion restores, completed normally with the refresh.

Address-library rows used by the fix:

| ID | Function | GOG / Steam | EA |
|---|---|---|---|
| 12277 | CCParty::SetHero | 0x00630240 | 0x0060BF50 |
| 14952 | CCCreature::LoadFromStruct | 0x006BE980 | 0x005B0440 |
| 16519 | ScriptBackend::ForScript_GetBackgroundDefaultsIndex | 0x0071BE90 | 0x00619EA0 |

The cross-build addresses above are from `extern/DAOAddressLibrary/data/addresses.csv`; EA's implementation was not independently decompiled here. The required functions are already named in the address-library commit pinned by master (`0cebd144ae973adfa39a4e6c59cb60fa8712cee5`). The fix keeps that pin. No address-library source or tables were edited.

## Validation

The GOG test used a staged copy under `build/issue328/game`, the named instance `gh-328-last-name`, copied saves and only Eluvian plus a temporary logging probe. The player's running game, installed loader and original saves were not changed. The temporary probe source was built in place of the saves example and then the original source and example binary were restored.

- **Baseline:** Lyna AutoSave_1, background-defaults index 3000; `First=[Lyna] Last=[] Full=[Lyna ]` both at SetHero and once the world loaded.
- **Fixed, same save after restarting:** original lookup 3000, refresh 2301; `First=[Lyna] Last=[Mahariel] Full=[Lyna Mahariel]` once loaded. The load completed in explore mode.
- **Another load in the same process:** copied Lyna Slot_111, same correct result. AutoSave_1 was in combat, where saving is refused, so the save/reload check used this non-combat slot.
- **Save/reload:** wrote new manual slot 499 in the isolated profile, waited for the saved event, loaded that slot and waited for the loaded event; the surname and full name remained correct.
- **New character:** a female elf mage completed Quick Play with row 2204 and `First=[Neria] Last=[Surana] Full=[Neria Surana]`; character creation still sets its own surname.
- **Switching heroes:** loading Lyna Slot_499 after creating Neria changed `Neria Surana` back to `Lyna Mahariel`; no stale surname remained.
- **Release trial:** the optimized x86 loader extracted from `DAOLib-0.10.0-wip-gh-328-last-name.zip` loaded the regression save and restored `Lyna Mahariel`; the release test suite passed with no failures. The [issue reporter verified the same WIP on the EA App build](https://github.com/Whocares66-Dev/DAOLib/issues/328#issuecomment-6773706005): a female human mage in a custom module retained her full name in a custom Codex title after reloading, and a male elf mage Warden retained his surname in single-player.
- **Build and checks:** x86 debug build, `ctest -R '^address_ids$'` (665 address rows), clang-format on the new files, clang-tidy on the new hook, and `git diff --check` passed. No core logic changed, and no headless string test was added: it would not exercise this load-order defect.

The before/after logs and temporary probe are in ignored `build/issue328/`. Local runtime checks were on GOG; the external EA App coverage is recorded above. Steam, every origin, localized tables and imported campaigns were not separately exercised. Both the UI and API use the repaired party cache, and the implementation uses the engine's table and localization routines.
