# Combat as the engine runs it: perception, the attack command, effects

Read 2026-10-18 by lib-agent-2 (three subagents in parallel) in the GOG exe and the toolset's scripts, for DAOParagonEngine's S5 (a fight), at paragon-agent-2's request. What the engine does, and where it hands over to the scripts (`creature_core`, `rules_core`, `combat_h`, `effects_h` and the `effect_*_h` handlers). Each section says what was read in the code and what is inferred. Builds on `dev/TURN_BASED.md` (the approach, the melee ring, the command queue) and `dev/AI.md`. Not yet seen in play as a whole.

## Perception

Read 2026-10-18 in the GOG exe by lib-agent-2 (a subagent), for DAOParagonEngine's S5. "Read" is what the code shows; "inferred" is a reading of its purpose. A creature's perception list is the `std::set` of ids at creature+0xC18 (`IsPerceiving`, `GetPerceivedCreatureList`; `src/dao/Addresses.h`, the melee rings' note).

### When

- `CCCreature::Update` calls the perception update `0x006B89B0` (`__thiscall`, the perceiver) when the creature's +0xBD4 is 0 and the manager at `*(client app+0x114)+0x74` answers true through its slot 0x4C. Read; what that slot paces (every frame or a throttle) is not read.
- The update does nothing for a creature whose slot 0x1D0 answers non-zero (inferred: dead), or which has no area or no area+0x41C.
- A countdown at +0xC38 is lowered by the frame's milliseconds (`[+0x2B8]+8`); at or below 0 the byte +0xC3C is set ("a fresh look"). `0x006A3690` sets the countdown again when a hostile is noticed (a value from `0x00A2CD70(0xB, 1.51)`, read as a game rule, inferred), and the update sets it to 0 when nothing changed. Read.
- In order, each update: (1) raises APPEAR for each id in a pending list at +0xC28 (`0x006AFCF0(other, 1, 0)`), then empties it; (2) checks every id in the perceived set and raises DISAPPEAR for each lost one; (3) unless slot 0x1D0 answers, looks for newcomers (`0x006B8540`). Read. What fills the pending list is not read; the APPEAR it raises has int 2 = 1, which rules_core reads as "already perceived but hostility changed" (inferred: the hostility changes).

### Range, line of sight

- Newcomers (`0x006B8540`): the area's spatial query (`0x00686F70` on area+0x278) with a sphere about the perceiver's position (+0x120) of the perceiver's perception range, the float at +0x68 of the attribute at +0x7C4 (read; where that value comes from, the .utc's PerceptionClass or a table, not read). For the party's controlled creature (the id at +0x1E8 of `0x005D57C0()`'s object) with the party's range at party+0x42C above 0, the query uses that party range and a creature counts as in range when its 3D distance squared is within the creature's own range squared. Read.
- Line of sight, for hostile candidates only: if the two stand in different rooms (+0x150) that `0x00687EC0` does not connect, or `CCArea::LineOfSight` from eye to eye (each position with z + 1.8) fails, the candidate is not seen. Read. Non-hostile candidates in range are perceived with no line-of-sight check. Read.
- Staying perceived (`0x006A8D60`, called with its second argument 1): lost when the other is gone, in another area (+0x254 differs), fails `0x00A285D0` on its +0x41C, answers its slot 0x1D0 (dead, inferred), or its +0x70 interface's slot 0x6C is not 1 (inactive or hidden, inferred); or when the perceiver answers slot 0x1D0; or when the distance (`0x0067A000` between the positions, 3D inferred) is 60.0 m or more. With argument 1 the range is a fixed 60 m and no line of sight is tested, so a creature once perceived stays perceived out to 60 m whatever the perception range (read). A flag from `0x006A44D0` (inferred: stealth) also ends it, and the update then calls `0x006A3690(id, 1)`.

### Who

