# Script commands, group "objects": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (subagents, b2b) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm` and a linear sweep of `.text`, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. Pops and pushes as `dev/script_commands/Party.md` says (-639 a failed pop, -638 a failed push).

Shared facts:

Objects and their flags:
- `CClientExoApp::GetGameObjectById` (0x005D5840) looks an id up in the game object array (app+0xCC, `CGameObjectArray::GetGameObject` 0x00A58190) and returns any game object: areas, the module, map objects included. `GetObjectById` (0x005D5BE0) returns only what answers the `CCObject` cast (vtable +0x18, cast slot 6), so never an area, the module, the party or a map object. (checked)
- A game object's type byte is +0x14 (`dao/core/ObjectType.h`: 3 module, 4 area, 17 map patch, 19 map). Cast slots (`dev/object_casts.csv`): +0x08 CCArea, +0x18 CCObject, +0x1C CCModule, +0x48 MapObject (type 19), +0x4C MapLocation (type 17). (checked)
- CCObject flags met here: +0x2BC destroyable (`IsDestroyable`/`SetDestroyable` read and write it), +0x2BD destroy pending, +0x2C4 plot (`IsPlot`), +0x254 the area's id. CCCreature +0x7D4 the hero/PC flag (`dev/PARTY.md`, `dev/COMBAT.md`). (checked)
- The module keeps one **tag index**, a vector at module+0x198 of 0x88-byte entries: the tag as up to 64 UTF-16 code units inline at +0, the object id at +0x84, kept sorted by an ordinal, case-sensitive comparison of the tag. Lower bound 0x005FF370, insert (tag, id) 0x00602650, remove (tag, id) 0x00600270. (checked)
- Insert has 18 callers (0x00602650), among them `SetTag` (0x0066950B), the area adding its own tag (area+0x180, 0x0068B3D4), and the creature, placeable, item, trigger, waypoint, projectile and other loaders (0x006C20B6, 0x007529AD, 0x007C28F8, …). Remove has 14, among them `SetTag` (0x006694D6) and the creature's destructor (0x006BB236). So the index holds every tagged object of every area the module has loaded, and those areas themselves. (checked for the callers' existence; which class each caller belongs to is inferred from its address range)

