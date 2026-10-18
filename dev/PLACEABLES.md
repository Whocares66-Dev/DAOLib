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
- **With no patch,** the radius is 4 × PersonalSpace (or from the body bag's creature's `PerSpace`).
- **Arrival** (slot 33, 0x0074D0A0) is any of: within 1.0 of the centre, no farther from the centre than the spot is, or on the spot. The walk itself ends within 0.05 of the spot.

### (b) The use, the animation and EVENT_TYPE_USE

Sub-action 21 does this in one tick, with no animation and no wait:
1. It fails with -3 if the target is gone, not a creature or placeable, or dead (a placeable may be "dead").
2. It fails with -1 without line of sight (`Combat::LineOfSight`).
3. A body bag (+0x600) with its creature (+0x664) still there has the creature's droppable items and money moved into it now (0x006B9620). This is before the action check, so it happens on any use.
4. **nAction must be one of the two actions of the placeable's current state** (Action0 or Action1 of its state controller row, 0x0074C720). Otherwise it fails with -1 and no event is sent. DESTROY reads as none while +0x2C4 is set.
5. It stores nAction at +0x68C, which is what `GetPlaceableAction` reads.
6. **The event**, both cases queued to the placeable (`AIMaster::AddEventDeltaTime(…, 10, ev)`, so not run at once) and gated by the placeable's event filter (0x00698490):
   - With `TrapFlag` (+0x628) = 1 and a creature user: EVENT_TYPE_TRAP_TRIGGERED (16) and no USE.
   - Otherwise **EVENT_TYPE_USE (14)**: creator the user, **int 0 = 1 if the user stands on the placeable's front side** (dot of user − placeable with its facing ≥ 0). No other ints or objects. **The action is not in the event**: the script reads it with `GetPlaceableAction`.
7. The sub-action answers 1, and the command completes with COMMAND_SUCCESSFUL.

**The user plays no animation natively.** The only use animation in the scripts is the disarm's `CommandPlayAnimation(904)` (`wicrouch_activate`, `placeable_h.nss`:289).

**The placeable's animation is native, driven by its state controller.** `placeable_types` (table 93) gives each type a `StateController` (a `statecnt_*` table) and a `BlendTree` (.anb). A state controller row is a from-state, with:
- one column per to-state, holding an `anim_placeables` id: the cell [from][to] is the transition and [s][s] the state's looping idle;
- then Action0, Success0, Success0a, Action1, Success1, Success1a;
- and BlockVis.

**`SetPlaceableState(o, s)`** (0x0074F9D0) plays the transition [old][new] through slot 0xB0 as `(anim, 1.0, -1.0, 1)`, with the idle [new][new] following. It then stores the state and swaps the walk-grid patch (below). A state with no actions makes the placeable inactive.

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
  - it makes the placeable inactive;
  - for a body bag, it handles the dead creature, and destroys a `plc_bagcoin03_0` bag;
  - with +0x614 set, it destroys the placeable.
- **The body bag's items** come in at sub-action 21, so one event delay before the script's `OpenInventory` shows them.

Not read: the sound columns' player, the event filter 0x00698490, where a click is refused on an unusable placeable, and whether `AddEventDeltaTime`'s 10 is milliseconds.
