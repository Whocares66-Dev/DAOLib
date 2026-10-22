# Animation: drawing and sheathing, and a script's animation command

How the engine plays a creature's animations for two cases read in the exe: a weapon drawn and sheathed, from the combat state to the clip's events that move the weapon, and `CommandPlayAnimation`, with its loops, the commands queued behind it and an animation that is missing. Both read 2026-10-18 by lib-agent-2. The animation trees and clips as files are `src/dao/core`'s Anb and Clip; which tree a creature's weapon style gives is `dev/ITEMS.md`, "The weapon style".

## Drawing and sheathing weapons (DAOParagonEngine #33)

Read 2026-10-18 by lib-agent-2 (and two of its forks) in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py --disasm`, scratch call and xref scans and capstone; the tables and clips through ghilana (core copies, `2da.erf`, `global.rim`); the scripts in `toolset\extract\scripts\`. No Ghidra, no game. "Checked" = read in the disassembly, the files or the scripts; "inferred" = a reading of purpose not followed to the end. Every address is GOG's and has an ID in the address library (`python tools\address_ids.py 0x...`).

### Summary

- **The engine draws and sheathes on the combat state, natively.** `CCCreature::SetCombatState` (0x006AE0A0) queues command type 39 (Sheathing) at the back of the queue, draw on entering combat with a delay of 0-99 ms, sheathe on leaving it with 1,000-2,999 ms. The scripts only set the state, and they do it for every creature: hostiles per creature, the whole party (controlled or not) through the game mode. The attack command also draws by itself before its swing if the weapons are still sheathed, and moves draw or sheathe with an upper-body overlay.
- **The row is fixed and the clip comes from the creature's combat set.** anim_combat row 100 `enter` draws and 101 `exit` sheathes; 177 `quick_enter` and 178 `quick_exit` are used while the combat state is set, so a combat-start draw plays `quick_enter`. The node is looked up in the creature's trees, where the weapon set's tree (OW/WS/TW/DW.anb by equipment; Bow/Cbow/Staff.anb from bitm_base's `WieldModel`, confirmed 2026-10-18 by lib-agent-2: 0x006C1812 calls 0x006B78D0, which reads `WieldModel` and `WieldModelOverlay`, hashes checked) supplies it. With no such node the drawn flags flip at once and `unarmed_enter`/`unarmed_exit` (110/111) plays if the creature has it.
- **The weapon moves at the clip's events,** in seconds in the clip's `.evt`: events.gda 3 EquipR, 4 EquipL, 5 UnequipR, 6 UnequipL. Each moves one hand's item between its `Wear` and `Wield` node and sets that hand's drawn flag. On a draw, the creature's combat trees are also swapped to the set's, which is the drawn stance and grip. No end-of-clip fallback was found: a clip without the event never moves the weapon.

### 1. What queues the draw and the sheathe

#### The combat state (checked)

`CCCreature::SetCombatState(int state)` 0x006AE0A0, `__thiscall`, `ret 4`. The creature's combat data is `[+0xC60]`, and its +0 is the combat state. If the new state equals the stored one, nothing happens. Otherwise it stores the state, then:

- for state 1 it calls **0x006ABA40**, entering combat;
- for anything else it calls **0x006ABBF0**, leaving combat;
- it calls vtable slot 0x154/4 and the +0x624 sub-object's slot 0x54 with (state == 1). Inferred: the UI and observers; not read.

The callers (all three E8 calls, checked):

| Caller | What it is |
|---|---|
| 0x0065DDBF | `ScriptCommand::SETCOMBATSTATE` (525) 0x0065DD60, the int passed unchanged |
| 0x006D4D75 | the client VM's twin of the same command (0x006D4D20, in `CClientVirtualMachineCommands`' table at +0xBC), passing (n == 1) |
| 0x006B4597 | a reset in CCCreature's constructor, `SetCombatState(0)`: a no-op, since the state starts at 0 |

So **the scripts are the only source of a change** (checked).

#### Entering combat, 0x006ABA40 (checked)

1. **Not a humanoid** (`ScriptBackend::ForScript_IsHumanoid` 0x006A25A0 false):
   - With no current command (0x0073C600 null), it queues a **type 14 PlayAnimation of row 100 `enter`** (int 0 = 100, int 3 = 1) through `QueueNewCommand` (0x0073D000) with addToFront 1, static 1, playerIssued 0, override -1 and word 1.
   - With a current command, it only calls 0x006A7F30(1).
   - No weapon moves on either path: a non-humanoid has no draw.
2. **Humanoid, weapons already drawn** (0x007C34C0 true): it calls 0x006A7F30(1) and stops.
3. **Humanoid, sheathed, no current command:** it queues **type 39** with:
   - int 0 = 1 (draw);
   - int 1 = `max([0x00C353B8], 0) + rand() % ([0x00C021F0] - [0x00C353B8])`. `[0x00C353B8]` is zero-initialised .bss and nothing else references it; `[0x00C021F0]` = 100. So **0-99 ms** (checked);
   - `QueueNewCommand(39, params, addToFront 0, static 0, playerIssued 0, override 0, 0)`: the **back** of the queue.

   It then calls 0x006A7F30(1).
4. **Humanoid, sheathed, busy** (a current command):
   - If it is moving (`+0xB20`, `IsMoving` in `dev/script_fields.csv`), and animations 100 `enter` and 109 `unequip` are both at weight 0 (0x00694E20: the animation set `[+0x154]`'s value for the row > 0), it plays **row 108 `equip`** on the animation set directly, with no command: 0x00696B50(108, 0, 3.0f). That is the `*Overlay.anb` node, the upper body drawing while the legs walk (inferred from the trees).
   - In every busy case it then calls 0x006A7F30(1).
   - **No type 39 is queued while busy.** The draw then comes from the attack (below), or not at all until the creature is idle (inferred).

0x006A7F30(bool combat) switches the creature's animation trees between explore and combat:
- It reads three of apr_base's row strings (+0xC, +0x28, +0x7C) and calls the animation set at a 3.0 blend.
- It moves no weapon: it calls neither 0x006A64B0 nor the attach.
- This is inferred from what it reads; not read to the end.

#### Leaving combat, 0x006ABBF0 (checked)

It first sets combat data +4 = -1 (checked: the attack target, which `GetAttackTarget` 0x00655000 reads at 0x00655063; script_commands/spells_combat.md, "Who writes it"). Then:

1. **Not a humanoid:**
   - With no current command, it queues **type 14 PlayAnimation of row 101 `exit`**, front and static as above.
   - Otherwise it calls 0x006A7F30(0).
2. **Humanoid, combat data +0xA set:** nothing happens and the weapons stay drawn. +0xA is the template's `WeaponsDrawn` field, read at load by 0x006C04C0 (the one writer a scan of all `[[+0xC60]+0xA]` byte writes found).
3. **Humanoid, already sheathed:** it calls 0x006A7F30(0).
4. **Humanoid, drawn, moving** (`+0xB20` ≠ 0):
   - If 101 `exit` and 108 `equip` are both at weight 0, it plays **row 109 `unequip`** as the overlay, 0x00696B50(109, 0, 3.0f).
   - It returns without calling 0x006A7F30(0) (checked; the trees then go back at the UnequipR event, below).
5. **Humanoid, drawn, standing:** it queues **type 39** with:
   - int 0 = 0 (sheathe);
   - int 1 = 1000 + rand() % (3000 - 1000) from `[0x00C021F4]` = 1000 and `[0x00C021F8]` = 3000: **1,000-2,999 ms**;
   - the back of the queue, not static.

   Unlike the draw, **this is queued even with a current command**: it waits behind it.

#### The Sheathing command, type 39 (checked)

- **Start** 0x007429F0: sub-action 36 (wait, int 0 = the command's int 1, the delay), then sub-action 48 with the parameters.
- **Sub-action 36** (0x007A20C0): the sub-action's duration `+0x14` against its elapsed time `+0xC`. A delay of 0 passes at once. The unit is milliseconds, inferred from the other timed sub-actions in `dev/TURN_BASED.md`.
- **Sub-action 48** (0x007A26E0) runs these steps in order:
  1. It is done at once if flag 0 of the parameters is 1 and the combat state is 1. No caller found sets flag 0.
  2. It picks the row:
     - int 0 = 0 → 101 `exit`; 1 → 100 `enter`; anything else toggles from the drawn state.
     - **While the combat state ([[+0xC60]+0]) is not 0,** 100 becomes **177 `quick_enter`** and 101 becomes **178 `quick_exit`**. So the draw on entering combat plays `quick_enter` (the state is set before the command runs) and the sheathe on leaving plays `exit`.
  3. It is done at once in these cases:
     - a draw when the weapons are already drawn, or when 108 `equip` has weight > 0 (the overlay is already drawing);
     - a sheathe when they are already sheathed, or when 109 `unequip` has weight > 0.

     The weights come from (+0x70)->vfunc 0x60, then `0x00A5F810(row, 0)`.
  4. **If the creature has the row** (`CCCreature::HasAnimation` 0x00694E70: a lookup of the row's id in each of the animation set's trees, 0x00A5E390; no Replacement walk in it), it adds:
     - sub-action 37 with int 0 = the row and int 3 = 1 (`AddSubAction(37, p, 1, 1, 0)`; 37 = `PlayAnimation`, 0x007A4390);
     - sub-action 5 with flag 0 = 1 (`AddSubAction(5, p, 0, 1, 0)`). Sub-action 5 (0x007A2110) with flag 0 = 1 calls 0x006B5D00, which drops the temporary trees listed at `+0xCA0` (0x006B5B30(2, name, 0) for each, then clears the list). **It moves no weapon.**
  5. **If it does not have the row:**
     - it sets both hands' drawn flags at once to the new state (0x007C33D0(new, 0));
     - then plays row 110 `unarmed_enter` (for a draw) or 111 `unarmed_exit` (for a sheathe) as sub-action 37, if the creature has that row;
     - otherwise it calls 0x006A7F30(new).

#### Other engine paths (checked unless marked)

- **The attack draws by itself.**
  - The attack's start (0x00742FD0) adds sub-action 4 (the approach) and sub-action 50 with int 0 = the command's type byte (1).
  - Sub-action 50 (0x007A1E40) calls the second handler table (CSSubActionHandler +0x4C) at that index: entry 1 is **0x007A6150**, the attack's preparation.
  - If the target exists and is not dead (0x00695300 = 0), it calls 0x007A4F90(creature, mode 1 = draw, overlay 0).
  - When sheathed, that adds **sub-action 48 (int 0 = 1)** to the attack command itself (`AddSubAction(48, p, 1, 1, 0)`): a full-body draw before the swing, with the same row choice as above (so `quick_enter` in combat).
  - Entry 25 (use ability, 0x007A83F0) does not call it.
- **Moves draw and sheathe with the overlay.**
  - 0x007A4F90 is also called by the move and approach sub-actions: 4 (0x007AA560), 7 (0x007A66A0), 8 (0x007A7440), 14 (0x007A99B0), 15 (0x007AA200) and 16 (0x007A8C20).
  - Each passes a mode from its own parameters (int 6 or int 2: 0 none, 1 draw, 2 sheathe) and overlay 1 (8 passes overlay 0).
  - With overlay set, a creature in the wrong state plays 108 `equip` or 109 `unequip` through 0x00696B50(row, 0, 3.0f) if it has the row; otherwise it calls 0x006A7F30(state).
  - Which move commands set that int is not read.
- **The idle re-check** 0x006AB6E0 (from CCCreature's vtable slot 87, inferred: the per-tick update) calls 0x006A8580(0, 0) when all of these hold:
  - the creature is out of combat (0x006A2580: [[+0xC60]] ≠ 1);
  - it has no current command;
  - a countdown at `+0x7E0` has run out.

  0x006A8580(instant, force) compares the drawn state with `WeaponsDrawn` (+0xA). If they differ, it queues type 39 at the back with int 0 = 1 if `WeaponsDrawn`, else 2 (toggle), and no delay. So an idle creature out of combat is brought back to its template's state.
- **At load and on a look change** 0x006A8580(1, …) is instant:
  - the creature's load (0x006C04C0, after reading `WeaponsDrawn`) calls (1, 0);
  - `ForScript_SetItemMaterialType` (0x00756540) calls (1, 1).

  With `WeaponsDrawn` set, it calls 0x006A7F30(1) and fires `HandleEquipAnimationEvent(3)` and `(4)` directly: drawn with no clip. An instant sheathe does nothing there.
- **Switching weapon sets** while drawn adds sub-action 48 with int 0 = 1 (`dev/script_commands/commands.md`, CommandSwitchWeaponSet).

#### The scripts (checked; paths under `toolset\extract\scripts\`)

**Combat state 1:**
- **Non-party creatures:** `_Core Scripts/rules_core.nss`:940, on `EVENT_TYPE_PERCEPTION_APPEAR`. The creature perceives a hostile, its state is 0 and it is not a follower: `SetCombatState(OBJECT_SELF, TRUE)`, then `WR_ClearAllCommands`.
- **The party, through the game mode:**
  - rules_core:1113 (a member in GM_EXPLORE or GM_DEAD perceives a hostile) and rules_core:643 (a follower attacked in explore) call `WR_SetGameMode(GM_COMBAT)`.
  - `Core Scripts/module_core.nss`:246, on `EVENT_TYPE_GAMEMODE_CHANGE`, calls `SetCombatStateParty(new == GM_COMBAT)`. That is `_Core Includes/core_h.nss`:1381-1398: **every member of `GetPartyList(GetHero())`, controlled or not**, where the state differs.
- Also rules_core:1123 (a party member resurrected in combat) and `wrappers_h.nss`:356 (a follower made active while the party perceives hostiles).

**Combat state 0:**
- **Non-party creatures:** `combat_h.nss`:1337, `Combat_HandleCreatureDisappear` on PERCEPTION_DISAPPEAR, when no hostile is perceived.
- **The party:** combat_h:1325 delays `EVENT_TYPE_DELAYED_GM_CHANGE` 3.0 s. Then module_core:310-352 sets GM_EXPLORE (retrying every 1-2 s until `CheckResurrection()`), and the mode change sets the party's state to 0.
- Also a follower's death (`player_core.nss`:418) and a roamer back home (rules_core:743).

**Scripts that queue a sheathe themselves:** only `placeable_h.nss`:676 (`CommandWait(2.0)` then `CommandSheatheWeapons()` for the killer of a placeable in explore) and `proving_core.nss`:221 (on leaving the Proving). No script calls `CommandUnsheatheWeapons`, and no AI script draws.

So, for Paragon: party, controlled or not, and hostiles alike draw through the same engine path when their state turns 1. The party's state follows the game mode.

### 2. Which rows and which clip

#### The rows (checked: anim_combat.gda; anim_base.gda has no rows 100-240)

| Row | AnimName | Type | Replacement | Used for |
|---|---|---|---|---|
| 100 | `enter` | 16 | 110 | draw, out of combat (scripts' Unsheathe, the idle re-check) |
| 101 | `exit` | 32 | 111 | sheathe, out of combat (leaving combat: the state is already 0) |
| 177 | `quick_enter` | 16 | 100 | draw while the combat state is set (entering combat, the attack's draw) |
| 178 | `quick_exit` | 32 | 101 | sheathe while the combat state is set (a script sheathe in combat) |
| 108 | `equip` | 16 | 1 | the moving draw, an overlay with no command |
| 109 | `unequip` | 32 | 1 | the moving sheathe, an overlay with no command |
| 110 | `unarmed_enter` | 16 | 1 | played after an instant flag change when the creature lacks the row |
| 111 | `unarmed_exit` | 32 | 1 | likewise for a sheathe |

**The row does not depend on the weapon style; the tree does.** No table column is involved. The engine asks for the row, and the creature's trees give the clip.

#### The combat set (checked)

0x007C3500 picks the set from the equipment (`+0xBE0`). `[item+0x6E0]` is bitm_base's `Type` (inferred from the values: 2 melee, 3 shield). The tests run in this order:
1. A melee main hand whose `EquippableSlots` is 1 (main hand only) gives **TW**.
2. Any other melee main hand gives **OW** with nothing in the off hand, **WS** with a shield (Type 3) and **DW** with a melee off-hand.
3. Anything else (no main-hand weapon, a bow or crossbow (Type 6), a staff (Type 5), or a melee main hand with some other off-hand item) gives the **none** record (id -1, empty names).

The four records are static (0x00C38C40 OW id 0, 0x00C38C7C WS id 1, 0x00C38CB8 TW id 2, 0x00C38CF4 DW id 3; the none record 0x00C38C04, id -1), each with its tree and overlay tree (`OW.anb`/`OWOverlay.anb` and so on, wide strings at 0x00B225DC…).

When the set changes (0x006B8D40), the old set's trees are dropped and the new set's are added through 0x006B5B30 (kind 2 drops, kind 0 adds; inferred from that use). The style for `GetWeaponStyle` is set at the same time (0x007C3600).

For the none set, 0x006A56B0 takes the main-hand item's bitm_base `WieldModel`/`WieldModelOverlay` in its place (checked):

| bitm_base | WieldModel | WieldModelOverlay |
|---|---|---|
| Shortbow 19, Longbow 20, 53, 43 | `Bow.anb` | `BowOverlay.anb` |
| Crossbow 21 | `Cbow.anb` | `CbowOverlay.anb` |
| Staff 16 | `Staff.anb` | `StaffOverlay.anb` |

**Where a bow or staff tree enters the creature before its first draw** (so that `HasAnimation(100)` finds `enter`) was not read. The other callers of 0x006B5B30 in the equip code (0x006B795C, 0x006B79E6) are the likely place.

#### The clips (checked, the data fork)

apr_base gives every humanoid (rows 1, 2, 3, 15 and the ambient, fat and child rows) the same base trees, and none of them has these nodes:

| apr_base column | Tree |
|---|---|
| ModelTree | `humanoid.anb` |
| CombatModelTree | `combat_humanoid.anb` |
| MovementTree | `movement.anb` |
| CombatMoveTree | `combat_movement.anb` |
| OverlayTree | `humanoid_overlay.anb` |

`combat_humanoid.anb` has only `unarmed_enter` = `mh_c.ua_ent` and `unarmed_exit` = `mh_c.ua_ext`. Every node below has flags 1 (plays once) and weight 1. There are no female copies (the only `_fem` tree is `movement_fem.anb`), so females play the `mh_c` clips.

| Set tree | enter 100 | exit 101 | quick_enter 177 | quick_exit 178 | Overlay tree: equip 108 / unequip 109 |
|---|---|---|---|---|---|
| OW | mh_c.ow_ent | mh_c.ow_ext | mh_c.ow_quick_ent | mh_c.ow_quick_ext | OWOverlay: mh_c.ow_o_ent / mh_c.ow_o_ext |
| WS | mh_c.sw_ent | mh_c.sw_ext | mh_c.sw_quick_ent | mh_c.sw_quick_ext | WSOverlay: mh_c.sw_o_ent / mh_c.sw_o_ext |
| TW | mh_c.tw_ent | mh_c.tw_ext | mh_c.tw_quick_ent | mh_c.tw_quick_ext | TWOverlay: mh_c.ow_o_ent / mh_c.ow_o_ext (OW's) |
| DW | mh_c.dw_ent | mh_c.dw_ext | mh_c.dw_quick_ent | mh_c.dw_quick_ext | DWOverlay: mh_c.dw_o_ent / mh_c.dw_o_ext |
| Bow | mh_c.bw_ent | mh_c.bw_ext | mh_c.bw_quick_ent | mh_c.bw_quick_ext | BowOverlay: mh_c.bw_o_ent / mh_c.bw_o_ext |
| Cbow | mh_c.cbw_ent | mh_c.cbw_ext | mh_c.cbw_quick_ent | mh_c.cbw_quick_ext | CbowOverlay: mh_c.cbw_o_ent / mh_c.cbw_o_ext |
| Staff | mh_c.stf_ent | mh_c.stf_ext | mh_c.stf_quick_ent | mh_c.stf_quick_ext | StaffOverlay: mh_c.stf_o_ent / mh_c.stf_o_ext |

#### With no clip (checked, sub-action 48)

`HasAnimation` checks the row itself (100/101 or 177/178), not its Replacement. If the creature lacks it, then in order:
1. Both hands' drawn flags change at once.
2. Row 110 or 111 (`mh_c.ua_ent` 0.9 s or `mh_c.ua_ext` 1.233 s, from `combat_humanoid.anb`) plays as sub-action 37, if the creature has it.
3. Otherwise there is no animation and 0x006A7F30(new) switches the trees at once.

**No weapon model is moved on this path** (it calls neither 0x006A64B0 nor the attach). That is harmless for the creatures that take it: an unarmed humanoid, with nothing to move (inferred). No fixed delay is used, other than the command's own int 1 wait.

### 3. When the weapon moves

#### The events (checked: events.gda, patch003's copy)

| ID | EventName | EquipEvent | UnequipEvent | RightHand |
|---|---|---|---|---|
| 3 | EquipR | 1 | 0 | 1 |
| 4 | EquipL | 1 | 0 | 0 |
| 5 | UnequipR | 0 | 1 | 1 |
| 6 | UnequipL | 0 | 1 | 0 |

Each clip has three files: `.ani` (the keys), **`.evt`** and `.gad`.
- **`.evt`** is GFF4 "EVT " V1.0, holding `AnimationEventList` (one struct per event):

  | Field | Name | Type | Meaning |
  |---|---|---|---|
  | 4016 | `AnimationEventTime` | float64 | seconds from the clip's start, not a share |
  | 4017 | `AnimationEventID` | int | the events.gda row |
  | 4018 | `AnimationEventTarget` | int | 1 in every file read |
  | 4019 | `AnimationEventString` | string | |

  Read 2026-10-18 over all 3,161 `.evt` the game takes (DAOLib #190, `clip::EventsOf`, creatures' animations included): every event's target is 1, a file's events are not in time order (44 come before the one ahead of them), and every id has a row of events.gda (table 95, "Animation Events": ID, Label, EventName, EquipEvent, UnequipEvent, RightHand, IsAudio, `clip::EventKindOf`) but one, id 15 in `c_dog.pauseturnr180cwalkf`. The commonest: 14 FootDown (7,528), 17 MovementSound (2,520), 16 DetonateEmitter (2,397), 20 Playsound (1,192), 7 AbilityCast (282), 19 Swish (221), 18 Bodyfall (158), 1 AttackImpact (126), 30 DeathblowDamage (73), 27 PlaceableSound (65), 33 GameSpeed (54), 9 ProjectileLoad (42). Never raised: 12 BlendEnd, 39 HideNode, 40 ShowNode. `EquipEvent`, `UnequipEvent` and `RightHand` are set for 3 to 6 alone, as the table above, and `IsAudio` for 17 to 21, 23, 24 and 27.

- **`.gad`** is GFF4 "GAD " V0.1. Its `AnimationAnimLength` (4009) is the clip's length in seconds.

#### Times (checked; seconds, share of the clip in brackets)

| Clip | Length | Equip/unequip events |
|---|---|---|
| ow_ent | 1.133 | EquipR 0.63 (0.56) |
| ow_ext | 1.133 | UnequipR 0.47 (0.41) |
| ow_quick_ent / ow_quick_ext | 1.133 | EquipR 0.63 / UnequipR 0.47 |
| sw_ent | 1.133 | EquipL 0.30 (0.26), EquipR 0.63 (0.56) |
| sw_ext | 1.133 | UnequipR 0.47 (0.41), UnequipL 0.83 (0.73) |
| sw_quick_ent / sw_quick_ext | 1.133 | EquipL 0.30, EquipR 0.63 / UnequipR 0.47, UnequipL 0.83 |
| tw_ent / tw_ext | 1.133 | EquipR 0.63 (0.56) / UnequipR 0.43 (0.38) |
| tw_quick_ent / tw_quick_ext | 1.133 | EquipR 0.60 / UnequipR 0.30 |
| dw_ent | 1.133 | EquipL 0.30, EquipR 0.63 |
| dw_ext | 1.133 | UnequipR 0.50, UnequipL 0.80 |
| dw_quick_ent / dw_quick_ext | 1.133 | EquipL 0.40, EquipR 0.63 / UnequipR 0.50, UnequipL 0.73 |
| bw_ent / bw_ext | 0.967 | EquipR 0.33 (0.34) / UnequipR 0.63 (0.65) |
| bw_quick_ent / bw_quick_ext | 0.967 | EquipR 0.50 / UnequipR 0.50 |
| cbw_ent / cbw_ext | 1.067 | EquipR 0.30 / UnequipR 0.80 |
| cbw_quick_ent / cbw_quick_ext | 1.067 | EquipR 0.37 / UnequipR 0.67 |
| stf_ent / stf_ext | 0.8 | EquipR 0.27 / UnequipR 0.53 |
| stf_quick_ent | 0.633 | EquipR 0.20 |
| stf_quick_ext | 0.633 | **UnequipL** 0.43: a data fault, the staff is a right-hand item |
| ow_o_ent / ow_o_ext (overlays) | 1.133 | EquipR 0.63 / UnequipR 0.47 |
| sw_o_ent / sw_o_ext | 1.133 | EquipL 0.30, EquipR 0.63 / UnequipR 0.47, UnequipL 0.83 |
| dw_o_ent / dw_o_ext | 1.133 | EquipL 0.40, EquipR 0.63 / UnequipR 0.50, UnequipL 0.73 |
| bw_o_ent / bw_o_ext | 0.967 | EquipR 0.57 / UnequipR 0.50 |
| stf_o_ent / stf_o_ext | 0.633 | EquipR 0.20 / UnequipR 0.43 |
| ua_ent / ua_ext | 0.9 / 1.233 | **none** (FootDown and MovementSound only) |

The other events in these clips are FootDown (14), MovementSound (17) and BlendStart (11). None of them has Done (13). On a draw the left hand comes first (shield or off-hand, at 0.30-0.40 s) and the right at 0.63 s; on a sheathe the right goes first.

#### What the event does (checked)

`CCCreature::OnAnimationEvent` 0x006C0BF0 switches on the event id: ids 3-6 go to 0x006AAC00, which calls **`HandleEquipAnimationEvent(id)`** 0x006A64B0.

1. **The hand.** It reads events.gda (table 95) `EquipEvent` and `RightHand` for the id. The slot is 3 (right) if `RightHand`, else 4 (left).
2. **The node.** It takes the item in that slot (0x00747CF0) and its bitm_base `WearR`/`WearL`/`WieldR`/`WieldL`:
   - an equip event takes `WieldR` or `WieldL`;
   - an unequip takes `WearR` or `WearL`.
3. **The attach.** 0x006A1350(animation set, appearance, slot, node) re-hangs the model on that row of `weaponparts.gda` (`AnimBaseWield::AttachWeaponToNode` 0x00A66B70; `dev/CHARACTERS.md`, "Where a weapon hangs": e.g. Longsword Wear 5 `Prop10` upper right back, Wield 14 `Prop01` right hand; a shield Wear 8 `Prop11`, Wield 16 `Prop03`).
4. **The flags and trees.** 0x006A56B0(isEquip = id is 3 or 4, id):
   - It sets the hand's drawn flag in the combat data: +8 right for 3/5, +9 left for 4/6 (0x007C33D0).
   - On an equip it puts the set's tree (or the weapon's `WieldModel`) in place of the apr_base row's string at +0x28, and the set's overlay tree (or `WieldModelOverlay`) in place of the string at +0x44 (0x00A60980(new, old) on the animation set). An unequip swaps them back.
   - Inferred: +0x28 is CombatModelTree and +0x44 OverlayTree, by the trees they trade places with.
5. Then it calls 0x006A0910(1) (not read).

**"Drawn"** (0x007C34C0) is +8 (the right hand) for every set but WS, where it is +8 and +9 (both hands). So a WS creature counts as drawn only after both events, and as sheathed after the first.

#### With no event, or no clip

- **No end-of-clip fallback was found** (checked for this path). Sub-action 5, the one that follows the clip, calls only 0x006B5D00 (dropping temporary trees), and sub-action 48 queues nothing else. A clip that lacks the event leaves the weapon where it was and the hand's flag unchanged.
- The data have one such case: `mh_c.stf_quick_ext` fires UnequipL. That moves no item (the left hand is empty) and clears +9, while +8 stays set, so a staff stays drawn in hand (inferred). It plays only for a staff sheathed while the combat state is set (a script sheathe in combat).
- **With no clip at all** (no node), the flags flip at once (above) and nothing re-hangs the model on that path (checked).
- The instant moves that do exist call the event handler directly with no clip:
  - load with `WeaponsDrawn`, and a look change: 0x006A8580(1, …) fires events 3 and 4;
  - a function at 0x007F2460 fires 5 and 6 and then 0x006A7F30(0) (not identified; inferred: a reset to sheathed);
  - 0x006A6640 re-fires 3 and 4 for a drawn creature, from 0x006C16E0 and 0x006C19F0 (inferred: after the model is rebuilt).

### 4. The grip

There is no separate grip or hand pose. The drawn look comes from the trees:
- At the equip event, CombatModelTree is swapped for the set's tree (OW/WS/TW/DW.anb, or the weapon's `WieldModel`), and the overlay tree for the set's `*Overlay.anb`.
- So the drawn idle, the ready stance and the hands come from that tree's nodes. They go back to `combat_humanoid.anb` (the unarmed stance) at the unequip event.

This is checked in 0x006A56B0's calls. What each swapped tree's `pause`/ready nodes hold, and how the apr_base strings at +0x28 and +0x44 map to columns, were not read.

### Not read

- 0x006A7F30 to the end (the explore/combat tree switch), 0x00A60980 and 0x006B5B30 beyond their uses here.
- `AddSubAction`'s third argument (1 for sub-action 37 and 48, 0 for sub-action 5): whether it means the front.
- Which move commands carry the draw mode in int 6 or int 2.
- How a bow's or staff's `WieldModel` tree is in place before the first draw.
- Who sets flag 0 of a Sheathing command.
- 0x006A0910 and 0x006A1350's internals.
- Nothing here was seen in play.

## CommandPlayAnimation (569): looping, queued commands behind it, and a missing animation

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, the toolset's scripts and the tables through ghilana, for DAOParagonEngine. Not seen in play. "(checked)" means read in the disassembly or the data; "(inferred)" means not. The shared command run path (StepCommands, start/end tables, the shared end 0x00746900, COMMAND_COMPLETE) is `dev/script_commands/commands.md`; the clear rules (static, the sub-action's byte +4) are `dev/TURN_BASED.md`, "A soft ClearAllCommands mid-swing".

### Short answers

1. **nLoops = -1 never holds the queue for ever. The command ends by itself with COMMAND_LOOPING (2)** as soon as the looping clip is reached, and the clip goes on looping on the creature with no command (inferred for the visual part). For Lily (pattern 32: `650.99` = animation 650 "Crouch Pray Enter", loops 98 ≥ 90 → -1, bPlayNext 1, bBlendIn 1), the command holds her for the enter clip, moves on to 652 "Crouch Pray Loop" (650's TransitionTo), and ends one step later with result 2. `Ambient_CommandComplete` returns at once on COMMAND_LOOPING and leaves AMBIENT_SYSTEM_RUNNING (0x40) set, so the engine sends her no AMBIENT_CONTINUE (`dev/AI.md`). When the plot later calls `Rubber_JumpHome`, she has no current command and the jump starts on her next command step. Even if the animation were still current, the jump would clear it: `CommandJumpToLocation` is type 10, whose `AddBehavior` in `commands.gda` is 2, a hard `ClearAllCommands` inside `QueueCommand`. **No code anywhere yields a running animation because something is queued behind it.** Neither the scripts (`Ambient_Stop` only clears the ENABLED bit, and the plot calls no ClearAllCommands) nor sub-action 37 do it. What frees the creature is the command's own end, or the add behaviour of the new command.
2. **A missing animation never waits, and there is no default duration.** Its length is 0, so the step is untimed. The command ends on its first step, which is the same frame it starts:
   - **1 (success)** for a one-shot animation, or for a looping one with nLoops ≥ 0;
   - **2 (COMMAND_LOOPING)** for a looping one with nLoops < 0;
   - **-3 (COMMAND_FAILED_INVALID_DATA)** when the id is 0 or has no row with an AnimName in table 0, the anim_ tables.
   
   With bPlayNext it moves on to the TransitionTo animation instead, as for one that played. Whether the clip counts as looping comes from the row's `Type` bit 0 when no node is found.

### The constructor (handler 0x0066FC20, not shared) (checked)

- It pops nAnimation, then nLoops if paramCount > 1, bPlayNext if > 2, and bBlendIn if > 3 (default 1). A failed pop returns -639. It builds type **14** (`Command::SetType(0xE)` at 0x0066FD66):
  - int 0: nAnimation;
  - int 2: nLoops;
  - int 7: 2 when bBlendIn = 0, otherwise 0;
  - flag 4: bPlayNext.
- Nothing is checked. A missing command pool pushes a null command.
- Its start (0x00744870) and end (0x00746E80) are the type table's entries +0x38, filled at 0x00747710.

### Start handler 0x00744870 (checked)

- It returns 0 and adds nothing for a null object, so the command completes with success. **It applies to any object, not only creatures.**
- It copies ints 0, 2, 4, 5 and 7, float 3 and flag 4 into a new parameter block. Then it calls `AddSubAction(37, params, 0, byte, 0)` with byte +4 = **(command flag 7 == 0)**.
  - A script command never sets flag 7, so **a script's PlayAnimation step is not interruptible.** A soft `ClearAllCommands` (and a soft-clear add behaviour) leaves it running. A hard clear ends it with -2 unless it is static.
  - Which engine code sets flag 7 was not read.
- If the command has a string 0 (scripts never give one), it also adds sub-action 5 with string 0 and int 1. This is not read further.

### Sub-action 37, 0x007A4390 (checked unless marked)

It runs with the parameters: int 0 the animation, int 2 the loops, int 3 the phase (0 at the start), int 4 an explicit next animation, int 5 a lead time, flag 0 "started", flag 4 play-next. Let `next = flag4 || int4 != 0`.

- **Guard.** Each run first answers **-3** if there is no object or no parameters, if int 0 = 0, or if `0x005CAAF0(anim)` is false. That function asks whether the id is in the map at 0xC32710, which 0x005CB4F0 builds from every row of table 0 with an `AnimName` (column hash 0x7A398AF9). The test is global, not per creature.
- **First run** (flag 0 clear):
  - **Length.** It uses `CCCreature::AnimationStepLength` (0x006957A0: the clip plus what it runs on into, up to the first loop) when `next` is false, and `CCCreature::AnimationLength` (0x00694E50: one clip) when `next` is true.
    - If float 3 > 0, the length is divided by it (the speed); scripts give 0, so 1.0.
    - If flag 3 is set, a tail is cut off (0x00694FE0); scripts never set it.
  - **Looping clip** (`0x00695390`, which is the creature's animation set (0x00A5E940) or else table 0's `Type` & 1, column hash 0x12A02374): duration = length × **int 2 (nLoops)**, and int 3 = 1.
  - **One-shot clip:** duration = length, ignoring nLoops.
  - If int 4 ≠ 0 and is not the clip's TransitionTo, int 5 = max(int 5, 100). Scripts give int 4 = 0.
  - **Playing it.** Unless flag 6 is set and that animation is already playing, it calls the creature's vtable slot 44 (+0xB0) with (anim, speed, blend, 1):
    - blend 2.5 by default (0xB03B08);
    - blend -1.0 when int 7 = 2, that is bBlendIn = 0 (0xAE2E00);
    - blend 3.0 when int 7 = 1 (0xAE7130).
    
    The meaning of these blend values is inferred. Then 0x007A3370 runs (floats 1 and 2, flags 1 and 2, vector 1; not read).
  - For a creature it calls 0x006A2A20 (inferred: stop moving). Then it sets flag 0.
- **Every run.** `remaining = duration > 0 ? duration - elapsed : 0` (0x007A1AE0). While remaining > int 5 it answers **0** (running). **A duration ≤ 0 counts as untimed, so remaining is 0 at once.** Then it switches on int 3:
  - **int 3 = 0 (a one-shot ended):**
    - If the creature's +0x421 is set (a SetObjectActive deactivation pending, `dev/PARTY.md`), it answers **1**.
    - Otherwise let `t` = int 4, or the clip's TransitionTo when int 4 is 0 (`0x005CA6B0`, column hash 0xFFC372E7).
    - If `next` is false, it answers **1**.
    - If `next` is true, it plays `t` only if int 5 > 0 or `t` differs from the TransitionTo (inferred: otherwise the animation player follows the transition itself). Then int 0 = `t`, and:
      - if `t` loops: duration = elapsed + AnimationLength(t) × nLoops, and int 3 = 1;
      - if not: duration = elapsed + AnimationLength(t), and int 3 = 2.
      
      It answers 0.
  - **int 3 = 1 (a loop ended):**
    - **nLoops < 0: it answers 2**, COMMAND_LOOPING (0x007A4776).
    - Otherwise int 5 = 0. If `next` is false it answers **1**.
    - If `next` is true: int 0 = TransitionTo (the exit), duration = elapsed + AnimationStepLength(exit), and int 3 = 2. It plays the exit at speed 1.0 with blend -2.0 and answers 0.
  - **int 3 = 2 (the play-next tail ended):** it answers **1**.
  - **Any other int 3:** it answers -3.
- **So, with nLoops = -1 on a looping clip:** duration = -length, so remaining is 0 and the answer is 2 at the first evaluation after the loop is reached.
  - Started directly on a loop clip, that is the **first run, the same frame** the command starts. The clip has been asked to play first.
  - With nLoops = 0 on a loop clip the answer is 1, also at once.
  - With N > 0, the command holds for N × the clip's length.
- **What 2 does.** The command step (0x0073CB60) removes a sub-action that answers anything but 0 and goes on to the next only after a 1. `StepCommands` (0x0073D320) ends the command for any result other than 0 or 1, with `ClearCurrentCommand(result, 1, 1, 0)`, which bypasses both the static and the interruptible tests. **So 2 ends the command and COMMAND_COMPLETE carries int 1 = 2.** script.ldf: `COMMAND_LOOPING = 2`.
- **Quirk: bPlayNext with no TransitionTo.** A one-shot clip with bPlayNext = 1 whose row has TransitionTo 0 sets int 0 = 0 after playing and answers 0. Its next run fails the guard, so the command ends with **-3** after the clip has played. This follows from the code and was not seen in play.

### End handler 0x00746E80 (checked)

- For a creature with parameters: if the command's flag 5 = 1, it calls 0x0069EBB0(1). Scripts never set it.
- Then it calls 0x007C1A30 on creature+0x744. That function calls 0x007C1690(-10.0) when a float at its +0xC differs from 1e-6; **it does not stop the clip** (corrected 2026-10-22: it was inferred to reset a blend or rate; read below). (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169)
  - [C] 0x007C1A30 (called on creature+0x744) tests a pointer at its +0xC and a float at that record's +0xC: when the pointer is set and the float is above 1e-6 it calls 0x007C1690 with -10.0 (0xC1200000). Otherwise nothing.
  - [C] 0x007C1690(x), for x = -10.0 (not within 1e-6 of 0): it fetches a record with 0x007C1570 and proceeds only if three of that record's pointers (+0, +4, +8) are set; else it writes 0 to the record's float +0xC. With them set and x below 0 it clears the record's byte +0x10, calls a virtual (vtable +0x60 of the object at +0x70 of its owner) and 0x00A661E0 on that object's +0x120/+0x130, then stores x (-10.0) in the record's float +0xC. For x above 99.0 it sets byte +0x10 to 1 and stores x. So the end handler leaves the record with +0xC = -10.0 and byte +0x10 = 0; it writes no animation id and calls no stop of the clip's playback.
  - [C] The branch for |x| <= 1e-6 (not taken from here) is the real clearing one: it zeroes the record's +4, +8, +0xC and byte +0x10, sets the owner's +0x748 to -1 and three bytes (+0xE8..+0xEA) of the object at vtable +0x60 to 0.
  - [I] What the record is (an animation layer's hold/release state, with 99 as a "hold" mark and a negative value as a release rate) and whether the clip then ends on its own are not read. "A loop keeps looping after a LOOPING end" therefore stays an inference from the absence of a stop call, not a seen result.
- Then the shared end 0x00746900 runs, with COMMAND_COMPLETE.
- After it, if the object's +0x421 (deactivation pending) is set, it calls the object's slot 80 (+0x140) with 0. That finishes `SetObjectActive(FALSE)` after its animation (`dev/PARTY.md`).

### How a newly queued command affects a running one (checked)

- **QueueCommand (0x0073CD10), the add behaviour.** When `nOverrideAddBehavior` is -1 (every `WR_AddCommand`; `AddCommand` 0x0066E0E0 passes it through), it reads `commands.gda` (table 94) for the new command's type:
  - `AddBehaviorSelf` (column hash 0x7569547A) when the current command has the same type;
  - otherwise `AddBehavior` (0xED36668A).
- It then acts on that value:
  - 0: nothing;
  - 1: `ClearAllCommands(0, 0)`, soft;
  - 2: `ClearAllCommands(1, 0)`, hard;
  - 3: a creature-specific choice (+0xC60 test).
  
  Only after that is the command added, at the back or the front.
- **The rows that matter here:** Play Animation (14) is 1/1; Jump To Point (10) and Jump To Object (9) are 2/2; Wait (13) is 1/1; Move To Point (20) is 2/2.
- **So:**
  - **A jump queued by default hard-clears**: the running PlayAnimation ends with -2 (COMMAND_FAILED_COMMAND_CLEARED; static ones excepted), and the jump starts on the next step.
  - **A PlayAnimation or Wait queued by default soft-clears**: queued non-static commands are freed with no COMMAND_COMPLETE, a running PlayAnimation stays (byte +4 = 1), and the new command runs after it.
  - With `nOverrideAddBehavior` 0, the new command simply waits for the animation's own end.
- **Sub-action 37 reads neither the queue (+0x2CC/+0x2D4) nor any "queue not empty" state.** `StepCommands` starts the next command only when the current one has no sub-actions left.
- **The jump itself.** The type 10 start (0x00742770) adds sub-action 33 (byte +4 = 0, untimed); that sub-action was not read.
- **The scripts.** `bhm000pt_tranquility.nss`:72–75 (JOWAN_HAS_INFO) runs `Ambient_Stop(oLily)`, `Rubber_SetHome` and then `Rubber_JumpHome` (`sys_rubberband_h.nss`:94, `WR_AddCommand(oTarget, CommandJumpToLocation(lHome))`). There is no ClearAllCommands. The jump's own add behaviour does the clearing.
- **The ambient side** (`sys_ambient_h.nss` `Ambient_Animate`:825): `WR_AddCommand(CommandPlayAnimation(anim, loops ≥ 90 ? -1 : loops, bPlayNext, bBlend))`, then RUNNING is set when loops > 0. `Ambient_CommandComplete`:1229 does the following for Interact and PlayAnimation:
  - on LOOPING, it returns and does nothing more (RUNNING stays set, so no further ambient beats);
  - on SUCCESSFUL, it clears RUNNING;
  - in every other case it calls `Ambient_DoSomething`.

#### Which sub-actions a command's start handlers add, and their byte +4 (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169)

- [C] `CCObject::AddSubAction` is 0x0073D4D0 (`__thiscall`, args: type, CDataArrays*, a3, byte, duration; the byte is stored at the new sub-action's +4). Every direct call site in the exe was scanned (73 calls). The byte is read by `ClearCurrentCommand` (0x0073C730): a soft clear leaves the running step alone when its byte is 1 (TURN_BASED.md, the melee attack section, and "How a newly queued command affects a running one" above).
- [C] Start handlers (from the type tables filled at 0x00747480; start at +0x54 of the table, end at +0x58, entry = 4 x type) and the sub-actions they add, all with byte 0 (so a soft clear removes the command):
  - type 1, attack (0x00742FD0): sub-action 4 and sub-action 50 (0x32), byte 0.
  - type 13, wait (0x00742950): sub-action 36 (0x24), byte 0.
  - type 20, move to location (0x00745630): sub-actions 36, 14 and 42 (0x24, 0xE, 0x2A), byte 0.
  - type 21, move to object (0x00746F00): sub-action 4 or 15 (0xF), chosen by a virtual call (slot +0x24) on the object it resolves, byte 0.
  - type 22, move to points (0x007457C0): sub-actions 52 (0x34) and 16 (0x10), byte 0.
- [C] With byte 1: type 14 play animation (0x00744870) when its command flag 7 is 0, which is every script command (see "Start handler 0x00744870"); type 40, switch weapon sets (0x00743570): sub-actions 22 (0x16) and 48 (0x30), byte 1; and sub-actions added while a command runs: the melee swing, sub-action 1 (0x007A63ED, already in TURN_BASED.md), sub-action 13 (0x007A6450), the ability steps 7 and 8 (0x007A86BD, 0x007A871E), the cast sub-actions 37 (0x25) added at 0x007A2883, 0x007A295E, 0x007A3053, 0x007A36D5, 0x007A4EB6, 0x007A4F72 and 0x007A7693, sub-action 6 (0x007A4F25) and sub-action 48 (0x007A5092, a3 = 1).
- [I] The other start handlers' sub-actions (types 8 to 12, 25, 31, 37 to 39, 41, 42) were listed by call site only; their byte is 0 at every call site found except those named above, but which handler owns each call site beyond the types named was not checked one by one.

### A missing animation (Q2)

- **Its length.**
  - `AnimationLength`/`AnimationStepLength` → 0x00A5F7A0 on the creature's animation set (+0x154): 0 for id 0 or no set. Otherwise it gives the node's length when one of the creature's trees has the id (0x00A5E390, the same test as `HasAnimation` 0x00694E70) (checked).
  - Otherwise it asks the animation system for the clip by the row's AnimName (0x005CB720 → 0x00A5EE50, × 1000.0 to ms), or 0 if the row has none (checked).
  - What that fallback answers for a clip no tree holds, and whether the Replacement chain (`core/Anb.h` `ClipOf`) is followed in this lookup, was **not read**. The answer below assumes it gives 0 (inferred).
- **The step with length 0.** The duration is 0 (× nLoops for a looping row), so the step is untimed and reaches the switch on its first run. The results are those in the short answers (checked from the code above):
  - one-shot: 1;
  - looping with nLoops ≥ 0: 1;
  - looping with nLoops < 0: 2;
  - with bPlayNext: the TransitionTo is taken up and timed by its own length (or -3 if there is none).
  
  The guard's -3 applies only to id 0 or an id with no AnimName row, not to "this creature lacks it".
- **When it completes.** `StepCommands` starts a command and steps it in the same call: a command with no sub-actions is ended, the next is started (0x0073C7D0), and the step runs. **So COMMAND_COMPLETE comes in the frame the command starts** (checked by flow; the frame is inferred).
- **What the creature shows.** Slot 44 is still asked to play the missing id; what it does then (nothing, or a replacement clip) was not read.

### Not read

- Sub-action 33 (the jump) and sub-action 5 (the string 0 follow-up); 0x007A3370 (the step's vector and float 1/2 use); 0x00694FE0 (flag 3's cut); the slot-44 player and its blend values' meaning.
- 0x00A5EE50's answer for a clip found in no tree, and whether table 0's Replacement is applied in 0x00A5E390 or the per-tree maps it reads.
- Which engine code sets flag 7 on a type-14 command (an interruptible animation) or flag 6 (don't replay).
- AddBehavior 3's +0xC60 test in detail.
- Nothing here was seen in play: Lily's COMMAND_COMPLETE with 2, and the -3 after a play-next with no TransitionTo, are both from the code.

## Blending, movement and layers (read 2026-10-18 by lib-agent-2)

Read in the GOG exe and the game's files for DAOParagonEngine's animation audit; the key constants and the look_at table checked.

### Clip cross-fades (animfade), read 2026-10-18 by lib-agent-2 in GOG DAOrigins.exe 1.05 (base 0x400000), for DAOParagonEngine

Read in the disassembly (daoexe.py, capstone) and the game's files (ghilana). Not seen in play. "(checked)" = read in disassembly or data; "(inferred)" = not. Scratch: `%TEMP%\lib-agent-2-animfade-*`.

#### Short answer

1. **There is no blend column in anim_base / anim_combat / anim_placeables and no blend field in the .anb.** The table's columns are ID, Brow, StrRef, Type, AnimName, WieldType, SynchAnim, TransitionTo, Priority, DisableLookAt, State, Replacement, Label (checked, ghilana; the code reads Type 0x12A02374, State 0xE991BD51, TransitionTo 0xFFC372E7, Replacement 0xC47761F2, AnimName 0x7A398AF9; no code pushes SynchAnim's hash 0x24907819, checked by a scan of all `push imm32`). The .anb node has only name, file, weight, flags, children, parent (4024-4030).
2. **The blend is a parameter of the play call**: `CCCreature` vtable slot 44 (+0xB0, 0x006AA7E0) `Play(anim, speed, blend, flag)`. Its call sites in sub-action 37 give **blend 2.5** (default), **3.0** (bBlendIn int 7 = 1), **-1.0** (bBlendIn = FALSE, int 7 = 2), **-2.0** (the play-next exit clip). The value is a **rate in weight per second**: a fade of full weight 0 to 1 takes **1 / blend** seconds, **linear**. 2.5 = **0.40 s**, 3.0 = **0.333 s**, 4.0 = 0.25 s.
3. **A per-pair blend curve overrides the rate**: the animation set's `<set>.tbi` (GFF4, fields 4031 AnimFrom, 4032 AnimTo, 4033 Data = 8 floats, list 4034). When the old and new clips are a listed (from, to) pair, the old clip's weight follows a cubic Bezier **y(t)** and the new clip's weight follows **1 - y(t)**, for the curve's length (0.433 s or 0.6 s in humanoid.tbi), instead of the linear ramp.
4. **Same clip asked again = no-op** (no restart, no blend) in the usual case. **The outgoing clip keeps playing** during the fade (only its weight falls). Phase sync on a walk/run switch: **not found** (SynchAnim is unread; events MvmtSetOffset/MvmtSetTransition not read).

#### 1. The play call and where the numbers come from (checked)

- Sub-action 37 (0x007A4390) calls the creature's vtable slot 44 (0x006AA7E0, `ret 0x10`) with the stack `(anim id, float speed, float blend, 1)`:
  - 0x007A4558: blend = float at 0xB03B08 = **2.5**. Then `GetParam(7)` (0xA5A220): 1 -> float 0xAE7130 = **3.0**; 2 -> float 0xAE2E00 = **-1.0**. (Command int 7 = 2 when bBlendIn = FALSE, else 0; so scripts' default is 2.5. Int 7 = 1 is not set by the script command; who sets it was not read.)
  - 0x007A4748 (the exit clip of a play-next chain, nLoops >= 0): speed 1.0, blend float 0xAFF63C = **-2.0**.
  - 0x007A47CB (the next clip of a chain when int 5 > 0 or it differs from the TransitionTo): speed 1.0, blend = `1000 / (int 5)`-derived: `fild [int5]; fdivr 1000.0` = 1000 / int5 per second. Int 5 is 100 at least (set at 0x007A4530 to max(int5, 100)), so this blend is at most **10** (0.1 s), and 1000/int5 for a longer lead.
- Wrapper 0x006AA7E0 -> 0x0069B3D0 `PlayInner(anim, speed, blend, 1)`:
  - **blend <= -2.0** is a code, not a rate: blend := **1000 / (0.20 * AnimationStepLength(anim) in ms)**, i.e. the fade lasts **20 % of the clip's step length** (0x00694FE0: the second float is compared to -2.0 and -3.0 with a tolerance of 1e-6; -2.0 -> 20 %, -3.0 -> 5 %, any other value <= -2 -> 20 %). So the exit clip (blend -2.0) fades in over **0.2 x its length** (a 1.0 s exit = 0.2 s). Nothing in the exe passes -3.0 that I found; it is a spare.
  - **blend = -1.0** is the other code: **instant** (see section 3).
  - **anim id 1 (pause) only**: speed += `(rand() % 90) / 100 - 0.45`, so the idle runs at speed + [-0.45, +0.44] (0x0069B478-0x0069B4A8; rand = 0x008E6A8E). Checked.
  - Rows with Type bit 2 (value 2: lookcenter, lookup, ... the look/face rows) go to the face queues (+0x178 looping, +0x168 once), not the body blend: `5CA610(anim, 2)`. Type bit 1 = loop; Type bits 16 / 32 = combat enter / exit (anim_combat 100, 101, 110, 111).
  - Otherwise `HasAnimation` (0x00694E70) then 0x006958F0 -> the animation set 0x00A5FCC0 `(anim, speed, blend, 0.0, str, str, 1)`.
- Wrapper side effects (0x006AA7E0): when arg 4 != 0 or anim == 1, `0x007C1690(0.0)` + `0x007C1EA0()` on creature+0x744 (not read); anim 100/110 -> `0x006A7F30(1)`, 101/111 -> `0x006A7F30(0)` (the stance, itself a 3.0 blend: 0x006A7F30 passes float 0xAE7130 = 3.0 to 0x006A6D90 and 0x006A6F60, and 0x006A7FA8..); if the old animation's State (col 0xE991BD51) is 1 or differs from the new one's, byte +0xB40 = 1, +0xB41 = 0.
- **Hooked as `dao::animation::OnPlayed` (read 2026-10-21, #217):** 0x006AA7E0 is `__thiscall (int anim, float speed, float blend, byte flag)`, `ret 0x10`, and returns nothing: every caller read (ShowLine 0x0068E997, sub-action 37 0x007A4597 and 0x007A4767) overwrites eax before using it. Only CCCreature's vtable (0x00B10484, slot 44) holds the address, no other vtable overrides it and nothing calls it directly, so a detour on the function sees every creature. The callback runs after the original returns. Seen in play 2026-10-21 by eluvian-agent (Eluvian #165, the forest clearing): every creature plays row 1 Pause on load at blend -1 then -2; idle twitches 640 to 644 at blend 2.5 from PlayAnimation; an archer's Aim 249 at speed 1.9575, Fire 243 48 frames later, Withdraw 248, then Pause at 3.0 and Exit 101 at 2.5. Walking about 75 m played no clip through slot 44: locomotion is driven elsewhere (the movement blend [inferred]).

#### 2. The animation set (class with vtable slots 22/23/33/34 = AnimBase 0x00B5959C family): what a play does (checked)

`0x00A5FCC0(anim, speed, blend, f, nameA, nameB, bool)`:
- stores the previous id: +0x144 <- +0x13C; the new id: +0x13C <- anim; if the creature's tree has the node (`0x00A5E940`, a lookup of the row's AnimName in the creature's trees) +0x140 <- anim, else `0x00A5F480` sets +0x140 <- the row's TransitionTo;
- **+0x148 = speed, +0x14C = blend**; then `0x00A5F9C0`.

`0x00A5F9C0` (the switch):
- **Early out (same clip, no restart, no blend):** if the new id has a node, the id equals the previous one, and the two node-name strings (+0x1D0 / +0x1EC) are equal, it returns at 0x00A5FA21 having done nothing. So asking again for the clip already playing neither restarts nor re-blends it (checked flow; a one-shot that has finished is not separately handled in this function, not read).
- Else it plays the new clip: vtable slot 22 (0x00A5E670, by id) / slot 23 (0x00A5F860, by name) with `(speed, 0.0, 1.0, blend)`. Slot 23 calls the model object's `IAnimatable` slot 8 (+0x20) `(node, anim, speed, 0.0)` = the clip's **playback rate** (ModelAnimationTree slot 12, 0x00813D70: `+0x20 = speed * 65535 / clip length`, time in 16 bits, time restarts at 0 (flag +0x32 = 1)), then slot 18 (+0x48) `(node, anim, 1.0, blend)` = set weight 1.0 with the blend (section 3).
- **The old clip:** it finds the pair's blend curve (`0x00A5F350(prev, cur)`: both ids -> AnimName -> node names, then a map lookup `0x00A69300` in the set's transition map at +0xF0, which `0x00A5D5A0` loads by name via `0x00A69770` / `0x00A69460` from a **GFF4 with fields 4031-4034 = the .tbi**). 
  - **If a curve exists:** `0x00A5C8A0(prev, fadeIn=0, curve)` and `0x00A5C8A0(cur, fadeIn=1, curve)` -> AnimBase slot 33 (0x00A5EF30) -> model slot 20 (+0x50) -> ModelAnimationTree slot 6 (0x00813860) -> 0x00815110 (tween mode 3, curve pointer, byte flag).
  - **Else:** `0x00A5C840(prev, 0.0, ..., blend)` and the new clip's 1.0 with the same blend, via AnimBase slot 34 (0x00A605E0, `SetWeight(name, target, blend)`): **both fade at the same rate: the old to 0, the new to 1**, a plain **cross-fade** (the two weights are separate ramps, not forced to sum to 1; if the old was part-faded its ramp is shorter: |from - to| / blend).
  - So the outgoing clip is **not frozen and not stopped**: only its weight goes to 0 (its playback rate +0x20 is untouched) (inferred: nothing here stops it; what removes a node at weight 0 was not read).

#### 3. The weight tween (ModelAnimationTree entries, 0x34 bytes each; the weight object is at entry +8) (checked)

- Set with a target and a blend (`ModelAnimationTree` slot 4, 0x00813740, `(clip, target, blend)`):
  - **blend == -1.0 (compare with float 0xAE2E00): the weight jumps to the target at once** (0x00815100: mode 0, value = target). This is `bBlendIn = FALSE`.
  - else 0x00815130: if the target equals the current value, a constant; otherwise **mode 1 (linear)**: start = current, end = target, elapsed = 0, **rate = blend / |end - start|**.
- Per frame (0x008151F0, arg = dt in seconds): `elapsed += dt`; mode 1: `u = elapsed * rate`; if `u <= 0` nothing changes (a negative rate never moves: blend must be positive here, the negative codes are resolved before, section 1); if `u >= 1` the value = end; else **value = (1 - u) * start + u * end** (mode 2, not used by the play path I read, uses u' = u*(2 - u)... the code at 0x00815453 is an ease-out `u * (2 - u)`, mode 2 is set by 0x00815190, whose caller was not read).
  - So the fade **from 0 to 1 takes 1 / blend seconds**, and from w to 1 takes (1 - w) / blend.
- **Mode 3 (a .tbi curve, 0x008151F0 at 0x00815230 and 0x0081528D-0x0081540F):** `elapsed += dt`; while `elapsed <= 0` nothing; when `elapsed >= curve.x3` (the float at curve+0x18) the value is **y3 (+0x1C)** (flag 0, the outgoing clip) or **1 - y3** (flag 1, the incoming clip). In between the time elapsed is mapped to a curve parameter s by a numerical search on the **cubic Bezier x(s)** with control x values at curve +0, +8, +0x10, +0x18 and the weight is the **Bezier y(s)** with the y values at +4, +0xC, +0x14, +0x1C (Bernstein terms with the constant 3.0 at 0xAF0C38 are visible; the search's stopping tolerance and step were not decoded: the constant 1/30 at 0xC12420 is used there). The incoming clip's weight is **1 - y** (flag 1: `fsubp`, 0x00815404).
  - Data as stored (field 4033): `[x0,y0, x1,y1, x2,y2, x3,y3]` = the four Bezier control points (time in seconds, weight).

#### 4. The .tbi files (checked, ghilana)

`humanoid.tbi` (832 bytes, in global.rim / anims.erf) lists 7 (from, to) pairs, all with y: 1 -> 0 (the outgoing clip's weight, the incoming one is 1 - y):

| from -> to | data |
|---|---|
| m.ua_cast_end.ani -> shared.pause.ani | 0,1, 0.108,1, 0.325,0, 0.433,0 |
| m.ua_dcast_end.ani -> shared.pause.ani | same |
| shared.pause.ani -> m.ua_aconjure_start.ani | same |
| shared.pause.ani -> m.ua_dconjure_start.ani | same |
| shared.berserk.ani -> shared.pause.ani | 0,1, 0.15,1, 0.45,0, 0.60,0 |
| ws.talent_shieldbashgetup.ani -> shared.pause.ani | same (0.6 s) |
| ws.talent_wcry.ani -> shared.pause.ani | same (0.6 s) |

- So for the mage cast end / conjure start the fade is **0.433 s** with a hold of 0.108 s at full old weight, then the old falls (and the new rises) until 0.325 s, then 0.108 s of the new clip alone; for berserk / shield bash getup / war cry exits it is **0.6 s** (hold 0.15, change 0.15-0.45).
- `c_beara.tbi` (bear form): `c_blackbear.pause.ani -> c_blackbear.runf.ani`, 0,1, 0.058,1, 0.175,0, 0.233,0 (**0.233 s**). `c_deepstkra`, `c_dsogre`, `c_goata`, `c_mutta`, `c_spidera`: empty lists (112 bytes). Other creature sets have no .tbi (the file list from ghilana: only those 7 exist).
- **The .tbi pairs probably never fire** (inferred; corrected 2026-10-18 by lib-agent-2): their keys (`shared.pause.ani`, `m.ua_cast_end.ani`, `shared.berserk.ani`, ...) look like clip file names, but none matches a node file of the shipped trees (`mh_c.*` and the like), so the lookup (`0x00A5F350` -> `0x00A69300`) finds no pair. How the set normalises names (`0x00A5E730`) was not read, and a play check (a 0.333 s linear fade against a 0.433 s curve after `ua_cast_end`) would settle it.

#### 5. Cases, in seconds

| case | blend | time | curve |
|---|---|---|---|
| CommandPlayAnimation clip, bBlendIn = TRUE (default) | 2.5 | **0.40** | linear (or the .tbi curve if the pair is listed) |
| CommandPlayAnimation, bBlendIn = FALSE | -1.0 | **0** (instant) | the weight jumps |
| exit clip of a bPlayNext chain (after the loops) | -2.0 | **0.2 x the exit clip's step length** | linear |
| next clip of a play-next chain whose int 5 > 0 | 1000/int5 (<= 10) | int5/1000 s (>= 0.1) | linear |
| int 7 = 1 (set by something not read) | 3.0 | 0.333 | linear |
| a command's clip ends (one-shot, no next) | none by the command | the engine's own TransitionTo follow-up (pause) | (inferred) the set's own play; see "Not read" |
| a listed (from, to) pair | the pair's curve | 0.433 / 0.6 / 0.233 | cubic Bezier (above) |
| movement controller (MovementAnimController): after a mode event 1, 4 or 8 | +0x50 = 4.0 (0xADEF1C) | **0.25** | linear; before the first such event it is **-1.0 = instant** (ctor 0x00A69F43) |
| combat stance on/off (0x006A7F30, draw/sheathe) | 3.0 | 0.333 | linear (checked: the stance setters pass 0xAE7130) |
| the weapon draw (sheathe.md): the set called at "3.0 blend" | 3.0 | 0.333 | linear |

- **Idle <-> walk <-> run:** locomotion is not a CommandPlayAnimation: it runs through `MovementAnimController` (vtable 0x00B59AC0, code 0x00A69A00-0x00A6BF00): it sets many named nodes' weights (AnimBase slot 34, `(node, target, blend)`) with the blend in its +0x50 (4.0 once a mode 1/4/8 event has come, so **0.25 s linear**), and its direction angles compare against 89 / 91 / 179 degrees and 1.3 (floats 0xB59AB8..0xB59ACC; the 0x00A6A0B0 function builds a bit mask of the move direction). The walk/run split of the creature's movement state +0xB20 (0/3 stand, 1/4 walk, 2 run, from the audio notes) was **not followed into the controller**; the 0.25 s figure is the controller's whole blend rate (checked for the writes of +0x50: only the ctor and 0x00A6A87A).
- **The ConversationBlend etc.:** not read.

#### 6. Questions as asked

- **Same clip replaced by itself:** continues (no restart, no blend) when the node exists and the strings match (0x00A5FA1F early out). Sub-action 37 also has a flag 6 test (don't replay if already playing, 0x007A454E) before even calling.
- **Outgoing clip during the fade:** still playing, only its weight falls (inferred; nothing in the switch stops or freezes it).
- **Walk -> run phase:** no sync code found. Table column SynchAnim is never read (no immediate push of its hash anywhere in the exe). Events 25 "Set Next Transition" (MvmtSetTransition) and 26 "Set Animation Offset" (MvmtSetOffset) exist in events.gda (table 95) and are the likely carriers of a locomotion phase/transition, with 23/24 Lock/Unlock Movement and 11/12 BlendStart/BlendEnd: **not read**.
- **Where the time comes from, per source:** a parameter of the play call (2.5, 3.0, -1.0, -2.0, 1000/int5); a constant in the exe (above, 4.0 for movement, 0.2 / 0.05 fractions for the -2 / -3 codes); `<set>.tbi` for listed pairs; **not** the .anb, **not** the anim_ tables.

#### Not read

- The caller of the tween update (0x008151F0) per frame and what drives the clip's own time/loop; what removes a node whose weight has reached 0.
- How a finished one-shot hands over to the TransitionTo clip with no play call (the "animation player follows the transition itself" in playanim.md); AnimationEvents 11-13 (BlendStart, BlendEnd, Done) and 23-26 handlers.
- The set's normalisation of clip names for the tbi map (0x00A5E730, 0x00A69300), and which tbi is loaded for which creature (0x00A5D5A0's caller; likely the apr_base tree name + ".tbi"; not read).
- Mode 2 of the tween (0x00815190's callers), the exact numeric search of the Bezier (0x0081528D-0x0081536C), the movement controller's event numbers and its named nodes (strings are runtime globals at 0xC4C5A0 etc.), and what sets bBlendIn int 7 = 1 (blend 3.0).
- Nothing here was seen in play.

### Movement animation: the blend, the play rate, start/stop, turning (animmove)

Read 2026-10-18 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py --disasm`, a rel32 call scanner, `apr_base`/`anim_base`/`events` through ghilana, and the movement trees and clips extracted with ghilana. No Ghidra, no game. "(checked)" = read in the disassembly or the data; "(inferred)" = reading of purpose, not followed to the end. Addresses are GOG's.

