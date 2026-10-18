# Animation: drawing and sheathing, and a script's animation command

How the engine plays a creature's animations for two cases read in the exe: a weapon drawn and sheathed, from the combat state to the clip's events that move the weapon, and `CommandPlayAnimation`, with its loops, the commands queued behind it and an animation that is missing. Both read 2026-10-18 by lib-agent-2. The animation trees and clips as files are `src/dao/core`'s Anb and Clip; which tree a creature's weapon style gives is `dev/ITEMS.md`, "The weapon style".

## Drawing and sheathing weapons (DAOParagonEngine #33)

Read 2026-10-18 by lib-agent-2 (and two of its forks) in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py --disasm`, scratch call and xref scans and capstone; the tables and clips through ghilana (core copies, `2da.erf`, `global.rim`); the scripts in `toolset\extract\scripts\`. No Ghidra, no game. "Checked" = read in the disassembly, the files or the scripts; "inferred" = a reading of purpose not followed to the end. Every address is GOG's and has an ID in the address library (`python tools\address_ids.py 0x...`).

### Summary

- **The engine draws and sheathes on the combat state, natively.** `CCCreature::SetCombatState` (0x006AE0A0) queues command type 39 (Sheathing) at the back of the queue, draw on entering combat with a delay of 0-99 ms, sheathe on leaving it with 1,000-2,999 ms. The scripts only set the state, and they do it for every creature: hostiles per creature, the whole party (controlled or not) through the game mode. The attack command also draws by itself before its swing if the weapons are still sheathed, and moves draw or sheathe with an upper-body overlay.
- **The row is fixed and the clip comes from the creature's combat set.** anim_combat row 100 `enter` draws and 101 `exit` sheathes; 177 `quick_enter` and 178 `quick_exit` are used while the combat state is set, so a combat-start draw plays `quick_enter`. The node is looked up in the creature's trees, where the weapon set's tree (OW/WS/TW/DW.anb by equipment; Bow/Cbow/Staff.anb from bitm_base's `WieldModel`) supplies it. With no such node the drawn flags flip at once and `unarmed_enter`/`unarmed_exit` (110/111) plays if the creature has it.
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

It first sets combat data +4 = -1 (inferred: the attack target). Then:

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
- Then it calls 0x007C1A30 on creature+0x744. That function calls 0x007C1690(-10.0) when a float at its +0xC differs from 1e-6 (inferred: it resets a blend or rate; **it does not stop the clip**, so a loop keeps looping after a LOOPING end).
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
