# Combat as the engine runs it: perception, the attack command, effects

Read 2026-10-18 by lib-agent-2 (three subagents in parallel) in the GOG exe and the toolset's scripts, for DAOParagonEngine's S5 (a fight), at paragon-agent-2's request. What the engine does, and where it hands over to the scripts (`creature_core`, `rules_core`, `combat_h`, `effects_h` and the `effect_*_h` handlers). Each section says what was read in the code and what is inferred. Builds on `dev/TURN_BASED.md` (the approach, the melee ring, the command queue) and `dev/AI.md`. Not yet seen in play as a whole.

## Perception

Read 2026-10-18 in the GOG exe by lib-agent-2 (a subagent), for DAOParagonEngine's S5. "Read" is what the code shows; "inferred" is a reading of its purpose. A creature's perception list is the `std::set` of ids at creature+0xC18 (`IsPerceiving`, `GetPerceivedCreatureList`; `src/dao/Addresses.h`, the melee rings' note).

### When

- `CCCreature::Update` calls the perception update `0x006B89B0` (`__thiscall`, the perceiver) when the creature's +0xBD4 is 0 and the manager at `*(client app+0x114)+0x74` answers true through its slot 0x4C. Read; what that slot paces: "The perception range and its pace" below.
- The update does nothing for a creature whose slot 0x1D0 answers non-zero (inferred: dead), or which has no area or no area+0x41C.
- A countdown at +0xC38 is lowered by the frame's milliseconds (`[+0x2B8]+8`); at or below 0 the byte +0xC3C is set ("a fresh look"). `0x006A3690` sets the countdown again when a hostile is noticed (a value from `0x00A2CD70(0xB, 1.51)`, read as a game rule, inferred), and the update sets it to 0 when nothing changed. Read.
- In order, each update: (1) raises APPEAR for each id in a pending list at +0xC28 (`0x006AFCF0(other, 1, 0)`), then empties it; (2) checks every id in the perceived set and raises DISAPPEAR for each lost one; (3) unless slot 0x1D0 answers, looks for newcomers (`0x006B8540`). Read. What fills the pending list is not read; the APPEAR it raises has int 2 = 1, which rules_core reads as "already perceived but hostility changed" (inferred: the hostility changes).

### Range, line of sight

- Newcomers (`0x006B8540`): the area's spatial query (`0x00686F70` on area+0x278) with a sphere about the perceiver's position (+0x120) of the perceiver's perception range, the float at +0x68 of the attribute at +0x7C4 (read; where it comes from: "The perception range and its pace" below). For the party's leader (the id at +0x1E8 of `0x005D57C0()`'s object, the attribute `GetPartyLeader` reads; not the controlled creature, `[[app+0xD0]+0x5C]`: corrected 2026-10-18 by lib-agent-2, `dev/PARTY.md`) with the party's range at party+0x42C above 0, the query uses that party range and a creature counts as in range when its 3D distance squared is within the creature's own range squared. Read.
- Line of sight, for hostile candidates only: if the two stand in different rooms (+0x150) that `0x00687EC0` does not connect, or the raycast `0x0067C530` (ID 13636, unnamed; not `CCArea::LineOfSight` 0x0067C3F0, corrected 2026-10-18 by lib-agent-2) from eye to eye (each position with z + 1.8) fails, the candidate is not seen. Read; "Perception through doors" below. Non-hostile candidates in range are perceived with no line-of-sight check. Read.
- Staying perceived (`0x006A8D60`, called with its second argument 1): lost when the other is gone, in another area (+0x254 differs), fails `0x00A285D0` on its +0x41C, answers its slot 0x1D0 (dead, inferred), or its +0x70 interface's slot 0x6C is not 1 (inactive or hidden, inferred); or when the perceiver answers slot 0x1D0; or when the distance (`0x0067A000` between the positions) is 60.0 m or more. That distance is 2D, x and y: the third term is `fldz; fmul st0,st0`, z multiplied as 0 (`0x0067A02D`, checked; corrected 2026-10-18 by lib-agent-2, not 3D), so the 60 m is horizontal. With argument 1 the range is a fixed 60 m and no line of sight is tested, so a creature once perceived stays perceived out to 60 m whatever the perception range (read). A flag from `0x006A44D0` (inferred: stealth) also ends it, and the update then calls `0x006A3690(id, 1)`.

### Perception through doors: a hostile behind a closed door is not perceived (read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe and the game's files by lib-agent-2 (with subagents) for DAOParagonEngine. Not seen in play. The key claims were checked again in the disassembly.

**The candidates** (0x006B8540 → the area's query 0x00686F70 → 0x00685B50), for hostiles and others alike, need all of:
- a room (+0x41C); a perceiver with none perceives nothing (0x006B8A11);
- a room whose bit (room +0x278) is in the perceiver room's 128-bit visible mask (+0x288, tested by 0x0067A3C0). The mask is built once (0x00A28FC0) from the room itself and its `EnvRoomVisibilityList` (.rml field 3047, CRoom +0xB0). A room numbered 128 or more has an empty mask;
- a 3D distance within the range: `ranges.gda` `PrimaryRange` by `PerceptionClass`, usually 30 m. For the controlled creature, the party's range for the query, then its own.

**A hostile** (0x006B8880–0x006B88F7, checked) is then tested twice. Non-hostiles get neither test.
1. If the rooms' numbers differ (perceiver room +0x14 against the other's +0x150), the room walk 0x00687EC0 must connect them (0x006B88C8).
2. Always, in the same room too: **0x0067C530** (0x006B88DF), a PhysX raycast from eye to eye (each position + 1.8 m, 0x00B004D8) with mask 0x24 (0x0067C5C7), clear only if it hits nothing.
   - The mask is the ray's NxGroupsMask bits0 (the groups argument is 0xFFFFFFFF): a shape blocks when its bits0 has 0x4 (placeables with collision: doors, crates, statues) or 0x20 (StaticGeometry, all of a level's static shapes: walls, floors, props, tree trunks, terrain); creatures (0x10) don't. So trees and walls block sight. Corrected 2026-10-18 by lib-agent-2 (it said 0x20 was the ground); `dev/COLLISION.md`.
   - **This is not `CCArea::LineOfSight`** (0x0067C3F0), which lets only placeables of base type 1 (door) or 8 block (0x0067A670). 0x0067C530 is the address library's ID 13636, unnamed.

A failure skips the candidate, except in a stealth-detection case. Once perceived, a creature stays perceived out to 60 m with neither test (0x006A8D60), so a door closed afterwards changes nothing.

**The room walk** (0x00687EC0, `__thiscall (CRoom *origin, int targetRoom)`, `ret 8`):
- The target is found by number over the layout (0x0067B430).
- If the target has no connectivity list (`EnvRoomConnectivityList`, .rml field 3099, CRoom +0xD0), the answer is whether the origin can see it (CRoom slot 0x54, 0x005D22E0: the same room, or in the visibility list).
- Otherwise a depth-first walk (0x00686A70) with the visited list at area +0x470:
  - each room passed through must be in the origin's visibility list;
  - a step between two rooms is taken when no door is registered for the pair (the map at area +0x634, keyed by (min, max) room, 0x00685100), or when any door registered for it doesn't block.
- **A door blocks** (0x0074BCC0, checked) when it is active (+0x2C5 = 1), has a state controller (+0x654), and its state's (+0x658) `BlockVis` ≠ 0. It is read live: nothing is cached. The area's +0x608 flag only re-runs `CCArea::RoomsSeen` (0x006886F0), the rooms drawn and fogged.
- **A door is registered** (0x0068C190, from the area's add 0x00750B10) when one of its states has `BlockVis`, its RoomIDs (+0x6A8, +0x6AC) are not both -1, and they differ. A door with two equal RoomIDs is never registered. A door with one -1 is registered under a pair the walk never asks for. Either blocks perception only through the raycast.

**A door's collision follows its state** (`SetPlaceableState` → 0x0074E850 → 0x0074D350, for base type 1, checked): a blocking state restores the physics groups mask kept at model-ready (+0x6D8); any other sets it to 1 (0x0074D398), out of mask 0x24. So an open door doesn't block sight and a closed one does (inferred: its kept mask has bit 0x4).

**The Circle's basement** (`bhm700ar_tower_basement.are`):
- `bhm700cr_sentinel.nss` wakes a sentinel only on `PERCEPTION_APPEAR` of the hero or a party member, on `DAMAGED` or on `DYING`.
- Never registered, so blocking by the raycast alone: `bhm700ip_sidedoor` (1103/1103), `bhm700_ip_first_door` (1101/1101), two `genip_door_tev_lrg` (1113/1113).
- Registered: `bhm700ip_seconddoor` (1325/1103, locked) and the `tev` doors 1113/1152, 1168/1152, 1174/1168, 1211/1174, 1291/1278, 1248/1211, 1221/1211, 1278/1248 (closed).

### Who

- Candidates are what the area's query returns; the perceiver itself and anyone already in the set (`0x008704D0`) are skipped. Read. No group filter beyond hostility.
- Hostility: `0x006A2B80(other)` is `IsObjectHostile(perceiver, other)` (when the client app's +0x1B0 is set). Read.
- A hostile candidate whose +0x7F0 is non-zero (stealthed) is not added to the set. When in range and in sight it re-arms the countdown (+0xC38, `0x006A3690`, 6000 ms from globals.gda row 11 STEALTH_CHECK_INTERVAL_MS). +0xC3C means 'the countdown ran out this update'. APPEAR is raised with bStealthed = 1 when +0xC3C is set (or for the controlled creature without party flag 0x10). The engine itself rolls nothing against stealth; the roll is the scripts' (the stealth section below). The term with +0xB94 and party +0x414 is the party's Survival tracking (the tracking section below), not stealth detection. (+0xB94 is the creature's trackable flag: `SetCreatureCanBeTracked` (764) writes it at `0x0066BFF7`, read 2026-10-18 by lib-agent-2.)
- Otherwise a candidate in range is added to the set and APPEAR raised at once (`0x006B3240`: `0x006B1120` inserts the id, then `0x006AFCF0(other, 0, 0)`); one in the party's range but not the creature's own is added with no event. Read.

### The events

- APPEAR (`0x006AFCF0`, `__thiscall (perceiver; other, int2, int3)`): type 25; target field (+0x28) the perceived; int 0 hostile (`IsObjectHostile`), int 1 the perceived's +0x7F0 (stealthed), int 2 and int 3 the arguments (int 2 = 1 from the pending list: "hostility changed"), object 0 the perceived. Queued to the perceiver: `AIMaster::AddEventDeltaTime(0, 0, perceiver, perceiver, 10, event)`, no delay. Read. rules_core reads int 0 nHostile, int 1 bStealthed, int 2 nHostilityChanged, object 0 oAppear.
- DISAPPEAR (in `0x006B89B0`): type 26; target field the lost one; object 0 the lost one; flag 0 hostile; queued to the perceiver the same way. Read.
- Neither is raised when the gate `0x00698490(type)` answers false (it looks the type up with `0x005FE960`, as the module's load does before queuing MODULE_LOAD: the type is in the object's disabled set, +0x28C, which only `EnablevEvent` changes and no shipped script calls; corrected 2026-10-18 by lib-agent-2, `dev/LOAD_EVENTS.md`), nor, unless `0x006A2CE0()` answers true, when either creature's +0xBD4 is 1. Read.
- The set changes in the same update as the event is queued: the insert just before APPEAR, the erase (and the melee rings' `0x007CBC50`) just after DISAPPEAR, so a script handling the event already sees the new list. Read.

### The perception range and its pace

Read 2026-10-18 in the GOG exe and the game's files (ghilana) by lib-agent-2 (a subagent), answering what the section above left open.

- The range is the creature's `PerceptionClass` row of `ranges.gda` (table 134), column `PrimaryRange`, in metres. The creature's load (`0x0071F5CC`–`0x0071F60D`, in the object at creature+0x7C4) reads the template's or save's byte `PerceptionClass` into +0x64, then `GdaTables` table 0x86 (134), row = that class, column `PrimaryRange` (hash `0x45BD822C`) into the float +0x68. Read. Nothing else writes it: no script command sets a perception range or class (`dev/script_commands.csv` has only `TriggerPerception` 166 and `ClearPerceptionList` 446), and the other readers of `PrimaryRange` are weapon and spell ranges. Read.
- `ranges.gda`'s perception rows (RangeType 1): 8 `PercepRngShrt` 10 m, 9 `PercepRngMed` 30 m, 10 `PercepRngLng` 40 m, 11 `PercepRngDefault` 30 m, 12 `PercepRngPlayer` 35 m, 13 `PercepRngMonster` 30 m, 14 `PercepRngCrowd` 5 m, 15 `PercepRngLargeCreature` 40 m. `SecondaryRange` (10 to 30) is not read for perception here.
- The usual value: of the 2,486 `.utc` the game takes (ghilana; 112 in enciphered add-ins not searched), 2,266 have `PerceptionClass` 9, so 30 m; 81 have 10 (40 m), 66 have 8 (10 m), 38 have 14 (5 m, crowds), 21 have 12 (35 m, the player's and the default heroes', `default_hm_noble` among them), 6 have 11, 5 have 15, 2 have 13. So a humanoid NPC sees 30 m, the player 35 m.
- The pace: the manager `CCCreature::Update` asks is the current game mode. `*(client app+0x114)` is the game mode controller (`dao/GameModes.h`) and its +0x74 is the current mode's object, set from the modes table at +0x58 when the mode changes (`0x00641E7B`; +0x78 is the mode's number). Its slot 0x4C answers true (`0x004137C0`, `mov al, 1`) only for `GameModeExplore` and `GameModeCombat`; every other mode (dialog, cutscene, GUI, loading, dead, chargen, movie, party picker, fly cam, fixed, pregame) answers false (`0x005C5E80`). Read.
- So the perception update runs on every creature update while the game is in explore or combat mode, and not at all in a conversation, a cutscene or a menu. Nothing in the call throttles it; how often `CCCreature::Update` itself runs (each frame for each creature, inferred) is the only pace. Read for the gate; the per-frame cadence is inferred.

### Stealth (read 2026-10-21 by lib-agent-2 with two subagents, for DAOParagonEngine; C checked, I inferred)

- **The flag** [C]: creature +0x7F0. It is written only by 0x006A2400, which is reached from `SetStealthEnabled` (758), the save's load, the template's load (`StealthMode`) and a setter of property 0x77. The engine never clears it on its own: not for an attack, a hit, an ability, running or combat.
- **What the engine does with it** [C, 0x006B8540]: a stealthed hostile is never added to a perceiver's set. If it is in the perceiver's own range and passes the room walk and the ray, the engine raises APPEAR (int 1 bStealthed = 1) once the 6 s countdown has run out, and re-arms the countdown. So a stealther in sight is re-announced every 6 s, and nothing else happens in the engine.
- **The detection roll is the scripts', and only against followers** [C, rules_core's APPEAR handler]:
  - `Stealth_CheckSuccess` compares the stealther's stealth rank with the perceiver's rank, taken from the `autoscale` table's `NStealthRank` (rows 0–8: 0, 1, 1, 1, 1, 2, 3, 4, 0) by creature rank. It applies only when the stealther is a follower and in combat; out of combat it always passes.
  - A failure calls DropStealth.
  - Either way the handler returns early, so no combat starts from it. There is no random roll.
- **Entering** [C, `skill_stealth.nss`]: the modal skill 100075 (cooldown 10, no duration) applies EFFECT_TYPE_STEALTH, crust vfx 90284 and a movement-speed effect of 0.6, plus 0.2 each for 310030 and 310032.
  - Out of combat it is always allowed.
  - In combat, ranks 1–2 need no hostile within 30 m (`STEALTH_COMBAT_DISTANCE_DETECTION`).
  - Combat Stealth and Master Stealth need a stealth rank above that of every perceived creature within 30 m.
- **What breaks it** [C, the scripts]:
  - Any ability use. Items are spared 90 % of the time at rank 2 and up, and Stealing never breaks it.
  - Non-physical damage only. Combat Stealth and Master Stealth keep stealth with a chance of level × 0.01 or × 0.02, at most 0.8.
  - An effect type with `BDropStealth` = 1 (23 rows of effects.gda: damage over time, placeable use).
  - A forced perception.
  - An attack from stealth threatens a critical hit, and a hit on a stealthed target becomes a miss. Whether a plain attack breaks stealth was not read.
- **Templates and monsters** [C]: `StealthMode`, `StealthRadius` and `Trackable` are read only by the engine's template load (0x006C06F0). Of 7,235 readable .utc files, only `aluviandarkstar.utc` has `StealthMode` (0), and none has the other two.
  - A creature with the talent that is hostile to the party spawns stealthed (`creature_core.nss`:504).
  - The AI refuses stealth in combat without Combat Stealth, when no ally is alive, or when an ally is already stealthed.
  - Jarvia stealths by script.

### The party's tracking (Survival) (read 2026-10-21 by lib-agent-2)

- **The party's 0x00631F30** [C] recomputes party +0x414 from the best active member's Survival rank (abilities 100051–100054) and sets flag 0x20 when a member has Sense Darkspawn (4050). It runs only on a party add or remove, an area change and an ability's add or remove, so a level-up does not refresh it [I].
- **Who is tracked:**
  - Survival 1 to 3 track a creature with Trackable (+0xB94, default 1; `SetCreatureCanBeTracked` 764) whose level is at most the best member's level − 2, the level, or the level + 2 respectively. Survival 4 tracks any creature [C for the structure; I for the level mapping].
  - Flag 0x20 tracks darkspawn: race 7 is DARKSPAWN.
  - A tracked creature enters the leader's perceived set silently, past range and sight.
- **Range:** Survival also widens the leader's perception range to 42.6, 51.1, 60.4 and 71 m.
- **Flag 0x10** (Survival 4) makes APPEAR fire on every update.
- **Not read:** the identity of the creature's +0x4F8 flag (it looks like "party side"), and the property 0x77 setter's callers.

## The attack command

Read 2026-10-18 in the GOG exe and the toolset's scripts (`rules_core.nss`, `combat_h.nss`, `effect_damage_h.nss`, `effect_death_h.nss`, `events_h.nss`), building on what `dev/TURN_BASED.md`, `src/dao/Animation.cpp` and `src/dao/Addresses.h` already record. "Read" means seen in the code; "inferred" is marked.

**One command is one swing.** `CommandAttack` (566, `0x0066e710`) makes a `COMMAND_TYPE_ATTACK` (1). Its step (`0x00742FD0`) queues an approach and the attack, sub-action 50, which queues a turn to face (39) and one swing (1, `0x007A87C0`) for melee, or the shot's steps for ranged (`dev/TURN_BASED.md`, `src/dao/Animation.cpp`). When the swing's wait ends, the command finishes and `COMMAND_COMPLETE` is sent; the AI then chooses again (`rules_core` → `AI_DetermineCombatRound`). It does not repeat: the scripts queue the next attack (read in the structure; the one-swing rhythm also seen in play, `dev/TURN_BASED.md`: "a creature re-decides after each attack").

**The approach** is `dev/TURN_BASED.md`'s "How close a melee attack strikes from": melee by the melee ring (sub-action 4; reach = both `PerSpace` + 1.2 m, and within 1.25 m of the ring place), ranged by the weapon's `bitm_base` `Range` (sub-action 15, capped at 60 m, with line of sight). Weapons add nothing to melee reach. `dao/Attack.h` calls these.

- **A ranged attacker's reach in practice is perception's, not the weapon's** (read 2026-10-21 by lib-agent-2, for DAOParagonEngine #156):
  - **The engine's band is the weapon's Range** [C]: the attack step (0x00742FD0) reads only `bitm_base.Range` (0x00743095; 45 m for short bow, long bow, crossbow and staff), caps it at 60 m (0x0069F330) and hands it to sub-action 15 with the two personal spaces.
    - Nothing in the step, sub-action 15 or the shot reads `ranges.gda`'s `WeaponRng*` rows (10/30/35 m), the `PerceptionClass` or `IsPerceiving`.
    - The approach's and the shot's sight test 0x0067C750 → 0x0067C610 → `CCArea::LineOfSight` (0x0067C3F0) has no distance test. It only checks that the ray's hit is within 0.1 m of the target.
  - **The scripts attack their threat target; perception is what usually puts an enemy there** [C]:
    - An enemy's target is `AI_Threat_GetThreatTarget` (`threatsystem.md`, "Where threat comes from"). It returns the stored target even when that target is not perceived (`ai_threat_h.nss`:208–226).
    - An enemy first enters the threat table through `PERCEPTION_APPEAR`, or through being hit by it (damage, a hostile ability, an attack). An archer that has not yet been perceived or struck has no entry, and so no target.
    - The AI gives no firing distance of its own. A ranged attacker gets a plain `CommandAttack` (`_AI_ExecuteAttack`, `ai_main_h_2.nss`:2548–2850).
    - Only a *stationary* creature tests the range itself: `GetItemRange` = `bitm_base.Range` (`2da_data_h.nss`:192), then `CheckLineOfSightObject` (`ai_main_h_2.nss`:2849–2895).
  - **So an archer engages at its perception range** [C for the ranges, I for the play figures]:
    - It is acquired at `ranges.PrimaryRange` by `PerceptionClass` (a 3D distance). The hurlock archer `pre200cr_hurlock_archer` and most NPCs have class 9, 30 m; the player has class 12, 35 m.
    - It is kept until 60 m on the ground (this file's Perception section).
    - Once it has a target it shoots from wherever it is, up to the weapon's 45 m.
    - Play (DAOParagonEngine, staged spawns): shots at 29.0 and 33.3 m; an archer placed at 37.8 m walked in and had not shot after 15 s. This fits acquisition at about 30 m plus staging slack [I].
  - **Being shot makes the target perceive the shooter** [C]. At an attack's impact `rules_core.nss`:556 signals `EVENT_TYPE_ATTACKED` (1001) to the target (`SendEventOnAttacked`, `events_h.nss`:410–423). Its handler (:625–) calls `WR_TriggerPerception` both ways unless the target is stealthed. `TriggerPerception` (166, 0x0065A7B0 → 0x006B32A0) adds the other to the perceived set without a sight test, when it is in the same area, alive, active and under 60 m on the ground (0x006A2BB0, the double 60.0 at 0x00B03BF8). A follower struck while idle then runs `AI_DetermineCombatRound` (`dev/PARTY.md`:525). So a party shot from beyond its own perception range still answers.
  - **What walks an enemy archer in** (read 2026-10-21 by lib-agent-2 with a subagent):
    - **Not its tactics** [C]: the hurlock archer's AI package 10065 (`aip_ds_archer`) has no "move closer" and no plain attack row. Its only moves (rows 3–6) back away 13 m from an enemy within 12 m (`AI_RANGE_MEDIUM`; short is 3.5 m, long 80 m), so the archer ends in the default action, a plain `CommandAttack`.
    - **Not the AI's ranged branch** [C]: `_AI_ExecuteAttack` gives no distance and does not switch to the dagger at range.
    - **The allies' help on perception** [C, `rules_core.nss`:1000–1062, `PERCEPTION_APPEAR`]:
      - When a creature perceives a hostile, each ally from `_AI_GetAllies` (within `AI_RANGE_MAX_ALLY_HELP`, 15 m) that is not yet in combat is moved, if it is a non-follower, not stationary (`AI_FLAG_STATIONARY` 0) and not in a climax army.
      - The move is `CommandMoveToObject(enemy, run)` after `WR_ClearAllCommands`.
      - Only while the perceiver's own `AI_HELP_TEAM_STATUS` is 0 or 1. The first such ally sets it to 4, so one ally is moved per perceiver [C for the code, I for the effect].
      - The moved ally keeps walking until it perceives the enemy itself, at its 30 m, and its own APPEAR starts its combat [I for where the walk ends].
    - **The first shot's delay** [C, :962–990]: a non-follower that first perceives a hostile more than 25 m away, with a non-melee weapon and not from a hostility change, queues `CommandWait(0–2 s)` (`AI_TRIGGER_DELAY_RANDOM_MAX` 2.0) instead of its combat round.
    - **The stationary flag is per placed instance**, in the area's `.are` [C]: the template `pre200cr_hurlock_archer` has `AI_FLAG_STATIONARY` 0, but placed archers often carry 1 (the Korcari Wilds instance; two of five `hurlock_lbow_axe` on Ishal's first level). A stationary archer is never moved by the help and shoots only within `GetItemRange` (45 m) and its line of sight.
    - **Not explained**: a staged archer at 37.8 m walking to 33.7 m and not shooting in 15 s. Candidates [I]: the help move from an ally that perceived first; or an attack command whose approach cuts its stop distance by 1 m per update without sight (`dev/TURN_BASED.md`:184).
- **An arrow's flight, and where BLOCKED comes from** (read 2026-10-22 by lib-agent-2 with subagents, for DAOParagonEngine #163). It builds on `EFFECTS.md`, "5. A projectile at impact".
  - **The path** [C]:
    - **Launch:** a plain attack keeps `Crust_ID` 0 (the maker 0x006B6F60, 0x006B6FAB), so the arrow is held at `Prop01`, the right hand, the hand that draws the string; the bow (`WieldR` 15) is in the left. The release (0x007E80E0) reads that node's world position.
    - **Aim:** the destination is re-read every update (0x007E7120). With a target it is the target's crust hook `+0x4A8` (0x007E6F00; that hook is absent, so the projectile sets `+0x4A8` = 1 at 0x007E6F4A), looked up in the target's crust map (slot 0x7C, 0x00547D20). On a humanoid, `CrustHook1` hangs under `Chest1` at (0.068, 0, 0) (`hm_arm_nuda_0.mmh`): the upper chest. Only without a graphics object or hook does it use the target's position `+0x120`, the feet.
    - **Arc:** `prj_base` rows 1 (arrow) and 3 (bolt) have `Simulation` 0, `InitialSpeed`, `MinSpeed` and `TopSpeed` 60, `Acceleration` 0, `StaticCollisions` 1, `DynamicCollisions` 0 and `ImpactTimeout` 3. A `Simulation` 0 row flies straight at 60 m/s toward the re-read destination, so it homes. The gravity (0, 0, −9.81) at 0x00C11840 is only for `Simulation` 1 rows (ballista, catapult, traps).
    - **Steps** [C]: each update (0x007E9170) re-reads the aim (0x007E7120, at 0x007E919D), moves toward it by up to the speed × dt, and sweeps from the old position to the new (0x007E946F): a homing polyline, one ray per update, not one chord. The arrow is made at the clip's `ProjectileLoad` event (9) and released at `ProjectileFire` (10, 0x006AFA40) of the fire clip (`EFFECTS.md`, the cast's events; `ANIMATION.md`, ranged shot), not at the attack's start; the launch reads `Prop01`'s world position at that moment, which is the posed bone [I]. Not read: `bw_fire`'s event times and the heights of `Prop01` at release and of `CrustHook1`.
  - **The sweep** (0x007E8C10, called at 0x007E946F with the step's old and new position) [C]:
    - **The static test** runs when `StaticCollisions` is set and the distance flown (`+0x468`) is at least the caster's `PerSpace` + 1.2 m (0x007E6A10: `CCCreature` slot 39, 0x006A21A0, `[[creature+0x7B0]+0x60]`, apr_base `PerSpace`; the double 1.2 at 0xAFEBC0; a `Simulation` row never skips). So it is a distance along the path, not a height: Human 0.35 → 1.55 m, Hurlock 0.40 → 1.60 m, Genlock 0.30 → 1.50 m. The flight step adds the step's length to `+0x468` after the sweep (0x007E950C..0x007E9516), so a step is swept only when the distance flown before it reaches that; at 60 m/s and 30–60 frames a second the first swept step starts 1.55 to about 3.5 m from the launch [I for the frame steps]. It is a closest-shape ray (0x00648C60), no radius, with mask **0x1024** under the AND filter (`COLLISION.md` §1). A shape is hit when `bits0 & 0x1024`: Placeables 0x4, StaticGeometry 0x20, or 0x1000.
    - **The classifier** (0x00648B70) splits the hit by the shape's bits0. `& 3`, 0x20000 or 0x4, with an actor id other than −1, is an object (kind 2, the actor's user-data id). 0x20 or 0x8 is static (kind 1). Under 0x1024 only a Placeables shape can be an object.
    - **The dynamic test** runs only with `DynamicCollisions` (not for arrows or bolts). It uses mask 0x20002, three rays ±`+0x470`, and new objects other than the caster and target. Its hits only raise the impact event with their ids (0x007E89F0); they never end the flight or set result 5.
  - **What a hit does** (0x007E9474..0x007E94AB) [C]:
    - On a static or object hit the caller stores the out-id in the target `+0x4C0` and sets the attack result (`+0x488`, 0x007E68A0) to **5 unless it is 3**, then puts the projectile at the hit point. So a roll of MISS stays a miss, and only a would-be hit becomes BLOCKED. This is the only writer of result 5 (0x007E7BF1, 0x007E821E and 0x00679B63 only read it).
    - An object hit is taken only when its id is neither the target's nor the caster's (0x007E8CF9, 0x007E8D01).
    - **A defect:** a static hit (a wall) returns without writing the out-id (0x007E8CC5..0x007E8CEA, after the surface's sound, 0x007E67F0). The caller then stores `[esp+0x14]`, which at that point holds the float bits of the step's length (0x007E941F..0x007E942C), as the new target. The result is still 5.
  - **No facing or stance test** [C]: none in the flight, and `Combat_GetAttackResult` (`combat_h.nss`:361) never returns BLOCKED.
  - **In the scripts** [C]: `ATTACK_IMPACT` (`rules_core.nss`:509) goes to `Combat_HandleAttackImpact` (`combat_h.nss`:1062–1079), which turns 5 into MISS and shows `UI_MESSAGE_BLOCKED`, the floating "Blocked". There is no damage, sound, VFX or animation of its own; the later BLOCKED branch (:1272) is empty. The engine skips the on-hit events for results 0, 3 and 5 (0x007E87C0).
  - **The arrow sticks** [C]: at arrival (0x007E90B0) the new target exists and `ImpactTimeout` is 3, so the arrow is made a child of the hit object's model at the hit point (0x007E79D0) and plays its material sound. It stays 3 s, then fades (`AlphaFade`).
  - **What a shield bearer's arrows hit: walls near the archer, not the shield** (settled 2026-10-22 with a watch on the sweep, DAOLib #229; DAOParagonEngine, Ishal floor 1; probe.log `projectile sweep hit`).
    - **The hero as the target, no shield:** two archers made 13 BLOCKED impacts, and all 17 blocked sweep steps were walls (kind 1, no id) at z 0.9–1.6, about 1–2 m past the archer.
    - **Alistair as the target, with the Templar kite shield equipped** (`gen_im_arm_shd_kit_tmp`), out in the open: five archers made 30 impacts in 25 s, HIT 16, MISS 14 and BLOCKED 0. All 20 blocked sweep steps were walls and none an object; the 3 on Alistair were rolls already missed, at walls near the archers.
    - So a shield is never struck. The earlier run's 12 BLOCKED of 28 impacts on shield bearers, against 4 of 50 with the shields unequipped, came from walls near where the archers stood [I: the positions of that run were not logged].
    - Nothing read makes a shield hittable [C]:
      - every vanilla shield `.phy` (`w_shd_kita_0`, `w_shd_srda_0`, the others of the same 2,026 bytes) is a lone GOB with no shapes;
      - `w_shd_kita_0.mmh` has a bbox, one mesh and a `CrustHook1`, with no collision node;
      - a humanoid's body capsules are Creatures | Nonwalkable (0x2 | 0x01000000), and its controller is groupsMask 0x10, neither within 0x1024.

**The swing** (`CSSubActionHandler::Swing`, `0x007A87C0`, read):

1. It fails with −7 (`COMMAND_FAILED_TARGET_DESTROYED`) if the target (the command's object 0) is gone or dead.
2. It waits while the target is out of striking reach, more than 60° of elevation away, or out of line of sight (`Combat::LineOfSight`).
3. For an ordinary attack (the command's int 2 is 0), it calls `CCCreature::SignalAttackPending(target)` (read below). For a deathblow, the command's int 2 is the result and 3 is used for the other hand.
4. It reads the results the scripts left (`0x006A40D0`): the main hand's and the off hand's records in the attacker's combat record (`0x006A0160`, `0x006A0170`). It goes on only if at least one is set (not 0) and neither is 4 (deathblow; the scripts then queue the deathblow command themselves). A critical hit (2) counts as a hit (1) here.
5. It picks the swing's animation from `anim_combat` (table 104) by the two hands' results (`0x0069F530`), so a miss swings differently from a hit. It plays it through `0x006A41A0` and keeps its length in milliseconds at the sub-action's +0x14. The length is the scripts' `SetAttackDuration` for a humanoid with a weapon, else the clip's own less a fifth (`src/dao/Animation.cpp`, seen in play).
6. It waits that time (`0x007A1AE0`), then succeeds. With no results set, it fails with −1 (`COMMAND_FAILED`).

**`EVENT_TYPE_COMMAND_PENDING` (35)**, from `SignalAttackPending`, read:
- Run at once on the attacker's script (`CCObject::RunEventScript`), not queued.
- Creator: the attacker. Int 0: 1 (`COMMAND_TYPE_ATTACK`). Int 1: 1 (the sub-type). Object 0: the attacker. Object 1: the target.

**The engine does not resolve the blow; the scripts do, inside COMMAND_PENDING.** `rules_core` calls `Combat_HandleCommandAttack`, which:
- rolls each hand (`Combat_PerformAttack`: hit, crit, miss, deathblow, backstab);
- builds the damage effect;
- hands the results over with `SetAttackResult(attacker, result1, effect1, result2, effect2)` (691, `0x0065DE40`, read). For each hand, this stores the result, the target (the current command's object 0) and, unless the result is 3 (miss), the effect's id (+0x20), in that hand's record. A copy of the effect is kept for later (`0x007C3840`);
- sets the swing's time with `SetAttackDuration` (353), and the aim's with `SetAimLoopDuration`;
- answers with `SetCommandResult(owner, COMMAND_RESULT_SUCCESS)` (716, `0x0065C0B0`), which stores the answer at the object's +0x3D4. Inferred: the command layer reads it there; it isn't read in `Swing` itself.

For a deathblow, `SetAttackResult` is called and then the deathblow command is put at the front of the queue (`CommandDeathBlow`), which clears the attack.

Result constants (`script.ldf`): `COMBAT_RESULT_INVALID` 0, `HIT` 1, `CRITICALHIT` 2, `MISS` 3, `DEATHBLOW` 4, `BLOCKED` 5, `BACKSTAB` 9.

**The hit roll** (read 2026-10-21 by lib-agent-2, for DAOParagonEngine #144; C checked, I inferred):
- **The engine rolls nothing** [C, `combat_h.nss` `Combat_GetAttackResult`]: the scripts roll once per swing, `RandomFloat()·100 < fAttack − defense`.
  - fAttack = attack rating + the weapon's ITEM_STAT_ATTACK + fBonus + `ATTACK_HIT_BIAS` + `Diff_GetRulesAttackBonus` + 50 + flanking − (distance − max(POINT_BLANK_RANGE, the weapon's optimum range), at least 0).
  - Defense = the defense property + `Diff_GetRulesDefenseBonus`.
- **The difficulty terms apply only to CREATURE_RANK_PLAYER** [C, `core_difficulty_h.nss`:54–75], from the difficulty table's `AttackBonus` and `DefenseBonus` by `GetGameDifficulty()`. Enemies get none.
- **At impact** [C, `combat_h.nss`:1086–1115]: a hit becomes a miss on an invisible target, or for a melee attacker with PerSpace < 1 more than 5 m away.
- **The rage demon against the Harrowing mage** [I: the demon's attributes inferred from the autoscale order; the mage's DEX and the difficulty were not read]: attack about 58 (MONSTER_SPIRIT, BaseAttack 55), mage defense about 45–47 on Normal. That predicts 63–67 % before flanking. Vanilla's 59 % (n = 41) is within noise of that.

**The impact comes from the animation, not a fixed fraction** (`src/dao/Addresses.h`, `kCCCreature_OnAnimationEvent`, read and seen in play with the probe, DAOLib #88):
- The swing's clip carries animation events (`events.gda`'s numbers). Event 1 is an attack's impact.
- The creature's slot 46 gets it from the animation manager. `0x006A6230` takes the off hand's record if the event's name contains "Left", otherwise the main hand's, and calls `0x006A6080`.
- An object out of the scene (shown only within 60 m of the camera, `kCreatureShowDistance`) raises no animation events, so far from the camera no blow lands.

**`EVENT_TYPE_ATTACK_IMPACT` (29)**, from `0x006A6080`, read:
- Queued (`AIMaster::AddEventDeltaTime`, no delay) to the attacker's script, not the target's.
- Creator: the attacker. The event's target field: the target.
- Object 0: the attacker. Object 1: the target. Int 0: that hand's result. Int 1: the damage effect's id.
- After queueing it, unless the result is 0 or 3 (miss), the engine raises the weapons' on-hit events (`0x00758180`, `EVENT_TYPE_ITEM_ONHIT`) for the attacker's and the target's equipped items.
- A ranged attack's impact is made by the projectile (`0x007E87C0`), also 29, with the same int 0 and 1 and more fields:
  - int 2 (the projectile's +0x4B0);
  - vector 0: the projectile's position; vector 1: its heading (inferred);
  - object 0: the shooter; object 1 and up: the targets hit.
  It's queued to the object given, and its on-hit events are skipped for results 0, 3 and 5.

In `rules_core`'s ATTACK_IMPACT:
- `GetAttackImpactDamageEffect(attacker, id)` (695, `0x0065E180`) fetches the kept effect.
- `Events_FilterAttackImpactEvent` drops stale impacts (a dead attacker, and so on).
- `SendEventOnAttacked` tells the target.
- `Combat_HandleAttackImpact` applies the damage. A blocked blow, an invisible target, or a melee target more than 5 m away (for attackers with `PerSpace` < 1) becomes a miss. Otherwise it calls `Effects_ApplyInstantEffectDamage`: an instant `EffectDamage` on the target.

**Damage, `EVENT_TYPE_DAMAGED` (2) and `EVENT_TYPE_DEATH` (4) are the scripts', not the engine's** (read in the scripts; no engine site creates either for combat):
- The damage effect's script (`effect_damage_h.nss`) works out the new health and, for a blow that isn't fatal, calls `SetCurrentHealth` ("the only single point in game that modifies health"). It then sends DAMAGED to the target with `SignalEvent`: creator the damager, float 0 the damage, int 0 the damage type, int 1 the ability.
- A fatal blow calls `KillCreature` (`effect_death_h.nss`), which applies the death effect. Its script sends DEATH (`SendEventOnDeath`), with creator the killer.
- The engine's own DEATH is raised only for a creature already dead when it enters the module (`0x0069DA70`).

**`EVENT_TYPE_COMMAND_COMPLETE` (36)** (`0x00743DD0`, read):
- Creator: the creature. Object 0: the creature. Object 1: the command's target (its object 0).
- Int 0: the command type (1 for an attack). Int 1: the status. For a use-ability command (type 25), int 2 is the ability. For status −9, object 2 is the path's blocker.
- Run at once when the command's +0x4C is 1, else queued.
- The status is `COMMAND_SUCCESSFUL` 1 after the swing, or a failure (`script.ldf`): −1 failed, −2 cleared (a deathblow clears it), −5 no space in the melee ring, −6 no line of sight, −7 target destroyed, −10 timeout (the approach), and so on.

So the order for one melee swing:
1. The approach.
2. The turn to face.
3. `COMMAND_PENDING`, run at once: the scripts roll the blow, `SetAttackResult`, `SetAttackDuration` and `SetCommandResult`.
4. The swing's animation.
5. At its impact event, `ATTACK_IMPACT`, queued to the attacker: the scripts apply the damage effect, whose script sets health and sends `DAMAGED`, or kills (`DEATH`).
6. When the swing's time is up, `COMMAND_COMPLETE` (status 1).
7. The AI queues the next command.

### A shot's projectile (read 2026-10-21 by lib-agent-2, for DAOParagonEngine; C checked, I inferred)

- **The row comes from the ammunition slot, not the weapon** [C, 0x006B6FF1..0x006B709B]. For an attack (command 1), or an ability whose `abi_base` `Flags` bit 0 is set (the archers' shots, 0x006B71B8):
  1. The item in RANGEDAMMO (slot 2, `0x007F4EA0(2)` on the creature's equipment) gives the row: `bitm_base` `Projectile` (0x015032BB) of **the ammo item's base item**.
  2. The main-hand item's (slot 0) local int `PROJECTILE_OVERRIDE` (well-known variable id 14, read by `0x00702E90` on item +0x2E0) replaces it when non-zero. It is a local variable, not an item property. It is read only when there is an ammo item.
  3. The main hand's own `bitm_base` `Projectile` is never read.
  4. No ammo item, or a row of 0, makes no projectile (0x006B70A4 → 0x006B731A).
- **The engine's made ammunition** (`0x006A0750(ammo base item, launcher, set)`) [C]: a new `CCItem` of the launcher's `bitm_base` `Ammo` base item, with its bool attribute at item +0x5D0 set (slot 21 of the attribute, called with 1 at 0x006A07EE). It is then equipped into RANGEDAMMO of that weapon set (`0x006BC7F0(item, 2, set)`). Nothing is made while bit 0x200 of the global flags 0xC371B4 is set; only the equipment's clear-all 0x007977D0 sets that bit (with 0x2).
- **+0x5D0 marks it transient** [C]. It is the flag the item notes call "being destroyed" (ITEMS.md:144, SAVE_WRITER.md:294):
  - the item raises no ADDED or REMOVED event;
  - the save writer skips it, so a save never holds made ammo;
  - a shot never uses it up.
- **Where it is made** [C unless marked]:
  - **An item added to the equipment** (`Equipment` vtable slot 21, 0x00797D10): when the added item's `bitm_base` `Ammo` is non-zero, and that set's RANGEDAMMO is empty or holds another base item, the ammo is made for that set. Every add goes through here, equipping included. Load and template spawn add the same way [I: their adds were not traced]. Since made ammo is never saved, a load makes it again [I].
  - **A real ammo item taken off** (the unequip 0x006B7690, item Type 7): the item goes to the backpack, and ammo is made from the main hand's `Ammo` (0x006B77F8..0x006B781A).
  - **After each weapon shot** (`CCCreature` slot 61, 0x006A6740), at the clip's "Projectile Fire" event (0x006AFA40 → 0x006985A0, only when the projectile's +0x4BA is set, the weapon path):
    - a real ammo item with a `Projectile` loses one (`0x00762100`, count 1) and is deleted when its stack runs out; made ammo is skipped;
    - then, if RANGEDAMMO is empty and the main hand's `Ammo` is non-zero, ammo is made.
  - Not before a shot: the projectile maker makes none, and with no ammo it makes no projectile.
- **Where it goes** [C unless marked]:
  - **Taken off directly**: the unequip 0x006B7690 tests +0x5D0 first. Made ammo goes to `0x006A5CD0(set)`, which removes it from that set's RANGEDAMMO and destroys it (vtable +0x198 with 0). It never reaches the backpack.
  - **The launcher taken off**: `Equipment` slot 22 (0x00797E20, an item removed) destroys that set's made ammo through `0x006A5CD0` when the removed item's `Ammo` is non-zero. Equipping a melee weapon over the bow therefore destroys it.
  - **Other ammo equipped**: the old item is taken off first, so made ammo is destroyed [I: the equip's own unequip not traced].
  - **A weapon-set switch**: nothing is added or removed, so each set keeps its own made ammo [I: the switch not read].
- **Also taken from the items** [C]:
  - +0x4AC, the effect attached to the projectile: the ammo item's local int `ITEM_CRUST` (variable id 10).
  - +0x4B0: the ammo item's base item.
  - +0x4F0: the main hand's `bitm_base` `ProjectileOffset` (0xFA66B18D, f32), for an attack only; the ability path jumps past it, so 0.
  - +0x4BA = 1 on the weapon path, 0 for an ability's own projectile.
- **An ability without `Flags` bit 0** takes `abi_base` `Projectile` and `Crust_ID` (+0x4B4); with no projectile it goes straight to its impact (ABILITIES.md:91).
- **The scripts read the same override** [C]: `combat_h.nss`:827 takes `prj_base` `DamageType` of the weapon's `PROJECTILE_OVERRIDE` row for a ranged attack, and `core_h.nss`:2807 a staff's `DamageBonus` from it. Neither column is read by the exe (EFFECTS.md:476).

## Effects

Read 2026-10-18 in the GOG exe and the toolset's scripts by lib-agent-2. "Read" means seen in the code; "inferred" means not followed to the end. DAOLib already has an effect's ageing (`Addresses.h`, `kEffectsHolder_AgeEffect` and kin; `dao/Effects.h`, `dao/Clock.h`) and the CDataArrays parameters an effect and an event share.

What an effect makes a creature play (the knockdown's fall, loop and get-up, stun, sleep, paralysis's pause) is in `dev/ANIMATION.md`, "Effects' animations".

### Applying an effect

- `Engine_ApplyEffectOnObject` (command 597, `0x0065ca20`) pops the duration type (int), the effect (engine structure 3), the target (object), the duration (float), the creator (object) and, with more than five arguments, an ability id (int). Read.
- Only creatures, placeables, items and areas of effect take effects: object types 4, 6, 10, 12 by the object's type byte at +0x14. Any other target, or none, deletes the effect and returns. Read.
- It then calls `CGameEffect::Prepare` (DAOLib's `kCGameEffect_Prepare`, ID 13143) with the duration type, duration, creator and ability. For effect type 7 (`EFFECT_TYPE_DEATH`) on a creature it first calls the target's slot 0x16C(0) when its slot 0x168 says so. Then `0x007a1a20` hands the effect to the effects manager (client app +0x1AC, its +0x68). Read.
- `0x007a1920` → `0x007a0e30` applies it. Effect types 1–11 (the engine's own: `EFFECT_TYPE_VISUAL_EFFECT` 4, `AOE` 5, `CAMERA_SHAKE` 6, `DEATH` 7, `PARALYZE` 8, `MOVEMENT_RATE` 9, `ALPHA` 10, `MOVEMENT_RATE_DEBUFF` 11) go to a native handler from a table on the manager, indexed by type, when the target's slot 0x6C allows. Their answer is the result, and no APPLY_EFFECT event is raised on that path. Read; what each native handler does is not read.
- Every other type (the scripted ones, 1001 and up, `EFFECT_TYPE_DAMAGE` 1013 among them) goes to `0x0079fe90`. While the target's effects holder is enabled (its byte +4), that builds a `CScriptEvent` of type 33 (`EVENT_TYPE_APPLY_EFFECT`), puts the effect in the event as its effect 0 (`0x0069d120(0, effect)`), and passes it to the target's slot 0x28. That slot is `CCObject::RunEventScript`, so the target's own event script runs it at once: `creature_core`, `placeable_core` and so on. It isn't queued, and it isn't a per-effect-type script. Read.
- In the scripts, `creature_core` hands unhandled events to `rules_core`. Its `EVENT_TYPE_APPLY_EFFECT` case calls `Effects_HandleApplyEffect` (`effects_h.nss`), which reads `GetCurrentEffect()`, switches on `GetEffectType` to the `effect_*_h` handlers, and ends with `SetIsCurrentEffectValid(nReturnValue)`. A dead target refuses everything except resurrection and death.
- After the script, `0x007a0e30` checks the effect's invalid word (+0x48). If the script marked it invalid, the apply returns 3 and the effect is not added: `0x007a1920` sets +0x48 to 1. If still valid, the holder's slot 0x1C adds it to the target's list, and slots 0x70 and 0x34 follow. So the script runs first and the list changes after, only for a valid effect. Read in outline; the slots' bodies are not read.
- `0x0079fc10`, after the event: when the effect's engine int 2 (+0x54's arrays, `EFFECT_INTEGER_VFX`, set by `SetEffectEngineInteger` or the visual-effect commands; `0x0079FC3D`–`0x0079FC44`) is above 0 and the effect is still valid (+0x48 = 0), it plays that visual effect. Read. A damage effect's `nImpactVfx` is its script int 2 (`EffectDamage`, `0x00662950`–`0x00662974`), which leaves the engine int unset and is played by the scripts (below), not here (corrected 2026-10-18 by lib-agent-2).
- **An instant effect with an animation becomes temporary** (read 2026-10-21 by lib-agent-2, for DAOParagonEngine #144; C checked, I inferred):
  - **What happens to it** [C, the apply 0x007A0E30 at 0x007A11DC..0x007A1225]: after the target's APPLY_EFFECT script has run, an effect with priority > 0 (0x00A65380; knockdown 981) and duration type 1 (INSTANT) is changed:
    - its type becomes 2 (TEMPORARY);
    - its duration (+0x2C) becomes the length of its animation in the target's own tree (`0x00A5F7A0`, in ms). The script's duration is ignored.
  - **One already held** [C]: if the target already holds an effect of the same type (`0x0079F6C0`), the new one is marked invalid instead.
  - **So an INSTANT knockdown lasts the length of row 148's clip in the target's tree** [C]: male human 1.367 s, wolf 0.933 s, spider 0.767 s. Which tree gives Eluvian's 1.166 s was not read [I: another race or gender].
  - **effects.gda's `AllowInstant`, `AllowTemp` and `AllowPermanent` are read by nothing** [C]: their hashes (0x174ACBDD, 0x8FCACB84, 0x3920511E) are not in the exe. `effects_h.nss`'s `_VerifyDurationType` (:126–163) is never called, and asks for "AllowTemporary", which is not a column.

### The current effect

- `GetCurrentEffect` (681, `0x006604d0`) and `SetIsCurrentEffectValid` (682, `0x00660580`) use the event the calling object is running: an object's +0x2A8 through its slot 0x18, an area's +0x3C4, or the module's +0x390 (`GetCurrentEffect`, `0x006604D0`, tests the type byte +0x14: an area, type 4, reads +0x3C4 at `0x0066051F`, the module, type 3, +0x390 at `0x00660538`; corrected 2026-10-18 by lib-agent-2, the two were swapped). If that event is type 33 or 34, its effect 0 is the current effect. Otherwise `GetCurrentEffect` returns no effect and `SetIsCurrentEffectValid` does nothing. Read.
- `SetIsCurrentEffectValid(nValid)` writes `!nValid` into the effect's invalid word, +0x48 (`0x00424030`). Read.

### Removing an effect: `EVENT_TYPE_REMOVE_EFFECT` (34)

- `0x007a0840` builds a type-34 event with the effect as effect 0 and runs it at once through the same slot 0x28, on the target's own script, while the holder is enabled. Then `0x007a0150` takes the effect out. Read. The scripts' `rules_core` calls `Effects_HandleRemoveEffect`.
- It's reached through `0x007a08e0` from:
  - expiry: `SEffectsHolder::AgeEffect` (`0x007a0b10`) → `0x007a0a90`, which first sets the invalid word;
  - removal by match: `0x007a0c50`, the holders' walk that `RemoveEffect` (598, `0x00674590` → `ForScript_RemoveEffect` `0x006741e0` → `0x00670a80`) and its kin drive;
  - a holder slot at `0x007a1410`, which calls `0x007a0840` directly. It's probably the holder emptying itself; inferred.
- So REMOVE_EFFECT is sent on `RemoveEffect` and on expiry. Read. Whether the engine itself removes effects at death is not read; the scripts' death handling removes what it chooses.

### Durations

- What DAOLib already read (`Addresses.h`, `dao/Effects.h`): each object's update ages its effects (`kEffectsHolder_Update` → `AgeEffect`). For a temporary effect (+0x28 = 2) that has started (+0x50), `AgeEffect` asks the owner (slot 0x2C) for this update's milliseconds and calls `CGameEffect::AddElapsed`, which adds them to +0x30, capped at the duration in +0x2C. When nothing is left, `0x007a0a90` ends the effect, and REMOVE_EFFECT follows.
- The granularity is the owner's update, in milliseconds. The time is the owner's own clock, the same one regeneration uses, which `dao/Clock.h` holds and advances.
- Duration types are `EFFECT_DURATION_TYPE_INSTANT` 1, `TEMPORARY` 2, `PERMANENT` 3 and `DEATH` 6. An effect whose +0x24 is 2 is removed by `AgeEffect` straight away, or by `0x007a00a0` in some game states (modes 4 and 5 via `0x005d5e70`). Inferred: that's how an instant effect goes once applied.

### Damage and death

- Damage is the scripts' job. `EFFECT_TYPE_DAMAGE` (1013) isn't in the engine's native range, so APPLY_EFFECT runs `Effects_HandleApplyEffectDamage` (`effect_damage_h.nss`). That reads the effect's float 0 (damage), int 0 (damage type), int 1 (flags) and int 2 (impact VFX), and calls `Effects_ApplyInstantEffectDamage`, which:
  - works out shields, wards and resistances;
  - sets health with `SetCurrentHealth` (`core_h`, through the health property);
  - calls `SignalDamage(oDamager, oTarget)`, command 359 (`0x00662310`);
  - sends `EVENT_TYPE_DAMAGED` itself with `SendEventOnDamaged` (`events_h`: float 0 damage, int 0 damage type, int 1 ability, creator the attacker, through `SignalEvent`).
  Read in the scripts.
- Death at 0 health is decided by the same script: when the hit is fatal and the target isn't plot or immortal, `KillCreature` (`effect_death_h`) builds `EffectDeath()` (type 7; int 0 set to 1 whatever its argument, `effect_death_h.nss`:30, the damage in int 4, `ACH_EFFECT_DEATH_DAMAGE_INDEX`, by `KillCreature`; creator the killer) and applies it with `EFFECT_DURATION_TYPE_DEATH`, or handles it inline.
- Type 7 is one of the engine's native types, so the engine's handler takes it (not read). The script side, `Effects_HandleApplyEffectDeath`, sends `EVENT_TYPE_DEATH` with `SendEventOnDeath(target, killer)`. Read in the scripts; how the native type-7 handler and the script handler share the work: "The death effect (type 7)" below.

### The death effect (type 7)

Read 2026-10-18 in the GOG exe and the toolset's scripts by lib-agent-2 (a subagent), for paragon-agent-2. "Read" means seen in the code; "inferred" is marked.

**The native handler tables.** `0x007a1840` (the effects manager's setup; the manager is `*(client app+0x1AC)+0x68`) fills two tables of 12 entries: `[manager+0]` for applying and `[manager+4]` for removing, indexed by effect type. Read:

| Type | Name (script.ldf) | Apply | Remove |
|---|---|---|---|
| 1 | | `0x00a21f90` (returns 0) | `0x007421d0` |
| 2 | | `0x007a01a0` | `0x007a0200` |
| 3 | | none | none |
| 4 | `EFFECT_TYPE_VISUAL_EFFECT` | `0x007a00a0` | `0x007a0150` |
| 5 | `AOE` | `0x007a0230` | `0x007a0520` |
| 6 | `CAMERA_SHAKE` | `0x0079f300` | `0x007421d0` |
| 7 | `DEATH` | `0x007a12e0` | `0x007a1410` |
| 8 | `PARALYZE` | `0x007a05b0` | `0x007a05f0` |
| 9 | `MOVEMENT_RATE` | `0x007a0610` | `0x007a0690` |
| 10 | `ALPHA` | `0x007a06f0` | `0x0079fa30` |
| 11 | `MOVEMENT_RATE_DEBUFF` | `0x007a0610` | `0x007a0690` |

The apply path (`0x007a0e30`) takes the native handler only when the type is 1–11 and its entry isn't null, and the target's slot 0x6C allows it; every other effect, a type-3 one included, goes to `0x0079fe90`, which raises `EVENT_TYPE_APPLY_EFFECT` (33) on the target's script. Read.

**The type-7 apply handler raises APPLY_EFFECT itself** (`0x007a12e0`, read):

1. It sets the effect's duration type (+0x28) to 6 (`EFFECT_DURATION_TYPE_DEATH`) and +0x38 to 999.
2. It sets +0x34 to `0x005ca740(effect int 0, effect int 1 == 0, 0)`: the death animation, the `React` column of `UnsyncDeathAnimations` (int 1 0) or `SyncDeathAnimations` at row int 0, 250 where there is none (read, corrected 2026-10-18). When int 1 is 0, it sets int 3 to 1.
3. It calls `0x0079fe90`: APPLY_EFFECT (33), run at once on the target's own script. For a creature, `creature_core` → `rules_core` → `Effects_HandleApplyEffect` → `Effects_HandleApplyEffectDeath`. That is how the scripts' death handler runs.
4. If the script left the effect valid (+0x48 is 0), it removes effects by three matches (`ForScript_RemoveEffect` with matcher `0x00695400` and 5, then `0x00670a80` with matchers `0x00651810`/4 and `0x006953e0`/0, and a holder walk `0x0079f5d0` with `0x007a0c50`), then calls the holder's slot 0x1C (`0x006977c0`), which queues command 32, `ExecuteEffect` (`0x00695f20`), for the dying creature (corrected 2026-10-18: not the list's add; where the effect is put in the list is not read). It returns 0. If invalid, it returns 1 and nothing is added.

The type-7 remove handler (`0x007a1410`) raises REMOVE_EFFECT (34) (`0x007a0840`), then the same removals, then walks the holder re-applying through `0x007a0e30` (inferred: effects held back while dead are put back on resurrection).

**What the scripts do in APPLY_EFFECT** (`effect_death_h.nss`, read):
- For a placeable: send DEATH (`SendEventOnDeath`), then `SetPlaceableHealth(0)`.
- For a creature:
  - nothing if it already has a death effect;
  - for a ghost: DYING, a visual effect, then DEATH;
  - otherwise: set `CREATURE_RULES_FLAG_DYING`, `Effects_RemoveAllEffects`, achievements and stats, mana and stamina to 0, the slayer's mana back, then `SendEventDying` (EVENT_TYPE_DYING, the scripts' 1023, object 0 the killer, by `SignalEvent`, so queued). It does not send DEATH for an ordinary creature.
  - `creature_core`'s DYING gives the party XP and treasure and plays the dying sound.

**Health and IsDead.** `IsDead` is the creature's slot 0x1D0 (`0x006a3660`): true when the creature has no stats object, or when its current health (slot 0x120) is 0 or less. There's no separate flag (read; the scripts say so too). `effect_damage_h` does not set health on a fatal blow; it calls `KillCreature`, "the health will be lowered when the target dies (applies the death effect)". So the health drop to 0 comes when the death effect is added to the list (inferred: through the effect's property change; not read).

**DEATH (4) for an ordinary creature comes from the engine, after the death animation** (`0x007aac10`, a sub-action handler, read):
- For an effect whose duration type is 6, it plays animation 3 (the death) on the creature (slot 0xB0, blended).
- It waits on the sub-action's timer (`0x007a1ae0`). When that runs out, it builds a CScriptEvent of type 4 with creator the effect's creator (the killer) and runs it at once on the creature (`CCObject::RunEventScript`).
- For a party member it then takes it out of the party's selection (`0x00639db0`, ID 12477: the selection vector +0x184 and a second vector, `vector::remove` `0x004E9640` at `0x00639E13`; corrected 2026-10-18 by lib-agent-2, not `SetPartyPlotFlag`'s), sends a death telemetry record, and calls `0x0069edc0` unless `0x006a3a80` answers 2.
- `creature_core`'s DEATH then clears DYING and the perception list, and so on.
- The sub-action (51) is the step of that `ExecuteEffect` command (read 2026-10-18).

**The call before Prepare** in `ApplyEffectOnObject` (`0x0065ca20`), for type 7 on a creature: slot 0x168 (`0x006a3750`, `IsImmortal`'s) answers true when the creature's +0x2C0 is set or its current command is type 38 (`COMMAND_TYPE_DEATHBLOW`). Slot 0x16C(0) (`0x006ad130`, `Engine_SetImmortal`'s) then writes +0x2C0 = 0 and sets a creature property (index 7 of the +0x7C4 table) to 0.0, or to 1.0 for a non-zero argument. Read. +0x2C0 is the immortal flag, not a deathblow state: `IsImmortal` (219) and `Engine_SetImmortal` (220) read and write it, the template's `Immortal` is loaded into it, and the save keeps it as field 16220 `SavegameObjectImmortal` (`dev/script_commands/objects.md`; corrected 2026-10-18 by lib-agent-2). So the death effect makes an immortal creature mortal before it dies.

**So, for an ordinary creature killed in combat:**
1. `effect_damage_h`: fatal → `KillCreature` → `ApplyEffectOnObject(EFFECT_DURATION_TYPE_DEATH, EffectDeath())`.
2. The engine: clear the immortal flag (slots 0x168/0x16C), `Prepare`, type-7 handler.
3. APPLY_EFFECT on the creature's script at once: DYING flag, effects removed, mana 0, DYING queued.
4. The engine: effects removed, the death effect added; health goes to 0 (inferred).
5. The death animation; at its end, DEATH (4) run at once on the creature, creator the killer.
6. `creature_core` DEATH.

## Death, the body bag and a creature's load: read 2026-10-18

Read in the GOG exe by lib-agent-2, for DAOParagonEngine (the Harrowing's fights), with subagents. Not seen in play. The key claims below were checked in the disassembly.

### Follower state

- **Where it is kept.** A creature's follower state is not a field of the creature. It is a map keyed by object id in the script backend (`[[0x00C327EC]+0xD0]+0x58`, the map at +0x2A8).
- **Reading it.** `GetFollowerState` (0x0065E670 → 0x00632810) answers the stored value, or 0 (`FOLLOWER_STATE_INVALID`) for an id not in the map.
- **The constants** (`script.ldf`): INVALID 0, ACTIVE 1, AVAILABLE 2, UNAVAILABLE 3, SUSPENDED 5, LOADING 6, LOCKEDACTIVE 7.
- **Setting it.** The setter is 0x0063B9A0 (the address library's `ScriptBackend::ForScript_ADDTOPARTY` is a wrong name). It switches on the state:
  - 0 removes the id.
  - 1 and 7 insert or overwrite it.
  - 2, 3 and 5 are set only if 0x006377E0 passes.
  - 4 and 6 do nothing.
- **Who calls the setter:**
  - `SetFollowerState` (0x0065E6E0).
  - `AddToParty` (ACTIVE) and `RemoveFromParty` (INVALID).
  - Hero creation (below).
  - A save's load (`CCCreature` slot 34, 0x006BE980, the creature's field 16203).
  - `CCCreature` slot 86 (0x006A8AC0), which sets ACTIVE again for a creature with the PC flag (+0x7D4) that is not a party member.
- **The hero is ACTIVE from its creation, set by the engine.** 0x0069DF10, which the character creation screen calls to make the hero, tags the creature `player_core` when the party is empty and sets its state to 1 (0x0069E008–0x0069E015, checked). No script sets the hero's state at character creation or at the module's start. The scripts that name the hero set UNAVAILABLE and back to ACTIVE mid-game (`arl300pt_fade`, `denpt_main`, `denpt_captured`, `cli410ar_city_gates_defend`, `cli800ar_palace_district`).
- **Companions read INVALID** until `AddToParty` or a hiring script runs (`utility_h` 851/882/888, `party_h` 296–348, through `WR_SetFollowerState`). `RewardXP` (`sys_rewards_h`:107), `sys_autoscale_h`:532 and `ai_main_h_2`:3204 skip INVALID.

### The death command, its animation and the command queue

**The death is a command** (corrects "The type-7 apply handler" above, its step 4):
- The type-7 handler's last call, the effect holder's slot 0x1C (0x006977C0, from 0x007A13EC), adds no effect to a list.
- It reads the effect's animation priority (0x00A65380: 999 for duration type 6, else +0x38).
- If that is above 0 and the creature has no command 32 current or queued (0x0073C650(0x20)), it queues one (0x00695F20). Checked.
  - The command is 32, `ExecuteEffect` in `commands.gda`, put at the front as static and not player-issued.
  - Its ints are the death animation (effect +0x34) and the priority; its object is the killer.
- ExecuteEffect's start (0x00742B70) adds sub-action 51. Sub-action 51 is the handler 0x007AAC10, the knockback's too.
- Where the death effect is put in the creature's effect list was not read.

**Queuing it clears the queue.** Command 32's override is 2 in `commands.gda`, so `QueueCommand` (0x0073CD10) calls `ClearAllCommands(1, 0)` (0x0073C930) first.
- Queued commands that are not static are freed, with no End and no COMMAND_COMPLETE.
- The current command, if not static, is cleared even mid-sub-action: `ClearCurrentCommand(-2, 0, 1, 0)` → `Commands::End(cmd, -2)`.
- End's shared handler (0x00746900) **queues** COMMAND_COMPLETE (`AIMaster::AddEventDeltaTime`, not run at once). Its int 0 is the command's type and int 1 is -2 (`COMMAND_FAILED_COMMAND_CLEARED`).
- An Attack's end handler (0x00746B30) also plays animation 1, Pause, first, for a ranged weapon only (`0x006A2AD0`: the weapon's `bitm_base` Type 5 or 6; refined 2026-10-18).
- 0x007A12E0 and 0x007AAC10 clear nothing themselves.
- ExecuteEffect is static, and its sub-action never finishes for a death, so the dead creature keeps it as its current command (inferred: until the effect goes).

**The animation: Die, then Dead held.**
- The death effect's +0x34 comes from 0x005CA740(int 0, int 1 == 0). It reads table 103 `UnsyncDeathAnimations` when int 1 is 0, else 102 `SyncDeathAnimations`. The row is int 0, the column `React`, and the fallback 250.
- `EffectDeath()` sets int 0 to 1 whatever its argument (`effect_death_h.nss`:30), and `KillCreature` puts the damage in int 4 (`ACH_EFFECT_DEATH_DAMAGE_INDEX`). So an ordinary kill is Unsync row 1 (SIMPLE_DEATH_V2), animation 250.
  - That corrects "int 0 force, int 1 the damage" above (the `KillCreature` line and the handler's step 2).
  - For int 1 = 0 the handler sets int 3 of the effect's +0x54 array to 1, the unsync death.
- In `anim_base`: 250 is Die (`die`, a one-shot, TransitionTo 3); 3 is Dead (`dieloop`, looping, TransitionTo 263, Die Get Up); 1 is Pause.
- On sub-action 51's first tick, an unsync, non-looping death animation plays through slot 0xB0, `(250, 1.0, 2.5, loop 0)`.
  - A creature whose appearance (+0x7B0's +0x58) is 0x4E29 gets blend -3.0 instead.
  - A blend of -2.0 or below is a code: 20% (-2) or 5% (-3) of the clip's length (0x00694FE0).
  - The timer is the Die clip's own length (`AnimationStepLength(250)`).
  - If the creature's current animation already has the new one's State (already dead), it goes straight to the loop through TransitionTo (0x00696060).
  - A sync death (int 3 = 0, a deathblow) runs 0x007A7290 instead; not read.
- Every tick while the duration type is 6, Dead (3) is played again as `(3, 1.0, -2.0, loop 1)` if the current animation is a loop whose State isn't 1, or isn't 3 while creature byte +0x1AC is 0. Inferred: Die plays out, the animation's own TransitionTo moves it to 3, and these calls hold it there.
- DEATH (4) is run once, when the timer runs out: the end of the Die clip.

### The body bag

**`SpawnBodyBag` (0x006A91E0) moves nothing: it makes or re-places the bag and links it.**
- Without `bForce`, no bag is made:
  - for a creature not dead (slot 0x1D0);
  - for one with no loot and no bag yet (+0x790 = -1);
  - for one from the area's pool of reused creatures (+0xB60 ≠ 0, inferred).
- With `bForce`, all three are skipped. No bag at all for a creature with no area. There is no party-member test.
- "Loot" (0x006A90D0) is money above 0, or a droppable item (0x006A4440: the item's +0x4B0, `IsItemDroppable`'s byte) equipped or in the backpack.
- An existing bag (+0x790 ≠ -1) is found and placed again, not made again.
- A new bag is placeable template `gen_ip_bodybag`, with the appearance's `CorpseModel` or `PLC_BonePile_01_0`.
  - Links: creature +0x790 = the bag; the bag's +0x664 = the creature (`LootableObjectId`); the bag's +0x600 = 1 (`IsBodyBag`).
  - It is placed at the creature's position and orientation, set plot (slot 0x164(1)) and active.
- Slot 0x130 (0x0074CFC0) copies the creature's name to the bag; it moves no items.

**The items move later, in 0x006B9620(bag).** It is called when the bag is used (0x007A7A0C, just before it opens) or when the corpse decays (0x006B994C):
- Equipped items flagged +0x5D0 are destroyed.
- Every droppable item, backpack first then equipped, is moved into the bag (0x00762C40: taken from the holder, added to the bag, not copied).
- Money becomes a `gen_im_copper.uti` stack in the bag, and is taken off the creature (inferred, 0x007CA020).
- Neither plot nor stealable flags are tested.

**`0x006A3A80` is the appearance's `DecayBehaviour`** (`apr_base` column hash 0x98BFB469, checked).
- Values: 3 for Rage Demon, Shade and Ash Wraith; 2 for Broodmother and High Dragon; 0 for the rest.
- Behaviour 3 calls 0x0069F360(1000): the corpse fades about a second after the bag spawns.
  - 0x0069F360 is the corpse's decay delay (the address library's `CCCreature::ForScript_SetItemDroppable` is a wrong name). `SetBodybagDecayDelay` calls it on the bag's creature.
  - It sets +0xC8C = 1 and moves the dead-time counter +0xC88.
- The decay (read 2026-10-20 by lib-agent-2, for DAOParagonEngine #126; [C] checked in disassembly, [I] inferred):
  - **Constants,** in ms, initialised data [C]: `[0x00C021C4]` = 60000, the time before decay, set by `SetCreaturesGlobalMaxTimeBeforeDecay` and saved as `SavegameCreatureTimeBeforeDecay` 16499 (written at 0x005FEF09, read at 0x005FFA64, 60000 when absent at 0x005FFA79); `[0x00C021C8]` = 2000, the fade's length; `[0x00C021CC]` = 1500, the time left at which the decay step runs.
  - **The update** 0x006BC670 runs while the corpse may decay (0x006A3AD0, below) [C]. It adds dt to +0xC88, the time dead. At 60000 or more: SetFade(0, 0), Decay(1), the pooled creature's (+0xB60) slot 0x140(0), and it leaves the pending list. Otherwise, with left = 60000 − dead: under 2000 left, SetFade(left / 2000, 1) (0x00695060: +0x190 alpha, +0x1A4 flag) and it leaves the pending list, and at 1500 or less left, Decay(0), once; else it adds itself to the pending list (0x0076B540, unique).
  - **Too many corpses** [C]: the pending list (a vector at 0x00C3552C) holds the decayable corpses not yet fading. When its size is above `CorpseLimit` (0x007C4BD0), the corpse being updated gets SetDecayDelay(0) (0x0069F360). `CorpseLimit` is by `GraphicsDetailLevel` (ClientOptions `[0x00C32938]` +0x4F0): 0 to 3 give 5, 10, 15 and 20, and the default is 3, so 20. Which corpses are cut follows the update order, not their age [I].
  - **SetDecayDelay(ms)** (0x0069F360) [C] sets dead = 60000 − 2000 − ms − 1 when at least 2000 is left, so the fade starts ms + 1 later, and sets +0xC8C = 1.
  - **The decay step** 0x006B98E0(atEnd) runs once (+0x794) [C]. It does nothing with no bag (+0x790 = −1) before 15000 ms dead, when the corpse may not decay, or for a pooled creature (+0xB60). Otherwise it calls DestroyObject(atEnd ? 0 : 1500) through vtable slot 0x198 (0x00696A60), so the creature goes at the fade's end and the bag stays; it moves the loot into the bag (0x006B9620), places the bag at the creature (0x007526F0) and calls 0x0069B900 on the bag.
  - **The fade** [C]: 0x00695080 multiplies +0x190 by +0x198 and passes it to the model (AnimBase at +0x154, slot 8 → 0x00A61280). That sets `mml_fOpacityMultiplier` on each mesh (0x004F4900, mode 4), switches the material to its blended render class (0xB→1, 0xC→5, 0xD→6), and sets mesh flag 0x10 (0x80000 with the flag) and model flag 0x4000. No effect is started: no particles, no vfx row. apr_base has no decay or vfx column, vfx_base has no decay or dissolve row, and the scripts' death handler (`creature_core.nss`:223–317) starts only a blood-pool decal (ordinary creatures), vfx 93095 `fxa_stealth_imp_p` (ghost trait, destroyed at 1500 ms with no bag), or an on-death explosion (explosive traits). So the "dark smoke" players describe is most likely the blended model fading over a dark ground [I: no vanilla frame captured] (read 2026-10-21 by lib-agent-2 for DAOParagonEngine #126).
- Whether a corpse may decay (0x006A3AD0) is decided in this order:
  1. Yes if a delay was set.
  2. No if a decay-blocking effect (type 4) is on it, or for a party member (inferred), or for the +0xBD4, +0xBD8 or +0xB60 cases.
  3. Otherwise only behaviours 0 and 3 decay; 1 and 2 never do.
- The death sub-action's `0x0069EDC0` unless DecayBehaviour is 2 (above, DEATH for a party member) is this.

### A placed creature's load

**Area load.** 0x00683820 walks the area's `CreatureList`. For each instance whose `Importance` passes, it makes the creature and calls 0x006ABDC0. That reads the instance's `TemplateResRef`, opens the `.utc` (slot 117, 0x006A27B0), and runs the creature's field reader, slot 23 (0x006C06F0), twice:
- over the template, with flag 1;
- then over the instance's struct, with flag 2 (0x006ABFBA–0x006ABFC3, checked).

**The instance overrides any field it has.** Each field is read with the current value as its default, and the int reader (0x00A37050) takes a value only when the field exists with GFF type 5. So the instance's `Rank` replaces the template's as its `TeamId` does.
- Rank: read at 0x006C088F, stored through the attribute at +0x4B0.
- The other fields that can be overridden:
  - the object's: position, orientation, `LocName`, `Tag`, `Active`, `EventScript`, `Plot`, `Immortal`, `GroupId`, `AILevel`, `InteractRadius`, `Conversation`, `TeamId`;
  - the creature's: `Importance`, `Race`, `Package`, `PackageAI`, `Rank`, `TreasureCategory`, `Combatant`, `SoundSet`, `NoPermDeath`, `CharType`, `IsSelectable`, `IsPlotGiver`, `Trackable`, `WeaponsDrawn`, `StealthMode`, `StealthRadius`, equipment, `DialogResRef`;
  - the stats' load, abilities and `PerceptionClass` among them.
- The Harrowing's instances carry `TeamId`, `Rank`, `TreasureCategory`, `Trackable` and others as type 5.
- Nothing at load derives a value from Rank. What reads it: `GetCreatureRank`, the client VM's copy, and the save. Scaling by rank is the scripts'.

**The template's ability lists add each entry plainly.** 0x0071ECD0 reads one INT per entry (`SpellNumber`, `TalentNumber` or `SkillID`) and skips only -1 or a failed read. It then calls 0x0071EAC0(id, type, -1), the same function `AddAbility` (0x00669AB0) calls.
- `AddAbility` checks the `ABI_base` row first and refuses AbilityType 4.
- 0x0071EAC0 reads only `AbilityType`, `GUIType` and `Passive_ID`. There is no prerequisite, level or rank check.
- It does, in order:
  1. Returns if the creature has the ability already (except an item ability from an item).
  2. Inserts the ability, and for a follower recomputes the party.
  3. Loads the ability type's GUI blend tree.
  4. Makes the cooldown record and the interface's `AbilityInstance`.
  5. If new and creature +0xC58 is 0, applies the passive (`Passive_ID` → table 0x75's properties) and queues ABILITY_ACQUIRED (73) for item abilities only.

### CREATURE_SPAWN_DEAD and PHYSICS_DISABLED in the engine (lib-agent-2, 2026-10-19, GOG exe, read-only)

#### Short answers
1. **The two strings are not static initialisers of globals. They are entries 4 and 11 of the engine's table of well-known script-variable names** (registered at 0x004E85D8-0x004E8700; entry = {category 0, id}). The variable key `{0, id}` is what the engine builds on the stack and hands to `ScriptBackend::ForScript_GetLocalInt` (0x00702E90, creature table at creature+0x2E0). Ids (checked, from the pushes): 1 AMBIENT_SYSTEM_STATE, 2 AREA_ID, 3 AMBIENT_ANIM_PATTERN, **4 CREATURE_SPAWN_DEAD**, 5 AREA_PARTY_CAMP, 6 RUBBER_HOME_ENABLED, 7-9 RUBBER_HOME_LOCATION_X/Y/Z, 10 ITEM_CRUST, **11 (0xB) PHYSICS_DISABLED**, 12 PLC_AT_DEST_AREA_TAG, 13 ITEM_RUNE_ENABLED, 14 PROJECTILE_OVERRIDE, 15 RUNSCRIPT_VAR.
2. **Value 1 = the creature dies at spawn by the engine setting its current health to 0.** Value 2 = a permanent EFFECT_TYPE_SIMULATE_DEATH (1045) effect, health untouched (a "fake death", removed when it starts a fight). The reader is the function at **0x006BF710**, called after the creature's field reader (slot 23, 0x006C06F0) has read the instance (second pass, flag 2: 0x006C0BA8-0x006C0BAA `cmp [flag],2; call 0x6BF710`) and again at the end of a conversation/staged-scene release (0x007F2800, call at 0x007F2B12, after the physics controller is rebuilt).
3. **PHYSICS_DISABLED non-zero: the capsule controller (PhysX character controller, creature+0x770) is not built** (0x006B1ED0 returns early, 0x006B1F67-0x006B1F6E). A dead creature (health <= 0) never gets one either, except DecayBehaviour 2 (dragons), and neither does a creature with a SimulateDeath effect. So for a spawn-dead corpse, PHYSICS_DISABLED is redundant.
4. **The pose: not found.** Nothing in 0x006BF710 plays an animation. The engine's own death animation is the death effect's sub-action 51 (dev/COMBAT.md:296-311), which a spawn-dead creature does not go through. Port recommendation (inferred): put the creature straight into anim_base **3 `Dead` (AnimName `dieloop`, Type 1 looping, State 1, TransitionTo 263)**, held; use row 2 `Dead_v2` (`dieLoop_v2`) only if you want variety. A killed creature plays 250 `die` (State 1, TransitionTo 3) first; spawn-dead needs no die clip.

#### 1. 0x006BF710 (checked in disassembly)
Entry: `ecx` = creature. Saves bytes +0x2C5 and +0x421, sets +0x2C5 = 1 and +0x421 = 0 for the duration, restores them at the end (0x006BF86F-0x006BF881). Reads local int key `{0,4}` from creature+0x2E0 (0x006BF775-0x006BF77D; GetLocalInt returns 0 if absent). Returns `value != 0` in al.

```
v = GetLocalInt(creature, CREATURE_SPAWN_DEAD)       // key {0,4}
switch v:
  case 1:                                            // 0x006BF81A
     creature->vslot_0x124(0, 1)                     // 0x006A7DD0: CCCreatureStats::SetCurrent(property 7 = current health, 0.0), flag 1 -> 0x0071B320 -> 0x00719A00 -> health-change handler 0x006BC4B0
  case 2:                                            // 0x006BF799-0x006BF818
     if (!HasSimulateDeath(creature, flagOnly=1)) {  // 0x0079FD80(1): any effect of type 0x415 not removed whose flag[4]==1
        e = new CGameEffect(type 0x415 = 1045 EFFECT_TYPE_SIMULATE_DEATH)   // 0x00A652A0
        e.creator = creature id (iface slot 0x20)
        e.durationType = 3                           // [e+0x28] = 3, EFFECT_DURATION_TYPE_PERMANENT
        e.flag[4] = 1                                // 0x00A5B890(4,1): marks "spawned dead"
        effects->Apply(e)                            // 0x007A1A20
        creature.iface(+0x70)->slot_0x78()           // 0x006B3620: drop the walk-grid obstacle (unless DecayBehaviour 2), stop look-at, reset movement controller, re-place
     }
  case -1:                                           // 0x006BF82B: wake: remove every SimulateDeath effect (predicate type==0x415), 0x007A0C90
  default (0, >=3): nothing
```
Health-change handler 0x006BC4B0(creature, oldHealth, flag), reached from SetCurrent of property 7 (0x00719A25-0x00719A32). Old > 0 and new <= 0 (6BC4EC): with flag set and not immortal, iface slot 0x50 (0x007A1A40, effects removed); 0x006B8D20; clears; vslot 0x1B8(0); iface slot 0x78 (0x006B3620) unless a SimulateDeath effect exists. Old <= 0 and new > 0 (6BC586, revive): 0x0073C930, +0xC88 (dead timer) = 0, rebuild model placement, **SetLocalInt(CREATURE_SPAWN_DEAD = 0) if it was 1** (0x006BC603-0x006BC631), iface slot 0x7C (0x006B3690: obstacle back, physics controller rebuilt) unless a SimulateDeath effect exists. So a revive clears the flag.

IsDead is vslot 0x1D0 (0x006A3660): health <= 0 (dev/COMBAT.md:238). Value 1 creatures are therefore ordinary dead creatures to every later check.

##### Other readers of key {0,4} (checked)
- 0x006A3AD0 (0x006A3B4F): corpse decay is refused when the var > 0 (the fade/body-bag decay never runs on a spawn-dead corpse). Rest of the function: dev/COMBAT.md:340.
- 0x0073F0E0 (0x0073F1CD): a search over area objects skips creatures with the var > 0 (what the function is for: not read).
- Scripts: creature_core.nss:346 codex credit only when var == 0 and SPAWN_HOSTILE_LYING_ON_GROUND == 0; :381 value 1 on EVENT_TYPE_SPAWN sets a random gore level (SetCreatureGoreLevel(RandomFloat())) and ends (no other spawn work: no crust, no AI start); ability_summon_h.nss:95 (Animate Dead ignores value 1); utility_h.nss:1503 on combat start, value 2 -> var = 0 and RemoveEffectsByParameters(EFFECT_TYPE_SIMULATE_DEATH). effects_h.nss:541/918: the script's apply/remove of type 1045 just returns TRUE (all behaviour is engine). Used with 1 by bhn200/pre100/ran401 scripts; ran401ar_highway_zevran1.nss:41-43 sets 0 and applies EffectResurrection.
- Death event (superseded 2026-10-19 by lib-agent-2: this said no death sub-action runs, so no DEATH (4) and no bag; the reading below shows health 0 → 0x006BC4B0 → slot 0x50 0x007A1A40 applies the type-7 death effect, whose sub-action 51 plays the die clip and raises DEATH): DEATH fires, and `creature_core`'s DEATH handler calls `SpawnBodyBag(self)` without bForce, so a bag is made only with loot (0x006A90D0: money > 0 or a droppable item); the `pre200cr_dead` templates have no items and no money, so they get none. DYING's XP and treasure need the killer to be a party member, and a creature killed by itself is not one. Codex credit is skipped for `CREATURE_SPAWN_DEAD` != 0 (`creature_core.nss`:346).

#### 2. PHYSICS_DISABLED (key {0,0xB}), checked
0x006B1ED0 (build the capsule, called from the activation path, from 0x006B36BB/0x007F2B00 with (-1.0f, -1.0f)): returns without building when any of: iface slot 0x6C false (inactive); **GetLocalInt({0,0xB}) != 0** (0x006B1F67-0x006B1F6E); creature is dead and DecayBehaviour != 2 (0x006B1F31-0x006B1F78); 0x0079FD80(0) true (has a SimulateDeath effect); no model (vslot 0xA0 null or its slot 8 null). Otherwise destroys any old controller (0x0069EDC0) and makes a PhysicsControllerCapsule (PATHFINDING.md:283-330). The script command that writes the same key is in dev/script_commands/effect_access.md:232. No capsule means no PhysX contact and no ground following by the controller; the creature stays where it was placed (z from the placement, see PATHFINDING.md:277 ff).

#### 3. Creature loaded from a save (checked structure, not played)
0x006BE980 (save reader, slot 34) has no call to 0x006BF710 and never reads key {0,4}. After the load it only does (0x006BEBA3-0x006BEBCF): if 0x0079F4A0(0) (a death-duration-6 effect is pending) and health > 0, `SetCurrent(health = 0, flag 1)`. A spawn-dead creature in a save is therefore restored by its saved stats (health 0), saved effects (SimulateDeath for value 2) and saved variable table. The var is written to a save only if `changed` was set (dev/script_commands/vars.md:34), and the template-loaded record has changed = 0, so a never-touched CREATURE_SPAWN_DEAD = 1 is re-read from the template when the area is built again. Whether a placed creature is rebuilt from its template on a save load, and what pose the save restores, is not read.

#### Table rows (ghilana, anim_base.gda, checked)
| ID | Brow | AnimName | Type | TransitionTo | Priority | State |
|---|---|---|---|---|---|---|
| 250 | Die | die | 0 one-shot | 3 | 4 | 1 |
| 254 | Die v2 | die_v2 | 0 | 2 | 4 | 1 |
| 3 | Dead | dieloop | 1 loop | 263 | 3 | 1 |
| 2 | Dead_v2 | dieLoop_v2 | 1 loop | 938 | 3 | 1 |
| 263 | Die Get Up (Resurrect) | diegetup | 0 | 1 | 0 | 0 |
| 1 | Pause | pause | 1 | 1 | 0 | 0 |
(State 1 also gives the 60/60 degree body tilt, PATHFINDING.md:372-400.) Template pre200cr_dead_1.utc (ghilana): Appearance_Type 15, HeadMorph hm_pre200_dead1.mor, PropertyList has Index 7 (health) with no Base (so no value read here; I cannot say what the stats load makes of it), plus SPAWN_HOSTILE_LYING_ON_GROUND = 0 among the VarTable.

#### Not read
- **Which code puts a spawn-dead (value 1) creature into the Dead pose**, or whether a posed clip is needed at all (e.g. the animation player starting in Dead for health <= 0, 0x006BDC70/0x006A1470). Leads: 0x006964D0(bool) (plays the current death effect's animation id, effect+0x34, through vslot 0xB0 with (id, 1.0, 0xAE2E00, 1) then 0x006956D0, else Pause if the bool is set; it runs after 0x006BF710 only in the conversation-release path 0x007F2B17-0x007F2B22); 0x006A26D0 (called right after 0x006BF710 in the field reader: plays the animation of table 0xAA row = var {0,3} when AMBIENT_SYSTEM_STATE bits 1 and 2 are set; the pre200cr_dead templates were not checked for those vars).
- What effect flag[4] means beyond marking the spawn-dead SimulateDeath (only 0x0079F2E0 reads it).
- 0x0079F4A0 semantics beyond "death-duration (6) effect pending, not removed".
- The sub-action/animation of a SimulateDeath effect applied by a script (no shipped script applies it).
- Play: nothing was seen in the game.

### What poses a creature placed dead (CREATURE_SPAWN_DEAD = 1) (lib-agent-2, 2026-10-19, GOG exe, read-only)

Builds on %TEMP%\lib-agent-2-spawndead.md (6BF710 etc.) and master dev/COMBAT.md:199-311 (death effect, ExecuteEffect, sub-action 51). New here: everything under "New".

#### Short answers
1. (a) **6BC4B0 plays nothing itself, and no dead state is read by the animation system. It makes the engine's ordinary death.** On health old>0 -> new<=0 with its flag set (6BF710 passes flag 1) and the creature not immortal (vslot 0x168 false), it calls iface slot 0x50 = **0x007A1A40** (checked). That builds a type-7 effect (`0xA652A0(7)`), creator = the creature (iface slot 0x20 -> 0xA65250 sets +0x40), `+0x28 = 6` (EFFECT_DURATION_TYPE_DEATH), `SetInt(0, 1)` (0xA5B710), and adds it with 0x007A1A20 -> 0x007A1920 -> 0x007A0E30. For a type-7 effect with no death effect yet present (0x007A1920 searches first, 0x007A1983-0x007A19A3) that reaches the native death handler **0x007A12E0** (sets effect +0x34 = `0x005CA740(1, ...)` = 250, runs APPLY_EFFECT in the scripts, queues command 32 ExecuteEffect, COMBAT.md:208-215/:279-287). ExecuteEffect's sub-action 51 (0x007AAC10) plays the pose. So a spawn-dead creature is a creature killed with no killer. 6BF710 forces `+0x2C5 = 1` (active) and `+0x421 = 0` for exactly this call: 0x007A0E30 only takes the native handler when the target's slot 0x6C (= +0x2C5 set, +0x421 clear, PARTY.md:8) says it is active, and the command queue runs only for an active creature (0x0073D34D tests +0x2C5). That is the answer to "why does 6BF710 touch those bytes" (checked for the flags and tests; inferred for the reason).
2. (b) **There is no "initial pose" chooser that tests IsDead.** The model is loaded earlier than 6BF710 (0x006BDC70, called at 0x006C056F in the field reader, 6BF710 at 0x006C0BAA). The animation set's constructor (0x00A60B80, 0x00A60C66-0x00A60C76) starts with +0x13C = 0, **+0x140 = 1 (Pause)**. 0x006BDC70 replays AnimBase+0x140 instantly (0x006BE284 `0x695700` -> vslot 0xB0 (anim, 1.0, -1.0, 1)), i.e. Pause, and uses IsDead (0x006BE22E, vslot 0x1D0) only to pick the **face** state (dead 0x006CAB40, alive 0x006CAB00 on the object 0x0069E9D0 returns), not the body. Its last step `0x006964D0(0)` (0x006BE2F4) plays the creature's death effect's animation instantly if one exists (none yet at model load for a first spawn) and does nothing otherwise. The ambient idle chooser 0x006A26D0 (right after 6BF710) only sets AnimBase+0x140 (`0x006956D0`) from table 0xAA row = AMBIENT_ANIM_PATTERN when AMBIENT_SYSTEM_STATE has bits 1 and 2; it plays nothing, and a spawn-dead creature has no such vars (not checked in the templates).
3. (c) **The pose is the death animation sub-action's, from the first tick of ExecuteEffect, after the area is running.** On its first tick the creature's current animation is Pause (State 0), so it starts the **Die clip (250 `die`, TransitionTo 3) from frame 0**, not `dieloop` from frame 0 and not an instant skip; the Dead loop (3 `dieloop`) is played from the tick after, held each tick (`(3, 1.0, -2.0, loop 1)`, 0x007AB368-0x007AB3B8). It is not entered at the clip's end by any instant jump. Only if AnimBase+0x140 already has State 1 (a death/sleep row) does the first tick use 3 directly (0x007AB08C-0x007AB09F: `if State(0x695700()) == 1 then anim = 3`). Caveat: the first-tick branch that sets the Die timer vs one that sets timer 0 (below) was not settled, and nothing was seen in play.

#### Port pseudo-code (inferred from the above)
```
at spawn with CREATURE_SPAWN_DEAD == 1 (after the model is loaded, once the creature may run commands):
   set health 0                       // engine does the rest; or, if the port has no effect system:
   face = dead                        // 0x6CAB40 (closed eyes/face state), body below
   if the port wants vanilla timing: play(250 die, rate 1.0, blend 2.5 (0.4 s), non-looping) from clip time 0 on the first command tick,
                                     then 3 dieloop (TransitionTo of 250) held every tick as (3, 1.0, blend -2.0 = 20% of its length, loop)
   if the port wants "already lying" : play(3 dieloop, 1.0, blend -1.0 instant, loop)   // the engine's own already-dead path: State==1 -> anim 3
```
Neither skips the die clip in vanilla; "already lying" is a port choice (the engine does it only for a creature whose current animation already has State 1). No DEATH script event ordering concern: the sub-action's timer runs DEATH (4) with the effect's creator at the end of the clip (COMBAT.md:243).

#### Evidence (checked = read in disassembly)
- 6BC4B0 (0x006BC4B0): health compare 0x006BC4B0-0x006BC4EC; flag `cmp byte [esp+0x14],0` 0x006BC4EC, IsImmortal vslot 0x168 0x006BC4FB-0x006BC507, `iface slot 0x50` call 0x006BC50C-0x006BC512, then 0x006B8D20 (clears +0xC18/+0xC28), party 0x00838210/0x00631F30, vslot 0x1B8(0) = 0x006A1B80 (gore/crust level, not animation), then slot 0x78 = 0x006B3620 (checked: face dead, controller reset, re-place; no play call).
- 0x007A1A40 (checked, 0x007A1A63-0x007A1ABC): new 0x7C-byte effect, `push 7; call 0xA652A0`, `[eff+0x28] = 6`, `SetInt(0,1)` (`push 1; push 0; call 0xA5B710`), `call 0x7A1A20`.
- 0x007A1920 (checked): duration type 6 -> searches existing death effects (0x00A64950); none -> `call 0x7A0E30`.
- 0x006964D0 callers (checked): 0x006BE2F4 (end of 0x006BDC70 model reload, arg 0), 0x007F2B22 (conversation release, arg 0... after 6BF710), 0x0070D50F (arg 1; a reset/place routine), 0x00697904. Body: effect = `0x79F240` (iface slot 1 = 0x69A0D0 returns [iface+0xE8] = the creature's death effect, then +0x34); chain TransitionTo (0x5CA6B0, hash 0xFFC372E7) up to 5 times until `0x695390` (tree has the node) is true; play (id, 1.0, -1.0 (0xAE2E00), 1) via vslot 0xB0 and `0x6956D0(id)` (AnimBase+0x140 = id). With no effect: if arg set, plays 1 Pause instantly and sets +0x140 = 1.
- AnimBase ctor 0xA60B80: +0x13C = ebx(0), +0x140 = 1, +0x144 = 0 (0x00A60C66-0x00A60C76). 0x695700 = `AnimBase(creature+0x154)->+0x140`; 0x6956B0 = +0x13C (current id).
- Floats: 0xAE2E00 = -1.0, 0xAFF63C = -2.0, 0xB03B08 = 2.5, 0xAE7130 = 3.0, 0xAF553C = -3.0 (read from the file).
- Plays of anim 3 by immediate in the whole exe: only 0x007AB370 and 0x007AB3A0 (both in 0x007AAC10), plus the generic 0x7AB08C->3 substitution. A scan of every `call [reg+0xB0]` with `push 3` or `push 250` within 20 bytes found only 0x7AB370, 0x7AB3A0, 0x81CFF0, 0x81D618 (the last two not read). Of the 40 IsDead (vslot 0x1D0) call sites none of the ones checked (0x6A52C0, 0x6A8D60, 0x6AB100, 0x6AD200, 0x6AE780, 0x6B89B0, 0x6BA840, 0x6BB890, 0x6CAD80, 0x70EAA0) calls a play: so no other code poses a dead creature. 0x6AB6E0 (idle tick) runs only when the current animation is 1.
- Script side: creature_core.nss:376-392: "Creatures spawning with 0 health will die instantly. This is handled in engine, the script here just ensures some gore" (then returns).
- First tick of 0x007AAC10 (checked as far as): anim from effect +0x34 (0xA53690) at 0x007AACD8; if effect int 0 is 0 the anim is replaced by `0x696060(0x695700())`; sets params (creator, priority, flags); 0x007AB06C-0x007AB09F: current anim State==1 -> 3; then 0x007AB193 `0x695390(anim)`: true -> 0x007AB2D9 plays (anim, 1.0, -2.0, 1) and sets the sub-action's +0x14 (timer) = 0; false -> either sync death 0x7A7290, or timer = now + `0x6957A0(anim)` (clip length) and play (anim, 1.0, 2.5, 0) (0x007AB1D4-0x007AB24C, blend -3.0 for appearance 0x4E29). COMBAT.md:310 describes the second form as what a kill plays. Per tick for type 6 (0x007AB313-0x007AB41F): if health > 0 set it 0 (0x007AB32D), play 3 as above, effect/sub-action flags, then `0x7A1AE0` remaining time; at <= 0 (and not held by flag 1) run DEATH.

#### Not read
- **Which branch a humanoid's first tick takes at 0x007AB198** (valid-node path 0x7AB2D9 with timer 0, or the timed path 0x7AB1D4 with the clip's length). The humanoid.anb has `die` and `dieloop` nodes (ghilana, checked), which suggests the first, but the 1.5-2.5 s DEATH delay seen in play (LIBRARY.md:238) fits the second; one of my readings of the stack slot [esp+0x1c] or of 0x695390 is wrong and I did not settle it. Either way the animation played first is 250 `die` for a creature in Pause, and 3 thereafter.
- Whether the Die clip is visibly played on the loading screen's last frames or after the screen goes (when the first ExecuteEffect tick runs after the area load): needs a play check (Eluvian: load an area with a CREATURE_SPAWN_DEAD creature, read the creature's animation id each frame).
- 0x7A0E30's slot-0x6C gate was taken from COMBAT.md:165, not re-read; 0x81CFF0/0x81D618; whether the pre100/bhn200/ran401 templates set AMBIENT_SYSTEM_STATE/AMBIENT_ANIM_PATTERN.
- Save load: a saved dead creature has its effect list and ExecuteEffect state from the save (0x6BE980 does not call 6BF710); not read what pose results.

### Non-humanoids' movement

Non-humanoids use the same controller as humanoids, `MovementAnimController` (vftable 0x00B59AC0).
- **The trees.** Its trees are the appearance's `MovementTree` and `CombatMoveTree` columns (hashes 0xEB48D02A, 0x75F492EC, checked). Each is an ordinary `.anb`, loaded with the other five trees into the creature's animation player (0x006BDC70 → 0x006A1470). For gender 2, a `_fem.anb` is used where one exists.
- **No fallback.** Nothing substitutes `movement.anb`; an empty column leaves the switch doing nothing.
- **No anim-table row for walking or running.** The controller sets weights on nodes by fixed names: `Motion`, `Transition`, `Walk`, `Run`, `WalkForward`, `WalkBackward`, `walkf`, `walkfl`, `walkfr`, `walkb`, `walkbl`, `walkbr`, `runf`, `runleanl`, `runleanr`. Transitions use anim rows, for example 84 `pauserunf`.

| apr_base row | Creature | Explore / combat tree | Game Walk / Run / Cbt | Anim Walk / Run / CbtStrF / Cbt |
|---|---|---|---|---|
| 49 | wolf `c_wolfa` (also 53, 74, the mabari 34/35) | `C_DOGa_expmvt` / `C_DOGa_cbtmvt` | 1.54 / 6 / 4.66 | 1.2 / 3.75 / 3.15 / 2.8 |
| 27 | rage demon | `C_RageDemon_expmvt` / `_cbtmvt` | 1.4 / 3.62 / 4.66 | 1.4 / 3.62 / 2 / 2.87 |
| 9 | black bear (8, 68 great bear: `C_GRETBEARa_*`) | `C_BEARa_expmvt` / `_cbtmvt` | 1.3 / 6 / 4.66 | 1.3 / 3.62 / 2 / 2.87 |
| 37, 41 | rats | `c_RODENTa_expmvt` / `_cbtmvt` | 3 / 3.5 / 3.5 | all 3.5 |
| 15 | human | `movement` / `combat_movement` | 1.54 / 6 / 4.66 | 1.54 / 3.62 / 2 / 3.62 |

- The wolf's explore tree: `walkf` `c_dog.wkf.ani`, `walkb` `c_dog.wkb.ani`, `runf` `c_dog.rnf.ani`, and the leans `c_dog.rn_lnl` and `c_dog.rn_lnr`. Its combat tree has the six `c_dog.cwk*` walks, and `runf` `c_dog.crnf.ani`.
- Every leaf of the rat's tree is `c_rat.run.ani`.
- The non-humanoid trees have no `Transition` clips.

**Weights:**
- Explore, not strafing (0x00A6B7A0):
  - Backward: `walkb` 1.
  - Forward: `walkf` and `runf` get 1 − |lean|, and the leans get the lean.
  - Run weight r (0x00A69B50) is 0 backward, 1 with the combat tree on, otherwise speed ÷ Game_RunSpeed above Game_WalkSpeed, else 0. `Walk` gets 1 − r and `Run` gets r.
- Combat, strafing (0x00A6B9B0):
  - The forward weight is w = (90 − the angle between facing and moving) ÷ 90.
  - `walkf` or `walkb` gets w; the side's `walkfl`/`walkfr` or `walkbl`/`walkbr` gets 1 − w.
- Combat, not strafing: r is 1, so `runf` plays.

**Playback rate** (0x00A6AA70):
- Explore:
  - below Game_WalkSpeed: speed ÷ Anim_WalkSpeed;
  - above Game_RunSpeed: speed ÷ Anim_RunSpeed;
  - between: the two blended linearly.
- Combat: speed ÷ Anim_CbtStrFSpeed when strafing, speed ÷ Anim_CbtSpeed otherwise.

### GM_DEAD

**The scripts set it; the engine never decides the party is dead.**
- `player_core.nss` (343–408), on a party member's DEATH:
  - if `IsPartyDead()`, it sets the death hint and calls `WR_SetGameMode(GM_DEAD)`;
  - that queues SET_GAME_MODE (1021) to the module, and `module_core`:370 calls `SetGameMode`.
- Areas with `AREA_GAME_MODE_OVERRIDE` may turn it into something else (the Landsmeet's "captured").
- The only engine caller with a literal 2 is the script command `ShowDeathScreen(bCaptured)` (771, 0x00660D60). It sets the party's `CapturedOnDeath` and calls `SetGameMode(2)`; no script calls it.

**The mode.**
- `GM_DEAD` = 2, class `GameModeDead` (constructor 0x0076E0A0): enter 0x0076E0F0, leave 0x0076E2D0, update 0x0076E000.
- Its slot 10 answers true, so it replaces the current mode rather than stacking on it.
- It has none of Explore's click and camera handlers.

**Enter, in order:**
1. Telemetry.
2. Saving is turned off (`SaveLoadProxy` +0x58 = 0, through the base enter, because the mode's +0x68 is 1).
3. The scene camera's motion is stopped (0x006C2890(0, 1), as at a cutscene's start).
4. The save list is rebuilt.
5. GUI scene 0x13, `GUIDeath`, is shown at once: the `DeathScreen` movie (`deathscreen.gfx`), with `GUIDeath.DeathHint` from `SetDeathHint`. This also signals GUI_OPENED.
6. The input group `GameModeDead` is turned on. Its one message is `PopGUIScreenMessage`, which closes the topmost full-screen GUI (inferred: Escape).

No delay is in the engine; any reveal is the movie's own.

**The movie's buttons:**
- **Load last save** sends `SaveLoad.ResumeGame`.
- **Load game** opens `SaveLoad` on top, and the game stays in GM_DEAD.
- **Quit** sends `ShutDownToStartMenuMessage`.
- **Captured** (`Party.CapturedOnDeath`): its OK sends `DeathResurrectPartyMessage`.
  - The mode's handler (0x0076E400) queues EVENT_TYPE_DEATH_RES_PARTY (61) to the module.
  - `module_core` resurrects the party, then sets GM_EXPLORE half a second later.

**Leave:** it hides scene 0x13, closes every full-screen GUI left open, and turns the input group off.

**The world is not paused.**
- No `SetPaused`.
- `CCCreature::Update` asks the mode (slot +0x4C) only before perception, and Dead answers false (0x006BE79E). So perception stops, but commands, regeneration (the living only) and the rest of the update go on.
- Effects keep ageing: no mode check was found (inferred).
- Script timers run.
- The sound category `audio_categories/ducking/OnDeath` plays while in the mode (0x005BD220).
