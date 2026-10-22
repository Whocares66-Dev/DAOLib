# Placeables

A placeable used, as the engine carries it out; its template is `dao/core/Gff3.h`'s and its place in an area `dao/core/Area.h`'s.

## Using a placeable: COMMAND_TYPE_USE_OBJECT (31), read 2026-10-18

Read in the GOG exe, the scripts and the tables by lib-agent-2 (with subagents) for DAOParagonEngine #18/#22. Not seen in play.

### Queuing it

- **The player's click.** `GameModeExplore::HandleMessage_PerformUseObjectMessage` (0x0070BC60) calls `Orders::UseObject` (0x007F6550; user, target, action).
  - It needs only a target.
  - Action 4 (DESTROY) becomes `Orders::Attack`.
  - Otherwise it queues player command 31 with object 0 = the target and int 0 = the action.
  - It then runs EVENT_TYPE_PLACEABLE_ONCLICK (78) at once on the **user's** script: creator the user, target the placeable, int 0 the action.
- **From a script.** `CommandUseObject` (0x0066F770) builds the same command and checks nothing.
- **The loot panel's Close** (`PerformUseObject <char> <container> 2`) is this same path, an ordinary use with CLOSE.

### The command's steps

The start handler (0x00745920) adds, for a creature user, sub-action 4 (`ApproachMelee`, 0x007AA560: the target, the user's `PersonalSpace`, walk or run). It then adds sub-action **21** (0x007A78F0), the use, with a copy of the command's data. No turn is queued, and nothing turns the user to face the placeable. The end is the shared one (0x00746900): COMMAND_COMPLETE is queued with int 1 = the result.

### (a) Where the user walks to

The spot is the placeable's melee-ring spot (`CCPlaceable` slot 38, 0x0074F240 → 0x0074DFB0). **No model use-point node is used.**
- **The rectangle.** The placeable's walk-grid patch for its current state, given a 0.5 margin and turned by its facing.
- **The spot.** Where the line from the user to the centre crosses that rectangle, pushed out by the user's `PersonalSpace` + the placeable's `InteractRadius` (template field, +0x3CC) + an extra.
  - The extra is 0, except for a door (`placeable_types.BaseType` 1): 0.25, or 0.75 × the long side up to 2.5.
  - A user already nearer than the edge stays where it is.
- **Fallbacks.** Spots in front of and behind the placeable along its facing, then a safe location nearby.
  - Each spot must be free (`MovementController::IsFree`); a door's also needs line of sight.
  - If none is found: -5 (`COMMAND_FAILED_NO_SPACE_IN_MELEE_RING`).
- **The spot function** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169; 0x0074F240, CCPlaceable slot 38; it calls 0x0074DFB0 for the rectangle and the line spot) [C]: its two calls of `CCArea::GetSafeLocation` (0x0073E370, `(pos, maxRadius, controller, reachable, out, snapToGround, creatureCheck, minRadius)`) pass:
  - **After a spot is accepted** (0x0074F591): pos = the spot, maxRadius = 1.5 for a door (`this[0x638] == 1`), else 3.0, **reachable = 0**, out = the spot, **snapToGround = 1**, creatureCheck = 0, minRadius = 0.0.
  - **The last resort** (0x0074F618): pos = the placeable's position (+0x120), maxRadius = max(sqrt(…), 1.0) + 0.5 + the argument (the user's PersonalSpace) + the value of 0x006953D0, reachable = 0, snapToGround = 0, creatureCheck = 0, minRadius = 0.0. The square root is `_CIsqrt` of a value computed just before ([I]: the patch's half diagonal; the operands were not traced).
  - In both the pushes were read right to left; the virtual call at +0x10 of the user between the pushes is taken as a call with no stack arguments ([I]; the decompiler also reads reachable 0 and snapToGround 1 in the first call).
  - So neither call sets the `reachable` flag (the ray 0x0067CFA0 inside IsLocationSafe, 0x0073DE70, is made whatever the flag). The door's own line-of-sight test (`Combat::LineOfSight`) is made by 0x0074F240 itself on the spot before the first call, with z from 0x0073DE20.
  - When 0x0074DFB0 answers 0 (no patch): the radius is `param_3 × 4.0` when the placeable's +0x664 is −1, else `0x0074CC30(0) + 0.5 + [0x00C0CBC4] + param_3`; the spot is the user's position when within that radius (squared distance, 0x0045B080) of the placeable's, else the point at that radius from the placeable's position toward the user.
- `IsLocationSafe` in these calls is the full test (IsFree, the ray, and with creatureCheck 0 not the box test): see PATHFINDING.md, section 1, the `IsLocationSafe` (0x0073DE70) note.
- **With no patch,** the radius is 4 × PersonalSpace (or from the body bag's creature's `PerSpace`).
- **Arrival** (slot 33, 0x0074D0A0) is any of: within 1.0 of the centre, no farther from the centre than the spot is, or on the spot. The walk itself ends within 0.05 of the spot.

### (b) The use, the animation and EVENT_TYPE_USE

Sub-action 21 does this in one tick, with no animation and no wait:
1. It fails with -3 if the target is gone, not a creature or placeable, or dead (a placeable may be "dead").
2. It fails with -1 without line of sight (`Combat::LineOfSight`).
3. A body bag (+0x600) with its creature (+0x664) still there has the creature's droppable items and money moved into it now (0x006B9620). This is before the action check, so it happens on any use.
4. **nAction must be one of the two actions of the placeable's current state** (Action0 or Action1 of its state controller row, 0x0074C720). Otherwise it fails with -1 and no event is sent. DESTROY reads as none while +0x2C4 is set.
5. It stores nAction at +0x68C, which is what `GetPlaceableAction` reads.
6. **The event**, both cases queued to the placeable (`AIMaster::AddEventDeltaTime(…, 10, ev)`, so not run at once; the 10 is the handler id for a script event, not a delay) and gated by the placeable's disabled set, +0x28C (0x00698490: it passes unless `EnablevEvent` took the type out, which no shipped script does; corrected 2026-10-18 by lib-agent-2, `dev/LOAD_EVENTS.md`):
   - With `TrapFlag` (+0x628) = 1 and a creature user: EVENT_TYPE_TRAP_TRIGGERED (16) and no USE.
   - Otherwise **EVENT_TYPE_USE (14)**: creator the user, **int 0 = 1 if the user stands on the placeable's front side** (dot of user − placeable with its facing ≥ 0). No other ints or objects. **The action is not in the event**: the script reads it with `GetPlaceableAction`.
7. The sub-action answers 1, and the command completes with COMMAND_SUCCESSFUL.

**The user plays no animation natively.** The only use animation in the scripts is the disarm's `CommandPlayAnimation(904)` (`wicrouch_activate`, `placeable_h.nss`:289).

**The placeable's animation is native, driven by its state controller.** `placeable_types` (table 93) gives each type a `StateController` (a `statecnt_*` table) and a `BlendTree` (.anb). A state controller row is a from-state, with:
- one column per to-state, holding an `anim_placeables` id: the cell [from][to] is the transition and [s][s] the state's looping idle;
- then Action0, Success0, Success0a, Action1, Success1, Success1a;
- and BlockVis.

**`SetPlaceableState(o, s)`** (0x0074F9D0) plays the transition [old][new] through slot 0xB0 as `(anim, 1.0, -1.0, 1)`, with the idle [new][new] following. It then stores the state and swaps the walk-grid patch (below). A state with no actions makes the placeable not interactive (0x0069BA70(0) at 0x0074FBA1, `SetObjectInteractive`'s native, writing +0x42C; not inactive, and it stays drawn: corrected 2026-10-18 by lib-agent-2, "Not interactive, removed when dead, hidden" below).

**`SetPlaceableActionResult(o, nAction, bSuccess, bVariation)`** (0x0074FC10):
- on success, sets the row's SuccessN, or SuccessNa with bVariation, through `SetPlaceableState`;
- a Success of -1 changes nothing;
- it always clears +0x68C.

**The script** (`placeable_core` → `Placeable_HandleUsed`, `placeable_h.nss`:178–551) takes the user from the creator, the action from `GetPlaceableAction`, and bVariation from int 0. Per action:
- **OPEN** inverts bVariation, so a door swings away from the user. It refuses a container in combat, then sends OPENED (1047; `placeable_core` ignores it).
- **UNLOCK:** a key or lockpick check, then UNLOCKED or UNLOCK_FAILED. The unlock handler gives the user a new `CommandUseObject` with OPEN or OPEN_INVENTORY.
- **OPEN_INVENTORY:** OPENED, then `OpenInventory`. `_autoloot` tags get `MoveAllItems` instead, and a body bag with only copper gets `AddCreatureMoney`.
- **DIALOG** and **EXAMINE:** a conversation or a popup.
- **AREA_TRANSITION:** the transition.
- **DESTROY:** an attack, with no result.
- **CLOSE** and **USE:** nothing of their own.

Every case but DESTROY ends with `SetPlaceableActionResult(self, nAction, result, bVariation)` (:550).

### (c) When it completes, and the codes

A failed sub-action ends the command with its code (`StepCommands` 0x0073D320 → `ClearCurrentCommand(code)` → COMMAND_COMPLETE, queued).
- **The approach:**
  - -3 `INVALID_DATA`: no user or area.
  - -7 `TARGET_DESTROYED`: the target gone, dead, in another area, or the user itself.
  - -5: no spot.
  - -4 `INVALID_PATH`: no path, or stuck.
  - -9 `PATH_ACTION_REQUIRED`: but a path blocked by the placeable itself is retried with the spot asked again.
  - -8 `DISABLED`.
  - There is no timeout.
- **The use:** -3 as above; -1 for no line of sight or an action the state doesn't offer.
- **Otherwise 1, success, as soon as USE is queued:** before the script has answered.
- **A lock is not checked by the engine.** A locked door's state offers UNLOCK, not OPEN, so an OPEN use fails -1 at the action check, and the script's UNLOCK case does the rest. No `Useable` flag is read on this path; where a click is filtered on it was not looked for.

### (d) Doors

Opening is the script's decision and the engine's work. USE goes to `placeable_core`, which ends with `SetPlaceableActionResult`. The engine then changes the state, plays the animation and patches the walk grid.

For example, `genip_door_fer_sml.utp` (Appearance 1) → `placeable_types` row 1 (`PLC_SmFerDrB_01_0`, BlendTree `PRB_door.anb`, `StateCnt_Door`, sounds `ClosedToOpened`/`OpenedToClosed`, whose player was not traced) → `statecnt_door`:
- **Closed_Unlocked** (0): Action0 OPEN → Success0 2 (Open), Success0a 4 (Open_2, the other way).
  - [0][2] = 1013 `door_closetoopen`, then the idle 1010 `door_open`.
  - [0][4] = 1075 `door_closetoopen2`, then 1073 `door_open2`.
- **Open** (2): CLOSE → 0, 1014 `door_opentoclose`, then 1011 `door_close`.
- **Locked:** UNLOCK → 0.

The animation's name (`anim_placeables.AnimName`) is played from the type's `BlendTree`.

**The walk grid.** At load the placeable reads `<ModelName>_<Appearance>.pwk` (0x007500E0; for this door `plc_smferdrb_01_0_1.pwk` in `pathfindingpatches.erf`, GFF V4.0), one grid patch per state, kept at +0x690. A state change takes the old patch out and stamps the new one into the area's grid at the placeable's position and facing (0x00A46CC0):
- taking out sets 7s and 8s back to 1;
- stamping writes the patch where a cell is below 8.

The door's closed patches are 7 (unlocked) and 8 (locked); open is a turned strip of 8s. Inferred: 8 blocks, 7 is a shut door the pathfinder may plan through.

### (e) Containers and the loot panel

- **Opening.** `statecnt_container` (and `statecnt_bodybag`, `gen_ip_bodybag` being Appearance 16) row 0 offers only OPEN_INVENTORY (11), with Success -1. The panel is the script's: USE with action 11 → OPENED → `OpenInventory(self, user)`.
  - The native (0x0065FA40) sets +0x618 = !bCanAddItems and sends `GUI::ShowContainerGUIMessage(container, canAdd)`. The GUI system turns that into ShowInventoryGUIMessage: the panel, in the GUI mode.
- **The lid does not move.** Success -1 changes no state, so a chest stays Closed_Unlocked. Its `container_closetoopen`/`container_open` play only if a script sets the state to 2.
- **Close** sends `PerformUseObject <char> <container> 2`: a USE_OBJECT command with CLOSE. A container in Closed_Unlocked doesn't offer CLOSE, so the command fails -1 at the action check and the container's script gets no USE. Only a container a script had put in Open would close, with 1028 `container_opentoclose`, then 1025.
  - The same use, before the check, moves a body bag's creature's items in again, if any are left.
- **`Container.InventoryClosedMessage`** goes to the placeable (0x0074E8D0 → 0x0074E710). With +0x618 set and the inventory empty:
  - it makes the placeable not interactive (0x0069BA70(0), at 0x0074E76E; not inactive: corrected 2026-10-18 by lib-agent-2);
  - for a body bag, it handles the dead creature, and destroys a `plc_bagcoin03_0` bag;
  - with +0x614 set, it destroys the placeable.
- **Every removal runs the same check**, not only the close: a take ends in 0x0074FC60, REMOVED (7) to the container, then 0x0074E710, so a bag emptied by Take All is not interactive at once. `dev/ITEMS.md`'s "Looting" has the takes and their events (read 2026-10-18 by lib-agent-2).
- **The body bag's items** come in at sub-action 21, so one event delay before the script's `OpenInventory` shows them.

Not read: the sound columns' player and where a click is refused on an unusable placeable. The event filter 0x00698490 is the object's disabled set, and `AddEventDeltaTime`'s 10 is the handler id, not milliseconds (read 2026-10-18 by lib-agent-2, `dev/LOAD_EVENTS.md`).

## A placeable's animations, initial state and state tables: read 2026-10-18

Read in the GOG exe and the game's files by lib-agent-2 (with subagents) for `dev/PARAGON_DOORS.md` parts 2 and 3. Not seen in play. The load's calls to `SetPlaceableState(state, 1)` were checked in the disassembly.

### The clip: found as a creature's is, from the type's BlendTree

- **The tree.** When the model is ready (the animation component's callback 0x007520E0 → 0x00751FB0), the placeable gets one tree: `placeable_types.BlendTree` (column hash 0x70F463FA). It goes through `AnimBase` slot 0x8C (0x00A61B10), the same AddTree creatures use.
- **Node names to ids.** AddTree maps each node's name to an id through table 0's `AnimName` (the merged Animations M2DA: `anim_base`, `anim_combat`, `anim_placeables`; 0x005CB4F0/0x005CB780). So an `anim_placeables` id plays the `.anb` node named by its `AnimName`, exactly as `anb::ClipOf` does.
- **No Replacement fallback.** `CCObject` slot 0xB0 (0x0069B3D0) falls back to the Replacement row only when slot 0x10 answers non-zero. `CCPlaceable`'s slot 0x10 answers 0, so a missing node plays nothing.
- **The door's tree** (`PRB_door.anb`):

  | Node | Clip |
  |---|---|
  | `door_closetoopen` | `ip_door.closetoopen.ani` |
  | `door_open` | `ip_door.open.ani` |
  | `door_opentoclose` | `ip_door.opentoclose.ani` |
  | `door_close` | `ip_door.closed.ani` |

  Plus `door_closetoopen2`, `door_open2`, `door_opentoclose2`, `door_dying`, `door_dead` (and their 2s), and `bdmg`.
  - The transitions are 0.7 s; the idles are a one-frame pose.
  - `anim_placeables` Type is 0 on the transitions and 1 (looping) on the idles.
- **The container's tree** (`PRB_container.anb`) points every node (`container_open`, `container_closetoopen`, `container_opentoclose`, `container_close`) at the one-frame `prp_chstwd_01.close.ani`. So a chest's lid never visibly moves, whatever its state.
- **Sound.** `ip_door.closetoopen.evt` has an event (id 27, at 0.01 s) with the text `ClosedToOpened`, a `placeable_types` sound column's name. Inferred: the door's sound is played from the clip's event.

### Rigid models: the clips move nodes, no bones

- **The door.** `plc_smferdrb_01_0.mmh` has `MMHTotalBones` 0.
  - Its mesh `DoorLow` is a child of `GOB`, with no bones used; the other nodes are `UserPoint01`/`02` and `SnaptoPoint01`.
  - The `.msh` has no blend weights or indices.
  - Its origin is the hinge (inferred from the bounding box, x 0 to 1.63).
- **The door's clips.** The channels are `doorlow_rotation` (and `doorlow_translation` in the idle). They match the `.mmh`'s **export names** (`MMHExportExportName`), not the node's name: a channel drives the node whose export has its name, as a rigid transform.
- **The wooden chest** (`PLC_ChstWD_01_0`) is the same: no bones; a `Lid` mesh (pivot 0, 0, 0.5) under `Body`; channels `body_*`, `lid_*`, `userpoint01_translation`.

### Playing it

`SetPlaceableState(s, 0)` plays the transition [old][new] as `(anim, 1.0, -1.0, 1)`.
- **The arguments.** A blend of -1.0 snaps the node's weight at once (0x00815100), so there is no cross-fade into the transition. -2.0 fades over 20% of the clip's length (its TransitionTo chain summed, 0x006957A0), -3.0 over 5%, and any other value is a rate. The fourth argument (loop) is never read: looping comes from the tree's node, or `Type & 1`.
- **The idle after.** The idle [new][new] is set as the next animation (0x006956D0: the component's +0x140, if it loops). When the transition ends, `CCObject` slot 0xCC (0x00696C50, inferred: animation ended) plays it as `(idle, 1.0, 3.0, 0)` (the 3.0 at 0x00AE7130). That is a blend at rate 3, about a third of a second if per second (inferred).

### The initial state is the state controller's row, shown as its idle at once

- **The field.** The `.are`'s placeable instance has `InitialState`; the `.utp` has none. The `.utp`'s `AnimContState` is not in the exe (inferred: the toolset copies it into the `.are`).
- **The load, in order** (0x00683380 → 0x0074EFE0):
  1. The placeable's field reader (slot 23, 0x00750370) runs over the `.utp`, then the instance, as creatures' does. Its fields: `Useable`, `Static`, `Appearance`, `BaseType`, `TurnRate`, `StateController`, `RoomID1`/`RoomID2`, `InitialState` → +0x658.
  2. The state is 0 until the instance gives one.
  3. The load step (slot 113, 0x007528D0) calls `SetPlaceableState(+0x658, 1)` (0x00752A88). When the model loads later, the model-ready callback makes the same call (0x00752076).
- **The value** is the row of the type's `statecnt_*` table, the `PLC_STATE_*` constants in `2da_constants_h.nss`: for a door 0 Closed_Unlocked, 1 Closed_Locked, 2 Open, 3 Dead, 4 Open_2, 5 Dead_2.
- **Flag 1** re-applies the state even when unchanged, plays no transition, and plays the idle [s][s] as `(anim, 1.0, -2.0, 1)`. It stamps the state's walk-grid patch, and makes the placeable not interactive if the state has no actions (not inactive: corrected 2026-10-18 by lib-agent-2).
- **Out of range.** A state of -1 (0xFFFF) or out of range is stored as 0, and its animation lookup gives none.
- **A save** keeps the state as `SavegameAreaPlaceableState` (16100, the u16 +0x658). It is restored into +0x658 (slot 34) and snapped the same way by the load step.
- `0x0074FBC0`, the third flag-1 caller, is not a load: it re-snaps the state when a conversation or staged scene releases its actors (0x007F2800).

### The statecnt_* tables

- **The tables.** The core game has 20: `aoe`, `areatransition`, `bbase`, `bodybag`, `bridge`, `cage`, `container`, `container_static`, `door`, `door_secret`, `flipcover`, `furniture`, `informational`, `nonselectable_trap`, `puzzle`, `selectable_trap`, `static`, `topballista`, `trap_trigger`, `trigger`.
- **The columns are always:** `ID`, `Label`, one column per state (N, the row count), `Action0`, `Success0`, `Success0a`, `Action1`, `Success1`, `Success1a`, `BlockVis`, `StrRef`. There are no other columns; the sounds are `placeable_types`'.
- **The parser** (0x00A3BBB0, cached by name by 0x00A3BE40, a table with no rows dropped):
  - It reads the state cells and the actions **by position**, from column 2.
  - The number of action groups is (BlockVis's column − (N + 2)) ÷ 3, at most 2.
  - `BlockVis` is the only column found by name (hash 0x86A27132). `ID`, `Label` and `StrRef` are not read. So the state columns must be exactly N, in row order.
- **The cells.** Cell [row = from][column = to] is an `anim_placeables` id: [s][s] is the idle, the others transitions. A lookup with to = 0xFFFF means to = from.
- **Actions.** An action of 0 is none, and a state with both 0 is made not interactive (a door's Dead rows, which stay drawn; not inactive: corrected 2026-10-18 by lib-agent-2). A container's Dead row keeps OPEN_INVENTORY, so it stays lootable.
- **Success and SuccessA.** `0x00A3BB10(state, action, variation)` gives Success for variation 0 and SuccessA otherwise, or 0xFFFF for an action the row doesn't have. A result of -1 means stay: `SetPlaceableActionResult` changes nothing for it. The variation is the use's int 0, the front side, which the scripts invert for OPEN (`dev/PLACEABLES.md`).
- **BlockVis** (the door: 1 for the closed rows, 0 for Open, Open_2 and the Dead rows):
  - At the area's add (slot 63, 0x00750B10), a placeable with a blocking state and two different `RoomID`s is registered as a door between those rooms (0x0068C190).
  - The current state's BlockVis (0x0074BCC0) is then read by a room-to-room walk that passes through doors not blocking (0x00686A70; inferred: visibility between rooms).
  - For a door (BaseType 1), 0x0074E850 sets a value on its model, 0.6 when open and 1.0 when shut (inferred: a fade or occlusion).
  - A state change sets the area's +0x608 (inferred: visibility dirty).
- **The walk grid's patch is chosen by the state's index** (0x0074DBA0: +0x690[state], skipped past the count at +0x694). The door's `.pwk` has six patches for its six states (that the file's order is the state's is inferred from the shapes: closed 7s, locked 8s, open a strip).

## A placeable clicked: Action0, always

- **The left click.** `GameModeExplore::HandleMessage_PerformPrimaryActionOnTargetUnderCursorMessage` (0x007132B0) calls 0x00712830(target, kind, **0**) (0x007132D5, checked).
  - For a placeable, the kind is 8 when either of the state's actions is set (0x0070CBE3).
  - Kind 8 takes the selection's first member as the user, and passes `target->slot 0x100(0)`, the state row's **Action0**, to `Orders::UseObject` (0x00712C8F, 0x00712CD4).
  - When Action0 is 0, the click does nothing, even if Action1 exists.
  - No skill, lock or trap is looked at, and no variation is chosen; the variation is sub-action 21's front-side test.
  - A dead creature whose body bag is set (+0x790) redirects to the bag (0x00712C34).
- **The secondary action.** `PerformSecondaryActionOnTargetUnderCursorMessage` (0x00713300), and the two `…OnTarget` handlers for an object id in 0x00C06794, pass **Action1** the same way.
  - The secondary kind is 4 when Action1 is 4 (DESTROY: an attack), else 8.
  - Its default binding was not found.
- **The tooltip.** It shows Action0's name, or Action1's while the input `ShowSecondaryInteraction` (50) is held. The names are table 100's rows by action (0x0070CDA0).

Read 2026-10-18 by lib-agent-2 for DAOParagonEngine, not seen in play.

## Not interactive, removed when dead, hidden: read 2026-10-18 by lib-agent-2

Read 2026-10-18 in the GOG exe, the game's files and the scripts by lib-agent-2 (with subagents) for DAOParagonEngine's side-by-side of the Harrowing's Fade. Not seen in play. The key claims were checked again in the disassembly or the files. The notes above that said "inactive" for a state with no actions and an emptied container were corrected the same day.

- **`SetPlaceableState`** (0x0074F9D0), when both of the new state's actions are 0 (0x0074FB61–0x0074FB89), calls **0x0069BA70(0)** (0x0074FBA1). That is `SetObjectInteractive`'s native (command 676; the address library's 14239, renamed from `ForScript_SetObjectActive` in fe62e8f). It writes +0x42C, which `GetObjectInteractive` (677) reads. It neither calls SetActive (+0x140) nor touches the model's visibility. So **a door in its Dead row stays drawn**, as the broken door is in vanilla.
- **`Container.InventoryClosedMessage`** (0x0074E76E) makes an emptied container not interactive the same way.
- **A dead placeable is removed only by its type:** at 0x0074FAB2, with health (+0x66C) ≤ 0 and `placeable_types` `RemoveWhenDead` (hash 0x1DEAA1EA) set, it calls slot +0x198(0) (0x00696A60; inferred: destroy). The death handler 0x0074D200 does so after a fade, at 3000 ms. Every `StateCnt_Door` type has `RemoveWhenDead` 0.
- **A placeable destroyed takes its current state's walk patch out** (read 2026-10-22 by lib-agent-2 with a subagent, for DAOParagonEngine #164) [C unless marked]:
  - **The path.** `DestroyObject` reaches `CCObject` slot +0x198 (0x00696A60, not overridden by `CCPlaceable`), which sets +0x2BD and posts event 0xB. `CCPlaceable`'s event slot (+0xC8, 0x0074D030) passes it to 0x00698A50.
  - **Event 0xB** (0x00698B41): when the byte +0x2BC is non-zero it calls slot +0xFC, then the deleting destructor (slot 0 with 1); when it is 0, nothing. Where +0x2BC is set was not read.
  - **`CCPlaceable`'s slot +0xFC** (0x00750A60) calls 0x0074DBA0(word +0x658, the current state, remove 1) at 0x00750AB9. It then calls 0x0067FB70(area, id) and goes on to `CCObject`'s 0x00697060. The destructor (0x007515B0) has no stamp work of its own.
  - **0x0074DBA0 does nothing** when the state is at or past the patch count (+0x694), or the patch pointer (+0x690[state]) is null, or bit 0 of +0x6D8 is set and the byte +0x638 is not 1, or health (+0x66C) is ≤ 0 and 0x0074D1B0 returns true [C for the tests; I that the last is a dead state without a patch].
  - **Its other callers** (a scan of `.text` for calls to it): remove in `SetHealth` at ≤ 0 (0x0074E921), in `SetActive` (0x0074F21E, 0x0074F234), in `SetPlaceableState` the old state out and the new in (0x0074FB27, 0x0074FB4E), stamp in on adding to an area (0x00750CB6); 0x0074F867 in slot +0x104 (0x0074F7A0) not read.
  - **The mage origin's bricked doorway**: `bhm000pt_tranquility.nss`, `case CANNON_FIRED`, finds `bhm700ip_brick_wall` and calls `WR_DestroyObject` (`DestroyObject(obj, 0)`), with no state set and no `SetObjectActive`. Its appearance is 50, "Doorway, Bricked" (`plc_bdoor01_0`, `StateCnt_Door_Secret`, `GeneratePathInfo` 1). So in vanilla its cells open through the destroy's slot +0xFC, unless a guard above holds [I for this wall's patches and +0x2BC].
- **SetActive(false) does hide** (`CCObject` +0x13C, 0x0069A950, for creatures and placeables alike):
  - **In explore or combat**, it sets the visibility target (+0x18D, 0x00695C00), and 0x00695A90 fades the alpha (+0x190) at 1/500 a millisecond (0x00B03AA8), so 0.5 s. At 0 it unregisters the object (0x00649C30) and calls slot +0x108 (inferred: out of the scene). The model's alpha is +0x198 × +0x190 (0x00695080).
  - **In other modes** (loading, cutscene, dialogue) it hides at once (0x00695C20).
