# Abilities as the engine carries them out

Read 2026-10-18 by lib-agent-2 (two subagents in parallel) in the GOG exe, the toolset's scripts and the game's tables, for DAOParagonEngine's next batch (blows that land, the mage's Harrowing), at paragon-agent-2's request. How `COMMAND_TYPE_USE_ABILITY` (25, `CommandUseAbility` 582) is queued and stepped, the events the scripts get, and where timing, cost, cooldown and projectiles come from. Builds on `dev/TURN_BASED.md` (the sub-actions, conjure 3.87 s), `dev/COMBAT.md` (COMMAND_PENDING, COMMAND_COMPLETE) and `src/dao/Conjure.cpp`. Each point says what was read and what is inferred. Not yet seen in play as a whole.

## The use-ability command and its events

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe and the toolset's scripts (`ability_core.nss`, `ability_h.nss`), for DAOParagonEngine's S5/S6. Builds on `dev/TURN_BASED.md` (the sub-action types seen in play: 15, 39, 50, 7, 8, 37; the conjure 3,866 ms and the cast 1,707 ms for Chain Lightning; the approach by `Range`) and `dev/COMBAT.md` (COMMAND_PENDING, COMMAND_COMPLETE). "Read" is what the code shows; "inferred" is marked. Column names are matched by the GDA hash (CRC-32 of the lowercase UTF-16 name); table 1 is `abi_*`, 134 `ranges`, 135 `spellconjuringspeeds`.

### The sub-actions and their handlers

`CSSubActionHandler`'s table (filled by `0x007AB8D0`, a handler per type at +0x48 + 4 × type) gives: 4 `0x007AA560` (approach the melee ring), 6 `0x007A1ED0`? (aim, see below), 7 `0x007A66A0` (conjure), 8 `0x007A7440` (use), 14 `0x007A99B0` (approach a point), 15 `0x007AA200` (approach to a range), 37 `0x007A4390` (play an animation), 39 `0x007A69E0` (turn to an object), 40 `0x007A6AF0` (turn to a point), 50 `0x007A1E40` (the command's own step, dispatched again by the command type through a second table at +0x4C; type 25's is `0x007A83F0`), 51 `0x007AAC10` (death). Read (the table's stores); the aim handler's address is not confirmed.

### Queuing them: the UseAbility step (`0x00743F50`)

- Read: it stops at once (queuing nothing, so the command ends) if the user is dead, if `abi`'s `AbilityType`, `TargetType` or `Range` cannot be read, if an item ability (`AbilityType` 4) has no item (object 1, or found by the string 0's tag through `0x006A6400`), if `CCCreatureStats::CanUseAbility` refuses, or if the target is not valid for the ability's `TargetType` (`0x00742E00`).
- The target: object 0, or, when that is no object, the command's event location 0 (a ground target). Read.
- Unless the target is the user, the approach (read): `Range` 0 → sub-action 4 (the melee ring, float 0 = 0.25, float 3 the user's personal space); any other `Range` → sub-action 15 with that row's `ranges.PrimaryRange` capped at 60 m (`0x0069F330`), float 1 = 0 (no least distance), flag 7 (line of sight) = `Projectile` ≠ 0, flag 13 = `TargetType` bit 6; a ground target → sub-action 14 with the point. Both skipped when the user's state (its slot 0xC through +0x70) has bit 1 set. Then the turn: 39 to the object, or 40 to the point, skipped on that state's bit 2.
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
  - a conjure time the command itself carries (the command's float 0, e.g. an order's) above 0 replaces this (`0x007446xx`, read in outline; its exact arithmetic is not settled);
  - where the time is shorter than the start clip and there is no loop, the clip is sped up to fit (speed = start length / time; read);
  - `ConjureVFX` > 0: a visual effect (an effect of type 4, `CGameEffect`) is applied to the caster for the conjure's time (read).
  These go to sub-action 50 as int 4 (the conjure's ms), int 5 (the conjure clip) and float 0 (its speed); 50 queues 7, the timed conjure (read: the ints; the hand-on to 7 is `dev/TURN_BASED.md`'s).
- The cast is `abi_base`'s `CastAnim`, played by sub-action 37 at the clip's own length (`AnimationStepLength`, `dev/TURN_BASED.md`; 1,707 ms for 246 "Cast Direct"). With `CastAnim` 0 nothing is played and the impact comes at once (below).

### Cost

- **The engine does not take the cost; the scripts do.** Sub-action 8 (`0x007A7440`) sends `EVENT_TYPE_ABILITY_CAST_START` (32) and nothing else touches mana or stamina there (read). `ability_core`'s CAST_START case calls `Ability_SubtractAbilityCost(oCaster, nAbility, oItem)` (`ability_h.nss`), for every ability but a modal one (read in the scripts). That is the drop the probe saw "at type 8".
- `abi_base`'s `Cost` is read by the engine only to check, not to take: `CCCreatureStats::CanUseAbility` (`0x0071CFFD`; Cost × (1 + fatigue/100) against property 8's current value, `dao/Addresses.h`) and the interface's ability info (`0x0062C9FA`). The builder (`0x00744166`) asks `CanUseAbility` and makes no command when it fails (read); `ability_core`'s COMMAND_PENDING case checks again with `Ability_CostCheck` and fails the ability (read in the scripts).
- `CostUpkeep` (sustained abilities) is read by the engine only for the interface (`0x0062CA19`); the upkeep is the scripts' (`effect_upkeep_h`), as `ability_core`'s comments say (read in the scripts).

### Cooldown