- Candidates are what the area's query returns; the perceiver itself and anyone already in the set (`0x008704D0`) are skipped. Read. No group filter beyond hostility.
- Hostility: `0x006A2B80(other)` is `IsObjectHostile(perceiver, other)` (when the client app's +0x1B0 is set). Read.
- A hostile candidate whose +0x7F0 is non-zero (stealthed, inferred from rules_core's "bStealthed") is not added to the set: when in range and in sight it restarts the countdown (`0x006A3690`) and an APPEAR is raised only when +0xC3C is set (or for the controlled creature without party flag 0x10, read). The stealth detection term (the candidate's +0xB94, a rounded value against party+0x414+0x14, and a class 7 with party flag 0x20) is read but not understood.
- Otherwise a candidate in range is added to the set and APPEAR raised at once (`0x006B3240`: `0x006B1120` inserts the id, then `0x006AFCF0(other, 0, 0)`); one in the party's range but not the creature's own is added with no event. Read.

### The events

- APPEAR (`0x006AFCF0`, `__thiscall (perceiver; other, int2, int3)`): type 25; target field (+0x28) the perceived; int 0 hostile (`IsObjectHostile`), int 1 the perceived's +0x7F0 (stealthed), int 2 and int 3 the arguments (int 2 = 1 from the pending list: "hostility changed"), object 0 the perceived. Queued to the perceiver: `AIMaster::AddEventDeltaTime(0, 0, perceiver, perceiver, 10, event)`, no delay. Read. rules_core reads int 0 nHostile, int 1 bStealthed, int 2 nHostilityChanged, object 0 oAppear.
- DISAPPEAR (in `0x006B89B0`): type 26; target field the lost one; object 0 the lost one; flag 0 hostile; queued to the perceiver the same way. Read.
- Neither is raised when the gate `0x00698490(type)` answers false (it looks the type up with `0x005FE960`, as the module's load does before queuing MODULE_LOAD; inferred: a disabled or already-pending event type), nor, unless `0x006A2CE0()` answers true, when either creature's +0xBD4 is 1. Read.
- The set changes in the same update as the event is queued: the insert just before APPEAR, the erase (and the melee rings' `0x007CBC50`) just after DISAPPEAR, so a script handling the event already sees the new list. Read.

## The attack command

Read 2026-10-18 in the GOG exe and the toolset's scripts (`rules_core.nss`, `combat_h.nss`, `effect_damage_h.nss`, `effect_death_h.nss`, `events_h.nss`), building on what `dev/TURN_BASED.md`, `src/dao/Animation.cpp` and `src/dao/Addresses.h` already record. "Read" means seen in the code; "inferred" is marked.

**One command is one swing.** `CommandAttack` (566, `0x0066e710`) makes a `COMMAND_TYPE_ATTACK` (1). Its step (`0x00742FD0`) queues an approach and the attack, sub-action 50, which queues a turn to face (39) and one swing (1, `0x007A87C0`) for melee, or the shot's steps for ranged (`dev/TURN_BASED.md`, `src/dao/Animation.cpp`). When the swing's wait ends, the command finishes and `COMMAND_COMPLETE` is sent; the AI then chooses again (`rules_core` → `AI_DetermineCombatRound`). It does not repeat: the scripts queue the next attack (read in the structure; the one-swing rhythm also seen in play, `dev/TURN_BASED.md`: "a creature re-decides after each attack").

**The approach** is `dev/TURN_BASED.md`'s "How close a melee attack strikes from": melee by the melee ring (sub-action 4; reach = both `PerSpace` + 1.2 m, and within 1.25 m of the ring place), ranged by the weapon's `bitm_base` `Range` (sub-action 15, capped at 60 m, with line of sight). Weapons add nothing to melee reach. `dao/Attack.h` calls these.

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

## Effects

Read 2026-10-18 in the GOG exe and the toolset's scripts by lib-agent-2. "Read" means seen in the code; "inferred" means not followed to the end. DAOLib already has an effect's ageing (`Addresses.h`, `kEffectsHolder_AgeEffect` and kin; `dao/Effects.h`, `dao/Clock.h`) and the CDataArrays parameters an effect and an event share.

### Applying an effect

- `Engine_ApplyEffectOnObject` (command 597, `0x0065ca20`) pops the duration type (int), the effect (engine structure 3), the target (object), the duration (float), the creator (object) and, with more than five arguments, an ability id (int). Read.
- Only creatures, placeables, items and areas of effect take effects: object types 4, 6, 10, 12 by the object's type byte at +0x14. Any other target, or none, deletes the effect and returns. Read.
- It then calls `CGameEffect::Prepare` (DAOLib's `kCGameEffect_Prepare`, ID 13143) with the duration type, duration, creator and ability. For effect type 7 (`EFFECT_TYPE_DEATH`) on a creature it first calls the target's slot 0x16C(0) when its slot 0x168 says so. Then `0x007a1a20` hands the effect to the effects manager (client app +0x1AC, its +0x68). Read.
- `0x007a1920` → `0x007a0e30` applies it. Effect types 1–11 (the engine's own: `EFFECT_TYPE_VISUAL_EFFECT` 4, `AOE` 5, `CAMERA_SHAKE` 6, `DEATH` 7, `PARALYZE` 8, `MOVEMENT_RATE` 9, `ALPHA` 10, `MOVEMENT_RATE_DEBUFF` 11) go to a native handler from a table on the manager, indexed by type, when the target's slot 0x6C allows. Their answer is the result, and no APPLY_EFFECT event is raised on that path. Read; what each native handler does is not read.
- Every other type (the scripted ones, 1001 and up, `EFFECT_TYPE_DAMAGE` 1013 among them) goes to `0x0079fe90`. While the target's effects holder is enabled (its byte +4), that builds a `CScriptEvent` of type 33 (`EVENT_TYPE_APPLY_EFFECT`), puts the effect in the event as its effect 0 (`0x0069d120(0, effect)`), and passes it to the target's slot 0x28. That slot is `CCObject::RunEventScript`, so the target's own event script runs it at once: `creature_core`, `placeable_core` and so on. It isn't queued, and it isn't a per-effect-type script. Read.
- In the scripts, `creature_core` hands unhandled events to `rules_core`. Its `EVENT_TYPE_APPLY_EFFECT` case calls `Effects_HandleApplyEffect` (`effects_h.nss`), which reads `GetCurrentEffect()`, switches on `GetEffectType` to the `effect_*_h` handlers, and ends with `SetIsCurrentEffectValid(nReturnValue)`. A dead target refuses everything except resurrection and death.
- After the script, `0x007a0e30` checks the effect's invalid word (+0x48). If the script marked it invalid, the apply returns 3 and the effect is not added: `0x007a1920` sets +0x48 to 1. If still valid, the holder's slot 0x1C adds it to the target's list, and slots 0x70 and 0x34 follow. So the script runs first and the list changes after, only for a valid effect. Read in outline; the slots' bodies are not read.
- `0x0079fc10`, after the event: when the effect's int 2 is above 0 and the effect is still valid, it plays that visual effect (int 2 is `nImpactVfx` for damage). Read.

### The current effect

- `GetCurrentEffect` (681, `0x006604d0`) and `SetIsCurrentEffectValid` (682, `0x00660580`) use the event the calling object is running: an object's +0x2A8 through its slot 0x18, an area's +0x390, or the module's +0x3C4. If that event is type 33 or 34, its effect 0 is the current effect. Otherwise `GetCurrentEffect` returns no effect and `SetIsCurrentEffectValid` does nothing. Read.
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
- Death at 0 health is decided by the same script: when the hit is fatal and the target isn't plot or immortal, `KillCreature` (`effect_death_h`) builds `EffectDeath()` (type 7; int 0 force, int 1 the damage, creator the killer) and applies it with `EFFECT_DURATION_TYPE_DEATH`, or handles it inline.
- Type 7 is one of the engine's native types, so the engine's handler takes it (not read). The script side, `Effects_HandleApplyEffectDeath`, sends `EVENT_TYPE_DEATH` with `SendEventOnDeath(target, killer)`. Read in the scripts; how the native type-7 handler and the script handler share the work is not read.
