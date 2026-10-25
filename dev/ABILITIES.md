# Abilities as the engine carries them out

Read 2026-10-18 by lib-agent-2 (two subagents in parallel) in the GOG exe, the toolset's scripts and the game's tables, for DAOParagonEngine's next batch (blows that land, the mage's Harrowing), at paragon-agent-2's request. How `COMMAND_TYPE_USE_ABILITY` (25, `CommandUseAbility` 582) is queued and stepped, the events the scripts get, and where timing, cost, cooldown and projectiles come from. Builds on `dev/TURN_BASED.md` (the sub-actions, conjure 3.87 s), `dev/COMBAT.md` (COMMAND_PENDING, COMMAND_COMPLETE) and `src/dao/Conjure.cpp`. Each point says what was read and what is inferred. Not yet seen in play as a whole.

## The use-ability command and its events

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe and the toolset's scripts (`ability_core.nss`, `ability_h.nss`), for DAOParagonEngine's S5/S6. Builds on `dev/TURN_BASED.md` (the sub-action types seen in play: 15, 39, 50, 7, 8, 37; the conjure 3,866 ms and the cast 1,707 ms for Chain Lightning; the approach by `Range`) and `dev/COMBAT.md` (COMMAND_PENDING, COMMAND_COMPLETE). "Read" is what the code shows; "inferred" is marked. Column names are matched by the GDA hash (CRC-32 of the lowercase UTF-16 name); table 1 is `abi_*`, 134 `ranges`, 135 `spellconjuringspeeds`.

### The sub-actions and their handlers

`CSSubActionHandler`'s table (filled by `0x007AB8D0`, a handler per type at +0x48 + 4 × type) gives: 4 `0x007AA560` (approach the melee ring), 6 `0x007A1ED0`? (aim, see below), 7 `0x007A66A0` (conjure), 8 `0x007A7440` (use), 14 `0x007A99B0` (approach a point), 15 `0x007AA200` (approach to a range), 37 `0x007A4390` (play an animation), 39 `0x007A69E0` (turn to an object), 40 `0x007A6AF0` (turn to a point), 50 `0x007A1E40` (the command's own step, dispatched again by the command type through a second table at +0x4C; type 25's is `0x007A83F0`), 51 `0x007AAC10` (death). Read (the table's stores); the aim handler's address is not confirmed.

### Queuing them: the UseAbility step (`0x00743F50`)

- Read: it stops at once (queuing nothing, so the command ends) if the user is dead, if `abi`'s `AbilityType`, `TargetType` or `Range` cannot be read, if an item ability (`AbilityType` 4) has no item (object 1, or found by the string 0's tag through `0x006A6400`), if `CCCreatureStats::CanUseAbility` refuses, or if the target is not valid for the ability's `TargetType` (`0x00742E00`).
- The target: object 0, or, when that is no object, the command's event location 0 (a ground target). Read.
- Unless the target is the user, the approach (read): `Range` 0 → sub-action 4 (the melee ring, float 0 = 0.25, float 3 the user's personal space); any other `Range` → sub-action 15 with that row's `ranges.PrimaryRange` capped at 60 m (`0x0069F330`), float 1 = 0 (no least distance), flag 7 (line of sight) = `Projectile` ≠ 0, flag 13 = `TargetType` bit 6; a ground target → sub-action 14 with the point, stopping at that PrimaryRange when `Range` is not 0 and within 0.05 m of the point when `Range` is 0 (corrected 2026-10-23 by lib-agent-2: it said this without the condition; below). Both skipped when the user's state (its slot 0xC through +0x70) has bit 1 set. Then the turn: 39 to the object, or 40 to the point, skipped on that state's bit 2.
- **The ground target** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
  - **A target naming no object** (object 0 is −1, or no object) is a ground use: vector 0 of the command is the point, and the target is −1 (0x00744191–0x007441D4). The target-type check 0x00742E00 still runs, and a refusal queues nothing.
  - **The approach:**
    - **Range 0:** sub-action 14 with flag 1, int 2 and float 5 only. Floats 0 and 1 stay 0, so it walks to within 0.05 m of the point. An object target gets sub-action 4, the ring.
    - **Any other Range:** float 0 = PrimaryRange (capped, 0x0069F330), float 1 = 0, flag 7 = `Projectile` != 0, then sub-action 14 at the point (0x0074445A). An object target gets sub-action 15 instead.
    - **Then the turn:** unless the state's bit 2 is set, sub-action 40, a turn to the point. An object target gets 39.
  - **Sub-action 14 (0x007A99B0):**
    - On its first run it puts the point's z on the ground and sets float 2 = max(0, float 0 − float 1). It is done at once when the creature is already within the band (and in line of sight when flag 7 is set).
    - Otherwise it sets up the walk.
    - Every run returns the move update `0x007A9130(creature, sub-action, 0.05, 7)`, whose only −9 is the path search's answer 6 (a kind-7 cell, a shut door's, on the path). Nothing in the use's own checks gives −9.
