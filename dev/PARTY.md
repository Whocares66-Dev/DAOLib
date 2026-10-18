# The party: who is active, the pool on an area transition, a follower's state

An object's active flag and the searches it limits, the party pool carried on an area transition, and `SetFollowerState`'s switch. Read 2026-10-18 by lib-agent-2 (with subagents) for DAOParagonEngine's mage origin. Not seen in play. The key claims below were checked in the disassembly. The formation the arrival places the active party in is `dev/LOAD_EVENTS.md`'s.

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

## CreateObject then a hire, in one script

Read 2026-10-18 by lib-agent-2 (with a subagent) for paragon-agent-2's Circle basement, not seen in play. The queue's order and the script run at delivery are `dev/LOAD_EVENTS.md`'s "The event queue's gate and delivery".

`zz_bhm000pt_debug.nss`:116–125 makes Jowan and Lily with `CreateObject` and hires each at once (`UT_HireFollower` → `WR_SetFollowerState` → `SetEventScript(player_core)`, `wrappers_h.nss`:353; `PARTY_MEMBER_HIRED` signalled, :376).
- The queue then holds SPAWN, then HIRED, both for player_core.
- player_core's SPAWN (`player_core.nss`:498–507) calls `AS_InitCreature(self)` with level −1, and `AS_AddClassLevels` returns at once (`sys_autoscale_h.nss`:266), so no class is set.
- The template's `PropertyList` (class 2) isn't read by the exe (`dev/PROPERTIES.md`). So HIRED's auto-level finds no class (`sys_autolevelup_h.nss`:66, "Player character has no class…").
- Vanilla's debug path is broken the same way. The real path (`bhm000pt_tranquility.nss`:157–165) hires the Jowan placed in the tower, whose creature_core SPAWN ran at the area's load.
