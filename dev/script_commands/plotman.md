# Script commands, group "plotman": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" is read in the disassembly; "(inferred)" is not.

Shared facts, all checked:
- Handlers are `__thiscall (commandId, paramCount)`, `ret 8`, on `CVirtualMachineCommands`; `this+8` is the running script's OBJECT_SELF id (as in `effect_access.md`). A failed pop returns -639 (the script fails), a failed push -638. Nothing else fails the script.
- **The party is always the client app's.** `0x005D57C0` (app's party, `CCParty`, may be null) is asked first; its +0x210 is the plot manager. Plots are looked up in it by a string that may be a GUID or a resref (`0x0075E850`), as `GetPlotResRef` takes either.
- How a flag is set, the plot script's `EVENT_TYPE_SET_PLOT`, stored (0–127) against defined (128+) flags and what `nCallScript` governs: `dev/PLOTS.md` ("Stored and defined flags"). Not repeated here.

### SetPartyPlotFlag (661)
- **Handler:** 0x0065FB90, shared with 663 (`COMMAND_SETPARTYPLOTVAR`, not in script.ldf); it branches on the command id (0x295). Calls the plot manager's `SetPlotFlag` (0x0075F7E0). (checked)
- **Does:** pops oParty, strPlot, nFlag, nValue, and nCallScript only when 5 arguments were passed (default 0). Calls `SetPlotFlag(strPlot, nFlag, nValue != 0, nCallScript != 0, OBJECT_SELF)`. OBJECT_SELF becomes the plot event's object 0 (`dev/PLOTS.md`). (checked)
- **State:** the party's plot manager (`party+0x210`) and the plot's four 32-bit flag words. (checked; the words per `dev/PLOTS.md`)
- **Edge cases:**
  - **oParty is ignored** while the app has a party. Only with no app party is it used, as a creature (`GetCreatureById` then `CCCreature::GetParty` 0x006A2CC0); a party object id then fails, and nothing happens. (checked)
  - nValue is made a bool: 2, -1 and so on store TRUE. (checked)
  - An unknown plot, no party or no plot manager: nothing, returns 0. (checked)
  - nFlag outside the documented 32–127: as `dev/PLOTS.md` says for stored (0–127) and defined (128+) flags. (inferred from that note)
  - 663 pops the object, the string and one int, then does nothing (the jump at 0x0065FCCD). (checked)
- Vanilla calls it only through `WR_SetPlotFlag` (`wrappers_h.nss`:102), with `GetParty(GetHero())`.

### GetPlotFlagRewardId (181)
- **Handler:** 0x00660100 (not shared). Calls the plot manager's 0x0075F710 → 0x0075C660. (checked)
- **Does:** pops strPlot, then nFlag. Finds the plot (0x0075E850), then the flag in its definition (0x0075C250), and pushes that flag's `PlotFlagReward` (GFF4 field 13003, read at 0x0075C67E). (checked)
- **State:** read only: the plot's definition, not the party's flag values. `dao/core/Plot.h` has the same field as `reward`. (checked)
- **Edge cases:** no app party, no plot manager, an unknown plot or a flag the plot doesn't define all push 0. A flag that has no reward pushes its stored 0 (inferred). Whether nFlag is stored or defined makes no difference: it is looked up in the flag list (inferred from 0x0075C250's use). No vanilla script calls it. (checked except as marked)

