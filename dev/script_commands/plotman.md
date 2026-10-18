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
- **The icon (slot 99):** it is set only when the object's +0x1AC is set, the **PlotAssist** option is on (`[0x00C32938]+0xA8`, named at 0x005E8C3C) and there is a party. The icon is `PlotDestination` if the party counts the object as a destination (0x00631830); otherwise `PlotGiver` if +0x25A is set; otherwise none. It goes through the floaty manager (0x00523880 → 0x0063FCF0). Changing the option refreshes every object's icon (0x005E1714). (checked)
- **State:** object+0x25A. This is saved as `SavegameIsPlotGiver` (16455) and put back through slot 98 on load (0x006C0A85). (checked)
- **Edge cases:**
  - `CCObject`'s slot 98 is `ret 4`, so items, triggers, waypoints, sound objects, AoEs and VFX objects do nothing. Areas, modules and the party fail the cast and do nothing. An invalid id does nothing. (checked)
  - Setting the same value again does not refresh the journal or the icon. (checked)

## Not read
- Who reads the story plot (journal+0x1CC) for the loading screen, and whether it is saved (`SavegameJournalStoryText` 16511 is a candidate).
- Whether a changed party-picker area name (module+0x350) is ever loaded; whether either name is saved.
- What object+0x1AC means for the icon (an "interactive"/has-floaty flag, guessed); what 0x006F1FE0 refreshes; the party's destination test 0x00631830.
- The reward id's meaning downstream (the rewards table).
