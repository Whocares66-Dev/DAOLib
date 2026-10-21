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

## The metadata (`.das.met`)

GFF4, type `SVMT`: what the load screen shows without opening the save. The area's name, the time played, the level, class, gender, race and background, the character's and the save's names, the journal's story text. Two fields xoreos does not name, 16809 and 16810, hold `single player;` and `SP;`.

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

## A save sent from one game to another (seen 2026-10-12)

The multiplayer mod's session, two games on one machine (its master 8bbaff0, Lyna's manuals 342 and 16, a save of 7.9 MB): the host's game saved by `dao::saves::Save` to a manual slot of its own choosing, the slot's `.das`, `.das.met` and `screen.dds` sent to the other game, written there into the same slot of the character it was playing, and loaded by `dao::saves::Load`. The guest was in the host's world 9 to 19 seconds after. So a save's folder is all a save is, it loads in another profile as it is, and `Load` works from a plugin in a loaded game. The load numbers its new objects from the save's own counter (the guest's build began at the id the host's had, 276461, and made the same 109 objects), so after it the two games agree on every id again. A character the profile had never had works too (the same session, later that day, twice): a third game with no character and no save was sent the host's save, written to a slot under a character folder made for it, and `Load` of that slot from the main menu loaded it. Not tried: a slot whose folder is one character's and whose save is another's.

A load numbers its new objects from the save's counter, which is where the game that wrote the save numbers what it makes next. So a host that writes a save for a guest and plays on, and a guest that loads it, give the same ids to different things: seen there, the host's next creature and the guest's character-screen stage (`char_stage` and its waypoints, the load's hundred-odd new objects) both at 0x31562. Two ways out, both seen in play: the host moves its own numbering on after the save (`NumberNewFrom`), or the guest has the load's build numbered elsewhere (`OnAreasBuild`'s `starting`, which is followed for a load's build as for any: in one game, 2026-10-12, a load asked for from inside the game with `starting` answering 0x20000000 built its 110 new objects at 0x20000001 on, none refused, and left the counter at 0x2000006E). In that run `starting` was given the save's counter (0x314EA), the game's own having been 0x1131562 before the load, so the counter is the save's by the time the build begins; the multiplayer mod's session saw `starting` given the game being left's instead, which is not explained.

## What it means for plugins

A save is the engine's own objects written out. A plugin that keeps state of its own needs it saved and loaded with the game's: DAOLib's "save and load" item (`dev/LIBRARY.md`). The script variables are one place the game already saves by name. A plugin's own data could ride in a table of its own, as SKSE's co-saves do beside Skyrim's saves.