- **Set by the scripts**, from `abi_base`'s `Cooldown`: `Ability_SetCooldown(oCaster, nAbility, oItem)` (`core_h.nss`, over the `SetCooldown` command, `kCCCreatureStats_SetCooldown`). The engine reads `Cooldown` only for the interface (`0x0062C9DB`) (read).
- When (read in `ability_core`): on `ABILITY_CAST_IMPACT` for a non-modal ability without a projectile (and for every item ability); on `EVENT_TYPE_ABILITY_PROJECTILE_LAUNCHED` (93) for one with a projectile; a modal ability gets 1.5 s at CAST_START for AI casters only, so the AI does not re-run it.

### The events (engine side), all run with the script `ability_core`

- `ABILITY_CAST_START` (32), from sub-action 8 (`0x007A7440`, read): creator and object 0 the caster, object 2 the command's target, int 0 the ability, plus the fields `0x006A0DA0` adds; run on the caster with the script named `ability_core` (`0x006985F0`, the script given by name, not the ability's `SpellScript`). Before it, sub-action 8 fails the command with −8 when the ability's type is disabled (`0x00A64740` flags) and with −1 when a `Range` 0 ability's target is out of striking reach. After it, with a `CastAnim` it queues sub-action 37 to play it (int 0 the clip, flag 3); with none, and the command's flag 1, it raises `ABILITY_CAST_IMPACT` at once (queued, or for an ability of type 4, item, through the object's slot 200).
- `ABILITY_CAST_IMPACT` (31) from the cast animation's impact (`0x006AF430`, read): queued to the caster (`AddEventDeltaTime`, no delay), the event's target field the target, object 0 the caster, object 2 the target, int 0 and int 1 from the cast record (`0x006A0180`, its +0 and +8), int 2 a counter of impacts, int 3 whether the animation event's name has "Left" (the off hand), vector 0 the target point; script `ability_core`.
- `ABILITY_CAST_IMPACT` (31) from a projectile's arrival (`0x007E71C0`, read): queued to the projectile's owner, object 0 the owner (projectile +0x4C4), object 1 −1, int 0 and int 1 from the cast record, vector 0 the projectile's position, vector 1 its heading (inferred), object 2 the target or objects 2 and up those hit; script `ability_core`. (A weapon's projectile instead gives `ATTACK_IMPACT` (29), `0x007E87C0`, `dev/COMBAT.md`.)
- `EVENT_TYPE_ABILITY_PROJECTILE_LAUNCHED` (93; not in `script.ldf`, `ability_core`'s `case 93`), from the creature's slot 56 (`0x006AFA50`, read): run on the caster with `ability_core`, object 0 the caster, int 0 the ability, vector 0 a point (the projectile's target, inferred); raised when the current command has a projectile in flight (`GetProjectileById`).

### Projectiles

- Made by the creature's slot 61 (`0x006B6F60`, read), for a current command of type 1 (attack) or 25 (use ability); inferred: on the cast animation's launch event. For an ability: `abi_base`'s `Flags` bit 0 set takes the equipped weapon's projectile (`bitm_base` `Projectile`, or the weapon's local override), else `abi_base`'s `Projectile` and `Crust_ID`. With no projectile (0), a use-ability goes straight to its impact (`0x006AF430(0)`). Otherwise `CreateObject` makes the projectile and sets: owner (+0x4BC), `prj_base` row (+0x4A4), crust (+0x4B4), from an ability (+0x4B8), the target point (+0x4E0, the target creature's position unless it is gone, else the event's location) and the target (+0x4C0); it is added to the area (`0x00679EA0`).
- Its flight comes from `prj_base` (table 1006): `InitialSpeed`, `Acceleration`, `MinSpeed`, `TopSpeed`, `Simulation`, `ImpactTimeout`, read at `0x007E8156` and `0x007E6900`–`0x007E6984` (the flight's integration not read). Arcane Bolt (`abi_base` 200254: `Projectile` 137, `Range` 4, `Speed` 1, `Cooldown` 6.0, `Cost` 15.0, `ConjureAnim` 244, `CastAnim` 246, `SpellScript` `spell_singletarget.ncs`): `prj_base` 137 "Arcane Bolt", model `fxa_arc_bolt_p`, `InitialSpeed` 5.0, `Acceleration` 10.0, `MinSpeed` 15.0, `TopSpeed` 40.0 (m/s and m/s², inferred), static collisions 1. Chain Lightning's is 114: 10 initial, 10 acceleration, 20 top.
- The spell's own script (`SpellScript`) is reached by `ability_core` through `Ability_DoRunSpellScript` with its own events (`EventSpellScriptCast`, `EventSpellScriptImpact`), not by the engine (read in the scripts).

## IsModalAbilityActive and Engine_SetModalAbilityGUI share one list

- `IsModalAbilityActive` (726, 0x0066DD80) searches the creature's stats' (+0x7C4 → `CCCreatureStats`) **+0xF8 vector of ability ids**.
- `Engine_SetModalAbilityGUI` (725, 0x00669E80 → 0x0071DA90(ability, status, 0)) writes that same vector. Status 0 erases the id and anything else appends it; then it sets the quickbar button's toggle (0x00719A40).
  - On a real change it may also send event 0x88 (on) or 0x89 (off) to the creature, when a table string is set and an equipment check passes (purpose not read).
- **Its other writer** is a save's load: `SavegameCreatureModalAbilityList` (16468), through 0x0071DA90(id, 1, 1).
- **The scripts call 725** from the upkeep effect: `effect_upkeep_h.nss`:107 on apply (TRUE), :212 on remove (FALSE).
- So a modal ability is "active" exactly while its upkeep effect's handlers say so; the engine reads no effect for it.

Read 2026-10-18 by lib-agent-2 for DAOParagonEngine, not seen in play.
