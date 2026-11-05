# The area creature pool, read for DAOParagonEngine #417

Read 2026-11-05 in the GOG exe (Ghidra decompiler) by a paragon-agent helper (CreatePool's mode and size gates and SetActive's release checked in the disassembly by lib-agent: 0x00687453, 0x00687468, 0x006B5E8B..0x006B5EAB); checked against the Redcliffe siege (the ladder's Slot_13) in Eluvian: the second wave takes pool members 28, 27, 26 and on, each with `Play(1, 1.0, -1.0, 1)` twice at once. Paragon's side: `crates/script/src/world/pool.rs`.

## What a creature holds

- `CCCreature` +0xB60 is a `CreaturePoolRef`: one pointer to a `CreaturePool` (0x20 bytes: the pool's name as a wide string, the available byte at +0x1C). `CreaturePoolRef::Ensure` 0x0069FCC0 makes it when it is null (constructor 0x0069F870: **available = 1**); `SetAvailable` 0x0069E300 writes +0x1C (nothing when the reference is null); `GetAvailable` 0x0069E310 reads it (0 when null).
- A save keeps the name (field 0x405E) and the byte (0x405F). `CCCreature::LoadFromStruct` 0x006BF5E0.. calls `Ensure(name)` when 0x405E is set and non-empty, then `SetAvailable` only when 0x405F is in the file. The name is the resource as `CreatePool` was given it, with its extension (`arl101cr_undead.utc`).
- `GetResRef` (0x006594E0) reads +0x1CC, which `CCCreature::LoadTemplate` 0x006A27B0 fills with `ResRef::ToCString(resref)`. `CreatePoolCreatures` makes the resref with `CResRef::FromWString` of the pool's whole key, so a pooled creature's resref **keeps the extension** (and so does the template a save writes for it).

## The area's side (`CCArea` +0x1D0, a map from the key to a vector of ids)

- `CCArea::CreatePool` 0x00687410: only in game mode 9 (loading), size 1 to 50 (`size - 1 < 0x32`), key not in the map yet; the key is `ResRef::ToString(name)`; the vector has `size` slots, each -1.
- `CCArea::CreatePoolCreatures` 0x00687570 runs at the area load's **step 5** (`GameModeLoading::AreaLoadSteps`, case 5, call at 0x006C9186) after `World_Update` and `AIMaster::Update`, for the controlled creature's area (`CClientExoApp::GetControlledCreatureArea`). For each pool and each slot that is still -1: `CreateObjectFromTemplate(area, 6, resref, (65505, 65505, 0))`, the id into the slot, `Ensure(key)`, tag `"pool_" + Left(key, len - 4) + index`, `SetActive(false)` (vtable 0x140), which makes it available (below). Slots a save filled are not made again.
- `CCArea::AddPooledObject` 0x00687360 (from `SetLocation`): with a non-empty name, the id is appended to the key's vector (the vector made empty when the key has none) unless it is in it (`ECDynArray::FindOrAppend` 0x0076B540 = `VectorIndexOf`, else `push_back`). It never fills a -1 slot, so a save's pool is the members in the order the area loaded them.
- `CCArea::CreateObject` 0x00687830: the key is `ResRef::ToString(templateRef)`; no vector: `CreateObjectFromTemplate`. A vector: the first id (in vector order) whose creature exists and `GetAvailable`: `SetAvailable(false)`, `ResetPooledCreature` 0x006AB900, `CCObject::SetPosition(location, 0)` (0x00695540: position stored as given, then the virtual `OnPositionChanged` 0x006B5EF0, which snaps only through a physics controller, and a parked creature has none). Nothing available: the loop ends and the function returns null [C, read 2026-11-05 by paragon-agent-2's reader, checked by lib-agent]: EBP is zeroed at 0x0068787E and set only to the created object (0x00687940) or the pooled creature (0x0068792A), and the exit moves it to EAX (0x00687953). The script command takes a null (0x0066970E) to its push of OBJECT_INVALID (edi = -1 from 0x00669691, `StackPushObject` at 0x006697A2).
- The script handler `CREATEOBJECT` 0x00669550 uses the **controlled creature's area**, not the location's; it then does `SetOrientation`, `SetActive(bSpawnActive)`, the override script, and for a creature with a pool reference `ResetPooledCreature` **again** and, when the 6th argument (bNoPermDeath) is non-zero, stores it at +0xBD8.

## ResetPooledCreature 0x006AB900

Only for a creature with a pool reference whose available byte is 0: health (property 7) and property 8 `SetCurrent(p, GetProperty(p)->vfunc 0xC (the total), 1)` (so `OnHealthChanged` revives a corpse); `CCArea::RemoveObjectFromTeam(creature, team +0x3C4)`, +0x3C4 = +0x3C8 (the original team), `AddObjectToTeam`; then vtable +0xB0 `Play(1, 1.0, -1.0, 1)` (row 1, Pause, at once). `SetTeamId` 0x0065BC30 writes only +0x3C4. Eluvian shows the Play twice (at the parking place, then at the new place).

## What frees a member

- `CCCreature::SetActive(false)` 0x006B5D90: for a party member only clears +0x421; otherwise after `CCObject::SetActive`, when +0xB60 is set and the appearance's +0x5D byte (Decapitate, 0x006AAC20) is 0, `SetAvailable(true)`. So **a decapitated member is never freed again**.
- `CCCreature::UpdateCorpseDecay` 0x006BC670: at the end of the corpse time (`g_timeBeforeDecay`, 60 s of death), `SetFade(0)`, `DecayCorpse(true)` (0x006B98E0: does nothing for a creature with +0xB60) and, for a creature with +0xB60, vtable 0x140 `SetActive(false)`. `CanCorpseDecay` 0x006A3AD0: +0xBD8 != 0 stops a decay only for a creature **without** a pool reference; with the forced-delay byte +0xC8C it is true at once; a corpse count above `CorpseLimit` cuts the delay (`SetDecayDelay(0)`).
- The snap when a member is made active again: `dev/PATHFINDING.md`, "The snap's search" (a created creature's controller snaps with the 10 m search and no overlap test).

## Left open

- The order the area walks its pools in `CreatePoolCreatures` (a hash map).
- Whether a snap at activation moves the siege's spawn waypoints in vanilla: it does not (Eluvian: exactly (339.62, 393.02) and (337.44, 390.67), z 45.72 and 45.65).