### SetPartyPickerStage (858)
- **Handler:** 0x0065F8F0 (not shared; the engine's name is `COMMAND_SETSHAREDCHARAREANAME`). Calls the module's setters 0x00652370 and 0x006523D0. (checked)
- **Does:** pops sAreaName, then s2DAName. Copies them into the module (`CClientExoApp::GetModule` 0x005D4FC0): +0x350 the stage area's name, +0x36C the table's name. Both are `std::wstring`. (checked)
- **State:** the module constructor (0x00605430) sets the defaults `char_stage` (0x00C35E00) and `partypicker` (0x00C35E1C). It then builds the party picker's stage object (0x6FC bytes, 0x007072E0) from +0x350, appending `.are`. (checked)
  - The table's name is read through 0x0062EE20 at five places: `SetFollowerState` (0x0065E7FF), 0x00638580, 0x00712830, 0x00767830 and 0x0076C170. Each uses `partypicker` when there is no module. (checked)
  - **No later reader of +0x350 was found**, so a new area name may not take effect until the stage is rebuilt (inferred; not read further).
- **Edge cases:** with no module, nothing happens. Neither name is checked, so an empty or unknown table means the picker's table lookups fail (inferred). Whether the names are saved was not read. No vanilla script calls it. (checked except as marked)

### SetStoryPlot (750)
- **Handler:** 0x00663DA0 (not shared). Calls `ScriptBackend::ForScript_GetPlotResRef` (0x0075F070) and `Journal` 0x0064DBA0. (checked)
- **Does:** pops sPlot. It turns sPlot into the plot's resref through the party's plot manager, which takes a GUID or a resref. It then stores the resref as the journal's story plot. The journal is the module's +0x490 (getter 0x005D5810; its class is `Journal`, vtable 0x00B14B38), and the plot goes at journal+0x1CC (a `std::wstring`). (checked)
- **State:** journal+0x1CC. The journal's copy (0x006F8320) copies it. Its reader (script.ldf: the loading screen) was not found. (checked except the reader)
- **Edge cases:** with no app party, plot manager or journal, nothing happens. **An unknown plot stores ""**, which clears the story plot: `GetPlotResRef` answers an empty string for it (0x0075F0CE). (checked)
- Vanilla: `sp_module_start.nss`:73–102 picks one `MNP00PT_SSF_*` plot by the main plot's progress, through `WR_SetStoryPlot`.

### SetPlotGiver (751)
- **Handler:** 0x00663E90 (not shared). Calls `GetObjectById` (0x005D5BE0, a `CCObject` cast), then the object's vtable slot 98 (+0x188) with `bIsPlotGiver != 0`. (checked)
- **Does:** for creatures and placeables, slot 98 is 0x0074E6A0. If the bool differs from the object's +0x25A, it (checked):
  - stores the bool at +0x25A;
  - adds the object's id to the journal's plot-giver list at +0x108 (0x006F4640, only if absent), or removes it (0x006F4680);
  - refreshes the journal (0x006F1FE0; probably the map's pins, inferred);
  - calls slot 99 (0x006979A0) to update the object's overhead icon.
- **The icon (slot 99):** it is set only when the object's +0x1AC is set, the **PlotAssist** option is on (`[0x00C32938]+0xA8`, named at 0x005E8C3C) and there is a party. The icon is `PlotDestination` if the object's destination flag (+0x259, set by slot 97) is set, and none for a member of the active party, which 0x00631830 tests (`CCParty::IsInParty`; corrected 2026-10-20 by lib-agent-2: it is the in-party test, not a destination test); otherwise `PlotGiver` if +0x25A is set; otherwise none. It goes through the floaty manager (0x00523880 → 0x0063FCF0). Changing the option refreshes every object's icon (0x005E1714). (checked)
- **State:** object+0x25A. This is saved as `SavegameIsPlotGiver` (16455) and put back through slot 98 on load (0x006C0A85). (checked)
- **Edge cases:**
  - `CCObject`'s slot 98 is `ret 4`, so items, triggers, waypoints, sound objects, AoEs and VFX objects do nothing. Areas, modules and the party fail the cast and do nothing. An invalid id does nothing. (checked)
  - Setting the same value again does not refresh the journal or the icon. (checked)

## Not read
- Who reads the story plot (journal+0x1CC) for the loading screen, and whether it is saved (`SavegameJournalStoryText` 16511 is a candidate).
- Whether a changed party-picker area name (module+0x350) is ever loaded; whether either name is saved.
- What object+0x1AC means for the icon (an "interactive"/has-floaty flag, guessed); what 0x006F1FE0 refreshes; the party's destination test 0x00631830.
- The reward id's meaning downstream (the rewards table).

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe. Not seen in play. Shared facts (the party is always the client app's, `0x005D57C0`; its +0x210 is the plot manager; a plot is looked up by GUID or resref, `0x0075E850`; -639/-638): top of `plotman.md`. Flags and scripts: `dev/PLOTS.md`.

### GetPartyPlotFlag (660)
- **Handler:** 0x00660200, shared with `GetPartyPlotVar` (662, `COMMAND_GETPARTYPLOTVAR`, not in script.ldf); the branch on command 0x294 (660). Calls the plot manager's `GetPlotFlag` 0x0075EA60.
- **Described in:** `dev/PLOTS.md` ("Stored and defined flags": `GetPlotFlag`, `nCallScript`) and `dev/LIBRARY.md` ("Plots": `GetPartyPlotFlag(GetParty(hero), guid, n, 0)`).
- **Does:** pops oParty, strPlot, nFlag, then nCallScript only when paramCount > 3 (default 0); calls `GetPlotFlag(strPlot, nFlag, nCallScript != 0, OBJECT_SELF)` (OBJECT_SELF is the VM's +8) and pushes the byte answer. (checked)
- **State:** read only (the plot's flag words, and the plot script runs for a defined flag; `PLOTS.md`).
- **Edge cases:** as `SetPartyPlotFlag`: **oParty is ignored** while the app has a party; with none, it is taken as a creature and its `GetParty` (0x006A2CC0) is used. No party, no plot manager or an unknown plot pushes **0** (FALSE). Any non-zero nCallScript counts as TRUE. For 662 the same code skips the `GetPlotFlag` call, pushes 0 and pops one int fewer; it is not a script.ldf function.

### GetPlotEntryName (665)
- **Handler:** 0x00660390 (not shared; the engine's name is `COMMAND_GETPLOTNAME`). The plot manager's 0x0075ED50, a copy 0x00401E90, `StackPushInteger`.
- **Described in:** `dev/LIBRARY.md` ("`GetPlotEntryName(guid)` the journal title's string id").
- **Does:** pops strPlot (GUID or resref) and pushes the plot's name as a string id (a talk-table reference) from the plot's definition. (checked for the call path; that the value is the `Name` string reference of the plot is from `LIBRARY.md`)
- **State:** read only.
- **Edge cases:** no app party, no plot manager, an unknown plot or a plot whose entry is -1 pushes **-1**, not 0 (the default `LocalizedString` object starts with the string id -1: 0x004D1B8B, checked). A failed pop is -639.

### GetPlotResRef (742)
- **Handler:** 0x0065FD30 (not shared). The plot manager's `ScriptBackend::ForScript_GetPlotResRef` 0x0075F070, a string copy 0x004C2AB0, `StackPushString`.
- **Described in:** `plotman.md` (`SetStoryPlot`: "an unknown plot stores "", which clears the story plot: `GetPlotResRef` answers an empty string for it, 0x0075F0CE"); `dev/LIBRARY.md`.
- **Does:** pops strPlot (a GUID, or a resref which comes back as it is named in the file) and pushes the plot's resource name. (checked)
- **State:** read only.
- **Edge cases:** no party, no plot manager or no such plot pushes "" (the copied string starts empty). The GUID is matched by the manager's lookup 0x0075E850, which also takes a resref, so the call is a normaliser too (inferred from the shared lookup). A failed pop is -639.

### GetPlotGUID (853)
- **Handler:** 0x0065FE60 (not shared). `ScriptBackend::ForScript_GetPlotGUID` 0x0075CB30, `StackPushString`.
- **Described in:** `dev/LIBRARY.md` ("`GetPlotGUID(resref)` gives a plot's GUID ("" for no such plot)").
- **Does:** pops strPlotResRef; the manager's 0x0075CB30 answers a bool and fills the GUID string; on false the string is set to the empty literal (0x00ADFF52, which is a wide NUL, checked) and that is pushed. (checked)
- **State:** read only.
- **Edge cases:** no party, no plot manager or no such plot pushes "". The argument is a resref (script.ldf); whether a GUID is also accepted is the lookup's business (`plotman.md`'s lookup takes either; not read here). A failed pop is -639.

### GetPlotFlagName (743)
- **Handler:** 0x0065FFB0 (not shared). The plot manager's 0x0075F420, a string copy, `StackPushString`.
- **Described in:** `dev/LIBRARY.md` ("a flag's name, "" where there is none, the main flags from 0 and the defined ones from 256").
- **Does:** pops strPlot, then nFlag; pushes the name the plot's definition gives that flag. (checked for the call path; the name's source field is from `LIBRARY.md`)
- **State:** read only.
- **Edge cases:** no party, no plot manager, an unknown plot or an undefined flag pushes "". The flag number is the one used in `GetPlotFlag`: stored 0 to 127, defined from 128 (`PLOTS.md`), the toolset numbering them from 256. A failed pop is -639.

### GetPlotSummary (381)
- **Handler:** 0x006640B0 (not shared). The plot manager's 0x0075F520, a string search 0x004C0F70 (find), a substring 0x004C3250, `StackPushString`.
- **Described in:** `dev/LIBRARY.md` ("`GetPlotSummary(guid, flag)` was "" for every set flag tried").
- **Does:** pops sPlot, then nFlag; asks the plot manager for the flag's text (0x0075F520, apparently the journal entry text of that flag; inferred) and cuts out what lies between the markers **`<summary>` and `</summary>`** (static wide strings built on first use from 0x00B08044 and 0x00B0802C, read; the cut is from the end of the first to the start of the second, with the first's length from 0x00C34B78). The cut text is pushed. (checked)
- **State:** read only (the static marker strings are set up once).
- **Edge cases:** if either marker is missing in the text, or the text is empty, or there is no party or plot manager, it pushes **""** (0x006642C3). That is why `LIBRARY.md` saw "" for every flag: it answers only for a flag whose text carries a `<summary>` element. The text is returned with no trim. A failed pop is -639.

## Not read
- 0x0075F520 (where the flag's text comes from), 0x0075F420 and 0x0075CB30's insides, and how 0x0075E850 treats a plot that is neither GUID nor resref.
