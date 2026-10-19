# The party: who is active, the pool on an area transition, a follower's state, following, control and tactics

An object's active flag and the searches it limits, the party pool carried on an area transition, and `SetFollowerState`'s switch; then the engine's follow, control and selection, and its share of the tactics, each in its own section below. Read 2026-10-18 by lib-agent-2 (with subagents) for DAOParagonEngine's mage origin. Not seen in play. The key claims below were checked in the disassembly. The formation the arrival places the active party in is `dev/LOAD_EVENTS.md`'s.

## Active: the effects holder's slot +0x6C, and the object's slot +0x140

- **The test is not on the main vtable.** `CCObject`'s, `CCCreature`'s and `CCPlaceable`'s main vtables (0x00B0EE9C, 0x00B10484, 0x00B1C084) hold a bare `ret` at +0x6C (0x005BDCF0).
- **The test** is slot +0x6C of the base at +0x70 (`mov eax,[obj+0x70]; lea ecx,[obj+0x70]; call [eax+0x6C]`): 0x00699FA0 in all three, +0x2C5 set and +0x421 clear. +0x421 is a deactivation pending while its animation plays; `SetObjectActive` sets it to 1 (0x00661B1F).
- **SetActive(bool)** is the main vtable's +0x140:
  - `CCObject` 0x00698CD0;
  - `CCCreature` 0x006B5D90, which calls it;
  - `CCPlaceable` 0x0074F200, which calls it and then stamps the walk-grid patch, 0x0074DBA0(+0x658, !active).
- **A follower in the active party is never deactivated through it.** `CCCreature`'s SetActive(false) only clears +0x421 when `IsFollower` (0x00631830, the active party +0x234) holds the creature (0x006B5D9C–0x006B5DC6, checked).
- **What SetActive does besides the flag** (0x00698CD0): it clears +0x421 and calls slot +0x154 and +0x13C(active), which registers the object with the app (inferred: its presence in the world). A change runs `ObjectTimer::Update` on +0x2B8. Deactivating calls slot +0x8C, 0x007A0DC0 on the effects holder and `ClearAllCommands(1, 1)`.
- **The commands:** `GetObjectActive` (141, the shared handler 0x00668A20) is the test. `SetObjectActive` (140, 0x006618B0) returns when already in the state asked; with an animation, deactivating queues it, sets +0x421 and leaves the flag for later; otherwise it calls +0x140.

## A search by tag finds inactive objects

- **The filter** 0x0067FEC0 (8 arguments, `ret 0x20`) starts with `if (!IsActive && tag.size == 0) return 0` (0x0067FED0–0x0067FEE6, checked). Then it tests the type mask (bit type − 1), the tag's equality when one is given (obj +0xB8), the group (-1 any, obj +0x3C0), and three optional flags: a check against the source (inferred: hostility), not dead, perceived (source +0xC18).
- **The search** is 0x006849C0 (address library `ScriptBackend::GetNearestObjects`, from fe62e8f), over the area's list at +0x690. Its only filter calls are 0x00684B2D and 0x00684C4F.
- **So** `GetNearestObjectByTag` (435) and `UT_GetNearestObjectByTag` (360, 0x0066DB20 → 0x0066DC0A) return an inactive object whose tag matches. `GetNearestObject` (433), `…ByGroup` (436), `…ByHostility` (127), and a tag of "", return active objects only.
- 0x00671510 is one handler for 127, 433, 434, 435 and 436; only 435 reads a tag (0x0067163C).
- Contrast: a conversation line's speaker and listener look up by tag through 0x006973B0 with inactive objects left out (`dev/PLOTS.md`).

## On an area transition the whole party pool goes, inactive

