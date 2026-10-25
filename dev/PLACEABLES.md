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

The start handler (0x00745920) adds, for a creature user, sub-action 4 (`ApproachMelee`, 0x007AA560: the target, the user's `PersonalSpace`, walk or run). It then adds sub-action **21** (0x007A78F0), the use, with a copy of the command's data. No turn is queued, and nothing turns the user to face the placeable. The end is the shared one (0x00746900): COMMAND_COMPLETE is run at once at the use's next step (commands.md) with int 1 = the result; the success ends at the head (corrected 2026-10-24 by lib-agent-2).

### (a) Where the user walks to

The spot is the placeable's melee-ring spot (`CCPlaceable` slot 38, 0x0074F240 → 0x0074DFB0). **No model use-point node is used.**
- **The rectangle.** The placeable's walk-grid patch for its current state, given a 0.5 margin and turned by its facing.
- **The spot.** Where the line from the user to the centre crosses that rectangle, pushed out by the user's `PersonalSpace` + the placeable's `InteractRadius` (template field, +0x3CC) + an extra.
  - The extra is 0, except for a door (`placeable_types.BaseType` 1): 0.25, or 0.75 × the long side up to 2.5.
  - A user already nearer than the edge stays where it is.
- **Fallbacks.** Spots in front of and behind the placeable along its facing, then a safe location nearby.
  - Each spot must be free (the path object's IsFree, B, 0x00A62A90; corrected 2026-10-25 by lib-agent-2); a door's also needs line of sight.
  - If none is found: -5 (`COMMAND_FAILED_NO_SPACE_IN_MELEE_RING`).
- **The spot function** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169; 0x0074F240, CCPlaceable slot 38; it calls 0x0074DFB0 for the rectangle and the line spot) [C]: its two calls of `CCArea::GetSafeLocation` (0x0073E370, `(pos, maxRadius, controller, reachable, out, snapToGround, creatureCheck, minRadius)`) pass:
  - **After a spot is accepted** (0x0074F591): pos = the spot, maxRadius = 1.5 for a door (`this[0x638] == 1`), else 3.0, **reachable = 0**, out = the spot, **snapToGround = 1**, creatureCheck = 0, minRadius = 0.0.
  - **The last resort** (0x0074F618): pos = the placeable's position (+0x120), maxRadius = max(sqrt(…), 1.0) + 0.5 + the argument (the user's PersonalSpace) + the value of 0x006953D0, reachable = 0, snapToGround = 0, creatureCheck = 0, minRadius = 0.0. The square root is `_CIsqrt` of a value computed just before ([I]: the patch's half diagonal; the operands were not traced).
  - In both the pushes were read right to left; the virtual call at +0x10 of the user between the pushes is taken as a call with no stack arguments ([I]; the decompiler also reads reachable 0 and snapToGround 1 in the first call).
  - So neither call sets the `reachable` flag (the ray 0x0067CFA0 inside IsLocationSafe, 0x0073DE70, is made whatever the flag). The door's own line-of-sight test (`Combat::LineOfSight`) is made by 0x0074F240 itself on the spot before the first call, with z from 0x0073DE20.
  - **The placeable's aim point, `0x0074D580`** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) [C]: it is slot 29 (vtable +0x74) of `CCPlaceable`'s primary vtable `0x00B1C084` (the slot's bytes at `0x00B1C0F8` are `80 D5 74 00`), the same slot `Combat::LineOfSight` (`0x0067C750`) calls on a target that is not a creature (the `*(param_2) + 0x74` call after the creature test at vtable +0x10) to get the point the sight ray goes to. It copies the placeable's position (+0x120, +0x124, +0x128) into the output vector; then, if the model pointer at +0x154 is non-null and `(+0x154)->+0x18->+0x04` is non-null, asks that object's vtable +0x7C with the argument 1 for a node, and when the node exists asks the node (its vtable +0x10) to carry the point (0, 0, 0, 1) and passes the result to `0x00452EF0` with `this` = the output. `0x00452EF0` copies that Vector4 to a local, runs `0x004D4650` on it (a normalisation of the w component: it divides by w unless |w| is under a small constant at `0x00B190A4`) and stores it into `this`: the output is REPLACED by the node's carried origin divided by w, it is not the position plus the node's translation. With no model, no such node or no model object the output stays the position. What node index 1 is (the second node of the model's tree) and whether the node's transform is world-space were not read [I]; `GUI.md` (PlaceCamera, "The head node supplies only a position") reads the same call shape on a creature as a world translation.
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

Added (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2 with subagents, for DAOParagonEngine #263) [C unless marked]:
- **A door opened by its use, and the next command, frame by frame:**
  - **Frame N, the user's update:** sub-action 21 queues USE with delay (0, 0). `AIMaster::AddEventDeltaTime` (0x0064ACE0) tail-calls `AddEventNow` (0x0064A630). Its due time is the world clock now, first in, first out among equal times. The sub-action answers 1, and `StepCommands` returns.
  - **Frame N+1, the event drain:** World_Update (0x0064B010) runs `AIMaster::Update` (0x0064AF00, at 0x0064B047) before the objects' loop.
    - An event is due when its time is ≤ now (0x0064A850, compare 0x00A35BE0). The world clock moves once a frame, so a delay-0 event queued in N is delivered at N+1's drain.
    - The door's script runs in the drain. `placeable_core.nss`:38–39 → `Placeable_HandleUsed`; the OPEN case (`placeable_h.nss`:198) has no delay and ends in `SetPlaceableActionResult` (:550), which reaches SetPlaceableState in the same call.
    - **SetPlaceableState** (0x0074F9D0) calls ApplyWalkPatch (0x0074DBA0) with (old state, 1), then, in an area, 0x0074E850(0), then (new state, 0). So the grid is open when the drain ends. (0x0073D710 runs only when its third parameter is 0; it loops GetCreatureById over a party vector and discards the result, so it has no effect.)
  - **Frame N+1, the user's update:** `CCObject::StepCommands` (0x0073D320) first, when the current command has no sub-actions left (record +0x14 == 0, 0x0073D360):
    - clears it (`ClearCurrentCommand(1, 1, 1, 0)` 0x0073C730, which ends it with success and runs COMMAND_COMPLETE at once, so rules_core's handler runs inside the user's step, before the next command starts) (corrected 2026-10-24 by lib-agent-2);
    - starts the next command (0x0073C7D0) when the queue has one (+0x2D4);
    - and then runs the current command (`RunCurrentCommand` 0x0073CB60, at 0x0073D3AC) in the same call. It returns when that answers 1.
    - So a command finished in frame N has its successor started in N+1. Its sub-actions run on in that call, down to the walk's first RequestPath, which sees the door open. The walk sub-action's walking is also gated by bit 2 of its parameter byte.
    - +0x14 is the sub-action list's size, and RunCurrentCommand pops each sub-action that answers 1. So the list is empty after the use's last sub-action, and the next update's head ends the use [C] (read 2026-10-24, checked by lib-agent-2; was [I]). No other place starts a command.
    - This fits vanilla's trace: the use's success and the resumed attack's start are 0.2 ms apart (DAOParagonEngine #263, Lyna/Slot_304).
  - **The drain's cap** (0x0064AE30):
    - The first due event is always delivered.
    - After it, the loop goes on while the elapsed time is at most 1,000 µs plus 100 µs per ten queued events (0xC01384 = 100, 0xC01380 = 10).
    - With more than 500 events queued (0xC0137C), or while loading, every due event is delivered.
    - So behind slow events the door's USE can wait a frame or more [I how often].
    - Even then, the first search call answers 4 from the setup unless both ends are one first-level node (PATHFINDING.md, "The setup's answers"); there the one-node case can still answer 6 on its first call.
  - **The objects' loop** updates each object at most once per World_Update. Its budgets per AI level are 3,000, 1,200, 480, 192 and 128 µs (0x00B06B0C..0x00B06AFC). The first object of a level always runs, and all objects run while loading. A skipped creature only waits longer.
  - **The scripts' side** (`rules_core.nss`:756–771, `ai_main_h_2.nss`:240–300):
    - On COMMAND_COMPLETE with −9 (a shut door), AI_DeterminePathBlockedAction is run for a controlled creature, or for a non-follower unless AI_DISABLE_PATH_BLOCKED_ACTION is set.
    - For an unlocked door it queues `CommandUseObject(door, PLACEABLE_ACTION_OPEN)`, then `GetPreviousCommand` with COMMAND_ADDBEHAVIOR_DONTCLEAR. So the use is done first and the command put back runs after it, in the order above.
    - **A locked door** (`ai_main_h_2.nss`:285–296) [C, read 2026-10-25, checked by lib-agent-2]: a creature that is not controlled queues a quick wait, then re-queues `GetPreviousCommand` with DONTCLEAR, so it tries again after the wait. A controlled creature gets no action.
    - **Anything else** (not a door state) answers FALSE, and `rules_core.nss`:777–797 runs `AI_DetermineCombatRound` instead [C].

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

A failed sub-action ends the command with its code (`StepCommands` 0x0073D320 → `ClearCurrentCommand(code)` → COMMAND_COMPLETE, run at once (immediate 1)) (corrected 2026-10-24 by lib-agent-2).
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

**Example, the Circle tower's level 2** (`cir210ar_tower_level_2`, layout lak504d; read 2026-10-25 by paragon-agent-2, checked by lib-agent-2 in the scripts and the files; the walks are paragon-agent-2's in vanilla through Eluvian) [C unless marked]:
- **The Metal Door** `genip_door_tev_lrg_r` at (−19.88, 6.2), between rooms 139 (the hall) and 3298 (the stairs), has InitialState 1 (locked) and Useable 0. Its `.pwk` (`plc_lrgrfrcdrr_01_0_93.pwk`) has six patches:
  - 0: a 17 × 2 strip of kind 7 (shut);
  - 1: the same strip of kind 8 (locked);
  - 2: a 2 × 17 strip of kind 8 elsewhere (open);
  - 3–5: mixed cells of kinds 1 and 8.
  - So the locked door's kind 8 strip keeps every walker out.
- **The two barriers** `cir210ip_blood_mage_barrier` (template `cir210ip_barrier`, appearance 233, team 134, Active 1, at (25.08, 0.05) and (26.34, 0.05)) close the other way round, through room 3297.
- **What drops the barriers:** `cir210ar_tower_level_2.nss`:111–130, on TEAM_DESTROYED (1019) for team 22 (the blood mages), **only while plot flag 11 (`BLOOD_MAGE_1_KILLED` of `plt_cir000pt_main`) is FALSE**; it does not set the flag, and with the flag already TRUE nothing drops the barriers there.
  - It resurrects blood_mage_02 (`EffectResurrection`), starts its conversation (`UT_Talk`), and calls `UT_TeamAppears(134, FALSE, OBJECT_TYPE_PLACEABLE)` (`utility_h.nss`:1672), which sets each barrier inactive (`WR_SetObjectActive`). That takes its patch out (SetActive, PARTY.md).
  - TEAM_DESTROYED goes to the area: `creature_core.nss` (about :328–340) takes the dying creature out of its team (`SetTeamId(self, −1)`), and when `GetTeam` then has none left calls `SendEventTeamDestroyed(GetArea(OBJECT_SELF), team)` (`events_h.nss` about :989): `Event(1019)` with int 0 the team.
- **In vanilla** a direct move from the hall to the stairs fails (COMMAND_FAILED_INVALID_PATH, beside the locked door). The way round through 3297, 3432 and the open door at (−21.88, 20.64) reaches them [paragon-agent-2's play].

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
  1. The placeable's field reader (slot 23, 0x00750370) runs over the `.utp`, then the instance, as creatures' does. Its fields include `Useable`, `Static`, `Appearance`, `RoomID1`/`RoomID2` and `InitialState` → +0x658; `BaseType`, `TurnRate` and `StateController` are `placeable_types` columns, not GFF fields (corrected 2026-10-25 by lib-agent-2; the full list below). 0x00750370 first runs CCObject's reader (COMBAT.md, "A placed creature's load"), then reads these fields, in order [C]:
     - **The lock and key:** AutoRemoveKey (+0x5E8), MaxHealth, KeyName (+0x574), LocPopupText (+0x5B8).
     - **The trap:** TrapDisarmable (+0x5F8), TrapDetectable (+0x62C), **TrapDisarm** (+0x648), **TrapDetection** (+0x644), TrapFlag (+0x628), TrapOneShot (+0x630), TrapType (+0x64C), TrapCreator (+0x660).
     - **The body bag and state:** IsBodyBag (0x0074BD70), LootableObjectId (+0x664), **InitialState** (a u16, +0x658).
     - **Useable,** with the interactive byte (+0x42C) as its default, then `SetObjectInteractive` (0x0069BA70).
       - The inventory load saves and restores +0x42C, so Useable's choice survives it.
     - **Static** (+0x620, forced 0 when +0x624 is 1).
     - **The appearance and lock:** ArtMode, Appearance (+0x4F0), **PickLock** (+0x5E0), KeyRequired (+0x5E4), DieWhenEmpty (+0x614).
     - **GroundPile:** read, then +0x5FC is set to 1 whatever was read.
     - **`placeable_types` by Appearance:** BaseType, TurnRate and StateController are those table columns (hashes 0x5010EDC3, 0x5F75A070, 0x9A320A51), not GFF fields.
     - **The inventory** (0x00761EC0, under a flag test).
     - **The rest:** Rank (+0x6BC), TreasureCategory (+0x6C0), RoomID1 (+0x6A8), RoomID2 (+0x6AC).
  2. The state is 0 until the instance gives one.
  3. The load step (slot 113, 0x007528D0) calls `SetPlaceableState(+0x658, 1)` (0x00752A88). When the model loads later, the model-ready callback makes the same call (0x00752076).
- **The value** is the row of the type's `statecnt_*` table, the `PLC_STATE_*` constants in `2da_constants_h.nss`: for a door 0 Closed_Unlocked, 1 Closed_Locked, 2 Open, 3 Dead, 4 Open_2, 5 Dead_2.
- **Flag 1** re-applies the state even when unchanged, plays no transition, and plays the idle [s][s] as `(anim, 1.0, -2.0, 1)`. It stamps the state's walk-grid patch, and makes the placeable not interactive if the state has no actions (not inactive: corrected 2026-10-18 by lib-agent-2).
- **Out of range.** A state of -1 (0xFFFF) or out of range is stored as 0, and its animation lookup gives none.
- **A save** keeps the state as `SavegameAreaPlaceableState` (16100, the u16 +0x658). It is restored into +0x658 (slot 34) and snapped the same way by the load step.
- `0x0074FBC0`, the third flag-1 caller, is not a load: it re-snaps the state when a conversation or staged scene releases its actors (0x007F2800).
- **What the install's areas carry** [S, paragon-agent-2's survey of 238 `.are` files; arl100ar spot-checked]:
  - **Every placeable instance** has TeamId, Rank, PickLock, TreasureCategory, TrapDetection, TrapDisarm, Useable, InitialState, Active, RoomID1/RoomID2 and InteractRadius (0), and none of the other fields.
  - **Instances that differ from their template:**

    | Field | Placeables that differ |
    |---|---|
    | Rank | 1,031 |
    | TreasureCategory | 731 |
    | TeamId | 659 |
    | Useable | 293 |
    | PickLock | 201 |
    | TrapDetection | 111 |
    | TrapDisarm | 106 |

  - **For a port:** a placeable's lock, trap difficulties, treasure, team, rank and interactivity come from the instance.

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
  - In `CCPlaceable::SetLocation` (slot 62, +0xF8, 0x00750B10; the override of `CCObject::SetLocation` 0x00696F90, which it calls at 0x00750C96; corrected 2026-10-24 by lib-agent-2: "the area's add, slot 63" was Ghidra's count from 1, and the function is SetLocation, not an add), a placeable with a blocking state and two different `RoomID`s is registered as a door between those rooms (0x0068C190).
  - **SetLocation stamps the current state's patch** (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
    - After the placement call (0x00696F90, which sets the position through 0x00695540 and calls 0x0068BF90), SetLocation asks whether the placeable is active (its +0x70 interface's slot 0x6C, GetObjectActive). If it is, it calls 0x0074DBA0(word +0x658, 0), which stamps the patch of the current state (+0x690[state]) into the area's grid through 0x00A46CC0. It then recomputes the room (`CCObject::UpdateRoom` 0x00695D70).
    - So every SetLocation of an active placeable stamps its current state's patch, and the door registration (0x0068C190) also runs there. That it runs whenever a placeable is put somewhere, into an area at load among them, is [I]: SetLocation's callers were not listed. An inactive one stamps nothing until it is changed. (corrected 2026-10-25 by lib-agent-2: for a placeable that is not a door the stamp is skipped here until model-ready sets +0x6D8, and for good without a physics object; the next bullet.)
    - **The mask is set at model-ready** (CCPlaceable slot 66, 0x0074F7A0; read 2026-10-25 by paragon-agent-2's helper, checked by lib-agent-2) [C unless marked]. The stamp's tests are in "0x0074DBA0 does nothing" below; the one that matters here is bit 0 (Items) of +0x6D8 with +0x638 ≠ 1:
      - It reads the `placeable_types` row's `EnableCollisionEvent` (column hash 0xE19F98B1).
      - **When:** the read succeeds, and EnableCollisionEvent is non-zero or +0x6D8 is still −1, and the model has a physics object (`[[+0x70 slot 0x60]+0x18]+4` − 8, its +0x1BC).
      - With EnableCollisionEvent non-zero it calls the physics object's slot 0x5C(0.2) (not followed).
      - Then, only while +0x6D8 is −1, it sets +0x6D8 to the physics object's slot 0x50 (the OR of the shapes' bits0) and stamps the current state's patch, `0x0074DBA0(+0x658, 0)` (0x0074F867), with no active test.
    - **So until model-ready, and for good without a physics object, a placeable that is not a door is never on the grid.** +0x6D8 starts at −1, whose bit 0 is set, so every earlier stamp (SetLocation, SetPlaceableState) is skipped.
      - A door is stamped either way.
      - A model whose shapes include the Items flag never stamps.
      - A `.phy` whose shapes carry no mask gives 0 and stamps [I: that such a model still makes a physics object].
    - **Example, Jarvia's hideout** (`orz230ar_gangsters_hideout`) [paragon-agent-2's reading of the files]:
      - The tripwires (`genip_tripwire`, appearance 45, `plc_wiretr_0.phy`: two posts at 0x81000004) and their oil barrels (`PLC_BarrelOil01_0`, 0x81000004) have no Items bit, so they stamp.
      - The tripwire's patch (`plc_wiretr_0_45.pwk`, 26 × 1 cells of 1/6 m) is kind 8 only at the posts, with kind 1 between, so the wire itself is walkable.
    - **The rows that never stamp** [S, paragon-agent-2's survey of `placeable_types`: every row with a `.pwk` has a `.phy`]: 24 rows that are not doors have a shape with the Items flag, so they never go on the grid:
      - 38, the blade trap;
      - 44, 188 and 376, corpse knight;
      - 139 and 520, coin bag;
      - 142, a wild flower;
      - 145, the trash pile;
      - 175–177, bone piles;
      - 181–183, 191 and 192, corpses;
      - 245, the large anvil;
      - 256 and 262–264, herbalism plants;
      - 372 and 373, slum bridges;
      - 474, invisible small item.
    - **Slot 63** (+0xFC, 0x00750A60, ID 17922) is the other way: it takes the current state's patch out (0x0074DBA0(+0x658, state, 1)) before its CCObject base 0x00697060 (ID 14143), which calls 0x0068B050 ("A placeable destroyed takes its current state's walk patch out" below). That 0x00697060 takes the object out of its area is [I].
    - The placeable keeps no copy of the grid itself: what the grid holds is the stamp, and a later state change or removal takes it out again (above).
  - **Example, from paragon-agent-2's reading of the files** (not re-read here): the Gauntlet's `urn230` doors (BaseType 1, Closed_Locked) have a `.pwk` whose state 0 patch is kind 7 and whose states 1–5 are kind 8.
  - The current state's BlockVis (0x0074BCC0) is then read by a room-to-room walk that passes through doors not blocking (0x00686A70; inferred: visibility between rooms).
  - For a door (BaseType 1), 0x0074E850 sets a value on its model, 0.6 when open and 1.0 when shut (inferred: a fade or occlusion).
    - **0x0074E850(bool)** (read 2026-10-22 in the GOG exe by paragon-agent's helper, for DAOParagonEngine #151) [C]: only for a placeable whose +0x638 is 1; it updates the collision (`CCPlaceable::SetCollisionBlocking(0x0074BCC0())`), then calls the placeable's model (+0x70) vtable +0x5C with 0.6 (0x00C0CBC8) when the door does not block and (its argument is set or the mode is not 4 or 5), else with 1.0 [C]. 0x006403F0 calls it on every placeable of the area's list as a conversation or cutscene begins or ends (section 2b) [C]. That +0x5C is the model's alpha, so an open door is drawn at 0.6 outside scenes, is [I].
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
  - **Its other callers** (a scan of `.text` for calls to it): remove in `SetHealth` at ≤ 0 (0x0074E921), in `SetActive` (0x0074F21E, 0x0074F234), in `SetPlaceableState` the old state out and the new in (0x0074FB27, 0x0074FB4E), stamp in on adding to an area (0x00750CB6); 0x0074F867 in slot +0x104 (0x0074F7A0) is the model-ready stamp (read 2026-10-25, the SetLocation bullet above; corrected 2026-10-25 by lib-agent-2: "not read").
  - **The mage origin's bricked doorway**: `bhm000pt_tranquility.nss`, `case CANNON_FIRED`, finds `bhm700ip_brick_wall` and calls `WR_DestroyObject` (`DestroyObject(obj, 0)`), with no state set and no `SetObjectActive`. Its appearance is 50, "Doorway, Bricked" (`plc_bdoor01_0`, `StateCnt_Door_Secret`, `GeneratePathInfo` 1). So in vanilla its cells open through the destroy's slot +0xFC, unless a guard above holds [I for this wall's patches and +0x2BC].
- **SetActive(false) does hide** (`CCObject` +0x13C, 0x0069A950, for creatures and placeables alike):
  - **In explore or combat**, it sets the visibility target (+0x18D, 0x00695C00), and 0x00695A90 fades the alpha (+0x190) at 1/500 a millisecond (0x00B03AA8), so 0.5 s. At 0 it unregisters the object (0x00649C30; the flag tested is +0x431, 0x0069AA2D, 0x00695B52) and calls slot +0x108. The model's alpha is +0x198 × +0x190 (0x00695080).
    - **Slot +0x108** (0x00696F50) does not take the model out of the scene and touches no physics (corrected 2026-10-24 by lib-agent-2, checked by lib-agent) [C]. Unless +0x164 is set, it stops the object's visual effects (its own slot +0x8C: 0x006978C0, 0x007A0750, 0x0079EC20), calls [+0x154]'s slot 8, and recomputes the room (0x00695D70).
    - **The guard:** SetActive's 0x00698CD0 guards on +0x23E (0x00698D60). With +0x23E set and 0x005D5600(id) false, activation skips the +0x13C call; deactivation always calls +0x13C(0) [C].
    - **Physics and SetActive:** nothing on the path (0x0074F200, 0x00698CD0, 0x0069A950, 0x00696F50, 0x00695A90) calls the PhysicsObject [C].
      - **A door** (+0x638 == 1) re-derives its mask from its state (+0x2C5) through 0x0074E850 → 0x0074BCC0, which calls SetCollisionBlocking (PhysicsObject slot 0x58) with +0x6D8 when blocking and 1 otherwise. That runs on a state change (0x0074FB3D), on activation (0x0074F8F0, inside 0x0074F7A0) and in the mode 4/5 loop 0x006403F0, not when SetActive(false) is called.
      - **Any other placeable** keeps its `.phy` mask in every state (`dev/COLLISION.md`).
      - **So** an inactive placeable that is not a door still blocks with its shapes [I: whether a model faded to alpha 0 is culled from physics was not read; a play check would settle it].
  - **In other modes** (loading, cutscene, dialogue) it hides at once (0x00695C20).
