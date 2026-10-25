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
  - **Activating with an animation queues a Wait** (read 2026-10-21 by lib-agent-2, for DAOParagonEngine #144; C checked, I inferred) (`SetObjectActive` 140, 0x006618B0, activating with nAnimation ≠ 0) [C]:
    1. The handler calls slot +0x140(1) to activate.
    2. It plays the clip at once through slot 44 (0x006AA7E0) with an instant blend (−1.0 at 0x00AE2E00).
    3. It queues a **static type-13 Wait command at the front** (0x00742090(0xD), [cmd+0xC] = 1, QueueCommand 0x0073CD10). The Wait lasts 80 % of the clip: AnimationLength (0x00694E50) − 0x00694FE0(len, −2.0).
  - **The Wait holds the AI** [C, `ai_main_h_2.nss`:378–392]: `AI_DetermineCombatRound` returns while a command is current, so the creature's AI starts when the Wait ends.
  - **The perception clear doesn't end it** [I]: `rules_core`'s `WR_ClearAllCommands` on the first APPEAR leaves static commands, so it should leave the Wait.
  - **nVFX** (only with an animation) [C]: it is applied as an effect record of kind 2, with +0x2C = the animation's length (ctor 0x00A652A0, applied through 0x0079F410/0x007A1A20). It does not hold the creature.
  - **The Harrowing**:
    - The wisps: `bhm600pt_harrowing.nss`:316 → `_TeamAppearsWithVFX(team, 1533)` (`bhm_functions_h.nss`:33–42) → `WR_SetObjectActive(member, TRUE, 1, 1533)`. Animation 1 is `c_wisp.rdy.ani`, 3.30 s, so the Wait is 2.64 s; play shows 2.7 s.
    - The demon: `bhm600tr_demon_talk_trigger.nss`:85 → `WR_SetObjectActive(oDemon, TRUE)`. `wrappers_h.nss`:112–139 picks 648 because apr_base row 27 has `BHasAppearAnim` = 1. Animation 648 is `c_ragedemon.appr.ani`, 4.1667 s, so the Wait is 3.33 s; play shows 3.4 s. The 60–70 ms over is inferred to be tick and event delivery.

## A search by tag finds inactive objects

- **The filter** 0x0067FEC0 (8 arguments, `ret 0x20`) starts with `if (!IsActive && tag.size == 0) return 0` (0x0067FED0–0x0067FEE6, checked). Then it tests the type mask (bit type − 1), the tag's equality when one is given (obj +0xB8), the group (-1 any, obj +0x3C0), and three optional flags: hostility to the source (0x0064B900, the rule `IsObjectHostile` uses; checked 2026-10-18 by lib-agent-2, below), not dead, perceived (source +0xC18).
- **The search** is 0x006849C0 (address library `ScriptBackend::GetNearestObjects`, from fe62e8f), over the area's list at +0x690. Its only filter calls are 0x00684B2D and 0x00684C4F.
- **So** `GetNearestObjectByTag` (435) and `UT_GetNearestObjectByTag` (360, 0x0066DB20 → 0x0066DC0A) return an inactive object whose tag matches. `GetNearestObject` (433), `…ByGroup` (436), `…ByHostility` (127), and a tag of "", return active objects only.
- 0x00671510 is one handler for 127, 433, 434, 435 and 436; only 435 reads a tag (0x0067163C).
- Contrast: a conversation line's speaker and listener look up by tag through 0x006973B0 with inactive objects left out (`dev/PLOTS.md`).

### GetNearestObject family: filters, hostility, order (GOG exe, read 2026-10-18 by lib-agent-2)

Tags: [C] checked in disassembly or data, [I] inferred.

#### Short answers

1. **nCheckPerceived** [C] is only membership: the candidate's id is in the source creature's perceived set at source(as creature)+0xC18 (hash set of object ids, `0x008704D0(set, &id)` = find != end). No line of sight, no distance, no stealth flag in the test. (Stealth and sight are decided when the set is filled, `dev/COMBAT.md`: a stealthed hostile is not added; a hostile out of sight is not added; non-hostiles in range are.) The source is the first argument object (`oObject`), cast to its creature (slot 0x10); a non-creature source with the flag on dereferences null+0xC18 [I: crash, not seen]. `GetNearestObjectToLocation` (434) has no such parameter: its handler pops only location, type, count, and calls a different function (`0x00684DD0`) that has no flag, no source, no OBJECT_SELF use [C].
2. **nCheckLiving** [C] = the candidate's vtable slot 0x120 (`this`-call, no args) returns a number > 0. For CCCreature slot 0x120 is `0x006AB4B0`: the current-health property (index 7 of the stats' property vector at +0x7C4 -> +0x190, the property's float value, rounded to int with `0x008E34F0`), 0 or less means dead. CCItem's slot 0x120 is `0x004E13D0` (returns -1), so non-creatures that do not override it count as not living [C for item, not read for placeable]. There is no separate dying/dead flag: health <= 0 is "dead" (`0x00695300` returns `slot0x120() <= 0`; the filter negates it).
3. **Filters in order** (the filter is `0x0067FEC0`, 8 stack args: candidate, typeMask, tagString*, group, source, hostility, living, perceived; `ret 0x20`): candidate null -> reject; inactive and tag empty -> reject; type mask bit; tag equality; group; hostility; living; perceived. Then in the search itself: include-self, skip-duplicates, skip ids that resolve to no object. Only the source's own area (`source slot 0xA0` -> area object) is searched; no distance cap; no hidden, plot or party test [C].
4. **Hostility (127)** [C]: yes the same rule as IsObjectHostile: the filter calls `0x0064B900(table=[app+0x1B0], candidateCreature, sourceCreature)` and `IsObjectHostile` `0x006A2B80` -> `0x0064BB40(ids)` -> the same `0x0064B900`. But the argument is only an on/off switch: **nonzero = keep hostile candidates only; 0 = no hostility test at all (everything)**. `GetNearestObjectByHostility(o, FALSE, ...)` therefore is NOT "non-hostile only"; it equals GetNearestObject. Both objects must be creatures or the candidate is rejected.
5. **Order** [C]: ascending squared 3-D distance (dx^2+dy^2+dz^2, `0x008D8B60` is a 3-float dot product, positions at object+0x120), from the source's position (or the location's). Insert position = first stored distance strictly greater than the new one, so equal distances keep scan order. Scan order: self first (distance 0, only if nIncludeSelf and it passes the filter), then walks the area's x-sorted object list outward from the source's index in the order +1, -1, -2, +2, +3, -3, -4, +4 ... A side is dropped once the axis gap squared (dx^2) exceeds the current N-th best squared distance. Handler cuts to the first min(N, found); N <= 0 is made 1. The result is an array of object ids.

#### Per command: what the handler 0x00671510 passes

Handler pops (script args in declaration order; ids as objects): command numbers are 127=0x7F, 433=0x1B1, 434=0x1B2, 435=0x1B3, 436=0x1B4. Call: `0x006849C0(area, out, source, typeMask, count, tag, group, hostility, living, perceived, includeSelf)`, `ret 0x28`, `this` = source's area.

| command | source | typeMask | tag | group | hostility | living | perceived | includeSelf | count |
|---|---|---|---|---|---|---|---|---|---|
| 127 ByHostility | oObject | nObjectType | "" | -1 (see note) | nHostility | nCheckLiving | nCheckPerceived | nIncludeSelf | nNumberOfObjects |
| 433 Object | oObject | nObjectType | "" | -1 | 0 | arg | arg | arg | arg |
| 435 ByTag | oObject | nObjectType | sTag | -1 | 0 | arg | arg | arg | arg |
| 436 ByGroup | oObject | nObjectType | "" | nGroupId | 0 | arg | arg | arg | arg |
| 434 ToLocation | none (location) | nObjectType | none | none | none | none | none | none | nNumberOfObjects |