The party (`[app+0xD0]+0x58`) has:
- +0x234, the active party (`IsFollower` 0x00631830 asks only it);
- +0x244, the party pool (`GetPartyPoolList` 0x00672AC0);
- +0x2A8, a map of creature id to follower state: an `stdext::hash_map<id, FollowerInfo>` (MSVC 8's `_Hash`), its value the state +0, substate +4, locked +8 and follow-the-leader +9 (`dev/script_commands/Party.md`). Its find, `0x006FE370`, hashes the id as itself, masks it by +0x20 and halves it when at or past the max index +0x24 (linear hashing), indexes the bucket vector at +0x14..+0x18 (8-byte iterator pairs) and walks the list at +4; the count is the map's +0xC (party+0x2B4), which `GetFollowerState` (`0x00632810`) tests first (checked 2026-10-18 by lib-agent-2).

**The arrival** (0x006C73C0, the loading states' step 14, and a save's load) walks the pool, +0x244 (0x006C7410, checked), with no test of state, active flag or hired flag:
- **In the active party** (0x006C7574): placed in the formation (`dev/LOAD_EVENTS.md`), the leader on the waypoint.
- **Otherwise:** slot +0x140(0), SetActive(false) (0x006C75FC, checked), and the creature keeps its own coordinates (+0x120). Being inactive, it skips the `GetSafeLocation` snap.
- **Then everyone** is added to the new area (slot +0xF8, 0x006C767D), which takes it out of the old area. The creature's +0xB9C list goes with it (inferred: summons).
- On a save's load (loading byte +0x68) everyone keeps their position, and nobody is set inactive.

**A list swap** (state 4) takes every pool member out of its area at step 1 (0x006C8720), before the old list's areas are saved (steps 3, 4) and deleted (step 5). The area's save skips pool creatures (0x00683F0D), and when the area is loaded again a saved creature in the pool is not added (0x00683AB7). Creatures outside the pool are deleted with their area and come back from its save. In a move within one list, the old area and its creatures stay in memory.

**So every follower state but INVALID is carried:** ACTIVE (1) and LOCKEDACTIVE (7) in formation, active; AVAILABLE (2), UNAVAILABLE (3) and SUSPENDED (5) made inactive, at their old coordinates. The object's active flag plays no part. The scripts rely on this. `Camp_PlaceFollowersInCamp` (PRELOADEXIT) activates recruited followers found with `Party_GetFollowerByTag`, which walks `GetPartyPoolList`. `bhm100tr_jowan_caught` does the same for Jowan and Lily, UNAVAILABLE since `bhm000pt_tranquility`.

## SetFollowerState and RemoveFromParty

`SetFollowerState` (706, 0x0065E6E0 → 0x0063B9A0, a switch, table 0x0063BA0C):

| State | What it does |
|---|---|
| 0 INVALID | out of +0x234 (0x006377E0), then 0x0063B8B0: out of the pool and **its map entry erased** (0x0063A1D0, the map's erase) |
| 1 ACTIVE, 7 LOCKEDACTIVE | 0x0063A020: into the pool if missing and the state stored; then 0x00637560 into +0x234, activating it if inactive (slot +0x140(1)) |
| 2 AVAILABLE, 3 UNAVAILABLE, 5 SUSPENDED | out of +0x234, then 0x0063A020 |
| 4, 6 LOADING | nothing |

- 0x006377E0 refuses to take out the sole active member (unforced), and then nothing else happens. 0x0063B8B0 refuses when the pool has one member or fewer, or for the hero (+0x7D4 = 1); the creature then has left +0x234 but keeps its entry.
- **`RemoveFromParty` (397, 0x0065E5C0) is `SetFollowerState(creature, 0)`** (0x0065E5FF–0x0065E602, checked): the creature leaves the pool and `GetFollowerState` (399, 0x00632810) answers 0, INVALID, for a missing entry. `AddToParty` (396, 0x0065E560) is `SetFollowerState(creature, 1)`, its second object ignored.
- **The scripts never call 396–398**: `script.ldf` comments them out as "DEPRECATED, use SetFollowerState()". A follower leaves the party as AVAILABLE (`UT_FireFollower`, `utility_h.nss`:861), and INVALID only when it leaves the pool too.

### A member's group: GROUP_PC on joining, kept on leaving (read 2026-10-18 by lib-agent-2)

Read in the GOG exe on 2026-10-18 for paragon-agent-2 (Paragon #68). Everything here is checked in the disassembly.

- **CCParty+0x1E4 is the group a joining member gets, and it is always 0 (GROUP_PC).** The CCParty constructor 0x0063A3C0 (vftable 0x00B05E84) zeroes ebx at 0x0063A407 and stores it at 0x0063A904. A scan of the whole .text for writes to `[reg+0x1E4]` finds 20, and this is the only one in the party code (0x00628000–0x00640000). The other 19 are in other classes: CClientExoApp's constructor, NxSceneDesc::load and others. Nothing loads the field from a save and no script command sets it.
- **Joining (SetFollowerState 0x0063B9A0, states 1 and 7):** 0x0063A020 runs, then 0x00637560. If the creature is not already in the party's list at +0x234 or +0x260, 0x00637560 does two things. It copies CCParty+0x1E0 into creature+0xBDC; the constructor sets +0x1E0 at 0x0063A966. Then it calls the creature's vtable +0x170 with CCParty+0x1E4, at 0x006375D2.
- **What vtable +0x170 is:** on CCCreature, slot 92 of vftable 0x00B10484 is 0x006B8450. It writes creature+0x3C0 (GetGroupId), and when the value changes it calls 0x006B8250. The script command SETGROUPID 0x0065BB00 calls the same slot. The group therefore changes only through that command and party joining.
- **Leaving (states 0, 2, 3 and 5):** these go through 0x006377E0, the removal from the party. It sets creature+0xBDC to −1 at 0x006378A1 and never calls vtable +0x170 or writes +0x3C0. Neither do 0x0063A020 or 0x0063B8B0, so the group stays 0 until a script sets it.
- **Inferred:**
  - That a script restores a member's group after leaving: the leave path doesn't, so any restore would have to be scripted.
  - That 0x006B8250 re-sorts the creature's group lists: not read.
- **Not read:**
  - What +0x1E0 and creature+0xBDC mean. The constructor's +0x1E0 comes from a stack slot, [esp+0x80] at 0x0063A94C.
  - The callees 0x006368F0, 0x00635B40, 0x00631F30 and 0x00695220 on the leave path, beyond the direct calls listed above.

## CreateObject then a hire, in one script

Read 2026-10-18 by lib-agent-2 (with a subagent) for paragon-agent-2's Circle basement, not seen in play. The queue's order and the script run at delivery are `dev/LOAD_EVENTS.md`'s "The event queue's gate and delivery".

`zz_bhm000pt_debug.nss`:116–125 makes Jowan and Lily with `CreateObject` and hires each at once (`UT_HireFollower` → `WR_SetFollowerState` → `SetEventScript(player_core)`, `wrappers_h.nss`:353; `PARTY_MEMBER_HIRED` signalled, :376).
- The queue then holds SPAWN, then HIRED, both for player_core.
- player_core's SPAWN (`player_core.nss`:498–507) calls `AS_InitCreature(self)` with level −1, and `AS_AddClassLevels` returns at once (`sys_autoscale_h.nss`:266), so no class is set.
- The template's `PropertyList` (class 2) isn't read by the exe (`dev/PROPERTIES.md`). So HIRED's auto-level finds no class (`sys_autolevelup_h.nss`:66, "Player character has no class…").
- Vanilla's debug path is broken the same way. The real path (`bhm000pt_tranquility.nss`:157–165) hires the Jowan placed in the tower, whose creature_core SPAWN ran at the area's load.

## Following while exploring: who drives it, the walking formation, when it stops

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe and the toolset scripts, for DAOParagonEngine #34. Not seen in play. "Checked" = read in the disassembly; "inferred" = a reading of purpose. Addresses are GOG's. Offsets on "the party" are on `[[app+0xD0]+0x58]` (`0x005D57C0`); "the leader" in this note is the **controlled creature** (`0x005D5CA0` = `[[app+0xD0]+0x5C]`), not the party leader at party+0x1E8.

### 1. Who drives it: the engine, natively

**The engine, not the scripts.** Out of combat no script moves a follower. `ai_main_h_2.nss`:750–757 says so in so many words: a non-controlled follower not in combat state returns from `AI_DetermineCombatRound` with *"can't put here any wait or move commands as they will conflict with the engine follow commands, especially if the user selected the GUI option to disable party following."*

**The chain** (checked):
- `CCCreature::Update` (around `0x006BE4C0`): if this creature is the controlled one (id == `0x005D5CA0()`), it calls the party's update `0x00630F60(dt)` (`0x006BE876`). dt is the creature's timer ms × 0.001 (`0xAE7118`). So the party is updated once per frame, from the controlled creature's update. If the controlled creature's update doesn't run, nobody follows (inferred: paused, inactive).
- **Party update `0x00630F60`** (ID 12304, no name):
  - It adds dt to party+0x224 (the follow accumulator) and to party+0x228 (the time since the last side flip, §2).
  - When +0x224 > 0.5 (`0xB05774`), it zeroes +0x224. Then, if the active party (+0x234) has more than one member or the non-party list (+0x260) has at least one, it calls the follow update `0x00760DC0(party)` on the follow object at party+0x22C (checked `0x00630F8B`–`0x00630FF3`).
  - **So the follow is re-planned every 0.5 s**, on the first frame past it.
- **The follow object** at party+0x22C is 0x2C bytes: eleven floats, one per slot. They are filled by `0x007607F0` (called from the party ctor `0x0063A941`), each the length of the slot's offset in the walking table (`0x00760520`, x and y only). Past slot 10 the length is 5.0 (`0xAE7308`). It has no vtable; its methods are `__thiscall` on the float array.

**The follow update `0x00760DC0`** (ID 18155, checked), each 0.5 s:
1. The leader is the controlled creature. It returns if there is none, or if the leader answers vtable +0x1D0 (dead, inferred).
2. `0x00760C60` zeroes +0xB0C of every member of +0x234 and +0x260 (the catch-up value, below).
3. **The game mode must be 0 or 1** (`[[app+0x114]+0x74]+0x5C` via `0x00640060`: GM_EXPLORE 0, GM_COMBAT 1). Any other mode (dialog 4, cutscene 5, GUI 3, …) returns.
4. Then for each id in the active party (+0x234), then in the non-party followers (+0x260), with a creature, its +0x7B0 record, and not the leader:
   - `slot = 0x00631D90(follower, leader)` (§2).
   - **Skip if `GetFollowPartyLeader(follower)` (`0x00632A80`, FollowerInfo +9) is false.**
   - **Skip if party+0xA4 is set.** This is the party's SmartData bool attribute **`HoldPositions`**: it is registered under that name (`0x0063B3DB`, string `0x00B05FB8`), saved as field 0x3FA5 (`0x006347B8`), and `CCParty`'s `ToggleHoldPartyMessage` handler `0x0062F1F0` flips it. That is the UI's Hold Position, party-wide.
   - **Skip unless `ShouldMove = 0x00760980(follower, leader, slot)`** (below) **and slot < 11.**
   - `spot = WouldBeLocation(follower, leader, slot, walking)` (`0x00760810`, §2).
   - **Skip if the spot is within 1.0 m of the follower** (`0x0067A000`, 2D; `0xADDC94` = 1.0).
   - **If the leader perceives the follower** (`0x00760940`: the follower's id is in the leader's perception set +0xC18), the follower walks there (step 5).
   - **Otherwise it is teleported** (§2, "Far behind").
5. The walk (checked `0x00761018`–`0x007610D5`):
   - `0x00760460` sets the catch-up value (below).
   - **If the follower's current command is `COMMAND_TYPE_MOVE_TO_LOCATION` (20)**, that command is retargeted in place:
     - vectors 0 and 1 = spot;
     - flag 3 = 1, flag 1 (run) = 1, flag 6 = 0;
     - via `CDataArrays` `0xA5A9B0`/`0xA5B890`.
   - **Otherwise**, `RemoveCommand(20)` (`0x0073C9E0`) is run first. Then `0x0069EF30` builds a fresh MoveToLocation and queues it with `CCObject::QueueNewCommand` (`0x0073D000`). Its parameters:
     - location 0 = spot;
     - flag 1 = 1, **always run**;
     - float 0 = 1e-6 (`0xB1CE64`);
     - flag 6 = 0;
     - flag 12 = 1.

**`ShouldMove`, `0x00760980`** (ID 18153, checked). It returns false unless the distance test passes:
- **The reference point** is the leader's position (+0x120). It is the leader's path destination when all three hold (inferred: the destination):
  - the leader's MovementController (+0xA40) holds a path of more than one 12-byte point (`[mc+0x14]` vector at +0x74..+0x78);
  - the leader's byte +0xB24 is 0;
  - the leader's +0xB20 is neither 0 nor 4 (+0xB20 is what `IsMoving` reads, `!= 0`).

  In that case the point is `mc+0x30`.
- **The distance** is 2D (x, y), from the follower to that point.
- **The threshold** is `PerSpace(follower)` (vtable +0x9C, `apr_base` PerSpace, 0.35 for humanoids) + `slotLength[slot]` + 2.0 (`0xC0D230`). Add 2.0 more (`0xAE7108`) when the leader's +0xB20 is 0 or 4 (standing).
- **The follower moves only when the distance ≥ the threshold.** For a humanoid in slot 0 (length 1.80): **4.15 m** from the leader's destination while the leader moves, **6.15 m** from a standing leader.

  | slot | 0, 1 | 2 | 3, 4 | 5, 6 | 7, 8 | 9, 10 |
  |---|---|---|---|---|---|---|
  | length (m) | 1.803 | 2.5 | 4.272 | 3.606 | 3.162 | 3.640 |

- **Then, with flag = 1.** The command type is the current command's, or the first queued one's when there is none (`0x0073C600`, else +0x2CC's first, byte +2).
  - It returns false at once if the leader's command is DEATHBLOW (38).
  - Flag becomes 0 if any of these holds:
    - the follower is in another area (+0x254);
    - the follower is in the active party (`IsFollower`) with a state other than ACTIVE 1 or LOCKEDACTIVE 7;
    - the follower is dead (+0x1D0);
    - the follower's combat data (+0xC60) has state ≠ 0 or its +4 ≠ -1 (inferred: an attack target).
  - When none of those holds, the command type decides:
    - type 0 (idle): return true;
    - type 20 with flag 6 == 0 (a move the follow itself or a script gave): return true;
    - type 14 (PLAY_ANIMATION): return true;
    - anything else: flag = 0.
- **When flag is 0**, true is returned only when all of these hold:
  - neither the leader nor the follower is in combat (`0x006A2580`: combat state == 1);
  - the follower is not dead;
  - **the leader does not perceive the follower.**

  That case is the teleport.
- **Flag 6 is the player's order.** The player's click-move sets it to 1 (`0x00638A86` → `0x006ADF70(…, flag6 = 1, …)`). So a follower the player ordered keeps its order, unless it is out of perception (then it is jumped).
  - `CommandMoveToLocation` (559, `0x0066F080`) sets flags 1 and 10 only, so a scripted move on a follower has flag 6 = 0. While the follow flag is on and the distance test passes, the follow retargets it (checked logic; the effect is inferred).
  - This is why plot scripts call `SetFollowPartyLeader(…, FALSE)` before scripted moves, e.g. `pre100pt_light_beacon.nss`:599 and `cli600tr_army_stop_follow.nss`:31.

**The follow-the-leader byte, FollowerInfo +9** (checked):
- **Readers.** `0x00632A80` is read by the follow update (`0x00760F7A`) and by the script handler (768).
- **No entry.** A creature with no map entry answers **1**, so non-party followers always follow.
- **Default.** A new map entry is made with follow = 1 and locked = 0 (`0x0063A0D5`/`0x0063A0D9`).
- **Saved.** It is in the save as field 0x3F9A (default 1, `0x006BEC6D`–`0x006BEC89`); substate is 0x3F98, locked 0x3F99.
- **Writers in the scripts.** `area_core` PRELOADEXIT sets TRUE for the whole pool on every area load (`area_core.nss`:108). So does a hire (`wrappers_h.nss`:370). `Party_SetFollowLeader` (`core_h.nss`:2047) and plot scripts write it too.

**The catch-up value, creature +0xB0C** (`0x00760460`, checked):
- It is computed for a follower with combat state 0.
- With d the 3D distance to the leader: if d > 2.0 and `0x007C4D20(follower, 0, 0, 0)` is false (not read), then +0xB0C = (d − 2.0) / 2.5.
- It is reset to 0 for all members every 0.5 s before the update.
- **No reader was found.** A displacement scan of .text, at the creature's base offsets too, found only these two writers. Treat it as unused, or read through a path not found.

### 2. The walking formation

**The tables.** Each is 11 slots of (x, y, z, 0), 16 bytes, (x right, y forward per `dev/LOAD_EVENTS.md`) (checked, dumped):

| slot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| walk `0xC0D240` | (-1.5,-1) | (1.5,-1) | (0,-2.5) | (-1.5,-4) | (1.5,-4) | (-3,-2) | (3,-2) | (-1,-3) | (1,-3) | (-3.5,-1) | (3.5,-1) |
| arrival/jump `0xC0D2F0` | (-1,-0.5) | (1,-0.5) | (0,-1) | (-2,-1) | (2,-1) | (-3,-1) | (3,-1) | (-1,-2) | (1,-2) | (-3,-2) | (3,-2) |

There are three more tables between the two, progressively tighter: `0xC0D3A0` (-1.38,-0.88)…, `0xC0D450` (-1.25,-0.75)…, `0xC0D500` (-1.13,-0.63)….

**Which table** (`0x007605B0`, ID 18149, checked):
- Its last argument ("arrival") chooses `0xC0D2F0`.
- Otherwise the global dword `0xC371B0` chooses: 1 → `0xC0D3A0`, 2 → `0xC0D450`, 3 → `0xC0D500`, 4 → `0xC0D2F0`, anything else → `0xC0D240`.
- `0xC371B0` has no writer: its only reference in the exe is the read at `0x0076061B`, and it is 0 in the file. So the walk always uses `0xC0D240` (inferred: a leftover formation-tightness option).

**The slot** (`0x00631D90(follower, leader)`, checked). With i the follower's index in the active party +0x234 and L the leader's:
- i == 0 → L − 1 (the follower at index 0 takes the leader's slot);
- i > 0 → i − 1;
- not in +0x234 → `count(+0x234) − 1 + j`, where j is its index in the non-party list +0x260 (−1 if in neither).
- **So slots go by party order, the controlled creature skipped, and non-party followers come after the party.** Slot ≥ 11 does not walk-follow (`0x00760FB5`).
- Note: `ShouldMove` indexes the 11-float length array with the slot before that bound is tested, so slot ≥ 11 reads past it (checked; harmless only by luck).

**Left and right.** The party byte +0x450 (1 from the ctor, `0x0063A877`) swaps the sides when 0: 0↔1, 3↔4, 5↔6, 7↔8, 9↔10 (`0x007605E2`–`0x00760611`). It is flipped by `0x0062EB50` (checked), when party+0x228 (the time since the last flip) is over 0.26 s (`0xC1CED0`). The flip also sets the follow accumulator +0x224 to −0.5, so the next re-plan waits 1.0 s, and zeroes +0x228. It is called:
- when the steering (WASD drive, `0x00763805` → `0x006ADCF0`) turns the heading more than 90° (`0xB00618`) from the leader's facing;
- when the player clicks a move point behind the leader (dot of facing and direction ≤ 0, `0x00638B0D`–`0x00638B25`).

**The would-be location** (`0x00760810(out, follower, leader, slot, arrival)`, ID 18151, checked):
- **Slot ≤ 10:**
  - The origin is the leader's position (+0x120).
  - The direction is the leader's +0xAB0 (inferred: unit movement direction) when its length is ≥ 0.99 (`0xB1CE68`). Otherwise it is the leader's facing (+0x130).
  - So **while the leader moves, the formation is turned by its movement direction; when it stands, by its facing.**
  - `0xA38A40` turns the slot's offset by the direction and adds it to the origin.
- **Slot > 10:** a safe location (`CCArea::GetSafeLocation` `0x0073E370`) within 4.0 m (`0xADEF1C`) of the leader.
- **Cleanup, walking:** if the spot is not free (`MovementController::IsFree` `0xA62A90`, the follower's own controller), it moves to the nearest free cell within 2.0 m back toward the leader (`0x0073E7F0`). There is no reachability test and no ground snap.
- **Cleanup, arrival:** additionally a straight-walk test to the leader (`0xA626F0`). Failing it, `GetSafeLocation` around the leader within 5.0 m, starting from both personal spaces summed (`dev/LOAD_EVENTS.md`).
- `GetFollowerWouldBeLocation` (`0x00632640` → `0x00760810(…, arrival = 0)`) is the walking answer.

**Re-planning and stopping** (checked, except where marked):
- **Each 0.5 s**, for each follower past its threshold, the spot is recomputed and the follower's own move command is retargeted in place (one command, not a new one each time).
- **The drive forces a re-plan.** The drive sub-action 16 (`0x007A8C20`) sets +0x224 to 0.5 (`0x0062EB90`, from `0x007A8E1F` and `0x007A8F6D`), so the next frame re-plans (inferred: when the steering starts or changes).
- **When the follower comes inside its threshold,** the follow stops issuing; the move it has keeps running to its last spot and ends there.
- **The 1.0 m test** keeps a follower already near its spot from being re-issued.
- **Walk or run: always run.** Flag 1 = 1, both new and retargeted. No read of the leader's walk/run state was found in the follow code. Party+0x4A9 is read in the party update, but for a notification, not movement.
- **No speed matching** was found, apart from the unread +0xB0C.

**Far behind: the jump, not a catch-up run.** When `ShouldMove` holds and the leader doesn't perceive the follower (`0x007610D7`–`0x00761133`, checked):
1. The would-be location is computed with the **arrival table** `0xC0D2F0` and its full cleanup (free cell, straight walk, safe location).
2. `ClearAllCommands(hard)` (`0x0073C930(1, 0)`).
3. `SetOrientation` (`0x00694CE0`) to the leader's facing (+0x130).
4. `0x0073D080(spot, leader's area +0x254, 1, 1, 0)`, which queues `COMMAND_TYPE_JUMP_TO_LOCATION` (10) into the leader's area.

When is a follower unperceived? The perception rules are `dev/COMBAT.md`'s:
- a party member is non-hostile, so it is perceived with no line-of-sight test within the leader's range (`PercepRngPlayer` 35 m, or the party's range at party+0x42C), and kept to 60 m;
- perception is lost in another area, for an inactive object, or by the stealth-like flag `0x006A44D0`.

**So in practice: a follower over 60 m off, or in another area, is jumped to its formation slot behind the leader within 0.5 s** (inferred from the two readings). This holds unless someone is in combat, the follower is dead, its follow byte is off, or Hold is on.

### 3. Stops: leader standing, combat, conversations, non-party followers

**Leader standing:** the threshold grows by 2.0 m (6.15 m for slot 0), and the formation turns by the leader's facing, not its motion (§1–2).

**Hold Position:** party `HoldPositions` (+0xA4) stops all follow and all jumps (checked). The per-follower byte +9 does the same for one creature.

**Combat** (checked, except where marked):
- The follow still runs in GM_COMBAT (mode 1).
- A follower whose combat state is set (+0xC60 +0 ≠ 0), or that holds a target (+4 ≠ −1), is skipped by `ShouldMove`. With anyone in combat, it is not jumped either.
- `rules_core` puts followers in combat state when hostiles are perceived (`rules_core.nss`:927–989, 1016–1037). From then on the AI drives them: every `COMMAND_COMPLETE` of a follower runs `AI_DetermineCombatRound` (`rules_core.nss`:777–796).
- The AI's own rejoin is `_AI_MoveToControlled` (`ai_main_h_2.nss`:3315–3346). It gives `CommandMoveToLocation(GetFollowerWouldBeLocation(self), TRUE)`, the walking table, when all hold:
  - the main controlled creature is not stealthy;
  - the last command succeeded;
  - the distance is > 3.5 m (`AI_RANGE_SHORT` = `AI_MELEE_RANGE`).

  With the AvoidNearbyEnemies behaviour, it moves only if no enemy is within 3.5 m of the leader. Otherwise it gives a wait (`_AI_DoNothing`, 1–2 s).
- It is used when the follower has no valid target (`ai_main_h_2.nss`:852–856, 892, 902, 928). On perception, a follower that fails `WR_TriggerPerception` is sent the same move (`rules_core.nss`:1022–1029).
- **The switch** is the creature's combat state: `SetCombatState`, the scripts' (inferred: cleared when combat ends, which hands the follower back to the engine follow).
- Idle out-of-combat followers keep following even while the leader is in combat: `ShouldMove` returns true for an idle follower before any leader-combat test.

**Conversations** (checked, except where marked):
- In GM_DIALOG (4) the follow update returns (mode test).
- `CCCreature::Update` skips the whole update of a creature for which `0x0068CC30(id)` holds in mode 4 (`0x006BE500`–`0x006BE51C`; inferred: the conversation's participants, frozen).
- Others keep updating but don't follow.
- **At the conversation's end,** `GameModeDialog`'s leave (`0x006910D0`, vtable slot 7) calls `0x006326C0` (`0x006911B2`). Unless `HoldPositions` is set, it puts every active member other than the controlled creature on its walking would-be location with `CCObject::SetPosition` (`0x00695540`), at once. It does so for a member for which `0x006A8D60(controlled, 0, 1, …)` is false (inferred: one beyond the "stays perceived" range of `dev/COMBAT.md`).
- Cutscenes (5), menus (3) and loading don't follow either.

**Non-party followers** (`AddNonPartyFollower` 351 → party+0x260) are followed the same way (checked):
- the same update walks +0x260 after +0x234;
- their slots come after the party's;
- `GetFollowPartyLeader` answers 1 with no map entry, and `SetFollowPartyLeader` can't change that (it writes only an existing entry);
- the follower-state test is skipped (`IsFollower` false);
- they are jumped when unperceived, like party members.
- `HoldPositions` stops them too. Their presence alone (a single-member party with one non-party follower) is enough to run the update.

### Addresses (GOG; IDs from `tools/address_ids.py`)

| What | Address | ID |
|---|---|---|
| party update (dt), the 0.5 s accumulator | 0x00630F60 | 12304 |
| follow update | 0x00760DC0 | 18155 |
| ShouldMove(follower, leader, slot) | 0x00760980 | 18153 |
| WouldBeLocation(out, follower, leader, slot, arrival) | 0x00760810 | 18151 |
| slot offset → spot (table choice, side swap, IsFree) | 0x007605B0 | 18149 |
| slot length / follow-object ctor | 0x00760520 / 0x007607F0 | |
| reset +0xB0C / set +0xB0C | 0x00760C60 / 0x00760460 | |
| leader perceives follower (+0xC18) | 0x00760940 | |
| slot from party order | 0x00631D90 | |
| formation side flip / force re-plan | 0x0062EB50 / 0x0062EB90 | |
| ToggleHoldPartyMessage (CCParty) | 0x0062F1F0 | |
| post-dialog placement | 0x006326C0 | |
| follow move builder (MoveToLocation, run) | 0x0069EF30 | |
| player click move (flag 6 = 1) | 0x006ADF70 | |
| tables: walk / arrival / three tighter | 0xC0D240 / 0xC0D2F0 / 0xC0D3A0, 0xC0D450, 0xC0D500 | |
| table selector (never written) | 0xC371B0 | |

### The 60 m test is horizontal

`0x0067A000`'s distance is 2D: x and y, the third term `fldz; fmul st0,st0` (checked `0x0067A02D`). So the 60 m "stays perceived" test is horizontal. `dev/COMBAT.md` had it "3D inferred", corrected the same day.

## Control and selection, as the engine runs it

Read 2026-10-18 by lib-agent-2 (with a forked subagent for part 2) in GOG's `DAOrigins.exe` 1.05 (capstone disassembly through `tools\daoexe.py` and a scratch caller scan) and the toolset's scripts. Not seen in play. **Checked** = read in the disassembly; **inferred** = from names, shapes or scripts, not traced to the end. Builds on the sections above, `dev/script_commands/Party.md`, `dev/script_commands/spells_combat.md`, `src/dao/Party.cpp` and `Addresses.h`'s `Party::Select` row.

### Three words: selected, controlled, leader

| | Where | Read by | Set by |
|---|---|---|---|
| **Selected** | `CCParty`+0x184, a `std::vector<id>` in the order selected; +0x194 the same as creature pointers, a SmartData list whose change notifies observers | `IsControlled` (714): **any** selected member answers TRUE (`std::vector::Contains`, 0x006316D0, checked) | `0x006368F0` (replace), `Party::Select` 0x00639E50 (add), `0x00639DB0` (remove); part 2 |
| **Controlled** ("main controlled") | `CCPlayer`+0x5C, the id, beside the party at `CCPlayer`+0x58; `CCPlayer` is `[app+0xD0]` (vftable 0x00B0F1D8 names it; its constructor 0x0069D600 writes +0x58 and +0x5C = -1, checked) | `GetMainControlled`, `0x005D5CA0`; the camera, the formation's leader on arrival, the AI's follow distance | only `CCPlayer::SetControlled` 0x0069D540 (checked: the one write of +0x5C in `CCPlayer`'s code), and it is always **the first selected**, below |
| **Leader** | `CCParty`+0x1E8, a SmartData attribute | `GetPartyLeader` (624); perception's party range (0x006B85A7, checked: it is the leader attribute, which `dev/COMBAT.md`'s "Range, line of sight" had as "the controlled creature", corrected the same day); the active party's order | `SetPartyLeader` (625, 0x00636180), and the first member to join an empty active party (0x006376A3, checked) |

- **Control follows the selection's first entry.** Nothing sets the controlled id directly from a click or a script; it is derived (checked, below).
- **The leader does not follow control.** Switching control leaves +0x1E8 alone (0x006368F0 only compares it, 0x00636B25, checked). `SetPartyLeader` does not change control (`dev/script_commands/Party.md`, checked). So after F2 the leader is still the hero unless a script moves it.
- **`IsControlled` is "selected".** With the whole party selected, every member answers TRUE, so each runs the scripts' partial AI (below), not its tactics.

### Switching control: the engine's path

#### The one entry: `CCParty::SetSelection(id, bool click, int)` 0x006368F0 (`__thiscall`, `ret 0xC`; address library ID 12442, no name)

Checked, step by step:
1. **Already so:** if the selection is exactly one id and it is `id`, return false (0x0063694A–0x00636982).
2. **The old first selected** (if any): for each object in its +0xBA8 list of type 8 (`0x005D5AA0` casts by type byte 8; the 123-slot vtables of that size are the `CCTrigger` family, so inferred: the triggers it stands in), call slot 0x1E8(1) (inferred: "the controlled creature left"). If the old first is not `id`:
   - its steering object (+0x764) is reset (0x00763830: clears +0x3C and, if the creature is driving by keys, +0xB24 = 1, stops it through the movement controller's state 3, 0x00A63800);
   - its look-at is reset, 0x0069EAC0(0.0, 0.0) → 0x0069E9D0's controller (inferred: the head-tracking).
   - **No command is cleared:** no `ClearAllCommands` or command call anywhere in 0x006368F0 or 0x00637220 (checked: their calls listed). The old creature keeps its queue.
3. **The selection is emptied** (0x005198D0(0) on +0x184; inferred: a resize to 0).
4. If `id` ≠ -1 and is a creature: push it (0x006F1340), tell its +0xBA8 triggers slot 0x1E4(1), set a flag, and refresh its input modes (0x0069F200: drops `Effect_DisableInput` and `Effect_DisableMovement` and adds them again from the creature's effect flags, through `InputModes::Get`; strings at 0x00B0F358 and 0x00B0F328, checked).
5. **The tutorial's hook:** if `id` is not the leader (+0x1E8) and the module has a training object (+0x498), 0x00693290(0x424, 0): when the training session waits for event 1060 (0x424), a `CScriptEvent` of that type goes to `genev_tutorial` (string 0x00B0E864) on the controlled creature (checked). No other script event is raised here.
6. **The pointer list is rebuilt:** 0x00635A90(+0x194, +0x184, 0): +0x194 locked (0x00403C70), cleared, refilled from the ids, and its slot 0x24 run, the unlock that notifies its observers (checked; that slot 0x24 notifies is `Party.md`'s reading of the same pattern on +0x300). The party itself observes it: **step 8 runs from here.**
7. **The follow timers:** if the party's +0x22C exists, 0x00760C60(party) zeroes the float +0xB0C of every active member (+0x234) and every non-party follower (+0x260) (checked; inferred: their follow timer, read by the follow update 0x00760DC0, part 2).
8. If something was selected and `click` is 1, a `Click` sound (string 0x00B05DAC, through 0x00510CA0(0x2B, …)) (checked; "sound" inferred from the name).
- The third argument is not read (checked: the frame's +0x54 never appears).

#### The observer: `CCParty::OnSelectionChanged` 0x00637220

Reached from `CCParty`'s `Messages::Observer` base (+0x20): its vftable 0x00B05E58 slot 0 is the thunk 0x00639F80 (`this − 0x20`, then 0x00637220) (checked). Also called by `CCCreature` slot 86 (0x006A8AC0, the PC re-added as ACTIVE, `dev/COMBAT.md`). Checked:
1. 0x00634260(+0x184, +0x194): the two kept in step.
2. Each **selected** creature: its room refreshed (0x00695D70) and its attribute +0xB70 set to 1 (slot 0x54, the SmartData `Set` the leader uses too). Each **active, not selected** member (0x006353D0: +0x234 less +0x184): +0xB70 set to 0. Inferred: +0xB70 is the "selected" flag the selection circles and portraits read.
3. **The controlled id = the first of +0x194, or -1 if empty** (0x00637329–0x0063736A), handed to `CCParty::SetControlled` 0x0062F260:
   - `CCPlayer::SetControlled` 0x0069D540: writes `CCPlayer`+0x5C (0x0069D58E). Then **slot 0x154 on the old and the new controlled**, `CCCreature::UpdateAILevel` 0x006A53E0 (below). If their areas differ, the new area 0x006880E0(1) and the old 0x006880E0(0) (inferred: which area is current).
   - A `FocusCameraOnObject` message (vftable 0x00B05784) with the id, sent to the game mode controller (`app+0x114`). Only `GameModeExplore` (and so `GameModeCombat`, its subclass) handles it: 0x0070B9B0, `CameraControllerBase::SetTarget(id)` on the controller's +0x60 (checked). `src/dao/Camera.h`: within 15 m the camera eases to the new target over max(0.3, 0.6 × (1 − d/15)) s, beyond it cuts; in the tactical view it keeps its place and the offset eases out. In dialog, cutscene and other modes nothing handles the message (inferred: no handler registered in `dev/message_handlers.csv`).
   - The party's own slot 25 (+0x64): a bare `ret` (0x005BDCF0, checked).
4. If paused (the app's +0xD8 byte +0xC) in explore or combat (mode 0 or 1), 0x006C9A50(party, 0.0, 1) on `app+0x11C` (inferred: one refresh of the party's view while paused).
5. 0x00635B60: the party bar's four slots rebuilt (`Party.md`, inferred).
6. The data tree's `SelectedChar` token (string 0x00B039B4) set to the controlled id (checked) — what the Flash HUD reads.
7. The GUI system (`app+0x158`, 0x0060C830): its +0x15C attribute set to the id and the creature's stats refreshed (0x0071D4A0, 0x00719AB0, 0x0071E300) (checked; inferred: the character screens follow control).

#### The AI and the controlled creature

**The engine does not switch the AI off.** It changes the creature's AI level only (checked):
- `CCCreature::UpdateAILevel` 0x006A53E0 (`CCCreature` vtable 0x00B10484 slot 85, +0x154): **4** (`CSERVERAIMASTER_AI_LEVEL_VERY_HIGH`) for the controlled creature (compared with `GetMainControlled`); else **2** if +0xC60 holds something; else **3** if `CCParty::IsInParty` (0x00631830); else, if the creature's +0x254 equals the controlled creature's (0x005D5CD0), 0x007C49B0(self, 1); then `AIMaster`'s set (0x0064B480 on `app+0x1AC`).
- `AIMaster` 0x0064B480: ignored while a script's `SetAILevel` (+0x2B4) is set; -1 takes it out (0x0064B240); 0–4 moves it to one of five lists of 0x14 bytes (0x0064B400) and stores it at +0x2B0 through slot +0x150. What the lists change in `AIMaster::Update` 0x0064AF00 was not read (inferred, from Aurora: how often it is updated, not whether).
- Run on: control changing (0x0069D540, old and new), a member leaving the active party (0x006378F8), and elsewhere.

**"Controlled → no AI" is the scripts'**, in `ai_main_h_2.nss`, through `IsControlled` (so for every selected member):
- `rules_core.nss`:700–800, `EVENT_TYPE_COMMAND_COMPLETE`: for any follower, controlled or not, `AI_DetermineCombatRound` runs (:777–797).
- `AI_DetermineCombatRound` (`ai_main_h_2.nss`:487) — a follower with party AI off (`IsPartyAIEnabled`) gets `_Partial`; :632 **a controlled creature returns at once out of combat, and in combat gets `AI_DetermineCombatRound_Partial`**; everyone else gets the tactics.
- `AI_DetermineCombatRound_Partial` (:959): attack the selected target (`GetAttackTarget`) or the last target when it is in melee reach with a melee weapon, or in range with a ranged one after an attack; **for the controlled creature nothing else** (:1022, it returns with no command), so it stands until the player orders it.
- `rules_core.nss`:662–683, `EVENT_TYPE_ATTACKED`: a follower in combat with no current command runs `AI_DetermineCombatRound(attacker)`, so a controlled creature that is struck while idle attacks back.
- Elsewhere `IsControlled` gates: avoiding hostile AoEs (`ai_main_h_2`:422), the follower's hidden range tactics (:507), the path-blocked action (`rules_core`:758, allowed only for the controlled or a non-follower), the perception reaction (`rules_core`:951, 1073, 1109).

**The previous creature returning to AI:** nothing in the switch queues a command or raises an event for it (checked: the calls of 0x006368F0, 0x00637220, 0x0062F260, 0x0069D540). Its queue is kept; when its current command completes, `COMMAND_COMPLETE` (raised only at a command's end, 0x00743DD0 from 0x00746900, checked) reaches `rules_core`, `IsControlled` is now false, and the tactics run. With an empty queue it waits for its next event: being attacked (above), a perception; and the engine's follow (below, checked) walks it after the new controlled creature within half a second, since an idle member counts as needing to follow.

#### Events on a switch

- **`EVENT_TYPE_ON_SELECT` (89)** is raised only in `Party::Select` 0x00639E50 (0x00639F34, checked: the one place in the exe that writes 0x59 as an event type), not in 0x006368F0. So clicking a portrait or a member signals it (part 2); **F-key cycling (`SelectNext/PreviousCharacter`, 0x00636CD0 → 0x006368F0(id, 1, 1)), `SetPrimaryControlled` (0x006368F0(id, 0, 1)) and the engine's fallbacks do not.** `player_core.nss`:863 only plays the `SOUND_SITUATION_SELECTED` sound set entry.
- **No "controlled changed" script event exists** (checked: event types 66, 67, 89, 90, 94 and 36 written only where listed; no other constant names one). A port that wants one compares `GetMainControlled` each frame, as `dao::party::OnControlChanged` does.
- `EVENT_TYPE_PARTYMEMBER_ADDED` (66) / `DROPPED` (67) are the party picker's (0x00636F50, 0x00766AB0), not control's.
- The tutorial event 1060 to `genev_tutorial` (step 5 above).

#### Who calls 0x006368F0 (checked, every caller)

| Caller | Arguments | When |
|---|---|---|
| `Party::Select` 0x00639E50, add = 0 | (id, 1, 1), then ON_SELECT | a click on a portrait or a member, `CenterOnObject`, `SelectPartyMemberIndex` without the add byte; below |
| `CCParty::HandleMessage_ToggleFullPartySelect` 0x00636E40 | (first selected, 0, 1) | collapsing "all selected" back to one; below |
| `0x00636CD0`, `SelectNext/PreviousCharacterMessage` 0x00636E20 / 0x00636E30 | (id, 1, 1) | cycles the party bar's slots (the list at party+0xC8; inferred: the bar) from the controlled creature by ±1, skipping empty, not controllable (+0x7F9 = 0) and dead (slot 0x1D0) members (checked) |
| `ScriptCommand::SETCONTROLLABLE`'s handler 0x00661050 (815, `SetPrimaryControlled`) | (id, 0, 1) | `spells_combat.md`; it crashes on an invalid object |
| `0x00636C00` (from `SetControllable(false)` 0x006A5F40) | (id, 1, 1) | the first controllable (+0x7F9) living (slot 0x1D0 = 0) member of the active party (+0x234, its items at +0x238), else -1 |
| `0x00637560`, a member into the active party | (id, 0, 1) | only when the active party had been empty: that member becomes the leader too (0x006376A3–0x006376BB, checked) |
| `0x006377E0`, a member out of the active party | (first active, 0, 1) or (-1, 0, 1) | only when it was the controlled creature (0x0063784E–0x00637882, checked) |
| 0x00638560, from the client load's step 0x16 (0x006C76F0, `dev/LOAD_EVENTS.md`) | (party+0x208, 0, 1) | when party+0x208 is not -1 (checked; inferred: the controlled id kept across the load) |
| 0x006D7DE0, in `CClientVirtualMachineCommands::InitializeCommands`' table (0x006DBBBE) | (id, 0, 1) | a client-side `SetPrimaryControlled`: `SetControllable(1)`, then only for a creature that exists, is controllable and alive (checked) — unlike 815's handler, it does not crash on an invalid object. When the client VM runs it was not read |

The hero's creation also sets the controlled creature directly: `CCPlayer` 0x0069D910 (from 0x0069DF10, character creation's hero, and 0x0069DBB0) calls 0x0069D540 (checked: the call; the purpose of 0x0069DBB0 is inferred, a load).

### Selection: growing, shrinking, all

Read by the forked subagent; checked unless marked. +0x184 holds the ids; +0x194 mirrors them as creature pointers (every writer updates both; 0x00635A90 rebuilds +0x194), and each writer ends with +0x194's slot 0x24, which runs the observer 0x00637220. **So control is always the selection's first entry, whoever wrote it; appending never changes it** (0x0069D540 returns at once when the creature is the same, 0x0069D55A, checked).

- **`Party::Select` 0x00639E50**, `__thiscall (id, bool add)`, `ret 8`. Nothing unless the creature is controllable (+0x7F9) and alive (slot +0x1D0 = 0).
  - add = 0: 0x006368F0(id, 1, 1), replacing the selection, so it becomes controlled; on success **ON_SELECT (89)** runs at once through `RunEventScript` (0x00639F58), the id in both object slots.
  - add = 1, a toggle: already selected with others → deselected (0x00639DB0); else appended (0x006355A0(id, 1), to +0x184 and +0x194 if missing). No event; control unchanged.
- **Its callers:**
  - 0x00639F90 is `CCParty::HandleMessage_SelectCharacterMessage` (`dev/message_handlers.csv` has 0 for it, left for an issue of its own; built at 0x0063A2C1, vftable 0x00B058E0): `Select(msg+4, 0)`.
  - `SelectPartyMemberIndexMessage` 0x00639FB0: msg+4 an index into the party's list +0xC8 (inferred: the bar's order); `Select(id, msg byte +6)`, the byte the add flag (inferred: Shift).
  - `GameModeExplore::SelectPartyMemberUnderCursorMessage` 0x0070C0B0: `Select(explore+0xCC, msg byte +4)`.
  - The primary click 0x00712830: case "select" (0x00712D1E) `Select(target, 0)`; case "deselect" (0x00712D57) 0x00639DB0.
  - Explore mode passes SelectNext/Previous, Index, ToggleFullPartySelect and ToggleHoldParty on to the party's receiver at party+0x18 (0x0070B9E0).
- **Select all, `ToggleFullPartySelectMessage` 0x00636E40:** counts the living active members (+0x234; no controllable test in the count). All selected → collapse to the first selected (0x006368F0(first, 0, 1)); else 0x00635860 appends every unselected, controllable, living active member (0x006355A0, no event).
- **Next/previous character 0x00636CD0(±1):** the table above; it replaces the selection.
- **Deselect 0x00639DB0** (ID 12477): the only one selected → 0x00636CD0(1), the next character; else removed from +0x184 and +0x194. Also on death (`dev/COMBAT.md`) and `SetControllable(false)`.
- **Box selection:** inputs `SelectionModifier` 46, `DragSelect` 48, `InclusiveDragSelect` 49 (registered 0x00896B72–0x00896C4C).
  - The drag 0x00712450, per frame from 0x00712F90 (inferred: explore's update); its start at 0x00C36010/14, the dragging flag 0x00C35E75, the list 0x00C36018.
  - While dragging, 0x007121E0 rebuilds the list: living active members (+0x234) whose screen position is inside the rectangle, in the active party's order; with 49 held, those already selected are kept too. No controllable test.
  - Each candidate is highlighted (0x0070C900: creature+0x430, then 0x006965E0).
  - On release, a non-empty list goes to 0x00635630(list, 1): +0x184/+0x194 cleared and filled in the list's order, then +0x194's slot 0x24 (0x0063583A–0x00635849, checked). **No ON_SELECT; control passes to the list's first** through the observer.

### Orders to the selected

Read by the forked subagent; checked unless marked.
- **Move click:** `PreMoveCharacterToLocationMessage` 0x0070AAF0 sets 0x00C35E77; `MoveCharacterToLocationMessage` 0x0070C9A0 needs it (else a feedback message) and orders **only the controlled creature** (0x00694990 → 0x005D5CC0), alive, on walkable ground (explore+0xD4, the point at +0xD0).
  - `GameModeExplore::MoveTo` 0x0070BE80: the point kept at +0xA0; the creature's steering object (+0x764) 0x00764020; `Orders::OrderReceived` (event 90) to the controlled creature alone.
  - 0x00764020 ignores a repeat within 1000 ms and 0.01 (squared, 0x00AF7498); if the point is more than 0.1 m from the current move's destination, it calls **the party's group move 0x006389F0(point, run)**, run = party+0x4A8 (`ToggleRunWalk` 0x0062EC60).
- **The group move 0x006389F0:**
  - The movers: 0x006360C0(mask 0x81), **the selection (+0x194)** less creatures whose effects-holder flags (creature+0x70 → 0x00A64740) meet 0x81. Inferred: bits 0x80 and 0x01 are what 0x0069F200 turns into `Effect_DisableInput` and `Effect_DisableMovement`.
  - The controlled creature: 0x006ADF70(point, run, 0.0, 1, 0).
  - Each other mover: slot = 0x00631D90(member, controlled) (its active-party index less one; non-party followers after); spot = 0x007605B0(…, slot, …, arrival 0); 0x006ADF70(spot, run, 0.0, 1, delay), **delay = min(slot + 1, count) × 300 ms**, +300 ms when the controlled creature is farther from the point than the last mover, ÷5 when the controlled creature's +0xC60 word 0 is 1 (0x006A2580; meaning inferred).
  - A move against the leader's facing flips the left/right byte party+0x450 (0x0062EB50), at most once per 0.26 s (timer party+0x228).
  - 0x006ADF70: a current move (type 20, MoveToLocation) is retargeted in place; else 0x0069EF30 → `QueueNewCommand(20, …, front 0, static 0, playerIssued 1, override -1)` with vector 0 the spot, flag 1 run, float 0, **flag 6 = 1 (a player's group move)**, int 0 the delay.
  - **ON_ORDER_RECEIVED goes to the controlled creature only**, not to the members moved in formation.
- **The formation table:** 0x007605B0 picks by the global 0x00C371B0: 1 → 0x00C0D3A0, 2 → 0x00C0D450, 3 → 0x00C0D500, 4 → 0x00C0D2F0, else 0x00C0D240; with the arrival flag, 0x00C0D2F0 (`dev/LOAD_EVENTS.md`). Nothing writes 0x00C371B0 by its address (read only at 0x0076061B), so a move uses **0x00C0D240** (inferred: the global stays 0): (∓1.5, −1), (0, −2.5), (∓1.5, −4), (∓3, −2), (∓1, −3), (∓3.5, −1) m (right, forward), turned to the leader's movement. With +0x450 = 0, left and right swap (slot 0↔1; from slot 3, odd↔even). The others are scaled variants, e.g. 0x00C0D450: (∓1.25, −0.75), (0, −1.75), (∓1.75, −2.5), (∓3, −1.5), (∓1, −2.5), (∓3.25, −1.5).
- **The primary click 0x00712830(target, action, arg)**, a switch on action − 3 (table 0x00712DC4) over 0x006360C0(mask 0x80):
  - **4, attack: every listed member gets `Orders::Attack(member, the same target)`** 0x007F6410 (0x00712BB3; front of the queue, player issued, ON_ORDER_RECEIVED to each, `src/dao/Orders.cpp`), and the target id goes into member+0xC60's word +4 (what the follow's "has an order" test reads, below).
  - 3, talk: the first listed only, 0x007F64C0 (inferred: a conversation command through 0x0069EE80), plus OrderReceived.
  - 8, use: the first listed only, `Orders::UseObject` 0x007F6550; a dead creature becomes its body bag (+0x790).
  - 5 select, 6 deselect (above), 7 nothing.
  - Callers: `PerformPrimaryActionOnTargetUnderCursor` 0x007132B0, `PerformSecondaryActionOnTargetUnderCursor` 0x00713300, `PerformDefaultActionOnTarget` 0x00713350, `PerformSecondaryActionOnTarget` 0x00713390; the action from 0x0070CAD0(object).
- **Abilities go to one caster:** `PerformAbilityMessage` 0x00711B50 takes the caster from msg+8 (ability +4, item +0xC); 0x007116C0, 0x00711A00 and 0x00711940 take the targeting state's caster (0x0070ABE0: +0x4C or +0x54). All end in `GameModeExplore::UseAbility` 0x00711680 → `Orders::UseAbility` 0x007F6690.
- **Hold, `ToggleHoldPartyMessage` 0x0062F1F0:** toggles the bool attribute party+0xA4 (saved; read in the party's load 0x00637D50). It stops the engine's follow (0x00760F90) and the catch-up 0x006326C0, not the group move.

### Who follows: the engine, not the scripts

Read by the forked subagent; checked unless marked.
- **Where:** `CCCreature::Update` 0x006BE4B0 calls the party's update 0x00630F60(dt) (0x006BE876) only for the controlled creature (0x006BE56E–0x006BE582). Every 0.5 s (0x00B05774), with more than one member or a non-party follower (+0x260), it runs the follow manager at party+0x22C, 0x00760DC0(party).
- **Whom:** in explore or combat (mode 0 or 1), every active member (+0x234) and non-party follower (+0x260) **except the controlled creature, selected or not**, that has a +0x7B0 record, `GetFollowPartyLeader` true (FollowerInfo +9; true with no entry), Hold off, slot < 11, and passes 0x00760980.
- **0x00760980, "needs to follow":** its distance to the controlled creature, or to that creature's move destination while it walks a path, exceeds the member's slot value (+0x9C) + a per-slot float of the manager (not read) + 2.0 m (0x00C0D230), + 2.0 m more (0x00AE7108) when the leader's +0xB20 is 0 or 4 (inferred: standing). False while the controlled creature does a Deathblow (command 38), the member is in another area (+0x254), or a party member's state is not 1 or 7. Then true only when the member's current command is none, PlayAnimation (14) or a follow move (type 20 with flag 6 = 0), and its +0xC60 holds no order ({0, -1}). **A member with a player's order (a group move, an attack target, any other command) is left alone**, unless out of the controlled creature's perception.
- **What it orders:** the spot 0x00760810(…, slot, 0) from the formation table around the controlled creature, only if over 1.0 m from it. Perceived by the controlled creature (0x00760940: in its +0xC18): a walk, a current move retargeted in place or a new MoveToLocation, run 1, flag 6 = 0, **player issued 0** (0x007610D0). Not perceived: `ClearAllCommands`, a turn, and a JumpToLocation (type 10, 0x0073D080) to the arrival table's spot (0x00760810(…, 1)).
- **On a control switch** 0x00760C60 zeroes each member's +0xB0C (above; inferred: this manager's timer).
- **Catch-up 0x006326C0:** unless Hold, every active member failing 0x006A8D60 is set at its formation spot (`SetPosition`, 0x00632640 → 0x00760810); called at 0x006911B2 in 0x00690E70 (inferred: a conversation's setup).
- **In combat the scripts mostly take over** (inferred from the test): a follower's tactics keep it in commands other than a follow move, so the manager leaves it. `AI_DetermineCombatRound` with no target moves a follower farther than `AI_FOLLOWER_PLAYER_DISTANCE` to `GetFollowerWouldBeLocation` (`ai_main_h_2.nss`:852–858), from the same table.

### The scripts on the two events

- `player_core.nss`:863, ON_SELECT (89): `SSPlaySituationalSound(SOUND_SITUATION_SELECTED)`, nothing else.
- ON_ORDER_RECEIVED (90): **no live handler** (`player_core.nss`:873 is commented out). Event 94 (`PLAYER_COMMAND_ADDED`, :879, raised in 0x0073CD10) plays the order sound.

### Corrections

- `src/dao/Party.cpp`'s head: 0x006360C0 is not "its own list"; it walks the selection (+0x194), less members with the masked flags. Corrected the same day.
- `src/dao/Orders.cpp`: no script listens for event 90. Corrected the same day.
- `dev/COMBAT.md`'s "Range, line of sight": the id at party+0x1E8 is the leader, not the controlled creature. Corrected the same day.
- `dev/script_commands/Party.md`, `IsControlled`: TRUE for every selected member, not only `GetMainControlled`. Corrected the same day.
- Open: `dev/message_handlers.csv` has 0 for `CCParty::HandleMessage_SelectCharacterMessage`, which is 0x00639F90. The table feeds generated code, so it waits for an issue of its own.

### For a port

1. Keep a selection list; derive control from its first entry on every change; keep the leader apart.
2. On a switch: the old creature's key steering and look-at stop, its queue is kept; the camera's `SetTarget` (eased within 15 m); the follow timers reset; the HUD's `SelectedChar` and the GUI's character set; ON_SELECT only for a click through `Party::Select`.
3. AI: no engine switch, only the AI level (4 for the controlled); the scripts' `IsControlled`, true for every selected member, gives the partial AI.
4. A move goes to every selected member not input-disabled, in the 0x00C0D240 formation, 300 ms apart; an attack sends every selected member at the same target; abilities, talk and use go to one creature.
5. Members without an order follow every 0.5 s by the engine, in explore and combat, unless Hold or `SetFollowPartyLeader(FALSE)`.

## Tactics: the engine's share

Read 2026-10-18 by lib-agent-2 in the GOG exe (`game\DAOrigins.exe`), the toolset's scripts (`toolset\extract\scripts\`), the game's tables (ghilana, `tools\gda.py`) and the decompiled `tactics.gfx` (`build\gfx\tactics_as`). It builds on `dev/script_commands/Party.md` (SetTacticEntry, the preset commands, SetNumTactics, GetTacticTargetObject and the rest, all checked there). **(C)** means read in the code, table or script. **(I)** means inferred. Addresses are GOG's, with their address-library IDs in brackets where a function is new here. Nothing was seen in play.

**In short:** the engine stores the tactics and saves them, validates rows for the GUI and for `SetTacticEntry`, and switches presets (a predefined preset through a script event). It never evaluates a tactic. The scripts choose and run tactics (`AI_DetermineCombatRound` → `_AI_ExecuteTactic`), reading the rows through the `GetTactic*` getters, and the condition logic is all in `ai_conditions_h.nss`.

### 1. Storage

#### Who has a tactics object

- `CCCreature` +0xC00 is a SmartData attribute `Attribute<PartyTactics*>`. Its data-tree name is `Tactics` (string 0xB100B8, registered at 0x006B50A4), so the GUI sees `GUI.SelectedChar.Tactics.*` (C).
- **Created** by 0x006A9030 [14623], only if absent: `new PartyTactics(creatureId)` (0xF4 bytes, ctor 0x007B1120 [20022]) (C). Its callers (C):
  - 0x0063A100, inside 0x0063A020 [12482], the party's follower-map insert (Party.md), so a creature gets one when it becomes a follower.
  - 0x006BECBF, inside 0x006BE980 [14952] (`CCCreature` vtable slot 34, the load from a save, I), when the saved `SavegameTacticsHasTable` is true.
- **Destroyed** by 0x006A3710 [14485], called from 0x0063B8B0 [12490], the follower-map erase (party +0x2A8) (C).
- So non-followers have no object: every getter answers its default, and `GetNumTactics` answers 0 (C). Monsters run only on package tables (§3).

#### `PartyTactics` (RTTI `PartyTactics`, vtable 0x00B216E8, 0xF4 bytes)

Fields are SmartData attributes. Each one's name is its data-tree token, read in ctor 0x007B1120 (C).

| Offset | Name (data tree) | Type | Initial (ctor) | Saved as |
|---|---|---|---|---|
| +0x58 | — (owner) | creature id | ctor argument | — |
| +0x5C | `Enabled` | bool | 1 | `SavegameTacticsEnabled` 16823, UINT8 |
| +0x6C | `TacticsList` | `ECDynArray<PartyTacticEntry*>` | resized to **4** (0x007B162E) | `SavegameTacticsList` 16824, list of PTE1 |
| +0x88 | `PresetIdList` | int list (16-byte records, I) | empty | `SavegameTacticsPresetList` 16835, list of UINT8 |
| +0xA4 | — (custom presets; storage begins at +0xA8, records of 16 bytes, each a list of entries) | vector | resized to `NumCustomSlots` (3) (0x007B1625) | `SavegameTacticsCustomList` 16836, list of PTL1 |
| +0xB4 | `NumCustomSlots` | int | 3 | — |
| +0xC4 | `PresetIndex` | int | 0 | `SavegameTacticsPresetIndex` 16834, UINT8 |
| +0xD4 | `PresetType` | int: 0 none ("Off"), 1 custom, 2 predefined (`Tactics.as`:32-34) | 1 | `SavegameTacticsPresetType` 16833, UINT8 |
| +0xE4 | `Dirty` | bool | 1 (I: the byte read is the 1 left in the temporary) | `SavegameTacticsDirty` 16832, UINT8 |

It also registers six message handlers under the same token (`GUI.SelectedChar.Tactics.<Message>`) (C). They are in §2.

#### `PartyTacticEntry` (RTTI, vtable 0x00B21530, 0x160 bytes, ctor 0x007AD310 [19949])

| Offset | Name (data tree) | Meaning | Written by | Save field (GFF4 struct `PTE1`, def 0x007AF190 [19979]) |
|---|---|---|---|---|
| +0x58 | `Enabled` | row on | SetTacticEntry, GUI | `SavegameTacticEntryEnabled` 16825, UINT8 |
| +0x68 | `Target` | target type, a table 140 row | SetTacticEntry, GUI | `…Target` 16826, UINT8 |
| +0x78 | `TargetID` | object for a per-follower target type (140 `GeneratePerFollower`) | GUI only | `…TargetObjectID` 16818, UINT32 |
| +0x88 | `Condition` | a table 137 row | SetTacticEntry, GUI | `…Condition` 16827, UINT16 |
| +0x98 | `ConditionID` | object for a per-follower condition (137 `GeneratePerFollower`) | GUI only | `…ConditionObjectID` 16819, UINT32 |
| +0xA8 | `Command` | a table 141 row | SetTacticEntry, GUI | `…Command` 16828, INT32 |
| +0xB8 | `CommandParam` | ability id (also set to the item's ability for Use Item) | SetTacticEntry, GUI, refresh | `…CommandParam` 16829, INT32 |
| +0xC8 | `CommandItem` | item object id, -1 none | GUI | — |
| +0xD8 | `ItemName` | wstring attribute (display) | refresh | — |
| +0x100 | `ItemIcon` | wstring attribute (display) | refresh | — |
| +0x128 | — | item tag (`GetTacticCommandItemTag`) | refresh | `…CommandItemTag` 16837, ECString |
| +0x144 | — | item resref | refresh | `…CommandItemResRef` 16838, ECString |

The save writer is 0x007AD880 (C). It writes the tag and resref only when the struct definition has the field. The xoreos names `…TargetTag` 16830 and `…ConditionTag` 16831 are not used by this build (no code holds them, C).

**The item refresh** is 0x007ACDF0 [19940], run per row by 0x007AD070 [19941] (C):
- For command 7 (Use Item) with `CommandItem` ≠ -1: it copies the item's tag (item +0xB8) to +0x128, its resref (+0x1CC) to +0x144, the name and icon, and sets `CommandParam` to the item's ability (item +0x660).
- For command 7 with no item and the refresh flag: it looks up the name and icon from the saved resref (0x007532E0).
- For any other command: `CommandItem` = -1, and the strings are cleared.

#### The tactic list in the save

- **The creature's GFF4 struct** (field defs 0x006B7CF0 [14880]) has `SavegameTacticsHasTable` 16821 (UINT8) and `SavegameTacticsTable` 16822 (struct `PTT1`, def 0x007AF850 [19985]) (C).
- **`PTT1`** holds `TacticsList` (list PTE1), `TacticsEnabled`, `Dirty`, `PresetType`, `PresetIndex` (UINT8 each), `PresetList` (list of UINT8) and `CustomList` (list of `PTL1`). **`PTL1`** is `{ TacticsList: list PTE1 }` (C).
- **Save:** 0x006B23B0 [14839] (the creature's save, I) writes HasTable = (+0xC00 ≠ null) and then calls `PartyTactics::Save` 0x007ADF40 [19958] (C).
- **Load:** 0x006BE980 reads HasTable, creates the object, then calls `PartyTactics::Load` 0x007AFBF0 [19992] (C).
- Preset ids, the preset index and the type are saved as **UINT8**, so an id above 255 does not survive a save (I, from the field type).
- The behaviour is not here. It is creature property 18 (§2), saved with the properties.

#### The slot count

- The engine's only numbers are the ctor's 4 rows and `SetNumTactics`' resize. It has no clamp and no table (Party.md; C).
- The real count is script: `Chargen_SetNumTactics` (`_Core Includes\core_h.nss`:2320-2348) is the only `SetNumTactics` caller. It sets n = `exptable` (table 164) `Tactics` at the creature's level, plus the highest Combat Tactics rank held: rank 4 (100113) +6, else rank 3 (100112) +4, else rank 2 (100111) +2, else rank 1 (100110) +1. `bSendNotification` = level ≠ 1 (C).
- For the hero at level 1 it also writes row 1 = Enemy / condition 31 / Attack (C).
- `exptable.Tactics` (C): levels 1-2 → 2, 3-5 → 3, 6-9 → 4, 10-14 → 5, 15-19 → 6, 20-25 → 7, so 13 at most.
- Callers (C):
  - chargen: `sys_chargen.nss`:409, 471, 484, 510.
  - `AL_DoAutoLevelUp`: `sys_autolevelup_h.nss`:363, which hiring runs.
  - autoscale: `sys_autoscale_h.nss`:517, 592.
- The notification is a type-6 entry on party +0x330 ("new tactic slot"; Party.md).

#### The script commands (all from `dev/script_commands.csv`; lookup = 0x007ADCF0 [19956] → `GetCreatureById` → +0xC00 → 0x007ADA40 [19953] row by index; index 1-based)

The commands are 319, 365-367, 372-374, 772-778 and 787. Commands 375-378 are item commands, not tactics.

| # | Command | Handler | Returns / does (C unless marked) |
|---|---|---|---|
| 319 | SetTacticEntry | 0x006615F0 | validated write of Enabled/Target/Condition/Command/CommandParam (Party.md) |
| 365 | GetTacticPresetID | 0x00651650 | `PresetIdList[PresetIndex]` only if PresetType == 2 and !Dirty, else 0 |
| 366 | SetTacticPresetID | 0x006516B0 → 0x007ADB40 | resets rows, sends event 83 synchronously, PresetIndex, PresetType = 2 |
| 367 | AddTacticPresetID | 0x00651710 → 0x007AF4E0 | appends to `PresetIdList` |
| 372 | GetTacticTargetObject | 0x0066AE10 | entry +0x78 (`TargetID`); missing → 0 |
| 373 | GetTacticConditionObject | 0x0066AF30 | entry +0x98; missing → OBJECT_INVALID |
| 374 | GetTacticCommandItemTag | 0x006614F0 | entry +0x128; missing → "" |
| 772 | GetNumTactics | 0x0066D580 | **size of `TacticsList`, but 0 when `Enabled` (+0x5C) is false**, or no object |
| 773 | SetNumTactics | 0x0066D600 → 0x007AFB70 [19991] | resizes the list and every custom preset (Party.md) |
| 774 | IsTacticEnabled | 0x0066ACC0 | entry Enabled && Target ≠ 0 && Condition ≠ 0 && Command ≠ 0. Does not read +0x5C |
| 775 | GetTacticTargetType | 0x0066AD80 | entry +0x68; missing → 0 |
| 776 | GetTacticCondition | 0x0066AEA0 | entry +0x88; missing → 0 |
| 777 | GetTacticCommand | 0x0066AFC0 | entry +0xA8; missing → 0 |
| 778 | GetTacticCommandParam | 0x0066B050 | entry +0xB8; missing → 0 |
| 787 | IsPartyAIEnabled | 0x0066AC60 | +0x5C (`Enabled`); no object → FALSE |

A second command table, `CClientVirtualMachineCommands` (vtable 0x00B113C4, filled by 0x006DB420, VM at 0xC1D084), has two client-side twins:
- 0x006D7CB0 [15372]: six ints, an **unvalidated** row write.
- 0x006DA000 [15413]: a SetNumTactics with notification.

No shipped script is known to use the client VM (I).

### 2. Presets, behaviour and the GUI

#### Where presets come from (scripts and tables, C)

- **Table 214 `tacticsuserpresets`** (ID, Label, NameStrRef, TacticsTable, ValidForClass1-3): 1 Tank→2001, 2 Damage Dealer→2002, 3 Archer→2003, 4 Healer→2004, 5 Nuker→2005, 6 Buffer→2006, 7 Crowd Control→2007, 8 Debuffer→2008, 9 War Dog→2010.
- **Each preset table** (2001-2010, e.g. `tanktacticspreset`) has the columns ID, TargetType, Condition, Command, SubCommand.
  - The game's Tank table is overridden by the enciphered add-in `dao_prc_cp_2`.
  - The AI package tables `aip_*` (10011-10142) have the same columns plus UseChance.
- **`Chargen_EnableTacticsPresets`** (`_Systems\_Includes\sys_chargen_h.nss`:907) calls `AddTacticPresetID(c, row)` for each 214 row whose class matches. It runs:
  - at CHARGEN_END create and level-up (`sys_chargen.nss`:431, 511);
  - on every non-summon PARTY_MEMBER_HIRED (`player_core.nss`:665).
- **`SetTacticPresetID`** is called only at chargen end (`sys_chargen.nss`:506): warrior 1, rogue 2, mage 5.
- **On level-up**, `Chargen_PopulateTacticsForPreset` (sys_chargen_h:975) reloads the preset if `GetTacticPresetID > 0`.
- **A follower's first hire** (`player_core.nss`:773-817) fills its rows from table 163 `packages_base`.`FollowerTacticsTable` (10106 Alistair … 10128 Mouse), and its behaviour from `FollowerBehavior`.

#### `EVENT_TYPE_LOAD_TACTICS_PRESET` (83)

- **The engine's side** (Party.md, C): `SetTacticPresetID` resets every row (0x007ABEC0 [19902]) and delivers event 83 at once to the creature, with int 0 = the preset id.
- **The scripts' side** (C): `player_core.nss`:932 → `HandleEvent_LoadTactics` (455) → `Chargen_LoadPresetsTable` (`sys_chargen_h.nss`:932).
  - It reads 214.`TacticsTable`, walks that table's rows up to `GetNumTactics`, and keeps a row if 141.`UseType` == 0 or the creature `HasAbility(SubCommand)`.
  - It writes each kept row with `SetTacticEntry(c, n++, TRUE, …)`.
- The rows of a predefined preset therefore pass the engine's validation (`SetTacticEntry`) one by one (C).

#### The tactics screen's messages (`PartyTactics::HandleMessage_*`, registered in ctor 0x007B1120)

| Message | Handler | Does (C) |
|---|---|---|
| `LoadPreset <index> <type>` | 0x007AE4B0 [19960] | Dirty = 0, then by type (below), then a GUI refresh (0x00693290, 0x430) |
| `SavePreset <slot>` | 0x007AE780 [19961] | copies `TacticsList` into custom slot `slot` (0x007AE160), PresetIndex = slot, PresetType = 1, Dirty = 0 |
| `DirtyPreset` | 0x007AD100 [19943] | Dirty = 1, so `GetTacticPresetID` answers 0 |
| `SetBehaviour <b>` | 0x007AD120 [19944] | sets creature property 18 = b (`PROPERTY_SIMPLE_AI_BEHAVIOR`, `core_h.nss`:24; a table 187 `aibehaviors` row) through the stats (+0x7C4), 0x0071B230 then 0x0071E300 |
| `RefreshTacticsData <flag>` | 0x007AD170 [19945] | the item refresh of every row (0x007AD070) |
| `ValidateAction <row>` | 0x007B0170 [19995] | reads the row condition's 137.`ConditionBase` and checks the row's command with 0x007B0040. If it is invalid, it rewrites Command (+0xA8), CommandParam (+0xB8) and CommandItem (-1) (C for the writes; I for the values) |

`LoadPreset` by type:
- **type 2:** `Enabled` = 1, then `SetTacticPresetID(PresetIdList[index])`, so event 83 fills the rows.
- **type 1:** `Enabled` = 1, rows ← custom slot `index`, PresetIndex = index, PresetType = 1.
- **type 0 ("Off" in the picker):** `Enabled` = 0, PresetIndex = PresetType = 0, every row reset (0x007ABEC0).

**The GUI's own writes:**
- Rows (`.Target`, `.Condition`, `.Command`, `.CommandParam`, `.CommandItem`, `.Enabled`, the `…ID` objects) and the `Enabled` switch are written by `ExternalCommands.SetValue` straight into these SmartData attributes. There is no engine code between them.
- The AI on/off button (`Tactics.as`, `AIOnOffButton_mc_init`) sets `GUI.SelectedChar.Tactics.Enabled` 0 or 1 (C, in the ActionScript).
- Within the PartyTactics code, +0x5C is written only by the ctor, load and `LoadPreset` (a linear sweep of 0x007AB000-0x007B2000, C).
- "ToggleTactics" (0xAF59F0) is an input action registered with the key map (0x0051E8DA, 0x006E381E). What it sends was not read.

**What the GUI is offered** comes from the engine's list builders, behind the movie's FSCommands. `SetTacticEntry`'s check 0x007B0040 uses the same builders, so the screen and the script share one rule set (C). The column hashes below were resolved by `tools/gda.py`'s CRC.

| FSCommand | Handler | Builder |
|---|---|---|
| `ListTacticTargets` | 0x00714860 [16336] | 0x007B0A80 [20010]: table 140 `ID`, `FollowerAI`, `GeneratePerFollower`; a target with at least one valid action (calls 0x007AFE80) |
| `ListTacticBaseConditions` | 0x00714A00 [16337] | 0x007AF570 [19983]: 139 `FollowerAI`, `ValidForTarget` & 140 `Type` |
| `ListTacticConditions` | 0x00714AF0 [16338] | 0x007AEB50 [19973]: 137 `ConditionBase`, `FollowerAI`, `GeneratePerFollower`, `ShowOnSelf` |
| `ListTacticActions` | 0x00714E20 [16340] | 0x007AFE80 [19993]: 141 `FollowerAI`, `ValidForTarget`, `UseType`, `UseItem`; for Use Ability it needs a non-empty ability list, for modes the modal list, for Use Item the item list |
| `ListTacticAbilities` | 0x00714FD0 [16341] | 0x007AED50 [19974]: the creature's abilities whose `ABI_base` `ShowIntactics`, `TargetType` and `UseType` fit the target type, and whose `AOE_Type` fits 139.`FilterAOE` |
| `ListTacticModalAbilities` | 0x00714D30 [16339] | 0x007AEF50 [19975]: `ShowIntactics`, `UseType` |
| `ListTacticItems` | 0x00714770 [16335] | 0x007AF6C0 [19984]: the party's usable items by `TargetType` and `ShowIntactics` |

### 3. Does the engine evaluate tactics? No (C)

**Native side (C):**
- No native code reads a row except:
  - the script getters (0x007ADCF0's ten callers: the nine `GetTactic*`/`IsTacticEnabled`/`SetTacticEntry` handlers and the client-VM twin);
  - the save and load;
  - the GUI handlers and the list builders.
- 0x007ADA40 has one caller, 0x007ADCF0. Every access to creature +0xC00 is create, destroy, save, load, the data-tree registration, the ctor/dtor, or 0x007AD0D0 (a sweep for the displacement 0xC00 over `.text`).
- No `PartyTactics` code runs per frame or on a combat event. Its only event is 83, which hands preset loading to the scripts.

**Script side (C; `_Core Includes\ai_main_h_2.nss`):**
- **Who calls it.** `rules_core.nss` calls `AI_DetermineCombatRound` on COMMAND_COMPLETE (787/796), ATTACKED (671/683) and perception. It sends `EVENT_TYPE_HANDLE_CUSTOM_AI` instead when `AI_CUSTOM_AI_ACTIVE` is set.
- **Order of returns in `AI_DetermineCombatRound` (305):**
  1. A non-combatant (314).
  2. A game mode neither combat nor explore (362). It queues `_AI_DoNothing` only if `!IsControlled`.
  3. A current command or a queue (378-387).
  4. `CREATURE_RULES_FLAG_AI_OFF` (392).
  5. A non-follower with `AI_LIGHT_ACTIVE` takes the Light AI (403).
  6. Area-effect escape (416-480). For followers only when `!IsControlled`.
  7. **`IsFollower && !IsPartyAIEnabled` → `AI_DetermineCombatRound_Partial` and return (487).** `_Partial` attacks the last target or the one the engine chose, if it is in range.
  8. `AI_MODIFIER_IGNORE` (499).
  9. `IsFollower && !IsControlled`: the behaviour's checks from table 187 (507-592).
  10. **`IsControlled` → `_Partial` only, after a combat-state check (632-640). The controlled creature never runs tactics.**
  11. Otherwise the tactic loop (643-763), below.
- **The tactic loop:**
  - `nTacticsNum` = `GetNumTactics` if `_AI_UseGUITables()`, else the package table's rows (672-677). `_AI_UseGUITables` (3198) is the module int `AI_USE_GUI_TABLES_FOR_FOLLOWERS` (default 1), and `GetFollowerState` ≠ INVALID, and not polymorphed.
  - The package table is `GetPackageAI` (UTC `PackageAI`, ≤ 10011 → 10011), or 188.`TacticsTable` when shapechanged (2108).
  - Above `AI_MAX_TACTICS` (50) it returns (678).
  - It resumes after `AI_LAST_TACTIC` when the last command failed (693-711).
  - The module flag `AI_DISABLE_TABLES` skips the loop (687).
  - Then `_AI_ExecuteTactic` for i = 1..n (724-738). It returns 1 done, 0 next, -1 default action, >1 jump.
  - Then `AI_ExecuteDefaultAction` (760).
- **`_AI_ExecuteTactic` (1055):**
  - GUI rows: `!IsTacticEnabled` → skip (1061). Then it reads `GetTacticTargetType`, `GetTacticCondition`, `GetTacticCommand` and `GetTacticCommandParam` (1125-1128), and `GetTacticCommandItemTag` (1121).
  - Package rows: a `UseChance` roll, cut on Casual (1073-1104), and the columns `TargetType`/`Condition`/`Command`/`SubCommand` (1132-1135).
  - The target must match: 140.`Type` & 139.`ValidForTarget` (1138-1175).
  - The condition: 137.`ConditionBase`/`ConditionParameter`/`ConditionParameter2` → an `_AI_Condition_*` helper in `ai_conditions_h.nss` that answers a target (1203-1383).
  - The command: a switch (1427-2052), then `WR_AddCommand` (2091-2095).
  - `GetTacticTargetObject` and `GetTacticConditionObject` are read for the per-follower target and conditions (`ai_conditions_h.nss`:1005, 2380, 2954).
- **The engine commands the conditions lean on** do real work but none evaluates a tactic: `GetNearestObjectByHostility`/`ByGroup`, the threat commands, `GetCreaturesInMeleeRing`, `GetAttackTarget`, `GetAbilitiesDueToAOEs`, `GetRemainingCooldown`, `CheckLineOfSightObject` and `GetFollowerWouldBeLocation`.

#### `IsPartyAIEnabled` and `IsControlled`

- **`IsPartyAIEnabled`** = `PartyTactics` +0x5C `Enabled` (C). It is set:
  - true by the ctor;
  - by the tactics screen's AI on/off button (data tree);
  - by `LoadPreset` (0 for "Off", 1 otherwise);
  - restored by load (`SavegameTacticsEnabled`).
- Its two effects:
  - the script's line 487 (a follower takes `_Partial`, so it fights back but runs no tactics);
  - `GetNumTactics` answers 0 while it is off (C, 0x0066D5B6).
- **`IsControlled`** (714, 0x00660C90; Party.md: the party's controlled list +0x184) gates these places in the scripts (C):
  - `ai_main_h_2.nss`:367 (no wait), 422 (no area-effect escape), 507 (no behaviour), 632 (`_Partial` and no tactics), 1022 (no wait in `_Partial`), 2074, 3224;
  - `rules_core.nss`:758, 951, 1022-1032, 1073.
- `GetMainControlled` is used only for target type 18 "Main Controlled" (`ai_conditions_h.nss`:1001, 2376, 2959) and `_AI_MoveToControlled` (3317).

### For a port

- **Store per follower:** Enabled, the rows (Target u8, TargetID, Condition u16, ConditionID, Command, CommandParam, item tag and resref), PresetIdList, PresetIndex, PresetType, Dirty, and 3 custom slots. Start with 4 rows; the scripts resize the list.
- **Give the scripts** the twelve getters and setters above, with their odd defaults: 372 → 0, 373 → -1, `GetNumTactics` → 0 when disabled, and `IsTacticEnabled` requiring Target, Condition and Command all non-zero.
- **Validate `SetTacticEntry`** with the same FollowerAI/ValidForTarget/UseType rules the list builders use.
- **Run event 83 synchronously** from `SetTacticPresetID` after resetting the rows.
- **Leave all evaluation** to `ai_main_h_2`/`ai_conditions_h`.