#### Short answers

1. **Play rate: yes, scaled to the creature's actual speed, from a data table, not from the clip's travel.** One rate is applied to every leaf clip of the movement tree (walkf, runf, runleanl/r, walkb, ...). With `x` = the controller's current speed (m/s) and the creature's `apr_base` row (`Anim_*Speed` = the speed the clip was authored at, `Game_*Speed` = the speed the creature really moves at): below the walk speed `rate = x / Anim_WalkSpeed`; between walk and run speed the rate is a straight line between `Game_WalkSpeed/Anim_WalkSpeed` and `Game_RunSpeed/Anim_RunSpeed`; above run speed `rate = x / Anim_RunSpeed`. Combat uses `Anim_CbtSpeed` (strafing/backing `Anim_CbtStrFSpeed`) and `rate = x / thatAnimSpeed`. For a human (Anim walk 1.54, run 3.62; Game walk 1.54, run 6.0): walk plays at 1.0, run at 6/3.62 = 1.657. No sub-clip speed matching, no clip travel in the loop.
2. **Walk/run is a blend of two sub-trees by speed, in the .anb**: top node `Motion` has children `Run` (runleanl, runf, runleanr) and `Walk` (WalkForward/walkf, or in combat WalkForward/WalkBackward with walkfl/walkfr/walkbl/walkbr). Non-combat weights: `Run = speed / Game_RunSpeed` if `speed > Game_WalkSpeed`, else 0; `Walk = 1 - Run`. Combat: Run is 1.0 (the combat tree's `runf`) and Walk and strafe nodes are weighted by the angle between the heading and the move direction. The speed ramps at 8 m/s^2, so walk to run crossfades over 0.56 s.
3. **Start, stop and in-place turn have dedicated clips, but only in the non-combat tree** (`movement.anb`'s `Transition` node has 10 clips; `combat_movement.anb`'s has none). The start (pause to walk/run, with a turn of 90 or 180 degrees built in), the stop (run to pause on the left or right foot) and the in-place turn (turnl90/turnr90) are all rows of `anim_base` (84-94, 74-75/80-83, 59/61), played on the `Transition` node while `Motion` goes to 0. The stop's foot is chosen by clip events (`MvmtSetTransition`), the only "foot planting" there is. No foot IK was found.
4. **Turning: the creature's facing is rotated by code at `Δ' = 180 deg * dt / T`, `T = 0.4 - 0.28 * |Δ|/180` seconds (450 deg/s for a small angle up to 1500 deg/s for 180 deg, clamped so it never overshoots), in every state but idle.** On top of that, a standing creature (non-combat, controller state 4) plays a 90-degree turn clip when the angle is above 30 degrees, at a rate chosen so the turn takes `angle/120` seconds (rate capped at 1.3), the clip's root yaw scaled to the angle. The model follows the fast logical turn and the clip's root yaw does nothing (see "Not settled about the turn" below). (corrected 2026-10-18 by lib-agent-2)

#### Classes

- **`MovementController`**, vtable 0x00B596C0 (1 slot, the destructor), the creature's object at **creature+0xA30** (`CCCreature`), constructed at 0x00A632D0. Fields read (checked unless marked): +0x70 the `MovementAnimController*`, +0x74 has-anim flag, +0x80 move direction, +0x90 desired facing, +0xA0 current facing (copied from the creature's orientation each update, 0x00A644F6), +0xD4 speed multiplier (1.0, no setter found), +0xD8 corner-slowdown factor, +0xDC speed offset (0), +0xE0 current speed, +0xE4 target speed, +0xE8 remaining path distance, +0xEC apr_base row id (word), **+0xF0 state**, +0xF8 strafe-lock (face a target), +0xF9 combat flag.
- **State [+0xF0]** (0xA63800 sets it): 0 idle, 1 walk, 2 run, 3 stopping (the stop clip is moving the creature), 4 turning in place. 0x00A639C0 sets 1 or 2 from a bool; 0x00A638B0 `SetDesiredFacing` sets 4.
- **`MovementAnimController`**, vtable 0x00B59AC0, at **controller+0x70**, constructed at 0x00A69EC0, size 0x180. [+0x10] the creature's `AnimBase` (its virtual 0x88 = `SetNodeWeight(name, weight, blend, 0)` 0x00A605E0, 0x74 = set a node's play rate 0x00A5F090, `AnimBase::GetNode(animId)` 0x00A5E390), [+0x40] flags of this frame, [+0x54] mode, [+0x6C] combat, [+0x6F] strafing (= combat and the controller's strafe flag), [+0x134] speed, [+0x138] target speed, [+0x130] remaining distance, [+0x13C] the controller's state, [+0x50] blend argument (-1.0 at construction, 4.0 after the first mode change), [+0x140]/[+0x15C] the strings of `apr_base` `MovementTree`/`CombatMoveTree`.
- The per-frame call: `MovementController::Update` 0x00A644E0 (from the creature's update, 0x006BE1AF region) -> `0xA63CE0`/`0xA63520` builds the input (five vec4: creature position, move direction, desired facing, current facing, and three floats speed*factor, target speed, remaining distance) -> `0xA6B180` stores it, previous copied -> `0xA6BF00` the anim update.

#### 1. The movement blend

##### The table (checked)

`0xA6B2B0` (on a change of the row id) reads `apr_base` (table 2) for the creature's row, column hashes: `MovementTree` 0xEB48D02A [+0x140], `CombatMoveTree` 0x75F492EC [+0x15C], `Anim_WalkSpeed` 0xCD9CC343 [+0x20], `Anim_RunSpeed` 0xBD6D4446 [+0x24], `Anim_CbtStrFSpeed` 0x7FB8C4E9 [+0x28], `Anim_CbtSpeed` 0x10727878 [+0x2C], `Game_WalkSpeed` 0x958F23C8 [+0x30], `Game_RunSpeed` 0xA0DCD78C [+0x34], `Game_CbtSpeed` 0x0DC3EBB2 [+0x38]. Defaults 1.0. (`Game_CbtStrFSpeed`, hash 0x03186054, is read by the controller, not by the anim side.) Human row (id 15): Anim walk 1.54, run 3.62, cbt strafe 2.0, cbt 3.62; Game walk 1.54, run 6.0, cbt strafe 3.5, cbt 4.66. The table has per-creature values (dragon 2.42/6.82, dog Anim walk 1.2 vs Game 1.54, ...), so a dog's walk plays at 1.54/1.2 = 1.28.

##### The creature's speed (checked)

Target speed, `0xA63120`: state 0, 3, 4 give 0; state 1 `(Game_WalkSpeed + [+0xDC]) * [+0xD4]`; state 2 the same with `Game_RunSpeed`; with the combat flag [+0xF9] `Game_CbtSpeed`, or `Game_CbtStrFSpeed` when [+0xF8] (strafe-lock). `0xA635D0` ramps the current speed [+0xE0]: **+8.0 m/s per second up and down** (double 0xB1B7C8 = 8.0, times dt in seconds, clamped to the target); in state 4 of the anim controller (a transition clip plays) the speed is set to the target at once; a stop with the heading reversed (dot of [+0x80] and [+0x90] below 0) sets it to the target (0) at once. The distance moved per frame is `speed * dt` (0xA63100). Run with no combat: `0xA63450` slows the step at path corners, `factor = max(0, 1 - d)`, `d = -dot(prevHeading, dir) * 0.65` (double 0xB596D8 = 0.65; no slowdown when d <= 0), kept in [+0xD8]; the speed given to the animation is `speed * factor`.

##### The play rate formula (checked), `0xA6AA70(x)` called from `0xA6B5D0` with x = [+0x134]

```
animWalk, animRun, gameWalk, gameRun  =  [+0x20], [+0x24], [+0x30], [+0x34]      (explore)
animWalk = animRun = [+0x28] (strafe) or [+0x2C];  gameWalk = gameRun = [+0x38]  (combat;  [+0x6F] picks strafe)
kw = gameWalk / animWalk ;  kr = gameRun / animRun ;  D = gameRun - gameWalk
if |D| <= 1e-6                 rate = x / animRun
elif x > gameRun - 1e-6        rate = x / animRun
elif x < gameWalk - 1e-6       rate = x / animWalk
else                           rate = kw + (kr - kw) * (x - gameWalk) / D
rate *= [+0x48]                 (1.0; nothing writes it)
```
(constant 1e-6 = double 0xAE09B0). `0xA6B5D0` sets it on every named leaf of the active array if it is >= 0 and differs from the last ([+0x68]): non-combat array `{walkf, walkb, runf, runleanl, runleanr}` (0xC4C8BC), combat `{walkf, walkfl, walkfr, walkb, walkbl, walkbr, runf}` (0xC4C8CC), by AnimBase virtual 0x74 (0xA5F090). So the walk and run loops always run at the same rate. For a human at x = 3.8 m/s the rate is 1.0 + (1.657 - 1.0) * (3.8 - 1.54)/(6 - 1.54) = 1.333.

##### The weights (checked unless marked), non-combat `0xA6B7A0`, combat `0xA6B9B0`

All set through `SetNodeWeight(name, weight, [+0x50], 0)`.
- Flag bits [+0x40], from `0xA6A0B0` (checked): 1 moving forward (dot of [+0xF0] move direction and [+0x100] facing > 0; in strafing the angle between the heading and the move direction below 89 degrees), 2 backward, 4 the turn is to the left, 8 to the right (sign of the 2D cross of the two facings, 4 when > 0), 0x10/0x20 strafe left/right, 0x40 vertical motion. Only with speed [+0x134] > 0; the facing/move vectors are compared by `Vector::AngleBetween` (0x004D4C90).
- Non-combat: `runBlend = 0xA69B50(speed)`: 0 when moving backward or not moving, 1.0 in combat, else `speed > Game_WalkSpeed ? speed / Game_RunSpeed : 0` (so at the walk speed Run jumps to 0.257 for a human the instant speed passes 1.54; **not clamped to 1** here, inferred that the tree clamps). `Walk = 1 - runBlend`, `Run = runBlend`, `WalkForward = 1`/`WalkBackward = 0` (swapped for backward, bit 2, with walkb = 1), `walkf = 1`. Run lean (inferred from 0xA6A9A0/0xA6B7A0): `runf = 1 - |lean|`, `runleanl`/`runleanr = |lean|` by the sign, where lean comes from the heading change (0xA6DEE0 through a smoothing object [+0x14], only if anims 65 and 66 exist and the speed reaches the run speed).
- Combat (strafing, `[+0x4C] = s`): forward: `walkf = s`, the side node `walkfl` or `walkfr` = `1 - s` (bit 0x10 / 0x20, else both 0 except...); backward: `walkb = s`, `walkbl`/`walkbr` = `1 - s`; `Walk = 1`, `Run = 0`, `WalkForward`/`WalkBackward` 1/0 by bit 2. `s` is linear in the angle between heading and move direction (about `(90 - angle)/90` forward, `(angle - 90)/90` backward; the 89/91/179 degree thresholds are floats 0xB59ACC/0xB59AC4/0xB59AC8, the 90 a double 0xB02CC0; inferred from the arithmetic, not followed to a formula). Run in combat is the whole run node when the speed is Game_CbtSpeed.
- **Blend speed**: `[+0x50]` is -1.0 at construction and 4.0 (float 0xADEF1C) once any mode has been entered (every enter routine writes 4.0); transition clips are played with a blend argument 4.0, the controller's +0x50, not 2.5 (corrected 2026-10-18 by lib-agent-2) (2.5 is the blend of the 0xA5FCC0 bookkeeping calls in the same function), and rate 1.0. What the number means inside the tree (seconds, or per second) was not read: both look like rates.

##### Speed thresholds in one list

- Walk/run is by *speed*, not by command: `Game_WalkSpeed` (1.54) is where Run starts to weigh in; `Game_RunSpeed` (6.0) is where Run reaches speed/6 = 1; "running" for the start and stop clips is `speed >= Game_RunSpeed - 1e-6` (0xA69C70).
- Command side: state 1 (walk) targets `Game_WalkSpeed`, state 2 (run) `Game_RunSpeed`, combat `Game_CbtSpeed` (4.66) or `Game_CbtStrFSpeed` (3.5) when facing a target.

##### Footsteps, foot planting, IK (checked / not found)

- **No foot IK or foot planting in the movement code.** RTTI has `IKDNode` (a two-pointer container in `CCArea`'s code, not IK; corrected 2026-10-19 by lib-agent-2) and `PartLookAtController` (head look-at). `apr_base` has `TerrainFollowPitch`/`Roll`/`Precision` (loaded at 0x00748460/0x00748476/0x0074848C into an object's +0xA8/+0xAC/+0xB0), read by the body's tilt to the ground, 0x006A7AE0 → 0x006A7520, which turns only the model about its feet (`AnimBase::SetOrientation` 0x00A5CCC0; the maths and its inputs in `dev/PATHFINDING.md`'s body tilt); a walking creature's z is its PhysX capsule's: `dev/PATHFINDING.md`, "How a walking creature gets its z" (read 2026-10-19 by lib-agent-2).
- **Footsteps are clip events** (`events.gda`): 14 `FootDown`, 17 `MovementSound` with string "Small" (and "|Left" with the foot), in `.evt` beside each clip. `mh_m.mov_wf.evt` (walk): FootDown 0.30 and 0.90 s, MovementSound 0.33 and 0.93; `mh_m.mov_rf.evt` (run, 1.1667 s): FootDown 0.37 and 0.93, MovementSound 0.37 and 0.93.
- **The controller's own clip events** (handler `0xA6BFA0`, events 11-26; checked): 13 `Done` (and 11) -> the playing transition clip is over: `[+0x6E] = 0`, then `0xA6BF70`: mode 4 (start) -> 2 (moving), modes 8 and 0x10 -> 1 (idle); 23 `MvmtLock` -> `[+0x6E] = 1`; 24 `MvmtUnlock` -> `[+0x6E] = 0`; **25 `MvmtSetTransition` (string = an anim name): if the playing loop is in the run set `{70, 71, 72, 66, 65}` (0xB59A84) or the walk set `{56, 79, 78, 57, 52, 55, 54, 53}` (0xB59A98) and the mode is moving and the right branch (Walk vs Run weight, the same comparison both ways) is dominant, `[+0x60]` := the id of the named transition clip** (the stop clip to play next); 26 `MvmtSetOffset` (string, converts to a number; handler 0xA6C274, a clip offset; not followed). The run clip sets it three times a cycle with the foot that is down: `mov_rf.evt` events at 0.07 s `runpausefr`, 0.40 s `runpausefl`, 0.80 s `runpausefr` (each with MvmtUnlock at the same time, and MvmtLock at 0, 0.17, 0.6 s). The walk clip has none (a walk stop uses what was last set or the default).

#### 2. Start, stop, and turning in place

##### The mode machine of the anim controller [+0x54] (checked)

`0xA6BC40(newMode, arg)` sets the mode and runs the enter routine: 1 idle, 2 moving, 4 start (enter `0xA6AE30`), 8 stop (`0xA6AD50`), 0x10 turn in place (`0xA6AFF0`). Each frame (`0xA6BF00`): flags [+0x40] = 0 when the controller's state [+0x13C] is 0 or 3, else `0xA6A0B0`. Flags with 0x33 clear -> `0xA6BCC0`: mode 2 and state 3 -> mode 8 (stop), mode 2 otherwise -> mode 1 (idle, a plain crossfade: Motion 0, Transition 1, per `0xA69DF0`), mode 4 not locked -> mode 8, mode 1 non-combat with the turn bits (4/8) and state 4 -> mode 0x10, mode 8 and 0x10 -> 1 when the controller is idle again. Flags with motion -> `0xA6BD80`: modes 1, 8, 0x10 -> mode 4 (start); mode 4 with under 250 ms of the clip left (0xA5F980 < 0xFA) and a walk start while the target is running -> mode 4 again (the start of the run follows the start of the walk); then mode 2 moves the blend (above).
- **Global switch** 0x00C1CFAC (byte, 1 in the file, nothing writes it): start/stop are off when 0 (`0xA69BA0`, `0xA6AE30`). Also off in strafing ([+0x6F]).

##### The Transition node (checked)

`movement.anb`: roots `Motion` (as above) and `Transition` with 10 flag-1 leaves: `runpausefr` `mh_m.mov_rf_p_rfoot.ani`, `runpausefl` `mh_m.mov_rf_p_lfoot.ani` (2.267 s), `pauserunf` `mov_p_rf.ani` (0.2 s), `shuffle` `mov_p_shffl.ani`, `turnr90` `mov_p_r90trn_p.ani` and `turnl90` `mov_p_l90trn_p.ani` (both 0.8 s), `pauseturnl180runf` `mov_p_r180_rf.ani` (0.333 s, sic: the names are crossed), `pauseturnr180runf` `mov_p_l180_rf.ani`, `pauseturnl90runf` `mov_p_l90_rf.ani` (0.267 s), `pauseturnr90runf` `mov_p_r90_rf.ani`. `combat_movement.anb`: roots `Transition` (no children) and `Motion` (Run: runf, runleanr, runleanl; Walk: WalkBackward walkbl/walkb/walkbr, WalkForward walkfl/walkfr/walkf). Playing a transition (`0xA6A300(animId, rate)`): the clip is found by id on the active tree (`GetNode`), `Motion` weight to 0 and `Transition` to 1 at blend [+0x50], the clip started at rate 1.0 and blend 2.5; a missing node (combat: none) leaves the controller moving or idle without a clip.

##### Start (mode 4, `0xA6AE30`; non-combat, non-strafe) (checked)

- Moving backward (bit 2): anim 86 `pausewalkb`.
- Forward: `angle` = `AngleBetween`(move direction [+0xF0], current facing [+0x110]); `running = speed target [+0x138] >= Game_RunSpeed - 1e-6`; `left` = flag bit 4.
  - angle < 45: 85 `pausewalkf` (walk) or 84 `pauserunf` (run).
  - 45 <= angle < 157.5: 91/93 right 90 (run/walk), 92/94 left 90 (`pauseturn{r,l}90{run,walk}f`).
  - angle >= 157.5: 87/89 right 180, 88/90 left 180.
  - Constants floats 45.0 (0xB59A6C) and 157.5 (0xB59A70). The 180 and 90 cases also compute `angle/180` or `angle/90` into a local, which is then overwritten (dead).
- The clip's total root travel `T` = the length of its end translation (0xA390F0 + 0x451990); if the remaining distance [+0x130] is > 0 and < `T`, `AnimBase[+0xB4]` = `remaining / T`, else 1.0. `AnimBase[+0xB4]` is the **root-motion object's** (AnimBase+0x80, vtable 0xB59728) +0x34, the scale of the clip's root translation (its +0x38 is the scale of the root yaw, set by `0xA65D10(scale)`); so a start clip is shortened in travel, not played slower. Inferred: the creature moves by the controller, and the root motion is added for the transition; not followed.

##### Stop (state 3, mode 8, `0xA6AD50`) (checked, inferred where noted)

- The controller (`0xA64350`, path following) compares the remaining path distance [+0xE8] with the travel of the stop clip chosen by the events above (`0xA6ACF0`/`0xA636B0`): **when remaining <= the stop clip's root travel (+1e-6) it sets state 3**, speed 0, and the anim controller goes to mode 8: the clip id is `[+0x5C]`/`[+0x60]` (`0xA69BA0(1)`: the last MvmtSetTransition id, or the one played). The clip is played with `AnimBase[+0xB4] = max(0.25, remaining / clipTravel)` (0.25 is a double 0xAE7FE8), so the stop clip's travel ends on the destination.
- A stop without state 3 (e.g. a command cleared): mode 1, a crossfade to the idle (Motion 0, Transition 1 held by the idle), no clip.
- A stop clip finishes with `Done` (event 13): idle. Stop and start clips are not used in combat (the tree has no `Transition` clips).

##### Turn in place (state 4, mode 0x10, `0xA6AFF0`) (checked)

- Entry: the controller is idle (state 0) and `SetDesiredFacing` (0x00A638B0) was called with a direction more than 0.999999 degrees from the current facing (double 0xB596E0 = 0.999999), the animation record not busy (AnimBase +0xC0 = 0), sets state 4. The facing is the creature's own; `SetFacing`, `SetFacingPosition`, `SetFacingObject`, `CommandTurn` (sub-action 42 through `TurnToward`), the attack and ability turns (sub-action 39) all end in `SetDesiredFacing`; a placeable and an inactive creature snap (`dev/script_commands/posorient.md`).
- The anim controller reaches mode 0x10 only when **not in combat** and the flags have 4 or 8 (below).
- `angle` = `AngleBetween(desired [+0x100], current [+0x110])`. **If angle <= 30 degrees (double 0xAF7068) there is no clip** (back to idle path `0xA6BF70`). Else the clip is `anim 61 turnl90` if bit 4, `59 turnr90` if bit 8 (rows 58 and 60, the 180s, exist in anim_base but this code uses only the 90 clips, whatever the angle).
- Clip rate: `rate = min(maxRate, base * clipSeconds / angle)`, `clipSeconds` = `0xA38ED0(node)` ms * 0.001 (the clip is 0.8 s). `base` = **120** (float 0xB59A78) and `maxRate` 1.3 (0xB59AD0) in explore; 240 (0xB59A7C) and 3.0 (0xAE7130) if the combat flag is set. So explore: the clip plays at `96/angle` capped at 1.3, the turn takes `angle/120` seconds: 90 degrees 0.75 s (rate 1.067), 45 degrees 0.615 s (rate 1.3, capped), 180 degrees 1.5 s (rate 0.533).
- Root yaw: the clip's end rotation (0xA38F70 + 0x4EE310) `rot`; if |rot| > 1e-6 the root-yaw scale `0xA65D10(|angle / rot|)` is applied (the clip's total turn is made the angle needed, quaternion axis-angle times (scale - 1)); else the scale is left at 120 (the unused base).
- Ends with `Done` or when the controller leaves state 4: state 4 -> 0 when the desired facing is zero or within 0.999999 degrees of the creature's facing, and the animation record is free (`0xA6458F`...`0xA645FD`).

##### Turn rate of the facing (checked), `MovementController::0xA63B20`

Called every `Update` while state != 0 (states 1, 2, 3 and 4; with the strafe-lock [+0xF8], the desired facing is the direction to the target, `0xA63770`):
```
Δ = Angle::Wrap(ToAngle(desired) - ToAngle(creatureFacing), -180, 180)           (0x4EE570, 0x4EDD70)
if |Δ| <= 1e-6: nothing
T = 0.4 - 0.28 * |Δ| / 180                       (doubles 0xAE7128 = 0.4, 0xB596E8 = 0.28)   seconds
step = sign(Δ) * min(|Δ|, 180 * dt / T)          dt = frame ms * 0.001 (struct +0x30)
creature facing = rotate(creature facing, step)    (0x4EE200, 0x4CB6D0, normalise 0x451A30)
```
Effective rate `180/T` deg/s: 450 at ~0 degrees, 580 at 45 degrees, 692 at 90, 900 at 120, 1500 at 180. Because the step is recomputed from the remaining Δ each frame, a 90-degree turn takes about 0.13 s. A moving creature (state 1/2) gets its desired facing set to its move direction every update (`0xA64350`: `[+0x80] = normalise(move)`, `SetDesiredFacing([+0x80])`, which does not change the state when it is not 0), so the heading follows the path at this same limit; no other limit.

##### The turn (settled (corrected 2026-10-18 by lib-agent-2))

The turn is the fast logical turn. The drawn model follows the object's orientation (+0x130), and the turn clip's root yaw does nothing: these clips lack GFF 4008 `AnimationHasGobAnim`, so the root-motion object holds no clip (see the turn clips in "Movement, casting, hits and gestures" below). The turn clip is cut after about 0.15-0.25 s, when state 4 ends, with a 0.25 s fade (corrected 2026-10-18 by lib-agent-2). This replaces the earlier "not settled" point on whether the model shows the logical facing or the clip's root rotation.

#### Not read

- The meaning of the blend argument (4.0, 2.5, -1.0) inside the tree (`SetNodeWeight` 0xA605E0 -> the tree's virtual 0x48), the clamping of weights, and the cross-fade curve.
- `0xA6A9A0` (run lean) in full, the angle-to-weight map of strafing beyond its thresholds, `0xA69CC0` and `0xA69AD0` (it sets the phase of the walk cycle at mode changes, [+0x64]/[+0x68], inferred).
- The root-motion object (AnimBase+0x80) and its consumer; `AnimBase` +0xC0, +0x13C, +0x140.
- Who writes MovementController +0xD4/+0xDC (speed multiplier and offset; haste/slow would be the natural users), +0xF4 (direct move versus path), +0xF8 (strafe-lock) and the combat flag's setter (0xA631D0 is the setter of +0xF9).
- `Game_CbtStrFSpeed`'s exact column (hash 0x03186054 is computed from the code's constants, not looked up; the name fits `apr_base`).
- Whether `MvmtSetOffset` (26) shifts a clip's start; the cases of the chain in 0xA6A300 (how `AnimBase` virtual 0x58 starts a transition at the stored rate) and the mode 4 -> 2 handling when the start clip is cut.
- Quadruped/creature trees (c_*_expmvt.anb, c_*_cbtmvt.anb): only the human trees were opened; the code is shared.

### Animation layers on top of the base clip (lib-agent-2, prefix animlayer, 2026-10-18)

GOG exe `game\DAOrigins.exe` (base 0x400000), no Ghidra, no game. "checked" = read in disassembly or data; "inferred" = from what the code/data implies, not run. Scratch: `%TEMP%\lib-agent-2-animlayer-*` (decoder `lib-agent-2-animlayer-q.py`, dumps, extracted .anb/.ani under `lib-agent-2-animlayer-anb\`).

#### Short answers

1. **Overlay tree.** `OverlayTree` (apr_base; hashed column 0x4375DAC6, read by the appearance loader 0x006BDF30 into the same tree pool as ModelTree/MovementTree/OverrideTree, one call each to 0x006A1470) is just more nodes in the creature's flat tree pool. `humanoid_overlay.anb` holds 8 leaf nodes under root `humanoid_overlay`: `bdmg` (one-shot, override), `bodyoverlay`, `lookdown/lookup/lookcenter` (head), `lookeyesdown/lookeyesup/lookeyescenter` (eyes). Combination is decided by the clip's own flags (`AnimationIsAdditive` 4011, `AnimationIsOverride` 4012), not by the tree: look clips are **additive**, bdmg/arms/equip clips are **override**; the "mask" is the set of bones the clip keys. No mask in the .anb.
2. **Blinking.** `Animation::RandomWaveController<float>` is **not** blinking: it is the light flicker (ctor 0x008155F0, update 0x008156E0; dev/EFFECTS.md has it). Constants and formula below anyway. No code path that plays a blink was found. The blink data is `mh_o.eyelids.ani`, an additive 19.9667 s loop with 6 baked blinks of about 0.17 s; nothing references it (not an .anb, not an exe string) - see Not read.
3. **Look-at.** Yes. A per-creature 0x170-byte object (`CCCreature+0x768`, lazily made by 0x0069E9D0, ctor 0x006CA690) owns three `PartLookAtController`s (vtable 0x00B0F388): Eyes (+0x10), Head (+0x54), Body (+0x98), configured from the `look_at` table (m2da id 229, 3 rows: Eyes, Head, Body). Targets: the player (random glances), the attack target, a conversation partner, a script/cutscene target. The yaw is played as the **time** inside a 6.0 s additive clip, the pitch as **weights** of three clips (centre/up/down). Limits and speeds in section 3.
4. **Additive / partial.** `AnimationIsAdditive` (4011) -> ModelAnimation+0x2D, `AnimationIsOverride` (4012) -> +0x2C, `AnimationOverridePriority` (4013) -> dword +0x28, `AnimationIgnoreScale` (4040) -> +0x2F (loader 0x00815FB0). The model's per-frame build (0x0054D4B0 over 0x008149D0) sorts every active clip entry into three lists: normal (+0x208), additive (+0x218), override (+0x228). Breathing: no separate breathing layer found; additive clips in the game are the look poses, `eyejitter`, `eyelids` and the combat-`b_look*` body poses.
5. **Upper-body equip/unequip.** anim_combat 108 `equip` / 109 `unequip` (Type 16 / 32, DisableLookAt 1) resolve to the weapon-set overlay tree's `equip`/`unequip` nodes: override clips (priority 0, flags 1 = one-shot) that key only the arm chain (collar, shoulder, elbow, wrist, fingers, prop bones). The legs keep playing the movement clip; the mask is the bone set the clip keys. Played by 0x00696B50(row, force, 3.0f) -> `AnimBase` Play with vec4 (1.0, 0, 1.0, 3.0).

#### 1. The overlay tree

##### What is in it (checked)

`humanoid_overlay.anb` (994 bytes, global.rim; GFF4 "ANBT", node struct fields 4024 name, 4025 file, 4026 weight, 4027 flags, 4028 first child, 4029 number of children, 4030 parent; tree name 4022). Every node weight 1.0.

| node | clip | flags | length (s) | additive | override | keyed bones |
|---|---|---|---|---|---|---|
| bdmg | mh_o.rct_odmg_v1.ani | 1 (one-shot) | 1.30 | 0 | 1 | 45 channels: brows, cheeks, lips, jaw, eyeblink_l/r, lowlid_l/r, undereye, sneer, chest1, chest2, neck, neck1, head, headbase, mouthbase, eyelookbackat. A hit flinch of body and face. anim_combat row 103 "Damage" (Type 1024, DisableLookAt 1) |
| bodyoverlay | mh_o.arms.ani | 0 | 0.1333 | 0 | 1 | leftshoulder_rotation, rightshoulder_rotation; 2 keys, same value at both ends = a held arm pose. anim_base row 99 "body overlay" (Type 3, TransitionTo 99, Replacement 1: self-looping) |
| lookdown / lookup / lookcenter | mh_o.headlow / headhigh / headmid.ani | 0 | 6.000 | 1 | 0 | head, neck, neck1 rotation (181/179/181, 172/179/166, 181/181/177 keys) |
| lookeyesdown / lookeyesup / lookeyescenter | mh_o.eyelow / eyehigh / eyemid.ani | 0 | 6.0333 | 1 | 0 | eye_left, eye_right rotation (182 keys; eyemid right has 181) |

anim_base rows (checked, `lib-agent-2-anb\anim_base.csv`): 4 Look Center `lookcenter`, 5 Look Up `lookup`, 6 Look Down `lookdown`, 7/8/9 `lookbodycenter/up/down`, 12/13/14 `lookeyescenter/up/down` (all Type 2); 99 `bodyoverlay` (Type 3). The tree's node names are matched to these by AnimName (core Anb.h).

Other overlay trees (apr_base column values): `bowoverlay`, `cbowoverlay`, `staffoverlay`, `ow/ws/tw/dw overlay`, `humanoid_ranged_overlay` (3 look nodes: `mh_c.b_lookcenter/up/down.ani`). The weapon sets' overlay trees hold `equip`, `unequip` (flags 1) and, for bow/crossbow/staff, `lookbodycenter/down/up` (`mh_c.bw_look*.ani` for bow and crossbow, `mh_c.b_look*.ani` for staff). Body look clips: 3.0333 s additive, channels lowerback, chest, chest1, chest2 rotation (92 keys each).

##### How layers combine (flags checked, maths inferred)

- The tree pool is flat: each creature's trees (model, combat, movement, overlay, override) are loaded the same way by 0x006A1470 (flag 0xF, or 0x1F with the last argument 1 for the Override column) into one object's node list; nothing in the loader treats the overlay tree differently.
- Per frame, `GraphicsStructures::Object` vtable slot 49 (0x0054D4B0; the vtable at 0x00AF85DC) walks the active clip entries (0x34-byte records in the tree: weight float at +0xC, clip id word at +0x2C, parent node index at +0x18, active byte +0x32) through 0x008149D0. For each active entry with weight > 0 (weight is the product up the parent chain, checked 0x00814C2E..0x00814CB4):
  - if the clip's +0x2D (additive) is set: entry (scale vec3, weight, clip id) goes to the additive list at object+0x228 (0x00814B00-0x00814B6A; corrected 2026-10-18 by lib-agent-2, the two lists were swapped);
  - else if +0x2C (override) is set: to the override list at +0x218 (0x00814B84-0x00814BEE);
  - else it is a normal clip of the blend group, and per group the weights are divided by their sum (0x0054D6C1..0x0054D70E: `w[+0x24] /= total`) so normal weights sum to 1.
  - `AnimationIgnoreScale` (+0x2F = 1) makes the entry's scale (1,1,1) instead of the tree's.
- **The sampler** (read 2026-10-20 by lib-agent-2; checked unless said) is Object slot 51 (0x00549E30), and it settles what was inferred here:
  - normal clips blend into the base by their weights, as above;
  - overrides: W is the sum of their weights. W ≥ 1 replaces the base, and 0 < W < 1 lerps (translation) or slerps (rotation) toward their weighted average, on the bones they key alone. The average does not depend on their order, and their priority is never read;
  - additive clips last: v · w added to a translation, slerp(identity, q, w) multiplied onto a rotation (the multiplication's order inferred). Weights are used raw, not divided by a sum.
- **The flinch** (row 103) starts through `PlayAdditiveAnimation` (0x00659300 → 0x00696B50) with a blend rate of 5.0 (0xAE7308) toward weight 1, so 0.20 s to full. No fade-out was found [open].
- **The overlay draw (108) and sheathe (109)** are gated by 0x00694E20, which requires rows 100 and 109 (or 101 and 108) at weight 0.
- **Row 99, bodyoverlay:** no code that starts it was found.
- Override priority (4013) is 0 in every overlay clip read, so ordering among overrides was not needed.

#### 2. Blinking

##### RandomWaveController<float> is the light flicker (checked)

vtable 0x00B267C8 (slot 0 destructor 0x008155D0, slot 1 Update 0x008156E0); base ProceduralController 0x00B267BC. The only constructor call is in the lighting instance maker (0x0055FC5B, object+0x1BC = the flicker multiplier). Object layout: +4 float* target, +8 amplitude A (copy of `*initial`), +0xC f1, +0x10 f2, +0x14 f3, +0x18 prev, +0x1C next, +0x20 duration (double), +0x28 elapsed (double). The three floats come from the light node's IntensityVariation/cycle fields at node+0x148/+0x14C/+0x150.

Update(dt double) - elapsed += dt; while elapsed <= duration: t = elapsed/duration clamped to [0,1], s = t*t*(3 - 2t) (constant 3.0 at 0x00AF0C38), `*target = prev*(1 - s) + next*s`. When elapsed > duration: elapsed = 0; prev = next; draw two numbers from the CRT LCG (seed global 0x00C39778, `seed*0x343FD + 0x269EC3`, `(seed >> 16) & 0x7FFF`), each scaled by 2/32767 (double 6.1037019e-5 at 0x00AF7EB8) to u in [0,2]; duration = f2 + (u1 - 1)*f3; next = A*(1 + f1*(1 - u2)), i.e. A*(1 +/- f1). Writes `prev` to the target on the first call of a segment. DAOLib's `src/fix/LightFlicker.cpp` already corrects the jump per cycle (dev/EFFECTS.md). Not related to eyes.

##### What the data holds for a blink (checked)

`mh_o.eyelids.ani`: 19.9667 s, additive, 6 channels: eyeblink_left/right rotation (216/229 keys), eyeblink_left/right translation (37 keys, vec3), lowlid_left/right rotation. Both eyes blink together, 6 times per loop. The eyelid translation is non-zero (peak y = -0.00162) for these windows (s, first nonzero to last nonzero key, with one 1/30 s key of zero before and after):

3.166-3.266, 6.533-6.633, 7.133-7.233, 11.167-11.266, 13.833-13.933, 19.333-19.433

so each blink lasts about 0.17 s including its ramps, with the eyes closed about 0.1 s, and the gaps between them are 3.1, 3.4, 0.6, 4.0, 2.7, 5.9 s (and the loop wraps with 3.17 s after the last one). Keys are at 1/30 s (109.2 steps of 65535). `mh_o.eyejitter.ani` (9.9667 s, additive, eye_left 31 keys / eye_right 56 keys) is eye micro-motion. Neither is named by any tree, any exe string, or the 1,207 .anb files read; who plays them is not read.

#### 3. Look-at

##### Where it lives (checked)

- `CCCreature::GetLookAt` 0x0069E9D0 (lazy `new 0x170`, stored at creature+0x768, ctor 0x006CA690 given the creature; a second ctor 0x006CA800 for another owner type at 0x007CF7CF).
- Layout: +0 creature, +0x10 Eyes controller, +0x54 Head controller, +0x98 Body controller (each 0x44 bytes, ctor 0x007F5BC0 with the settings row from 0x007F6350 = `0x00C390E8 + 0x34 * row`, row 0 Eyes, 1 Head, 2 Body), +8/+9/+0xA "controller enabled" bytes (+0xB/+0xC/+0xD the alternate set, chosen by +0xE), +0xDC mode (0 off, 1 automatic, 2 returning to neutral), +0xE0 own eye position (16 bytes), +0xF0/+0x100/+0x110 orientation quaternions, +0x120 target position, +0x130 has-target byte, +0x134 neutral/offset quaternion (set by 0x006C9FE0 from a yaw and a pitch), +0x144 dirty, +0x148 milliseconds to the next glance decision, +0x14C..+0x158 four candidate object ids (slot 0 attack target [creature+0xC60 combat data +4], 1, 2, 3 = set by `CCCreature::SetLookAtTarget(objectId, slot)` 0x0069EAA0 / LookAt 0x006CA950; slot 1 is used by conversation lines (0x0068E678, 0x006FE7F6/0x006FE808) and cutscenes, slot 2 from 0x0070FE99, slot 3 (+0x158) is also the one the glance roll fills with the player), +0x15C quaternion target relative to the facing, +0x16C enabled byte (SetLookAtEnabled).
- Switches: `SetLookAtEnabled` (script 805, 0x0066BE50) -> 0x0069F590 area: true calls 0x006CAB00 (mode 1, automatic), false calls 0x006C9E60 (disable). 0x006CAB90 = reset to neutral: mode 2, one Apply with dt 9999, mode 0, controller states reset to -1, and mode 1 is restarted if it was 1. Global enable byte 0x00C03944 gates the whole Apply (0x006CA990). The update 0x006CAD80 returns at once if +0x16C is 0, if creature vtable +0x1D0 is true, or if 0x007C4970(creature) is false (that tests `creature+0x70` vcall +0x64 > 2, an animation level-of-detail style check, inferred).

##### The `look_at` table (checked, ghilana table 229, `look_at.gda`, 3 rows; hashes verified: the loader 0x007F5C40 reads these column hashes)

| ID | Controller | Center/Up/Down rows | Min/Max AnimationYaw | Min/Max AnimationPitch | MinSpeed | MaxSpeed | SlowAngle | FastAngle | MaxAllowedYaw | MaxAllowedPitch |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | Eyes | 12 / 13 / 14 | -45 / 45 | -45 / 45 | 360 | 360 | 10 | 25 | 20 | 10 |
| 1 | Head | 4 / 5 / 6 | -45 / 45 | -45 / 45 | 20 | 200 | 20 | 55 | 45 | 10 |
| 2 | Body | 7 / 8 / 9 | -45 / 45 | -45 / 45 | 96 | 96 | 10 | 25 | 45 | 30 |

Load (0x007F5C40, checked): settings struct (+0 centre row, +4 up row, +8 down row, +0xC MinYaw, +0x10 MaxYaw, +0x14 MinPitch, +0x18 MaxPitch, +0x1C MaxAllowedYaw, +0x20 MaxAllowedPitch, +0x24 MinSpeed, +0x28 MaxSpeed, +0x2C SlowAngle, +0x30 FastAngle). All the degree values are multiplied by pi/180 (double 0.017453279 at 0x00AF14C0), so speeds are rad/s, and SlowAngle/FastAngle are stored as **cos(angle/2)** (x pi/180 x 0.5, then `cos` via 0x008E6020) so they can be compared with a quaternion dot product. Then the Min/Max of Yaw and Pitch and MaxAllowed are ordered/clamped.

##### What target is chosen (automatic mode 1, 0x006CAD80, checked unless marked)

Each frame, with mode 1:
- If the creature is the player-controlled leader (id compare with 0x005D5CA0 of the world 0x00C327EC): no target.
- Otherwise a countdown `+0x148` (ms; dt x 1000, constant 1000.0 at 0x00AEBC00) runs; when it is <= 0 a decision is rolled with `rand()%100` (0x0053DF70(100)) and the `globals` table (m2da 105, column Value, hash 0x3FC13DE1, read with 0x00A2CD70(row, hash, default)):
  - row 2 `PERCENT_CHANCE_TO_LOOK_AT_PLAYER` = 70: if not looking at the player, the roll < 70 starts looking at the player (+0x158 = player id); if already looking, the roll < 35 (half) keeps looking, otherwise it looks away (+0x158 = -1). A candidate that is neither -1 nor the player is left alone.
  - the next decision comes after `rand % (globals[1] - globals[0]) + globals[0]` ms: rows 0 `MIN_TIME_FOR_LOOKAT_ACTION` = 1500, 1 `MAX_TIME_FOR_LOOKAT_ACTION` = 4000.
- The four candidate ids are then tried in order (list from 0x006CACD0 fills slot 0 from the creature's combat data/target); a candidate counts if the object exists, its `+0x7F0` is 0 and its squared distance to the creature's eye position is under the slot's threshold in the array at 0x00C35784: slots 0 and 1 are 65505.0 (no limit in practice, 0x00B07ECC), slot 2 = `globals[9]^2` = `LOOKAT_THRESHOLD_DISTANCE_PLAYERSELECT` 25 m, slot 3 = `globals[10]^2` = `LOOKAT_THRESHOLD_DISTANCE_NEARBYNPC` 5 m (initialised once, 0x006C9E80, called 0x005DC075). The first one that passes is the target (its eye position, 0x006CABF0, into +0x120).
- Turn speed multiplier (0x006C9F10(k): controller +8 = k, controller min/max speed = settings x k): on acquiring a target (no target before) `k = globals[3] + rand01*(globals[4] - globals[3])` = 0.75..1.0 (rows 3 `MIN_LOOKAT_HEADTURN_SPEED`, 4 `MAX_LOOKAT_HEADTURN_SPEED`); on losing a target `k = globals[5] + rand01*(globals[6]-globals[5])` = 0.4..0.6 (rows 5, 6 `MIN/MAX_LOOKAWAY_HEADTURN_SPEED`). rand01 uses the CRT LCG with seed 0x00C35780 and 1/32767 (double 3.0518509e-5 at 0x00AF7EB0), so `(r & 0x7FFF)/32767`.
- Globals rows 7 `MIN_DELAY_BEFORE_PLAYER_LOOKAT` (250) and 8 `MAX_DELAY_BEFORE_PLAYER_LOOKAT` (500) were not found used in this function (inferred: the delay before a creature turns when the player is selected / comes near); not traced.
- Explicit targets: `SetLookAtTarget` stores the ids; conversation (ShowLine), cutscenes (0x007F108A clears slot 1) and 0x0070FE99 (slot 2, the id at `[esi+0x9c]`) feed the same slots.
- Dialogue line effect is not separately read; dev/CUTSCENES.md says a cutscene's camera look-at is the separate camera behaviour (0x006C3010), not this.

##### From target to angles (0x006CA250 recompute, checked)

mode 1 and has-target: `d = normalize(target - eyePos)`; yaw = 0x004EE5A0(d), pitch = 0x004EE570(d); both **clamped to [-90 deg, +90 deg]** (floats at 0x00B10B8C = -90 and 0x00B00618 = +90); the quaternion `q(yaw, pitch)` (0x004EE200) is multiplied by the inverse of the creature's orientation +0xF0 and stored at +0x15C. Without a target: +0x134 (the neutral/offset quaternion, identity unless `0x006C9FE0`/`SetLookAt offset` was called). Mode other than 1: identity.

##### The controller (PartLookAtController::Update, 0x007F53E0(dt, targetQuat), checked)

- If the part has no clips (+0x1C = 0, i.e. the creature lacks the three rows) it just stores identity in +0x24.
- Clamp: the target rotation is limited so its angle does not exceed `limit = a*MaxAllowedYaw + (1-a)*MaxAllowedPitch`-shaped (settings +0x1C and +0x20 blended by `a`, the absolute dot of the rotation axis with the constant vector at 0x00C11C80..88 = (0, 0.0078125, 1); which of the two gets `a` is inferred); the clamp is by comparing |w| with cos(limit/2) and rebuilding the quaternion with that angle (0x004CBC40). Which of yaw/pitch gets which weight is inferred from the use of +0x1C/+0x20; the formula shape is read.
- Speed: `d = |dot(current, target)|` (0x004CB3B0); `t = 1 - (d - cosFast)/(cosSlow - cosFast)` clamped to [0,1]; `cand = lerp(minSpeed, maxSpeed, t)` (ctrl +0xC/+0x10); `speed = max(old speed, cand)` stored at ctrl +0x14 (so the speed only rises while a turn lasts; reset to -1 on a snap); step = speed*dt. So Head: 20 deg/s for a turn smaller than 20 deg (SlowAngle), up to 200 deg/s beyond 55 deg (FastAngle); Eyes 360 deg/s flat; Body 96 deg/s flat; all scaled by the multiplier above. (The cos(angle/2) comparison means the angle thresholds are of the full rotation.)
- If `step > pi/2` (double 1.5707963 at 0x00AFFE90) or `cos(step/2) >= remaining dot`: snap to the target; else rotate the current quaternion toward the target by `step` (axis-angle, 0x004CBC40, multiplied in with 0x004CB640).

##### From the quaternion to the clips (0x007F5730..0x007F59F0, 0x007F5A20, checked)

- yaw = 0x004EE310(q) * pi/180, pitch = 0x004EE370(q) * pi/180 (these return degrees in a pi/180-scaled convention; the exact units of 0x004EE3x0 were not decoded; the clamp below is against the settings in radians), each clamped to the table's Min/MaxAnimationYaw and Pitch.
- Pitch -> weights: `pitch > 0`: up weight = pitch / MaxAnimationPitch, down = 0; else down weight = pitch / MinAnimationPitch, up = 0; centre weight = 1 - the nonzero one.
- Yaw -> clip time: `u = (yaw - MinYaw)/(MaxYaw - MinYaw)` (0 at -45, 1 at +45), `time = u * L + 1/30` where `L = clipLength - 1/30` (field +0x20 = `length_ms*0.001 - 0.0333333` with the constants at 0x00AE7118 = 0.001 and 0x00B231B8 = 1/30; code 0x007F53C0). So the sampled time runs 1/30 s .. the clip's end; the clip's t=0 key is an identity placeholder never used. Centre of the sweep is 3.017 s of the 6.000 s clip.
- Every frame it calls the host (the animation host at ctrl+0x18; creature animation set) `SetWeight(row, w)` (0xA5C840, host vtable +0x88) for the three rows and `SetTime(row, t)` (0xA5C980, vtable +0x78) with the same t for all three; the three clips are started once (0x007F5A20: host vtable +0x58 Play with vec4 (0,0,1,-1) for the centre row and (0,0,0,-1) for the up/down rows; first component 0 = does not advance on its own, last -1 = no end, inferred meanings), and `+0x1C` records that the host `HasAnimation` for all three (0xA5D920).
- So the pose is: additive(centre clip at time t, weight 1-|p|) + additive(up or down clip at t, weight |p|), a pure pitch pose weight and a yaw sweep by time, per part.
- Checked in the clips: `headmid` head_rotation is identity at 3.0 s and rotates linearly about one local axis (about -32.8 deg at 0.05 s, 0 at 3.0 s, +26.1 deg at 5.97 s) while neck (-12.7 .. +16.1) and neck1 (-1.5 .. +2.0) add: total about -47.0 deg at the start and +44.2 deg at the end of the clip, i.e. the +/-45 deg of the table. `headhigh` adds head 28.1 deg, neck 16.9 deg, neck1 13.4 deg about the other axes at the centre; `headlow` head -20.1, neck -23.8, neck1 -7.2 (signed angles about z / y / z). Body: chest, chest1, chest2, lowerback.

##### Quaternion bone facts (checked)

The same three parts: Eyes - eye_left, eye_right; Head - head, neck, neck1; Body - lowerback, chest, chest1, chest2 (the body clips depend on the weapon set: `b_look*` and `bw_look*` in the weapon overlay trees; a creature with a tree lacking `lookbody*` has no body look, because +0x1C is 0 for that part).

##### Disabling (checked/inferred)

anim_base/anim_combat column `DisableLookAt` (hash 0x07EF3CC0, read by the table loader at 0x005CA693): 1 on draw/sheathe (100, 101, 108, 109, 110, 111, 177, 178), `Damage` (103), deaths and most reactions; 0 on idle, walk, talk, most `amb*`. Where the engine consults it was not read; the effect is inferred (a clip with 1 stops the look-at while it plays).

#### 4. Additive and partial clips

- Marker: in the clip (.ani, GFF4 "ANI V0.1", root struct): `AnimationIsAdditive` 4011 (byte), `AnimationIsOverride` 4012 (byte), `AnimationOverridePriority` 4013 (uint32), `AnimationIgnoreScale` 4040 (byte), `AnimationAnimLength` 4009 (float seconds). Loader 0x00815FB0 stores them in the ModelAnimation (+0x2D, +0x2C, +0x28, +0x2F, +8). Getters: `Animation::AnimationManager` vtable slots 18 (IsAdditive(id), 0x0080FEC0) and 19 (IsOverride(id), 0x0080FFA0) and the same two slots of `ModelAnimationTree` (0x00813EE0, 0x00813F70).
- No bone mask field exists in the .anb or the clip: the bones a clip keys are the mask (an override clip replaces only the channels it contains; an additive clip adds only to those).
- Additive clips found (all in `mh_o.*` and `mh_c.*`): the head/eye/body look poses (6.0, 6.03, 3.03 s), `eyejitter`, `eyelids`. Others not enumerated (could be done by scanning all 4,518 .ani for AnimationIsAdditive = 1).
- No breathing layer was found: a scan of the overlay trees shows none, and no table row says breathing (inferred: breathing is baked into the idle clips).
- anim_base columns (checked): Type 2 = look rows 4-14, Type 3 = body overlay 99. `Priority` is the existing column (Priority 1 on combat enter/exit rows; used for the command queue, not for this blend).

#### 5. The upper-body overlays (rows 108 / 109 and the draw/sheathe sets)

anim_combat (checked): 108 `equip` Type 16, 109 `unequip` Type 32, Priority 1, DisableLookAt 1, TransitionTo 1, Replacement 1; 100 `enter` / 101 `exit` (Replacement 110 / 111); 110 `unarmed_enter`, 111 `unarmed_exit`; 177 `quick_enter` / 178 `quick_exit` (Replacement 100 / 101).

Clips (checked, all flags 1 = one-shot, AnimationIsAdditive 0, AnimationIsOverride 1, priority 0):

| set | equip clip | unequip clip | length (s) | bones keyed |
|---|---|---|---|---|
| one-hand (ow) / two-hand (tw) | mh_c.ow_o_ent | mh_c.ow_o_ext | 1.1333 | right arm: collar, shoulder, elbow, wrist, 15 finger bones, prop01 (rotation+translation) |
| sword and shield (ws) | mh_c.sw_o_ent | mh_c.sw_o_ext | 1.1333 | both arms, props 01/02/03/18 |
| dual (dw) | mh_c.dw_o_ent | mh_c.dw_o_ext | 1.1333 (ext keys only the arm joints, 12 channels) | both arms, prop01/02 |
| bow | mh_c.bw_o_ent | mh_c.bw_o_ext | 0.9667 | left arm, prop02 |
| crossbow | mh_c.cbw_o_ent | mh_c.cbw_o_ext | 1.0667 | both arms, prop02 |
| staff | mh_c.stf_o_ent | mh_c.stf_o_ext | 0.6333 | right arm (19 channels) |

So the "bone mask" is a root-less list of the arm chain from collar down (never spine, never legs, never the root); the legs' movement clip keeps playing (override only replaces those bones).

Playing it while moving (checked, 0x006ABA40 / 0x006ABBF0 from dev/ANIMATION.md): `0x00696B50(row, force=0, blend=3.0f)`: it returns unless the row's type (0xA5E8A0) is 1 or 2; if force is 0 it also requires the row's current weight (0xA5F810) to be 0 (compare with 0x00B190A4); then it calls the animation set `Play` (vtable +0x58) with the vec4 (1.0, 0, 1.0, 3.0) plus two zero arguments. By the look-at use of the same call, the vec4's first component is the play speed (0 for a scrubbed clip), the third the start weight, the fourth the blend (3.0 here, -1 for a held clip); these meanings are inferred, not read at the callee (0xA5F860 passes them to the tree's `Play`, vtable +0x20, and then slot +0x48).
- The weights of the blend are not read at the end of the clip: the tree node flag 1 (one-shot) makes the clip stop; whether the 3.0 is a rate (1/3 s) or a time (3 s) was not read.
- DisableLookAt = 1 on 108/109 means the look-at is off while they play (inferred, see section 3).

`bdmg` (anim_combat 103 Damage, Type 1024, DisableLookAt 1, flinch): `mh_o.rct_odmg_v1.ani`, 1.30 s override over 45 face and chest/neck/head channels (list in section 1). The layers a hit adds to the base clip are this override, not an additive.

#### Not read

- The sampler that consumes the normal/additive/override lists (what "additive" computes in maths, how override weights fade a base bone), after 0x0054D4B0. Everything about additive/override maths is inferred from the keys.
- Who plays `mh_o.eyelids` / `mh_o.eyejitter`: not in any tree, string or table found; may be FaceFX (`FacialAnimation::*`, `FxActor`, `.fxa`) or the head's own mechanism. No blink code in the exe was found by name; the blink rate in play is not known (data: 6 per 19.97 s).
- The controller's exact clamp weights (MaxAllowedYaw vs Pitch by axis), the units returned by 0x004EE310/0x004EE370, and the slot meanings 1-3 of the candidate ids beyond slot 1 = conversation/cutscene and the 0x0070FE99 caller (slot 2).
- The creature's animation-level-of-detail check (0x007C4970) and how Disabled look-at rows are applied; `MIN/MAX_DELAY_BEFORE_PLAYER_LOOKAT` (globals 7, 8) use.
- The play vec4's callee meanings (speed, start, weight, blend), and the time base of the 3.0 blend.
- The other overlay trees of creatures (`c_*_ovr.anb`, 38 files carry `bdmg`): not dumped; their look clips and the other_creatures' `look_at` rows (only 3 rows exist: the same table is used for all creatures; non-humanoids with no look rows have no look-at).
- Breathing, and a list of all additive clips in the game.

### Idle and ambient variation: how it is picked and blended (prefix animidle)

Read 2026-10-18 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py --disasm`, a text disassembly (`%TEMP%\lib-agent-2-b2b-b-text.txt`), the tables and clips through ghilana, the scripts in `toolset\extract\scripts\`. No Ghidra, no game, not seen in play. "(checked)" = read in the disassembly, the data or the scripts. "(inferred)" = a reading not followed to the end. Addresses are GOG's. Builds on master's `dev/ANIMATION.md` (the PlayAnimation command, sub-action 37) and `dev/AI.md` (the AMBIENT_CONTINUE heartbeat).

#### Short answers

1. **The blend trees hold no random choice at all.** `humanoid.anb` is a flat list of 244 clips under one root with no node that has several weighted children (the only parents are movement trees: Run/Walk/Motion/Transition, which blend by speed). The base idle is node `pause` (anim_base row 1, `mh_m.p.ani`, a 9.967 s loop). The variation is five one-shot nodes `pausetwitch_v1..v5` (rows 640..644) that **code picks, not the tree.** There are two pickers, and both are separate from the other: (a) the engine's own idle twitch in `CCCreature` (0x006AB6E0 + 0x006A01E0), uniform `rand() % 5` + 640, on a countdown of 12 / 10 / 6 s plus up to 50 %, for any creature standing in `pause` with sheathed weapons, party members included; (b) the scripts' ambient system (`sys_ambient_h.nss`, `ambient_anim_patterns.gda`), only for NPCs that have `AMBIENT_SYSTEM_STATE` bit 0x01.
2. **Explore idle** = `pause` in the ModelTree `humanoid.anb` (`mh_m.p.ani`). **Combat idle** = `pause` in the weapon style's tree (OW/WS `mh_c.sw_rdy.ani` 2.0 s, TW `mh_c.tw_rdy.ani` 1.033 s, DW `mh_c.dw_rdy.ani` 1.967 s, Bow `mh_c.bw_rdy.ani` 4.967 s, Staff `mh_c.stf_rdy.ani` 1.6 s). The switch is `CCCreature::0x006A7F30(combat)`, trees swapped at blend 3.0. Nothing in the engine or the scripts plays the combat trees' own `pausetwitch_v1` (WS/TW/DW).
3. **Ambient scripts** pick with `Random()` and play with `CommandPlayAnimation(anim, loops>=90 ? -1 : loops, bPlayNext, bBlend)`. **The pause entries in a pattern ("1.02") have one less loop than written** because the float is truncated (see 3.4).
4. **Followers do vary:** the engine twitch applies to a party member (countdown 12,000 ms + rand % 6,000 while idle in explore with sheathed weapons). The scripts' ambient system never touches them (`IsPartyMember` returns early; `wrappers_h.nss`:371 sets their `AMBIENT_SYSTEM_STATE` to 0).
5. **Blend:** the number passed to the animation play call is a **rate, fade time = 1 / blend seconds** (inferred from 3 consistent sites). The engine twitch starts the pause **0.4 s before the twitch clip ends** at blend 2.5; a script `CommandPlayAnimation` starts a clip at blend 2.5 (3.0 when `bBlendIn`... see 1.4; -1 when `bBlendIn` is false). The pairs in `humanoid.tbi` (BlendCurves) are 7 special ones and none involves a twitch.

#### 1. A standing creature's idle

##### 1.1 The tree data (checked)

- `.anb` is GFF4 "ANBT V1.0". Fields from `src/dao/core/Anb.h`: node list 4021, name 4024, clip file 4025, weight 4026, flags 4027, first child 4028, child count 4029, parent 4030. `AnimationTreeNodeWeight` is 1.0 on every node of the trees read.
- `humanoid.anb` (ModelTree of apr_base rows 1, 2, 3, 15): 244 nodes, a single root `humanoid` (index 244, parent -1) whose 244 children are all leaves. Node 223 `pause` -> `mh_m.p.ani`, flags 0 (a loop). Nodes 222, 221, 220, 219, 213 are the twitches, flags 1 (once):

| Node | anim_base row | Clip | Clip length |
|---|---|---|---|
| pausetwitch_v1 | 640 | mh_m.mov_tw_v1.ani | 5.9333 s |
| pausetwitch_v2 | 641 | mh_m.mov_tw_v3.ani | 3.1667 s |
| pausetwitch_v3 | 642 | mh_m.mov_tw_v4.ani | 3.1333 s |
| pausetwitch_v4 | 643 | mh_m.mov_tw_v9.ani | 3.3333 s |
| pausetwitch_v5 | 644 | mh_m.mov_tw_v2.ani | 6.8000 s |
| pause | 1 | mh_m.p.ani | 9.9667 s |

  Lengths are `AnimationAnimLength` (GFF4 id 4009) of the clips. anim_base rows 640-644: `Type` 16388 (0x4004, bit 0x4 and 0x4000), `TransitionTo` 1, `Replacement` 1, `Priority` 0, `DisableLookAt` 0. Row 1: `Type` 1 (loop), `TransitionTo` 1, `Replacement` 1.
- No code tests `Type` bit 0x4000: the only helper that reads the row's Type for a bit (0x005CA610(id, mask)) is called with mask 1 (looping, 0x00695390 and 0x00A5E9AA/C6) and mask 2 (0x0069B446, the overlay path). So 16388 is data with no meaning in the exe (checked as far as the callers of that helper go).
- Apr_base rows 1/2/3/15 name the same trees: CombatModelTree `combat_humanoid.anb`, MovementTree `movement.anb`, CombatMoveTree `combat_movement.anb`, OverlayTree `humanoid_overlay.anb`, BlendCurves `humanoid.tbi`; row 3 (dwarf) adds AmbientOverride `dwarf.anb`.
- A creature whose trees lack the twitch node plays the row's `Replacement`, which is 1, so the pause (rows 640-644; the `Replacement` path 0x0069B52A..., inferred from the PlayAnimation notes).

##### 1.2 The engine's idle twitch (checked unless marked)

**Where:** `CCCreature` vtable slot 87 (the per-tick idle update), address **0x006AB6E0**. It returns early unless every one of these holds, in this order:

1. The creature is **not in the combat state** (0x006A2580 false: `[[+0xC60]] != 1`).
2. Its **current base animation is row 1, `pause`** (0x006956B0 == 1).
3. It has **no current command** (0x0073C600 null).
4. **AI level** `[+0x2B0]` (GetAILevel): if it is below 2, a party member must be within 50 m on the ground (0x00630120 on the creature's position `+0x120`, squared distance 2,500 from 0x00B0FC58). At 2 or more there is no distance test.
5. A **countdown** `[+0x7E0]` (ms), reduced by the update's elapsed time (`[[+0x2B8]+8]`) on every call that gets this far, must be <= 0.

When the countdown has run out the function (a) reloads it (0x006A59D0(0), below) and (b) twitches only if all of these are also true: the creature has **no active ability whose `abi_base.BlendTree` column (hash 0x70F463FA) is > 0** (0x0071A530 over `[+0x7C4]`'s list at +0xFC, false); 0x006A8580(0,0) returned false (the weapons-drawn state is already what `WeaponsDrawn` says; `dev/ANIMATION.md`); and **no weapon is drawn** (0x007C34C0 on `[+0xC60]` false). If the byte `[+0xB29]` is set, it first queues PlayAnimation 111 `unarmed_exit` with flag 3 (what +0xB29 is was not read).

**What it queues:** the id from **0x006A01E0** ("the twitch id"): it reads the game mode (`[[0x00C327EC]+0x114]` -> 0x00640060 -> `[[+0x74]+0x5C]`); modes 0 Explore, 1 Combat and 3 GUI give **`640 + rand() % 5`**, any other mode (2 Dead, 4 Dialog, 5 Cutscene, loading and the rest) and none give 0 and no twitch. `rand()` is MSVC's (0x008E6A8E), so the five are uniform (a slight bias from RAND_MAX 32767 % 5). It then queues a type-14 PlayAnimation command through `QueueNewCommand` (0x0073D000, back of the queue, not static, player-issued 0) with:

| Parameter | Value | Meaning |
|---|---|---|
| int 0 | 640..644 | the twitch |
| int 2 (nLoops) | -1 | ends with COMMAND_LOOPING (2) as soon as the play-next step has started (see 1.4) |
| int 5 | 400 | the lead: the command's play-next step starts when 400 ms of the twitch remain |
| flag 4 (play next) | 1 | |
| flag 7 | 1 | the sub-action's byte +4 = (flag 7 == 0) = 0: **interruptible**, a soft ClearAllCommands removes it |

**The countdown, 0x006A59D0(initial):** it reads **globals.gda (table 105) column `Value` (hash 0x3FC13DE1)** through 0x00A2CD70(row, hash, default):

| globals row | Label | Value (ms) | Used when |
|---|---|---|---|
| 12 | IDLE_TIME_PARTY_MEMBERS_MS | 12000 | the creature is in the party (0x00631830 on the party object from 0x006A2CC0) |
| 13 | IDLE_TIME_HOSTILES_MS | 6000 | not party and hostile to the player (0x006A2B80) |
| 14 | IDLE_TIME_FRIENDLIES_MS | 10000 | not party, not hostile |
| 15 | IDLE_TIME_GUI_MS | 5000 | the game mode is 3 (GUI), for everyone |

Value `v` -> `[+0x7E0] = v + rand() % ftol(v * 0.5) + initial` (0.5 from 0x00C021D0). So **party 12,000-17,999 ms, hostiles 6,000-8,999 ms, friendlies 10,000-14,999 ms, GUI 5,000-7,499 ms.** It runs at: the end of the idle function, with initial 0 (checked); the **shared command end** (0x00746A90, initial 0), so any command ending restarts the wait (checked); and the creature's load (0x006C2118) with `initial = rand() % 5000` for a non-hostile and `-6000` (= -row 13) for a hostile, so a spawned hostile can twitch within 0-3 s and a friendly staggers over 0-5 s extra (checked).

Because step 5 only counts down on updates that pass steps 1-4, the time is time spent standing idle in `pause`, out of combat.

##### 1.3 The GUI paperdoll has its own twitch (checked)

The inventory/character paperdoll (the update at 0x007B4BB0, called from 0x007343D2 and 0x0073BF72, both classes' paperdoll updates) counts a float `[+0x1CC]` down by dt (not when `[+0x144] == 1`), and at <= 0 plays `640 + rand() % 5` on its creature through slot 44 with speed 1.0, **blend -2.0**, then adds `10.0 + (unsigned)(rand() % 40)` seconds (doubles 0x00AE7D48 = 10.0; 4294967296.0 at 0x00AE095C is the unsigned fix-up). So 10-49 s between twitches there.

##### 1.4 How a twitch is played and blended (checked for the numbers, inferred for the meaning)

Sub-action 37 (0x007A4390), `dev/ANIMATION.md`:

- Start: the clip is played with `slot 44` (`CCCreature` vtable +0xB0, 0x006AA7E0 -> 0x0069B3D0) as `(anim, speed 1.0, blend, 1)` with **blend 2.5** (0x00B03B08); **-1.0** when int 7 = 2 (bBlendIn false); 3.0 when int 7 = 1 (0x00AE7130, nothing found sets it).
- At the end of the one-shot (`int3 = 0`), with `next` true: it plays `t` = the row's TransitionTo (1 for the twitch rows) **if int 5 > 0 or t differs from TransitionTo**. For the engine twitch int 5 = 400, so it plays row 1 at speed 1.0 and **blend = 1000.0 / int5 = 2.5** (0x007A47C5-0x007A47F0: `fild int5; fdivr 1000.0`). It then keeps t's loop for `length * nLoops` (negative), and the next evaluation answers 2 (COMMAND_LOOPING). So the engine twitch ends the command 0.4 s before the twitch clip's end with the pause already fading in; the pause keeps looping on the creature.
- 2.5 per second = 0.4 s, which equals the lead of 400 ms exactly, so **blend is a fade rate and the fade time is 1 / blend seconds** (inferred; the animation layer behind it, 0x00A5FCC0 and 0x00A5F9C0, was not followed to the weights).
- **Blend -2.0** (the exit of a play-next step and the paperdoll) means "fade over 20 % of the clip": 0x0069B3D0 turns a blend <= -2.0 into `1000.0 / 0x00694FE0(StepLength(anim), blend)` and 0x00694FE0(ms, -2.0) returns `ms * 20 / 100` (checked for -2.0; the code has a 5 % case for -3.0, not used here). For `pause` (9.967 s) that is a 1.99 s fade-in. The same 20 % is cut off the command's duration when flag 3 is set (0x007A44B5).
- **Speed jitter on `pause`:** 0x0069B3D0 (reached with HasAnimation true and the animation set present) does, **when the animation is 1**, `speed += (rand() % 90) / 100.0 - 0.45` (0x0069B473-0x0069B4A8; 100.0 at 0x00AED110, 0.45 at 0x00B0F120), so a pause started through this path plays at **0.55 to 1.44** (steps of 0.01) of normal speed, each time. This is on every PlayAnimation of row 1, including the engine twitch's return to the pause, the scripts' "1.0x" steps and a play-next exit, so identical neighbours drift apart. A pause the tree enters on its own (a move command ending, TransitionTo) is not touched (no code path found, inferred).
- **Blend curves:** `humanoid.tbi` ("TBI V1.0", list of AnimationBlendcurve) has 7 entries, all about casting or a berserk/shield-bash getting up, to or from `shared.pause.ani`: `m.ua_cast_end` -> pause and `m.ua_dcast_end` -> pause (data 0, 1; 0.108, 1; 0.325, 0), `shared.berserk` -> pause and `ws.talent_shieldbashgetup` -> pause (0, 1; 0.15, 1; 0.45, 0), pause -> `m.ua_aconjure_start` and pause -> `m.ua_dconjure_start` (0.108 / 0.325). None involves a twitch (checked). How the exe picks a curve over the plain rate was not read.

#### 2. Combat idle and explore idle

##### 2.1 Which rows (checked)

- There is one idle row, **1 `pause`**, in every case. Which clip it is depends on which tree is searched first.
- **Explore:** the creature's ModelTree `humanoid.anb`, `pause` = `mh_m.p.ani`.
- **Combat:** the weapon style's tree (the trees `dev/ANIMATION.md` lists: OW, WS, TW, DW, Bow, Cbow, Staff), whose `pause` is a ready stance:

| Tree | `pause` clip | Length | Twitch node |
|---|---|---|---|
| OW.anb | mh_c.sw_rdy.ani | 2.0 s | none |
| WS.anb | mh_c.sw_rdy.ani | 2.0 s | pausetwitch_v1 = mh_c.sw_rdy_misc.ani (2.1 s, flags 1) |
| TW.anb | mh_c.tw_rdy.ani | 1.0333 s | pausetwitch_v1 = mh_c.tw_rdy_misc.ani (2.3 s) |
| DW.anb | mh_c.dw_rdy.ani | 1.9667 s | pausetwitch_v1 = mh_c.dw_rdy_misc.ani (1.5 s) |
| Bow.anb | mh_c.bw_rdy.ani | 4.9667 s | none |
| Staff.anb | mh_c.stf_rdy.ani | 1.6 s | none |

  The combat trees' `pausetwitch_v1` is reachable only by the row 640 (they are the same AnimName). **No code plays it:** the engine twitch needs "not in combat" and "no weapon drawn", and no script plays 640 outside the ambient system, which is off in combat (`Ambient_DoSomething`: `GetGameMode() == GM_COMBAT || GetCombatState`). So the WS/TW/DW twitch nodes are unreachable (the engine twitch would never choose them, as the weapons are sheathed when it runs).
- `combat_humanoid.anb` has only `unarmed_enter` / `unarmed_exit`; `combat_movement.anb` and `movement.anb` hold the walk/run/turn blends and no `pause` (`pause` is not in them, checked by node name).

##### 2.2 The switch (checked what is read, rest inferred)

`CCCreature::0x006A7F30(bool combat)` (called from the enter/leave-combat code and the draw/sheathe code of `dev/ANIMATION.md`): it calls 0x006A6D90(this, 3.0) and 0x006A6F60(this, 3.0) (the 3.0 is 0x00AE7130: the movement and base trees' blend rate, 0.33 s by the rule in 1.4), sets bytes 8 and 9 of the combat data (0x0069E9D0) to 1 for explore and to 0 for combat, reads three apr_base strings (+0xC, +0x28, +0x7C of the row), and **adds the combat trees to the animation set with priority 0x7FFFFFFF for combat** (0x00A60810(tree, 0x7FFFFFFF)) and with priority 0 (and the weapon trees) for explore. Because the tree set looks for a node from the highest priority down, `pause` is then the first one in the combat set. Not read: which tree goes where, and 0x006A6D90/0x006A6F60's bodies.

When a creature stands in combat (state 1) the engine twitch is off and nothing else varies the stance; the combat `pause` just loops.

#### 3. The ambient system in the scripts

##### 3.1 The table `ambient_anim_patterns.gda`, table id 170 (`TABLE_AMBIENT`) (checked)

Columns: `ID`, `Label`, `NoPlayNext` (s32), `ActionTotal` (s32), `Action0..Action12` (f32). 134 rows readable (rows 0-131, 1000, 1001; three add-ins' extra rows are enciphered). An action `A.LL` is animation id `A` (anim_base / anim_combat / anim_placeables row) and loops `LL`, 99 meaning forever; a **negative** action is a placeable interaction (`InteractWithObject(creature, nearest placeable, abs(A), 1, loops, ...)`); zero ends the list. Rows relevant to the question:

| ID | Label | NoPlayNext | ActionTotal | Actions |
|---|---|---|---|---|
| 1 | ambient_twitches (the default) | 0 | 8 | 1.02, 640, 1.03, 641, 1.02, 642, 643, 644 |
| 2 | ambient_relaxed | 0 | 1 | 624.99 |
| 4 | wardog_lazy | 0 | 7 | 640, 624.25, 641, 1.02, 640, 641, 1.03 |
| 5 | guard_patrolling_1 | 0 | 4 | 632.01, 1.03, 632.01, 1.04 |
| 6 | guard_patrolling_2 | 0 | 3 | 1.04, 632.01, 1.06 |
| 11 | listener_passive_1 | 0 | 8 | 800.15, 1.04, 848.08, 1.05, 640, 1.03, 642, 643 |
| 13 | idle_chatter | 0 | 6 | 816.01, 808, 3018.01, 810, 3016.01, 847 |
| 24 | guard_wander_left_right | 0 | 8 | 838, 1.02, 839, 848.09, 640, 642, 644, 800.08 |
| 28 | guard_wander_right | 0 | 6 | 838, 848.09, 640, 642, 1.02, 644 |
| 30 | darkspawn_wander | 0 | 8 | = row 1's list |
| 67 | bored_loitering_1 | 0 | 11 | 3029, 644, 3002, 640, 1.02, 641, 800.12, 642, 998.08, 643, 1.03 |
| 73 | bored_wander_1 | 0 | 11 | 644, 996, 640, 1029, 641, 997, 800.12, 642, 1002, 643, 1.03 |
| 97 | wandering_slob | 0 | 10 | 1.02, 640, 1.03, 994, 641, 996, 1.02, 997, 643, 3002 |
| 103 | bored_stationary | 1 | 10 | 641, 800, 851, 802.02, 853, 802.02, 801, 644, 643, 642 |
| 113 | bored_stationary_2 | 0 | 12 | 640, 823, 641, 1.01, 814, 642, 1.01, 972, 643, 1.01, 813, 644 |

(Full list: 134 rows read through `table 170`; the loops in the table are as designed, see 3.4 for what the VM computes.) Single-action rows (one loop, 99 = forever) are the "stand in this pose" patterns such as 32 `praying` (650.99), 23 `sit_on_ground` (921.99), 19 `arms_crossed` (802.99), 25 and 34 `hands_behind_back` (844.99, 848.99), 56 `relaxed` (625.99). `NoPlayNext` = 1 on the long poses and sequences (7, 9, 33, 35, 56, ...).

##### 3.2 `sys_ambient_h.nss` (`_Systems\_Includes\sys_ambient_h.nss`) (checked)

Creature locals (`var_constants_h.nss`:48-53): `AMBIENT_SYSTEM_STATE` (bitmask: 0x01 ENABLED, 0x02 SPAWNSTART, 0x04 NOPLAYNEXT, 0x08 NOBLEND, 0x10 ALWAYSON, 0x40 RUNNING), `AMBIENT_MOVE_PATTERN`, `AMBIENT_MOVE_PREFIX`, `AMBIENT_ANIM_PATTERN` (the row), `AMBIENT_ANIM_FREQ` (float `x.y`; -1.0 = all in order; 0 = none), `AMBIENT_ANIM_STATE` (internal).

Constants: `AMBIENT_SMALL_DELAY` 0.1 s, `AMBIENT_RESUME_PERIOD` 15.0, `AMBIENT_PAUSE_PERIOD` 30 (s), `AMBIENT_LOOP_FOREVER` 90, `AMBIENT_ANIM_FREQ_RANDOM` 3.5, `AMBIENT_ANIM_PATTERN_TWITCHES` 1.

Entry points: `Ambient_SpawnStart` (creature_core.nss:492 on spawn, 1325 `rules_core.nss`: EVENT_TYPE_OBJECT_ACTIVE) -> `Ambient_DoSomething`. `rules_core.nss`:802 calls `Ambient_CommandComplete(type, status)` at the end of every command-complete event; `creature_core.nss`:680 `EVENT_TYPE_AMBIENT_CONTINUE` calls `Ambient_DoSomething(OBJECT_SELF, IsObjectValid(GetEventObject(ev, 0)))`. `Ambient_Start*` from area scripts.

**`Ambient_DoSomething`** (line 980): returns if the creature is inactive, dead, a party member, or ENABLED is clear; returns if a current command or a queued command exists; unless ALWAYSON, returns if `GetGameMode() == GM_COMBAT` or `GetCombatState(creature)`, and when `Ambient_TimerExpired` (the `GetLowResTimer()` of the last party-near event is more than 30,000 ms old; the AMBIENT_CONTINUE with a party member as object 0 refreshes it, `bUpdateTimer`); then `Ambient_DoCommand`, then `Ambient_ReachedDestination`, then `Ambient_Animate` if `AMBIENT_ANIM_STATE != 0` else `Ambient_Move`.

**`Ambient_Animate`** (line 678): pattern = the override or `AMBIENT_ANIM_PATTERN`; freq = the override or `AMBIENT_ANIM_FREQ`. **With no pattern (<= 0) and no move pattern, it falls back to pattern 1 with freq 3.5** (line 720-727) and writes them to the creature. At the start of an animation phase (`AMBIENT_ANIM_STATE == -1`): freq < 0 -> `nAnimsToDo = ActionTotal` (play all, in order); else `nMin = FloatToInt(f)`, `nMax = FloatToInt((f - nMin) * 10)`, **`nAnimsToDo = Random(abs(nMax - nMin)) + Min(nMin, nMax)`**. For the default 3.5: nMin 3, nMax 5 -> `Random(2) + 3` = **3 or 4** actions (`Random(n)` is 0..n-1). Each call plays one action and decrements; at 0 the phase ends and `Ambient_Move` (movement pattern 0: go straight to a new animation phase with `AMBIENT_ANIM_STATE = -1`).

Choice of the action: ordered mode, action index `ActionTotal - nAnimsToDo`; random mode, **`Action<Random(ActionTotal)>`: uniform, with replacement** (so in pattern 1, 5 of 8 slots are twitches: each of 640..644 is 1/8, the pause 1.02 is 2/8 and 1.03 1/8).

Play: `fAction > 0`: `nAnimation = FloatToInt(fAction)`, `nLoops = FloatToInt((fAction - nAnimation) * 100)`; `bBlend = !(STATE & NOBLEND)`; `bPlayNext = !(STATE & NOPLAYNEXT)` and then `!NoPlayNext` of the row; **`bPlayNext = TRUE` always when `nAnimation == 1`**. **`WR_AddCommand(oCreature, CommandPlayAnimation(nAnimation, nLoops >= 90 ? -1 : nLoops, bPlayNext, bBlend))`** (line 825); then STATE |= RUNNING when nLoops > 0. A negative action: `InteractWithObject(creature, nearest placeable, interaction, 1, loops..., bPlayNext, FALSE)`.

**`Ambient_CommandComplete`** (line 1229): not for party members or a creature in combat state; for Interact and PlayAnimation commands, status COMMAND_LOOPING (2) returns (RUNNING stays set, so the engine heartbeat is suppressed); COMMAND_SUCCESSFUL clears RUNNING. It then calls `Ambient_DoSomething()`, which starts the next action the moment the command ends: **there is no gap between actions** (`AMBIENT_SMALL_DELAY` 0.1 s is only used in the move phase).

##### 3.3 The engine's side of the ambient system (checked)

- There are **no ambient-animation strings** in the exe (`--strings ambient` gives only FMOD categories, the `AmbientDialog` commands and `ObjectNodeAmbientLight`); the engine knows nothing of patterns.
- The engine's part is `EVENT_TYPE_AMBIENT_CONTINUE` (1026), `dev/AI.md`: raised by `CCCreature::Update` for each non-follower with ENABLED and not RUNNING, every 3,000 ms of its accumulator `+0xC84`, when a party member is within 50 m on the ground (the event's object 0 is that member); and at the end of a conversation or staged scene (object 0 = the creature). The script reacts with `Ambient_DoSomething`, which does nothing if a command exists, so for a creature busy with its pattern the heartbeat only matters to restart it after it stalled (a combat, a conversation, no party for 30 s).
- A creature enabled for ambient but with an empty queue between two actions is also a candidate for the engine twitch (1.2), but the command-complete -> `Ambient_DoSomething` call is synchronous, so in practice the countdown never reaches a quiet creature (inferred).

##### 3.4 What the VM makes of the table's loops: they are one less (checked by emulating float32, inferred that the VM keeps float32)

`FloatToInt` truncates (`dev/script_commands/conversions.md`: `_ftol2`, toward zero). The table stores float32, and `(fAction - nAnimation) * 100` is a float32 computation, so `1.02` (the float 1.0199999809) gives 0.0199999809 * 100 = 1.99999809, which truncates to **1**, not 2. Checked by `struct`-rounding each step:

| Table value | Loops the script derives |
|---|---|
| 1.01 | 0 |
| 1.02 | 1 |
| 1.03 | 2 |
| 1.04 | 3 |
| 1.05 | 4 |
| 1.06 | 5 |
| 640.01 | 1 (float32 640.01 is above) |
| 619.05 | 4 |
| 624.25 | 25 |
| x.99 | 98 (>= 90 -> -1, forever) |

Whether a value lands one low or exact depends on how float32 rounds that decimal (the `.01`, `.02` of the pause rows are low; 640.01 is high). The sleeping and praying `.99` rows are 98-99 either way and so forever.

Durations, using `dev/ANIMATION.md`'s PlayAnimation timings (length of the clip, a play-next tail of `StepLength(TransitionTo)` when `bPlayNext` and nLoops >= 0; both checked there):

- **A `pause` step "1.0k"** (nLoops = k-1 after truncation), play-next forced on: duration = `9.9667 s * nLoops`, then the exit step (row 1's TransitionTo is 1) adds the step length of the pause, **9.9667 s** more, played with blend -2.0. So 1.01 holds ~9.97 s, 1.02 ~19.93 s, 1.03 ~29.9 s, 1.04 ~39.9 s, 1.05 ~49.9 s (inferred from the notes' quirk list; not seen in play).
- **A twitch step "64x"** (nLoops 0, play-next on for pattern 1, since `NoPlayNext` is 0): the clip's length (3.13-6.8 s), then the 9.9667 s pause tail: 640 ~15.9 s, 641 ~13.1 s, 642 ~13.1 s, 643 ~13.3 s, 644 ~16.8 s per step (same inference). With `NoPlayNext` 1 (rows 103 and the poses) a twitch step is only the clip.
- The ambient twitch therefore does not use the engine twitch's 0.4 s lead (the scripts' int 5 is 0 and the exit is blended at -2.0, a 20 % fade, 1.99 s for the pause).

#### 4. Party members and followers standing in explore

- **Scripts:** none. `Ambient_Start`, `_SpawnStart`, `_DoSomething` and `_OverrideBehaviour` return for `IsPartyMember`; `wrappers_h.nss`:371 (the hire code) sets `AMBIENT_SYSTEM_STATE` to 0; the engine's heartbeat skips followers.
- **Engine:** yes, the twitch of 1.2, with the "party member" countdown of **12,000 ms + rand() % 6,000** (globals row 12), uniform over rows 640..644, interruptible (flag 7), only in game modes explore, combat (when the creature's own state is not combat) and GUI, with weapons sheathed and the current animation `pause`. The party leader that the player moves is a party member too (the same function runs for any creature; the player gives commands, so the countdown only runs while it stands with an empty queue). Followers that follow have no command while standing (`dev/PARTY.md`, the follow update 0x00760DC0 returns true for an idle follower), so they qualify. After a combat the weapons are sheathed 1,000-2,999 ms after the combat state turns off (`dev/ANIMATION.md`) and only then can a twitch start.
- The first twitch after standing still comes after the countdown last reloaded: a command ending reloads it (0x00746A90), so after a move order the first twitch is 12-18 s after the move ends.

#### Not read

- The animation layer behind `0x00A5FCC0` / `0x00A5F9C0` / the tree's vtable +0x58/+0x5C: how blend (rate 2.5) turns into weights over time, and how a curve of `humanoid.tbi` overrides it. The "fade time = 1 / blend" is inferred from 0x007A47C5 (blend = 1000 / lead ms) and the 20 % rule of 0x00694FE0.
- Whether `0x006A6D90` and `0x006A6F60` (3.0 blend) are the base-idle fade and what 0x006A7F30's tree table does beyond the priorities.
- `CCCreature +0xB29` (queues `unarmed_exit` before a twitch), `+0x7C4`'s list meaning beyond "abilities with BlendTree", and AI level defaults for creatures (the proximity test 4 applies only below 2).
- Whether the VM really rounds every float operation to float32 (I assumed yes in 3.4).
- Playing the idle path in the game (no play check; the countdown and its table values are read, not seen).
- A table dump of rows 0-131 beyond the ones listed (the table is `table 170` in ghilana; three add-ins' rows are enciphered).

## Movement, casting, hits and gestures (read 2026-10-18 by lib-agent-2)

### Movement follow-ups: walk start, stop clips, turn clips, mode exits (anim2mv)

Read 2026-10-18 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py --disasm`, a rel32 call scanner, the .ani/.evt files through ghilana and `tools\gff4.py`. No Ghidra, no game. "(checked)" = read in the disassembly or the data; "(inferred)" = reasoning from what was read. Not seen in play. Addresses are GOG's. Scratch: `%TEMP%\lib-agent-2-anim2-*`.

#### Short answers

1. **Walk start: no clip is played, the controller goes straight to mode 2 in the same frame.** `pausewalkf` (85) has no node in movement.anb, so `0xA6A300` fails its "is the clip in the Transition tree" test and calls `0xA6BF70` (mode 4 -> mode 2). Mode 2's enter then fades `Motion` to 1.0 and `Transition` to 0.0 at blend 4.0 = 0.25 s linear, and the walk/run leaves restart at clip time 0. Run start (`pauserunf` 84, `mov_p_rf.ani` 0.2 s) does have a node and plays.
2. **Stop clips: the logical position is already at the path's end, and nothing moves the object during them.** State 3 is entered on arrival (speed set to 0 at once, no deceleration), the controller does not move the creature in state 3, and the stop clips drive nothing in the object: the engine's root-motion channels are not in these clips. The 0.05 m of travel in `root_translation` is bone animation inside the model (checked).
3. **Turn clips: the model's drawn orientation is the creature's logical orientation, rotated by the code's fast turn (about 0.15 to 0.25 s for 90 degrees); the clip animates the body only.** The clip's root yaw is not applied to the object (the root-motion object has no clip for these files), and the clip has no 90 degrees of yaw in any bone (checked). So the "root-yaw scaling" in animmove is a no-op for the human locomotion clips.
8. **Mode 8 (stop) has no exit on MvmtUnlock or on a time test. Its exits: the engine's Done (event 13) at the clip's end (2.267 s at rate 1.0), or new movement input (mode 4 at once).** The stop clips have no Done in their .evt; Done is raised by the clip event sweep when the playhead crosses the clip's length (inferred from 0x815E81, not seen). Mode 0x10 (turn) exits on Done too, but earlier in practice: as soon as the controller leaves state 4, which is when the facing is within 0.999999 degrees of the desired one, so the 0.8 s turn clip is cut to a 0.25 s fade after about 0.15 to 0.25 s.

#### 1. Walk start from idle (checked unless marked)

Chain, per frame `0xA6BF00(dt ms)`:
- Controller state [+0x13C] goes 0 -> 1 (walk) when the order starts; flags [+0x40] come from `0xA6A0B0` and are nonzero only with speed [+0x134] > 0. The speed ramps +8 m/s per second (`0xA635D0`), so the flag is set from the first update after the speed leaves 0.
- `0xA6BD80` (motion flags): mode 1, 8 or 0x10 -> `0xA6BC40(4)`. The mode-4 enter `0xA6B710`: [+0x64] = 0.0, [+0x60] = 0 (the stored stop-clip id is forgotten), then for old mode 1/4/8/0x10 (byte table 0xA6B750: 00 01 01 00 01 01 01 00 ...; entering mode 4 from mode 2 does nothing) [+0x50] = 4.0 (float 0xADEF1C) and `0xA6AE30`.
- `0xA6AE30` (start): not combat, global switch 0xC1CFAC set; forward: angle between the move direction [+0xF0] and the current facing [+0x110]; running = target speed [+0x138] >= Game_RunSpeed - 1e-6 (`0xA69C70`). Angle < 45 (float 0xB59A6C): id 85 (walk) or 84 (run). 45 <= angle < 157.5 (0xB59A70): 91/93 (right 90 run/walk) or 92/94 (left, by flag bit 4); >= 157.5: 87/89 or 88/90. Backward (flag bit 2): 86 `pausewalkb`. Then `0xA6A300(id, rate 1.0)`.
- `0xA6A300`: first `0xA6A2B0` clears the previous transition (fades its node to 0 at [+0x50] and zeroes [+0x5c]). Then `0xA5F440(id)` (the row's AnimName -> `0xA5E730`, which answers which root tree of the animation set holds that name, 0 if none) is compared with `0xA5E730("Transition")` (string 0xC4C5BC). **A mismatch (the node is missing, so the answer is 0) jumps to `0xA6A38E`: `0xA6BF70` and return**, and `0xA6BF70` is mode 4 -> `0xA6BC40(2, 0)`, mode 8/0x10 -> mode 1. Only on a match is AnimBase virtual 0x58 (`0xA5E670`, `PlayById(id, rate, 0.0, 1.0, blend = [+0x50], 0, 1)`) called; its bool result is tested too, and false does the same `0xA6BF70`.
- Which ids have nodes (movement.anb Transition node): 84, 87-94 (the 90/180 starts), 74/75 (run stops), 59/61 (turns), shuffle. **Not present: 85 `pausewalkf`, 86 `pausewalkb`, 80-83 (walk stops).** anim_base rows 85/86 exist (TransitionTo 56/55, Replacement 56/55) but nothing in this path reads TransitionTo (`0xA5F440` reads the AnimName only).
- Mode-2 enter `0xA6A830(oldMode, arg)`: `0xA69AD0(arg)` first: if the loops are not armed ([+0x6D] = 0) or the `Motion` weight is below 0.01 (float 0xAF7498), set [+0x6D] = 1 and call AnimBase virtual 0x6C on the leaf array (non-combat `{walkf, walkb, runf, runleanl, runleanr}` 0xC4C8BC, combat 0xC4C8CC) with time = arg + [+0x64] and 1.0 (so the loops restart at that phase; arg is 0 from `0xA6BF70`, [+0x64] is 0 unless a start/turn clip's MvmtSetOffset set it, see section 4), then [+0x64] = 0. Old mode 1, 4 or 8: [+0x50] = 4.0. `0xA6A2B0` again. Then the current base clip (AnimBase [+0x140], the idle) is faded to 0 at [+0x50] and `Motion` = 1.0, `Transition` = 0.0, both at blend [+0x50] = **4.0 = 0.25 s linear** (weight rate per second). The per-frame weights of Walk/Run (`0xA6B7A0`) and the play rate (`0xA6AA70`) then act as in animmove.
- **So a walk start from idle is: idle fades out over 0.25 s while Motion/Walk fades in over 0.25 s, the walk loop starting at clip time 0 (inferred: units of virtual 0x6C's time not read, the 0.0 arg is a start); the speed ramp 0 -> 1.54 m/s takes 0.19 s.** A run start from idle with a straight-ahead angle plays `pauserunf` (0.2 s) as Transition (below), then mode 2.
- When the transition clip is played (any of the nodes that exist): `0xA6A300` after the play fades `Motion` to 0 and `Transition` to 1 at [+0x50] = 4.0 (0xA6A4A4/0xA6A4C7 and the other branches), the clip itself started with blend [+0x50] = 4.0 (not 2.5 as animmove had it: 2.5 is the blend of the `0xA5FCC0` bookkeeping calls in the same function). If the pair (idle clip, transition clip) has a .tbi curve (`0xA5F350`), the curve replaces the ramp; humanoid.tbi has none for these, so linear.

#### 2. Stop clips and where the creature is (checked unless marked)

- Path following `0xA64350`: remaining path length [+0xE8] = `0xA63E80`. With an anim controller, `0xA6ACF0` returns `0.25 * |node+0x20|` (double 0xAE7FE8 = 0.25) for the stop clip chosen by `0xA69BA0(0)` (the clip node's +0x20 vec3 is its end root translation, filled by the clip wrapper's constructor at 0xA39440..: `0xA390F0(0, node+0x10)` and `0xA390F0(length, node+0x20)`, both zero-initialised first). If that is `< remaining`, `0xA636B0` gives the travel the clip still has; the state-3 test is `remaining < travelLeft + 1e-6`. Then the step is taken (`0xA63450`, `0xA63F40`), and **arrival (`0xA63F40` true) sets state 3 as well**, with the struct's position at the path end.
- **For the human stop clips `|node+0x20|` is 0**: `0xA390F0` looks for a channel named **"gob_gobtranslation"** (UTF-16 at 0xB58CBC; rotation "gob_gobrotation" at 0xB58C9C) in the clip's channel list (field 4005, names 4000), and these clips (mh_m.mov_rf_p_lfoot/rfoot, mov_p_rf, mov_p_r90trn_p, mov_rf, mov_wf, checked by dump) have `god_translation`, `god_rotation`, `root_translation`, `root_rotation` and no such channel. So travelLeft = 0 and **state 3 begins only at arrival (remaining < 1e-6), at the end of the path**. The travel ratio `AnimBase[+0xB4] = max(0.25, remaining/T)` written by `0xA6AD50` is then a division by zero and is never used (see section 3: the root-motion object has no clip for these files).
- In state 3 (`0xA644E0`): `0xA63B20` (turn the facing toward [+0x90]) is called, **[+0xE0] speed = 0 at once (no deceleration; the creature arrives at full speed and stops dead)**, `0xA64350` (movement) is not called. The creature update (`0x6BB9CA` region) applies the controller's position to the object as in any frame (`0x6B6880`, then SetOrientation `0x694CE0`) because AnimBase+0xC0 is 0 (section 3). The position does not change, as the struct position is unchanged.
- The clip itself: `0xA6AD50` (mode 8 enter): id `0xA69BA0(1)` = [+0x60] (set by the run clip's MvmtSetTransition events to `runpausefl` 74 or `runpausefr` 75; walking sets none, so **a walk stop has no clip: [+0x60] = 0 -> `0xA6BF70` -> mode 1, a crossfade to idle**); `0xA6A300(id, 1.0)`: rate 1.0, blend [+0x50] = 4.0 (0.25 s), Motion -> 0, Transition -> 1. So the stop clip plays in place, 2.267 s, 1.0x.
- The foot is chosen when the run loop's events fire (section 4 of animmove); the stop clips' own events: `mov_rf_p_lfoot.evt`: 0.00 MvmtLock(23), 0.03 MvmtSetTransition(25) "pause", 0.17 FootDown(14) "|Left", 0.30 FootDown "|Scuff", 0.30 MvmtUnlock(24). `mov_rf_p_rfoot.evt`: 0.00 Lock, 0.03 SetTransition "pause", 0.07 FootDown, 0.17 MovementSound(17) "Unknown", 0.27 FootDown "|Left|Scuff", 0.27 Unlock. (The add-in dao_prc_cp_2 has copies of the same sizes, 278/322 bytes; not readable, assumed equal.)

#### 3. Turn clips and the model's orientation (checked unless marked)

- **The root-motion object** (class at AnimBase+0x80, vtable 0xB59728, methods 0xA65BD0 start, 0xA65C80 init, 0xA65CF0 reset, 0xA65D10 set yaw scale, 0xA65E60 add a rotation, 0xA65ED0 update, 0xA66xxx warp helpers). Fields: +0x00 position, +0x10 base orientation (quaternion), +0x20 current orientation, +0x34 translation scale, +0x38 yaw scale, +0x3C clip time (ms), **+0x40 clip (AnimBase+0xC0)**, +0x50 target position, +0x60 target yaw, +0x64 end time, +0x68/+0x69/+0x6A flags (warp to a target position / yaw / ...). `0xA65BD0` stores the clip in +0x40 **only if `0xA38F20(clip)` is true: the clip's GFF4 has field 4008 `AnimationHasGobAnim`**. The locomotion clips' template (anim, node, quat, list, vec3) has no field 4008 (checked on lfoot, rfoot, r90trn_p, mov_p_rf, mov_rf, mov_wf), so **+0x40 = 0**, and `0xA65ED0` then returns false at once unless a warp flag is set. `0xA394F0` and `0xA6458F...` (`AnimBase+0xC0 != 0`, via `0x695670`) are the "gob clip playing" tests.
- **Consumers**: (a) `0x6BB910..0x6BBB2F` (the per-frame creature update, `dt` in ebp+8): `0xA644E0` (controller) is called with the frame struct (position at +0, facing at +0x10 from the object's +0x120/+0x130). If the controller returns true: if `0x695670` (AnimBase+0xC0 != 0) is false, which it is for every locomotion clip, the creature's position is integrated (`0x6B6880`) and **the facing is stored with `0x694CE0` (object +0x130 = the vector, then virtual 0x80 `0x6A0190` -> `0x6955C0`, which hands +0x130 to the animation owner `0xA5CBE0` (it turns the vector into yaw `0x4EE570` and pitch `0x4EE5A0` and rotates the model) and the yaw to the sub-object at +0xA8)**. If AnimBase+0xC0 is set (a gob clip), the controller's facing change is not applied; its angle difference (yaw/pitch, `0x4EE570/0x4EE5A0`) is turned into a quaternion (`0x4EE200`) and added to the root motion's +0x10/+0x20 with `0xA65E60`, and (b) `0x6A3150` (virtual of the creature, called from the same update chain; vtable 0xB1059C) calls `0xA65ED0` with output pointers and writes the result to the object: position by creature virtual 0x11C (`0x6BBB70`) and facing by `0x694CE0`. `0x695980` is the same with the root-motion output applied; `0xA65ED0`'s outputs are at the object's +0x00 (position) and +0x20 (orientation) when flags +0x68/+0x69 are set, else computed from the clip by `0xA39260` (translation at time +0x3C, scaled by +0x34, rotated by +0x10) and `0xA39300` (rotation, scaled by +0x38).
- **Result for the turn clips**: the object's orientation (+0x130) is the controller's, from the fast turn `0xA63B20` (same rate formula as animmove, `Δ' = 180 dt / (0.4 - 0.28 |Δ|/180)`: about 580 deg/s at 45, 700 at 90; with the per-frame recomputation, 90 degrees takes about 0.15 to 0.25 s). The model's world rotation is rebuilt from it every frame. The clip's root yaw (`0xA65D10`, the scale `|angle/rot|` of animmove) has nothing to scale: **no clip, no effect** (inferred from +0x40 = 0; the call is made, `0xA65D10` returns early at 0xA65D4B).
- The clips themselves (checked, decoded with Clip.h's rules): `mov_p_r90trn_p.ani` (0.8 s) has `root_rotation` (a bone, 64-bit keys) at 119 to 121 degrees from the identity all along, i.e. the rest orientation (the Y-up to Z-up turn), `god_rotation` identity at both keys, `god_translation` zero; no bone has a net 90 degree yaw between the first and last key (only `leftspawn`/`rightspawn`/`eyelookatbase`, 90 degrees, and hip, ankle and hip-twist bones swinging 27 to 49 degrees and returning to 0). So the clip is stepping legs in place, not the body turn. `mov_rf_p_lfoot` the same (root_translation ends at z = +0.005 from -0.045, 0.05 m, as a bone).
- Not seen in play: whether 0.15 to 0.25 s for a 90 degree standing turn matches the vanilla look (the model turns by code, the legs step during 0.8 s of clip that is cut, see section 4).

#### 4. Exit conditions of mode 8 and mode 0x10, and of the others (checked unless marked)

Mode word [+0x54]; per frame `0xA6BF00`: flags [+0x40] = 0 if the controller state [+0x13C] is 0 or 3, else `0xA6A0B0`; **`(flags & 0x33) == 0` -> `0xA6BCC0`, else `0xA6BD80`**.

`0xA6BCC0` (no forward/back/strafe flag):
| mode | rule |
|---|---|
| 2 | state == 3 -> mode 8; else -> mode 1 |
| 4 | [+0x6E] (lock) != 0 -> stay; else -> mode 8 |
| 1 | not combat, flags & 0xC (turn bits), state == 4 -> mode 0x10; else `0xA69DF0` (settle: if the weight of node [+0x178] is below 1e-6, fade the leaves/Transition/Motion... `0xA69CF0`, disarm the loops `0xA69AA0`, [+0x70] = 0) |
| 8 | `Motion` weight below 1e-6 (float 0xB59AB8) -> `0xA69AA0` (disarm loops); then **state == 0 -> mode 1, otherwise stay** |
| 0x10 | **state == 0 -> mode 1, otherwise stay** |

`0xA6BD80` (motion flags), after `0xA6A7E0`; if flag bit 1 (backward) differs from the previous frame's [+0x44], [+0x60] = 0: modes 1, **8** and **0x10** -> `0xA6BC40(4)` **unconditionally (new movement input leaves the stop or the turn at once, no lock test)**; mode 4: if the playing start clip ([+0x5C]) has under 250 ms left (`0xA5F980 < 0xFA`), the start was a walk (`0xA69C70([+0xD8])` false) and the target is a run (`0xA69C70([+0x138])` true) -> mode 4 again (the run start follows); mode 2 nothing.

Events (`0xA6BFA0`, ids 11-26; the jump table: 11 and 13 -> one handler, 23 -> lock, 24 -> unlock, 25, 26, the rest ignored): 11 BlendStart and 13 Done: the event's string (the engine puts the finished clip's AnimName there) is compared with the AnimName of the row [+0x5C] (the clip being played); if equal, [+0x6E] = 0 and `0xA6BF70` (**mode 4 -> 2, modes 8 and 0x10 -> 1**). 23 MvmtLock: [+0x6E] = 1 if the string matches (the lock event compares the same way). 24 MvmtUnlock: [+0x6E] = 0. **The lock only matters for mode 4 -> 8 in `0xA6BCC0`; mode 8's exit never reads it.** 25: [+0x60] = id of the named clip when the right branch is dominant (animmove). 26: [+0x64] = integer string / 30.0 (double 0xAF7068), the start-phase offset in seconds used at the next `0xA69AD0`: `mov_p_r90_rf` "4" (0.133 s), `mov_p_l90_rf` "21", `mov_p_r180_rf` "15", `mov_p_l180_rf` "34" (frames at 30 per second), each together with an Unlock at 0.20 to 0.23 s.

- **Where Done comes from**: event id 0xD is built in the clip event sweep `0x815990` (called from the tree-entry step `0x811D10` via 0x811E14) at 0x815E81, after the .evt events of the window: when the sweep's window (previous time, current time] contains the clip's length ([ebx+8]), and the clip is not looping ([ebp+0x30] = 0). The event carries the node name in its +0x14/+0x18. The same sweep emits id 0xB at 0x815DCC when a float parameter ([ebp+0x28], not traced to its caller) lies in the window. So Done is the playhead crossing the end of a one-shot, with no .evt entry (inferred; the sweep was read, the caller's arguments were not).
- **Mode 8, exact**: entered from mode 2 with state 3, or from mode 4 unlocked with no motion flags; stays until (1) Done/BlendStart of [+0x5C]'s clip -> mode 1; (2) `0xA6BCC0` finds state 0 -> mode 1 (but the controller cannot leave state 3 while the anim mode is in {4, 8, 0x10}: `0xA6457B`: `test [anim+0x54], 0x1C` -> stay, so this needs a new order that resets the state); (3) motion flags (a new order) -> mode 4. **No time test, no weight test (the weight test only disarms loops), no MvmtUnlock.** For `runpausefl/fr` with rate 1.0: **2.267 s** until Done; a new walk/run order cuts it at once.
- **Mode 0x10, exact**: entered from mode 1 (not combat, flags & 0xC, state 4); enter `0xA6AFF0` computes the clip (animmove) and returns to `0xA6BF70` if the angle <= 30 degrees; stays until Done (0.8 s clip at the chosen rate) or **`state == 0` -> mode 1**. State 4 -> 0 happens in `0xA644E0` (0xA645B3..0xA645FD) once the angle between the current facing and the desired one is <= 0.999999 degrees (or the desired is zero) and AnimBase+0xC0 is 0, so after the fast turn (about 0.15 to 0.25 s): mode 1 -> the mode-1 enter `0xA6B6C0` calls `0xA6A2B0` which fades the turn clip out at 4.0 (0.25 s). **So the visible clip is the first 0.15-0.25 s of a rate-adjusted turn clip, then a 0.25 s fade (inferred; not seen).**
- **Mode 4 (start)** exits: Done of its clip -> mode 2 (the 0.2 s `pauserunf`: Done at 0.2 s; the 90/180 starts are 0.267 and 0.333 s); a missing node or a failed play -> mode 2 at once; no motion flags and unlocked -> mode 8; motion flags again with the re-start rule above. The start's Lock (23 at 0.07 s in the 90/180 starts, 0.1 s in `mov_p_rf`) keeps mode 4 from falling to the stop while the order is withdrawn.

#### Not read

- The caller of the event sweep's parameters (the id-0xB time at [ebp+0x28]) and thus when BlendStart fires; whether Done also fires for clips played with the loop flag off by the set.
- AnimBase virtual 0x6C's time unit (`0xA69AD0`), 0x70 (`0xA69AA0`), the settle `0xA69DF0`/`0xA69CF0` beyond the weights.
- Whether a new move order can set state 1 or 2 while state 3 holds (the setter `0xA639C0`/`0xA63800` callers, and the command layer's gate for a move while the stop runs).
- `0xA63F40` (the path advance: that it puts the position exactly at the path end on arrival), `0xA63E80`, `0xA63450`, `0x6B6880`.
- How many .ani files in the game have field 4008 (the gob clips) and what plays them; the use of the warp flags +0x68..+0x6A in the root-motion object (`0xA66xxx`).
- Quadrupeds (their stop/turn trees and whether they have gob channels), the dao_prc_cp_2 add-in's clip/evt copies (assumed equal), the combat tree (no Transition node).
- Nothing here was seen in play; the numbers 0.15-0.25 s for a turn and the dead stop at the path end are readings of the code and should be checked against the vanilla game.

### Ending a cast (cast-end, conjure, berserk) and the bow/staff trees, read 2026-10-18 by lib-agent-2 in GOG DAOrigins.exe 1.05 (base 0x400000), for DAOParagonEngine

Read with `daoexe.py --disasm`, ghilana (tables, .anb/.ani/.evt/.tbi) and `gff4.py`. Not seen in play. "(checked)" = read in disassembly or data; "(inferred)" = not. Builds on `lib-agent-2-anim-fade.md` (blends, .tbi), master's `dev/ABILITIES.md` and `dev/ANIMATION.md`, `lib-agent-2-playanim.md` (sub-action 37), `lib-agent-2-wstyle.md`. Scratch: `%TEMP%\lib-agent-2-anim2cast-*` (extracted clips under `-mh\`, `-res\`, `-anb\`).

#### Short answers

**Q4**
1. **Nothing plays the cast-end clip from the ability code.** Only the conjure (sub-action 7) and the cast (sub-action 37, queued by sub-action 8) play clips. The cast-end, the conjure loop and the return to `pause` are the **follow-through of the animation set**: when a non-looping clip finishes, the engine raises animation event 13 "Animation Done" and the creature's handler (vtable slot 51, `0x00696C50`) plays the clip's **TransitionTo** row with **speed 1.0, blend 3.0 (0.333 s linear)**, but only if the finished clip is still the creature's current animation (+0x13C of the animation set). It does not look at the command queue (checked: handler; the raising of event 13 is inferred, see Not read).
2. The chain is data: `ua_dconjure_start` (244) -> TransitionTo `ua_dconjure_loop` (264, loops); `ua_dcast` (246) -> `ua_dcast_end` (290) -> `pause` (1); `ua_acast` (247) -> `ua_acast_end` (291) -> `pause`; `ua_dcast_loop` (258, loops) -> `ua_dcast_end` (290). `pause` is anim id 1, so its play gets the random speed offset (speed += rand()%90/100 - 0.45, `0x0069B478`) (checked).
3. **The cast command ends at 80 % of the cast + cast-end length**, before the cast-end clip has finished (1,707 ms for Chain Lightning = 0.8 x (1.3333 + 0.8) s; checked, reproduces the number seen in play). The cast-end is not conditional on a queue: it plays as soon as the cast clip is done; a new command's play that replaces the current animation before then cancels it (the Done handler's current-anim test).
4. **"dcast" is "direct", "acast" is "area"; neither is a stance** (not dual, not staff). It is picked per ability by `abi_base` ConjureAnim (244 d / 245 a) and CastAnim (246 d / 247 a); staff versus non-staff comes from which tree has the node (Staff.anb's `mh_c.stf_spl_*` or combat_humanoid's `mh_c.ua_spl_*`).
5. **Berserk**: `BERSERK` (abi 700, AbilityType 1 talent, AbilityMode 1 modal, Range 0, TargetType 1 self, ConjureAnim 0, CastAnim 144 = anim_combat `berserk`, TransitionTo 1) is played as the ability's CastAnim by sub-action 37 (the same path as a spell's cast), clip `mh_c.tal_brsrk.ani` 2.5 s from combat_humanoid.anb; it returns to `pause` by the Done event: Play(pause, 1.0, blend 3.0), command ending 0.8 x 2.5 = 2.0 s after it starts. **The `.tbi` pair `shared.berserk.ani -> shared.pause.ani` (0.6 s Bezier) does not match any node in the shipped trees**, so it does not apply (inferred from the file names, see section 4).

**Q7b** Master's sentence is **right in substance and incomplete in mechanism**: the chooser `0x007C3500` returns no set for bows, crossbows and staves (so no `OW/WS/TW/DW.anb`), and Bow.anb / Cbow.anb / Staff.anb (and the `*Overlay.anb`) come from the item's `bitm_base` `WieldModel` / `WieldModelOverlay` (hashes `0x8BF8FC78` / `0x543CC507`). Two places use them: (a) **at equip**, the creature's item-equipped handler `0x006C16E0` calls, after the appearance slot `0x00748DA0` and the set chooser `0x006B8D40` (call at `0x006C1808`), `0x006B78D0(item, 0)` at `0x006C1812`, which adds both trees to the creature's animation set with `0x006B5B30`; (b) at the **draw's equip animation events** (3-6), `0x006A56B0` swaps the apr_base strings at +0x28/+0x44. (a) answers master's "not read: how the tree is in place before the first draw": it is added at the moment the weapon is equipped. See section 7.

#### 1. The three sub-actions, in order (checked unless marked)

`0x00743F50` queues for an ability: approach (4 / 15 / 14), turn (39 / 40), then sub-action **50** (the command's step), which after COMMAND_PENDING queues **7** (conjure) and **8** (use). `dev/ABILITIES.md` has these. What is new here is what 7, 8 and 37 play.

##### Sub-action 7, the conjure (`0x007A66A0`)
- Fails -8 if int 2's disabled mask meets the user's disabled flags. On the first run (flag 0 clear), if **int 5 (ConjureAnim) is 0 it answers 1 at once**.
- Otherwise int 3 (if set) goes to `0x0079FD00` on the user's state; `0x006A2A20` (stop moving) is called; **if int 4 (the conjure's milliseconds) is > 0** it calls the creature's Play (vtable slot 44, +0xB0) with `(anim = int 5, speed = float 0, blend = -2.0, flag 1)` (stack at `0x007A6742`-`0x007A676C`, blend from `0xAFF63C` = -2.0). **If int 4 is 0 no clip is played at all** (the jump at `0x007A673B`). It then calls `0x007A4F90(user, int 6, 1)`, stores int 4 as the duration (+0x14) and sets flag 0.
- Every run: `0x007A1AE0` (remaining time) and it answers 1 when remaining <= 0.
- **Blend -2.0 is a code, not a rate** (`dev/` fade note, `0x0069B3D0`): the fade-in lasts 20 % of `AnimationStepLength(anim)` (int arithmetic `len*20/100`, `0x00694FE0`): `blend = 1000 / (0.2 x stepLength_ms)`. Staff `ua_dconjure_start`: 0.4667 s step length (the chain stops before the loop clip, `0x006957A0`) -> 93 ms fade; `ua_aconjure_start` 0.7667 s -> 153 ms.
- So `Speed` "Instant" (loops 0) spells (conj ms 0) show **no conjure clip**; only spells with a conjure time play `ua_?conjure_start`, which on its Done event hands over to `ua_?conjure_loop` (TransitionTo, blend 3.0) and loops until the conjure's time is up. Chain Lightning: start 0.4667 + 1 loop x 3.4 s = 3.8667 s (the 3,866 ms seen in play, checked against `mh_c.ua_spl_d_strt.ani` 0.4667 s and `mh_c.ua_spl_d_lp.ani` 3.4 s).

##### Sub-action 8, the use (`0x007A7440`): the cast clip
- After CAST_START (as in `dev/ABILITIES.md`), `0x007A763B` calls `0x007A4F90(user, int 6, 0)`; then, **if the ability has a CastAnim** (`ebx` from the parameter block), it builds a **sub-action 37** (`push 0x25 ... call 0x0073D4D0`, AddSubAction(0x25, params, 0, 1, 0), `0x007A768F`-`0x007A7693`): int 0 = CastAnim, **flag 3 set** (`SetFlag(3, 1)` at `0x007A767F`), int 7 unset. So the cast plays with the default **blend 2.5 (0.40 s linear) from whatever is current** (the conjure loop, or `pause` for an instant spell), speed 1.0 (float 3 = 0).
- **The 37 in cast mode is timed shorter**: 37's code (`0x007A44AE`-`0x007A44C7`) with flag 3 = 1 subtracts `0x00694FE0(len, -2.0)` = 20 % of the length from the duration; the length is `AnimationStepLength(CastAnim)` when the command is not "play next" (it is not), which is the **cast clip + its TransitionTo chain up to the first looping clip** (cast + cast-end; `pause` loops and is not added).
  - Duration (ms) = L - L*20/100 with L = len(cast) + len(cast_end) in ms. Combat_humanoid `ua_dcast` (`mh_c.ua_spl_d_cst.ani` 1.3333 s) + `ua_dcast_end` (`mh_c.ua_spl_d_end.ani` 0.8 s) = 2,133 -> 426 cut -> **1,707 ms** (matches play). Staff `mh_c.stf_spl_d_cst` 1.0 s + `mh_c.stf_spl_d_end` 0.5333 s = 1,533 -> **1,227 ms**.
  - Then the command (nothing queued by 8 after 37) is done: the next command starts 20 % of L before the cast-end clip would finish (0.43 s early for the humanoid set).
- Impact timing (checked in the clips and the dispatcher): the cast clip's own events call the creature: **event 9 "Projectile Load" -> creature vtable +0xF0 = `0x006B6F60`** (makes the projectile, or with Projectile 0 goes straight to `0x006AF430(0)`, the CAST_IMPACT), **event 10 "Projectile Fire" -> vtable +0xDC = `0x006AFA40`** (ABILITY_PROJECTILE_LAUNCHED), **event 7 "Ability Cast Impact" -> `0x006AF430`** (`0x006C0C5F`). Dispatch: `0x00694EE0` maps 9 -> +0xF0, 10 -> +0xDC, 11 "Blend Start" -> +0xD4 (`0x00696BD0`), 13 "Animation Done" -> +0xCC (`0x00696C50`), 16 -> +0xE0, 33 -> +0xE4, 38 -> +0xE8; `0x006C0BF0` (slot 46, +0xB8) handles 1, 2, 3-6, 7, 8, 21, 28-31 itself.
  - Clip events (seconds): staff `stf_spl_d_cst` 9 at 0.17, 10 at 0.93 (length 1.0); `mh_c.ua_spl_d_cst` 9 at 0.9, 10 at 0.93 (length 1.3333); staff `stf_spl_a_cst` 7 at 0.9 (length 1.0333); `stf_spl_inst` (instant) 7 at 0.47; `mh_c.tal_brsrk` 7 at 0.0 (length 2.5).
  - Meaning: a direct cast with a projectile creates it at the "Projectile Load" time and launches it at "Projectile Fire"; a direct cast with Projectile 0 (e.g. Flame Blast) hits at the Load event; an area cast (acast) hits at its Cast Impact event; berserk's impact is at t = 0.

#### 2. The cast-end: how and when (checked)

- Handler `0x00696C50` (CCCreature vtable slot 51, +0xCC), argument the event record:
  1. anim id of the finished clip = `0x00694C40` (the event's string -> AnimName -> id);
  2. if that clip loops (`0x00695390`) return (a loop never "finishes");
  3. if the row's **State** (col 0xE991BD51, via `0x00A5E8A0`) is 1 or 2 -> call vtable +0xD0 (`0x006AA8F0`, not read) and return;
  4. if the finished id == the set's current id (`0x006956B0`, +0x13C): if the creature has "deactivate pending" (+0x421) call slot +0x140(0); else **Play(next = set+0x140 (`0x00695700`), speed 1.0, blend 3.0, flag 0)** through vtable slot 44. Then it clears set+0xE4 and three byte flags at set+0x80+0x68..0x6A;
  5. if the finished id is not current: nothing is played.
- set+0x140 is the row's TransitionTo (`0x00A5F480` stores it at `0x00A5F4A2`; `0x00A5E940` first stores the played id itself when the creature's trees have the node). Nothing else reads +0x140 except the curve scheduling at `0x00A5FC02`.
- Chain for a direct cast: `ua_dcast` Done -> Play(`ua_dcast_end`, 1.0, 3.0) -> its Done -> Play(`pause`, 1.0, 3.0) (+ random idle speed). Each handover is a 0.333 s linear cross-fade; the old clip keeps playing while its weight falls (fade note).
- **Command interaction**: the cast command ends before cast-end is done (section 1). If the next command plays a clip (e.g. the next cast's conjure with blend -2.0 or 2.5), that is the new current animation, and the pending cast-end's Done event no longer finds itself current (nothing more is played from it); the old cast-end only fades out.
- **Berserk**: `anim_combat` 144 `berserk` (Type 4096, TransitionTo 1, State 0, Replacement 107 `attk_v1`): same path, Done -> Play(pause, 1.0, 3.0). With a weapon set whose tree has no `berserk` node (OW, WS, TW, DW have none) the lookup finds it in the base tree combat_humanoid (`berserk` -> `mh_c.tal_brsrk.ani`, flags 1, 2.5 s); Replacement 107 is the fallback when no tree has it (inferred from the column's use for other rows; not read here).
- Other talents use the same pattern: CastAnim rows 145, 146, 147, 149, 154, 2000-2044 are anim_combat rows with Type 4096, TransitionTo 1, Replacement 107 (attk_v1) (data), e.g. `talent_warcry` 2005, `talent_charge` 147.

#### 3. The abi_base columns and rows (checked, ghilana)

abi_base (table 1) columns that name animation: **`ConjureAnim`, `CastAnim`, `BlendTree`** (a string, e.g. `DoubleStrike.anb`, `ShieldWall.anb`; the one `*****` is a placeholder), also `AutoDraw`, `Flags`, `Speed` (the conjure speed index), `ConjureVFX`. bitm_base has a column **`ModalabilityBlendTree`** (an int, e.g. Longsword 1, Crossbow 0): neither of these two was read in the exe (`BlendTree` / `ModalabilityBlendTree` are candidates for the modal-ability blend trees such as ShieldWall; not followed).

| Group (abi_base) | AbilityType | ConjureAnim | CastAnim | examples |
|---|---|---|---|---|
| single-target, projectile or cone, TargetType 4/126/127 | 2 spell | 244 `ua_dconjure_start` | 246 `ua_dcast` | ARCANE_BOLT 200254 (Speed 1, Projectile 137), CHAIN_LIGHTNING 10211 (Speed 2, Proj 114), FIREBALL 10003 (Proj 2), FLAME_BLAST 10001 (Proj 0), WALKING_BOMB, DISPEL_MAGIC |
| self / ally / ground, no projectile | 2 spell | 244 or 245 | 247 `ua_acast` | HEAL 10104 (244/247), HASTE 10208 (244/247), SPELL_SHIELD 10400 (244/247), INFERNO 10002 (245/247, Speed 2), BLOOD_MAGIC 10700 (245/247), SUMMON_WOLF 1004 (245/247) |
| talent with a clip | 1 talent | 0 | anim_combat row: 144 berserk (BERSERK 700), 145 deadly strike, 146 dirty fighting, 147 charge (SHIELD_BASH 617), 149 threaten, 154 riposte, 2000-2046 | WAR_CRY 3037 -> 2005, MIGHTY_BLOW 3028 -> 2043, DEADLY_STRIKE 708 -> 145 |
| aimed archer talents | 1 | 0 | 0 | SCATTERSHOT 800, CRITICAL_SHOT 804: Flags 1 (the aim sub-actions 37 anim 249 / 6 / 243) |
| item ability | 4 | 0 | 0 / 247 / 257 potion / 904 trap / 2034 throw | ITEM_ELFROOT -> 257 `potion_drink` |

- `dconjure`/`aconjure` and `dcast`/`acast`: the d clips point the staff/arm at a target; the a clips are a general raise. In the data **d goes with abilities that have a target creature, a projectile or a cone/line (TargetType 4, 126, 127), a with self, ground-area, party-wide and summons**; there are exceptions (HEAL, a single-ally heal, uses 244+247; CRUSHING_PRISON and DISPEL_MAGIC use 246). Conjure and cast need not be the same letter. It is chosen by the row only; the engine makes no choice.
- **anim_base rows** (Type, TransitionTo, State, Replacement):

| id | AnimName | Type | TransitionTo | State | Replacement |
|---|---|---|---|---|---|
| 1 | pause | 1 (loop) | 1 | 0 | 1 |
| 244 | ua_dconjure_start | 4096 | 264 | 0 | 1 |
| 245 | ua_aconjure_start | 4096 | 265 | 0 | 1 |
| 264 | ua_dconjure_loop | 4097 | 0 | 0 | 1 |
| 265 | ua_aconjure_loop | 4097 | 0 | 0 | 1 |
| 246 | ua_dcast | 4096 | 290 | 4 | 1 |
| 247 | ua_acast | 4096 | 291 | 4 | 1 |
| 258 | ua_dcast_loop | 4097 | 290 | 4 | 1 |
| 290 | ua_dcast_end | 4096 | 1 | 0 | 1 |
| 291 | ua_acast_end | 4096 | 1 | 0 | 1 |
| 292 | ua_instant | 4096 | 1 | 0 | 1 |
| 241-243, 248, 249 | reload, aimloop, fire, withdraw, aim | | 242 / 0 / 248 / 1 / 242 | | |

  `conjure_loop` has TransitionTo 0: it loops until the conjure's time ends and the cast's Play replaces it. A conjure start whose loop is missing from the tree (TransitionTo node absent) is not covered here.
- **Clip files (humanoid, node name -> file, seconds)**, trees `combat_humanoid.anb` (flat, one-shot nodes flags 1, loops flags 0): `ua_dconjure_start` mh_c.ua_spl_d_strt 0.4667; `ua_dconjure_loop` mh_c.ua_spl_d_lp 3.4; `ua_dcast` mh_c.ua_spl_d_cst 1.3333; `ua_dcast_loop` mh_c.ua_spl_dcst_lp; `ua_dcast_end` mh_c.ua_spl_d_end 0.8; `ua_aconjure_start/_loop`, `ua_acast`, `ua_acast_end` mh_c.ua_spl_a_*; `pause` mh_c.ua_rdy; `berserk` mh_c.tal_brsrk 2.5; `ua_instant` m.ua_ready.ani. Staff.anb (all flags as above): d_strt 0.4667, d_lp 3.4, d_cst 1.0, d_end 0.5333, dcst_lp 0.7, a_strt 0.7667, a_lp 2.7, a_cst 1.0333, a_end 1.0333, spl_inst 0.9667, rdy 1.6. (The non-staff `mh_c.ua_spl_a_*` lengths were not read.)

#### 4. The `.tbi` pairs and the mage / berserk clips

- humanoid.tbi (`lib-agent-2-anim-fade.md`) keys are `.ani` file names: `m.ua_cast_end.ani`, `m.ua_dcast_end.ani`, `m.ua_aconjure_start.ani`, `m.ua_dconjure_start.ani`, `shared.pause.ani`, `shared.berserk.ani`, `ws.talent_shieldbashgetup.ani`, `ws.talent_wcry.ani`.
- **None of those names is a node file in the shipped humanoid-combat trees.** I searched every readable .anb (1,207 extracted; the enciphered add-ins' are unreadable): `shared.pause.ani` appears only in `lbry.anb` and `noble.anb`; `m.ua_dcast_end.ani` etc. and `shared.berserk.ani` appear in no tree (`wizard.anb` and `pre100_wizard.anb` hold `m.ua_aconjure.ani`, not `_start`). The player and party combat nodes are `mh_c.ua_spl_*`, `mh_c.stf_spl_*`, `mh_c.tal_brsrk.ani`, `mh_c.ua_rdy.ani` and so on.
- Hence, **if the pair key is the node's file name (the fade note's inference), no mage cast-end, conjure-start or berserk fade uses a Bezier curve for the player and followers: they are all the linear 3.0 / 2.5 / -2.0 blends above.** This is inferred: the key construction in `0x00A5F350` (the model slot 23 lookup `0x00813D10`, then the map `0x00A69300`) was not decoded to the end. A play check (cast-end into pause) would settle it: a 0.333 s linear fade versus a 0.433 s curve with a 0.108 s hold.

#### 5. Constants (checked, floats read from .rdata)

| address | value | use |
|---|---|---|
| `0xB03B08` | 2.5 | default blend of sub-action 37 (the cast) |
| `0xAE7130` | 3.0 | blend of the Done follow-through (`0x00696CCB`) and the stance setters |
| `0xAFF63C` | -2.0 | conjure's blend code (20 % of step length), also 37's flag-3 cut |
| `0xAE2E00` | -1.0 | instant |
| `0xB0F26C` | 0.05 | argument of `0x006A6D90` in `0x006B78D0` (equipment with slot bit 0x10) |

#### 6. Cases in seconds (humanoid combat tree, no staff)

| step | clip | blend | time |
|---|---|---|---|
| conjure start (only if conjure ms > 0) | `ua_dconjure_start` 0.4667 | -2.0 code = 20 % x 0.4667 | fade 0.093 s |
| -> conjure loop (Done of start) | `ua_dconjure_loop` 3.4 | 3.0 | 0.333 s |
| conjure ends (start + loops x 3.4) | | | sub-action 7 done |
| cast (sub-action 37, flag 3) | `ua_dcast` 1.3333 | 2.5 | 0.40 s; Projectile Load at 0.9, Fire at 0.93 |
| cast Done (t = 1.3333 after its start) -> cast-end | `ua_dcast_end` 0.8 | 3.0 | 0.333 s |
| command done | | | 1.707 s after the cast started |
| cast-end Done (t = 2.133) -> pause | `pause` 1 | 3.0 | 0.333 s, idle speed +[-0.45, +0.44] |

#### 7. Q7b: Bow.anb, Cbow.anb, Staff.anb

- bitm_base columns `WieldModel` (hash `0x8BF8FC78`) and `WieldModelOverlay` (`0x543CC507`), hashes checked with `gda.column_hash`: Shortbow 19 `Bow.anb`/`BowOverlay.anb`, Crossbow 21 `Cbow.anb`/`CbowOverlay.anb`, Staff 16 `Staff.anb`/`StaffOverlay.anb`, the swords' rows empty (ghilana, checked).
- The adder is **`0x006B78D0`** (`__thiscall(creature, item, kind)`, `ret 8`), called from only two places, both right after `0x006B8D40` (the set chooser): `0x006C1812` in the on-equip handler `0x006C16E0` (kind 0) and `0x006C1A95` in the on-unequip `0x006C19F0` (kind 2). Body: gate `0x006A25A0` (the appearance's CombatModelTree is `combat_humanoid`); reads WieldModel at the item's base row (`item+0x464`); if the string is non-empty -> `0x006B5B30(creature, kind, string, 0)`; then reads WieldModelOverlay and `EquippableSlots` (`0xD5B0AE22`): if the equippable-slots mask has bit 0x10 it calls `0x006A6D90(0.05)` instead of adding the overlay tree, otherwise (and the string non-empty) it adds the overlay with `0x006B5B30` (`0x006B79E6`).
- `0x006B5B30(kind, name, flag)`: kinds 0 and 1 call the animation set's vtable slot +0x8C `(name, 0xF)` (add a tree); kind 1 also `0x00A60810` and records the name in a list at creature+0xCA0 (`0x006B5D00` drops those, kind 2); kind 2 calls slot +0x94 (remove the tree) after looking its slot up; it then refreshes (`0x0069E9D0`/`0x006C9EF0`).
- So a bow or staff in the main hand **adds its WieldModel tree to the animation set when it is equipped** (including a weapon-set switch, which goes through the same handlers), long before the first draw, which is why `enter` / `quick_enter` / `exit` and the staff's spell clips are found. The draw's equip event `0x006A56B0` (master's section) also swaps the apr_base strings +0x28/+0x44 for the none set; the exact interplay of the two was not decoded.
- Staff.anb holds the staff's own `ua_*` spell nodes (see section 3), `enter`, `exit`, `quick_enter`, `quick_exit`, `pause` (mh_c.stf_rdy), `attk_v*`/`miss_v*` (mh_c.tw_atk_*), `fire` (= `mh_c.stf_spl_d_cst.ani`), `ua_instant`.
- Master's `dev/ANIMATION.md` sentence "the weapon set's tree (OW/WS/TW/DW.anb by equipment; Bow/Cbow/Staff.anb from bitm_base's WieldModel)" can stay; add that the WieldModel trees are added at equip by `0x006B78D0`, and that the "not read" note on this is now read. Order of lookup between the base tree, the set tree and the WieldModel tree for a node present in more than one was not read.

#### Not read

- **Who raises event 13 "Animation Done"**: the handler is read; the ModelAnimation code that raises it at a one-shot clip's end (the .evt files of the clips have no event 13; none of the 37 scanned authored ids include it) was not found. Also event 11/12 (Blend Start/End; slot +0xD4 `0x00696BD0` plays a 3.0-blend `0x00A5C840` on a state-1/2 row) and `0x006AA8F0` (slot 52, State 1/2).
- Whether the .tbi keys are file names; the `0x00A5F350` key construction; the exact slot-0x58 "scheduled fade" at `0x00A5FC02` (it schedules at `clipLength - curve.x3` seconds when a pair exists).
- The part of `AnimationStepLength` (`0x006957A0`) for nodes with property 0x1F / 0x20 (adds another animation's length via `0x006A9140` / `0x006BF8A0`).
- `0x007A4F90`, `0x0079FD00`, `0x006A2A20`'s use of int 3 / int 6; sub-action 8's exact `ebx` source (read as CastAnim from the parameter block, consistent with 1,707 ms); the aimed path (sub-actions 6 / 37 with anim 249 / 243).
- `ua_dcast_loop` (258) and who plays it (a channelled cast; no ability row names it); `ModalabilityBlendTree` / `BlendTree` columns' uses; non-staff `ua_spl_a_*` lengths.
- The order of tree lookup when a node is in several trees; the random-speed behaviour of `pause` after a cast beyond the code at `0x0069B478`.
- Nothing here was seen in play.

### Which animation rows play for an attack and its reactions, and who picks them (prefix anim2hit, lib-agent-2, 2026-10-18)

Read in the GOG exe (base 0x400000) with `tools\daoexe.py --disasm`, a call scanner, the tables through ghilana and the toolset's scripts. "checked" = read in disassembly or data; "inferred" = a reading of purpose. Scratch is `%TEMP%\lib-agent-2-anim2hit-*`. No game run.

#### Short answers

1. **Normal swing (hit, crit, miss): the engine picks, in `CSSubActionHandler::Swing` 0x007A87C0, from table 104 `nosynccombatanims` (not `anim_combat`).** It takes the two hands' results the scripts left (`SetAttackResult`), folds them to one result, and asks the table manager at handler+0x60 (`0x00A3B0D0`) for a row of AttackerResult x WeaponWield-mask. Several rows fit: **random, but never the same row as the last pick of the same key by the same attacker** (`0x00A3AFE0`). Then `0x006A41A0` plays the row. 0x0069F530 is NOT the chooser: it only folds the two hands' results into one (below). All checked.
2. **Rows of anim_combat**: hit variants v1..v6 = rows 107, 106, 138, 139, 140, 102; miss variants v1..v4 = rows 105, 182, 191, 192; backstab = 2006 (`talent_backstab`); a crit uses the hit rows (result 2 is folded to 1 in `0x006A40D0` and `0x0069F530`). A DW attack picks its pool by which hand landed (mask), so v4..v6/miss v3,v4 are the off-hand ("|Left") swings.
3. **Reactions on the target**: a **miss** is the engine's: the attacker's swing clip carries event 2 (`AttackImpactReaction`), which runs `0x006AB040` -> `0x006AAE20` and adds a play-animation command (type 14) to the *target* with row **181** (Evade v1) or **176** (Evade v2), 50/50 by `rand()` parity, if conditions hold. This is **0.2-0.45 s before the impact event** in the attacker's clip. A **hit** is the scripts': `effect_damage_h.nss` calls `PlayAdditiveAnimation(oTarget, 103)` for a non-fatal blow of damage >= 3.0. Blocked/parry/deflect have no row of their own: the scripts turn BLOCKED into MISS (combat_h.nss:1073-1079), and the evade rows' clip is the target's weapon/shield block clip for its set. Rows 141-143 (`hvyDam`, `bdmg_v2`, `bdmg_v3`) and 104 (`cough`) are referenced by nothing I found (no engine immediate, no script).
4. **Timing**: the result is applied at the swing clip's event 1 (`AttackImpact`) in every attack clip, the two-handed ones included; times below. (Corrected 2026-10-20 by lib-agent-2: the earlier "the two-handed clips have no event 1" was a misread of their long event lists. `mh_c.tw_atk_v1`'s `.evt`, the copy the game takes in `global.rim`, has event 2 at 0.90 s and event 1 at 1.27 s, read by lib-agent-2; `v2`'s event 1 is at 1.20 s and `v3`'s at 1.07 s. No fallback is needed.)

#### 1. Who picks the swing's row (checked)

Swing 0x007A87C0 (`ret 8`, `this` = sub-action handler; from COMBAT.md steps 4-6), the part after `SignalAttackPending`:

1. `0x006A4000` builds a 0x50-byte event (type 0x23 = 35, COMMAND_PENDING) and runs it on the attacker (already in COMBAT.md).
2. `0x006A40D0(&result, &wieldMask)` (`__thiscall` attacker, `ret 8`) reads the main-hand record (`0x006A0160`) and off-hand record (`0x006A0170`), each `[+0]` = result. Returns false (swing fails -1) if both are 0, or either is 4 (deathblow). 2 -> 1 for each hand. `wieldMask` out = 3, except for a creature whose combat data `[creature+0xC60]+0x5C` (= `GetWeaponStyle`, `0x006A3C30`; 1 OW/WS, 2 DW, 3 TW, dev/script_commands/spells_combat.md) is **2 (dual wield)**: then mask = `2*(off result in {1,3,9}) | (main result in {1,3,9})`, i.e. bit0 = main hand landed, bit1 = off hand landed. `result` out = `0x0069F530(main, off)`.
3. **`0x0069F530(a=main, b=off)` (`ret 8`) combines two hands into one result** (this is what you asked about): crit 2 -> 1 in both; a==0 -> b; b==0 -> a; both set: a==1: b in {1,3} -> 1 else 0; a==3: b==1 -> 1, b==3 -> 3, else 0; a==4 -> 1; anything else -> 0. So hit+miss = hit, miss+miss = miss, a lone backstab (9) passes through as 9 (backstab+anything = 0).
4. If a combined result is non-zero, `0x00A3B0D0(vecResult, vecWield, vecTarget(empty), attackerObjectId)` (`ret 0x10`; `this` = handler+0x60) = `0x00A3A440` (key from the three vectors; `0x00A3A340` packs `(res << (wBits+tBits)) | (wield << tBits) | target`) then **`0x00A3AFE0(attackerId, key)`** (the picker). The table is loaded by `0x00A3B240` from the handler's constructor `0x007ABC40..` (`push 0x68` = table 104, columns AttackerResult 0xE4D64BCE, WeaponWield 0x1DE44006, TargetResult 0x37415711; the same loader class is used for table 102 at 0x00747B00).
5. `0x006A41A0(row)` plays it (below). If the picker returns 0 (no row fits, e.g. a result of 5, 6, 7, 8, or backstab+other), `0x006A41A0` finds `HasAnimation(0)` false (`0x00694E70`) and **plays row 107 (Attack v1, id 0x6B)** instead.

**The picker `0x00A3AFE0`** (checked): the list for the key is a vector of the table's `AttackerAnimation` values in table order (map lookup `0x005D0AB0`). State per key (map at manager+0x50, entry `{lastIndex, lastAttackerId}`):
- n = 0 -> 0. n = 1 -> that one, no state.
- n >= 2 and the stored attacker id equals the current attacker's id: n == 2 -> `(last+1) % 2`; n > 2 -> `(last + 1 + rand() % (n-2)) % n` (so it skips the last one and the one before it).
- otherwise (a different attacker last used this key, or none): `rand() % n`.
- stores the index and attacker id. `rand` = `0x008E6A8E` (the CRT's).
It is one state for all creatures (the manager is the handler's), keyed by (result, wield mask, target result): consecutive swings by the same creature never repeat a clip, but other creatures' swings in between reset it to a plain random.
How the key sets are built (rows added to every key whose result/wield/target bits include the row's, in `0x00A3B240`) was read only in outline; the effect (inferred): mask 3 (everyone but a dual-wielder with one hand landing) sees all rows of the right result; mask 1 only wield-1 rows; mask 2 only wield-2 rows.

##### Table 104 `nosynccombatanims` (checked, ghilana; columns Variation, AttackerResult, WeaponWield, TargetResult, AttackerAnimation, TargetAnimation)

| ID | Label | AttackerResult | WeaponWield | AttackerAnimation (anim_combat row) |
|---|---|---|---|---|
| 1 | HIT v1 | 1 | 1 | 107 attk_v1 |
| 2 | HIT v2 | 1 | 1 | 106 attk_v2 |
| 3 | HIT v3 | 1 | 1 | 138 attk_v3 |
| 4 | HIT v4 | 1 | 2 | 139 attk_v4 |
| 5 | HIT v5 | 1 | 2 | 140 attk_v5 |
| 6 | HIT v6 | 1 | 2 | 102 attk_v6 |
| 7 | MISS v1 | 3 | 1 | 105 miss_v1 |
| 8 | MISS v2 | 3 | 1 | 182 miss_v2 |
| 9 | MISS v3 | 3 | 2 | 191 miss_v3 |
| 10 | MISS v4 | 3 | 2 | 192 miss_v4 |
| 11 | BACKSTAB | 9 | 0 | 2006 talent_backstab |
| 12 | Single DEATHBLOW | 4 | 0 | 0 (unused here: result 4 never reaches the swing) |

TargetAnimation is 0 in every row: this table gives the target no animation. The older `synccombatanims` (table 34, "HIT/HIT" pairs with rows 108-119, 153-176, 251 and TargetAnimation) and `halfroundcombatanims` are not read by Swing; I found no reader of the paired-reaction rows (anim_combat rows 112-135, 155-175 are labelled "CUT").

##### anim_combat rows 102, 105-107, 139, 140, 182, 191, 192 (and 106, 138) (checked)

| Row | Label | AnimName | Type | Replacement |
|---|---|---|---|---|
| 107 | Attack v1 | attk_v1 | 64 | 0 |
| 106 | Attack v2 | attk_v2 | 64 | 107 |
| 138 | Attack v3 | attk_v3 | 64 | 107 |
| 139 | Attack v4 | attk_v4 | 64 | 107 |
| 140 | Attack v5 | attk_v5 | 64 | 107 |
| 102 | Attack v6 | attk_v6 | 64 | 107 |
| 105 | Miss v1 | miss_v1 | 64 | 107 |
| 182 | Miss v2 | miss_v2 | 64 | 107 |
| 191 | Miss v3 | miss_v3 | 64 | 107 |
| 192 | Miss v4 | miss_v4 | 64 | 107 |
| 2006 | Talent: Backstab | talent_backstab | 4096 | 107 |

The row's clip depends on the creature's set tree (OW/WS/TW/DW.anb replace the CombatModelTree; ANIMATION.md). Node -> clip, each flags 1 (once), weight 1 (checked, `.anb` dumps):

| Row (node) | OW and WS | TW (also Staff attk_/miss_) | DW | unarmed (combat_humanoid.anb) |
|---|---|---|---|---|
| 107 attk_v1 | sw_atk_v4 | tw_atk_v1 | dw_atk_v1 | ua_atk_v1 |
| 106 attk_v2 | sw_atk_v5 | tw_atk_v2 | dw_atk_v6 | ua_atk_v2 |
| 138 attk_v3 | sw_atk_v6 | tw_atk_v3 | dw_atk_v6 | ua_atk_v1 |
| 139 attk_v4 | sw_atk_v7 | tw_atk_v1 | dw_atk_v4 | ua_atk_v2 |
| 140 attk_v5 | sw_atk_v5 | tw_atk_v2 | dw_atk_v5 | ua_atk_v1 |
| 102 attk_v6 | sw_atk_v6 | tw_atk_v3 | dw_atk_v3 | ua_atk_v2 |
| 105 miss_v1 | sw_atk_v4 | tw_atk_v1 | dw_atk_v1 | ua_atk_v1 |
| 182 miss_v2 | sw_atk_v5 | tw_atk_v2 | dw_atk_v6 | ua_atk_v2 |
| 191 miss_v3 | sw_atk_v6 | tw_atk_v1 | dw_atk_v4 | ua_atk_v1 |
| 192 miss_v4 | sw_atk_v7 | tw_atk_v2 | dw_atk_v5 | ua_atk_v2 |
| 2006 talent_backstab | sw_tal_flnk | tw_tal_flnk | dw_tal_flnk | (none) |

(Staff tree: attk_v1..v6 -> tw_atk_v1,2,3,1,2,3; miss_v1..v4 -> tw_atk_v1,2,1,2. Bow/Cbow trees have no attk_/miss_ nodes. All clips are `mh_c.<name>.ani`.) Dual-wield: variants whose clip events carry "|Left" (dw_atk_v3, v4, v5) are the off-hand swings, matching wield mask 2 (rows 139, 140, 102 and misses 191, 192). In the OW/WS/TW trees (not dual) the mask is always 3, so all 6 hit variants (4 miss) are in the pool; the 2-wield "left" variants there are plain right-hand clips (their events are not "|Left").

A row the creature lacks (non-humanoids with their own trees) is not replaced by the table's `Replacement` column in the swing path; `0x006A41A0` only falls back to row 107 (checked; the Replacement column's own reader is not read here).

##### Backstab, crit, deathblow, ranged, dual wield

- **Crit**: same rows as a hit. The scripts' result 2 is folded to 1 in `0x006A40D0` and `0x0069F530`; the crit differs only in the damage the scripts compute and the floaty/gore. (checked)
- **Backstab**: the scripts return COMBAT_RESULT_BACKSTAB (9) from `Combat_PerformAttack` for melee, no ability, `Combat_CheckBackstab`; `combat_h.nss` ~596, 970 pass it as the main-hand result with off-hand INVALID. -> row 2006, one row, no variation. Its clip carries the impact (sw_tal_flnk 1@0.97 of 1.933 s; tw_tal_flnk 1@0.67 of 2.133 s; dw_tal_flnk 1@1.06 of 1.933 s). (checked)
- **Dual wield**: `SetAttackResult(main result, effect, off result, effect)` for HAND_BOTH (combat_h.nss ~976-990), main-only for HAND_MAIN, off-only for HAND_OFFHAND (a swing with only the off hand: mask 2). Each hand's own impact comes from its own "|Left"-tagged event (0x006A6230 / 0x006AB040 pick the off-hand record when the event string contains "Left", string at 0x00B0F840). (checked)
- **Deathblow (the synchronized kill)** is NOT table 104. The scripts' `Combat_PerformAttack` returns result 4 with `nDeathblowType` 0 ("auto select in engine") or 5 (ogre slowmo) for a party member's killing blow (combat_h.nss ~700-810), `SetAttackResult` then `CommandDeathBlow` at the queue's front (`WR_AddCommand(...)`, combat_h.nss ~955). The command's start handler (the function around 0x00745F00..0x00746300, not fully read) picks a row of **table 102 `syncdeathanimations`** (columns WeaponWield 1 = WS, 2 = DW, 3 = TW, Partsbased, Position, Appearance, PiercingType, CastAnim, React) with the key (target's apr_base `ValidDeathBlows`, hash 0x8A442AFE, read at 0x0074603F; weapon style via `0x006A3780`; the weapon's piercing/bladed flag), then **`0x00A3A510` (key + `0x00A3A4A0`: plain `rand() % n`, no anti-repeat)**, reading the same table class (`push 0x66` at 0x00747B00; columns Appearance 0x052BEA01, WeaponWield 0x1DE44006, PiercingType 0x97F088C1). It plays `CastAnim` on the attacker and `React` on the target (anim_base rows, e.g. WS humanoid V1 300 / 306, V2 301 / 308, bladed V3 318 / 321, decap 327 / 330, beast bladed 341 / 268; DW 304/314, 305/316, 329/334, 320/325, 343/270, 338/366; TW 302/310, 303/312, 319/323, 328/332, 342/269, 337/365; positional 346-350 / 351-355). Their next rows (loops 307, 309 ... and 263 getup) are the rows' TransitionTo/Replacement. The kill lands from **event 30 (DeathblowDamage)** in the *target's* React clip (`0x006A62F0` builds a 0x7C-byte script event of type 4 (read as EVENT_TYPE_DEATH) with int 0 = the event string's integer or 0x3F7; sw_dblow_die_v1 30@0.57, v2 30@0.57, v3 30@0.5 and 1.13, tw_dblow_die_v2 30@0.67 and 1.73, v3 30@1.03 and 2.23, dw_dblow_die_v1 30@0.33 and 0.9, v2 30@0.37 and 0.43, v3 30@0.57 + event 29 DecapitateVFX @0.73), not from the attacker's clip, which has no event 1 (checked in `.evt`). The row-selection function's details (which column wins when several rows fit) were not read.
- **Ranged shot**: no table 104. The attack command's sub-action 50 queues turn (39) then sub-action 2 (`0x007A6490`, COMMAND_PENDING), then `0x007A4DD0` queues 37 Aim (anim_base row 249, `aim`), 6 aim wait, 37 Fire (anim_base row 243, `fire`); loops 242 `aimloop`, withdraw 248 (`src/dao/Animation.cpp`, DAOLib #51). Bow clips `mh_c.bw_aim/_aim_lp/_fire/_wdrw`, crossbow `cbw_`, staff `stf_spl_d_cst` (ua_dcast) as `fire`. The impact is the projectile's (ATTACK_IMPACT from `0x007E87C0`, COMBAT.md), via events 9 ProjectileLoad / 10 ProjectileFire. Talent shots use rows 2007 / 2021 / 2000 (`talent_ranged_v1..v3`). (checked for rows and the trees; the event times of bw_fire are not read here.)

##### Play speed and length (read at `0x006A41A0`; the numbers are checked, the reading of the speed is inferred)

`0x006A41A0(row)` (`ret 4`): row replaced by 107 if `HasAnimation(row)` (0x00694E70) is false. L = `0x00694E50(row)` (clip length, ms, from `Animation::...` at 0x00A5F7A0). `blend(x)` = `0x00694FE0(x, -2.0f)` is about 20% of x (`x*20/100` in the common branch). T = `[creature+0xC60]+0x64` (what `SetAttackDuration` stored, ms). With T <= 0: play speed 1.0, the step lasts L - blend(L), and the fade-in rate is 1000/blend(L). With T > 0: speed = (L - blend(T)) / T, the step lasts T. It calls the model's play slot (+0xB0) with row, speed, fade rate. The wall time of an event = event time / speed. (`Animation.cpp` records the same: scripted time for a humanoid with a weapon, else 0.8 L.)

#### 2. Event times of the swing clips (checked: `.evt`, `.gad` length; events by id: 2 AttackImpactReaction, 19 Swish, 1 AttackImpact; seconds in clip time, before the speed above)

| Clip | Length | event 2 (target reacts) | event 19 swish | event 1 (impact) |
|---|---|---|---|---|
| mh_c.sw_atk_v3 (unused by trees) | 1.833 | 0.30 | 0.57 | 0.70 |
| mh_c.sw_atk_v4 (OW/WS attk_v1, miss_v1) | 2.000 | 0.63 | 0.70 | 0.93 |
| mh_c.sw_atk_v5 (attk_v2, v5; miss_v2) | 2.000 | 0.80 | 0.90 | 1.10 |
| mh_c.sw_atk_v6 (attk_v3, v6; miss_v3) | 1.967 | 0.50 | 0.57 | 0.80 |
| mh_c.sw_atk_v7 (attk_v4; miss_v4) | 1.967 | 0.77 | 0.87 | 1.07 |
| mh_c.dw_atk_v1 | 1.467 | 0.20 | 0.37 | 0.50 |
| mh_c.dw_atk_v3 (|Left) | 1.467 | 0.40 | 0.57 | 0.70 |
| mh_c.dw_atk_v4 (|Left) | 1.467 | 0.30 | 0.47 | 0.60 |
| mh_c.dw_atk_v5 (|Left) | 1.467 | 0.23 | 0.37 | 0.53 |
| mh_c.dw_atk_v6 | 1.467 | 0.43 | 0.53 | 0.73 |
| mh_c.ua_atk_v1 (unarmed) | 1.500 | none | none | 0.63 |
| mh_c.ua_atk_v2 | 1.533 | none | none | 0.60 |
| mh_c.tw_atk_v1 | 2.300 | 0.90 | (a custom sound 0.93) | 1.27 |
| mh_c.tw_atk_v2 | 2.433 | 0.83 | none | 1.20 |
| mh_c.tw_atk_v3 | 2.500 | 0.73 | 0.30 | 1.07 |
| mh_c.sw_tal_flnk (backstab, OW/WS) | 1.933 | none | 0.73 | 0.97 |
| mh_c.tw_tal_flnk | 2.133 | none | none | 0.67 |
| mh_c.dw_tal_flnk | 1.933 | none | 0.89 | 1.06 |

(Unarmed clips have no event 2 at all, so an unarmed attacker's miss makes no evade reaction. sw_atk_v2 has event 2 twice, 0.80 and 0.87, and event 1 at 1.20; sw_atk_v1, also unused, 0.53 and 0.97.) The impact is the engine's event 1 -> `CCCreature::OnAnimationEvent` 0x006C0BF0 (id -1 into a byte table at 0x006C0DB0 and a jump table at 0x006C0D70: id 1 -> 0x006A6230 -> 0x006A6080 -> ATTACK_IMPACT, id 2 -> 0x006AB040, ids 3-6 equip/unequip, 7 -> 0x006AF430 AbilityCast, 21 -> 0x006A5B70 WeaponBlocked, 28 -> 0x006AAC20 Decapitate, 29 -> 0x006A5C10, 30 -> 0x006A62F0, 31 -> 0x006AABB0, 32 -> 0x006BFC80, 34/37 -> 0x0069E5B0 attach point; the case mapping is checked, the handlers' purposes other than 1, 2, 30 are not read).

#### 3. Reactions on the target (checked unless marked)

##### Evade / miss (the engine, event 2)

Function chain: attacker's clip event 2 -> `OnAnimationEvent` -> `0x006AB040(event)` (picks the attacker's off-hand record `0x006A0170` if the event string contains "Left", else main `0x006A0160`) -> **`0x006AAE20(this = attacker, event, handRecord)`** (`ret 8`, frame `ebp`). Hand record: `[+0]` result, `[+4]` target id, `[+8]` effect id.

Gates, in order (all must hold, else nothing plays):
1. event and record non-null; `record.result == 3` (MISS). A hit, crit, backstab or block (5) plays nothing here.
2. the target `esi = GetCreatureById(record.target)` exists (`0x005D58C0`) and `HasAnimation(0xB5 = 181)` is true (so a target without the Evade v1 row never evades; row 176 is then also reached through its Replacement 181).
3. the target's own command state: `0x0073C600(target)` is the type of its head command (byte at +2; 0 = none) and `0x0073C7B0` another field of it.
   - no command (0): require the attacker's `[attacker+0x2CC]` object's `+8` (`0x007B51E0`) to be 0: the attacker's own command queue empty, which an attacker mid-attack seldom is, so an idle target seldom evades [inferred];
   - `0x0073C7B0` is `CCCreature::ForScript_IsConjuring` (ID 17541): the type of the running sub-action of the target's current command, 0 with none (read 2026-10-20 by lib-agent-2). So `== 1` with an attack command means the target is in its Swing sub-action (COMBAT.md's sub-actions: 4 and 15 approach, 50 the attack, 39 turn to face, 1 the swing);
   - an attack command (type 1) with `0x0073C7B0 == 1`: require the target's combat data target (`[target+0xC60]+4`) to be the attacker's id, `0x006A2AD0(target)` == 0 (that function is true if the target's main-hand item's base item `Type` (bitm_base column hash 0x12A02374) is 5 or 6: a staff or ranged weapon), and the target's own hand results (`0x006A0160/0x006A0170`) each be 0 or 3;
   - any other command type: no evade.
4. melee reach: `0x006A3DC0(attacker, target)` true: the target is a creature (`[target+0x14] == 6`) and the xy distance is within `0x00695320(a, t)` + 1.2 (constant 0x00AFEBC0) (so it never fires for ranged attacks, whose impact is a projectile anyway).
5. facing: the angle (`0x004D4C90`, radians x 57.29582) between the target's facing (`target+0x130`) and the direction target -> attacker (positions at +0x120) is **<= 30 degrees** (constant 0x00AF7068 = 30.0). The target must be facing its attacker.

If all hold: row = 181 (0xB5) normally; `call 0x008E6A8E` (rand), `eax & 0x80000001` then sign-fixup; if the result is 0 (even rand) the row becomes **176 (0xB0, Evade v2)**. So 50/50 (CRT rand parity). Then a parameter block `{ int0 = row, int7?(index 1) = 1, int 1 = 1, float 3 = 1.0 }` (ctor 0x0069F670) goes to **`0x0073D150(target, block, 1, 0, 2)`**, which builds a command of **type 14 (COMMAND_TYPE_PLAY_ANIMATION, the same as `CommandPlayAnimation`)** and adds it through `CCObject::QueueNewCommand` (0x0073D000, ID 17553) as (type 14, params, addToFront 1, static 0, playerIssued 0, override 2, word 1) (read 2026-10-20 by lib-agent-2): **at the front, not static, and override 2, a hard clear** (`ClearAllCommands(1, 0)`, 0x0073C930, before the add). So the target's running command, its own swing or move, ends with COMMAND_FAILED_COMMAND_CLEARED, its queue is dropped, and the evade, an ordinary PlayAnimation step (sub-action 37), is its only command.
- **So a target evades** when its attacker's blow is a miss, it is within melee reach and faces the attacker within 30°, and it is either idle (with the attacker's queue empty) or **in the Swing sub-action of its own attack aimed at this attacker, with a melee weapon, and both its hands' results still none (0) or a miss (3).** It never evades while walking up to attack (sub-actions 4, 50, 39), using an ability, moving, or when its own swing is a hit (results are set before the swing plays). The evade's hard clear cancels its own swing.

Rows (anim_combat): **181 `Evade v1`** (`evade_v1`, Replacement 1), **176 `Evade v2`** (`evade_v2`, Replacement 181). Clips by the target's set tree (OW/WS/TW/DW.anb replace the CombatModelTree; combat_humanoid.anb is the fallback for no set):

| Target set | evade_v1 (row 181) | evade_v2 (row 176) | lengths |
|---|---|---|---|
| OW | mh_c.rct_ow_block | mh_c.rct_ev_v1 | block 0.967 s, ev_v1 0.933 s |
| WS | mh_c.rct_sw_block | mh_c.rct_ev_v1 | 0.933 s |
| TW | mh_c.rct_tw_block_v2 | mh_c.rct_tw_block_v2 | 0.967 s |
| DW | mh_c.rct_dw_block | mh_c.rct_ev_v1 | 0.933 s |
| none/unarmed (combat_humanoid) | mh_c.rct_ev_v1 | mh_c.rct_ev_v1 | 0.933 s |

So the "block" (weapon or shield parry, with a WeaponBlocked sound event 21 at 0.33 s of each block clip) and the "dodge" (ev_v1) are the same two rows; there is no separate shield-block, deflect or parry row. COMBAT_RESULT_BLOCKED (5) is converted to MISS by the scripts (combat_h.nss:1073-1079, "We retired combat result blocked"). A `Replacement` of 1 means "none, play nothing" (as `anim_combat` elsewhere).

Plus the scripts: `combat_h.nss:403-411` (displacement/Evasion talent) applies `Effect(1055)` on the target on certain misses (Evasion only plays during wait/attack commands): an effect, not a row.

##### Hit reaction (the scripts, at the impact)

At event 1 (ATTACK_IMPACT) `rules_core` -> `Combat_HandleAttackImpact` -> `Effects_ApplyInstantEffectDamage` (`effect_damage_h.nss`, lines 28 and 738-755): for a creature target and a non-fatal blow, **`if (fDamage >= 3.0f) PlayAdditiveAnimation(oTarget, ANIMATION_DAMAGE_ADDITIVE = 103)`**. A fatal blow plays nothing here (KillCreature -> the death command, dev/COMBAT.md). A placeable target gets 103 for any non-fatal damage (line 942). Nothing else in the scripts plays a hit reaction (grep of `PlayAdditiveAnimation` in all of `toolset\extract\scripts`: only these two sites).

Engine side of `PlayAdditiveAnimation` (command 196, handler 0x00659300; dev/script_commands/objects.md "PlayAdditiveAnimation"): plays at once on the model, no command: it does nothing if the object's current animation row has `State` = 1 (column hash 0xE991BD51; e.g. rows 2, 3, 250, 254, 268-283), and without bForceRestart only if the additive is not already playing (`0x00A5F810(anim,0)` == 0.0). The scripts pass no force, so a second hit while 103 plays does not restart it.

Row 103 `Damage` (`bdmg`, Type 1024 = additive, State 0, Replacement 0): the node `bdmg` is in **`humanoid_overlay.anb`** -> clip **`mh_o.rct_odmg_v1`**, 1.30 s, flags 1, no events (checked). It is an overlay (additive) drawn over whatever the body plays, not a body clip. Non-humanoids need a `bdmg` node in their own overlay tree to show it. Rows 141 (`hvyDam`, Replacement 103; `humanoid.anb` node -> `mh_c.rct_hdmg_f`, 1.30 s), 142, 143 (`bdmg_v2/v3`) exist in the tables and the tree, but nothing I found plays them (no `push 0x8D/0x8E/0x8F` in the code is a row id: they are table ids; the scripts never name them).

Conditions: no per-appearance column such as a "no hit reaction" flag is read for this: the engine has no hash for `SynchronizedCombat` (0xCDD753B2, not in the exe), and the damage gate is the script's 3.0. The only suppressions are the State-1 test and the not-already-playing test above, and a model with no `bdmg` node.

#### 4. When, in order (one melee swing, checked except where marked)

1. COMMAND_PENDING (the scripts roll the blow, SetAttackResult, SetAttackDuration T, SetCommandResult).
2. `Swing` picks and plays the row (0x006A41A0): the clip starts at speed (L - 0.2T)/T (inferred reading), fading in over 0.2L.
3. Event 2 at t2 (clip time; wall t2/speed): if the result is a miss and the gates hold, the target gets the Evade command and starts the block/dodge clip at once (0.2-0.4 s of clip time before the impact).
4. Event 1 at t1: ATTACK_IMPACT to the attacker's script (queued with no delay) -> damage effect applied -> for a non-fatal damage >= 3.0 the target starts overlay 103 (1.3 s) in the same frame as the damage, not before.
5. At T (or 0.8 L) the swing sub-action ends; COMMAND_COMPLETE.

So a hit's reaction starts at the impact event, and a miss's evade starts at event 2, before it; a hit never plays the evade, a miss never plays 103.

#### Not read

- The deathblow command's row choice: read 2026-10-20, "The deathblow" below.
- The key-set construction in `0x00A3B240` (I took the subset semantics from its loops' shape).
- `0x007B51E0`'s object; `0x006A3A30` (gate of event 30); QueueCommand's event 0x5e payload and its type-0x19 branch; the command's word (+8) and the block's int 7; that an idle target never evades in play.
- Ranged shots' impact event times (`bw_fire` etc.) and the staff's basic-attack path (the staff tree maps attk_ to the tw_atk clips; whether the staff attacks as ranged in play was not checked).
- Creatures with their own trees: the wolf's, spider's and ogre's read 2026-10-20, "Monsters' attack trees" below; dragons' and the rest not.
- Not seen in play.

### Effects' animations, the deathblow, and monsters' attack trees (read 2026-10-20 by lib-agent-2, for DAOParagonEngine #33; checked unless marked)

Nothing here was run in the game. The knockdown's times agree with what DAOTurnBased measured in play.

#### Effects' animations: knockdown, slip, knockback, stun, sleep, paralysis

- **No knockdown flag, and the scripts play nothing.** Each condition is its own effect type, and the engine takes its animation and priority from `effects.gda` (table 136), columns `Animation` and `Priority` (hashes 0x43C8D895, 0x76E5BBCB), copied onto the effect when it is made (the CGameEffect constructor 0x00A650C0: +0x34 animation, +0x38 priority, +0x3C flags, for types 12 and up; the native types 1–11 set theirs in their handlers). Rows spot-checked: 8, 1014, 1027, 1033, 1037.

  | Condition | Effect type | Priority | Row | Humanoid clip |
  |---|---|---|---|---|
  | Knockdown | KNOCKDOWN 1014 | 981 | 148 `talent_chargereaction` | `mh_c.rct_kbck_v1`, 1.367 s, one-shot, TransitionTo 151 |
  | Slip | SLIP 1051 | 6 | 284 `slip` | `mh_c.rct_slp`, 3.767 s, the fall and the rise in one clip |
  | Knockback | KNOCKBACK 1057 | 4 | 273 `knockback` | `mh_c.rct_kbck`, 1.2 s |
  | Stun | STUN 1027 | 1 | 271 `dazed` | `shared.dazed`, 4.967 s, a loop |
  | Sleep (the spell) | SLEEP 1033 | 2 | 271 `dazed` | the same loop, standing |
  | Sleep (plot, `ForceSleepStart`) | SLEEP_PLOT 1037 | 2 | 250 `die` | `mh_c.sw_die_v1` 2.1 s, then `dieloop`, lying |
  | Paralysis | PARALYZE 8 (native) | 998 (its handler) | none | no clip: the animation controller is paused |
  | Root | ROOT 1003 | 980 | 286 `root` | `mh_c.rct_root`, 4.867 s |
  | Heavy impact | HEAVY_IMPACT 1032 | 990 | 141 `hvyDam` | `mh_c.rct_hdmg_f`, 1.3 s |
  | Fear | FEAR 1036 | 995 | 287 `horror` | `mh_c.rct_horr`, 4.933 s |
  | Death, feign death | 7 / 1041 | 999 / 997 | 250 | die, dieloop |

- **ExecuteEffect (command 32), sub-action 51 (0x007AAC10).** Adding an effect whose priority (0x00A65380: 999 for duration type 6, 0 for an animation of 0 and type 12 or more, else +0x38) is above 0 queues command 32 at the front, clearing the queue (0x006977C0 → 0x00695F20 → 0x0073D000). Each tick the sub-action takes the creature's top-priority effect (creature +0x158, 0x004621C0) and shows its animation, and it ends when no effect is left to show.
  - A one-shot start clip is played with a blend of 2.5 (a 0.4 s fade), and the step lasts its length. A loop (anim_base Type bit 0) is played with −2.0 (20 % of the step) and no timer.
  - When the effect ends, the sub-action plays the current animation's TransitionTo (0x005CA6B0) through 0x007A5E80, which grows the step by that clip's length; a loop ends in a crossfade into `pause`.
  - **Knockdown on a humanoid:** 148 (1.367 s) → by its TransitionTo, 151 `talent_charge_react_loop` (`mh_c.rct_kdwn_lp_v1`, one frame looped) while the effect stands → on removal 152 `talent_charge_react_getup` (`mh_c.rct_kdwn_up_v1`, 2.133 s) → pause. The step is 1,366 ms while the effect stands and 3,499 ms with the get-up, as DAOTurnBased saw in play ("51 (1366 of 3499 ms)").
  - **The knockdown's own duration is the script's.** Fireball (`spell_aoe_instant.nss`:205–209) applies it for `RandomFloat()` seconds, after a failed physical resistance check. `RandomFloat` (767, 0x0064EB20) is an LCG with its own seed at 0xC349AC: s = s·214013 + 2531011, then ((s >> 16) & 0x7FFF) / 32767, so 0 to 1 s. Neither `effects.gda` nor `effect_knockdown_h` changes it, and ageing starts at the effect's first update (0x007A0B10 sets +0x50). The fall's 1,367 ms step plays out even when the effect has already ended, so the creature is down about 1.4 s and then gets up (read 2026-10-21 by lib-agent-2).
    - **The get-up comes after the script's duration plus the fall's clip; settled 2026-10-21 by lib-agent-2.** The cause was found while reading #144 (COMBAT.md, "An instant effect with an animation becomes temporary").
      - **The engine lengthens the effect** [C, the apply 0x007A0E30 at 0x007A1235..0x007A125D]. For an effect with priority > 0 (knockdown 981) and duration type 2 (TEMPORARY), after the APPLY_EFFECT script has run, it adds the length of the effect's animation in the target's tree (`0x00A5F7A0`, ms) to the duration (+0x2C).
      - **One exception** [C for the test, I for its meaning]: the length is not added when `0x00A5E940(anim)` answers true, which looks like a looping-clip test.
      - **So a knockdown of d seconds stands for d + 1.367 s on a human male.** Paragon-agent's log fits: a 0.5 s knockdown played 152 (get-up) at 1.85 s against 0.5 + 1.367 = 1.867 s. DAOTurnBased's 1.4–2.3 s for Shield Bash's 0–1 s fits too.
      - **What the code read earlier still holds**: ageing starts at once, and sub-action 51 writes none of the effect's clock. The duration itself is longer.
      - **An INSTANT knockdown** becomes TEMPORARY with the clip's length alone (the same apply, 0x007A1202..0x007A1225).
  - **Stun and the sleep spell** hold the `dazed` loop for the effect's whole time, then crossfade into `pause` (about 2 s of `pause`'s 9.97 s step) with no get-up clip. **Plot sleep** is die → dieloop, then 263 `diegetup` (`mh_c.sw_diegup_v1`, 2.4 s).
  - **Paralysis plays no clip.** Its apply (0x007A05B0) sets priority 998, plays its effect's visual and queues command 32. The sub-action calls the creature's slot 0x4C(1), `CCreatureAnimController::Pause` (0x006C9E60), freezing it in whatever pose it has; on removal slot 0x4C(0) (0x006CAB00) resumes it. Paralysis (998) outranks a knockdown (981).
- **Other creatures' trees** have the same rows by AnimName: bear, dragon, golem and deep stalker have `kd_enter`, `kd_lp` and `kd_exit` (the getting-up times of 1.5–3.8 s DAOTurnBased saw); bronto, brood mother and high dragon have no knockdown nodes. Their lengths were not read.
- Not read: the bodies of 0x007A5E80, 0x007A4D50 and 0x007A6DA0 (the get-up's blend, a one-shot's timer through 0x007A7290); what 0x00695700 and 0x00696060 return for the start clip; what plays when a higher-priority effect goes while a lower one stands (inferred: the lower one's start clip again); the cause of the dragon's fall and rise after a knockback (by the table, likely a second effect on the same blow); RECURRING_KNOCKDOWN (2001, not in `effects.gda`); add-ins' effects rows.

#### The deathblow

- **The start handler is 0x00745E10** (ID 17720, `ret 8`), slot 38 of the first handler table 0x00747480 fills; its end is 0x00747360 (cited from DAOTurnBased: it takes the striker out of the slow-motion set).
- **The row:** if the script's deathblow type is not 0 it is table 102's row itself, with no lookup (19–23 the five positional rows, `Position` = type − 0x12; 5 BEAST_SW_DBLOW_V1). Type 0 ("auto") looks the row up by an **exact match** on five columns, then takes one of the matches by `rand() % n` (0x00A3A510 → 0x00A3A4A0, the CRT's `rand`, only for n > 1; no weights, nothing against repeats):
  - `WeaponWield`: the attacker's weapon style (combat record +0x5C: 1 one-handed or weapon and shield, 2 dual, 3 two-handed), 0 for a positional row.
  - `PiercingType`: from the attacker's weapon's base item (table 6, column 0x97F088C1): 1 (blades, bows) gives the table's 1, 2 (blunt weapons, creature weapons) its 0; any other value means no sync kill.
  - `Partsbased`: 1 only when a gore option byte is set (0x00838210 on `[0xC32938]+0x88`, which option is not identified) **and** the target's apr_base `Neck_Model` (0xDC9C928D) is not empty (the human, elf, dwarf, hurlock and genlock heads have one).
  - `Appearance` and `Position`: 0 for a humanoid target. Otherwise apr_base's `ValidDeathBlows` (0x8A442AFE): −1 no deathblow; 0 the humanoid rows; 1 the beast rows; 2 `Appearance` 1 and a positional row, `Position` = max(0x006A3780, 1), the best of tables 118–122 ("Sync Large Creatures 1..5") for the target's appearance.
  - **What follows** [I, from the code and the data, not played]: a humanoid killed with a blunt weapon gets V1 or V2 at random; with a blade, always V3, or with gore and a neck always the decapitation row; a blunt weapon with gore and a neck matches no row and falls back. A beast's kill is fixed by weapon style and blade or blunt. Ogres have `ValidDeathBlows` 1 (the beast rows); the positional ones (2) are the brood mother, Wild Sylvan, high dragon, pride demon, archdemon and Grand Oak.
- **No row, or no valid victim:** the attacker plays anim 107, and the target gets table 103's row 1 (SIMPLE_DEATH_V2, React 250) as a plain death.
- **The two sides:** the target is given a command of type 7 whose handler (0x007A12E0 with 0x005CA740) plays the row's `React`, or table 103's for the fallback. The attacker plays the row's `CastAnim` through sub-action 37 (0x007A5FC0 → `AddCommand(0x25)`) with a placement record, and is added to the slow-motion set (0x00745DF0).
- **The placement** (0x007A5A30): **the target is not moved or turned.** The attacker slides over **0.15 s** (0x0074639D) to the point on the line between them at PersonalSpaces(target, attacker) + 1.2 m from the target (0x00695320; 1.2 at 0x00AFEBC0), or, for a large creature with a row in tables 118–122, to that row's spot in the target's frame (Percentage, Distance, AngleATK, AngleTarg). No walk-grid or collision test was seen. The vector's maths is [I].
- Not read: the gore option byte; the guards 0x0073C5F0 (a target's current command of type 0x20), 0x0079F4A0, 0x006956B0 → 0x005CA710, 0x006A51C0, the second `AddCommand(…, 5, …)` at 0x007462C7 and 0x007EED00 (when `Position` is set); 0x006A3780's numbers; whether sub-action 37's move is cut by the grid; no row choice was played.

#### Monsters' attack trees: the wolf, spider and ogre

- **Their trees** (apr_base): wolves, the mabari and the party dog use `C_DOGa_*` (clips `c_dog.*`); the giant, corrupted and poisonous spiders `C_SPIDERa_*`; Ogre A `c_DSOgre_*`. The model tree holds idle, death and ambient clips; the combat tree the attacks, talents, stun, knockdown and deathblow reactions; the overlay tree only `bdmg`; the override tree a humanoid's clips played against this monster (its deathblows, the victim's side of a grab) [I, from the node names].
- **No monster swing has event 2** (none of the wolf's, spider's or ogre's `atk_v1`–`v3`), so **a monster's miss makes the target play no evade**; only event 1 lands the blow. A miss plays the same clip as a hit.

  | Rows | Wolf | Spider | Ogre |
  |---|---|---|---|
  | 107, 139, 105 (attk_v1, v4, miss_v1) | atk_v1 2.300 s, event 1 at 0.70 | atk_v1 1.833, 0.57 | atk_v1 2.967, 1.23 |
  | 106, 140, 182, 192 (attk_v2, v5, miss_v2, v4) | atk_v2 2.300, 0.97 | atk_v2 1.900, 0.77 | atk_v2 2.967, 1.10 |
  | 138, 102, 191 (attk_v3, v6, miss_v3) | atk_v3 1.600, 0.50 | atk_v3 1.400, 0.70 | atk_v3 2.967, 0.97 |

  `c_dsogre.atk_v1.evt` spot-checked by lib-agent-2: event 1 at 1.23 s, no event 2.
- **A player's miss against them:** only the spider has `evade_v1` (`c_spidera.ev_v1`, 1.533 s); on the wolf and the ogre, rows 181 and 176 fall through their Replacement to `pause`, no visible evade [I, from Anb.h's rule].
- **Reactions:** `bdmg` (wolf 0.667 s, spider 0.500, ogre 0.467), `hvyDam` (1.167, 0.833, 1.467), knockback (1.333, 1.200, 1.400), `dazed` (3.000, 2.267, 3.300). Knockdown: the wolf (`kd_enter` 0.933, `kd_lp`, `kd_exit` 1.500) and the spider (0.767, 0.967, 1.667) have rows 148, 151 and 152; **the ogre's tree has no such nodes** (its `kd_enter` 2.600 and `kd_exit` 3.800 are named by no row), so how an ogre's knockdown reaches them is not read.
- **Death** (row 250): wolf `die_v1` 1.300 s, spider 2.800, ogre 3.300, each with a bodyfall event. Deathblow reactions on a monster: the ogre's bladed one `dblw_dmg_v1` 12.500 s (the 12.5 s DAOTurnBased measured), its blunt one 4.633; the wolf's 2.400 or 3.567; the spider's 3.667.
- **Talents** (abi_base `CastAnim` → anim row → node; event 7 the impact, 9 and 10 a projectile's load and launch):
  - Ogre: Hurl (row 2007) `tal_hurl` 5.233 s, 9 at 1.1 and 10 at 4.1; Ram (147) `tal_ram` 4.633, 7 at 1.63; Stomp (2016) 4.133, 7 at 1.73; Sweep (2014) 4.633, 7 at 1.5; Roar (2023) 3.500, no impact; the four large positional swings (rows 183–186, abilities 90062–90065) 2.967 s with event 7, not 1.
  - Ogre Grab (90036, scripted): the grabber plays 2009 `tal_grb` 2.100 → 2010 loop → 2011 exit; the first loop's event 7 applies GRABBED, each later one is a punch. The victim (a humanoid, from the override tree) plays `mh_c.cr_ogr_ent` → loop → exit.
  - Wolf: Charge (147) `c_dog.charge` 1.833, 7 at 0.67 and 1 at 0.73; Shred (2020) 3.333, 7 at 1.0; Lunge (149) 3.467, 7 at 0.23; Overpower (scripted, 2019 → 2053 → 2054), the victim playing `mh_c.h_ovlm_*_do` with event 30 at each blow.
  - Spider: Overwhelm (scripted, 2019 `tal_ovlm_en` 4.100 → loop → exit), victim `mh_c.h_ovlm_*_sp`; Web (2021) `tal_dbuf` 2.433, 9 at 1.13 and 10 at 1.23; Poison Spit (2007) `tal_spit` 1.867, 9 at 0.6 and 10 at 0.63.
- Not read: how the ogre's knockdown reaches `kd_enter`/`kd_exit`; the Stone Prisoner add-in's own wolf tree and howl (enciphered); the swing speed for monsters (their `SetAttackDuration`); which of a creature's trees the engine searches first; each creature template's abilities; the projectiles table; deep stalkers and other trees.

### Conversation gestures on a line with no cut (anim2talk), read 2026-10-18 by lib-agent-2

GOG exe 1.05 (base 0x400000), no Ghidra, no game. "checked" = read in disassembly or data; "inferred" = not. Scratch: `%TEMP%\lib-agent-2-anim2talk-*`. Builds on lib-agent-2-audio-vo.md (ShowLine's non-cut branch), lib-agent-2-anim-layers.md (look-at), lib-agent-2-anim-fade.md (slot 44, blend codes).

#### Short answer

1. **A no-cut line animates its speaker only by what the .dlg line says, and mostly says nothing.** The only body-gesture field is the line's `ConversationLineAnimation` (12213, U32, a row id of `anim_base`; 0xFFFFFFFF = none). There is **no emotion, gesture, slide or random-pick field** on the line (NTRY/RPLY templates, checked with `gff4.py --templates`: StageName, StageAtCurrentLocation, Text, Speaker, Listener, GameLanguage, Icon, Visibility, Ambient, Cond, Action, Cut, Animation, Skip, FastPath, NoVOIngame, RevertAnim, SlideShowTexture, ChildrenList, Active). Of the 88,632 no-cut lines in the 1,721 campaign conversations, **only 350 have an Animation**; the other ~88,300 play no gesture: the speaker keeps whatever it was doing (its idle/pause, an ambient loop, a previous line's looping gesture), plus FaceFX lip-sync, plus the look-at. Gestures in vanilla dialogues come from the cut (authored cutscene actions), not from this path. Checked (data scan of all 1,721 .dlg).
2. **How it is played (checked, ShowLine 0x0068E250 at 0x0068E973-0x0068E997):** `speaker->vtable[0xB0](animId, speed 1.0, blend -2.0, 1)`, i.e. the creature's slot 44 (0x006AA7E0 -> 0x0069B3D0) with exactly the exit-clip arguments of sub-action 37: blend -2.0 is a code = fade over **20 % of the clip's step length** (lib-agent-2-anim-fade.md), speed 1.0, last arg 1. It is a plain base-layer play, not an overlay; looping rows loop until something replaces them.
3. **Face:** the VO line's FaceFX animation (`<strref>_m.fxe`, from `modulefacialanimations.erf`) through `PlayVO` 0x00A5DA60 -> FacialAnimationPlayer slot 6 (0x0084EEF0). No per-emotion facial pose exists in the line; the face is whatever the .fxe authors. FaceFX output sits after the whole body pose (node rotation = FaceFX * animation).
4. **Look-at (checked):** at line start the speaker and the **controlled creature** (not the line's Listener tag) are made to look at each other with `SetLookAtTarget(id, slot 1)` (0x0069EAA0), controllers resumed (mode 1, automatic). Cleared at the end of the conversation (`SetLookAtTarget(-1, 1)` at 0x007F108A).
5. **Stage (.stg):** not used by a no-cut line. `ConversationStageName` (12100) / `StageAtCurrentLocation` (12104) are read only inside the cut branch (0x0068E584 `0x007D2AA0` runs after the cut player exists). Checked.

#### (a) Body gestures

##### The field and who chooses
- `ConversationLineAnimation` 12213, read by `0x007F0050(line)`; `ConversationLineRevertAnim` 12217 (U8, 255 or 0), read by `0x007F00A0`. Chosen **by the .dlg author per line**; nothing random, no list, no emotion mapping. The Paragon port needs no picker: play that row.
- Table: it is an `anim_base` id (m2da "Animations", `anim_base.gda`, columns ID, Brow, StrRef, Type, AnimName, WieldType, SynchAnim, TransitionTo, Priority, DisableLookAt, State, Replacement). The id is passed straight to slot 44.
- Distribution over the 350 animated no-cut lines (checked, my scan): 820 Talk Sad 42, 824 Headshake 35, 817 Talk Speak 25, 818 Talk Reaction 17, 822 Talk Angry 15, 825 Nodding 15, 811 Pound Fist 12, 658 Guard Stand at Attention 12, 823 Headache 11, 607 Greeting Slight Head Bow 10, 821 Talk Confident 9, 287 Horror 9, 3019 Talk Dismissive 8, 808 Point Forward 7, 813 Looking Right 6, 850 Salute 6, 854 Point Forward 2 6, 644 Idle Twitch v5 6, then a tail of the dg_* (800-854), dg_tk_* (3003-3030), amb* (606-660) and a few wi_/am_ rows (905, 906, 919, 941, 986, 978, 3002). Also on 4 cut-carrying lines (803, 820, 850, 655), where the cut path does not use it (checked: only the non-cut branch calls the play).
- Rows (checked, `anim_base`): 800-854 are the "dg" (dialogue gesture) set. Type **5** (loop) rows: 815-822 `Talk Normal 1/2, Speak, Reaction, Cursing, Sad, Confident, Angry` (AnimName dg_tk_norm_1, dg_tk_norm_2, dgtalkspeak, dgtalkreaction, dgtalkcursing, dgtalksad, dgtalkconfident, dgtalkangry), 844 Hands Behind Back, 3003 Preach, 3011-3023 Begging/Barter/Beseech/Normal 3-4/Bossy/Contradict/Dismissive/Listen/Nervous/Salesman/Beckon, 3030 Madman. Type **4** (one-shot) rows: 808-814 points/pound fist/stop/looking, 823 Headache, 824 Headshake, 825 Nodding, 826-843 body gestures (Shrug v1/v2 842-843, Hand*, Raised Finger, Surrender 827...), 850 Salute. TransitionTo is 1 (the idle `pause`) on all the talk/gesture rows except the enter/loop/exit chains (800-802, 803-805, 844/848/849). DisableLookAt is 1 only on headshake/nod/headache/bless/salute rows (the head-moving clips), 0 on the talk loops and arm gestures.
- Clips: the blend tree node names map to files, e.g. `dgtalksad` -> `mh.dg_tk_sad.ani`, `dg_tk_norm_1` -> `mh.dg_tk_norm.ani`, `dgheadshake` -> `mh.dg_tk_shake.ani` (humanoid.anb, checked). The node's Flags are 1 for a one-shot (headshake) and 0 for the talk loops, matching row Type 4/5. Clip lengths not read.

##### How it is played (checked)
- ShowLine non-cut branch, after the voice and timer are set (0x0068E96E):
  - `eax = 0x007F0050(dialog+0x1C8 (the conversation), line)`; if not -1: `speaker->vtable[0xB0](eax, 1.0f, -2.0f, 1)` (0x0068E978-0x0068E997: float 0xAFF63C = -2.0, `fld1`, push 1).
  - Then `dialog+0x2C8 = 0x007F00A0(conversation, line)` (RevertAnim flag).
- Slot 44 (`0x006AA7E0`, then `0x0069B3D0`): per lib-agent-2-anim-fade.md: row Type bit 2 goes to the face queues; otherwise the animation set's play with blend. blend -2.0 = a code: fade 20 % of the clip's step length (so a 2 s loop fades in over 0.4 s). "Same clip again" is a no-op. No stop is called on the previous clip: it is the normal cross-fade of the base layer.
- No `CommandPlayAnimation` is queued: the call is direct, so it does not go through the creature's command queue (it is not a sub-action 37 run, has no duration, and nothing ends it). A one-shot row returns to its TransitionTo (1, the idle) through the animation set itself (inferred from lib-agent-2-playanim.md); a loop row loops until replaced.
- **Revert:** the next time a reply is taken (`0x0068D8F0`, checked 0x0068D98C-0x0068D9B5) and the previous line had no cut (+0x20C clear): the previous speaker (+0x16C) has its VO stopped (`StopVO`, 0x00A5D360 via vtable +0x60 of obj+0x70), and **if +0x2C8 is set, `speaker->vtable[0xB0](1, 1.0, -2.0, 1)`: animation id 1 (`pause`, the idle) with blend -2.0.** RevertAnim is 255 on 279 lines of the campaign (123 of them on animated no-cut lines).
- **At the end of the conversation (0x007F1000-0x007F109E, checked)** for each participant (the 0x14-byte entries at conversation +0x70): `0x006CA130` (look-at controller), the look-at enabled byte `[lookat+0x16D] = 0`, `SetLookAtTarget(-1, 1)`, then `0x006A0040` (resume look-at, or return to neutral if the current animation's State flag says so). **No animation is played to undo a gesture**: a looping talk row set without RevertAnim keeps looping after the conversation ends until the creature's next animation (the script side: AMBIENT_CONTINUE at a conversation's end restarts ambient patterns, dev/AI.md; a party member's follow/idle replaces it). Inferred for what ends it; the absence of an undo is checked.
- Lines in the middle of a chain: a following line without an Animation does not touch the body layer, so the previous line's loop continues under it.

#### (b) The face

- The VO for the line: name from `0x007F2B70` (`<vobank>/VO/<strref>_m|_f`), `PlayVO` 0x00A5DA60 (callers: ShowLine 0x0068E916, 0x0069FF54, ambient manager 0x006FE71B) unless `NoVOIngame`. In PlayVO (checked 0x00A5DC6F-0x00A5DCD6): the speaker's face animatable (obj+0x18 -> +4 -> vtable +0x60 -> vtable +8) must exist; the FacialAnimationManager (singleton 0x0084DCB0) slot 8 (0x0084E1E0 -> 0x0084DEA0) is asked whether it has the animation (a wstring name); if so the name is stored at AnimBase+0x1B4 and `FacialAnimationPlayer::Play` (slot 6, 0x0084EEF0) is called with (name strings, content = the sound when faceSync else 0, 0, 0). 0x0084EEF0 narrows the two wstrings (0x00A098E0) and calls FaceFX's `PlayAnim` (0x00A0C0A0) with them. If FaceFX does not have the animation, the sound is played alone (Play(0,0)).
- The .fxe is `<strref>_m.fxe` in `modulefacialanimations.erf`, or `corefacialanimations.erf` for core VO and sound sets (dev/CUTSCENES.md); the actor is a `.fxa` in `face.erf`, named by the head model (below). **There is no emotion input anywhere on the line or in PlayVO**: no group per emotion is selected by the engine. Any smile/frown/brow pose is baked in the line's own .fxe. (Checked: no emotion field in the template; PlayVO's arguments are names only.)
- What FaceFX writes: the bone nodes' +0xF0 rotation and +0x130 translation, applied after body and additive look clips (dev/CHARACTERS.md; lib-agent-2-face-additive.md). Its length is the line timer on a voiced line (`0x00A5D4D0`, timer = max(0, T - 0.3), lib-agent-2-audio-vo.md).
- The idle blink/eye clips (`mh_o.eyelids`, `eyejitter`) are not played by this path (lib-agent-2-face-eyes.md: they exist only as cut actions).
- Skip: `+0x2C9` makes the next StopVO reset the face (resetFace).
- **How the face plays** (read 2026-10-19 by lib-agent-2, for DAOLib #206; checked unless said):
  - **Its names:** PlayVO is handed the same wstring twice (ShowLine 0x0068E8A4–0x0068E916), so FaceFX's group and animation are both `<strref>_m`, or `<strref>_f`. `36658_m.fxe` (magic `BIOE`) holds an `FxAnimGroup` named `36658_m`, authored for `humanmale`, with curves on face-graph nodes (`jawOpen`, `pucker`, ..., `Orientation Head Pitch`), not on bones. The suffix is `_f` exactly when the voice event's is: the main character is female and the `_f` event exists (0x007F2B70). Each language has its own timing.
  - **The actor** is the head model's own field, `MMHFacialAnimationBlueprintName` (6248) on its `.mmh`, not a race and gender table: `hm_uhm_basa_0.mmh` names `humanmale.fxa`, `ef_uhm_*` `elffemale.fxa`, `kn_uhm_*` `child.fxa`, 392 `cn_hed_*` `Human.fxa`, besides `succubus`, `werewolf` and `sylvan`. Many heads have it empty (`hm_hed_dcpa_0.mmh`), and they get no face animation. `FacialAnimationBlueprint` (0x0084D4F0) strips the bones' `default_` prefix.
  - **The clock:** with a voice, the FMOD channel's position. The player's update (0x00850FB0) hands the sound's position (0x005B65C0, ms × 0.001) to FaceFX's tick (0x00A0BAF0) as an external time, so the mouth pauses and stops with the sound. Without one the position is −1 and FaceFX keeps its own clock. That a position ≥ 0 replaces FaceFX's clock is inferred.
  - **Start and end:** while FaceFX has not started (the tick returns 0) and there is a sound, the player starts the sound (0x0085117B), so the face drives the voice's start under faceSync. The face ends at state 2. It loops only with Play's fifth argument (+0x7C), which PlayVO passes as 0. Stop (slot 7, 0x0084EC40) puts the bones back to the reference.
  - **No fallback:** with no face (an empty field, a dog), or no `.fxe` of that name (manager slot 8 false), the voice plays alone and the mouth stays at rest. There is no default talking clip and no flap by loudness (inferred from their absence). The line's timer is then the sound's length.
  - **Barks and ambient lines play faces too, through PlayVO:** the sound set's voice (`CCCreature` 0x0069FE50 from PlaySoundSet, event `<ss_name>/VO/<strref>_m`, face `<strref>_m`, at the creature's +0x870, stopping a voice already playing when asked) and the ambient manager (0x006FE71B). Barks' faces are in `corefacialanimations.erf` (`179932_m.fxe`, Alistair's). A cut's spoken line (ASLN) plays its own with the same names.
  - **Its output is bones, a third term** (the evaluator 0x0084F550): each bone of the actor's `FxMasterBoneList` is its reference plus, per linked face-graph node of value v, v × the node's position and scale deltas; its turn from the reference is the product, in the poses' order with each new factor on the right, of `slerp(I, pose × conj(reference), v)`, starting from identity and normalised once (settled 2026-10-20 by lib-agent-2 against the engine's own instructions in an x87 emulator; `dao/core/FaceFx.h`, #206). Each bone is handed that turn as {x, z, y, −w} and (position − reference) / 100 with y and z swapped through `ObjectNode`'s face setter (slot 1 of the vtable 0x00AF972C, 0x0055E000, scale dropped). The node composes rotation = FaceFX (+0xF0) × animated (+0xE0) and translation = rest + animation + FaceFX (+0x130) (0x0055E9D0). So it comes after the whole body pose (normal, override and additive clips, the look-at among them) and enters no clip list. A cut's `cs_f.*` clip and FaceFX on the same bone compose, and neither wins. No consumer of `FxMorphTargetNode` was found, so no morph targets (inferred).
  - **And the face material's emotion weights:** an `FxEmotionsWeightNode` ("Material parameter index" 0..7, name `mml_fEmotionsMapWeight`, mesh `FaceM1`; 0xB57AD8–0xB57B44) writes its value into component i of the 2 × vec4 `mml_fEmotionsMapWeight` on the face mesh (the evaluator's first part, 0x0084F561–0x0084F6F7). These are the 8 weights over `mml_tEmotionsMask` (the shader's use inferred from the name, dev/CHARACTERS.md). So emotions come from the `.fxe`, as bone motion and as these weights.
  - **No blink layer in the engine:** a blink shows only when the line's curves drive the eyeBlink or lowLid bones.
  - **Cut action 44 is `ActionAnimateFace`** (vtable 0xB232E0, update 0x007CE9A0): it plays a named FaceFX animation on the actor's face through the manager, from the action's start, and stops it at its end. Its arguments are inferred.
  - **Order in a frame:** the scene's tick (0x0052A090, from the scene loop 0x0052A565, after the scene's preparation) calls the manager's slot 13 (0x0084CB60) with the scene time (`[edi+0x48]`), then 0x0084CAF0, which runs each player's update (0x00850FB0), which runs the evaluator (0x0084F550). The player fades the face out near its end (0x00851007–0x00851036; inferred from the code's shape).
  - **The face graph's evaluation** (read 2026-10-19 by lib-agent-2's readers, for #206; the starred items checked, the rest read in the listings):
    - **Order:** `FxFaceGraph::Update` (0x00A19B60) makes one forward pass in the file's node order, with no recursion and no sort. In all 13 readable `.fxa` every link's input index is below its node's; the four enciphered add-ins' actors were not read.
    - **A curve sets its node:** the tick (0x00A0BAF0) sets the node's pending value to the curve's value at t, finding the node by the curve's name (0x00A1AB50).
    - **A node's value:**
      ```
      acc = inputOp == 0 ? 0 : 1          // table 0xB57C5C, only when the node has links
      for each link that is not corrective: acc = acc (+ or ×) f(link, input.value)
      corr = for each corrective link: min(1, corr + p0 · max(I.maxRecip · v, -I.minRecip · v))
      value = clamp((acc + pending) · (1 - corr), min, max); pending = 0
      ```
      A curve adds to its links' result. inputOp 0 is sum and 1 multiply; one shipped node uses 1 ("6 multi", with no links). A node's four floats are (min, 1/min, max, 1/max); the reciprocals serve only the corrective link, whose `min(1, ...)` is inferred from one compare. I is the corrective link's input node and v its value.
    - **The link functions** (*): the id is each class's vtable slot 8 (`mov eax, N`), and the evaluation is a switch on the file's id (jump table 0xA1A688, 11 entries; its copy at 0xA1A6B4 agrees). With x the input node's value, p the parameters and n their count:
      - 0 Null: 0.
      - 1 Linear (m = 1, b = 0): m·x + b with two parameters, m·x with one, x with none.
      - 2 Quadratic (a = 1): a·x·|x|.
      - 3 Cubic (a = 1): a·x³.
      - 4 Sqrt (a = 1): a·sign(x)·√|x|, and 0 at 0.
      - 5 Negate: −x.
      - 6 Inverse: 0 when |x| < FLT_EPSILON, else 1/x.
      - 7 OneClamp: 1 when x ≤ 1, else 1/x.
      - 8 Constant (c = 1): p0.
      - 9 Corrective (factor = 1): above.
      - 10 ClampedLinear (m = 1, clampx = 0, clampy = 0, clampdir = 1): with clampdir > 0, m·(x − clampx) + clampy when x > clampx, else clampy; with clampdir ≤ 0 the sides swap.
      - Only Linear, Negate, Constant and Corrective occur in the shipped files. The formulas were checked by running the disassembly through an x87 emulator.
    - **Node types** 0, 4 and 8 alone occur, and the update treats them alike. Inferred: 0 a combiner or generic target, 4 a bone pose's target (every bone pose names a type 4), 8 `FxEmotionsWeightNode`. The Delta, CurrentTime and MorphTarget classes exist and are not used; the nodes' track records (state 3, none) do nothing.
    - Not read: how the tick works out its elapsed time, the blend in and out, the per-bone weight (1.0 in every shipped file), and ids above 10.
  - A core reader needs the face graph as well as the curves: the `.fxe` holds curves on graph nodes, and the `.fxa` holds the graph, its link functions (linear, quadratic, cubic, sqrt, negate, inverse, oneclamp, constant, corrective, clampedlinear, perlin noise, custom; strings 0xB573E0–0xB57810) and the bones' poses.
- Not read: the `.fxe` and `.fxa` layouts (the reader, #206); how FaceFX's tick uses the external time; what writes the scene time `[edi+0x48]`, so whether a pause freezes a face with no voice; the `.fxe` loader (0x0084DBA0) and how the head's field reaches the player; whether record type 8 is exactly `FxEmotionsWeightNode`; the fade's constants (0xB2B56C, 0xAE70F8); where the node's compose (0x005B5D40) runs against the face's update in a frame, so whether the face lags a frame; `ActionAnimateFace`'s arguments and whether a shipped cut uses it; whether every `_f` event has an `_f.fxe`; `g_eclipseGame+0x254`, which decides faceSync.

#### (c) Idle, listener and the look-at

- **There is no "talk stance" row for a no-cut line.** The engine plays no stance, no turn-to-face and no talk idle at line start. Idle is whatever the creature was doing (checked: ShowLine's non-cut branch makes only the calls listed here; no PlayAnimation command, no 0x006A2A20 stop-moving). The `dg_tk_*` loops are used only if the author sets Animation. Whole-body turning toward the player is not done here either; only the look-at body/head/eye controllers turn (Body part: limit MaxAllowedYaw 45 deg, 96 deg/s; Head 45 deg, 20-200 deg/s; Eyes 20 deg, 360 deg/s; `look_at` table, lib-agent-2-anim-layers.md).
- **Look-at at line start (checked, 0x0068E60A-0x0068E6EB):**
  - `B = 0x0068D270()` = the **controlled creature** (`[[app+0xD0]+0x5C]`), or the party's +0x1F8 creature if B is the conversation's owner (CUTSCENES.md); `speaker` = the object the line's speaker resolved to.
  - `B->SetLookAtTarget(speakerId, 1)` (0x0069EAA0; ecx = B, args (id, slot) pushed 1 then id), then `lookat(B)->Resume()` (0x0069E9D0 -> 0x006CAB00: mode 1, automatic, blend constants 0xAE2E00), `0x006CA130` (an immediate Apply with a large dt: snap), `[lookat+0x16D] = 1` (enabled).
  - Same for the speaker (only if the speaker's cast to creature, vtable +0x10, is non-null): `speaker->SetLookAtTarget(B.id, 1)`, Resume, Apply, enable.
  - 0x0069EAA0 -> `0x006CA950`: `slot[id == self ? -1 : id]` stored at `lookat + 0x14C + 4*slot`; slot 1 is the conversation/cutscene slot. So the speaker's head/eyes/body track B's eye position while the line plays, B's likewise (but the automatic mode excludes the player-controlled leader from automatic targets, lib-agent-2-anim-layers.md, so the player's own creature probably does not turn; inferred, not reconciled).
  - The line's **Listener (12203) is not used in this branch**: ShowLine takes (speakerId, line index) only (checked: no call to the Listener resolver 0x007F02F0 in 0x0068E250; PlayLine resolves speaker 0x007F0210 and passes the id). The listener matters for ambient barks (the manager's speaker/listener pair, 0x006FE7F6/0x006FE808) and for cuts.
  - The look-at is not changed between lines of the same conversation except by the next ShowLine repeating it; the slot is cleared (-1) for every participant at the conversation's end.
- Time/blend: the Apply is immediate at line start (a snap), then the part controllers move at their table speeds toward the target each frame as in any look-at.
- Look-at disabling: animation rows with `DisableLookAt` = 1 (headshake, nod, headache, bless, salute, horror 287...) are said to suspend the look-at while they play (effect inferred; consumer not read, lib-agent-2-anim-layers.md).

#### (d) Stage (.stg)

- A no-cut line uses no stage: `ConversationStageName`/`AtCurrentLocation` only feed `0x007D2AA0` and the stage transform for a cut player that exists (the cut branch 0x0068E514-0x0068E5C4). Many no-cut lines still carry a stage name in the file (e.g. aoa100_aidan's first lines, stage `aoa100st_aidan`); ignored. For the camera on a no-cut line: `0x0068D4B0` (the speaker/B pair; dev/CUTSCENES.md).

#### Not read
- Clip lengths and root-motion of the dg_tk_* / dg* clips; whether any dg clip is additive or override (flags 4011/4012 not scanned for these).
- Who/what ends a looping talk row after the conversation (no undo found in the conversation code; the follow/ambient scripts replace it).
- The `.fxe`'s format (DAOLib #206; its group and animation names, and its emotions, are read in (b) above).
- Whether `DisableLookAt` actually gates the look-at controllers (consumer not found).
- The cast at speaker vtable +0x10 (assumed AsCreature) and 0x006CA130's exact Apply arguments.
- The animated-line counts for ambient (bark) lines: the ambient manager plays Animation through the same slot (barks note) but its blend was read only there.

## The face: additive clips and the eyes (read 2026-10-18 by lib-agent-2)

### Additive and override clips: the engine's maths, and the mh_o.* clips (lib-agent-2, prefix faceadd, 2026-10-18)

GOG exe `game\DAOrigins.exe` (base 0x400000), no Ghidra, no game. "checked" = read in disassembly or data; "inferred" = from what the code/data implies. Scratch: `%TEMP%\lib-agent-2-faceadd-*`.

#### Short answer

1. **An additive clip's rotation is a delta, applied as `delta * base` (Hamilton product, delta on the left = in the parent's frame, the same side as FaceFX's +0xF0), and the delta is `slerp(identity, key, weight)`.** Identity is the neutral key. Nothing is added to the weight-blended pose otherwise. Order per bone: (1) the OVERRIDE clips' weighted spherical mean, with total weight Wo; (2) base = `slerp(normalBlend, overrideResult, Wo)` (override replaces by Wo, the normal blend keeps 1-Wo; Wo >= 1: override alone; Wo <= 0: normal alone); (3) every ADDITIVE entry in list order: `Q = slerp(I, q, w) * Q`; (4) the result is the node's animated rotation +0xE0, and the node then composes `local = FaceFX(+0xF0) * Q(+0xE0)`. Translation: additive is `v += w * key` (an offset; the node adds rest + animation + FaceFX). So the look-at (additive clips) and FaceFX both sit after the whole base+override pose.
2. **The earlier note's list mapping was swapped** (dev/ANIMATION.md and lib-agent-2-anim-layers.md say additive = object+0x218, override = +0x228). Read here: clips with `+0x2D` (AnimationIsAdditive 4011) go to the list at **+0x228**, clips with `+0x2C` (AnimationIsOverride 4012) to **+0x218**. Checked in 0x008149D0 and the sampler (below).
3. **mh_o.\*: all the look and eye-motion clips are additive (4011 = 1): eyehigh, eyelow, eyemid, eyejitter, eyelids, headhigh, headlow, headmid. `arms` and `rct_odmg_v1` are override (4012 = 1). Priority 4013 is 0 in all ten.** A delta's identity is stored as the identity quaternion (0,0,0,1), NOT the bone's rest rotation: the keys at neutral are exactly (0,0,0,1) while the head model's rest rotations are far from it (eyes (0.5,-0.5,-0.5,0.5); eyelids (0.707,0,0,0.707)). Blended as an absolute pose, an identity key puts the eye bone at identity, a 120 degree turn from its rest, which is the white eyes; the eyelid bones at identity are a 90 degree turn from rest.

#### 1. The sampler (checked)

##### Where the lists are consumed

- `GraphicsStructures::Object` vtable slot 49 `0x0054D4B0` clears three vectors (normal +0x208, override +0x218, additive +0x228) and calls `0x008149D0` once per clip tree; it pushes (a1 = +0x208, a2 = +0x218, a3 = +0x228, a4 = +0x290 the scale). In `0x008149D0` an entry with `ModelAnimation+0x2D` set is written through `[ebp+0x10]` = a3 = +0x228 (0x00814B00..0x00814B6A), one with `+0x2C` through `[ebp+0xC]` = a2 = +0x218 (0x00814B84..0x00814BEE). Entries are 0x14 bytes (scale vec3, weight float at +0xC, track base word at +0x10, word at +0x12), and only entries with weight > 0 are added. The normal list has 0x30-byte nodes (blend-tree nodes; weight at +0x24, children), and per group the weights are divided by their sum (0x0054D6C1..0x0054D70E).
- Slot 51 `0x00549E30` (the pose build, after the lists are made) loops the model's channel entries (0x30 bytes at object+0x1D8; the dword at +0x28 is the channel type, a jump table at 0x0054A18C) and calls one evaluator per type with (normalList, nNormal, +0x218 list, count, +0x228 list, count, track table, ..., bone index): type 0 scalar `0x00544F30`, type 1 vec3 `0x00544E20`, type 2 **rotation** `0x00545020`, type 3 vec4 (colour) `0x00545170`. Each writes its result through the entry's pointer at +0x10. A channel is evaluated only if some loaded clip keys the bone (`0x00544270` scans the track table: any non-null track for the bone).
- Defaults: `0x008112E0` = (0,0,0,1) for rotations, `0x00811270` = (0,0,0) for vec3, `0x008112A0` = (0,0,0,0) for the vec4.

##### Rotation, step by step (`0x00545020`)

1. `0x00544510`: override accumulation. Start at identity with W = 0; for each override entry with weight w > 0 whose track exists: sample the key (below), then `0x00811130(acc, q, &W, w)`: `W' = W + w`, `acc = slerp(acc, q, w / W')` through `0x004CBC10` / `0x004CBAA0`. A running weighted spherical mean. Returns W = Wo.
2. If Wo >= 1 the result is the override mean. Else `0x00544700` blends the normal tree (the same running slerp, tree weights already normalised) into Qn; if Wo <= 0 the result is Qn; otherwise `0x00811130(Qn, Qo, &(1-Wo), Wo)` = `slerp(Qn, Qo, Wo)`.
3. `0x00544620`: for each additive entry (weight > 0, track present, in list order): sample q with `0x008118D0`, then `0x008111E0(Q, q, w)`: `delta = slerp(identity, q, w)` (`0x004CBC10` puts identity in the output and calls `0x004CBAA0`), then `Q = 0x004CB640(delta, Q)` = **delta * Q**.
4. Store Q at the entry's +0x10 pointer. 0x004CBAA0 is a true slerp: dot of the two, sign flip if negative (shortest path), `acos`/`sin` weights (0x004CB930, 0x004CB3E0), a linear fallback near dot 1 (threshold double at 0x00AE09B0); `rsqrtss` inside. The outputs are unit (slerp of unit quaternions; products of unit quaternions), and no renormalise is found in the path, none is needed.
- `0x004CB640(out, a, b)` is the Hamilton product a*b (x = ax*bw + bx*aw + ay*bz - az*by ...). The node (0x0055E9D0) calls it as `local = +0xF0 (FaceFX) * +0xE0 (animation)` and builds the matrix with `0x004D3D00`. So FaceFX and each additive delta act on the same side (outermost) of the animated rotation; the order among several additive entries is list order (they do not commute).
- A key is sampled by `0x00818490` (finds the two keys around the 16-bit time, factor f) and `0x008118D0` / `0x00811B20` lerp the two 4-vectors componentwise with (1-f, f). Quaternions are lerped, not slerped, between keys, and not renormalised in the sampler; the keys are 1/30 s apart so the error is tiny. The caller's time is the clip's playback time (for look clips the controller sets it with SetTime).

##### Translation and others

- vec3 (`0x00544E20`): override accumulation `0x00543FC0` (weighted mean, returns Wo), normal tree `0x00544140`, mix with `0x00811030` (`result = normal*(1-Wo)/... + override*Wo`, a weighted mean), then additive `0x00544090`: **`v += w * key`** (`0x008110F0`). Starting default zero. The key is an offset added to the node's rest (dev/CHARACTERS.md).
- scalar (`0x00544F30`): the same shape with floats (`0x00544370` additive).
- vec4 (`0x00545170`): per-component linear weighted mean for override/normal, and additive `Q += w * q` (`0x00810FE0`); that is the colour channel type, not rotation. (I first took this one for the rotation; the default (0,0,0,0) at 0x008112A0 and the Hamilton product in 0x008111E0 settled it.)
- Scale: `AnimationIgnoreScale` (+0x2F) only sets the entry's scale vec3 to (1,1,1) in `0x008149D0`; the scale arrives in the list entries' first three floats (a4 = object+0x290). How the scale multiplies a key was not read.

##### Where FaceFX and the look-at fit

- FaceFX writes the node's +0xF0 (rotation) and +0x130 (translation); the animation writes +0xE0 and +0x120; rest is +0x100. Node: translation = rest + animation + FaceFX (added), rotation = +0xF0 * +0xE0 (checked, 0x0055E9D0 and dev/CHARACTERS.md). FaceFX is therefore applied after the base, override and additive result.
- The look-at is not a separate mechanism: it plays the overlay tree's additive clips (headlow/mid/high etc.) with weights and a scrubbed time (dev/ANIMATION.md), so it is entry (3) above, `delta*base` per look clip.

#### 2. The mh_o.* clips (checked: `tools\gff4.py` dumps of the clips ghilana serves, same bytes in global.rim and anims.erf)

| clip | length s | 4011 additive | 4012 override | 4013 prio | channels (keys) |
|---|---|---|---|---|---|
| mh_o.eyehigh | 6.0333 | 1 | 0 | 0 | eye_left_rotation (182), eye_right_rotation (182) |
| mh_o.eyemid | 6.0333 | 1 | 0 | 0 | eye_left_rotation (182), eye_right_rotation (181) |
| mh_o.eyelow | 6.0333 | 1 | 0 | 0 | eye_left_rotation (182), eye_right_rotation (182) |
| mh_o.eyejitter | 9.9667 | 1 | 0 | 0 | eye_left_rotation (31), eye_right_rotation (56) |
| mh_o.eyelids | 19.9667 | 1 | 0 | 0 | eyeblink_left_rotation (216), eyeblink_left_translation (37), eyeblink_right_rotation (229), eyeblink_right_translation (37), lowlid_left_rotation (216), lowlid_right_rotation (229) |
| mh_o.headhigh | 6.0 | 1 | 0 | 0 | head_rotation (181), neck_rotation (181), neck1_rotation (179) |
| mh_o.headmid | 6.0 | 1 | 0 | 0 | head_rotation (181), neck_rotation (177), neck1_rotation (166) |
| mh_o.headlow | 6.0 | 1 | 0 | 0 | head_rotation (181), neck_rotation (181), neck1_rotation (172) |
| mh_o.arms | 0.1333 | 0 | 1 | 0 | leftshoulder_rotation (2), rightshoulder_rotation (2) |
| mh_o.rct_odmg_v1 | 1.30 | 0 | 1 | 0 | 45 channels: brow_left/right, cheek_left, chest1, chest2, eyeblink_left/right (rot + trans), eyelookbackat, head, headbase, inner/outer lips, jawbone, lipcorner, lowlid, mouthbase, neck, neck1, sneer, undereye, and more (rotations, some translations) |

Eyelid translation keys are offsets (zero outside the 6 blinks, peak y about -0.0016).

##### What a delta's identity looks like (checked)

Keys read with the lib-agent-2 clip decoder (`lib-agent-2-animlayer-q.py`; Clip.h's decoding), against the rest rotations in `hm_hed_dcpa_0.mmh` (Gff4 dump, MMHRotation, the copy the game takes):

| bone | additive clip's key at neutral (x,y,z,w) | head model rest rotation (x,y,z,w) |
|---|---|---|
| eye_Left / eye_Right | (0,0,0,1) at t=0 and at the end of every eye clip; eyemid at 3.03 s is (0,0,0.0033,1) | (0.5,-0.5,-0.5,0.5), a 120 degree turn |
| eyeBlink_Left/Right, lowLid_Left/Right | (0,0,0,1) at t=0, mid-clip (between blinks) and at the end | (0.7070,0,0,0.7072), a 90 degree turn |
| Head | (0,0,0,1) at t=0 and at 3.0 s of headmid (the centre of the yaw sweep); (0.2284,0.0256,-0.0064,0.9732) at 6.0 s | (0,0,0.106,0.994) |
| Neck / Neck1 | (0,0,0,1) at t=0, about identity at 3 s | Neck (-0.704,-0.063,-0.063,0.704); Neck1 (0,0,0.083,0.997) |

So: a delta at rest is the identity quaternion, never the rest rotation. If the engine used these keys as absolute poses (override-style), every keyed eye bone would go to identity (120 degrees from rest) and the lids 90 degrees from rest: the white eyes. The override clips are the opposite: `mh_o.arms` keys are absolute poses (leftshoulder (0.1736,0.3705,0.0134,0.9123) against rest (-0.114,0.478,-0.025,0.871); rightshoulder (-0.1755,-0.3579,0.009,0.9171) against (0.114,-0.478,-0.025,0.871)), flat for 0.13 s.

For a port: `Q = base`; for each additive clip (weight > 0) in order, `Q = slerp(identity, key, weight) * Q`. For a weight of exactly 1 that is `key * Q`. With slerp(identity, q, w) you may use the shortest-path sign flip as the engine does.

#### Not read

- The normal-tree blend `0x00544700` / `0x00544140` internals beyond "the same running slerp / weighted mean" (read by the call shape, not instruction by instruction), and the initial value when no normal clip is active for a bone (`0x00544AB0` starts from the entry's own first 16 bytes, which I did not trace to the rest rotation).
- The binding that sets each entry's +0x10 pointer (vfunc +0x68 called from 0x0054D380): the node field it points at is taken from dev/CHARACTERS.md (+0xE0 rotation, +0x120 translation), not read at the bind.
- Scale's combination, and `AnimationIgnoreScale` beyond setting (1,1,1).
- The order of the entries inside the additive list (the tree's walk order, 0x008149D0 per clip tree); only that it is list order and the products do not commute.
- Other additive clips in the game (mh_c.b_look*, bw_look*, etc.) were not re-dumped here; the engine path is the same for every clip with 4011 = 1.
- Who plays `mh_o.eyelids` and `mh_o.eyejitter` (still not found in a tree or table).
- Not run in the game: no emulation of the sampler against a pose.

### What turns eye_Left / eye_Right

#### Short answer

1. **No aim constraint.** Nothing in the exe points `eye_Left` / `eye_Right` at the `*Lookat` nodes or at a world target. The head model's `eyeLookAtBase / eyeLookAt / eye_LeftLookat / eye_RightLookat` nodes are not children of the head (they hang off `GOD`, beside `Root`) and the exe has no string for any of them (checked, see 1). They are a leftover of the authoring rig; the engine never reads them (inferred from absence of any reference).
2. **In normal play the eyes are turned only by the additive look clips**: the Eyes `PartLookAtController` plays `lookeyescenter/up/down` (`mh_o.eyemid/eyehigh/eyelow.ani`), yaw as clip time, pitch as weights. The eyes get the same world-target quaternion as the head and body (not reduced by the head's turn), clamped by themselves to **+-20 deg yaw, +-10 deg pitch**, at **360 deg/s** flat (times the 0.75..1 / 0.4..0.6 multiplier).
3. **FaceFX can also turn the eyes** (the actor has `default_eye_Left/Right` bones with Eye Yaw / Eye Pitch / Gaze nodes), but only while a face animation (spoken line, gesture) plays; at idle it adds nothing. Its rotation is composed with the look clips' (see 4).
4. **`eyejitter` and `eyelids` are not played by the engine at all in play.** They occur only as animation actions authored into cutscenes / conversations (`CutsceneActionAnimAnimationName` = `mh_o.eyejitter__0`, `mh_o.eyelids__0`). No automatic blink and no automatic jitter exist in the exe (not found; see Not read).

#### 1. The nodes (checked, data)

`hm_hed_dcpa_0.mmh` (extracted through ghilana, dumped with `tools/gff4.py --dump`; indent = depth, so parents are known). Chain: `GOB > GOD > Root > ... > Neck > Neck1 > Head > headBase > { underEye_*, eyeBlink_Left/Right, lowLid_Left/Right, eye_Right, eye_Left }`, plus `Head > eyeLookBackAt`.

- `eye_Left` / `eye_Right`: children of `headBase`; rest translation (+-0.03224, -0.03615, 0.02897), rest rotation (0.5, -0.5, -0.5, 0.5) (x,y,z,w). Same position as `eyeBlink_*` and `lowLid_*` (siblings).
- `eyeLookAtBase` (translation (0, -0.736, 1.738), rotation z -90 deg), `eyeLookAt` (child, rotation 180 deg about y), `eye_RightLookat` (+-0.03224 on y) and `eye_LeftLookat`: a separate subtree under `GOD`, a sibling of `Root`, `RightSpawn`, `LeftSpawn`. It is the classic 3ds Max look-at target rig, with channels (`eyelookat_rotation` ...) exported but no consumer.
- `eyeLookBackAt`: child of `Head` at (0.070, -0.082, 0). It is keyed by the `bdmg` flinch clip (45 channels, includes `eyelookbackat`); nothing else found.
- Exe strings (checked, whole-file scans ASCII and UTF-16): "eyeLookAt", "Lookat", "eyeLookBackAt", "eyelook" and "eyejitter" are **absent**. Only `lookat` hits are `COMMAND_SETLOOKATENABLED`, `D3DXMatrixLookAtLH`, RTTI `PartLookAtController`. The only eye bone names in the exe are the wide strings at 0x00B0F468 `RightEye`, 0x00B0F47C `LeftEye`, 0x00B0F48C `eye_Right`, 0x00B0F4A0 `eye_Left` (plus `Head` 0x00B0F45C, `head` 0x00B0F450).
- Their one user (checked, disassembly): `CCCreature` function **0x006A1BF0** (ID 14446; called from 0x0068D58D; dev/CUTSCENES.md already calls it `eyesLocal`). It looks up the bone `eye_Left` and `eye_Right` (the pointers come from the creature's model, vtable +0x4C then +0x10 by name), falls back to `LeftEye`/`RightEye`, then to `Head`; with both eye bones it returns the sum of their positions times the double at 0x00AE70F8 (0.5 inferred), i.e. the midpoint. **It reads positions only and writes no rotation.** The sibling at 0x006A1A30 (ID 14444) returns the head height from the `head` bone. These give the look-at its eye position.

#### 2. What writes the eyes' rotation in play (checked unless noted)

The Eyes controller (`PartLookAtController` vtable 0x00B0F388, at look-at object +0x10; dev/ANIMATION.md "Blending", `lib-agent-2-anim-layers.md` section 3 has the whole account). New here, from the order of the update:

- **Order per frame** (`0x006CA990`, called with dt): `0x006CA250` recomputes the target quaternion (+0x15C) if dirty; then `0x006CA150` runs the controller update `0x007F53E0(dt, +0x15C)` for **Body (+0x98) first, then Head (+0x54), then Eyes (+0x10)**. All three get the **same** target quaternion (the direction to the target relative to the creature's facing, each yaw and pitch clamped to +-90 deg), and each limits it itself (Eyes' settings +0x1C/+0x20 = MaxAllowedYaw 20 deg, MaxAllowedPitch 10 deg, blended by how much of the rotation is about the vertical axis; head 45/10, body 45/30). The eyes do not subtract what the head has already turned, so with a target 30 deg away the head turns 30 and the eyes add 20 on top (inferred from the call arguments: no head quaternion is passed to the Eyes call).
- **Speed:** Eyes `look_at` row 0: MinSpeed = MaxSpeed = 360 deg/s (flat), SlowAngle 10, FastAngle 25 (irrelevant when min = max), times the turn multiplier 0.75..1.0 on acquiring a target and 0.4..0.6 on losing it. A step above pi/2 or reaching the target snaps (0x007F53E0).
- **Output:** the controller converts the limited quaternion to yaw and pitch, each clamped to the table's +-45 deg animation range (rows: Min/MaxAnimationYaw -45/45, Pitch -45/45), then every frame calls the host's `SetWeight` (0xA5C840) and `SetTime` (0xA5C980) on rows 12/13/14 (`lookeyescenter/up/down`): time = (yaw+45)/90 * (len - 1/30) + 1/30, up weight = pitch/45 (positive), down weight = pitch/-45, centre = 1 - that. With the clamp at 20/10 deg the eyes in play use only the clip time range 0.28..0.72 of the sweep and pitch weights up to 0.22. Clips mh_o.eyemid/eyehigh/eyelow.ani: additive, 6.0333 s, keys on `eye_left` and `eye_right` rotation only (182 keys).
- **Clip angle per second of time** is not recomputed here; the 45 deg end-to-end sweep of the head clips is checked (anim-layers section 3), the eye clips' sweep is inferred to match the table's +-45.
- **Only the table's rows and the clip drive the eye rotation in play**: there is no other write to the eye bone (the only code that names the bones is 0x006A1BF0, which only reads).
- The look-at is off (and the controllers eased to identity, 0x006CA150's disabled branch) when `SetLookAtEnabled(false)`, global byte 0x00C03944 is 0, the creature is the player-controlled leader (no target is chosen, so eyes rest centred), or the part has no clips.

#### 3. eyejitter and eyelids: who plays them (checked in data, absence in exe inferred)

- `mh_o.eyejitter.ani` (9.9667 s, additive, eye_left 31 keys / eye_right 56 keys) and `mh_o.eyelids.ani` (19.9667 s, additive, blinks) are named in **no .anb, no table, no script and no exe string**.
- They do appear in **cutscene/conversation data**: e.g. `str510_morrigan.dlg` (in `packages/core/patch/patch004.erf`) holds actions of type 4 (play animation, category 20) with `CutsceneActionAnimAnimationName = 'mh_o.eyejitter__0'` and `'mh_o.eyelids__0'`, `CutsceneActionAnimBlendtreeName = 'str510_morrigan.dlb'`, start time slightly negative (-0.09, -0.27, -0.59 s: the clip is already running when the scene starts), stop time 1e10 (never), weight curve base value 100, and a **per-actor speed that differs** (1.078, 1.312, 0.866 ...), so the loops drift apart between actors. dev/CUTSCENES.md already says additive/override clips such as `mh_o.eyelids` are not counted in the pose weights or removed when out of time (0x007D62A3).
- So: blink and jitter exist in cutscenes only, played as ordinary authored clips at speed about 0.87..1.31 times, looped (a clip past its length wraps, inferred). **Outside cutscenes vanilla creatures do not blink and do not jitter** (no code path found). If the peer engine blinks or jitters in free play, that is its own addition. The blink data if wanted: 6 blinks per 19.97 s, closed about 0.1 s of each 0.17 s (anim-layers section 4).
- Scan of the GOG install's `packages` and `modules` (checked): the UTF-16 names `mh_o.eyejitter__0` and `mh_o.eyelids__0` occur 2,283 and 2,279 times in 139 files (patch004.erf, and the module area `.rim` files such as al_arl01al_redcliffe_villag 54 each), always as a pair, never as plain ASCII; they are cutscene/dialogue action data (the `__0` suffix is the toolset's), one pair per actor per scene. (I did not unpack the rims, so the per-resource type is read only for `str510_morrigan.dlg`.)

#### 4. FaceFX and the eyes (checked in data; mechanism from dev/CHARACTERS.md)

- `humanmale.fxa` (and the other race/gender actors in `face.erf`) has bones `default_eye_Left`, `default_eye_Right`, `default_eyeBlink_Left/Right`, `default_lowLid_*`, `default_underEye_*`, `default_brow_*`, `default_Head`, `default_headBase`, `default_Neck`, `default_Neck1`; face-graph nodes `eye_Left_RX+/-`, `RY+/-`, `RZ+/-` (and Right), `Eye Yaw`, `Eye Pitch`, `Gaze Eye Yaw/Pitch`, `Eye_Yaw`, `Eye_Pitch`, gestures `E_GESTURE_EyesLeft/Right/Up/Down/CloseEyes`, `B_EyesWide`, `B_EyeSquint`, `Blink`, `blinker`, `blinkLeft/Right`, `lowLid*Up/Down`, `eyeLidLeft/Right_RY-`. So a `.fxe` face animation (the spoken lines, `<strref>_m.fxe`) **can** turn the eyes and blink and move the lids while it plays.
- Mechanism (dev/CHARACTERS.md, checked there): `FacialAnimation::FacialAnimationPlayer` (0x0084f550) hands each `ObjectNodeBone` the actor's output minus the reference pose: rotation into +0xF0 (the turn from reference to current), position into +0x130 ((current - reference)/100, y/z swapped); `ObjectNode` local matrix (0x0055e9d0): **rotation = FaceFX(+0xF0) composed with the animated rotation (+0xE0)**; translation = rest + animation + FaceFX. The look clips are inside +0xE0 (animated), so FaceFX is applied as an extra turn in the node's frame on top of look-at; neither replaces the other. At idle FaceFX's +0xF0 is identity (seen in play 2026-10-17).
- Order summary for one frame (inferred from those pieces): animation (movement + normal clips, then additive look/jitter/eyelid clips, then override clips) accumulates into +0xE0/+0x120; the FaceFX graph sets +0xF0/+0x130; the node's matrix is built from both.

#### 5. Judging the peer engine's choices

- Eyes turned only by the look clips, no aim constraint to the `*Lookat` nodes: **confirmed**.
- Eyes limited to 20 deg yaw / 10 deg pitch, 360 deg/s: confirmed (table 229 row 0 plus 0x007F53E0).
- An eye blink in free play, or eye jitter, would not be vanilla: not found in the exe or any tree; only cutscene data plays them.
- If the peer engine moves the eyes by the head-relative angle, vanilla does not: all three parts take the same target quaternion relative to the creature's facing.

#### Not read

- The sampler that applies the additive list (so "additive = delta added to the pose, scaled by weight" is inferred) and whether eyes' clips at weight 0 for the up/down rows are skipped.
- The eye clips' exact angle per second (inferred equal to the +-45 of the table; the head clips were checked).
- Whether the node-name lookup by hash exists anywhere that would reach `eyeLookAt` (no string, so no lookup by name; a hashed lookup was not searched).
- Which resource types in the rims carry the pair beyond the one dlg read; whether any non-cutscene (ambient) play of them exists.
- What the FaceFX graph outputs for `blinker` in free play (idle FaceFX adds nothing, seen 2026-10-17; the `.fxa` graph's idle contents not decoded).
- The consumer of `eyeLookBackAt` (keyed by `bdmg`, no code reference found).
- `MIN/MAX_DELAY_BEFORE_PLAYER_LOOKAT` use (unchanged from anim-layers).