- Optional includeSelf pop: only if the handler's arg count (2nd stack arg) is > 5 for 433 and > 6 for the others, else 0. Living and perceived are popped unconditionally (pop failure returns error -0x27F/-0x27E). Defaults per script.ldf: type OBJECT_TYPE_ALL = 0xFFFFFFFF, count 1, living/perceived/self 0.
- Note [C for the missing init, I for the intent]: the handler's stack slots for hostility (+0x3C) and group (+0x48) are never initialised for the commands that do not pop them (type slot is initialised to -1, object to -1, others to 0). The pushed value is whatever is in them; the intent is certainly hostility 0 and group -1 and the game behaves so, so a port should use those.
- 434: area = `0x005D5840(location+0x20)` (the location's area id); `0x00684DD0(area, out, &pos(x,y,z from location+0..8), typeMask, count)`, `ret 0x10`. If the area is not found the array is empty.

#### What _AI_GetEnemies passes (`toolset/extract/scripts/_Core Includes/ai_conditions_h.nss` line 2451) [C]

`GetNearestObjectByHostility(OBJECT_SELF, TRUE, OBJECT_TYPE_CREATURE, AI_MAX_CREATURES_NEAREST /* = 5 */, TRUE /*living*/, TRUE /*perceived*/, FALSE /*not self*/)`. Then the script filters: followers only, enemies farther than AI_FOLLOWER_ENGAGE_DISTANCE_{LONG,CLOSE} from GetMainControlled() end the list (a `break`, the list is sorted by distance from OBJECT_SELF), and creatures with the ability active (use-ability) or with AI_MODIFIER_IGNORE are skipped. An override target (`_AI_GetTargetOverride`) returns a one-element array; a follower using an ability on its attack target within melee range returns that target.

So: the 5 nearest hostile, living creatures that are in OBJECT_SELF's perceived set, nearest first.

#### Pseudo-code a port can copy

```
// types: script mask. engine type byte T (1 = GUI ... 6 = creature ... 10 = placeable); bit = 1 << (T - 1); OBJECT_TYPE_CREATURE = 32 = bit of 6.
bool Filter(cand, mask, tag, group, src, wantHostile, wantLiving, wantPerceived) {
    if (!cand) return false;
    if (!cand.IsActive() && tag.empty()) return false;          // so tag searches see inactive objects
    if (!(mask & (1u << (cand.type - 1)))) return false;
    if (!tag.empty() && cand.tag != tag) return false;          // exact string compare of cand+0xB8 (0x004C0A50, wide chars)
    if (group != -1 && cand.group != group) return false;       // cand+0x3C0
    bool ok = true;
    if (wantHostile && src) {
        auto c = cand.AsCreature(); auto s = src.AsCreature();
        if (!c || !s || !app.hostilityTable) return false;
        ok = IsObjectHostile(c, s);                              // 0x0064B900, same as 0x006A2B80
        if (!ok) return false;
    }
    if (wantLiving) ok = !(cand.CurrentHealthRounded() <= 0);    // slot 0x120; non-creatures: -1 => false
    if (ok && wantPerceived) ok = src.AsCreature().perceivedSet.contains(cand.id);   // +0xC18
    return ok;
}

array GetNearest(src, mask, count, tag, group, wantHostile, living, perceived, includeSelf) {
    area = src.GetArea(); if (!area || !src.AsObject()) return {};
    res = [];                                                    // ids with squared distances, ascending
    idx = indexOf(src.id in area.objectsSortedByX);              // if not found, empty
    if (includeSelf && Filter(src, mask, tag, group, src, ...)) res.insert(src, 0.0);
    for (k in 1, -1, -2, 2, 3, -3, -4, 4, ...) {                 // outward from idx, both sides until each is exhausted or pruned
        cand = list[idx + k];                                    // object must resolve; ids already in res skipped
        if (!Filter(cand, ...)) continue;
        d2 = dot3(cand.pos - src.pos);                           // pos = +0x120 (x,y,z)
        if (res.size() >= count && dx*dx > res[count-1].d2) { mark this side done; }  // x-sorted pruning, candidate is still inserted
        res.insertBefore(first element with d2 > new, new);      // ties after existing
    }
    n = count <= 0 ? 1 : count;  return first min(n, res.size()) ids;
}

array GetNearestToLocation(loc, mask, count) {                   // 0x00684DD0
    area = loc.area; if (!area) return {};
    // same outward scan from the x-binary-search position (0x0067CD00)
    // per candidate: id resolves, mask bit, nothing else (no active test, no tag, no living, no perceived)
    // distance = dot3(cand.pos - loc.pos); same insertion and cut
}
```

Since the pruning is exact, a port may simply filter every object of the area and sort ascending by squared 3-D distance with a stable sort in scan order, then cut to count; only the order among exactly equal distances differs (arbitrary in the engine).

#### Evidence

- Handler `0x00671510`: command switch `0x006715A2`-`0x00671677` (cmp 0x1B1, 0x1B3, 0x1B4, 0x7F, 0x1B2); object pop `0x00671615` -> `0x00A2F6D0`; tag pop `0x0067163C` -> `0x00A2F4C0`; group pop `0x00671656` -> `0x00A2F3C0` into [esp+0x48]; hostility pop `0x00671662`-`0x0067166D` into [esp+0x3C]; location pop `0x006715D5` -> `0x00A2F650`; type and count `0x00671677`-`0x006716AB` (count <= 0 -> 1 at `0x006716AD`); living/perceived/self pops `0x00671743`-`0x006717A4`; the 10-arg call `0x006717CD`-`0x00671800`; result trim `0x00671828`-`0x00671887`; ToLocation `0x006716E2`-`0x00671731`.
- Search `0x006849C0` (address library ID 13767 `ScriptBackend::GetNearestObjects`): area list at area+0x690 (vector of ids at +0x694..+0x698); source index found by linear search `0x00684AA0`; include-self filter call `0x00684B2D`; scan loop `0x00684B78`; duplicate skip `0x00684C00` (`0x008663A0(out, &id)`); id -> object `0x00684C17` (`0x00A58190`); filter call `0x00684C4F`; distance `0x00684C5C`-`0x00684C7E` (`0x008D8B60` dot product of the position difference with itself); pruning `0x00684CA8`-`0x00684CF8`; insertion position loop `0x00684D09`-`0x00684D3F` (fcompp + `test ah,0x41`, stops at first stored > new); inserts `0x00684D47` (distances) and `0x00684D56` (ids); step sequence `0x00684D5F`-`0x00684DA5`.
- `0x00684DD0`: sibling of 0x006849C0, not a wrapper: it takes (area, out, pos*, mask, count) and does the same outward scan from `0x0067CD00` (binary search over the x-sorted list by +0x120.x), with only the id-resolves and type-bit tests (`0x00684F5B`-`0x00684F7A`) and a 3-D squared distance (`0x00684F80`-`0x00684FB8`); `ret 0x10`.
- Filter `0x0067FEC0`: IsActive `0x0067FED0`-`0x0067FEE6`; type `0x0067FEEC`-`0x0067FEFE`; tag `0x0067FF04`-`0x0067FF1F`; group `0x0067FF28`-`0x0067FF37`; hostility `0x0067FF39`-`0x0067FF95`; living `0x0067FF97`-`0x0067FFAB` (`0x00695300`: slot 0x120 <= 0); perceived `0x0067FFB2`-`0x0067FFE4`.
- `0x0064B900`: groups at +0x3C0 ordered max/min; flags +0x3B8 and +0x3BC (override: if either creature has +0x3B8 == 1 and the groups differ, hostile iff either has +0x3BC == 1); otherwise the group table `0x0064B830(max)` row contains min (`0x0064B810` -> `0x0064B5B0` != -1). Same function behind `IsObjectHostile` (`0x0064BB40` converts ids to objects with `0x005D58C0`).
- Vtable slots: CCCreature primary vtable `0x00B10484` (119 slots): +0x10 and +0x18 both `0x0079AF30` (the casts to creature and object, `generated/ObjectCasts.h` indices 4 and 6), +0x120 `0x006AB4B0`. CCItem vtable `0x00B1C61C`: +0x120 `0x004E13D0` (`or eax,-1; ret`).
- Script side: `toolset/extract/ldf/cscript.ldf` lines 143-161 (OBJECT_TYPE_*, ALL = 4294967295), 1804 and 1818 (signatures); `ai_constants_h.nss:137` AI_MAX_CREATURES_NEAREST = 5.

#### Not read

- Whether the area's object list (+0x690) is kept sorted by x on every move or only at insertion (the search and the binary search assume it; `0x0067CD00` and the walk are read, the maintenance is not). If it were stale the engine's pruning could miss objects; a port that sorts everything cannot.
- `0x004C0350` (the tag string compare's body): taken as exact, case-sensitive; not read to the end.
- CCPlaceable, CCWayPoint slot 0x120 (only CCCreature and CCItem read), and what the creature slot returns with no stats object (`0x006AB516`/`0x006AB527` branches).
- Slot 0xA0 on the source (the area) and slot 0x18 (cast to object) bodies beyond their vtable entries.
- Why the handler's hostility/group slots are not initialised for the commands that do not pop them (above); the working game shows they behave as 0 and -1.
- What `IsActive` (slot 0x6C of the sub-object at +0x70) does beyond `dev/PARTY.md`.
- The perceived set's contents are as `dev/COMBAT.md` says (not re-read here); whether dead creatures are removed from it, and when.

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
     - Added 2026-10-24 (read, checked by lib-agent-2) [C]: the type test is the follow's own (0x00761038–0x0076103D). 0x0069EF30 only builds the new move. A second test (0x0076104B) reads the current move's sub-action count. With none left, the follow does nothing: a finished move is neither retargeted nor replaced, and the head ends it at the follower's next step.
     - **What the move does with it** (read 2026-10-24, checked by lib-agent-2) [C unless marked]: from its second run, sub-action 14 goes straight to the move update with flags 7 (0x007A9BA9), which reads vector 1 as the goal. Flag 3 then stops, frees and searches afresh from call 1 in that update (PATHFINDING.md, "A retarget is a fresh search").
       - So every re-follow of a follower already walking is a new search. The move update has no "keep walking" case.
       - A re-follow over open ground passes the straight walk on call 1 and walks at once. A non-straight one stands while the setup runs (one or two updates).
       - The only filters come before the move update, in the follow: `ShouldMove`, and the spot being more than 1.0 m away.
       - A follower with the player's order (flag 6) is not retargeted.
       - Whether its walk cycle visibly plays in place while it stands is [I].
   - **Otherwise**, `RemoveCommand(20)` (`0x0073C9E0`) is run first. Then `0x0069EF30` builds a fresh MoveToLocation and queues it with `CCObject::QueueNewCommand` (`0x0073D000`). Its parameters:
     - location 0 = spot;
     - flag 1 = 1, **always run**;
     - float 0 = 1e-6 (`0xB1CE64`);
     - flag 6 = 0;
     - flag 12 = 1.
   - Added 2026-10-25 (read, checked by lib-agent-2) [C unless marked]:
     - **The call** (0x007610A4–0x007610D0) passes `0x0069EF30(spot, run 1, float 0 = 1e-6, flag 6 = 0, int 0 = 0, front 0, static 0, player 0, override −1, flag 9 = 0, flag 12 = 1)`.
       - With override −1, row 20's add behaviour 2 applies (COMBAT.md, "Queuing it clears the queue"; dev/ANIMATION.md, "How a newly queued command affects a running one"). So the new follow move ends a current PlayAnimation (which ShouldMove lets through) with −2, and keeps a static command current.
     - **Flag 12** has one writer (0x0069EFE4) and one reader, 0x007A39F4. The reader is in the loop of 0x007A3810 (ID 19828), the move update's walk over the creatures in the way (0x0073F230's list).
       - It acts when the mover is in the party and the blocker is the controlled creature (0x0069F140, 0x006A7D30). A move with flag 12 then calls 0x006A9430(1), which resets the movement controller, and answers 2, so the move ends as done, not blocked.
       - So a follow move ends when the controlled creature is in its way [I for the meaning; C for the code].
     - **Followers always run.** The follow passes run 1 and never reads the party's run/walk byte (+0x4A8).
       - That byte's readers on a party are the constructor, `ToggleRunWalk` (0x0062EC60), the save (0x006347EC), the load (0x006380FD) and the steering (0x00763A3C). This comes from a scan of all 50 `+0x4A8]` operands; the rest belong to other classes.
       - So a walking leader's followers run.

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

## The party picker (read 2026-10-21 by lib-agent-2 with a subagent, for DAOParagonEngine)

C = read in the disassembly or script source, I = inferred. Addresses are GOG's (base 0x400000). Script paths are under `toolset\extract\scripts\`.

### Short answers

1. **A newly chosen follower becomes ACTIVE (object active flag) in a script, not in the engine's picker.** Submit queues `EVENT_TYPE_PARTYMEMBER_ADDED` (66) for each member that is in the party, and `module_core.nss` (`Core Scripts\module_core.nss`:484-503) runs `WR_SetObjectActive(oFollower, TRUE)` and sets the plot flag `GEN_<TAG>_IN_PARTY`. Event 67 (`DROPPED`) sets `GEN_<TAG>_IN_CAMP` and the same file's `:506-520` ends with `WR_SetObjectActive(oFollower, FALSE)`. The select path 0x00766040 activates nothing (it sets the entry to state 1, refreshes the portrait, fills a slot and plays a sound set: 0x006B5740 is `ScriptBackend::ForScript_PlaySoundSet`, not activation). The only engine-side state change in Submit is `SetFollowerState(id, 2)` for the dropped. (C)
2. **Entries.** The list is the party pool (CCParty+0x244, the pool's insertion order), each a 0x34-byte record, built once in `GameModePartyPicker` 0x0076C170, then sorted by `entry+0xC`, the `ID` column of the `partypicker` table (default 0x7FFFFFFF, so members without a row go last). The hero is an entry, always selected and never toggled. **The byte at +0x10 is "this is the hero" (id == party+0x1E8), not a lock from a script, a plot flag or SetPartyPickerGUIStatus.** The scripts' lock is the follower state 7, LOCKEDACTIVE, kept in `entry+8`. (C)
3. **Exit and Escape (cancel) raise no event at all, 71 included.** 71 is raised only by Submit. 104 (`PARTYPICKER_INIT`) is raised by the engine, on showing the screen, not by a script. (C)

### Q1: activation in detail

- **Accept = `SubmitPartyChangesMessage`, 0x00767060** (ignored while the mode's +0xA0 or +0xA1 byte is set, i.e. while showing or leaving). It calls 0x00766C10, then Exit (0x00765960). (C)
- **0x00766C10 per entry** (walks vector at mode+0x88, stride 0x34, `entry+8` = state):
  - state 2 (AVAILABLE, dropped): if the party's `0x0062F250` is false and the creature exists, 0x0069F470 (frees a pointer at creature+0x78C, returns 1); `SetFollowerState(party, id, 2)` (0x0063B9A0, 0x00766E64); then queues event 0x43 = 67 at once (alloc 0x50, `ev+0x20 = 0x43`, `ev+0x24 = module id`, object 0 = id; delivered through `AIMaster::AddEventDeltaTime` 0x0064ACE0, 0x00766EC7). (C)
  - state 1 (ACTIVE, picked) with slot `entry+0x24` in 0..3: builds event 0x42 = 66 with object 0 = id and keeps it in a local array indexed by slot (0x00766EE9-0x00766F35). No SetFollowerState and no active flag here. (C)
  - state 7: nothing (falls to the loop's next entry). (C)
  - After the loop the 66s are sent in slot order 0..3 (0x00766FBD-0x00766FE5), then event 0x47 = 71 (`PARTYPICKER_CLOSED`) at 0x00767014. So the order is 67s (in entry order), 66s (in slot order), 71. (C)
- **A picked member becomes ACTIVE through the party plot's script** (read 2026-10-21 by lib-agent-2) [C]. The engine never sets it, and neither does `module_core` itself.
  - **The engine:** `SetFollowerState` (0x0063B9A0) holds the only writers of the state map at CCParty+0x2A8 (0x0063A020) and of the active list at +0x234 (0x00637560). None of its seven callers is the click, Submit's picked branch (0x00766ED5..0x00766F9B builds only the 66 event), the picker's teardown, or the CCParty apply handler.
  - **The scripts:**
    1. Event 66 goes to `module_core.nss`:484. It calls `WR_SetObjectActive(f, TRUE)`, then `WR_SetPlotFlag(PLT_GEN00PT_PARTY, GEN_<X>_IN_PARTY, TRUE, TRUE)`; the last TRUE runs the plot's script.
    2. That script is `gen00pt_party.nss`. Its `GEN_<X>_IN_PARTY` case (for example :206–210) calls `SetFollowerInParty` (:53–60).
    3. `SetFollowerInParty` clears `GEN_<X>_IN_CAMP`, calls `WR_SetObjectActive(TRUE)` and **`WR_SetFollowerState(f, FOLLOWER_STATE_ACTIVE, TRUE)`**, then queues `CommandJumpToObject(GetPartyLeader())`.
  - **A dropped member:** the engine sets it to AVAILABLE first (0x00766E64). Event 67's `GEN_<X>_IN_CAMP` case then calls `Party_SetFollowerInCamp` (`party_h.nss`:293–299). That clears IN_PARTY, sets AVAILABLE again and deactivates the object.
  - **The order** [C for the queue, I for the run order]: the 67s are queued before the 66s, so a dropped member leaves the active party before a picked one joins. The 4-member cap is not exceeded.
- **A second path does the same with different data:** `CCParty` handler of `ApplyPartyPickerChangesMessage`, 0x00636F50, walks the party's own list at +0x270 and compares each member's state with a stored previous one (map at +0x2A8, default `+0x2D4`): previous 2 -> now 1 queues 66 (0x00637109-0x00637120), previous 1 -> now 2 queues 67 (0x00637168), then 0x00636090 and a final 71 (0x006371D6). Message class 0x00B1E7E4; constructors seen at 0x007768F5 and 0x0077DC89 (GUI side, not traced). So the GUI/party-side path that applies a change also ends in 66/67/71 for scripts. (C for the code, I for who sends it)
- **Script side** (C): `module_core.nss`:484-520 as above. `sp_module.nss`:240 sends event 71 to `sp_module_ppicker_closed.nss` (read-only status returns; else per-area plot flags or `OpenPrimaryWorldMap` in a camp, `GEN` flags in the Landsmeet and the Climax); `module_core.nss`:531 `WM_SetPartyPickerGuiStatus()` (world_maps_h.nss:222: sets USE or NO_USE from the area's `PARTY_PICKER_ENABLED`).
- `SetFollowerState` 7 and 1 are the only ones the scripts use to lock a member (`sp_module_travel.nss`:338-368: Alistair and Morrigan LOCKEDACTIVE until Lothering; Alistair at the Landsmeet). (C)

### Q2: the entries

- **Build: 0x0076C170**, called once from the show step 0x0076C82E (after fade scene 0x22). For each id in the pool list (party+0x244, count from 0x0062F9B0): creature = `GetCreature(id)`; skipped when the byte at creature+0x610 (`0x00838210` on creature+0x604) is true (the destroying/hidden flag, name not read); state = `GetFollowerState` (0x00632810); `isHero = (party+0x1E8 == id)`. (C)
- **The `partypicker` table** (module's name, default `partypicker`, `SetPartyPickerStage` 858 changes it; `dev/script_commands/plotman.md`:28) is searched for the row whose `Tag` (0x88578372) equals the creature's tag (creature+0xB8). Columns read (hashes from `tools/gda.py` (retired 2026-10-24, DAOLib #106: the tables are read by `dao/core/GdaFile.h`, their columns listed in `dev/gda_schema.csv`)): `ID` 0x66FBC936 -> entry+0xC (default 0x7FFFFFFF), `Stature` 0xF495CE71 -> entry+0x28 (default 1), `BlendTree` 0x70F463FA (string), `VFX` 0x9512145E -> entry+0x2C (default 0). Table columns per `dev/gda_schema.csv` (the Party Picker worksheet): `ID Label Tag BlendTree AddAnimation RemoveAnimation AddSound RemoveSound Stature VFX`. These drive the 3D stage (the figures that appear), not the list. (C for reads; the column-to-field mapping of the constructor arguments is a careful stack count, so I)
- **Entry layout** (ctor 0x00765390; stride 0x34): +0 creature id, +4 the stage's clone creature (made by 0x007662A0), +8 state (1 picked, 2 available, 3 unavailable, 5, 7 locked), +0xC sort key (table ID), +0x10 byte isHero, +0x14, +0x18, +0x1C, +0x20 the row's AddAnimation, RemoveAnimation, AddSound and RemoveSound (corrected 2026-10-24 by lib-agent-2: not visual flags; 0x00767830 reads them from the `partypicker` row, and a follower with no row gets row 0's, the player's), +0x24 slot index (-1 not in the party; 0..3 filled), +0x28 stature, +0x2C VFX, +0x30 a scene/effect handle (-1). (C)
- **Order:** the pool's order, then a sort by +0xC (0x0076AE90 -> 0x0076ABD0 -> introsort 0x00769F60, whose compare is `A.+0xC < B.+0xC`, 0x00767F81-0x00767F9E). 0x0076AE90 then places each clone on the stage ("The 3D stage" below), so it is not a sort alone (corrected 2026-10-24). The hero comes with its own row's ID like any other. (C for the compare, I for it being the only key)
- **Auto-selection:** after the entries are built the second loop (0x0076C47F-0x0076C5D6) puts every entry with `state == 1` or `state == 7` or `+0x10 != 0` into the next free slot (max 4; `mode+0x124` is the slot vector of ids, `mode+0xA2` the count) and sets `entry+0x24`. Locked state 7 members and the hero therefore always occupy slots. The hero is always a party member, so the picker never lists it unselected. (C)
- **Toggle rules** (click `SelectPartyMemberUnderCursor`, 0x00768AE0; and `Unselect` 0x00767100, which skips entries with +0x10 or state != 1):
  - works only while `GetPartyPickerGUIStatus` (party+0x74) == 2 (USE), not while the mode is showing/leaving (+0xA0/+0xA1) and not while the cursor is over a movie (the GUI system's slot 71 at +0x11C, `ScaleformManager::MouseOverInterface` 0x00511420, ID 6593; corrected 2026-10-24 by lib-agent-3: a hover test that routes the click, not a busy flag);
  - entry with `+0x10 != 0` (the hero): ignored;
  - state 1: deselect (0x007661B0, needs mode+0xA2 > 0); state 2: select (0x00766040, needs mode+0xA2 < 4); **any other state (3 UNAVAILABLE, 5, 7 LOCKED) is only highlighted, never toggled**. (C)
- **The "locked byte at +0x10" is the hero, not a script lock.** The scripts' lock exists only as the state value 7 in `entry+8` (from `WR_SetFollowerState(oX, FOLLOWER_STATE_LOCKEDACTIVE)`). The GUI movie's `PartyPicker.LockedPartyMembers` list (`dev/gui_data_paths.csv`:2205) is presumably entries with state 7 or +0x10; not traced. (C for the byte; I for the movie)
- **Caveat (I, matters for a port):** event 104 is queued (AIMaster 0x0064AD80, due now) at 0x0076C827, *before* the entries are built (0x0076C82E). The queue drains later, so the INIT script's `LOCKEDACTIVE` states arrive after the entries copied the states (Alistair and Morrigan in a camp would show as state 1 and be toggleable). The built list is never rebuilt (the build has one caller). I did not see this in play; the scripts' comment "just before it opens" suggests the author expected it earlier. Verify in play before copying.

### Q3: cancel, 104

- **Exit** (`ExitPartyPickerMessage`, 0x00765960): `GUI.slot 0x34(0x22)` (fade scene), `mode+0xD4 = 0`, `mode+0xA1 = 1`. No event, no state change. Submit reaches it too (0x007670BC). (C)
- Escape is the same message (0x00779815 builds the Exit message; its trigger, the screen's Cancel/Escape, not traced). The reset of entries to their opening state on cancel is 0x00766AB0 (it reapplies each entry's original slot visuals), which runs once at show (0x0076C859), not on cancel. So **cancel leaves the pool and states untouched (nothing was applied) and raises no 71**, hence `sp_module_ppicker_closed` never runs and a camp's world map is **not** opened by cancel (`module_core` WORLD_MAP_CLOSED/`Camp_PlaceFollowersInCamp` is the world map's own cancel). (C for the code, I for the effect)
- **104 comes from the engine**, queued in the show step 0x0076C7C4-0x0076C827 (`ev+0x20 = 0x68`, target = the module id, `AddEventNow`), once, when the fade ends (flag +0xA0 cleared at 0x0076C8A2). `dev/script_commands/inventory.md`:217-221 has it. Handlers: `sp_module.nss`:104 -> `module_core_travel` (`sp_module_travel.nss`:338 locks followers) and `module_core.nss`:602 (`RW_CatchUpToPlayer` for every pool member: follower XP catches up to the hero). (C)
- 71 (`PARTYPICKER_CLOSED`) is only raised by Submit (0x00767014) and by the CCParty apply path (0x006371D6). (C)

### The 3D stage (read 2026-10-24 by lib-agent-2 with subagents, for DAOParagonEngine #243)

Read in the GOG exe and the files (ghilana); the camera's constants (0x00C0D704 = 7.75, 0x00C0D708 = 7.0, 0x00C0D710 = 30.0, the aspect test against 1.6 at 0x00B010F0), 0x0076AE90's sort then placing, and the tint colour's two readers were re-read by lib-agent-2. Nothing was played.

- **The mode** [C]: `GameModePartyPicker`'s slot 6 (0x0076CF40) is enter, slot 7 (0x0076BEC0) leave, and slot 8 (0x0076C720) the update. The update runs the show step:
  1. build the entries and clones (0x0076C170);
  2. sort and place them (0x0076AE90);
  3. read the rows (0x00767830);
  4. activate the stage (0x00707040);
  5. set the poses (0x00766AB0);
  6. set the camera (0x007672C0).
- **Enter** [C]: input context `GameModePartyPicker`; the `ambient` and `vfxamb_combat` sound busses set; the scene camera's position, orientation and FOV saved (mode +0xB0/+0xC0/+0xD0); scene 0x22 faded in; the stage's marks gathered (0x0076CC90).
- **The stage** [C]: the module's SharedCharacterStage (+0x388, `char_stage`, layout `oth999d`; CHARGEN.md). It is **switched to**, not drawn apart. 0x00707040 sets area +0x45C = 1, saves the current area at +0x6E8, and makes the stage's scene (+0x62C) the renderer's active one (0x0067C3B0(1)). The camp stays loaded.
- **A clone** (0x007662A0, entry +4) [C]:
  - It is a fresh `CCCreature(-1)`, not in the area's object list. It copies the follower's appearance record (+0x7B0, through 0x0074A610), its sound set and its equipment (0x007B8950, not read).
  - It goes into the stage's scene only (its slot 66 with mode +0x84). The row's `BlendTree` (`pp_<name>.anb`) is loaded through 0x006B5B30.
  - Each frame (0x00765880) it is updated: animation 0x0069EAF0(dt), slot 0x110(dt).
- **Its place** [C]:
  1. The stage mark whose tag is `char_` + the creature's tag, with its position and facing.
  2. Otherwise the next unused mark of the Stature's list: 0 `char_short*`, 1 `char_med*`, 2 `char_tall*`.
  3. Otherwise (0, 0, 0).
  - **Stature is not a scale**; nothing scales a clone.
  - **The vanilla marks:** char_stage.are has only named marks, all at z 0.900991: `char_player` (−2.628284, −3.132766, yaw 28°) and ten companions' `char_gen00fl_<name>` (the stage file lists them all). So an unmatched follower stands at the origin.
- **Its pose** [C]:
  - The anim_base rows are 673 `party_enter` → 674 `party_enter_loop` → 675 `party_exit` → 676 `party_exit_loop` → 673.
  - **Picked (1) and locked (7):** rest in 674.
  - **Available (2) and unavailable (3):** rest in 676.
  - **The hero:** idles normally.
  - The opening poses are played with blend −1.0.
  - **Select** (0x00766040): AddAnimation 673 with blend −2.0 (20% of the clip), then 674; AddSound, `SS_YES` (47), on the clone, at most once in 0.1 s.
  - **Deselect** (0x007661B0): 675 → 676, and `SS_CUSS` (54).
- **Its effect** [C]: the row's VFX (entry +0x2C; only 3042 Morrigan and 3043 Wynne, the crust rows `fxc_mor_gui_c` and `fxc_wyn_gui_c`) is placed on the clone while picked or locked (0x00765F90, handle at +0x30). Its removal is triggered by clone +0xCF4, whose writer was not read.
- **Its tint** [C for the code, I for the colour]:
  - An unavailable entry (state 3) is tinted with amount 0.85 (0x00C0D700) and the colour at 0x00C374B0..B8 (0x00765830 → 0x00765470 → the scene object's slots).
  - Nothing writes that colour statically (two readers only), so it is presumably 0: black.
  - The hover (0x007658E0) passes amount 0, so it shows no tint.
- **The camera** (0x007672C0, once at the show step) [C, numbers checked]:
  - It is reset (0x006C4410), set to mode 0 (0x006C2890(0, 1)) and given FOV 30° (0x006C4B20).
  - With P and F the hero's clone's position and facing, it is placed at `P + d·F + (−0.5)·(F.y, −F.x, 0) + (0, 0, 2.5)`, where d = 7.75, or 7.0 when the aspect is 1.6 or more. It looks at `P + (0, 0, 0.75)`.
  - So it is in front of the hero, looking back at the line. For vanilla `char_player` that is about (−6.705, 3.476, 3.401) looking at (−2.628, −3.133, 1.651) [I that the waypoint's stored quaternion becomes this facing vector].
  - Near and far are unchanged.
- **The pan** [C]: `PanCameraLeft` (0x00765990) and `PanCameraRight` (0x007659E0) slide the camera 1 m along its own right vector over 1 s (curve 3, `SceneCamera::SetPosition` 0x006C45F0). It has no limit, and the orientation is unchanged. Who sends them was not traced.
- **The pick** [C]:
  - 0x007675B0 unprojects the mouse through the camera and casts a physics ray (`World::Raycast` 0x006497A0, up to 32 hits).
  - It skips hits of type 2 with flag 0x20000, and matches a hit's id to each clone's attribute at clone +4 [I: its physics body's id]. It does not use MouseOverInterface.
  - `SelectPartyMemberUnderCursor` (0x00768AE0) ignores its message and picks again.
  - When the hovered entry changes, the GUI's two strings are filled: +0x140 the creature's name, and +0x168 a line from strref 0x4F7B7 with a creature attribute (data-tree paths not traced).
- **The teardown** [C]:
  - **Exit** (0x00765960) only starts scene 0x22's fade and sets +0xA1.
  - **The update** then waits for the fade (or 2 s), hides screen 0x1B and sets +0x98.
  - **Leave** (0x0076BEC0) deletes the scratch waypoint (+0xE0) and destroys each clone (slot 0x108, then its destructor). It restores the saved camera (position, orientation and FOV, at once) and returns to the saved area's scene (0x00707020). It turns the sound busses back off and pops the input context.
  - The stage is never unloaded.
- **Not read:**
  - the marks' order within a list (comparator 0x0076AD50);
  - the equipment copy 0x007B8950;
  - clone +0xCF4's writer;
  - the four `0x00634F20(0)` calls in leave;
  - who sends the pans;
  - the DLC Shale's row (enciphered).

### Not read

- Who builds `ApplyPartyPickerChangesMessage` (0x007768F5 / 0x0077DC89) and what GUI action calls it.
- The meaning of creature+0x610 (picker skips it). The 3D stage's code is read below in "The 3D stage", with what of it is still not read.
- The party picker movie sends only `GameModeController.SubmitPartyChanges` and `ClearPartyPicker` (`PartyPicker.as`:28, :37).
- `ClearPartyPicker` (0x00767230) runs Unselect for slots 0–3, with no event and no state change.
- The +0x270 list that 0x00636F50 compares is filled from the +0x2A8 map at 0x00638640..0x00638992, apparently for a GUI data tree. It is not the picker's entries.
- The Escape key's path to the Exit message; the `LockedPartyMembers` count's source for the movie.
- The 104 timing caveat above in play. Nothing here was run.

Prior work used: `dev/script_commands/inventory.md`:217-232, `dev/script_commands/plotman.md`:28-34, `dev/PARTY.md`:143-165 and :461, `dev/gda_schema.csv` (the Party Picker worksheet), `dev/AUDIO.md`:1445, `dev/CUTSCENES.md`:344 (party+0x1E8 = hero), `dev/LOAD_EVENTS.md`:150-156 (AddEventDeltaTime / AddEventNow). New here: Submit's per-entry order, the entry layout and what +0x10 is, the sort key, the toggle rules, activation by script, the 104 queue-order caveat.

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
- `CCCreature::UpdateAILevel` 0x006A53E0 (`CCCreature` vtable 0x00B10484 slot 85, +0x154) tests in this order [C 0x006A53EB..0x006A5481, read 2026-10-23 by lib-agent-2; corrected 2026-10-23: it was noted as 4, then 2 if +0xC60 holds something, then 3 for the party], and the first that holds decides:
  1. The controlled creature (0x005D5CA0, compared with `GetMainControlled`): **4** (`CSERVERAIMASTER_AI_LEVEL_VERY_HIGH`).
  2. In combat (`+0xC60` non-null and its first dword non-zero): `0x007C49B0(self, 2)`, i.e. **2..4** by the band. This comes before the party test and the area test, so a party member in combat is 2..4, not 3, and a creature in combat in another area is not −1.
  3. In the active party (`CCParty::IsInParty` 0x00631830): **3**.
  4. In the controlled creature's area (`+0x254` against 0x005D5CD0): `0x007C49B0(self, 1)`, i.e. **1..3**.
  5. Otherwise **−1**.
  Then `AIMaster`'s set (0x0064B480 on `app+0x1AC`).
- `AIMaster` 0x0064B480: ignored while a script's `SetAILevel` (+0x2B4) is set; -1 takes it out (0x0064B240); 0–4 moves it to one of five lists of 0x14 bytes (0x0064B400) and stores it at +0x2B0 through slot +0x150. What the lists change is read 2026-10-23 by lib-agent-2 (it was inferred, from Aurora: how often it is updated, not whether):
  - **The chooser for other creatures** [C]:
    - A creature that is neither controlled, in combat, nor in the active party, and is outside the controlled creature's area (`+0x254` differs), gets **−1** (corrected 2026-10-23; the order is above), which is in no list, so it is never updated. −1 is also where a creature starts (the constructor) and goes at destruction.
    - `0x007C49B0(self, base)` is called with base 2 in combat or 1 out of it; the band below is added to it (read 2026-10-23 by lib-agent-2; it replaces an inference):
    - **0x007C49B0** [C]: band 0 adds +2, band 2 adds +1, and any other band adds 0. A dead creature gets base − 1 with no band read. The result is clamped to 0..4. What it reads is the stored level (the next bullet): band 1 is stored as level 0, so +2 [C for the store] (corrected 2026-10-23 by lib-agent-2: it said band 1 is never stored).
    - **What writes it** [C]: the engine's `LodManager` (static at 0x00C33B20, vftable 0x00B03C04). Its pass 0x006184E0 hands each object a band. The model holder's slot 0x44 (`AnimBase` at creature `+0x154`, 0x00A5D5D0) stores the model level the band maps to, not the band (corrected 2026-10-23 by lib-agent-2), at holder `+0x150`, reached through object slot 0x144 (0x0069F180) → 0x00695E00.
      - 0x00A5D5D0 takes the first level of the band's row in 0x00B03B98 (0,1,2,3 / 1,0,2,3 / 2,1,0,3 / 3,2,1,0) whose bit is clear in `[holder+0x15C] | 2` (0x00A5CB10). So level 1 is never taken, and bands 0–3 give levels 0, 0, 2 and 3 [C].
      - It stores the level and reloads the model (0x00A68DB0) only when the level changed. Creature slot 25 (0x00695DE0) reads `+0x150` back, or −1 with no holder [C].
    - **The distance** [C]: 3-D, from the camera's point (`[controlled creature's area +0x658]+0x550`, copied by 0x006170B0) to the model (its node [I]), in 0x006170F0. The point is the explore camera's focus, not the eye or the feet (corrected 2026-10-23 by lib-agent-2; details below). An object in another area, or with no model, is at 1000.
      - **The origin is ChunkManager +0x550** (corrected 2026-10-23 by lib-agent-2; batch 26 had the followed object's feet) (the area's +0x658), copied each pass by 0x006170B0 [C]. Its writers:
        - **The ChunkManager's per-frame update 0x005CA1B0** (from the area's update, 0x0068B035 → 0x0067B200) [C], skipped while +0x580 is set:
          - **The list branch:** with +0x560 set, it takes the +0x10 vector of the first active viewer (byte +0x20) in its list +0x4E8 (0x005CA2B2..0x005CA2E8).
          - **The eye branch:** otherwise it takes the scene camera's eye (`[+0x4E0]` the area's scene, its +0xC0 `SceneCamera`, slot 0x38 → the perspective camera's translation, 0x00465A20).
          - **Which branch runs:** the constructor sets +0x560 = 1 and +0x580 = 0, and only the cutscene player changes them (0x007E3DFE, restored at 0x007E1795). So the eye branch is not reached [I, from a byte scan].
        - **The viewer is CameraControllerExplore's own record** at `ctl+0xB0`. It is registered by 0x005C97F0 from the controller (0x0076FD1F, 0x0076FDC8), with its active byte `ctl+0xD0` set to 1 (0x0076FDB8) [C]. No code that clears that byte was found [I].
        - **Explore's per-frame update** stores the focus with the pan offsets added at `ctl+0x70` (0x00771474..0x00771479), then copies it to the viewer's vector `ctl+0xC0` (0x00771481) [C].
        - **So in explore the origin is the explore camera's focus** [C]: the creature's position (+0x120) plus the controller's pan offsets (CAMERA.md:27). It is not the eye, which is 21 m off in the tactical view.
          - **A tactical pan moves it.** This matches DAOParagonEngine's sightings [S]: Shale's head went to level 2 at 13 m from the panned focus but 6 m from the hero, and Bodahn, 2 m from the hero and 19 m from the eye, stayed at level 0.
          - Close in, with no pan, the focus is the leader's feet.
        - **At a load:** 0x005CA590 (from 0x0067B210: the loading mode's 0x006C5C50, with the followed object's position, and 0x00707417, a zero vector), and the loading mode's own call of 0x005CA1B0 (0x006C91BA) [C].
        - **In a conversation:** 0x0068D4B0 writes the conversation camera's position (0x0068D78F). The per-frame update rewrites it from the explore viewer while that is still registered. Which runs later in a frame was not read [I].
        - **In a cutscene:** the player sets +0x580, so the per-frame update leaves +0x550 alone. 0x007D3A10 (from 0x007E3EE4, 0x007DD8A2) is the only writer: a cutscene actor's sampled position (0x007D2CF0) [C for the writer; whether it is the camera's eye or a target was not read].
      - **Batch 26's line was wrong.** "Camera +0x110, the followed object's position" was 0x006C5C50, which is the loading mode's (its callers 0x006C8223, 0x006C8523, 0x006C8D8E are in the load state machines 0x006C76F0 and 0x006C8800), not a per-frame camera update.
      - It is multiplied by 1.5 for a size ratio under 0.7, or by 0.5 for one over 2.0. The ratio is `[creature+0x7B0]+0x68` / 1.8848 (set at start-up as sqrt(3.5525) by 0x00AD06E0). CAppearance +0x68 = sqrt((2·r)² + h²), with r (+0x60) and h (+0x64) set by 0x007484A0 and 0x007484E0 and filled by 0x00748A40 from `apr_base` (table 2): `PerSpace` for r (default 0.6) and `Height` for h (default 1.8), or `Height_F` when appearance +0x50 == 2; the columns are found by the CRC-32 of the lower-cased UTF-16 name (0xB04DB298, 0x0DD1F4C1, 0x1B23D645) [C; that +0x50 == 2 is female is I] (read 2026-10-23 by paragon-agent for DAOParagonEngine #111, checked by lib-agent-2).
      - It is multiplied by 1.2 for a dead creature that is not the party's.
      - It is multiplied by 0.5 when `[slot 0x24]+0x638 == 1` [I for the meaning].
    - **Thresholds per game mode** (slot 26 of each GameMode, through 0x00616FF0) [C]:
      - Explore, combat and loading (0x0070AB10), and the base mode (0x006942E0): band 0 under 10 m, band 2 under 29 m, band 3 beyond.
      - Cutscene (0x00691E70): 19 and 39 m.
      - **The edges** (corrected 2026-10-23 by lib-agent-2; it said hysteresis on the outer edge only) [C]: 0x00616FF0(i, e, cap) stores, for bands 0–2, the edge e, the edge e + 0.5 and the cap (stride 0x3C, from +0x5C). The base setter gives (10, cap 10), (0, cap 0) and (29, cap 10); the cutscene gives 19 and 39 in place of 10 and 29. The test 0x00618B60, for each of bands 0–2 in turn: under e the object takes that band; under e + 0.5 it keeps its stored level (slot 25) if that is this band or the next, else it takes this band; past band 2's edges it takes band 3.
        - So at 10 m there is no hysteresis: band 1 holds nothing (edge 0, cap 0) and no level 1 is ever stored, so band 0 runs to 10.5 m whichever way the creature moves.
        - At 29 m it is two-way: a creature at level 2 keeps it to 29.5 m, and one at level 3 keeps it down to 29 m. The cutscene's edges are the same at 19 and 39 m.
      - Each band holds objects up to a cost cap of 10 (object cost `+0x244`, default 1.0). A full band pushes objects outward (0x00618370) [I for the effect].
      - Nothing tests the frustum or room visibility.
      - **The class word `+0x240`** (corrected 2026-10-23 by lib-agent-2; it said class 0 goes straight to band 0 outside loading, I: the party's) [C]: the creature constructor 0x00699670 sets it to 2; it is set to 0 by the party add 0x00637560, `AddNonPartyFollower` 0x006342E0 and the list rebuild 0x00635A90, and to 1 by 0x006377E0, `RemoveNonPartyFollower` 0x00631E00 and the model build 0x006C1400. LodManager is its only reader (0x00618AC5).
        - One object's pass, 0x00618A90: past band 2's edge + 0.5 (`[LodMgr+0xD8]`, 29.5 m; 39.5 in a cutscene) any object takes band 3. Short of it, an object of class 0 is put in band 0 when the game mode is 0, explore only (0x00618B15, `GetGameMode`). Everything else is queued by class (`[LodMgr+0x2C+class*16]`) and placed by distance through 0x00618B60, class 0 first (0x00618F80). So combat and the other modes place a party member by distance.
        - Band 0 is not unconditional: the put, 0x006184E0, still applies the band's cost cap, and a full band pushes a member outward, to band 3 at most, which has no cap. Which member is pushed was not settled [I that it is the farthest].
      - A forcing byte at `g_eclipseGame+0x258` is not read.
    - **Rate** [C]: the tick 0x00619060 runs each frame from the game modes' update and recomputes everything every 1/3 s (0x00B03C10). An area load forces a full pass.
    - **Show and hide use the same distance:** a creature is shown under 60 m (0x00BFD440, lowered by graphics detail), and other objects under 60 m (0x00BFD444).
    - So a living creature that is not in the party and not controlled is at 1..3 out of combat in the controlled area, 2..4 in combat in any area, and −1 otherwise.
    - **The level follows at once** [C]: 0x0069F180 calls `UpdateAILevel` (slot 85) when 0x00695E00 reports the stored band changed. It is called on every pass, and no band is stored, when the object's `+0x7BC` is non-zero [I for the meaning].
    - **Its other callers** [C]: `SetLocation` 0x00696F90, `SetCombatState` 0x006AE0A0, control changes 0x0069D540 and the party code 0x006378F8.
    - Placeables have their own chooser, 0x0074D840, giving −1 or 1.
  - **What the level changes** [C]: mainly how often the creature is updated, through the AIMaster pass's time slice (TURN_BASED.md, "The throttle is the AIMaster's time slice"). `CCCreature::Update`, `StepCommands` and perception do not read it. The other readers of `+0x2B0` are:
    - the idle twitch (0x006AB73E): below level 2 it needs a party member within 50 m;
    - `RequestPath` (0x0073EDEA): level 4 doubles a step factor passed to `MovementController::Step` [I: a search time slice];
    - the approach's standing counter (0x007A97DA): skipped at levels 0 and 1, so such an approach never fails with −4 for standing still;
    - 0x007F6506: it sets a level-0 creature to 1 [I for when];
    - `GetAILevel`'s handler 0x00659DE0;
    - the creature's load from a field `AILevel` (0x0069B160), which also sets the script lock `+0x2B4`. No readable `.utc` has the field [I: saved creatures].
  - **The scripts** [C]: no script calls `GetAILevel`. `SetAILevel` is used at `rules_core.nss`:1197–1198 (resurrection: HIGH, then −1 to unlock) and `effect_resurrection_h.nss`:40 (−1); `den300pt_last_request.nss`:60 sets HIGH on a world-map location, not a creature. A death case setting level 0 (`rules_core.nss`:1175) is commented out.
- Run on: control changing (0x0069D540, old and new), a member leaving the active party (0x006378F8), and elsewhere.

**"Controlled → no AI" is the scripts'**, in `ai_main_h_2.nss`, through `IsControlled` (so for every selected member):
- `rules_core.nss`:700–800, `EVENT_TYPE_COMMAND_COMPLETE`: for any follower, controlled or not, `AI_DetermineCombatRound` runs (:777–797).
- `AI_DetermineCombatRound` (`ai_main_h_2.nss`:487) — a follower with party AI off (`IsPartyAIEnabled`) gets `_Partial`; :632 **a controlled creature returns at once out of combat, and in combat gets `AI_DetermineCombatRound_Partial`**; everyone else gets the tactics.
- `AI_DetermineCombatRound_Partial` (:959): attack the selected target (`GetAttackTarget`) or the last target when it is in melee reach with a melee weapon, or in range with a ranged one after an attack; **for the controlled creature nothing else** (:1022, it returns with no command), so it stands until the player orders it.
  - Added 2026-10-25 (read by paragon-agent-2's helper, checked by lib-agent-2) [C]:
    - **The party's Wait(1.0):** `AI_SetPartyAllowedToAttack(TRUE)` (`ai_main_h_2.nss`:3357–3370) queues `CommandWait(AI_DO_NOTHING_DELAY_QUICK)` (1.0 s, `ai_constants_h.nss`:183) on each party member other than the caller that has no current command and an empty queue.
      - **Its callers:** `rules_core.nss`:663 (EVENT_TYPE_ATTACKED on a follower), :175 and :209 (COMMAND_PENDING of the controlled creature's attack or hostile ability), and `ai_main_h_2.nss`:1037 (Partial with a hostile target) and :2079 (a controlled creature's tactic on a hostile while not yet allowed).
      - So the controlled creature gets the Wait only through another member's call (:663, :1037).
      - A queued move (row 20's hard clear) ends that Wait with −2.
    - With nothing to do, Partial returns with no Wait for the controlled creature (:1016–1022, "keeping queue empty"), and so does `_AI_DoNothing` (:3224–3229).
- `rules_core.nss`:662–683, `EVENT_TYPE_ATTACKED`: a follower in combat with no current command runs `AI_DetermineCombatRound(attacker)`, so a controlled creature that is struck while idle attacks back.
- **A follower that is not controlled loops on a perceived target it cannot reach** (read 2026-10-25 by paragon-agent-2's helper, checked by lib-agent-2) [C for the lines; I that the loop is seen in vanilla]:
  - It never runs `AI_DeterminePathBlockedAction` (`rules_core.nss`:756–771 runs it only for a controlled creature or one that is not a follower). So it opens no door.
  - After the −9, `AI_DetermineCombatRound` resumes the tactics at the next row (:693–708). At the last row, or for a failed default attack (`_AI_ExecuteAttack`, :2612–2735), it Waits (`_AI_DoNothing`, :3207–3285).
  - Its next round starts at row 1 again. `_AI_GetEnemies` (`ai_conditions_h.nss`:2451–2577) takes `GetNearestObjectByHostility(…, PERCEIVED, …)`, filtered only by distance from the main controlled creature and by active abilities, with no test of reach.
  - Only a creature that is not a follower lowers its threat for a target it cannot attack (`AI_Threat_UpdateCantAttackTarget`, :3232–3236).
  - So the same target is chosen again. The scripts' only guard is that such a target is not perceived (COMBAT.md, "Perception through doors").
  - **The same holds for a location from `GetClusterCenter`** (445), whose candidates have no perceived or sight test (effect_access.md). A cluster behind a shut door can be cast at, and the approach ends −9 at the door.
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
| 0x00638560, from the client load's step 0x15 (0x006C76F0, at 0x006C8276, right after the party is placed at 0x006C8264; `dev/LOAD_EVENTS.md`) | (party+0x208, 0, 1) | when party+0x208 is not -1 (checked; inferred: the controlled id kept across the load) |
| 0x006D7DE0, in `CClientVirtualMachineCommands::InitializeCommands`' table (0x006DBBBE) | (id, 0, 1) | a client-side `SetPrimaryControlled`: `SetControllable(1)`, then only for a creature that exists, is controllable and alive (checked) — unlike 815's handler, it does not crash on an invalid object. When the client VM runs it was not read |

The hero's creation does not select it (corrected 2026-10-19 by lib-agent-2; this said it set the controlled creature directly): `CCPlayer` 0x0069D910's only call to 0x0069D540 (0x0069DA2B) passes null on a failure branch, before the creature is deleted. The hero is selected because 0x0069DF10 calls `SetFollowerState(hero, 1)` at 0x0069E015, and 0x00637560, with the active party empty, makes it the leader and calls `SetSelection` (0x006376BB). Both calls checked; "The selection: where it starts" below.

### Selection: growing, shrinking, all

Read by the forked subagent; checked unless marked. +0x184 holds the ids; +0x194 mirrors them as creature pointers (every writer updates both; 0x00635A90 rebuilds +0x194), and each writer ends with +0x194's slot 0x24, which runs the observer 0x00637220. **So control is always the selection's first entry, whoever wrote it; appending never changes it** (0x0069D540 returns at once when the creature is the same, 0x0069D55A, checked).

- **`Party::Select` 0x00639E50**, `__thiscall (id, bool add)`, `ret 8`. Nothing unless the creature is controllable (+0x7F9) and alive (slot +0x1D0 = 0).
  - +0x7F9 is written only by CCCreature's constructor (0x006BA402, to 1) and SetControllable (0x006A5F46) [C for a scan of .text for the displacement 0x7F9, every other hit a read; I for completeness: a write through a computed pointer would be missed]. What keeps a hostile unselectable stays [I] (lib-agent-2, 2026-10-24).
  - add = 0: 0x006368F0(id, 1, 1), replacing the selection, so it becomes controlled; on success **ON_SELECT (89)** runs at once through `RunEventScript` (0x00639F58), the id in both object slots.
  - add = 1, a toggle: already selected with others → deselected (0x00639DB0); else appended (0x006355A0(id, 1), to +0x184 and +0x194 if missing). No event; control unchanged.
- **Its callers:**
  - 0x00639F90 is `CCParty::HandleMessage_SelectCharacterMessage` (built at 0x0063A2C1 in 0x0063A2B0, called from 0x0063B837; vftable 0x00B058E0; in `dev/message_handlers.csv` since DAOLib #188): `Select(msg+4, 0)`. Sent as the binding text "SelectCharacterMessage 219" through Eluvian (2026-10-23, lib-agent-3) it reached neither this handler nor the selection, where "SelectPartyMemberIndexMessage 1 0" did both. Why (read 2026-10-24, DAOLib #253): a binding's text reaches only what the current game mode handles, and explore mode has no SelectCharacter handler; the argument is read as an unsigned decimal (the class's text reader, 0x007749C0). Sent as "Party.SelectCharacterMessage 19", a movie's text naming its receiver, it reached this handler with Leliana (examples/messages).
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
  - **In detail** (paragon-agent's reading for DAOParagonEngine #34, checked by lib-agent-2, 2026-10-24; static, not played):
    - **The start** [C]:
      - 0x00712450 polls inputs 48 and 49 each frame (is-down, not an event). The frame of the press records the cursor (input 43) at 0x00C36010/14.
      - A drag begins once `|cursor.x − start.x| × 2 > 7`, i.e. 4 pixels or more across. Only x is tested, and there is no time.
      - Nothing in it reads the camera's mode, so it works in the close view as in the tactical [C for the absence; I for "works"].
      - A gate asks slot 0x40 of `[[app+0x114]+0x60]` or `explore+0x5C == 1`; that object was not identified [I].
    - **The box's look** [C]:
      - It is `GraphicsStructures::SelectionBox` (vtable 0x00AF8C44, constructor 0x005551B0), not a Flash movie.
      - The GUI system's constructor makes it (0x005243CD–0x005243DF) with the colour (0xFF, 0xCC, 0x00, 0xFF), kept at GUISystem +0xDC: opaque gold [I for the channel order].
      - It has five vertices of 0x14 bytes, a closed outline [I], with the material "ScreenSpace" (0x00AF8C50). The corners are turned from pixels to clip space by 0x00554E80, and it is drawn by 0x00554DA0.
      - It shows from the frame the drag begins.
    - **Which point is tested** [C]:
      - It is the creature's origin (+0x120..+0x128, w = 1), projected by the scene camera's slot 0x48 (0x00465700 → 0x00465BB0): x = W(0.5·ndc.x + 0.5), y = H(0.5 − 0.5·ndc.y). A point behind the camera (clip z < 0) is not in.
      - The rectangle test 0x0070A9B0 is half-open.
    - **Shift** (input 49) keeps members already selected even when they are off the box or the screen (0x006316D0). Without it, only those inside are kept [C].
    - **The release** uses the last held frame's list, not a new one. A non-empty list replaces the whole selection (0x00635630(party, list, 1)), and nothing happens if any id in it has no creature [C].
    - **Control** goes to the list's first member, in active-party order. It does not stay with the old leader when it is still selected, so with Shift a boxed member earlier in the order takes control [C].
    - **An empty box** leaves the selection as it was; a box never deselects. Under 4 px no drag starts, so the click's own handlers act. The "over 2 px" flag 0x00C35E76 is set but never read, so a drag once started always commits. A box 4 px wide and 0 px tall selects nobody [C].

### The selection: where it starts, when it is empty, what to use in place of "empty = hero" (lib-agent-2, 2026-10-19)

Read in GOG's DAOrigins.exe (capstone via tools\daoexe.py and a byte scan for call sites) and the toolset scripts. Not seen in play, except one corroboration from DAOTurnBased (below). **Checked** = read in disassembly or scripts; **inferred** = from shape or names. Prior notes: master's `dev/PARTY.md` "Control and selection, as the engine runs it" (lines 393-478, the caller table 462-476, "Selection: growing, shrinking, all" from 480), `dev/COMBAT.md` 243, 272-277, `dev/LOAD_EVENTS.md` 98, 111, `dev/gff4_fields.csv` 1829, 1837. Short names: SetSelection = CCParty 0x006368F0 (ID 12442); Deselect = 0x00639DB0; Observer = 0x00637220; Bar = CCParty+0xC8.

#### Short answers

1. **The hero enters +0x184 when the engine makes it ACTIVE, not through any script.** Chargen's end (0x0069DF10) creates the hero (0x0069D910, which does NOT select it) and calls `SetFollowerState(hero, 1)` (0x0063B9A0) at 0x0069E015. That goes to 0x00637560, and an empty active party makes its first member the leader and the selection `[hero]` (SetSelection(id, 0, 1) at 0x006376BB); the observer then derives the controlled id (CCPlayer+0x5C). No script at chargen end or module start sets control or the leader (the only `SetPrimaryControlled` in the scripts is `bhm600pt_harrowing.nss`:266, a mage-origin tutorial step). **A save does not store +0x184.** It stores one id, `SavegameSelectedCharacter` (16270) = the controlled creature at save time (-1 if none). The load reads it to party+0x208; the client load's step 0x15 places the party, then `0x00638560` (called at 0x006C8276) does SetSelection(party+0x208) if it is not -1, replacing whatever the roster's joins selected. So a "whole party selected" state does not survive a save. **An area transition touches no selection** (none of the 11 callers of SetSelection is a transition's); it stays as it was (inferred from the caller list).
2. **Yes, the selection can be empty or hold only a dead member, with members alive.** (a) A selected member's death never empties it: Deselect keeps a sole selected member and asks for the next character; if no other living controllable one exists the selection stays on the dead one. (b) Empty (-1) happens with: `SetControllable(false)` on the controlled one when no other controllable living active member exists (0x00636C00 then SetSelection(-1)); a controlled member removed from the active party with nobody else active (forced, 0x006377E0; unforced it refuses the sole member); a load with saved -1 before any join. (c) Control is never given to "the hero" by default anywhere; the hero is only first in the active party order.
3. **Use: controlled = first entry of the selection (the engine's own definition), -1 when empty. Never substitute the hero.** At a new game: `[hero]`. At a load: `[saved controlled id]` (or the first ACTIVE member that joined, if the save had -1). On a death: the rule below.

#### Pseudo-code a port can copy

```
// state: activeParty (ordered, +0x234), selection (ordered ids, +0x184), leader (+0x1E8), bar = first 4 of activeParty
//        (minus members whose flag creature+0x604's byte +0xC is set; inferred meaning: hidden from the bar), -1 for empty slots
controlled() = selection.empty() ? INVALID : selection[0]          // 0x00637329-0x0063736A; GetMainControlled = CCPlayer+0x5C

SetSelection(id):                                                   // 0x006368F0
    if selection == [id]: return false
    selection.clear()
    if id is a creature: selection.push(id)                         // id == -1 leaves it EMPTY
    // observer: controlled = selection[0] or -1; SelectedChar token, camera focus, AI level

AddToActiveParty(m):                                                // 0x00637560, from SetFollowerState 1/7
    if m not in activeParty (+0x234) and not in followers (+0x260):
        if m.isPC (creature+0x7D4 == 1) or m == leader: activeParty.insert(0, m)   // hero is always first
        else if m.isPC == 0: activeParty.push_back(m)
        else: insert before the first member whose isPC value is smaller (rank order; not exercised by hero/companions)
    if activeParty.size() == 1: leader = m; SetSelection(m)          // only when the party was empty

NewGame:  hero created -> AddToActiveParty(hero)  =>  selection=[hero], leader=hero, controlled=hero

SaveGame: write SavegameSelectedCharacter = controlled()           // -1 if none; the selection list itself is not saved
LoadGame: roster joins run (first ACTIVE joiner is selected if party was empty) ...
          step 0x15 places the party, then: if saved != -1: SetSelection(saved)   // no controllable/alive check

OnMemberDeath(m):                                                   // DEATH handler 0x007AAC10 -> 0x007AB4C2, after the death animation, m already IsDead; only if the creature's `IsInParty` byte (creature+0x504) is set
    Deselect(m)
Deselect(m):                                                        // 0x00639DB0
    if m not in selection: return false
    if selection.size() <= 1: SelectNextCharacter(+1); return true  // NOTE: does not remove m
    selection.remove(m)                                             // controlled = new selection[0]
SelectNextCharacter(+1):                                            // 0x00636CD0
    cur = controlled(); i = bar.indexOf(cur); if i == -1: return false
    loop k = i+1, i+2, ... (mod bar.size):                          // bar order = active party order, hero first
        e = bar[k]; if e == -1: continue
        if e == cur: break                                          // wrapped to itself: accepted, no change
        if e.controllable (+0x7F9) and not e.IsDead: break
    SetSelection(bar[k])                                            // bar[k] == cur => "already so", selection unchanged (may be a dead member)
// with several selected: m is just removed; the next in selection order becomes controlled, no bar search.
// Whole party dead (game over): selection = the last member left selected (a dead one); controlled = that id. Nothing clears it.

SetControllable(m, false):                                          // 0x006A5F40
    m.controllable = false; Deselect(m)
    if controlled() == -1: first = first controllable living member of activeParty (+0x234 order); SetSelection(first or -1)
RemoveFromActiveParty(m, force):                                    // 0x006377E0 (SetFollowerState 0/2/3/5)
    if !force and activeParty == [m]: refuse
    activeParty.remove(m)
    if m == controlled(): SetSelection(activeParty.empty() ? -1 : activeParty[0])   // first active, dead or not
    else: selection.remove(m) if present
```

Port rules that follow: a death of the controlled hero with a living controllable follower passes control to the **next bar slot after the hero's** (active party order: hero first, then companions in join order, wrapping), skipping slots that are empty, uncontrollable or dead; with several selected, to the next in the selection. With none such it stays on the corpse (controlled = the dead hero). A "hero" fallback for an empty selection is wrong both ways: an empty selection means nobody is controlled (-1), and a non-empty one may be a follower or a corpse. Compare ids; do not infer "hero".

#### Evidence

##### Q1 start of the selection

- **Observer derives control** (PARTY.md:430-444 region, "Observer"): checked there; I did not redo it. Selection first entry -> CCParty::SetControlled 0x0062F260 -> CCPlayer 0x0069D540, the only write of CCPlayer+0x5C (0x0069D58E).
- **0x0069D910 does not select or set control on success** (checked, corrects PARTY.md:478 "calls 0x0069D540"). It builds the creature (0x00413660 size 0xD20, ctor 0x006B9ED0), sets creature+0xBDC, calls 0x0069D790; its only call of 0x0069D540 is at 0x0069DA2B, **with argument 0 on the failure branch** (`push 0` at 0x0069DA27; 0x0069D540 returns at once on a null arg, 0x0069D554), followed by the creature's destructor (vtable slot 0, arg 1) and a null return. On success it returns the creature (0x0069DA3E). So the hero is not made controlled by creation itself.
- **0x0069DF10 then joins it** (checked): 0x0069DFB8 `call 0x69D910`, tag `player_core` (0x0069DFCE-0x0069E003), then `push 1; push creatureId; call 0x63B9A0` at 0x0069E008-0x0069E015 (COMBAT.md:277 has the same call, "the hero is ACTIVE from its creation"). 0x0063B9A0 states 1 and 7 -> 0x00637560 (PARTY.md:150, 163).
- **0x00637560** (checked, disassembly above): `lea esi,[edi+0x234]`, Contains in +0x234 and +0x260 (0x008663A0) else skip to 0x006377CB; then group copy; then creature byte +0x7D4: `== 1` -> insert at index 0 (`0x006CE860(…, 0)`, 0x0063766F); the id equal to the leader attribute (+0x1E8, 0x006375F5) -> index 0; byte 0 -> push_back (0x006F1340); other values -> insert before the first member with a smaller byte (0x00637618-0x0063765C, inferred what the byte ranks). **If the list is then of size 1** (0x0063769E), the leader attribute is set (slot 0x54 at 0x006376A9) and `SetSelection(id, 0, 1)` runs (0x006376BB). PARTY.md:473 has this row; the hero-first insertion and the size test are new.
- **Scripts** (checked, grep of `toolset\extract\scripts` for SetPrimaryControlled and SetPartyLeader): the first only at `bhm600pt_harrowing.nss`:266 (after the valor-sword step, `SetPrimaryControlled(oPC)` then `BeginTrainingMode`). SetPartyLeader only in the Fade (`arl300pt_fade.nss`:87-229), `cli410`, `cli800`, `epi_attendees_h`:440, `denpt_captured`, `denpt_main`. None at chargen end or a module start. In the Fade the hero is made UNAVAILABLE after Morrigan/Wynne join; the hero leaves the active party, so by 0x006377E0 the selection moves to the first active member: **the controlled creature is a non-hero for a whole area**, which the "empty = hero" read would miss.
- **Save** (checked): the party's save at 0x00634610-0x00634651 writes field 0x3F8E (16270, `SavegameSelectedCharacter`, gff4_fields.csv:1829) = `0x005D5CA0` (GetMainControlled) or -1 when the app is null (0x00634632-0x0063463F); the same function writes 0x3F96 (16278 `SavegamePartyLeader`) = the leader attribute (0x00634610 region; the leader value's read at 0x00637ED3-0x00637EDD, passed to SetPartyLeader 0x00636180). A second save routine at 0x00637A60-0x00637A97 pushes the same labels (`0x3f96`, `0x3f8e`); no label for +0x184 exists in either.
- **Load** (checked): the party's load 0x00637D50 reads 0x3F96 to SetPartyLeader (0x00637EDD) and 0x3F8E into party+0x208 (`mov [ebp+0x208], eax` at 0x00637F0D; the constructor's init at 0x0063A6BE writes the same field from ebp, -1 inferred). Only 0x00637F0D and the constructor write it (byte scan 0x628000-0x640000 for `[reg+0x208]`: 0x0062CAAE push only, 0x00637F0F, 0x00638562 read, 0x0063A6C0). Reads: `0x00638560`: `mov eax,[ecx+0x208]; cmp eax,-1; je ret; push 1; push 0; push eax; call 0x6368F0`. Its only caller is 0x006C8276, after `0x006C73C0` (the party placed, step 0x15, 0x006C8262-0x006C8264) and before the state moves to 0x16 (0x006C827B). LOAD_EVENTS.md:98, 111 cover 0x006C73C0. Roster restore is the creatures' own loads: `CCCreature` slot 34 0x006BE980 calls 0x0063B9A0 (call site 0x006BEAA1; COMBAT.md:274), so ACTIVE members join in load order, the first selecting itself in an empty party; step 0x16 then overrides with the saved id.
- **Chargen's end and an area transition** use kind-7 requests (LOAD_EVENTS.md:62), which skip step 0x15 (that state machine 0x006C76F0 runs for kinds 2 and 8: inferred from LOAD_EVENTS.md:62). SetSelection's 11 call sites (byte scan for `E8` to 0x006368F0): 0x00636CA6/CBC/DFB (0x00636C00 and 0x00636CD0), 0x00636F0D/F29 (ToggleFullPartySelect), 0x006376BB, 0x00637882, 0x00638570, 0x00639F02 (Party::Select), 0x006610C2 (command 815), 0x006D7E52. None in a transition, so a transition leaves the selection alone (inferred).

##### Q2 shrinking the selection

- **Death** (checked): the sub-action handler 0x007AAC10 runs the DEATH script event at the end of the death animation (COMBAT.md:243), then at 0x007AB49D-0x007AB4C2: `lea ecx,[ebp+0x4F8]; call 0x838210` (reads byte [ecx+0xC] = creature+0x504; `IsInParty`, ACTIVE in the party: checked 2026-10-22, see "The run/walk toggle" below; COMBAT.md says "for a party member"), then `ecx=party (0x006A2CC0: [[app+0xD0]+0x58]); push creatureId; call 0x00639DB0`. At that time the creature is already IsDead (health 0).
- **Deselect 0x00639DB0** (checked, disassembly above): `0x008663A0(+0x184, &id)` Contains; absent -> return false (0x00639E43). Present and the creature exists: if the vector has `<= 1` entries (`cmp eax,1; ja 0x639E0C`) -> `0x00636CD0(1)` and return true **without removing** (0x00639DFA-0x00639E09); else remove from +0x184 (`0x004E9640` at 0x00639E13) and +0x194, and run +0x194's slot 0x24 (the observer) (0x00639E2C-0x00639E3A).
- **0x00636CD0(dir)** (checked): `bar = party+0xC8` (0x00636CED); empty bar (0x0086BEE0) -> false; `cur = GetMainControlled()` (0x00636D0B); `i = index of cur in the bar` (0x0063C4B0, -1 -> return false at 0x00636D35); loop: `idx = (idx + n + dir) mod n` (0x00636D43-0x00636D4D, `div ebx`); skip an entry of -1 (0x00636D6E); an entry equal to `cur` is accepted before any other test (0x00636D96 `je 0x636DE8`); else the creature must have +0x7F9 != 0 (0x00636DC7) and slot 0x1D0 (IsDead) == 0 (0x00636DD4-0x00636DE2), else the loop goes on. Accepted: `SetSelection(bar[idx], 1, 1)` (0x00636DFB). With nobody else qualifying the loop reaches `cur` itself, SetSelection(cur) finds `selection == [cur]` and does nothing (PARTY.md step 1): **the corpse stays selected and controlled.**
- **The bar's order** (checked): 0x00635B60 (callers 0x0063623B, 0x006373B5, 0x006BBED9) builds Bar = party+0xC8 as 4 slots from the active party list +0x238, in its order, for up to 4 entries, **skipping a member whose creature+0x604 flag byte (+0xC) is set** (0x00635D2C-0x00635D39; what the flag means was not read), slots without a member stay -1 (0x00635C04, the default 0xC313C0 is -1). It marks selected slots by Contains(+0x184) (0x00635E17), living by slot 0x1D0 (0x00635D11-0x00635D1D) and slot 0x190 (not named). The hero is first in +0x238 (0x00637560 above), so first in the bar, then companions in join order. PARTY.md:469 had "the party bar's list at party+0xC8 (inferred: the bar)": now checked to be a view of the active party.
- **0x00636C00** (checked, disassembly above): walks +0x238 from the front: `GetObject` (0x005D58C0), keep the first with +0x7F9 != 0 and IsDead == 0 (0x00636C62-0x00636C79), then SetSelection(thatId, 1, 1) (0x00636CA6); at the end of the list: `SetSelection(-1, 1, 1)` (0x00636CAE-0x00636CBC; the -1 is the vector's static default 0xC313C0). So **-1 = empty selection** when no active member is controllable and alive. Its only caller is 0x006A5F80, in `SetControllable` 0x006A5F40: for false, Deselect(self) (0x006A5F65), then **only if GetMainControlled == -1** call 0x00636C00. So with a sole selected member made uncontrollable the 0x00636CD0 pass leaves it selected (it accepts itself), `GetMainControlled` is not -1, and 0x00636C00 is not run.
- **0x006377E0** (checked, disassembly above): not in +0x234 -> skip; unforced (`[esp+0x10]==0`) and list size `<= 1` -> return 0 (0x00637819); else remove from +0x234 (0x00637828), and `if id == GetMainControlled` (0x00637853) -> SetSelection(first of the remaining +0x234 or -1, 0, 1) (0x0063785A-0x00637882), "first" not tested for alive or controllable; else, further down, the +0x194 pointer list loses the creature if it contained it (0x00637962-0x0063798A). Callers: 0x0063B9BB/B9F3/BB72/BB85 (SetFollowerState), 0x006BB120 (a creature's destruction path, with force 1: the one that can empty the party).
- **Set -1 empties** (checked): SetSelection step 3 clears the vector (PARTY.md:408), step 4 pushes only when `id != -1` and a creature. Then the observer: controlled = -1 (PARTY.md:437 "or -1 if empty").
- **Whole active party dead:** nothing runs once the last Deselect finds one entry; selection = the last remaining selected member, dead; GetMainControlled = its id. Inferred from the above: no other writer of +0x184 exists that a death reaches (SetSelection's callers are listed in PARTY.md:462-476 and above).
- **Played corroboration** (DAOTurnBased, `src/game/Bridge.cpp`:589-600): in the Ortan Thaig fight of its issue 42, "with the hero dead and the last of the party standing 83 m from her body, GetPartyLeader answered the hero (IsDead 1) and GetMainControlled the survivor". Consistent with: death moves control to a living follower, the leader attribute stays the hero's. (A comment there; the log is not in what I read.)

##### Q3

See pseudo-code. Order rule on death: with a sole selected hero the next is the first living controllable slot after the hero's in the bar; (hero first in the bar) so Alistair-or-whoever joined first. With a multi-select it is the next selected id in the +0x184 order.

#### Not read

- The meaning of creature+0x610 (the bar's skip) and slot 0x190 in the bar build. The byte at creature+0x504 is read: `IsInParty`, in the active party list (the run/walk subsection below; corrected 2026-10-23 by lib-agent-2: it said ACTIVE).
- Who sets creature+0x7D4 on the hero before 0x0063B9A0 runs (the insert-at-0 branch also fires through "id == leader"; with an empty list either branch gives the same result).
- The 0x0069DF10 loop at 0x0069E01C-0x0069E023 (it re-tests a party state after the join; might create more than one member) and 0x0069DBB0's purpose (a load, inferred).
- Whether the loader of kind-7 requests (new game, area transition) runs any step that calls SetSelection indirectly (I used the caller list: none).
- Whether the leader attribute or the selection is re-set at the Fade's end and the Harrowing by other engine paths (e.g. CCCreature slot 86 0x006A8AC0 re-adding a PC as ACTIVE, PARTY.md:~432; its flow after the join was not followed).
- The client-side `SetPrimaryControlled` 0x006D7DE0 (when the client VM runs it).
- No game run; none of this is seen in play.

### The run/walk toggle, the creature flag byte +0x504, and the Fade (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169)

- **Run/walk, party +0x4A8** [C]: the `CCParty` constructor (0x0063A3C0, ID 12488, unnamed) writes `mov byte ptr [esi+0x4A8], 1` at 0x0063A8A6, so a new party runs. `ToggleRunWalk` (`CCParty::HandleMessage_ToggleRunWalkMessage`, 0x0062EC60) flips it (`cmp [ecx+0x4A8],0; sete`). The party's save writes it as field 16294 `SavegamePartyRunInDriveMode` (0x006347EC-0x006347FB, after a presence test), and the load (0x00637D50) reads it back at 0x006380EA-0x006380FD (`cmp al,1; sete cl`), keeping the constructor's 1 when the field is absent. The move order's `run` argument is this byte (this file, "0x00764020 ... the party's group move 0x006389F0(point, run)").
- **The creature byte +0x504 is `IsInParty`** [C]: the death handler's gate (0x007AB49D, `lea ecx,[ebp+0x4F8]; call 0x00838210`) reads the bool value (+0xC of the object at creature +0x4F8, so +0x504). The three bool objects at creature +0x4E8, +0x4F8 and +0x508 are built by 0x00478190 in the creature's constructor (0x006BA02E-0x006BA05A, default 0) and registered for the interface under the names `IsHostile` (string 0x00B10298), `IsInParty` (0x00B10284) and `IsDead` (0x00B10274) by the reflection function at 0x006B4859-0x006B4970 (each name is pushed just before its member's registration). `0x00838210` is a generic `mov al,[ecx+0xC]` getter that the linker folded with `Physics::PhysicsControllerBox`'s slot 14: it is not a physics call. The value is written at 0x006BD51E-0x006BD52E in 0x006BD4D0 (ID 14946, unnamed; a creature method taking a float, after `0x006A5200`): `CCParty::IsInParty(id)` (0x00631830) `== 1`: it returns a bool, whether the list +0x234 holds the id (`call 0x008663A0; movzx eax, al; ret 4`), so it is "in the active party list" and states 1 (ACTIVE) and 7 (LOCKED_ACTIVE) both count, since SetFollowerState puts both in +0x234; so are the creature's byte +0x504 and 0x006A2CE0 (corrected 2026-10-23 by lib-agent-2: it said the follower state ACTIVE). So the earlier "a party-member-style flag" (this file, the Death reading and "Open questions") is now read: it is "ACTIVE in the party", the same test `0x006A2CE0` (ID 14471, unnamed: `CCParty::IsInParty(own id) == 1`) answers live.
- **Where else +0x504 gates** [C]: `CCCreature::Update` (0x006BE4B0) copies it into +0xC95 at 0x006BE7B6-0x006BE7E8 (with a second test, `0x00631870`); the health-change handler 0x006BC4B0 recomputes the party's tracking (0x00631F30) after a death (0x006BC51B-0x006BC533) and after a revive (0x006BC5A3-0x006BC5C5) only when it is set; `0x006BA810` and `0x006C16E0` (+0x188, with +0x7F0) read it too. The death handler's Deselect call (0x007AB4BB-0x007AB4C2, `CCParty::Deselect` 0x00639DB0) therefore runs only for a creature that is ACTIVE in the party.
- **The Fade's UNAVAILABLE hero** [C]: already read in this file ("Q1 start of the selection", Scripts: in `arl300pt_fade.nss`:87-229 the hero is made UNAVAILABLE after Morrigan and Wynne join, so by 0x006377E0 the selection moves to the first active member). A new game's join (0x0069DF10 -> `SetFollowerState(hero, 1)` at 0x0069E015 -> 0x00637560) is unaffected: the hero is ACTIVE from chargen's end until a script makes it UNAVAILABLE.

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
    - Added 2026-10-25 (read, checked by lib-agent-2) [C unless marked]:
      - **The retarget in place** also sets the existing move's player flag to 1.
      - **The queued move** passes flag 9 = 0 and flag 12 = 0, with override −1, so row 20's hard clear applies: a Wait or a PlayAnimation is ended with −2 at once, and a static command is kept.
      - **A third caller of 0x0069EF30,** 0x006AC20B, queues a move with front 1, player 1, override 2 and flag 9 = 1. What it is was not read [I].
  - **ON_ORDER_RECEIVED goes to the controlled creature only**, not to the members moved in formation.
- **The formation table:** 0x007605B0 picks by the global 0x00C371B0: 1 → 0x00C0D3A0, 2 → 0x00C0D450, 3 → 0x00C0D500, 4 → 0x00C0D2F0, else 0x00C0D240; with the arrival flag, 0x00C0D2F0 (`dev/LOAD_EVENTS.md`). Nothing writes 0x00C371B0 by its address (read only at 0x0076061B), so a move uses **0x00C0D240** (inferred: the global stays 0): (∓1.5, −1), (0, −2.5), (∓1.5, −4), (∓3, −2), (∓1, −3), (∓3.5, −1) m (right, forward), turned to the leader's movement. With +0x450 = 0, left and right swap (slot 0↔1; from slot 3, odd↔even). The others are scaled variants, e.g. 0x00C0D450: (∓1.25, −0.75), (0, −1.75), (∓1.75, −2.5), (∓3, −1.5), (∓1, −2.5), (∓3.25, −1.5).
- **The primary click 0x00712830(target, action, arg)**, a switch on action − 3 (table 0x00712DC4) over 0x006360C0(mask 0x80):
  - **4, attack: every listed member gets `Orders::Attack(member, the same target)`** 0x007F6410 (0x00712BB3; front of the queue, player issued, ON_ORDER_RECEIVED to each, `src/dao/Orders.cpp`), and the target id goes into member+0xC60's word +4 (what the follow's "has an order" test reads, below).
  - 3, talk: the first listed only, 0x007F64C0 (inferred: a conversation command through 0x0069EE80), plus OrderReceived.
  - 8, use: the first listed only, `Orders::UseObject` 0x007F6550; a dead creature becomes its body bag (+0x790).
  - 5 select, 6 deselect (above), 7 nothing.
  - Callers: `PerformPrimaryActionOnTargetUnderCursor` 0x007132B0, `PerformSecondaryActionOnTargetUnderCursor` 0x00713300, `PerformDefaultActionOnTarget` 0x00713350, `PerformSecondaryActionOnTarget` 0x00713390; the action from 0x0070CAD0(object).
- **Abilities go to one caster:** `PerformAbilityMessage` 0x00711B50 takes the caster from msg+8 (ability +4, item +0xC); 0x007116C0, 0x00711A00 and 0x00711940 take the targeting state's caster (0x0070ABE0: +0x4C or +0x54). All end in `GameModeExplore::UseAbility` 0x00711680 → `Orders::UseAbility` 0x007F6690.
  - **The ability order** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
    - **0x00711680** calls 0x007F6690, then 0x007113F0 (not read).
    - **0x007F6690(ability, creature, item, target, point):**
      - It does nothing when the creature's slot 0x1D0 answers 1.
      - It calls `Orders::OrderReceived` 0x007F6370.
      - An ability in the creature's active modal list (the stats' +0xF8; the stats are +0x7C4, through 0x0053CC40) is switched off: 0x006B9970 at 0x007F6726, then return (corrected 2026-10-23 by lib-agent-2: it said the list was +0x7C4, and the switch-off [I]) [C, 0x007F6700–0x007F6726].
        - **0x006B9970(ability)** [C]:
          - It builds event 0x23 (COMMAND_PENDING, 35) with int 0 = 25 (the UseAbility command) and int 1 = the ability. Creator and objects 0 and 1 are the creature.
          - It runs it at once through 0x00698960 [I: that this runs the event script; only its prologue was read].
          - If the ability's `BlendTree` (0x70F463FA) is not empty, it then goes to 0x0073C600.
          - So switching a mode off is the scripts' COMMAND_PENDING path (ability_core.nss:194–204), run straight away and not queued.
      - An item the creature may not use (`CCCreature::ForScript_CanUseItem`) does nothing.
      - When the ability's abi_base `ProcessWithEvent` (0x57119DC9) is non-zero, an event of that type is delivered at once instead (AIMaster::DeliverNow), with int 0 the ability and object 0 the item. Only TRAP_MAKING, HERBALISM and POISON_MAKING have it, all 101 (EVENT_TYPE_USE_ABILITY_IMMEDIATE).
      - **Otherwise** it calls `QueuePlayerCommand(25, params, 0)`, which calls `QueueNewCommand(25, params, 0, 0, 1, −1, 1)`, so the command goes at the back of the queue. The params are int 0 the ability, object 0 the target, object 1 the item, vector 0 the point.
        - **The order clears what the creature is doing** (corrected 2026-10-24 by lib-agent-2, checked by lib-agent) [C]. `QueueNewCommand` 0x0073D000 stores its fifth argument (1) as the command's +0x10 (player-issued) and passes −1 to `QueueCommand` 0x0073CD10 as the override. With −1, QueueCommand takes the add behaviour from `commands.gda` (table 94): `AddBehaviorSelf` when the current command has the same type (0x0073C600, 0 for none), `AddBehavior` otherwise. Row 25, Use Ability, has AddBehavior 2 and AddBehaviorSelf 1. What 1 (a soft clear) and 2 (a hard clear) do is in `dev/ANIMATION.md`, "How a newly queued command affects a running one".
        - **So** an ability order hard-clears whatever the creature is doing (ClearAllCommands(1, 0); a non-static current command ends −2), a Wait the AI queued among them. If the creature is already using an ability, the order soft-clears the queue instead and waits for that ability to finish.
    - **The party AI:** `AI_DetermineCombatRound` (ai_main_h_2.nss:377–386) returns at once while a command is current or queued, so the queue, not the player-issued flag, holds the AI off.
    - **What drops a follower's order:** damage interrupts (rules_core.nss:385 for a ranged attacker hit in melee, :445 while conjuring) clear anyone's commands. :941 (a hostile appears out of combat) clears only non-followers.
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
  - **Event 0x5E (94) is `EVENT_TYPE_PLAYER_COMMAND_ADDED`** (read 2026-10-23 by lib-agent-2 with a subagent) [C]:
    - **The raiser:** `CCObject::QueueCommand` (0x0073CD10, 0x0073CD57..0x0073CDD1) raises it when the command's `+0x10` (player-issued) is set. `QueuePlayerCommand` 0x0073D060 sets it, so every order raises it.
    - **Contents and timing:** it goes to the ordered creature, with int 0 = the command type (`cmd+2`), before the add behaviour is applied.
    - **The handler:** only `player_core.nss`:879–883 handles it. It plays `SSPlaySituationalSound(ORDER_RECEIVED, …)`, not when stealthy, with the commands table's `SoundsetProbability`, which is 0 for DRIVE, MOVE_TO_LOCATION and MOVE_TO_OBJECT in explore.
    - **Not the same as `ON_ORDER_RECEIVED` (90),** which no script handles.

### Corrections

- `src/dao/Party.cpp`'s head: 0x006360C0 is not "its own list"; it walks the selection (+0x194), less members with the masked flags. Corrected the same day.
- `src/dao/Orders.cpp`: no script listens for event 90. Corrected the same day.
- `dev/COMBAT.md`'s "Range, line of sight": the id at party+0x1E8 is the leader, not the controlled creature. Corrected the same day.
- `dev/script_commands/Party.md`, `IsControlled`: TRUE for every selected member, not only `GetMainControlled`. Corrected the same day.
- Nothing in the game sends `SelectCharacterMessage` (read 2026-10-24, DAOLib #253): its builder 0x00776250 is the factory that makes it from text (stored at 0x007808EF in 0x007808A0, the table of names), no binding text names it and no movie of the core archives does. Its fields and the party's other messages are in `dao/Messages.h`.

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
- **The item refresh 0x007ACDF0, its other branches** (beside master's found and non-Use-Item ones) [C]:
  - Command 7 with CommandItem −1: when the flag is on, it looks the name and icon up from the resref (0x007532E0; the name through a string-ref record, 0x004D1EB0); otherwise it does nothing.
  - CommandItem set but the item gone: CommandItem becomes −1, and **the tag and resref stay**.
- **The constructor 0x007AD310** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
  - CommandItem (+0xC8) starts at −1.
  - Enabled (+0x58) is built false and set to 1 by the constructor's last write (0x007AD831).
  - Target, TargetID, Condition, ConditionID, Command and CommandParam are 0, and ItemName and ItemIcon empty.
  - So an untouched row answers GetTacticTargetObject and GetTacticConditionObject with 0. OBJECT_INVALID is only the out-of-range answer.
- **The reset 0x007ABEC0** (SetTacticPresetID, LoadPreset type 0) [C]:
  - It sets Enabled 1; Target, Condition, Command and CommandParam 0; CommandItem −1; ItemName and ItemIcon "".
  - It empties the resref (+0x144) and the tag (+0x128).
  - **TargetID (+0x78) and ConditionID (+0x98) are not written.**

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

## Hiding a party alert (read 2026-10-22 by lib-agent-2)

- **`HideAlertMessage`** (`CCParty` 0x0062F220) [C]:
  - **The argument** is one int at message +4 (`messages.csv`: an 8-byte message), the alert's `Type`.
  - **0x00628970** (`this` = party +0x330) calls 0x00628500, which walks the `Party.Alerts` list (+0x68) from index 0 and returns the first alert whose attribute at +0x58 (its `Type`) equals the argument, or −1.
  - **A found alert** is handed to 0x00628650 with the list at +0xA0, erased from +0x68 (0x004A62E0), and the list notified (its attribute slot 0x24). What +0xA0 keeps (the hidden alerts) is [I].
  - **None of that type:** nothing. One alert per message.
- **The items alert** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2; the caller is the party possessor's OnItemAdded 0x006313A0, ITEMS.md):
  - **0x00629620** (`this` = party +0x2C0 + 0x70, the alerts) [C]:
    - It makes nothing in game mode 9 (`GameModeController::GetGameMode` 0x005D5E70; −1 with none), or when an alert of type 3 is already in the list (0x00628500(3)).
    - Otherwise it makes one (0x006294C0), sets its Type (+0x58) to 3 (ALERT_ITEMS), appends it to the list (+0x68) and notifies.
  - **So** any number of items added together gives one "Items received" alert (strref 336172, `LOC_ITEMS_RECEIVED` in notifications.gfx's Alert.as; the string is not in the exe) [C].
  - `CreateItemOnObject` with bSuppressNotification sets flag 4 (ITEMS.md), which skips the alert. Flag 2 skips it too.
  - Whether a merge onto a stack reaches OnItemAdded, and which flags stores and crafting set, are [I].
    - Whether the mode-9 skip holds while the add-ins' PRCSCR list scripts run (LOAD_EVENTS.md, "The add-ins' list scripts (PRCSCR)") is open: step 1 calls SetGameMode with (0, 1, 0).
- **The caller is the notifications movie, not an engine timer** [C, `Alert.as`]. It sends `Party.HideAlert <this.Type>` in two places:
  - in `Hide()` (:205), when its fade-out animation ends. The fade starts after the movie's own delay (`TriggerFadeoutDelay`, a `setInterval`), which stops while the mouse is over a non-sticky alert;
  - on a click (`OnButtonAction`, :236–253), after `GUI.GoToQuest`, `GoToCodex`, `GoToItem`, the tactics screen or the ability screen.
  - So of two alerts of one type, the first in the list goes, whichever of them faded or was clicked [I for the effect].
- **`HideUberAlertMessage`** (0x0062F240, no argument) calls 0x00629E50 on the same object; not read.
