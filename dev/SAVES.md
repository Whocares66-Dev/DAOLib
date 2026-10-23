# Save games

A save is a folder under `Documents\BioWare\Dragon Age\Characters\<name>\Saves\<slot>\`: the save itself (`<area>.das`), its metadata (`<area>.das.met`), a screenshot (`screen.dds`) and the character's picture. `tools/gff4.py --dump FILE` reads the first two whole, with field names from xoreos's table (`dev/gff4_fields.csv`). Read 2026-09-30 from a save made at Castle Cousland, on this machine.

## The save (`.das`)

GFF4, type `SAVE`, version 1.1, 78 struct templates. Its top struct:

- **`SavegameCampaign`** (`MODU`): the module and its state.
    - The groups and which are hostile to which.
    - The journal: active and completed quests, the plot destinations.
    - The world map, the plot actions, the ambient dialogs.
    - The module's script variables (`ScriptVarTable`: `APP_RANGE_VALUE_*` and the like, by type: int, float, string, object, resource).
    - Where the campaign starts (area list, area, position, orientation).
- **`SavegameAreaList`** (`ARE1`): the areas in memory, each with its tag, object id and objects. For the placeables: template, position, orientation, object id, state.
- **`SavegamePlayerchar`** (`PLYR`): the player's creature, the map's zoom and legend, the time played in seconds.
- **`SavegamePartyList`** (`PRTY`):
    - The party pool, the backpack, plot items, money.
    - Crafting recipes, the selected character and the leader, approval.
    - The party's settings: hold positions, run in drive mode, the quickbar lock.
    - The plot manager, the lines seen.
- **`SavegameVersion`** (`VERS`): the build that saved it (13263, this exe's) and whether cheats were used.
- **`SavegameGameState`** (`GMST`): the next effect id, the AI master (the engine's event queue, `dao/Addresses.h`), the world timer.
- **`SavegameAddinsList`** (`ADIN`): the 58 add-ins the game knows, each with its name in every language.
- **`SavegameStorySoFar`**: the story events and level-ups.
- **`SavegameWorldDatabase`**: id ranges per group.

### The journal's active quest (read 2026-10-20 by lib-agent-2, for DAOParagonEngine #19; checked unless said)

- **Held** at the journal's +0xD8, `ActivePlot` (an attribute of a `JournalEntry *`): an entry of `PlotList` (+0x58), never of `CompletedPlots`; null for none. `dao::journal`'s `Active()` reads it (`src/dao/Journal.cpp`).
- **Set** by:
  - the journal screen's "Make Active", which sends `Journal.SetActiveQuest` with the plot's ResRef. Its handler (0x006F4FB0) calls `SetActivePlot` (0x006F4080): it clears the pointer, finds the entry by its ResRef (entry +0x58), rebuilds the plot destinations (0x006F20E0) and notifies. The movie hides the button while PlotAssist is off; the engine does not check;
  - `AddPlot` (0x006F5040), which makes a new entry active only when none is (0x006F522D–0x006F5243);
  - and it is cleared by `CompletePlot` (0x006F5380) when the entry completed is the active one (0x006F5643–0x006F5675). No other is picked then [inferred: none seen]. No script command sets it.
- **Saved** as `SavegameCampaign` > `SavegameJournal` (`JRNL`) > `SavegameJournalResRef` (16510), a string: the active plot's ResRef, or empty for none. The writer is 0x006ED770 (its tail 0x006EE255–0x006EE2CF). The loader (0x006F7290) reads the quest lists first, then calls `SetActivePlot` with the saved ResRef (0x006F81F5). Real saves agree: `gwb000pt_sideplot`, `lel000pt_main`, `shl000pt_main`, and empty in a save with no quests.
- **After a load** the saved quest is active if its ResRef is among the loaded `PlotList`'s, else none: the engine picks nothing else.

## How the game loads a save (read 2026-10-20 by lib-agent-2, DAOLib #202)

Read in the GOG exe's disassembly, every loader's field list checked against real saves (Lyna AutoSave_1 with 123 areas, Lyna2 Slot_33); checked unless marked [I]. What follows is each struct's loader and the fields that matter; the full field tables (label, file type, reader type, default) were kept with the reading, not here.

### The rules every loader follows

- **Raw reads.** A field is read at its label's offset with the reader's width, 1 or 4 bytes, with no type check or conversion: a writer must use the file's types.
- **Template first.** An object is built from its template, then the save is laid over it: the area step loads the template (`TemplateResRef`, 3) and then calls the object's restore with the saved struct. So **a field the save lacks keeps the template's value**, for every kind: creatures (0x006ABDC0 loads the whole `.utc` through 0x006A27B0, then vtable +0x88, 0x006BE980), placeables, triggers, waypoints, stores, sounds and AOEs (vtable slot 34). The template is the default.
- **The common object read** (0x00698ED0, CCObject slot 34) runs first for every kind: Position, Orientation, effects, Active, immortal and plot flags, variables, event script, name, Tag, team, radius; and the commands and the action queue **only on a saved game's load** (the loading data's byte +0x68), not when an area comes back from a kept area list.

### The order of a save's load

The loading machine (0x006C76F0, 23 steps), then the area machine (state 6, 0x006C8EF0):

- step 3: the module unloads, which clears the old game's event queue;
- step 5: the save is parsed, then the world database (16500, three id pools, each taken only when in range), the world timer (`WorldTimer::Load` 0x00A36010: day 16701, ms 16702; below) and the effect counter (`SavegameEffectStartingID` 16626, 1 when absent);
  - **The world timer** [C]:
    - It calls SetTime(day, ms, 0) (0x00A35A80), then Update (0x00A35A10) twice.
    - The running total (an int64 of microseconds at +0x28) is not touched. SetTime sets offsets, +0x40 the day and +0x44 the ms, so that WorldTime (0x00A35AF0) answers the loaded values. WorldTime adds the total's days and remainder to the offsets and carries ms into days by +0x4C.
    - The save (vtable slot 9, 0x00A35FD0) writes WorldTime's day and ms, nothing else.
    - **A day is 7,200,000 ms (+0x4C) or 7,200 s (+0x50), 2 hours of world time at scale 1.** It is set only by the constructor 0x00A35F30, through 0x00A35E50(5): 5 × 1,440,000 ms. Nothing else, no script and no module field, changes it.
    - So a saved ms is always below 7.2e6.
    - The day number is a calendar: day = year·336 + (month − 1)·28 + (dayOfMonth − 1) [C for the 336-day year in the code; I for the reading of the saves' dates, e.g. 450,166 = year 1339, month 10, day 11]. The constructor's default is 450,381 (year 1340). What sets a new game's earlier start day was not read.
- step 7: the module made with MODU's ObjectID; step 8: the hero's gender for the talk table, then the campaign (below);
- step 10: the party object made with PRTY's ObjectID; step 11: the hero (PLYR);
- steps 13–15: the area list's files; step 16: the party (`CCParty::Load` 0x00637D50); step 17: the areas and their objects, a slice a frame (0x006FABB0); step 18: the event queue (`AIMaster::Load` 0x0064AAD0);
- step 21: the party placed, each at its own saved place, and the saved selection applied; step 23: the controlled creature's area made current, the machine to state 6, unpaused.
- The party loads before the areas, so an area creature whose id already exists is not made twice. No game mode is saved: the area machine asks for explore.

### The campaign (MODU): 0x005FF8B0, from the module load at 0x00605690

- In order: `CampaignCIFEntryAreaList` 17100 and `CampaignCIFEntryArea` 17101 (where the party is), `CampaignCIFEntryPosition` 17102 (default (0, 0, 0)), `CampaignCIFEntryOrientation` 17103 (a direction, default (1, 0, 0)), `SavegameCreatureTimeBeforeDecay` 16499 (the global decay time, 60000 ms when absent), the module's `ScriptVarTable` 17000, the groups 16450 (a present list clears every group first; 0x0064C310), the journal 16504 (0x006F7290, below), the world map 16780 (0x00701250), the plot actions 16840 (0x006FDCF0), `SavegameCampaignResource` 16014, the ambient dialogs 16530 (0x006FED90).
- It answers 1 only when 16014 is not empty: the "loaded from a save" gate that holds back MODULE_START (`dev/LOAD_EVENTS.md`). The module's own ObjectID (23) is read only at step 7, not here.
- **Variables** (0x00705E90): a generic list typed by each element's struct label: `SVI1` int, `SVF1` float, `SVS1` string, `SVO1` an object id (u32), `SVR1` a resource string; the name 17001 then the value 17003. `ScriptVarTableType` 17002 is read only by a fallback for a non-generic list, which no real save has.
  - **Variables load onto declared names only** (read 2026-10-21 by lib-agent-2; checked, and seen in play by eluvian-agent for #221: a variable added to a save read 0). The save's loader runs with create = 0 (module 0x005FFA91, area 0x0068833B, object 0x00699144): each setter looks up (name, type) with 0x00703BF0, and the adder 0x00703A00 runs only with create set. So a saved variable whose name and type the .cif's ScriptVarTable (0x00704F60), the module IFO's VarTable (0x00704240) or the template did not declare, with create = 1, is dropped silently. The value 17003 is read at raw width with no type check, so a wrong type misreads rather than skips. Only changed variables are written, so one at its declared value is never in a vanilla save. Whether names match case-insensitively was not read.
- **The journal**: groups first, then the active, completed and orphan quests, the plot destinations 16513–16515 (each by GUID, 0x006F6130), conversations, story text, and last the active quest's ResRef (16510, `SetActivePlot`; above). Saved maps, pins and plot actions that match nothing in the game's data are skipped.

### An area (ARE1): made by 0x006FABB0, loaded by 0x0068B1B0, its objects by 0x006882C0

- An ARE1 whose ObjectID is −1 is ignored whole. 0x0068B1B0 reads rooms viewed (16018) and the area map (16016, applied only when its size matches the grid).
- 0x006882C0 loads the variables, then waypoints, placeables (0x00683380, each 0x0074EFE0 / 0x0074FE30), triggers (0x00687AC0, 0x0079A7E0), creatures (0x00683820), sounds (0x007C2BE0), stores (0x0067AC00, 0x007EC0B0) and AOEs (0x0067E0D0, 0x007E5060), and the area's effects last.
- **A trigger's kind** is `SavegameAreaTriggerType` (16112): 0 TRGR, 1 TRAT, 2 TRTP, 3 TRAR, 4 TRAV, 5 TRAM, not the struct's label (0, 3, 4 and 5 seen in saves; 1 and 2 [I] from the factory's order).
- **Ids:** placeables, waypoints, AOEs, sounds, creatures and container items read their ObjectID (23, default −1); **triggers and stores do not, and get fresh ids at each load**.
- A placeable's RoomID1 and RoomID2 are not saved: the loader takes them from the `.are`'s placeable list, matched by X and Y. A sound's Tag and Position are set by the common read and then overwritten by its own fields. A waypoint reads nothing after its MapNote when MapNote is 0.

### A creature (CRE1 in an area, CRP1 the hero, CRL1 a pool member)

- **The area step** (0x00683820): its ObjectID (default −1); skipped when that id already exists; Importance 16263 filtered against the area's 16020 (0x0067A170; the filter's meaning not read). Then the template and the restore, as above.
- **The restore** (0x006BE980), in order: the common read; importance; the party state (0x0063B9A0: follower state, pool); **stats** (below) then UpdateCanLevelUp; substates, tactics, flags, class ranks; **equipment first** (+0xBE0, vtable +0x6C) **then the backpack** (+0xBF0, +0x70); the body bag, rank, appearance, sound sets, portrait floats, pool flag, no-permanent-death, TimeSinceDeath. creature +0xC58 is 1 during the load, so the template's own items are skipped and do not double up.
- **Stats** (16209; 0x0071EDC0) are saved and read **only when the template's +0xBD4 is 0** (0x006BEB10): a creature with no stats struct is one whose template marks it so (Duncan, the hunters, Junar), not one "never active", and keeps its template's stats. A property is restored by its row in properties.gda (`SavegameStatPropertyIndex` 16353), not by its ATTR/DPLT/SMPL label: Simple's Base 16300, Attribute's Modifier 16301, Depletable's Current 16302 and Regen 16304. Then the talent, spell and skill lists (16306, 16305, 16307, each through AddAbility 0x0071EAC0), the modal abilities 16468 (a present list replaces the set), and a party member's quickslots (0x0062C3D0).
- **Appearance** (16320, APP1/APL1/APC1; 0x0074A890): whatever labels the struct has, the type 16321 first. The hero's morph (16950) is not read by the restore: `BuildAppearanceModel` (0x006C0DE0) reads it from the save's PlayerChar struct.

#### The hero's made head in a save, SavegamePlayerMorph (16950) (read 2026-10-22 by paragon-agent's helper, DAOParagonEngine #172; the play seen by lib-agent)

Read 2026-10-22 in the GOG exe (ghidra-dao, read-only) and in a save with tools/gff4.py. [C] read in the code, [I] inferred, [F] read in the file, "seen in play" seen in the running game.

##### The writer: 0x00803000

- [C] 0x00803000 (`__fastcall`, this = the paperdoll) gets the attribute at this+0x5C, calls its object's vfunction68 and then vtable +0xE4 on the result (the head morph's name; CHARGEN.md has +0x5C as the paperdoll's `PaperCreature` attribute and +0xE4 as the model's head-morph-name getter). The result goes through 0x004F4710.
- [C] It builds the wide string `L"Head_Morph"` (0x004C2480) and calls `GraphicsStructures::Morph` 0x0056FA20 with the type tag 0x20204350 (`"PC  "`) and an out buffer and size.
- [I] The name passed is the morph file's own name. This rests on the decompile's data flow and on the game's own saves holding `Head_Morph` as the file's name (below).
- [C] It then calls 0x00643420 to get a struct, takes its field 0x4236 (16950) through 0x00A5A8E0, resizes it with 0x004DD730(size), and copies the bytes one by one with 0x004782F0.
- [I] The struct from 0x00643420 is the PlayerChar struct that `BuildAppearanceModel` reads (the same call and field are read there).
- [C] When the paperdoll has a hero (this+0x58), it copies the hero's CAppearance (hero+0x7B0, 0x0074A610) and calls `CCCreature::SetAppearance`. In game mode 0xB it first sets hero+0x7B4 = 1.
- [C] Before all this it runs the paperdoll's queued commands (this+0x2E0..0x2E4) and clears +0x2EC.
- Unread: which step of character creation calls 0x00803000.

##### The clear: 0x007FEED0

- [C] 0x007FEED0 calls 0x00643420, takes field 0x4236 (16950) through 0x00A5A8E0, and calls 0x004DD730(0).
- [I] 0x004DD730 resizes the list, so this empties the hero's morph list. Its callers were not followed.

##### The reader: BuildAppearanceModel 0x006C0DE0

- [C] Guarded by a byte at +0x7D4 of the object from 0x005D58C0 being 1, a check through 0x0074A7C0(1), and two equal shorts at +0x58 and +0x5A. It then calls 0x00643420 and, at 0x006C0EBC, pushes 0x4236 and calls 0x00A5A8E0.
- [C] When the list is not empty (count from 0x004DB2A0), it allocates that many bytes (0x00413660) and copies the list into them (0x0047C940 per element).
- [C] It hands the buffer to 0x005895D0 on a stack object made by 0x00587BD0 (its type from the object of 0x00867E30, vtable +0x1C).
- [I] 0x005895D0 parses the morph file. That it is a morph parser is inferred from the call order and the bytes being a .mor.
- [C] On success it takes 0x00587C20's result into 0x00534A70, and stores that through 0x004F4710 at this+0x1E4 (`LEA ECX,[ESI+0x1E4]`). It then frees the buffer (0x004136C0).
- [I] +0x1E4 is the appearance's head-morph slot (CHARGEN.md's `UpdatePortraitMessage` writes the figure's head-morph name to `hero.appearance+0x1E4`).
- Names: none of 0x00643420, 0x00A5A8E0, 0x005895D0, 0x00534A70, 0x004F4710, 0x007FEED0 or 0x00803000 is named in the address library (tools/addrlib.py lookup); 0x006C0DE0 is `BuildAppearanceModel` (ID 14965).

##### What a save of the game's holds (Lyna/Slot_10)

- [F] The hero's `SavegamePlayerMorph` (16950) is a u8 list of 44,614 bytes: `GFF V4.0PC  MORPV0.1`, struct `morp`.
- [F] Inside it: field 2 (the name) is `Head_Morph`, MorphParts (23000) has 6 entries (`EF_UHM_BASa_0`, `ef_uem_basa_0`, `ef_har_ha2a_0`, ...), MorphTintFileNames (23001) has 12, and the nodes (23002) number 42. These include face bones such as `cheek_left` and `tongue1` (a position, no vertices) and meshes such as `FaceM1` (2,386 vertices, 5 texture, 4 vector parameters).
- [F] The hero's APP1 `SavegameAppearanceMorphName` (16328) is `''`; its type is 2 and its gender 2.
- [I] 16328 is empty in the game's saves generally; only this one save was read.

##### What the play showed

- Seen in play: the original game (GOG, an Eluvian instance with only the Eluvian plugin) loaded a save written by Paragon over a copy of Lyna/Slot_10, its 16950 replaced by a head made in Paragon's character creation. The head was written by core's `morph::Write` (DAOLib #237) over `hm_pcc_b01.mor`, named `Head_Morph`, and has **no bone nodes** (ComposeHead makes none).
- Seen in play, matching base: an elf woman's made head over an elf woman's save (Slot_10's hero is elf, female). The game drew the written head seated correctly on the body, with the shape the sliders gave it (a wide jaw, the nose).
- Seen in play, mismatched base: a human man's made head over the same elf woman's save. The game drew the written head floating above the body. Paragon's writer keeps the base's appearance type and gender, so the base's skeleton decides where the head sits.
- [I] The float comes from the human head's vertices sitting at a human man's height on an elf woman's skeleton, not from the missing bone nodes; the matching case, also without bone nodes, sits right.

##### Evidence

- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\game_written_elf.jpg: the original game, the written elf head seated.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\game_written_human_on_elf_base.jpg: the original game, the written human head floating over the elf woman's body.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\elf_before_chargen.png: the elf head on Paragon's appearance page, before the save.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\before_chargen.png: the human head on Paragon's appearance page.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\after_load_json.png: the human head in Paragon after loading its own JSON save.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\after_load_original.png: the human head in Paragon after loading the written original-format save.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\docs\Characters\Lyna\Saves\Slot_11\GH3476EamonsestateinDenerimLoghainmet.das: the written original-format save with the human head ("madehead original"); its 16950 bytes equal the JSON save's (54,416 bytes).
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\docs\Characters\Lyna\Saves\Slot_12\GH3476EamonsestateinDenerimLoghainmet.das: the written original-format save with the elf head ("elfhead original"), the one the game drew seated.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\hero_saved.mor: the human head's morph file, extracted from the JSON save.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\saves\madehead.json and elfhead.json: Paragon's own saves of the two heroes.
- C:\project\DAOParagonEngine-cutscenes\scratch\headsave\docs\Characters\Lyna\Saves\Slot_10\GH3476EamonsestateinDenerimLoghainmet.das: the scratch copy of the game's own save that both were written over, read for the Head_Morph above.

- **Equipment** (16218, 16219): an item's Position is (an INVENTORY_SLOT_* id, the weapon set 0 or 1, z ignored). Backpack and plot items use x and y as their place, with slot 0xFF.
- **Item lists** (0x007626C0, the same loader for every container): only when the list field is present, it clears and destroys the container's items first; each item is rebuilt from its ObjectID and TemplateResRef (0x00758340), and an entry with an empty ResRef is skipped.
- **Effects** (16623, GME1 with its DTA1 arrays): added by the common read, before stats, not cleared first, and not applied by the load; defaults in the file (priority 999 for duration type 6).
- **Commands** (16721 CMD1, 16740 CAQ1, SAC1): restored only on a saved game's load. `SavegameCommandID` 16723 is never read: commands get fresh ids.
- **A restored command resumes; it is not started again** [C]. The objects' commands are restored by 0x00698ED0, only when the loading state's byte +0x68 is set (a saved game's load).
  - `CommandPool::New` 0x007468C0 gives each command the pool's next sequence number, since 16723 is written but never read.
  - 0x00744DF0 reads one CMD1: type 16722, static 16724, data 16725, and player-issued 16726 if the field is there.
  - The CMD1 list (16721) is appended to the queue in order.
  - 0x00747100 reads the CAQ1 (16740): the current command (16720) goes to the current slot, then its sub-action list (16730) is put back raw. Each SAC1 keeps its type, flag, interruptible byte, TimeIndex (the elapsed ms), LastTimeIndex, Length (ms), StartTime (a delay in ms) and data, with nothing rebased or reset.
  - The command's start handler is not called, so COMMAND_PENDING is not raised again, unless sub-action 50 is still in the saved list.
- **What a restored sub-action does on its first step** (read 2026-10-21 by lib-agent-2, for DAOParagonEngine) [C unless marked]:
  - **The runner** (0x0073CB60) spends StartTime first. It then adds the elapsed ms to TimeIndex only when Length > 0, and calls the handler. No handler tests TimeIndex == 0 or reads LastTimeIndex.
  - **The "set up" test**: the swing (1, 0x007A87C0), the conjure (7, 0x007A66A0), play-animation (37, 0x007A4390) and the moves (4, 14, 15, 52) each test flag 0 of their data (the CDataArrays flags, saved as 0x4103). Wait (36) tests Length == 0.
  - **Moves, 4, 14, 15 and 52, always set up again.** The SAC1 writer clears **flag 0** of their data while it writes them and puts it back afterwards (0x00743D37..0x00743D6A: 0xA5A280 reads the flag, 0xA5B890 sets it to 0). The data's objects are saved intact, so the target survives a load; if the target's id has no live object, 4 and 15 end the command with −7.
    - On load the move update (0x007A9130) therefore resets the controller and calls RequestPath from where the creature stands, so the walk continues. It then sets flag 0 again (0x007A93E5).
    - The set-up also restarts the full timeout (float 5 → creature +0xCEC) and re-claims 4's ring spot.
    - 4, 14 and 15 end at once if the creature is already in reach or range.
    - 52 resumes at its saved waypoint index (int 5), but rewrites the waypoints from where the creature stands, so an offset may be applied twice [I].
    - Length and StartTime mean nothing to the moves.
  - **The turn (42)** keeps its flag. It calls TurnToward (0x007A51F0) every step on the fresh controller, so it turns on from the present facing.
  - **The swing and the conjure, if saved after their first step** (flag set), skip their set-up and only count Length − TimeIndex down. The clip is not restarted at the elapsed point, and **their impact event is lost**, not fired at once. A swing saved before its first step runs normally and raises COMMAND_PENDING.
  - **One-tick steps** (50 0x007A1E40, use 8, the ranged pending step 2, and 48) finish within a tick, so they cannot be saved half done. One still in the list on load runs from its top. 50 raises COMMAND_PENDING for an ability; for a melee attack the swing's first step raises it, and for a ranged one step 2.
  - **Wait (36)** keeps its duration in Length (+0x14). It is seeded from data int 0 only when Length is 0, so a restored Length stands. It ends when Length − TimeIndex ≤ 0, and answers −1 if the creature is dead.
- **The saved queue never holds the current command** [C]. The object writer (CCObject vtable slot 21, 0x00697EE0, its Name/Tag step at 0x00698150; the commands when byte +0x70 is set) writes the queue at object +0x2CC as CMD1s (16721), and the current command, kept apart at [object +0x2DC], alone as the CAQ1 (16740).
- **Tactics** (16821, 16822, PTT1/PTL1/PTE1): the table is read only when `SavegameTacticsHasTable` is not 0, and its rules replace the template's, not merge.
- **Dead creatures:** `TimeSinceDeath` 16481 goes raw into +0xC88, which the decay tick adds to; the corpse decays at the global decay time (60000 ms by default) and the body bag step runs at 1500 ms left. The bag's id (16600, +0x790) is saved but its made flag (+0x794) is not, so that step can run again after a load. Dead is the saved health (0) and the saved death effect, not a field of its own.

### The party (PRTY): CCParty::Load 0x00637D50, step 16

- In order: the pool members 16204 (CRL1, each skipped if its id exists), the leader 16278 (set only if that creature exists), the selected character 16270 (kept, applied at step 21), the plot manager 16400 (`PlotManager::Load` 0x0075E300), the inventory (`Inventory::Load` 0x00740790: backpack, plot items, money, recipes), the lines seen 16503, the flags 16291–16294 (auto level-up, quickbar locked, hold positions, run in drive mode), the quickbar's size 16310, the picker's status 16274 (2 when absent), the backpack's sort 16299, the approvals 16275, item storage 16288, new items 16290.

### The game state (GMST) and the event queue

- GMST holds the effect counter 16626, the event queue (`SavegameAIMaster` 16636) and the world timer 16700; no game mode, speed or pause is saved.
- **The event queue** (0x0064AAD0) loads only on a saved game's load. Each element's `SavegameEventID` picks its kind (0x0079C0C0): 10 and 24 are script events (0x0079C000); 6, 7, 11, 12, 13, 16 and 27 plain events (0x0079BF60); any other id is dropped. They are appended unsorted after MODULE_LOAD. Every event seen in real saves is id 10, 0 to 12 a save.
- **Written but never read:** a trap's 16103–16107 and a transition's LoadScreen 16110 (only writers use them, 0x00799F14 and 0x0079B8C5), VERS (no reader on the load path), `SavegameCommandID` 16723, the module's ObjectID by the campaign loader, and ADIN (16006) and StorySoFar (16008), for which no reader was found on the load path.

### The events during a save's load

- MODULE_LOAD (20) is queued at step 8; **MODULE_START (19) is not sent** for a save; MODULE_GETCHARSTAGE (63) runs at once at step 8, before the party and areas exist.
- No SPAWN, and no INVENTORY_ADDED or REMOVED, for restored objects (the inventory loads set the flag word 0x00C371B4).
- AREALOADSAVE_SPECIAL (41) runs at once at the area machine's step 3; AREALOADSAVE_PRELOADEXIT (42) and POSTLOADEXIT (43) are queued at its steps 4 and 8, each followed by an `AIMaster::Update` that drains the queue. So MODULE_LOAD and the saved events first deliver at the area step 4 [I: nothing drains the queue earlier].

### Not read

- The item's template load (vtable +0x1C0) and whether equipping on a load fires events or applies item stats; the step after stats (0x0079F4A0 on the effects holder); what 16020's filter means; the backpack's y (item +0x600), ITL1 16237 (a byte at item +0x46C), and 0x00C371B4's bits 0x2 and 0x40.
- Steps 2, 4 and 19's calls; what 0x005FE960 tests before MODULE_LOAD; the plot manager's derivation after its load (0x0075E5F6 on); the DTA1 loaders for vectors, quaternions and strings; whether an area transition sets the +0x68 byte.

### The typed reader

`src/dao/core/Savegame.h` (`dao::core::savegame::Read`, DAOLib #202) reads the save into plain structs: the campaign, the areas and their objects, the hero, the party and the game's state. Its header says what a caller does with the objects (the engine lays a saved object over its template, so a field the save lacks is none, not 0), and `Savegame.cpp` marks each part by the loader in the exe it was checked against, or as seen only in saves, or as kept raw. `tests/test_savegame.cpp` holds a save built by hand and, when `game/saves` has copies of the player's saves, their counts against this file's dump.

## The engine's writer (read 2026-10-21 by lib-agent-2 with three subagents, for DAOLib #221; C checked, I inferred)

The per-field tables (field id, name, GFF4 type, source offset, condition, in write order) are in `dev/SAVE_WRITER.md`.

- **Writers fill a struct whose template already exists, and the template decides what is saved** [C].
  - Every step is `Has(id)` and then a typed setter.
  - A setter silently does nothing when the template lacks the field, has it as another type, or has it as a list, struct or reference.
  - The helpers: u8 0x00478340, u16 0x006EB250, u32 0x00478420, s32 0x004783B0, f32 0x0062F610, vec3 0x005FE630, quaternion 0x006968E0, string 0x004799E0, tlkstring 0x00630810.
  - So a new struct must have the full vanilla template (PARAGON_SAVES.md), with each field's exact type. Some types are not what one would guess: u16 for a placeable's state 16100; u32 for ItemAbilityID 1011, an effect's Creator 16621 and LootableObjectID 16602; s32 for AOECreator.
  - A field the template has but nothing writes stays at the template's default, all 0xFF (16456 SavegameCanLevelUp reads −1 in every save).
- **The slots** [C]: every kind writes through slot 21 and loads through slot 34, and each kind's writer first calls the base writer.

  | kind | writer | loader |
  |---|---|---|
  | base CCObject | 0x00697EE0 | 0x00698ED0 |
  | CCCreature | 0x006B2470 | 0x006BE980 |
  | CCPlaceable | 0x0074D600 | 0x0074FE30 |
  | CCItem | 0x00754020 | 0x00758340 |
  | CCTrigger | 0x00799E10 | 0x0079A7E0 |
  | Store | 0x007EB510 | 0x007EC0B0 |

  - Nested writers: the effects list 0x00A64E20 and one effect 0x00A65490 (GME1); an item list 0x00761D10; the data arrays (DTA1) 0x00A5AA60; the creature's stats 0x0071BA40 (CRS1, or CPS1 for the hero), appearance 0x00748E90, equipment 0x00797700 and inventory 0x0073FC70; the variable table 0x00705A70; tactics 0x007ADF40.
- **The base writer's order** [C]: ObjectID 23 (s32) → Position 4 → Orientation 5 → ObjectLoopingAnimation 16256 → SavegameEffectList 16623 → Active 16201 → Immortal 16220 → Plot 16250 → TemplateResRef 3 → ScriptVarTable 17000 → EventScript 16221 → Name 16255 → Tag 16222 → the commands 16721 and 16740.
  - Active is written as `active && !(+0x421)`: an object whose deactivation is pending is saved inactive.
  - The Tag is written as "" when it equals the TemplateResRef; the loader maps "" back.
  - EventScript is written only if +0x288 is set.
  - Orientation is the animation controller's node quaternion, else it is built from the facing at +0x130.
  - TeamID 16453 and InteractionRadius 16262 are read by the base loader but written by the kind's writer.
- **The area** (`CCArea` 0x00683C80) [C]:
  - The area-list step 0x006FB3A0 removes the current list's areas from 16001 (a generic list of ARE1, element template `[0xC34BD0]`), then appends a fresh struct per area in list order, with only Tag (1) set.
  - The area writer then fills it, in this order:
    1. the effects (16623);
    2. ObjectID 23 (u32);
    3. the variables (17000);
    4. 16020;
    5. the lists, each sized first and then filled in the order of the area's object vector (+0x694): creatures 16011, triggers 16012, sounds 16019, placeables 16010, AOEs 16013, waypoints 16015, stores 16017;
    6. the map 16016 (one u8 per byte) and the rooms viewed 16018.
  - Each object goes into one list, chosen by its cast. Not written at all: party-pool creatures, items lying in the area, visual effects and projectiles.
  - A pannable sound is written only inside its sounds-region trigger (TRAV 16111).
  - Effects with +0x48 == 1 are skipped.
  - The party's current area and position are in MODU 17100–17103 (written by 0x005FEC00), not in the area list.
- **Writer and loader differ** [C]:
  - A container writes MaxItems after its item list, and the loader reads it before. A container overwrites each item's Position with its cell (index, item +0x600, 0), or (slot, weapon set, 0) for equipment, after the item's own fields.
  - Trigger geometry is written as each point minus the trigger's Position.
  - The sound writer always writes WOrientation as 1.0.
  - A waypoint with a LocText field writes NoteText as "".
  - Written but never read: the trap and transition fields 16103–16107 and 16110, a store's ObjectID, and the second word of a tlkstring (a literal name); the loader reads only the strref.
  - The store's will-not-buy and will-only-buy lists are appended on load with no duplicate test, so each save-and-load round trip doubles them (a vanilla fault).
  - An effect's started flag (+0x50) is neither saved nor loaded, so the first AgeEffect after a load starts it again and re-runs its visual effect's start.
  - Sub-items are saved as resrefs alone and remade on load.
  - A party-pool creature saves an empty Backpack: the party's Inventory is shared.
- **Not read** (the three files' own lists): the creature's property, quickslot and tactics writers in detail, the hero's PlayerMorph writer, the variable writer's match rules, the Equipment/Backpack/PlotItems own writers beyond the shared item list, and how the top struct is put on disk after the area steps.
### The typed writer (`Savegame::Write`, DAOLib #221)

`dao::core::savegame::Write(original, save)` (`src/dao/core/SavegameWrite.cpp`, DAOLib #221) gives the file's bytes with a changed `Save` written into them, through Gff4's `Editor`. It is the reader run the other way over a copy of the original, so what the reader does not model (the add-ins list, the story so far, the junk fourth float of a vector list) is untouched, and a `Save` read and written back unchanged is **byte-identical** (tested on a save made by hand, and on the player's saves in `game/saves`: five saves from 227 KB to 13 MB).

- **What it rests on.** The loaders read a field at its label's offset with the raw width and no type check (above), so a number is written only into a field of the member's width and a float only into a float. The 78 templates are the engine's own struct definitions, the same in every save, and every struct of a template has every field of it: so a field is "absent" only as a template that lacks the label (a creature's `CRE1` has no name, a pool member's `CRL1` no morph), and **the writer cannot add a field** (that would change the template's size and move every struct of it). A member the template lacks is not written; an `Opt` that is set, or a list that is not empty, is told in `Written::unwritten`. The conventions of the files the engine writes are the ones used: a list that is empty may be null or have a count of 0, an empty string is a null offset or a count of 1, a generic list's element is the template's index with the struct flag (0x4000) and the struct's offset, a talk-table string's own text is 0 where it has none. Checked in the saves with `tools/gff4.py`; the engine's writers (the journal's 0x006ED770, the tactics' 0x007AD880) write every field of the definition, which the 78 unchanging templates say too.
- **Nothing moves.** A string, a list or a struct that changes is appended to the data and its slot pointed at it; the old bytes stay as dead bytes. A scalar is written over its slot. Changes are found by reading the original struct, never by comparing two `Save`s: a string or list is written only if what the original reads as differs.
- **Lists.** A list element `Read` made carries `source`, the offset of the struct it came from; `Write` pairs it with that struct whatever its place now, so one taken out of the middle or moved keeps the file's own data for it. An element a caller made, or copied (a second use of one source), is a new struct built from its members: its template is the list field's, or for a generic list the element's kind (a variable's type, a stat's kind, a trigger's `label` or type, an event's `label`). A blank struct has every list, string and reference null and its object id -1 (the engine's default for a missing one): **a caller gives a new object its id**, and an area without one is ignored whole by the loader. An element whose kind the file has no template for is left out and told.
- **Children.** A child struct held by a null reference is made when its member is set (an `Opt` child), or, for a plain member (the journal, the world map, the party's plot manager, the game state's queue and timer), only if something is written into it. A child is never removed, and an `Opt` that is none is never a removal.
- **The command writer** (0x00698150, `Command::Write` 0x007439C0, `WriteActionQueue` 0x00745110; lib-agent-2, checked) writes 16722 to 16725 always and 16726 only if the template has it, 16720 for a current command, and per sub-action 16731 to 16738; a kept, unsaved area gets both lists empty, and a sub-action of type 4, 14, 15 or 52 is written with flag 0 of its data's flags array cleared (the array is saved as 0x4103; the writer puts it back, 0x00743D37) and its objects intact, so on load a move sets itself up again and re-paths from where the creature stands (the writer here writes the flags as the Save has them).
- **The engine's whole writer** (lib-agent-2, checked; its field tables are with #221): every object is written by CCObject slot 21 (0x00697EE0; the loader is slot 34, 0x00698ED0), then by its kind's writer (creature 0x006B2470, placeable 0x0074D600, item 0x00754020, trigger 0x00799E10, store 0x007EB510). Each step is "if the template has the field, a typed setter", and a setter does nothing for a field of another type or a list, struct or reference: so a new struct needs the vanilla template with the exact types (a placeable's state 16100 is u16; ItemAbilityID 1011, an effect's creator 16621 and LootableObjectID 16602 u32; AOECreator s32). A field the template has and nothing writes keeps the fresh struct's 0xFF (16456 reads -1); a blank made here is 0 in a scalar. The base order is ObjectID, Position, Orientation, 16256, effects, Active, Immortal, Plot, TemplateResRef, variables, EventScript, Name, Tag, commands. An area's lists are written creatures, triggers, sounds, placeables, AOEs, waypoints, stores, then the map and rooms viewed; party-pool creatures, items on the ground, visual effects and projectiles are never in an area. Where it differs from the loader: Active is saved as active and not deactivating; a Tag equal to the TemplateResRef is saved as ""; a container writes MaxItems after its items; an item in a container has its cell as Position (index, +0x600, 0), (slot, set, 0) when equipped; trigger geometry is relative to the trigger's Position; a sound's WOrientation is always 1.0; a store's buy lists double at each round trip (vanilla; the writer here writes them as the Save has them); a pool creature saves an empty Backpack.
- **Ids are the caller's, and one to an object.** The engine keeps one object to an id: a creature copied with its items holds the first one's item ids, and in play (2026-10-21) the copy's area list crashed when it was torn down (a freed item, `EXCEPTION_ACCESS_VIOLATION` at 0x20455241 in `FUN_006A00D0`, the walk over a creature's equipment, called from `CCCreature::~CCCreature`). `Save::DuplicateIds()` lists the ids a save holds twice.
- **The event queue** (DAOLib #226). The engine's loader picks an event's kind by its `SavegameEventID` and reads the template's label for nothing (0x0079C0C0). A plain event (6, 7, 11, 12, 13, 16, 27) is read by 0x0079BF60 from exactly five fields, 16631 to 16635 (day, time, caller, target, id), all in the model; a script event (10, 24) is the same five and then the SCE1 at 16650, and an event whose SCE1 cannot be made is dropped (0x0079C000) [I: a null SCE1 fails]. So the writer removes a plain event as it removes any list element (leave it out of `eventQueue`) and builds a new one from its members: `QueuedEvent::MakePlain(id, day, time, caller, target)` is a struct of the EBAS template, `MakeScript` an ESCR with its payload. A script event whose `script` is taken out is written afresh as an EBAS, since a child is never removed and the file's struct still holds the SCE1. A file with no EBAS template (the engine's saves have it) takes an ESCR with a null payload. Seen in the saves on this machine: 78 templates in each, both EBAS and ESCR among them.
- **Not checked.** The writer does not make the save agree with itself: ids against the id pools (`idPools`), the party's pool against its members, an area's rooms against the `.are`. The loaders skip what they cannot make (an item with no resref, an area with id -1, a map or pin the game's data lacks).
- **Not written.** The add-ins list and the story so far (they ride along), a field a template lacks, a struct held in a generic field of its own (none in a save), the removal of a child struct or of a field.

## The metadata (`.das.met`)

GFF4, type `SVMT`: what the load screen shows without opening the save. The area's name, the time played, the level, class, gender, race and background, the character's and the save's names, the journal's story text. Two fields xoreos does not name, 16809 and 16810, hold `single player;` and `SP;`.

**What the engine does with it** (read 2026-10-21 for DAOLib #226, checked in the exe): `SaveGame::Write` (0x00646280) names the files with the wide strings `.das` (0x00B06AD0) and `.met` (0x00B06AC4), the second appended to the first, so `<name>.das.met`, and `savegame` (0x00B06ADC) where the name is empty; the metadata is made by 0x00648110, which writes the character 16807, the save's name 16808, 16809, 16810, the area 16800, the story 16511 and then the time played, level, class, gender, race and background (16801 to 16806), the same fields as `SaveMeta`. The string `.met` is named in three places only, the writer and two readers (0x006CFC13 and 0x006D0D9D, the save list's refresh, which opens each save's file, checks it is a GFF V4.0 file and reads 16809 and 16810 out of it: [I] a save whose metadata lacks 16809 or cannot be opened is left off the list). **No code that loads a save names `.met`**: `FindSave` and the load path go by the `.das` alone, so a save with no metadata still loads (by a plugin's `Load` of its slot) and is only missing from the screen. The screenshot is not in this file: `screen.dds` and `<character>.jpg` (formats `%ls\%ls.jpg`, 0x00B06A84) sit beside the save.

`dao::core::SaveMeta::Write(original, meta)` (DAOLib #226) is the writer for it, in the style of `savegame::Write`: a copy of the original file in which each field that differs from what the original reads is set through Gff4's `Editor` (a string appended and its slot pointed at it, a number written over its slot), so a file written back unchanged is byte-identical, the story rides along unless `story` was changed, and 16809 and 16810 are never touched. It gives the bytes and the labels of any changed field the file has no field for (`unwritten`). It does not write the story from anywhere (the engine builds it from the journal), the picture, or the two unnamed fields.

## How the game writes and finds one

Read 2026-09-30 (`src/dao/Addresses.h`, `SaveGame`):

- **Asking.** A save or load is asked for with `SaveGameMessage` or `LoadGameMessage` to `SaveLoadProxy`, with a slot and a kind: 0 a manual save (slots to 500), 1 an autosave (1–4), 2 the quicksave, and a fourth kind.
- **The folder.** It is `<documents>\<character>\Saves\<Slot_|AutoSave_|QuickSave_><n>` (`SlotFolder`).
- **Writing.** `SaveGame::Write` writes `<name>.das` and `<name>.das.met` there, `savegame` if the name is empty.
- **Loading.** `SaveGame::FindSave` finds the `.das` in a slot's folder for loading.

Seen in play, 2026-09-30:

- **A quicksave.** The key sent `SaveGameMessage` with slot -1 (the handler chooses), kind 2 and no name. `SaveGame::Write` was then called for Aedan, slot 1, kind 2, and returned 1. Last came the screenshot's path, `...\Aedan\Saves\QuickSave_1\screen.dds`.
- **Loads.** A load from the menu sent `LoadGameMessage` slot 2, kind 0, and `FindSave` was called with the same. A quickload sent slot 1, kind 2.
- **An earlier quicksave did not call `Write`**, though its files were written. Saves run in the loading game mode, whose worker (`GameModeLoading::Worker`) calls `Write` only when a check passes, and what that check is has not been read.
- **The save and load screen.** As it fills, it asks `SaveLoadProxy::ScreenshotPath` for every slot of every character, with an empty path for one with no screenshot.

So a plugin's save data can hang on `Write` returning: it has the character, the slot and the kind, which name the folder. It can hang on `FindSave` or `LoadGameMessage` for a load. The unread check is the gap: a save that skips `Write` would be missed.

`dao/Saves.h` does that. `OnSaved` runs after `Write` succeeds, which is before the save's picture: the game asks for `screen.dds`'s path after `Write` (the quicksave above), and a plugin that read the slot's folder from `OnSaved` found the picture missing or of 0 bytes (seen 2026-10-12 by the multiplayer mod's session, which sends a host's save to its guests; the save loaded all the same). So `OnSaved` means the `.das` and its `.das.met` are written, and a plugin that wants the picture waits a frame or more for it. `OnGameLoaded` runs when a playthrough is in the world: at the module's load event after a `FindSave`, with that save's slot, or at the module's start event for a new game, with none, so a plugin never carries one playthrough's state into another. `Slot::Read` and `Slot::Write` take the plugin's own file in the slot's folder, which the game's own `SlotFolder` names. The check that can skip `Write` turned out to be a recursive walk over the slot's folder (`0x004C9B10`) that the worker requires to succeed; why one quicksave wrote its files without `Write` is still not read. Read 2026-10-05: `Write` (`0x00646280`) is the only code that names a save's files (its own `.das` and `savegame`, at `0x006463EE` and `0x00646387`), and its only caller is the worker (`0x006C6AD5`), so every save of every kind is written by it; that quicksave was most likely made before the hook was in place, not by another path.

Once that is read, a plugin's data can go beside the save under the same name, `<name>.daolib`, the way SKSE keeps a `.skse` file beside a Skyrim save.

## Asking for a save or a load

Read 2026-09-30 from the save proxy's handlers, for `dao::saves::Load` and `Save`:

- **The proxy.** `SaveLoadProxy` is the application's `+0x150`; the game mode controller passes `LoadGameMessage` and `SaveGameMessage` on to it (`0x00640140`). Its state is SmartData attributes, each holding its value at its `+0xC`.
- **Whose saves.** A load means the character chosen at `+0xE4` (its folder's name). `SelectHeroMessage` chooses by its place in the list at `+0x150`, whose records hold the folder's name at `+0x98`; `RefreshHeroesMessage` fills the list from the folders, reading each save's `.das.met`.
- **The load screen's mode.** `+0xAC` is the save screens' mode (`SetSaveLoadModeMessage`). Setting it to 1 or 2 makes the first character in the list the chosen one, or in the world the one played, and refreshes the saves. The load handler does that itself unless the mode is 2, which is how the load screen keeps a choice. So a load of a chosen character is: mode 2, `SelectHeroMessage`, `LoadGameMessage`. The handler sets the mode back to 0 once the load has begun.
- **The checks.** Both handlers first ask the game mode controller whether a save or load may happen now (`0x006CB740`). A manual save's slot of -1 takes the next free one (`+0x6C`), an autosave's comes from `+0x7C`, and the quicksave's must be 1 (`+0x8C`); a manual or quick save also needs `+0x5C`, which saving's being allowed sets. A save then begins only once there is a game (the application's `+0x14C`), and the save handler answers whether it did. It hands the slot, kind and name to the application (`0x005D7FC0`), whose save state copies the name.

## Resume and LastPlayedGame (read 2026-10-21 by lib-agent-2 with a subagent, for DAOParagonEngine)

Read 2026-10-21 by lib-agent-2 in the GOG exe (base 0x400000) with `tools/daoexe.py --disasm`, read-only; no Ghidra, no game. C = read in the disassembly (or a file on this machine), I = inferred.

### Short answers

1. **The setting is not called "LastSave".** The exe has no such string. It is the client option `LastPlayedGame`, a wide string in `DragonAge.ini` section `[GameOptions]`, held in `CClientOptions` (the global at 0x00C32938) at `+0x1A0`, the string at `+0x1AC`. Its value is the full path of a `savegame.das` (C). On this machine: `LastPlayedGame=C:\Users\tianx\OneDrive\Documents\BioWare\Dragon Age\Characters\Lyna\Saves\AutoSave_4\savegame.das`. It is read through 0x006CB910, which is not a reader of the setting: it is "can this path be opened for reading" (C). The setting is read in 0x006CC8C0.
2. **Written** by one function, 0x006CB8D0 (takes the .das path), called from exactly two places (C):
   - `SaveGame::Write` (0x00646280) at 0x00646622, right after the save's jpg is copied: on every save that goes through `Write` (manual, autosave, quicksave, post-campaign).
   - the save reader 0x00645650 at 0x00645799, after the .das has been opened and its header read, on every load, including a load that resume itself asks for.
   It holds the .das path, not a slot or a name. It does not hold the character separately; the character is the path's folder. 0x006CB8D0 sets the option, then calls `CClientOptions` 0x005E6280 (the same function `CommitOptionsMessage` calls, 0x005E6B89) and 0x005E4880(1). Seen: `DragonAge.ini` was last written 2026-10-19 17:30:54.591, two milliseconds after `AutoSave_4\savegame.das.met` (17:30:54.589) (C, file times).
3. **The list at the save proxy's `+0x134`** (the proxy is `[[0xC327EC]+0x150]`, `SaveLoadProxy`):
   - **Element:** a pointer to a 0x180-byte `SaveGameData` record (ctor 0x006CCA00), a SmartData record whose attributes each hold their value at `+0xC`. Registered names: `Slot` (+0x60, int), `SaveType` (+0x70, int), `Name` (+0x80), `ClassID`, `Area`, `Story`, `Screenshot`, `TimePlayed`, `LastPlayed`. The sort key sits in the record at `+0x58` (low) and `+0x5C` (high) (C).
   - **Whose:** one character's, the one chosen at the proxy's `+0xE4` (its folder name). Not all characters (C).
   - **Which saves:** in the load screens' mode (`+0xAC` not 1), all four kinds, folders `Slot_n` (kind 0, manual), `AutoSave_n` (1), `QuickSave_n` (2), `PostCampaign_n` (3). In the save screen's mode (`+0xAC` == 1) only kind 0 (C for the structure in 0x006D1160, I for the folder-to-kind names from SAVES.md:220 and the folders on disk).
   - **Order:** by the .das file's last-write time, newest first, all kinds mixed. The insertion in 0x006CF550 (0x006CFA58-0x006CFACD) walks the list and inserts before the first element whose 64-bit key is smaller; on equal keys the new one goes after the existing (C). The key is the `*.das` file's time as Unix seconds (the loop at 0x004C99A1-0x004C99C7 takes `ftLastWriteTime`, subtracts the 1601 to 1970 offset and divides by 10^7) (C for the conversion, I for which store feeds the record).
   - **Filled** by `RefreshSaves`' worker 0x006D1160 (called from `ResumeGame` at 0x006D162C via 0x006D15D0, and from the screens): it clears the list (0x006CF4E0), and if `+0xE4` is non-empty calls 0x006D0A50 for kind 0 and, when the mode is not 1, for kinds 1, 2 and 3. 0x006D0A50 lists `<Characters>\<char>\Saves\*` (mask `*`), and for each folder lists `*.das`, takes the first file (must be a file with size > 0), reads its `.das.met` header, and inserts a record with 0x006CF550 (C). In save mode (`+0xAC` == 1) 0x006D1160 also inserts a "new save" record at index 0.
   - **A save's name in the list** (0x006CF550; read 2026-10-23 by paragon-agent for DAOParagonEngine #204, checked by lib-agent-2) [C]:
     - **The format:** the name is `%ls%ls%ls` (0x00B10F60), filled with three parts.
     - **The word**, by kind (table 0x006CFB44):
       - kind 0 has no word;
       - kind 1 is strref 399579 "Auto-save";
       - kind 2 is 336243 "Quick Save";
       - kind 3 is 395082 "Post-Campaign Save".
     - **The number:** ` (%d) - ` (0x00B10F74), only when there is a word. The `%d` is the folder's own number (`AutoSave_n` and so on): 0x006D0A50 reads it with `StringToInt` (0x004C0950) on part of the folder's name (0x006D0D53) [I that the part starts after the `_`].
     - **The name**, the first non-empty of:
       - the `.das.met`'s `SavegameMetaSaveName` (16808, 0x006CC1D0);
       - its `SavegameMetaAreaName` (16800, 0x006CC130);
       - the fallback, the folder's `*.das` listing entry (0x006D0A50's `esi`, passed at 0x006D0F2C) [I that its string is the file's bare name].
     - **No readable `.das.met`:** when it is missing or is not GFF `V4.0` (0x006D0E7D), the name and area lookup is skipped and the name is the word, the number and the fallback (0x006CF9BB).
     - **Seen in the files on this machine:** an autosave's, a quick save's and a post-campaign save's `.das.met` all have an empty SaveName. So they list as "Auto-save (1) - Redcliffe" and so on (derived from the code, not seen on the screen).
     - A manual save with an empty SaveName lists as its area (from the code). Every manual save read had a name.
     - **Save mode:** in save mode only kind 0 is listed (0x006D1237 skips kinds 1–3), as the bullet above says.
     - **Slot bounds:** 0x006D0A50's kind table (0x006D1150) gives 501 for manual saves (slots 1–500), 5 for autosaves, and 2 for quick and post-campaign saves.
   - **How `ResumeGame` picks when `LastPlayedGame` is missing or stale:** the first element of the list, i.e. the newest save of the chosen character over all four kinds. Its path is rebuilt by 0x00645E00 from `+0xE4`'s folder name, the record's `Slot` and `SaveType` (that function lists the folder with `*.das` and returns the full path of the .das it finds), then tested for existence with 0x006CB910. "Stale" means the file cannot be opened; the path is not checked against the character (C).

### ResumeGame, step by step (handler 0x006D15E0, `SaveLoadProxy`, `ResumeGameMessage` dev/message_handlers.csv:303)

1. 0x006CB740: the game mode controller may allow a save/load now. That reads `[0xC327EC]+0x114` (the mode controller) and returns false in a state where it is not allowed. If false, nothing happens (C; the same check the save and load handlers make, dev/SAVES.md:245).
2. `+0xAC` (the save screens' mode, read through 0x0053CC40 = `[ecx+0xC]`) must not be 1 (save mode). In mode 1 nothing happens (C).
3. 0x006D15D0 = 0x006CBF10 then 0x006D1160. 0x006CBF10 chooses the character: the first record of the hero list at `+0x150` (0x006CBF1B-0x006CBFC9; records hold the folder name at `+0x98`, dev/SAVES.md:243), set as `+0xE4`; and, when a game is running (`5D57C0` returns the player and `5D5BE0` finds its creature), the player's name into `+0xBC` and `+0xE4` becomes the running game's character folder, `[0xC327EC]+0x14C` then `+0x18` (0x006CBFCC-0x006CC035). Then 0x006D1160 refreshes the save list for that character (above).
4. If the list `+0x134` is empty, it stops: no load, even if `LastPlayedGame` is valid (0x006D1637-0x006D164B, C). So a character with no saves cannot resume, even to another character's last game.
5. 0x006CC8C0 (out: a path string): a) if the options global is set, copies the string `+0x1AC` (`LastPlayedGame`) into the out string and returns 0x006CB910 of it (0x006CC8E9-0x006CC913); if the file opens, that is the answer. b) Otherwise it takes the first list element, `Slot` (`+0x60`'s `+0xC`) and `SaveType` (`+0x70`'s `+0xC`), builds the path with 0x00645E00 and returns 0x006CB910 of it (0x006CC91D-0x006CC9DF).
6. If that returned true, `ResumeGame` calls `CClientExoApp` 0x005DA7B0 with the path (`mov ecx,[0xC327EC]` at 0x006D166E): the same call a load makes (it makes a 0x38-byte save reader and calls 0x00645650 at 0x005DA83B). The handler returns 1 either way.

### Evidence

- 0x006CB910 (C): `mov eax,[0xAF0458]; mov ecx,[0xAF0444]; ... push 2; push eax; push ecx; push edx` then 0x004C9170 (the `ECFileAccess` open, vtable 0xAF0474), then `cmp [esp+8],-1; setne bl`. [0xAF0444] = 0x80000000 (GENERIC_READ), [0xAF0458] = 3 (OPEN_EXISTING). So it returns "the file opens".
- The string `LastPlayedGame` is at 0x00B01750, next to `AchievementDisplayTime` (0x00B01760) and `LastCampaign`; the options constructor 0x005E7DD0 registers the member at `+0x1A0` (0x005E7FDA, default empty; 0x005E968B resets it to the empty wide string 0x00ADEF14). `[GameOptions]` on this machine has the key (`DragonAge.ini` lines 19-38, `LastCampaign=single player`, `AchievementDisplayTime=5`, `LastPlayedGame=...`, `CompletedTraining=8`).
- The two other uses of `0xC32938 + 0x1A0` are 0x006CB8D1 (setter) and 0x006CC8E9 (reader); a scan of all 161 references to the global for a `+0x1A0`-`+0x1B8` access within 14 bytes found only these two (C, for that window only).
- Callers of the setter 0x006CB8D0: 0x00645799 (inside 0x00645650, ID 12673) and 0x00646622 (inside `SaveGame::Write`, ID 12680, `+0x3A2`). In `Write` the argument is built after the strings `savegame` (0x00B06ADC), `.das` (0x00B06AD0), `.met` (0x00B06AC4) and the jpg formats `%ls%ls\%ls.jpg` (0x00B06A64), `%ls\%ls.jpg` (0x00B06A84); the value in the ini is the `.das` path, so that is what it is (C by the ini, I for which local it is).
- Sort: 0x006CFA9D `cmp ebp,[eax+0x5C]; jg insert; jl next; cmp edx,[eax+0x58]; ja insert` at i = 0..n; insertion by `vector::insert` 0x006CE860 (index, ptr), append by 0x006F1340 when no earlier slot is found. The hero list at `+0x150` is built the same way in 0x006CEA00 (0x006CEF5C-0x006CEF86) by its record's `+0x58`/`+0x5C`, newest first (I: the key is the character's newest save time, found by 0x006CDE50 per kind, which keeps the greatest 64-bit time of the `*.das` files, 0x006CE0B1-0x006CE0C1).
- Why the main menu's Resume picks the newest character's newest save and the death screen picks the running character's: step 3 above.

### Notes for DAOParagonEngine

- To mimic: Resume = (mode allows) and (mode != 1) and (character has at least one save in `Characters\<char>\Saves\{Slot_,AutoSave_,QuickSave_,PostCampaign_}*\*.das`) then load `LastPlayedGame` if that file exists, else the newest `.das` by last-write time over those folders. Character = the running game's, else the one whose newest save is newest.
- A save made by a plugin through `SaveGame::Write` updates `LastPlayedGame` too; one that bypasses `Write` (writes its own .das) does not.
- Saves load through the same path (0x00645650) set it too: loading an older save makes it the "last" one, so "last save" means "last saved or loaded", not newest on disk. The fallback is by file time.

### Not read

- Where the main menu's Resume button is shown, hidden or greyed (whether it looks at `LastPlayedGame` or at the list); no code outside the two accesses above was found to read the option, but the scan was a window scan.
- Whether the options screen's "restore defaults" for the Game tab clears `LastPlayedGame` (0x005E968B's block is inside the constructor's range; its group was not read).
- What 0x005E4880(1) and the full body of 0x005E6280 do beyond being the commit path (the ini write was shown only by the file time).
- The 0x006CF550 argument order is answered (read 2026-10-23 by paragon-agent, checked by lib-agent-2): the arguments are the number, the kind, the listing entry, the 64-bit sort key, the meta struct (four words) and a string pointer. Still open: the `Story`, `TimePlayed` and `LastPlayed` sources, and the `SaveType` numbering past SAVES.md:220 (kind 3 as PostCampaign is from the folder names on disk and the iteration, not from a table).
- Whether the hero list's key (`+0x58`/`+0x5C`) is the newest save time over all four kinds or only some (0x006CDE50 is called four times, once per kind, and its out values were not followed to the record).
- The `ResumeGame` message's Flash side (what the death screen and main menu send exactly) beyond dev/COMBAT.md:621; no DAOTurnBased or DAOEluvianMCP note mentions `ResumeGame` or `LastPlayedGame` (grep of both repos' dev/).

## A save sent from one game to another (seen 2026-10-12)

The multiplayer mod's session, two games on one machine (its master 8bbaff0, Lyna's manuals 342 and 16, a save of 7.9 MB): the host's game saved by `dao::saves::Save` to a manual slot of its own choosing, the slot's `.das`, `.das.met` and `screen.dds` sent to the other game, written there into the same slot of the character it was playing, and loaded by `dao::saves::Load`. The guest was in the host's world 9 to 19 seconds after. So a save's folder is all a save is, it loads in another profile as it is, and `Load` works from a plugin in a loaded game. The load numbers its new objects from the save's own counter (the guest's build began at the id the host's had, 276461, and made the same 109 objects), so after it the two games agree on every id again. A character the profile had never had works too (the same session, later that day, twice): a third game with no character and no save was sent the host's save, written to a slot under a character folder made for it, and `Load` of that slot from the main menu loaded it. Not tried: a slot whose folder is one character's and whose save is another's.

A load numbers its new objects from the save's counter, which is where the game that wrote the save numbers what it makes next. So a host that writes a save for a guest and plays on, and a guest that loads it, give the same ids to different things: seen there, the host's next creature and the guest's character-screen stage (`char_stage` and its waypoints, the load's hundred-odd new objects) both at 0x31562. Two ways out, both seen in play: the host moves its own numbering on after the save (`NumberNewFrom`), or the guest has the load's build numbered elsewhere (`OnAreasBuild`'s `starting`, which is followed for a load's build as for any: in one game, 2026-10-12, a load asked for from inside the game with `starting` answering 0x20000000 built its 110 new objects at 0x20000001 on, none refused, and left the counter at 0x2000006E). In that run `starting` was given the save's counter (0x314EA), the game's own having been 0x1131562 before the load, so the counter is the save's by the time the build begins; the multiplayer mod's session saw `starting` given the game being left's instead, which is not explained.

## What it means for plugins

A save is the engine's own objects written out. A plugin that keeps state of its own needs it saved and loaded with the game's: DAOLib's "save and load" item (`dev/LIBRARY.md`). The script variables are one place the game already saves by name. A plugin's own data could ride in a table of its own, as SKSE's co-saves do beside Skyrim's saves.