- The conjure's set-up (read): from `ConjureAnim`, `Speed` and `ConjureVFX`. Where `ConjureAnim` is set, the conjure's length is that animation's length (`CCCreature::AnimationLength`), and, where the animation has a loop (`0x00695390`), plus the loop's length times `spellconjuringspeeds.Speed` (row `Speed`). The command's float 0, where above 0, shortens it (the animation's length less that time, `0x008E34F0` a float to int). Where `ConjureVFX` > 0, a visual effect (`CGameEffect` type 4, temporary, its duration the conjure's) is applied to the user at once (`0x007A1A20`). Inferred: float 0 is a conjure time a script gave the command.
- Then sub-action 50 with: int 0 the command's type (its byte +2: 25), int 1 the ability, int 2 = 4 << `AbilityType` (a mask), int 4 the conjure's milliseconds, int 5 `ConjureAnim`, int 6 a value from `0x00742F20` (not read), float 0 a playback scale, vector 0 the target's position (or the point), object 0 the target (invalid for a point), object 1 the item. Read.

### Sub-action 50 for an ability (`0x007A83F0`): COMMAND_PENDING

- Read: it builds `EVENT_TYPE_COMMAND_PENDING` (35): creator the user; int 1 the ability (int 0 set too, its value not followed); location 0 the target's position; object 0 the user, object 1 the target, object 2 the item (`0x006A0DA0`, by the ability's type). It runs it at once on the user's own script (`0x00698960`, `RunEventScript`), not queued.
- In the scripts, `creature_core` → `rules_core` → `ability_core` (its COMMAND_PENDING case reads int 1 as the ability, object 1 the target, object 2 the item, location 0 for a ground target): it checks the target type, the use conditions (`Ability_CheckUseConditions`) and a modal ability's state, runs the spell script's pending event (`EventSpellScriptPending`), and answers with `SetCommandResult`.
- Read: the engine then reads the answer at the user's +0x3D4. Not `COMMAND_RESULT_SUCCESS` (1): the step returns −1 and the command fails. Success: it queues sub-action 7 (the conjure, the step's data with flag 0 cleared) and sub-action 8 (the use, flag 1 = not (`Flags` & 1)). Where `Flags` & 1 (inferred: an aimed ability, such as a bow talent), `0x007A4DD0` queues, if the user sees the target, sub-action 37 playing animation 249, sub-action 6 (the aim, toward the object or point), and 37 playing animation 243.
- For an item ability whose item is the controlled party member's, the step also checks the item's charges (`0x006A9600` against +0x5F0, `0x006AE360`) and fails with −1 where it may not be used (read in outline).

### Sub-action 7: the conjure

- Timed, its duration the set-up's milliseconds: 3,866 ms for Chain Lightning, seen in play (`dev/TURN_BASED.md`). It plays `ConjureAnim`; an interrupted conjure never reaches sub-action 8. The handler (`0x007A66A0`) was not read further here.

### Sub-action 8: the use (`0x007A7440`)