The flag bytes and the per-class slots:
- `CClientExoApp::GetObjectById` (0x005D5BE0) ends with the object's cast slot +0x18 (slot 6, AsCCObject, `dev/object_casts.csv`), so it answers null for an area, the module and the party: every handler below that uses it treats those as invalid. (checked)
- CCObject flag bytes met here (checked): +0x2BC destroyable (the constructor 0x00699670 sets 1 at 0x00699AFD), +0x2BD destroy pending (set by `DestroyObject`'s slot +0x198, 0x00696A60), +0x2C0 immortal (a dword), +0x2C4 plot, +0x2C5 active (`dev/PARTY.md`), +0x2C6 always visible. The GFF3 template reader 0x0069AAD0 reads the bytes `IsDestroyable`, `Plot` and `Immortal` into +0x2BC/+0x2C4/+0x2C0 (0x0069AE33–0x0069AE9C); the save keeps plot and immortal as GFF4 fields 16250 `SavegameObjectPlot` and 16220 `SavegameObjectImmortal` (0x00697EE0 writes, 0x00698ED0 reads). (checked)
- Main vtable slots +0x164 SetPlot, +0x168 IsImmortal, +0x16C SetImmortal: every object class shares `CCObject`'s (0x00699FD0 `[+0x2C4] = byte arg`; 0x00560070 `return [+0x2C0]`; 0x00699FE0 `[+0x2C0] = arg`) except `CCCreature` (0x006AD1A0, 0x006A3750, 0x006AD130, below). (checked from the vtables)
- CCCreature +0x7D4, which several handlers here test (SetTag's "player", the destroy event), is the template's `CharType` byte, 1 for the hero: `IsHero` (500) in the players group's file.

Projectiles:
- A projectile is a `CProjectile` (vftable 0x00B23BB4, 0x510 bytes, constructor 0x007E8410), allocated by 0x00679670 and registered by 0x00679EA0, both `__thiscall` on `[g_clientExoApp+0x104]`. That object has the layout of `CProjectileManager` (vftable 0x00B0D928, constructor 0x00679A90; lists at +0x1C and +0x2C, which 0x00679EA0 fills with the projectile's id). (checked) It adds the projectile to this manager, not to the area as `dev/ABILITIES.md` had it (corrected there the same day).
- The constructor's defaults (checked, 0x007E84F3–0x007E8580): +0x4A4 prj row 0, +0x4AC 0, +0x4B0 0, +0x4B4 2, +0x4B8 from-an-ability 0, +0x4B9 wide angle 0, +0x4BA 0, +0x4F4 custom event null, and **owner +0x4BC, target +0x4C0 and event target +0x4C4 all -1**.
- The fields the script handler writes (checked, 0x00661D9D–0x00661DE3): +0x4A4 nType (the `prj_base` row), +0x4D0 the source position, +0x4E0 the target point, +0x4C0 the target object, +0x4AC nCrustEffectId, +0x4B9 bWideAngle (a byte, `!= 0`), +0x4C4 oEventTarget. After registering, it sets +0x448 = 2 (inferred: a state, "launched"). The owner +0x4BC is never set, so it stays -1.
- **The crust's field is not settled.** The script handler stores the crust at +0x4AC. The ability path (`0x006B70C5`–`0x006B7111`) writes both +0x4AC (a value from 0x00702E90) and +0x4B4, and `dev/ABILITIES.md` names +0x4B4 the crust. One of the two names is wrong (`dev/ABILITIES.md` says so, corrected the same day). (checked: the writes; which field is the crust: not read)
- **Impact** is `0x007E89F0` (`__thiscall (hit list)`, `ret 4`; checked):
  - The event target is +0x4C4, or the owner +0x4BC when +0x4C4 is -1. It is looked up with `GetObjectById` 0x005D5BE0. **If there is no such object, nothing is sent.**
  - A custom event (+0x4F4) whose type (+0x20) is non-zero is sent instead of the default. Its creator (+0x24) is set to the projectile's id. It is queued to the event target with `AIMaster::AddEventDeltaTime(0, 0, target, target, 10, event)` (0x0064ACE0), and +0x4F4 is cleared (ownership passes with the queue).
  - Otherwise a projectile from an ability (+0x4B8) goes to 0x007E71C0, the ability's impact (not read), and any other to the default impact, 0x007E87C0.
- **The default impact event** (0x007E87C0, checked) is a new `CScriptEvent` (0x0069BAC0):
  - Type 29, `EVENT_TYPE_ATTACK_IMPACT`; creator (+0x24) the projectile.
  - Object 0 is the event target (+0x4C4). Objects 1..n are the objects hit (the hit list), or, with no list, the object at +4 of the record from `0x007E68A0` (+0x488, an attack result: inferred).
  - Int 0 is that record's +0 (the combat result, as `placeable_h`'s `Placeable_HandleImpact` reads it), int 1 its +8 (log_h: "DamageEffect"), int 2 the projectile's +0x4B0. **For a script projectile +0x4B0 is 0, not nType**, although `placeable_h` reads int 2 as the projectile type.
  - Vector/location 0 is the projectile's position (+0x120). Location 1 comes from 0x007E6D80 (`placeable_h`: "orientation").
  - It is queued to the event target as above. Unless int 0 is 0, 3 or 5 (COMBAT_RESULT_INVALID, MISS, BLOCKED), it is then also queued to each id in the list at +0x4F8, which is then emptied (0x00634F20).

### IsObjectValid (206)
- **Handler:** 0x006593C0 (not shared). It calls `CClientExoApp::GetGameObjectById` (0x005D5840) and the object's cast slots. (checked)
- **Does:** pops the object and pushes TRUE when the id names a live game object that is a `CCObject` (creature, item, placeable, trigger, waypoint, sound, AoE, projectile, VFX, store), or is of type 4 (area, cast +0x08), 3 (module, +0x1C), 19 (map, +0x48) or 17 (map patch, +0x4C); else FALSE. (checked)
- **State:** only the object array (app+0xCC); nothing on the object is read beyond its type byte +0x14. (checked)
- **Edge cases:** OBJECT_INVALID and a destroyed object's stale id → FALSE (the array lookup fails). The **party object** (type 16) and GUI/tile types → FALSE: no case for them (inferred that the party is not in the array as a CCObject; the cast table says CCParty does not answer slot 6). Inactive objects, objects with a destroy pending (+0x2BD) and dead creatures → TRUE until the destroy is delivered. A failed pop → -639; a failed push → -638. (checked)

### GetResRef (208)
- **Handler:** 0x006594E0 (not shared). It calls `GetGameObjectById` and `std::wstring::operator=` (0x004C2AB0). (checked)
- **Does:** for a `CCObject`, pushes the wide string at object+0x1CC (the template's resref, inferred). For an area or the module, copies the 0x41-byte resource name at +0x80 (`area+0x18+0x68`) and pushes its narrow text at +0x21 of it (0x00A29F30, widened by 0x004C4140). (checked)
- **State:** CCObject +0x1CC; CCArea/CCModule +0x80 (+0x21 the text). (checked)
- **Edge cases:** an invalid id, a map, a map patch or any other non-CCObject → "" (the empty string it started with). The text has no extension: `ntb_functions_h` compares a trigger's to "ntb200tr_wolf" (inferred from the script). A failed pop → -639, a failed push → -638. (checked)

### GetNameStrref (838)
- **Handler:** 0x00669370 (not shared). `GetObjectById`, then 0x00403C70 (`lea eax,[ecx+0xC]`). (checked)
- **Does:** pops the object and pushes the dword at object+0xE4: the strref of the name held at +0xD4 (`[+0xD4 → +0xC] + 4`). (checked; that +0xD4 is the `LocName` CExoLocString is inferred)
- **State:** object+0xE4, read only. (checked)
- **Edge cases:** an invalid id, an area or the module pushes **0**, not -1. A name with no strref pushes whatever +0xE4 holds (-1 for none is inferred). Pop failure -639, push failure -638. (checked)

### SetTag (210)
- **Handler:** 0x006693E0 (not shared). `CClientExoApp::GetModule` (0x005D4FC0), `GetGameObjectById` (0x005D5840), the module's tag index remove 0x00600270 and add 0x00602650, the object's set-tag 0x00696590. (checked)
- **Does:** pops the object, then the tag. For a CCObject with a module present it takes the object's id (attribute +4), removes (old tag, id) from the module's index, sets the tag, and adds (tag, id) again. (checked; that 0x00600270/0x00602650 are the module's tag index, which `GetObjectByTag` reads, is inferred)
- **State:** the tag is the wstring at object+0xB8 (`dev/PARTY.md`), assigned as given (0x004C2AB0): no case folding or trimming. (checked)
- **Edge cases:** an **empty tag does nothing** (the size is tested first, 0x0066949A): a tag cannot be cleared. An invalid object, area or module does nothing. The tag **"player"** (compared at 0x006965A9 with the wide string 0x00AECCB0) is only stored on a creature whose +0x7D4 (the hero flag) is 1; for any other object it is silently refused and the old tag re-indexed. Returns 0. (checked)

### GetObjectByTag (211)
- **Handler:** 0x006597E0 (not shared). It calls `CClientExoApp::GetModule` (0x005D4FC0) and `CCModule::ForScript_GetObjectByTag` (0x005FF450). (checked)
- **Does:** pops sTag then nNth. Cuts the tag to its first 64 characters (0x004C30F0), finds the lower bound of the tag in the module's tag index (0x005FF370), adds nNth, and if that position is inside the index and the entry's tag equals sTag exactly (0x004C0E10, case-sensitive), pushes the entry's id; else OBJECT_INVALID. (checked)
- **Which objects:** one module-wide index, not the current area's list: objects in every area the module holds loaded, the areas themselves (their own tag), and objects outside the current area (the party pool, inventories' items, inferred from the item loader's insert). **No active test and no type filter:** inactive objects are found, as for `GetNearestObjectByTag` (`dev/PARTY.md`). The id is pushed as stored, with no check that the object still exists. (checked; the areas' scope is inferred from the loaders and the area destructor's remove)
- **Order of nNth:** equal tags sit together; insert puts a new entry at the lower bound, i.e. **before** the existing ones with the same tag, so nNth 0 is the most recently registered (loaded, created or re-tagged) object (checked for the insert position; "most recent" is inferred from it).
- **Edge cases:** no module → OBJECT_INVALID. A negative nNth wraps the unsigned bound check, or lands on an entry with a smaller tag; either way OBJECT_INVALID. A tag longer than 64 characters matches the objects whose tag starts with those 64 (the index holds 64 at most, inferred). sTag "" finds an object with an empty tag if such objects are indexed (not read). A failed pop of either argument → -639; a failed push → -638. (checked) 0x005FF580 is a typed variant of the same search (by object type), used by 0x005FF89F, not by this command. (checked)

### DestroyObject (214)
- **Handler:** 0x006599E0 (not shared). It calls `CClientExoApp::GetObjectById` (0x005D5BE0) and the object's vtable +0x198 (slot 102): `CCObject` 0x00696A60 for every class but `CCItem` (0x00752C90). (checked)
- **Does:** pops the object, then nDelay. 0x00696A60: if +0x2BD (destroy pending) is clear, sets it and queues event 0xB (NWN's EVENT_DESTROY_OBJECT) through `AIMaster::AddEventDeltaTime(0 days, nDelay ms, self, self, 0xB, null)` (0x0064ACE0); if already set, does nothing. Delivery is to the object's handler, vtable +0xC8, on a later AI update, never inside the calling script (`dev/LOAD_EVENTS.md`, "Delivery"). (checked)
- **On delivery** (`CCObject` 0x00698A50, case 0xB): only if **+0x2BC (destroyable) is set**: slot +0xFC (leave the area: 0x00697060 sets +0x254 to -1, takes the object out of the area's grid 0x00687A50 and list 0x0068B050), then slot 0, the deleting destructor. Not destroyable → nothing at all. (checked)
- **Creature** (0x006A25C0): the event is **dropped when +0x7D4 (the hero) is 1**; otherwise the same. Its +0xFC (0x006B6E60) also clears all commands (`ForScript_ClearAllCommands(1, 0)`) and removes each of its AoE objects (list +0xB9C). Its destructor (0x006BAFE0) takes it out of the active party and the pool (0x006377E0 and 0x0063B8B0 on the party, forced), destroys its body bag (+0x790, if the placeable's +0x600 IsBodyBag is set), and removes its tag from the module index (0x00600270). (checked; the party calls' meaning is `dev/PARTY.md`'s)
- **Item:** with nDelay 0, 0x00752C90 first takes it out of its holder at once (`ScriptBackend::ForScript_RemoveItem(item, -1)` 0x00762100), even if not destroyable; with a delay, the handler (0x00753B10, case 0xB, destroyable only) takes it from its possessor (+0x448, 0x00763040) at delivery. (checked)
- **Edge cases:** an invalid object, an area or the module → nothing (`GetObjectById` refuses them). **Plot (+0x2C4) and immortal are not consulted**; party members other than the hero are destroyed; `core_h`'s `Safe_Destroy_Object` is what refuses party members, plot and immortal objects (script side). A second call while one is pending is ignored, so the first delay wins. **A non-destroyable object, or the hero, keeps +0x2BD set after the event is dropped, so every later DestroyObject on it does nothing even after `SetDestroyable(TRUE)`** (checked: nothing clears +0x2BD on those paths; `SetDestroyable` writes only +0x2BC). nDelay is milliseconds on the AI world clock; a negative delay goes to `AddEventLater` with a due time in the past (inferred: delivered on the next update). Returns 0; a failed pop → -639. (checked unless marked)

### SetObjectAlwaysVisible (187)
- **Handler:** 0x00661C00 (not shared). (checked)
- **Does:** pops the object, then the flag, and writes `bAlwaysVisible != 0` to the byte +0x2C6. (checked)
- **State:** +0x2C6. A sweep of the whole `.text` for `[reg+0x2C6]` finds only this write, the constructor's clear (0x00699BAB) and an object copy (0x00751A9D, in 0x007515D0): **nothing reads it**, so the culling override script.ldf describes is not in this build, and it is not saved. (checked for direct accesses; an access through another base is not excluded, inferred none)
- **Edge cases:** an invalid object, area or module does nothing. No shipped script calls it. Returns 0. (checked)

### IsDestroyable (215)
- **Handler:** 0x00659A50 (not shared). (checked)
- **Does:** pushes the byte +0x2BC, 0 or 1. (checked)
- **Edge cases:** an invalid object, area or module gives 0 (FALSE). Default for a new object is TRUE (constructor), overwritten by the template's `IsDestroyable`. (checked)

### SetDestroyable (216)
- **Handler:** 0x00659AB0 (not shared). (checked)
- **Does:** writes `bDestroyable != 0` to +0x2BC. (checked)
- **What it governs:** `DestroyObject` (slot +0x198, 0x00696A60) does not test it; it sets +0x2BD and queues the engine event 0xB. The event handler tests it: `CCObject::EventHandler` (0x00698A50) on 0xB destroys (slot +0xFC, then the deleting destructor, slot 0) only when +0x2BC is set (0x00698B46); `CCItem`'s (0x00753B10) also tests it before taking the item from its possessor; `CCPlaceable`'s passes 0xB to the base; `CCCreature`'s (0x006A25C0) drops 0xB for the hero (+0x7D4 = 1) before the base sees it. (checked)
- **Edge cases:** an undestroyable object keeps +0x2BD = 1 after the ignored event, and slot +0x198 returns at once while +0x2BD is set, so **a later `DestroyObject` after `SetDestroyable(TRUE)` still does nothing**. (checked for the base class; that nothing else clears +0x2BD on a creature or placeable is inferred) The flag is read when the event arrives, not when `DestroyObject` is called. Invalid object: nothing. No shipped script calls it. (checked)

### IsPlot (217)
- **Handler:** 0x00659B10 (not shared). (checked)
- **Does:** pushes the byte +0x2C4. (checked)
- **Edge cases:** invalid, area or module gives 0. (checked)
- **Engine readers of +0x2C4** (checked as accesses; their purpose inferred): `Inventory` slot 12 (0x0073F600: plot, or the item's +0x464 == 0x11) and slots 1 and 2 (0x00740950, 0x00740300, adding items); `ItemFilterPlot` slot 1 (0x0080E100, the inventory's plot filter); `CCPlaceable` slot 64 (0x0074C720) answers action 4 (DESTROY, `dev/PLACEABLES.md`) as 0 for a plot placeable, so a plot placeable cannot be bashed. Death and damage do not read it in the engine: the scripts do (`dev/COMBAT.md`, "Damage and death").

### SetPlot (218)
- **Handler:** 0x00659B70 (not shared): calls main-vtable slot +0x164 with `bPlot != 0`. (checked)
- **Does:** every class but the creature writes the byte +0x2C4 (0x00699FD0). A creature (0x006AD1A0) writes +0x2C4 and, if its properties table (+0x7C4 → +0x190) has entry 7 (Health), calls that property's slot 14 with (2, bPlot), which stores the byte +0x74 of Health (0x0079C4D0). (checked)
- **State:** +0x2C4; for a creature Health's +0x74, the flag the regeneration step skips on (`dev/PROPERTIES.md`, "Regeneration"): so a creature made plot by script stops regenerating health natively (inferred from the two readings). A template's `Plot` is loaded straight into +0x2C4 without touching +0x74 (checked at 0x0069AE8C). (checked unless marked)
- **Edge cases:** invalid, area or module does nothing. Returns 0. (checked)

### IsImmortal (219)
- **Handler:** 0x00659BE0 (not shared): main-vtable slot +0x168. (checked)
- **Does:** non-creatures return the dword +0x2C0 **as stored** (0x00560070), so `Engine_SetImmortal(o, 5)` then `IsImmortal(o)` gives 5. A creature (0x006A3750) returns 1 when +0x2C0 is non-zero **or its current command is type 38 (Deathblow)** (`[+0x2DC]` type byte via 0x0073C600), else 0. (checked)
- **Edge cases:** invalid, area or module gives 0. (checked)
- Note: `dev/COMBAT.md`'s "call before Prepare" read this slot and slot +0x16C(0) as a "deathblow state" (corrected there the same day); +0x2C0 is the immortal flag (this command's, the template's `Immortal`, save field 16220), so the death effect makes an immortal creature mortal before it dies. (checked)

### Engine_SetImmortal (220)
- **Handler:** 0x00659C50 (not shared): calls 0x005C5E80 (`xor al,al; ret`, result unused, a stub), then main-vtable slot +0x16C with the int **as given** (not normalized). (checked)
- **Does:** non-creatures store the int at +0x2C0 (0x00699FE0). A creature (0x006AD130) stores it, then if it has property 7 (Health) calls Health's slot 12 with 1.0 (non-zero) or 0.0, which writes Health's +0x78, the minimum its current is clamped to (0x0079C490; `dev/PROPERTIES.md`, `SetCreatureProperty`); then sets the byte stats+0x1B0 to 1 (inferred: a dirty flag). (checked)
- **State:** so script.ldf's "unable to drop below 1 health" is the engine's: any set of an immortal creature's current Health is clamped to at least 1.0. (checked from both readings) Scripts are told to use `core_h`'s `SetImmortal`, which logs and calls this. (read in the scripts)
- **Edge cases:** invalid, area or module does nothing. A non-creature's flag has no engine effect found. Returns 0. (checked)

### GetArea (223)
- **Handler:** 0x00655000, shared with `GetAttackTarget` (522); the case is `cmp edi,0xDF` at 0x00655040. (checked)
- **Does:** pops the object and pushes the area id at object+0x254, OBJECT_INVALID (0xFFFFFFFF) when the object is invalid. (checked)
- **State:** +0x254, the object's area id (the field perception compares, `dev/COMBAT.md`). (checked)
- **Edge cases:** an area, the module or the party is not a CCObject here, so `GetArea(area)` gives OBJECT_INVALID, not the area itself. What +0x254 holds for an item in an inventory is not read (inferred: the id it had, or invalid). Push failure -638. (checked unless marked)

### GetMeleeRingPosition (606)
- **Handler:** 0x0066CAC0 (not shared). It calls `CClientExoApp::GetCreatureById` 0x005D58C0 and `MeleeRings::At` 0x007CCC30, then `StackPushVector` 0x00A2D6D0. (checked)
- **Does:** pops the creature, then nRingIndex. It pushes the centre of ring nRingIndex, the ring record's +0/+4/+8 (x, y, z). (checked)
- **State:** the rings are the vector at creature+0xB44 (begin +0xB48, end +0xB4C, 4-byte pointers). `RefreshMeleeRing` sets each centre on every `CCCreature::Update`, through 0x006A3060 (`src/dao/Addresses.h`, the melee rings' note), so the value is the last update's. That centre is the creature's position (+0x120), or a model node's position when the ring's +0x2C names a node. To it is added the facing (+0x130..+0x138) times the ring's offset (+0x24). (checked in 0x006A3060; node lookup inferred)
- **Edge cases:** a non-creature, an invalid object, a creature with no rings, or an index ≥ the ring count (compared unsigned, so a negative index too) pushes (0, 0, 0). The bounds check runs before `At`, so this command never makes a ring. Which rings exist: ring 0, and 1–9 where the appearance has `meleering1`–`9` rows (Addresses.h). (checked)
- It matches script.ldf ("the position of a melee ring"). The centre is not a place on the ring: `GetCreaturesInMeleeRing` (205) and `dao/Attack.h` cover the places. A failed push returns -638. (checked)

### PlayAdditiveAnimation (196)
- **Handler:** 0x00659300 (not shared). It calls `GetObjectById` 0x005D5BE0, 0x006956B0 (the model +0x154's current animation, `[model+0x13C]`, 0 with no model), `GdaTables::GetInt` via 0x005CA710, and 0x00696B50 (`__thiscall (anim, bool force, float)`, `ret 0xC`). (checked)
- **Does:** pops the object, then nAnimation, then bForceRestart. It plays the animation **at once on the model**: no command is queued, and nothing waits. (checked)
- **Steps:**
  - It looks up the object's current animation row in table 0 (Animations, `anim_*.gda`), column `State` (hash 0xE991BD51). If the row's State is 1, it returns doing nothing (62 rows in `anim_base`, e.g. 2, 3, 250, 254, 268–283). A missing row gives -1 and goes on. (checked)
  - 0x00696B50 plays only if the model's 0x00A5E8A0(anim) answers 1 or 2 (inferred: additive or override, as script.ldf requires).
  - Without bForceRestart, it also plays only if 0x00A5F810(anim, 0) is exactly 0.0 (inferred: not already playing).
  - It then calls the model's slot 0x58 with (anim, 1.0, 0.0, 1.0, 5.0, 0, 0); 5.0 is from 0x00AE7308. (checked; what the floats mean is inferred)
- **Edge cases:** an invalid object does nothing. Any object type is taken. **An object with no model (+0x154 null) faults:** 0x00696B50 passes the null model to 0x00A5E8A0, which reads `[ecx+0x18]`. (checked that there is no check; the fault is inferred) A non-additive animation id does nothing, silently. Returns 0. (checked)

### FireProjectile (193)
- **Handler:** 0x00661C70 (shared with FireHomingProjectile 370; it branches on the command id, 0xC1 for this one). It calls `StackPopVector` 0x00A2F460, 0x00679670/0x00679EA0 on the projectile manager, and `StackPushObject` 0x00A2D980. (checked)
- **Does:** pops nType, vSourcePosition, vTargetPosition, nCrustEffectId, bWideAngle and oEventTarget, in that order. It makes a `CProjectile` with those fields (SHARED) and target object -1, registers it, and pushes its id. (checked)
- **State:** the flight uses `prj_base` row nType (`dev/ABILITIES.md`, Projectiles). bWideAngle is +0x4B9, which the flight code tests at 0x007E7E75 (checked); the longer arc it gives is inferred from script.ldf.
- **Impact:** see SHARED. It sends EVENT_TYPE_ATTACK_IMPACT (29), or the custom event, to oEventTarget. **With oEventTarget = OBJECT_INVALID (the default), no event is sent**, since the owner is -1 for a script projectile. `pre100ip_catapult` passes OBJECT_SELF and reads location 0 for the impact point. (checked)
- **Edge cases:**
  - **nType 0 makes nothing and pushes OBJECT_INVALID.** No other nType is checked against `prj_base`. (checked)
  - oEventTarget is stored without a check. (checked)
  - The handler looks up oTarget's area (slot 0xA0), or the player's area (0x005D6B30), and **discards the result**. (checked)
  - Which area the projectile is in: not read.
  - A failed pop returns -639. (checked)

### FireHomingProjectile (370)
- **Handler:** 0x00661C70 (shared with FireProjectile 193; the case for 0x172 pops an object at 0x00661D02). (checked)
- **Does:** pops nType, vSourcePosition, oTarget, nCrustEffectId and oEventTarget. There is no bWideAngle, so +0x4B9 stays 0. It makes the projectile with the target point (0, 0, 0) and the target object oTarget at +0x4C0, and pushes its id. (checked)
- **State:** the flight reads +0x4C0 (0x007E714A `cmp [esi+0x4C0], -1`; 0x007E7194 looks it up). +0x4C0 is reset to -1 at 0x007E79C1, inferred: when the target is lost, after which the projectile flies on to its point. (field use checked; the tracking is inferred)
- **Edge cases:**
  - oTarget is not validated. An invalid target gives a projectile with no target and an aim of (0, 0, 0) (inferred).
  - nType 0 pushes OBJECT_INVALID, as for 193.
  - The impact event is as for 193: nothing without oEventTarget. (checked)

### SetProjectileImpactEvent (371)
- **Handler:** 0x00661E50 (not shared). It calls `StackPopEngineStructure(0)` 0x00A2F650, `CClientExoApp::GetProjectileById` 0x005D5AF0 and `CScriptEvent::CScriptEvent_copy` 0x0069CB10. (checked)
- **Does:** pops oProjectile, then eCustom. If the id is a projectile and the event is non-null, it stores a new copy of the event (0x50 bytes) at the projectile's +0x4F4, then frees the popped event. (checked)
- **On impact:** if the event's type (+0x20) is non-zero, it replaces the default ATTACK_IMPACT event. Its creator is set to the projectile, and it goes to the event target alone: not to the +0x4F8 list, and with no impact position or hit list added. A type-0 event leaves the default. (checked, 0x007E8A26–0x007E8A72)
- **Edge cases:**
  - The id must be a `CProjectile` (object kind byte +0x14 checked by `GetProjectileById`); any other id or OBJECT_INVALID frees the event and does nothing. (checked)
  - **A second call overwrites +0x4F4 without freeing the first copy**, which leaks it. (checked: there is no release)
  - Without an event target the custom event is never sent (SHARED). (checked)
  - Returns 0. (checked)

## Not read
- Whether a destroyed creature's or placeable's inventory items are destroyed with it or orphaned: the destructor 0x006BAFE0 was not followed through all its calls (0x006BB2A7 onwards).
- Whether objects with an empty tag are put in the module's tag index (the loaders' insert calls were not each read).
- Whether the world clock under `AddEventDeltaTime` stops while the game is paused (affects nDelay).
- `CCPlaceable`'s slot +0xFC (0x00750A60) and the other classes' destructors.
- What the module's tag index functions 0x00600270/0x00602650 do in detail (remove and add, inferred).
- Who else, if anyone, clears +0x2BD (destroy pending) on creatures and placeables.
- The `Inventory` slots 1, 2, 12 in full: what a plot item changes when added or offered (sale, drop) is inferred.
- +0x254 for items held in an inventory.
- 0x007E71C0, an ability projectile's impact; what fills the hit list and the +0x4F8 list; how 0x007E6D80 builds location 1.
- The flight itself: how bWideAngle shapes the arc, homing's steering, `ImpactTimeout`.
- The area a script projectile is placed in.
- What the model's slot 0x58 parameters (1.0, 0.0, 1.0, 5.0) are, and what the Animations table's `State` 1 means.