- Read: it fails with −3 with no user, −1 where the ability's row cannot be read, −8 where the user's disabled flags (`0x00A64740`) meet int 2's mask; for a `Range` 0 ability at another creature, −1 unless `CCCreature::InStrikingReach` holds (the ring is not asked again).
- `EVENT_TYPE_ABILITY_CAST_START` (32), read: creator the user; object 0 the user, object 1 the item, object 2 the target; int 0 the ability. It is run at once with the script `ability_core` (`0x006985F0`, given the name), not the user's own script. In `ability_core`: the cost is subtracted here (`Ability_SubtractAbilityCost`, or a 1.5 s cooldown for a modal ability of an uncontrolled creature), and the spell script's cast event runs (`EventSpellScriptCast`). This is why the mana falls at sub-action 8 in play: the scripts take it, at CAST_START.
- Then, read: where the ability has a `CastAnim`, the use queues sub-action 37 playing it (flag 3 set), and the impact comes from that animation (inferred: from its animation event, as event 1 is an attack's impact, `dev/COMBAT.md`). Where it has none and flag 1 is set (that is, `Flags` & 1 clear), it raises `EVENT_TYPE_ABILITY_CAST_IMPACT` (31) itself: creator the user; object 0 the user, object 1 the item, object 2 the target, from the use's result record (`0x006A0180`); int 0 the ability, int 1 the record's third word (inferred: a hit or result value); vector 0 the record's location; the script `ability_core` (the event's script name, inferred to be what +0x2C of a `CScriptEvent` holds). It is queued to the user (`AIMaster::AddEventDeltaTime`, no delay), or, for an item ability in one state (`0x006A2CE0`), handed to the user at once (its slot 200).
- `ability_core`'s CAST_IMPACT reads int 0 (named "resistance", legacy), int 1 (the combat result), int 2 (hit), int 3 (hand), objects 1 (item) and 2 (target; a projectile's impact may carry more targets in 3 and up), location 0 (else the target's location), applies the impact VFX and runs the spell script's impact event (`EventSpellScriptImpact`).

### Sub-action 37: play an animation (`0x007A4390`)

- Read: a general player of an animation (int 0 the `anim_base` row; int 2 loops; float 3 a speed; flags 3 and 4 shortening and looping). Its duration is the animation's length (less a part where flag 3 is set, `0x00694FE0`), times the loops for a looping one. The cast's 1,707 ms for Chain Lightning (`dev/TURN_BASED.md`) is this.
- **The cast's length** (read 2026-10-25 by paragon-agent-2's helper, checked by lib-agent-2) [C unless marked]:
  - Sub-action 8 queues the cast clip as sub-action 37 with int 0 = CastAnim and flag 3 = 1, nothing else (`AddSubAction(0x25, data, 0, 1, 0)`, 0x007A7660–0x007A7693). Float 3 stays 0, so the speed is 1.
  - So a cast lasts `AnimationStepLength(CastAnim)` (the clip and what it runs on into, up to the first loop) less 20 %, with a missing node counting −1000 ms (`dev/ANIMATION.md`, "A missing animation (Q2)").
  - **Example, the arcane horror** (apr_base 12; its trees have `ua_acast` 247 = 2.267 s, `ua_dcast` 246 = 1.300 s, `ua_dcast_end` 290 = 0.767 s and `ua_instant` 292 = 1.567 s, but no `ua_acast_end` 291 and no `talent_threaten` 149) [C for the code; the clip lengths and the vanilla times are paragon-agent-2's]:

    | Cast | Step | Less 20 % | Vanilla |
    |---|---|---|---|
    | Cast Area, 247 → 291 | 2267 − 1000 = 1267 ms | 1014 ms | 1.05–1.17 s |
    | Cast Direct, 246 → 290 | 2067 ms | 1654 ms | 1.69–1.83 s |
    | Instant, 292 | 1567 ms | 1254 ms | 1.34 s |

    - paragon-agent-2's survey of apr_base found no other appearance with a missing node on a cast's chain [S].

### Which script runs an ability

- The engine names only `ability_core` (for CAST_START and CAST_IMPACT) and the user's own script (for COMMAND_PENDING, which `rules_core` passes to `ability_core`); it never reads `SpellScript` (no code uses its hash). The spell's script is the scripts' choice: `Ability_DoRunSpellScript` (`ability_h.nss`) takes the row's `SpellScript` resource from the ability's table (`Ability_GetAbilityTable`) and runs it with `_Ability_HandleEvent(ev, resource)`, passing a new event per phase (pending, cast, impact).

### COMMAND_COMPLETE

- As `dev/COMBAT.md` has it (`0x00743DD0`): int 0 the command type (25), int 1 the status, int 2 the ability for a use-ability command; −1 failed (a refused pending, a use out of reach, no row), −3 no user, −5 no room on the ring, −6 no line of sight, −8 disabled, −10 a timeout in the approach.

### Not read

- The conjure handler's own body, how the cast animation's event becomes CAST_IMPACT, the aim sub-action, `0x00742F20`'s value (int 6), and `0x007A4F90` (called by the conjure and the use with int 6).

## Ability timing, cost, cooldown and projectiles

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe, the toolset's scripts (`ability_core.nss`, `ability_h.nss`, `core_h.nss`) and the tables (`abi_base`, `spellconjuringspeeds`, `prj_base`), for DAOParagonEngine's use-ability work. Builds on `dev/TURN_BASED.md` (the sub-actions 15, 39, 50, 7, 8, 37 seen in play; conjure 3,866 ms and cast 1,707 ms for Chain Lightning) and `src/dao/Animation.cpp` / `Conjure.cpp`. "Read" is seen in the code; "inferred" is marked.

### Conjure and cast times

- The use-ability command's builder (`0x00743F50`, run when the command starts) reads `abi_base`'s `ConjureAnim`, `Speed` and `ConjureVFX` (read). With a `ConjureAnim`:
  - the conjure's start clip is `ConjureAnim`, its loop the clip `anim_base`'s `TransitionTo` names (`0x005CA6B0`), each clip's length from the creature's animation set (`CCCreature::AnimationLength`, `0x00694E50`);
  - when the loop clip loops (`0x00695390`), the number of loops is `spellconjuringspeeds` (table 135) row `Speed`'s `Speed` column: 1 Instant 0, 2 Fast 1, 3 Normal 2, 4 Slow 4, 5 Ritual 6;
  - the conjure's time in ms = start length + loops × loop length when loops > 0, else 0 (read). Chain Lightning (`Speed` 2, one loop): start + loop = the 3,866 ms seen in play (consistent). Arcane Bolt (200254, `Speed` 1, Instant) gets 0: no timed conjure (inferred from the formula, not seen);
  - a conjure time the command itself carries (the command's float 0, e.g. an order's) above 0 is **added** to the start clip's length, and the loops' result is discarded (0x00744679; corrected 2026-10-22: it was noted as replacing the computed time; "Conjure time arithmetic" below);
  - where the time is shorter than the start clip and there is no loop, the clip is sped up to fit (speed = start length / time; read);
  - `ConjureVFX` > 0: a visual effect (an effect of type 4, `CGameEffect`) is applied to the caster for the conjure's time (read).
  These go to sub-action 50 as int 4 (the conjure's ms), int 5 (the conjure clip) and float 0 (its speed); 50 queues 7, the timed conjure (read: the ints; the hand-on to 7 is `dev/TURN_BASED.md`'s).
- The cast is `abi_base`'s `CastAnim`, played by sub-action 37 at the clip's own length (`AnimationStepLength`, `dev/TURN_BASED.md`; 1,707 ms for 246 "Cast Direct"). With `CastAnim` 0 nothing is played and the impact comes at once (below).

#### Conjure time arithmetic, the builder 0x00743F50

- [C] (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) The whole conjure block is inside "ConjureAnim (the low 16 bits of the abi_base column 0x9ABEAEF1) is not 0". With 0, the builder sets neither int 4, int 5 nor float 0 of sub-action 50 and goes straight to the other fields.
- [C] startLen = CCCreature::AnimationLength (0x00694E50) of ConjureAnim; loopClip = 0x005CA6B0(ConjureAnim); loopLen = AnimationLength(loopClip). If 0x00695390(loopClip) says the clip does not loop, loopLen is 0 and the conjure stays 0. If it loops, loops = table 0x87 (spellconjuringspeeds) column 0x07F4F006 at the row the ability's Speed column (0x07F4F006 of table 1) names; when loops > 0 the conjure ms = loops x loopLen + startLen (IMUL then ADD at 0x0074465B-0x00744664), else 0. This is the bullet "the conjure's time in ms = start length + loops x loop length when loops > 0, else 0" above, confirmed.
- [C] A conjure time carried by the command (the float 0 of the command's own CDataArrays, CDataArrays::GetFloat(0) at 0x00744679), when above 0: conjure ms = startLen - (int)(T x D) where D is the double at 0x00B0ECF8, which is -1000.0 (bytes 00 00 00 00 00 40 8F C0), the conversion being the truncating 0x008E34F0. That is startLen + (int)(T x 1000): the time T (seconds) is ADDED to the start clip's length, and the loops result is discarded. It is not "replaces the computed time with T". Nothing else reads the float.
- [C] Playback scale (sub-action 50's float 0) is 1.0, except when conjure < startLen, loopLen is 0 and conjure > 0: then startLen / conjure (sped up to fit).
- [C] ConjureVFX (abi_base column 0x2B6BDC9F) > 0: a CGameEffect of type 4 (kind field +8 = 2, duration field +0xC = round(conjure / scale) ms, subject the caster) is applied through 0x007A1A20.
- [C] Sub-action 50's fields as the builder writes them: int 0 the command's byte +2, int 1 the ability, int 2 = 4 << AbilityType (SHL at 0x007447F8), int 4 the conjure ms, int 5 the ConjureAnim, int 6 the value of 0x00742F20, float 0 the scale, vector 0 the target position or point, object 0 the target (invalid for a point), object 1 the item. Only the ints 4, 5 and the float 0 are inside the ConjureAnim branch.

### Cost

- **The engine does not take the cost; the scripts do.** Sub-action 8 (`0x007A7440`) sends `EVENT_TYPE_ABILITY_CAST_START` (32) and nothing else touches mana or stamina there (read). `ability_core`'s CAST_START case calls `Ability_SubtractAbilityCost(oCaster, nAbility, oItem)` (`ability_h.nss`), for every ability but a modal one (read in the scripts). That is the drop the probe saw "at type 8".
- `abi_base`'s `Cost` is read by the engine only to check, not to take: `CCCreatureStats::CanUseAbility` (`0x0071CFFD`; Cost × (1 + fatigue/100) against property 8's current value, `dao/Addresses.h`) and the interface's ability info (`0x0062C9FA`). The builder (`0x00744166`) asks `CanUseAbility` and makes no command when it fails (read); `ability_core`'s COMMAND_PENDING case checks again with `Ability_CostCheck` and fails the ability (read in the scripts).
- `CostUpkeep` (sustained abilities) is read by the engine only for the interface (`0x0062CA19`); the upkeep is the scripts' (`effect_upkeep_h`), as `ability_core`'s comments say (read in the scripts).

#### Why an ability is greyed (read 2026-10-23 by lib-agent-2 with a subagent, for DAOParagonEngine #196)

- **CanUseAbility 0x0071CE90** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) (ID 16539) is (ability, uint* reason, itemId = −1), `ret 0xC`. The reason is written only through a non-null pointer [C].
- **The order of the checks** [C] (corrected 2026-10-23 by lib-agent-2: it replaces the earlier, shorter list):
  - **The item checks:** for an item ability (AbilityType 4), 0x0071C0A0 false gives 244342. Then, with `stats+0x1B4` set, CanUseItem 0x0062AA80 false gives reason 0.
  - **The active modal:** an ability in the active-modal vector (stats +0xF8, ABILITIES.md:127) is usable at once (0x0071CF8A), before the disabled test.
  - **The disabled test:** `(4 << AbilityType | 0x80) & 0x00A64740()` gives 223888. 4 << type is DISABLE_COMBAT, TALENTS, SPELLS, SKILLS or ITEMS for types 0–4. 0x80 is EFFECT_FLAG_DISABLE_INPUT, which disables every type.
  - **The cost:** abi_base `Cost` (0xF03D763E) × (1 + property 41's modifier, vtable +0x18, × 0.01), against property 8's current value (vtable +0x24).
    - It is asked of every type, and refused only when the cost is greater, so an equal cost passes.
    - It is excused while the active-modal vector holds 10700 (Blood Magic).
  - **The reason is `0x5BDCC − (ClassHasMana != 0)`:** 376267 "Insufficient mana" when the stats' `ClassHasMana` bool (+0x6C) is set, else 376268 "Insufficient stamina".
  - **The order at the end:** the conditions (0x0071C310, mask 0x7FFFFFFF), then the cooldown (0x0071CE60 > 0, refused with no reason), then conjuring (395779). So the cooldown comes after the conditions and before conjuring.
- **ClassHasMana** [C]:
  - It is false when the stats are made, and registered as L"ClassHasMana" by 0x0071A940.
  - 0x0071A070(class) sets it to cla_data (table 3) `BUsesMana` (0x34E302CF) != 0 at the class's row. Its callers are 0x0071B0D3, after a property-27 set, and 0x0071E3FC.
  - **cla_data `BUsesMana` is 1 for** Wizard, Shapeshifter, Spirit Healer, Blood Mage (12), Shale (16), MONSTER_SPIRIT (23), MONSTER_BALANCED_MENTAL (24) and MONSTER_SPELLCASTER (25).
  - **It is 0 for** Warrior, Rogue, Champion, Templar, Berserker, Reaver, Arcane Warrior, Assassin, Bard, Ranger, Duelist, the Dog and the physical monster classes.
- **The conditions** (corrected 2026-10-23 by lib-agent-2: the "0x100 and 0x200/0x400/0x800" line was wrong) [C, 0x0071C310]:
  - **The mask** is abi_base `Conditions` (0xB1CAEE1D) & the asked mask; none means usable.
  - **The weapon bits** (mask & 0xC7) come from the main and off hands' bitm Type (`item+0x6E0`):
    - **4:** main 6, else 222425 "No ranged weapon equipped", returning at once.
    - **1:** main 2, or off 2, or main 0 (unarmed counts as melee), else 222426.
    - **2:** off 3, else 222427.
    - **0x40:** both 2, else 315522.
    - **Bits 1, 2 and 0x40:** each failure writes its reason, and the last test's verdict counts.
    - **0x80:** main 2 and 0x00A664C0 true, else 378484 and refused. When it passes, earlier verdicts are dropped. 0x00A664C0 is EquippableSlots 0x00A66440 == 1 << 0, a two-handed item (ITEMS.md; read 2026-10-23, [C]).
  - **0x100:** `Condition_AppGroup` (0xC7E5BB24) & the appearance's restriction group (0x0071A210) == 0 gives 383166 "Not useable by current race."
  - **0xE00, the class** (0x0071B010: property 27's base, truncated). The block runs only when the mask holds some of 0xE00. A bit means the class may use the ability:
    - class 1 (Warrior) without 0x800 gives 383167 "Warriors cannot use this.";
    - class 2 (Wizard) without 0x200 gives 383168 "Mages…";
    - class 3 (Rogue) without 0x400 gives 383169 "Rogues…";
    - any other class passes.
    - (An earlier 0x200/0x400/0x800 → 383167/8/9 mapping was wrong on all three.) A specialisation does not change property 27, as far as the scripts go [C, read 2026-10-23]: 0x007FCE60 raises CHARGEN_ASSIGN_ABILITIES (51) to the module, and sys_chargen's 51 spends property 38 and adds the ability. Property 27's script writers are Chargen_SelectCoreClass (sys_chargen_h.nss:798/802), sys_autoscale_h.nss:379 and ability_summon_h.nss:669; native writers (the template hand-over, a save load) were not read [I].
  - **0x10:** `Condition_Mode` (0x07342EA2) non-zero and HasActiveModalAbility 0x0066DD60(mode) false gives 310917, and is refused. It is the last test.
- The quickbar's round-robin step that asks `CanUseAbility` for each slot: GUI.md, "The quickbar: the load, the rebuild, a slot's item and which slots are usable".

### Cooldown

- **Set by the scripts**, from `abi_base`'s `Cooldown`: `Ability_SetCooldown(oCaster, nAbility, oItem)` (`core_h.nss`, over the `SetCooldown` command, `kCCCreatureStats_SetCooldown`). The engine reads `Cooldown` only for the interface (`0x0062C9DB`) (read).
- When (read in `ability_core`): on `ABILITY_CAST_IMPACT` for a non-modal ability without a projectile (and for every item ability); on `EVENT_TYPE_ABILITY_PROJECTILE_LAUNCHED` (93) for one with a projectile; a modal ability gets 1.5 s at CAST_START for AI casters only, so the AI does not re-run it.

### The events (engine side), all run with the script `ability_core`

- `ABILITY_CAST_START` (32), from sub-action 8 (`0x007A7440`, read): creator and object 0 the caster, object 2 the command's target, int 0 the ability, plus the fields `0x006A0DA0` adds; run on the caster with the script named `ability_core` (`0x006985F0`, the script given by name, not the ability's `SpellScript`). Before it, sub-action 8 fails the command with −8 when the ability's type is disabled (`0x00A64740` flags) and with −1 when a `Range` 0 ability's target is out of striking reach. After it, with a `CastAnim` it queues sub-action 37 to play it (int 0 the clip, flag 3); with none, and the command's flag 1, it raises `ABILITY_CAST_IMPACT` at once (queued, or for an ability of type 4, item, through the object's slot 200).
- `ABILITY_CAST_IMPACT` (31) from the cast animation's impact (`0x006AF430`, read): queued to the caster (`AddEventDeltaTime`, no delay), the event's target field the target, object 0 the caster, object 2 the target, int 0 and int 1 from the cast record (`0x006A0180`, its +0 and +8), int 2 a counter of impacts, int 3 (corrected 2026-10-24 by lib-agent-2, from paragon-agent-2's reading) [C, 0x006AF539–0x006AF55D] written only when the event's string (record +0x20, length +0x34) is non-empty: it is `find(L"Left") != 0` (`wstring::find` 0x004C0FB0, `L"Left"` at 0x00B0F840), which is −1 with no match, so it is 0 only for a string that begins with "Left", and 1 for any other string, "|PlayHit" included; with an empty string the event has no int 3; that "Left" marks the off hand is [I], vector 0 the target point; script `ability_core`.
  - **The instant impact with CastAnim 0 and the other answers of sub-action 8** (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
    - **The instant impact with CastAnim 0:**
      - Built only when the step's flag 1 is set.
      - An item ability (0x00719990 == 4) of an active party member (0x006A2CE0) goes to the creature's slot 200 at once.
      - Any other goes to `AIMaster::AddEventDeltaTime(0, 0, caster, caster, 10, event)`, due now and delivered at the next frame's drain. The sub-action answers 1 either way.
      - So the use is still current when its impact runs, and it ends at its next step. If the impact's script applies an effect that queues ExecuteEffect (slot 0x1C, a clearing command), the use ends COMMAND_FAILED_COMMAND_CLEARED (-2) instead.
      - Vanilla's grabs (Overwhelm, paragon-agent-2's trace, 3 of 3 in each run): the impact at 44,931.9 ms, the UseAbility cleared at 44,971.1, and the 32 started at 44,972.
    - **The other answers:** -3 with no creature; -1 with no abi row, or for a target out of striking reach (`Range` 0); -8 when disabled (int 2 & 0x00A64740()). A target check (0x007A74FE–0x007A7553) can also answer -1.
  - **The hit counter, the item and effect_shown** (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2; corrected 2026-10-24 by lib-agent-2 where it differs from an earlier reading) [C]:
    - Add [C]:
      - **The count:** the counter is the record's +0xC. It starts at 1 and is incremented after use, so the first animated impact has nHit 1.
      - **The item:** 0x006AF430 passes object 1 as the item only when the ability's type (0x00719990) is 4 (an item ability), else −1.
      - **effect_shown 0x006A3B90**, called only from sub-action 51 (0x007AB171), fills the record from an effect: the ability from effect +0x44, and +4 from the effect's object 1. Sub-action 51 then stores the partner at creature +0x744 (0x007AB186).
    - Added (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C unless marked]:
      - **The impact needs the current clip:** case 7 of `CCCreature::OnAnimationEvent` (0x006C0C5F) assigns the event's string (event +0x1C) to the record's +0x20, then calls 0x006AF430(event).
        - With an event given, its clip must be the creature's current animation. The clip is the event's +0x18 name, made an id by 0x005CB780 (0x00694C40); the current animation is the controller's +0x60 → +0x13C. Otherwise nothing is raised.
        - It also needs the record's ability (+0) to be non-zero.
      - **effect_shown (0x006A3B90)** writes +0 and +4 only when the effect's +0x44 (its ability) is non-zero, and nothing else. The counter is kept.
        - Sub-action 51 calls it only on its branch that plays a clip: not for type 8 (paralysis), and not for a row with no animation (−1).
      - **So an effect's clip raises numbered impacts.** Every event 7 of the clip the effect plays raises ABILITY_CAST_IMPACT with the ability and partner the effect gave, and the counter goes on from 1. Sub-action 51 itself counts and times nothing.
      - **Example, Overwhelm** (the werewolf, apr_base 22, `c_wrwlf_cbt.anb`) [C for the data]:
        - **The rows:** the attacker holds effects.gda row 1043 and the victim row 1042, both labelled `EFFECT_TYPE_OVERWHELMED`; they differ in BConsiderHostile, 1 for 1042 and 0 for 1043, and both have Priority 10 and Flags 543. The attacker's Animation is 2019 `talent_overwhelm`, TransitionTo 2053 (the loop), then 2054 (the exit). The victim's: 2055, then 2057 (the loop), then 2056.
        - **The attacker's clips:** 2019 is `c_shka.tal_ovlm_en_w`, with event 7 at 0.00, 0.60, 1.43 '|PlayHit', 1.90 '|PlayHit', 2.53 and 3.13. 2053 is `c_shka.tal_ovlm_lp_w`, with event 7 at 0.27 and 0.77 '|PlayHit'. 2054 is `c_shka.tal_ovlm_ex_w`, with event 7 at 0.23 and 0.97 '|PlayHit'. The `.evt` files hold the events.
        - **The victim's clips** were not read here. paragon-agent-2 has them with no event 7, so only the attacker raises impacts.
        - **Vanilla's trace agrees** (paragon-agent-2's trace, fight-compare/vanilla-run1.events.jsonl):
          - Hit 0 is sub-action 8's instant impact (CastAnim 0), at 44,931.8 ms.
          - ExecuteEffect starts at 44,972. Hits 1–6 follow at 44,982, 45,534, 46,434, 46,854, 47,519 and 48,057, at 2019's event times.
          - Hits 3 and 4 ('|PlayHit') carry int 3 = 1. Every hit has int 1 = 0x80000000, the reset's value.
        - The UseAbility that the 32 clears ends through the shared end, which resets the record, so the 32's first animated hit is 1.
- `ABILITY_CAST_IMPACT` (31) from a projectile's arrival (`0x007E71C0`, read): queued to the projectile's event target (+0x4C4), or its owner (+0x4BC) when that is −1; object 0 is the event target (+0x4C4), −1 for an ability's projectile, whose path sets only the owner (+0x4BC at 0x006B70C5) and the target (+0x4C0 at 0x006B7203) (corrected 2026-10-18 by lib-agent-2; the impact 0x007E71C0 writes object 0 from +0x4C4 at 0x007E7230); object 1 −1, int 0 and int 1 from the cast record, vector 0 the projectile's position, vector 1 its heading (inferred), object 2 the target or objects 2 and up those hit; script `ability_core`. (A weapon's projectile instead gives `ATTACK_IMPACT` (29), `0x007E87C0`, `dev/COMBAT.md`.)
- `EVENT_TYPE_ABILITY_PROJECTILE_LAUNCHED` (93; not in `script.ldf`, `ability_core`'s `case 93`), from the creature's vtable index 55 (`0x006AFA40`, read; not slot 56, `0x006AFA50`: corrected 2026-10-18, lib-agent-2, from the vtable bytes): run on the caster with `ability_core`, object 0 the caster, int 0 the ability, vector 0 a point (the projectile's target, inferred); raised when the current command has a projectile in flight (`GetProjectileById`).
  - [C] (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169; the creature's vtable entry at 0x006AFA40, Ghidra "CCCreature::vfunction56") 0x006AFA40 clears a byte (+0xE) of the creature's command-info record (0x0069E9D0), asks 0x00697BF0 for a projectile id and CClientExoApp::GetProjectileById (0x005D5AF0). With one in flight and the lazily made 0x50-byte record of 0x007E68B0 (0x006972A0, held at [holder+8]) whose first dword is non-zero, it builds a CScriptEvent of type 0x5D (93): event field +4 = the creature's id, CDataArrays object 0 = the creature's id, integer 0 = the record's first dword, vector 0 = the record's dwords from +0x10, run on the caster with the script "ability_core" (0x006985F0). Then the base slot (CCObject) is called.
  - [I] That the record's first dword is the ability id and +0x10 the projectile's target point: the record's writer was not read.

### Projectiles

- Made by the creature's vtable index 60 (`0x006B6F60`, offset 0xF0 in `CCCreature`'s vtable 0x00B10484, read; not slot 61, which is `0x006A6740`: corrected 2026-10-18, lib-agent-2, from the vtable bytes), for a current command of type 1 (attack) or 25 (use ability); inferred: on the cast animation's launch event. For an ability: `abi_base`'s `Flags` bit 0 set takes the equipped weapon's projectile (`bitm_base` `Projectile`, or the weapon's local override), else `abi_base`'s `Projectile` and `Crust_ID`. With no projectile (0), a use-ability goes straight to its impact (`0x006AF430(0)`). Otherwise `CreateObject` makes the projectile and sets: owner (+0x4BC), `prj_base` row (+0x4A4), crust (+0x4B4), from an ability (+0x4B8), the target point (+0x4E0, the target creature's position unless it is gone, else the event's location) and the target (+0x4C0); it is added to the projectile manager (`0x00679EA0` on `[app+0x104]`, `CProjectileManager`; not to the area: corrected 2026-10-18 by lib-agent-2, `dev/script_commands/objects.md`).
- **Which field is the crust is not settled** (corrected 2026-10-18 by lib-agent-2). This path (`0x006B70C5`–`0x006B7111`) writes both +0x4AC (a value from `0x00702E90`) and +0x4B4, and the crust is named +0x4B4 above; `FireProjectile`'s handler stores its `nCrustEffectId` at +0x4AC, not +0x4B4 (`dev/script_commands/objects.md`). One of the two names is wrong; which was not read.
- Its flight comes from `prj_base` (table 1006): `InitialSpeed`, `Acceleration`, `MinSpeed`, `TopSpeed`, `Simulation`, `ImpactTimeout`, read at `0x007E8156` and `0x007E6900`–`0x007E6984` (the flight's integration not read). Arcane Bolt (`abi_base` 200254: `Projectile` 137, `Range` 4, `Speed` 1, `Cooldown` 6.0, `Cost` 15.0, `ConjureAnim` 244, `CastAnim` 246, `SpellScript` `spell_singletarget.ncs`): `prj_base` 137 "Arcane Bolt", model `fxa_arc_bolt_p`, `InitialSpeed` 5.0, `Acceleration` 10.0, `MinSpeed` 15.0, `TopSpeed` 40.0 (m/s and m/s², inferred), static collisions 1. Chain Lightning's is 114: 10 initial, 10 acceleration, 20 top.
- The spell's own script (`SpellScript`) is reached by `ability_core` through `Ability_DoRunSpellScript` with its own events (`EventSpellScriptCast`, `EventSpellScriptImpact`), not by the engine (read in the scripts).

### What switches a modal ability off: 0x0071DEE0 (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C unless marked]

- **0x0071DEE0(bAutoCancel, nAbility, bShapechange)**, `ret 0xC`, walks the stats' modal list (+0xF8) and switches off (0x006B9970) each ability that matches one of three cases:
  - **bAutoCancel:** a mode whose abi_base `AutoCancel` is set.
  - **nAbility:** a mode in the same `Condition_Group` as nAbility.
  - **bShapechange:** the ability held by a shapechange effect (effect +0x44). It is found through the effect search 0x00A64FA0 → 0x00A64D80: +0x44 != 0, duration type +0x28 not 1, 5 or 6, type +0x24 not 4, 6 or 10, and +0x48 == 0.
- **Its six callers** (an E8 scan):
  - **SetAppearance** (0x006C1591) passes (0, 0, 1), on its "changed" branch, unless a skip flag ([esp+0x38]) is set. So an appearance change ends a shapechange's mode.
  - **QueueCommand** (0x0073CEF3) passes (0, the command's int 0, 0), only for a command of type 25 (UseAbility). So queuing an ability switches off the modes of its Condition_Group.
  - **Four combat sub-actions** (0x007A8E06, 0x007A9B96, 0x007AA4AB, 0x007AA786) pass (1, 0, 0), each right after 0x007A4F90. So starting to move or attack switches off every AutoCancel mode.
- **The shapeshift abilities** [C, ghilana abi_base]:
  - UseType, `Condition_Group` 17007, AutoCancel 0, CostUpkeep 30/50/60 (or 0) and an empty BlendTree.
  - Cooldowns: 0.5 for the four plot forms, 60 for Swarm and 90 for Spider and Bear.
  - ability_h.nss:253 makes a shapeshifted caster's spells and talents free.
- **0x006BBCB0, the effect flags' reader** [C]:
  - Callers: 0x0062FE2A, 0x006BCF78 and 0x006BD6DC. The first and last come right after the stats' 0x0071E300 and 0x0071D4A0.
  - Its effects' flag 0x100 goes to 0x00719AC0 (LockQuickslotBar). The inverse of 0x200 goes to the +0x5F4 object's slot 0x54 (the equipment-change permission [I for the name]).
  - It is gated by +0xC3D, which the constructor sets to 1 (0x006B4627) and 0x006BBCB0 clears (0x006BBCE6). So the gated part runs once per creature.

## IsModalAbilityActive and Engine_SetModalAbilityGUI share one list

- `IsModalAbilityActive` (726, 0x0066DD80) searches the creature's stats' (+0x7C4 → `CCCreatureStats`) **+0xF8 vector of ability ids**.
- `Engine_SetModalAbilityGUI` (725, 0x00669E80 → 0x0071DA90(ability, status, 0)) writes that same vector. Status 0 erases the id and anything else appends it; then it sets the quickbar button's toggle (0x00719A40).
  - On a real change it may also send event 0x88 (on) or 0x89 (off) to the creature, when a table string is set and an equipment check passes (purpose not read).
- **Its other writer** is a save's load: `SavegameCreatureModalAbilityList` (16468), through 0x0071DA90(id, 1, 1).
- **The scripts call 725** from the upkeep effect: `effect_upkeep_h.nss`:107 on apply (TRUE), :212 on remove (FALSE).
- So a modal ability is "active" exactly while its upkeep effect's handlers say so; the engine reads no effect for it.

Read 2026-10-18 by lib-agent-2 for DAOParagonEngine, not seen in play.

## Passives: where their modifiers live, and when they come and go

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, a call scanner over `.text`, `tools\gda.py` (retired 2026-10-24, DAOLib #106: the tables are read by `dao/core/GdaFile.h`, their columns listed in `dev/gda_schema.csv`), and ghilana's merged tables, for DAOParagonEngine. Not seen in play. "(checked)" means read in the disassembly or the table; "(inferred)" means not. Builds on `dev/ITEMS.md` (item stats, `SetItemMaterialType`), `dev/PROPERTIES.md` (property slots), `dev/COMBAT.md` (0x0071EAC0, ability add), and `dev/script_commands/stats.md` (254, 333).

### Where a passive's modifiers live (checked)
- An ability's passive is `ABI_base.Passive_ID` (hash 0xC4EE8EE0) → table **117 `passive_abilities`** (`ID, Prop1, Modifier1, Prop2, Modifier2, Prop3, Modifier3, Conditional`; 99 rows, 97 ABI rows have a Passive_ID).
- All of it is native: **0x0071B600** `CCCreatureStats::ApplyPassive(ability, condition, bApply)` (ID 16516, `ret 0xC`):
  - factor = 1 if `Conditional == 0`; else 1 if the `condition` argument is a single bit present in `Conditional` (switch at 0x0071B6C1 over condition−1, bytes 0x0071B960: 1→bit 1, 2→bit 2, 4→bit 4, 8→bit 8, 16→bit 0x10; any other value, 0xFF included → 0).
  - `bApply == 0` multiplies the factor by −1.0 (double at 0x00AF0E48).
  - for i = 1..3 (`Prop%d`/`Modifier%d`, 0x00B17620/0x00B17608): if `Prop_i != 0`, `0x0071B2D0(Prop_i, factor × Modifier_i, 1)`: **adds to the property's MODIFIER** (+0x54, keeping the current, PROPERTIES.md). On apply, health's current is raised only under the test below (corrected 2026-10-23 by lib-agent-2: it had the test on the current before the modifier, and no test that the maximum changed; read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2).
- **The health top-up** [C, 0x0071B802–0x0071B8C3]:
  - **Before:** for each i, it takes the old maximum, `old = creature->vtable[+0x128]()`, or −1 with no creature (stats+0x5C).
  - **The add:** it then calls 0x0071B2D0(Prop_i, factor × Modifier_i, 1).
  - **After:** it reads `cur = GetCurrent(Prop_i)` (0x006437A0 on stats+0x190), then the new maximum `new = vtable[+0x128]()` again.
  - **The test:** only when all of these hold:
    - bApply != 0;
    - Prop_i == 7;
    - old != new;
    - the truncated `cur` (0x008E34F0) == old.
  - **Then** it calls `SetCurrent(7, GetCurrent(7) + Modifier_i, 0)` (0x0071B320).
  - The amount added is the table's raw Modifier_i, not factor × Modifier_i. That only matters when a conditional passive's factor is 0, and then the maximum has not changed, so no top-up happens.
- **What slot +0x128 returns:** CCCreature's vtable slot +0x128 is 0x006AB530 (ID 14689). It returns health's total (property 7's slot +0xC), truncated, or 0 with no stats [C].
- **So** a top-up happens only when the maximum moved and the current, after the add, still equals the old maximum.
  - A penalty that cannot lower the maximum takes nothing more from the current. An example is TRAIT_FRAGILE's −20 on a creature whose health total is already clamped at its minimum 1 [C for the rule].
  - That a template creature's health is 0 base at that point, before AS_InitCreature scales it, is [I], from a play test in DAOParagonEngine, not read in the game.
- **TRAIT_FRAGILE** [C, data]: abi_base 150001 has Passive_ID 1001. passive_abilities 1001 is Prop1 7 (Health) −20, Prop2 10 (Defense) −10, Prop3 33 −5, Conditional 0.
- No script applies passive property modifiers (grep of the toolset scripts: none). The scripts only add extras on top by `HasAbility` (e.g. `ability_h.nss:1342`, Dual-Weapon Expert's bleed on hit).

### Who calls ApplyPassive (all 8 calls found by scanning `.text`, checked)
- **Add** 0x0071EAC0 (AddAbility 253 and template lists, COMBAT.md): `ApplyPassive(ability, stats+0x1B8, 1)` at 0x0071EC8C, only if the ability is new and creature+0xC58 is 0.
- **Remove** 0x0071EF00 (RemoveAbility 254 passes `(ability, type, 0, 3)`): unless the 4th argument is 2, `ApplyPassive(ability, stats+0x1B8, 0)` at 0x0071EF84, right after 0x0071A450 and before the vector erase.
- **Condition change** 0x0071B970 (ID 16517): `c = 0x0071A0C0()`; if `c != stats+0x1B8`, for every ability in the four vectors (+0x150 talents, +0x160 spells, +0x170 skills, +0x180 item abilities): `ApplyPassive(a, old, 0)` then `ApplyPassive(a, c, 1)`; then `stats+0x1B8 = c`. Unconditional passives net to zero; conditional ones come off and go on with the weapon style. Its only callers: the creature's item-equipped handler 0x006C16E0 (call 0x006C18A8) and item-unequipped handler 0x006C19F0 (0x006C1AD5).
- **Save** 0x0071BA40 (ID 16518, from 0x006B256D): before writing the properties (0x00797000), `ApplyPassive(a, stats+0x1B8, 0)` + `ApplyPassive(a, 0xFF, 1)` for all (leaves only the unconditional passives in the modifiers), then after, `ApplyPassive(a, 0xFF, 0)` + `ApplyPassive(a, stats+0x1B8, 1)` (restores). So a save holds unconditional passives inside the saved modifiers and conditional ones not (checked in outline; the load side not read: inferred that +0xC58 marks a creature being restored, so AddAbility does not apply passives twice).

### (a) RemoveAbility removes the passive's modifiers: yes (checked)
For a talent/spell/skill it subtracts each `Modifier_i` from `Prop_i`'s modifier, with the same condition the add used (stats+0x1B8), so a conditional passive that is currently active is subtracted and an inactive one is not. An item ability (`AbilityType` 4) is refused by the handler and keeps its passive. Not on removal: the health top-up (only on apply); lowering a depletable's modifier re-clamps its current (PROPERTIES.md).

### (b) Conditional passives 5200-5202: the engine, on equip/unequip (checked)
| passive_abilities ID | ABI_base row (Label, GUIType 9) | Props | Conditional |
|---|---|---|---|
| 5200 | 8 DUAL_WEAPON_FINESSE "Dual-Weapon Finesse" | 9 Attack +5, 10 Defense +5 | 8 |
| 5201 | 5 DUAL_WEAPON_EXPERT "Dual-Weapon Expert" | 21 Melee_Crit_Modifier +2.5 | 8 |
| 5202 | 3036 DUAL_WEAPON_MASTER "Dual-Weapon Mastery" | 41 Fatique (fatigue) −2.5 | 8 |

These are the only rows with `Conditional ≠ 0` in the core table (ghilana; four add-in `passive_abilities_*` files are enciphered and unread). ABI_base's own `Conditions/Condition_Mode/Condition_Group` are 0 on these rows: the condition is `passive_abilities.Conditional`.

The condition 0x0071A0C0 (ID 16490), from the creature's two hands (0x006A2B40) and `bitm_base.Type` (item+0x6E0, hash 0x12A02374; ITEM_TYPE_* in `2da_constants_h.nss:440-446`):
| main hand | off hand | condition |
|---|---|---|
| Type 6 ranged or 5 staff/wand | any | 16 (0x10) |
| Type 2 melee, `EquippableSlots` == 1 (main hand only: two-handed, inferred) | none | 4 |
| Type 2 melee, otherwise | none | 1 |
| Type 2 melee | Type 3 shield | 2 |
| Type 2 melee | Type 2 melee | **8 (dual weapons)** |
| anything else (unarmed, melee + other) | | 0xFF (no conditional passive) |

So the dual-weapon passives apply natively when both hands hold melee weapons, and are re-evaluated **only when an item is equipped or unequipped** (0x0071B970's two callers); no per-frame update and no script event. A weapon-set switch goes through the same equip/unequip handlers (inferred). Gaining the talent while already dual-wielding applies it at once (AddAbility passes stats+0x1B8). The initial value of stats+0x1B8 was not read.
  - the stats' constructor 0x0071E940 stores `+0x1B8 = 0xFF` (0x0071EA8F), with +0x1B4 and +0x1BC = 0. 0xFF is "no condition", as 0x0071A0C0 returns and the save pairs use [C]. (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2, [C])

### (c) ClearCreatureProperties keeps passives (checked)
Passives are **modifiers** (+0x54), never bases. ClearCreatureProperties (333, 0x00795C10) sets only each property's base (and a depletable's current) to max(Min, 0) and does not touch modifiers, so every applied passive (and every equipped item's stats, also modifiers) survives it; the creature's totals after it are max(Min,0) + engine link + the kept modifiers.

### Not read

- creature+0xC58's meaning (inferred: restoring from a save) and the load path of passives; stats+0x1B8's initial value is read (below, corrected 2026-10-23 by lib-agent-2).
- Whether the four enciphered add-in `passive_abilities_*` files add rows (the running game reads them; Eluvian can).
