# Script commands, group "commands": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts, all checked:
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`, on `CVirtualMachineCommands` (vftable 0x00B0030C). It returns 0, or -639 (0xFFFFFD81) when a pop fails, or -638 (0xFFFFFD82) when a push fails. Either failure aborts the script.
- A `command` is a 0x14-byte pooled record (`dao/Objects.h`). +0 is the sequence number (u16), +2 the type (u8, `Command::SetType` 0x00742090), +4 the parameters (`CDataArrays *`), +8 "signal COMMAND_COMPLETE" (1 from the constructor 0x00741F10), +0xC static, +0x10 player issued.
- `CommandPool::New` (0x007468C0) on the commands system (`app+0x1AC → +0x6C`) gives the next sequence number. That number runs 1..65535 and then wraps to 1, never 0.
- Popping a command (`StackPopEngineStructure(2)`) gives the handler a deep copy, its `CDataArrays` copied too (`Command::CopyFrom` 0x00741FC0, via `CopyGameDefinedStructure` 0x00674AF0). Pushing a command also stores a copy. A pushed null pointer is a null command, which `GetCommandType` reads as 0. `0x0066E3A0` returns a command to the pool.
- **The constructors check nothing about their objects.** Each builds the record, pushes it and frees its own copy. Whether the command does anything is decided when it starts.
- **Running a command.** `CCObject::StepCommands` (0x0073D320) runs each frame:
  - A current command with no sub-actions is ended with `ClearCurrentCommand(1, …)`, which is success.
  - Then the next command is started: `Commands::Start` → vtable 0x00B1BA44 slot 2 (0x00742DA0) → a start handler per type, from the table at `+0x54`.
  - A sub-action answering anything other than 0 or 1 ends the command with that value as its result.
  - Ends go through slot 3 (0x00742190) to an end handler per type (table `+0x58`). Both tables are filled at 0x00747480.
  - Every end chains to the shared end 0x00746900. It clears the sub-actions, stores the command as the object's previous command (+0x2C8, through 0x0073CB20) and, if command+8 is set, sends EVENT_TYPE_COMMAND_COMPLETE (0x00743DD0: int 0 the type, int 1 the result).
  - **A start handler that adds no sub-actions makes its command complete with COMMAND_RESULT_SUCCESS (1) on the next step.** Its return value is ignored (`StartNextCommand` 0x0073C7D0).
- Sub-actions are added by `CCObject::AddSubAction` (0x0073D4D0; names ours): `__thiscall (type, CDataArrays *, a3, byte flag, duration)`. The handler table is `CSSubActionHandler`'s, filled at 0x007AB8D0.
- `CDataArrays::GetInteger` and `GetFloat` (0x00A5A220, 0x00A5A250) answer 0 for an index out of range, compared unsigned. `SetInteger` and `SetFloat` grow the array to index+1.

### RemoveCommandByIndex (534)
- **Handler:** 0x0065C040 (not shared). It calls `CClientExoApp::GetObjectById` and `CCObject::ForScript_RemoveCommandByIndex` (0x0073CA90).
- **Does:** pops the object, then the index (int). It walks the queue list at +0x2CC from its head. At the matching position it unlinks the command (0x0062ABC0) and returns it to the pool (`CommandPool::Release` 0x0066DE30). No event is sent and the command gets no end handler. (checked)
- **State:** the queue is +0x2CC, with its count at +0x2D4. The current command is **not** in this list (its record is at +0x2DC), so index 0 is the first command waiting. (checked)
- **Edge cases:** the index is compared as its low 16 bits, unsigned (`movzx … word`). So -1 means 65535, nothing matches and nothing happens, and 65536 means 0. An invalid object does nothing. The handler returns 0. (checked)
- **Quirk:** a failed pop of the index returns **-638** (the push-failure code) instead of -639. (checked)

### RemoveCommand (535)
- **Handler:** 0x0066EB20 (not shared). It calls `CCObject::ForScript_RemoveCommand` (0x0073C9E0) and `CCObject::ClearCurrentCommand` (0x0073C730).
- **Does:** pops the object, then the command. It matches by the command's **16-bit sequence number** (word +0), not by pointer. (checked)
  - If the current command has that number, it calls `ClearCurrentCommand(-2 COMMAND_FAILED_COMMAND_CLEARED, 0, 1, 0)`. That runs the end handlers and so sends COMMAND_COMPLETE with -2. (checked)
  - Otherwise it unlinks the first queued command with that number and releases it silently. (checked)
- **State:** current command `[+0x2DC]→+0`, queue +0x2CC. `ClearCurrentCommand` refuses a **static** current command (+0xC = 1) for this call, so a static current command is not removed. (checked)
- **Edge cases:** an invalid object does nothing. **A null command with a valid object reads word [0] and faults (an access violation)**: there is no null check at 0x0066EB74. The popped copy is freed. Returns 0. (checked)

### GetPreviousCommand (20)
- **Handler:** 0x0065BE80 (not shared). It calls 0x0073C5E0 (`mov eax,[ecx+0x2C8]`).
- **Does:** pops the object and pushes a copy of the object's previous command. It pushes a null command (type 0) for an invalid object or one that has finished no command. (checked)
- **State:** +0x2C8 is written only by the shared end 0x00746900 (0x0073CB20 releases the old one and stores the ended one). So it is the last command that **ended**: completed, failed or cleared, through any end handler. Commands taken out by `RemoveCommand`/`RemoveCommandByIndex` while still queued never get there. (checked)
- **Edge cases:** any object type works, since the field is on `CCObject`. A push failure returns -638. (checked)

### GetCommandIsPlayerIssued (628)
- **Handler:** 0x00651090 (not shared).
- **Does:** pops a command and pushes its +0x10 as an int, or 0 for a null command. (checked)
- **State:** +0x10 is set by `CCObject::QueuePlayerCommand` (the player's orders). Commands that scripts queue have 0. (checked; see `dao/Addresses.h`)
- **Edge cases:** **the popped copy is never freed**, so each call leaks one command record from the pool. Every other getter here frees its copy. (checked)

### GetCommandByIndex (536)
- **Handler:** 0x0065BFA0 (not shared). It calls `CCObject::GetCommandByIndex` (0x0073C6D0).
- **Does:** pops the object (and looks it up at once), then the index. It walks the queue list +0x2CC and pushes a copy of the command at that position. (checked)
- **State:** queue only. Index 0 is the next waiting command, not the current one (`GetCurrentCommand` 539 reads `[+0x2DC]`). (checked)
- **Edge cases:** the index is its low 16 bits, unsigned. A position past the end, a negative index (65535 or so) or an invalid object pushes a null command, whose type is 0 (COMMAND_TYPE_INVALID). Returns 0 unless the push fails. (checked)

### GetCommandType (541)
- **Handler:** 0x0066EBA0 (not shared).
- **Does:** pops a command, pushes its type byte (+2) and frees the copy. (checked)
- **Edge cases:** a null command gives 0. Engine-only types come back raw: 32 ExecuteEffect, 33 Drive, 39 Sheathing, 22 for MoveToMultiLocations, and 21 for MoveAwayFromObject, not 16. (checked; the type names are `dao/core/CommandWord.h`'s)

### GetCommandPriority (730)
- **Handler:** 0x0066EC10 (not shared).
- **Does:** pops the command, **always pushes 0** and frees the copy. Nothing is read. This matches script.ldf's "DEPRECATED". (checked)
- **Edge cases:** none. A null command still gives 0. (checked)

### GetCommandInt (544)
- **Handler:** 0x0066EC70 (not shared). It calls `CDataArrays::GetInteger` (0x00A5A220).
- **Does:** pops the command, then nIndex. It pushes `params->ints[nIndex]` and frees the copy. (checked)
- **Edge cases:** a null command gives 0. An index out of range, or negative (compared unsigned), gives 0. (checked)
  - The handler does not check `params` for null. The only null-params command found is a `CommandStartConversation` built with no valid OBJECT_SELF (below), and it would fault. (checked that there is no check; the fault itself is inferred)

### SetCommandInt (545)
- **Handler:** 0x0066ED10 (not shared). It calls `CDataArrays::SetInteger` (0x00A5B710).
- **Does:** pops the command, the value, then nIndex. It calls `SetInteger(nIndex, value)` **on the popped copy**, then frees the copy and pushes nothing (void). (checked)
- **Result: it has no effect.** The script's own variable, and the queued command it may stand for, are other copies. No game script calls it. (checked; the scripts were grepped)
- **Edge cases:** a null command is skipped. Returns 0. (checked)

### GetCommandFloat (546)
- **Handler:** 0x0066EDB0 (not shared). It calls `CDataArrays::GetFloat` (0x00A5A250).
- **Does:** pops the command, then nIndex. It pushes `params->floats[nIndex]` and frees the copy. (checked)
- **Edge cases:** a null command or an index out of range gives 0.0. A command with no params faults, as for `GetCommandInt` (inferred). (checked apart from that)

### SetCommandFloat (547)
- **Handler:** 0x0066EE60 (not shared). It calls `CDataArrays::SetFloat` (0x00A5B7D0).
- **Does:** pops the command, the float, then nIndex. It sets `floats[nIndex]` on the copy and **pushes that copy back** as the result, then frees its own. So unlike `SetCommandInt` it works, through the return value: `wrappers_h.nss` does `cCommand = SetCommandFloat(cCommand, fTimeout, 5)`. (checked)
- **Edge cases:** a null command pushes a null command. An index at or past the size grows the float array to index+1. A negative index makes the resize argument 0 (inferred: the floats are cleared), and the write goes to a dummy global. (checked up to the resize call 0x00534B10, whose meaning is inferred)

### CommandMoveToLocation (559): bDeactivateAtEnd

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm` and byte scans of `.text`, for DAOParagonEngine. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not.

#### The constructor (0x0066F080)
- Pops the location, then bRunToLocation, then **bDeactivateAtEnd only if paramCount > 2** (`cmp [ebp+0xC], 2`, 0x0066F104); otherwise it stays 0. (checked)
- Builds a command of **type 20** (`push 0x14; call Command::SetType`, 0x0066F1D7) with a `CDataArrays`: flag 1 = bRun, **flag 10 = bDeactivateAtEnd** (`SetFlag` 0x00A5B890 with 1 and 0xA), vector 0 = the location's position, vector 1 = its orientation (+0x10..+0x18). No ints. (checked)
- So the flag is not a field of the command record: it is flag 10 of its parameters. The run-time state is the creature's byte **+0x421**, below.

#### The start (table +0x54 entry 20 = 0x00745630; the tables are filled at 0x00747480)
- The object's slot +0x10 (cast slot 4, `AsCCCreature`): a **non-creature gets no sub-action and completes with success; flag 10 is never read for it**. (checked)
- For a creature it adds, in order: sub-action **36** (wait, duration = params int 0, which this constructor never sets, so 0), **14** (the move, with a copy of the params and int 2 = the movement mode from 0x00745250), **42** (turn to vector 1, the location's orientation). (checked)
- Then **if flag 10 is set: creature byte +0x421 = 1** (0x00745770–0x00745780). (checked)
- +0x421 is the "deactivation pending" byte of `dev/PARTY.md` (SetObjectActive with an animation sets it too). The active test 0x00699FA0 (called on obj+0x70: `[+0x255]` = obj+0x2C5 set and `[+0x3B1]` = obj+0x421 clear) therefore answers **inactive from the moment the move starts**: `GetObjectActive` is FALSE and the tag searches that filter on activity (`dev/PARTY.md`, the filter 0x0067FEC0) treat the walker as inactive during the walk. (checked for the write and the test; the script-visible consequence inferred)

#### The end (table +0x58 entry 20 = 0x00746D60; MoveToMultiLocations' end 0x00746DC0 chains to it)
End handlers are `(object, command)` on the commands system, which holds the result at +0x50 and an "immediate" flag at +0x4C, written by slot 3 (0x00742190, `__thiscall (object, command, result, immediate)`). (checked)
1. If the object is a creature and the result is not 1: stop it (0x006A2A20). (checked)
2. **Read the byte +0x421 into bl** (0x00746D87) — before anything else changes it. (checked)
3. The shared end 0x00746900 (the shared facts above): clears the current command slot, stores the previous command, and **sends COMMAND_COMPLETE** (0x00743DD0) if the command's +8 is set. (checked)
4. **If bl was set: main-vtable slot +0x140(0)**, `SetActive(false)` (CCCreature 0x006B5D90). (checked)

So:
- **Path:** slot +0x140(0) directly, not `SetObjectActive`'s animated path and no delay. `SetActive(false)` (CCObject 0x00698CD0; `dev/PARTY.md`): clears +0x2C5 and +0x421, leaves the world, and calls `ClearAllCommands(1, 1)`, which **clears the rest of the queue**. A creature in the active party (`IsFollower`, 0x00631830) is **not** deactivated: CCCreature's SetActive(false) only clears +0x421 for it (0x006B5D9C–0x006B5DC6). (checked; the effects of 0x00698CD0 are PARTY.md's reading)
- **Not only on success.** +0x421 is read whatever the result: arrival (1), a sub-action failure (path failed, any value other than 0/1 from sub-action 14), or a clear (-2 from `RemoveCommand`, or any `ClearCurrentCommand`) all deactivate. A failure also stops the creature first (step 1). (checked)
- **Order against COMMAND_COMPLETE:** the event is built before the deactivation; whether it is *handled* before depends on the "immediate" argument of `ClearCurrentCommand` (0x0073C730, `__thiscall (result, immediate, …)`, forwarded through 0x0064A510 to slot 3):
  - **Arrival and sub-action failure:** `StepCommands` calls `ClearCurrentCommand(result, 1, 1, 0)` (0x0073D36F and 0x0073D42D), so immediate = 1 and 0x00743DD0 runs the event **at once on the object's event script** (`RunEventScript` 0x00698960), inside the end, **before** SetActive(false). The handler sees the creature still in its area, with +0x421 = 1 (so already "inactive" to `GetObjectActive`), and any command it queues is then cleared by the deactivation's `ClearAllCommands`. (checked)
  - **A clear** (`RemoveCommand` calls `ClearCurrentCommand(-2, 0, 1, 0)`): immediate = 0, so the event is **queued** with `AIMaster::AddEventDeltaTime(0, 0, self, self, 10, ev)` (0x00743F2C) and the deactivation happens **before** it is delivered, on a later AI update, to an inactive creature. Whether delivery to an inactive creature runs its script: not read. (checked apart from that)
  - The event: type 0x24 at +0x20, creator = the mover; object 0 the mover, object 1 the command's object 0 (-1 here); int 0 the command type (20), int 1 the result. (checked)
- **A stale +0x421 also triggers it:** a MoveToLocation (or MoveToMultiLocations) without the flag, ending while +0x421 is 1 from an animated `SetObjectActive(FALSE)` still pending, deactivates the creature at its end through the same slot. (checked from the code; not seen)

#### What `utility_h`'s UT_ExitDestroy expects (read in `_Core Includes/utility_h.nss` 1120–1146)
- It finds the exit waypoint (`GENERIC_EXIT` by default) near oTarg, clears `RUBBER_HOME_ENABLED`, `SetObjectInteractive(oTarg, FALSE)`, optionally queues `CommandWait(random 0–1.5 s)`, then `WR_AddCommand(oTarg, CommandMoveToLocation(lExit, bRun, TRUE), FALSE, TRUE)`: queued at the back, **static**. (read)
- It builds an `EVENT_TYPE_SET_OBJECT_ACTIVE` (1014) event with int 0 FALSE and **never sends it** (dead code), and **never calls DestroyObject**: despite the name, the creature is only walked out and **deactivated by the engine at the move's end**, through flag 10. Being static, soft clears keep the command. It expects deactivation on arrival; the engine also deactivates on a failed path or a clear, and not at all for a creature in the active party or a non-creature. (checked against the engine paths above)

Not read: whether an event queued to an inactive creature (the cleared move's COMMAND_COMPLETE) is delivered and runs its script; what sub-action 14 returns on a failed path (any non-0/1 value deactivates all the same); `ClearAllCommands`' treatment of the queued commands it removes (events or silent).

### CommandMoveToMultiLocations (644)
- **Handler:** 0x00673280 (not shared). It calls `StackPopArray`, `ScriptArray::Size` and `CDataArrays::SetVector/SetInteger/SetFlag`.
- **Does:** pops the array, bRun, nStartingWP, then bLoop. It builds type **22** (script.ldf's RECOVER number) with these parameters (checked):
  - vector 5+i: location i's position only (orientation and area are dropped);
  - int 4: the count;
  - int 5 and int 6: nStartingWP, clamped to count-1 when it is ≥ count (there is no lower clamp);
  - flag 1: bRun, and flag 15: bLoop.
- **Start** (0x007457C0): for a creature only. It copies the parameters and sets int 2 to a movement mode (0x00745250, not read), then adds sub-action **52** (0x007A9BD0, not read). End 0x00746DC0 → 0x00746D60, MoveToLocation's end. (checked)
- **Edge cases:** an empty array gives count 0 and start -1. A non-creature gets no sub-action and completes with success. A pool missing at build time gives a null command. (checked)

### CommandMoveToObject (560)
- **Handler:** 0x0066F260 (not shared).
- **Does:** pops oTarget, bRun, fMinRange, bUseOriginalPosition, then fMaxRange. It builds type **21** (checked):
  - object 0: the target;
  - flag 1: bRun, and flag 5: bUseOriginalPosition;
  - float 0: max(fMaxRange, fMinRange), and float 1: fMinRange.
- **Start** (0x00746F00): for a creature only. It logs, copies the parameters, sets int 2 to the movement mode (0x00745250), then chooses the approach (checked):
  - **target a placeable** (its vtable slot 9 non-null): sub-action **4**, `ApproachMelee`, the placeable's use spot (`dev/PLACEABLES.md`);
  - **any other object, creatures included:** sub-action **15**, `ApproachRanged`, which reads float 0 as the max, float 1 as the min and float 4.
- **Results:** -3, -7 and -5 come from the approaches (0x007AA58E: invalid data; 0x007AA5DC: target destroyed; 0x007AA654: no space). End 0x00746DD0 stops the creature (0x006A2A20) unless the result is 1, then runs the shared end. (checked)
- **Edge cases:** a target id with no object makes the start return 0 with no sub-action, so the command **completes with success** (it also leaks the parameter copy). A non-creature mover also gets success, although script.ldf says it fails. (checked)

### CommandMoveAwayFromObject (561)
- **Handler:** 0x0066F430 (not shared).
- **Does:** pops oTarget, fAwayDistance, then bRunToLocation. It builds type **21**, the MoveToObject type, so it runs exactly as `CommandMoveToObject` does (checked):
  - object 0: the target, and flag 1: bRun;
  - float 0: 0.5 (constant 0x00AE7104), and float 1: 0;
  - float 4: fAwayDistance;
  - flag 5 (use original position): 1.
- **Edge cases:** a placeable target goes to the melee/use spot, which ignores float 4. Only a non-placeable target (sub-action 15) reads float 4 (inferred: that is where "away" is done; 0x007AA200 reads float 4 twice). `GetCommandType` reports 21. (checked apart from the noted part)

### CommandFly (811)
- **Handler:** 0x0066F5C0 (not shared).
- **Does:** pops the location, and **a second int if paramCount > 1** (undocumented; script.ldf declares one parameter, so scripts always give 0). It builds type **42** and sets **static (+0xC) = 1**, the only one here that does (checked):
  - vector 0: the location's position, and vector 1: its orientation (+0x10..+0x18);
  - flag 0: that extra int ≠ 0.
- **Start** (0x00742C30): for a creature, sub-action **47** (0x007A2B60) with a copy of the parameters. That sub-action answers 1 (done) when the creature is within 1e-6 of the position in x and y and its facing is within 1e-6 of vector 1's angle. Otherwise it drives fly animations (not read further). End 0x00747330 calls 0x006A0E10 on the creature (not read), then the shared end. (checked)
- **Edge cases:** a static command is kept by soft clears, and `RemoveCommand` will not clear it while current. A non-creature completes with success. There is no pool null check in the constructor. (checked)

### CommandTurn (327)
- **Handler:** 0x0066E9D0 (not shared). It calls `Vector::FromAngle` (0x004EAE30).
- **Does:** pops fFacingDirection (in degrees). It builds type **41** with vector 0 = (-sin θ, -cos θ, 0), from `FromAngle`'s (sin(-θ), cos θ) with y multiplied by -1. It sets static = 0 explicitly. (checked)
- **Start** (0x00742200): sub-action **42** (0x007A6BA0) with a copy of the parameters, for any object. Sub-action 42 calls `CSSubActionHandler::TurnToward` toward vector 0. It answers -1 when the object is null or dead or the command has no parameters. (checked)
- **End** (0x00746E10): for a creature, `MovementController::SetDesiredFacing(0,0,0)` and 0x00A63800(0), which cancel any turn left (inferred). Then the shared end. (checked)
- **Edge cases:** the angle is not normalised. (checked)

### CommandEquipItem (562)
- **Handler:** 0x0066F8B0 (shared with CommandUnequipItem 563; the case is `cmp [esp+0x2C], 0x232` at 0x0066F96B).
- **Does:** pops oItem, then nEquipSlot and nWeaponSet (`ScriptCommand::PopEquipSlotAndWeaponSet` 0x00651150 always pops both). It builds type **29** with object 0 = the item, int 0 = the slot and int 1 = the set. (checked)
- **Start** (0x007421D0): `mov eax,1; ret 8`. **It adds no sub-action, so the command completes with success on the next step and equips nothing.** The end is the shared one (0x00746E70 → 0x00746900). No native code acts on type 29 when it starts. (checked)
- **Evidence beside it:** `genpt_zevran_main.nss` has `CommandEquipItem` commented out and calls `EquipItem` (392) instead. (checked)
- **Edge cases:** the item is not checked. (checked)

### CommandUnequipItem (563)
- **Handler:** 0x0066F8B0 (shared with CommandEquipItem; any id other than 562 takes this path).
- **Does:** pops oItem and builds type **30** with object 0 = the item. (checked)
- **Start:** the same do-nothing 0x007421D0, so it **completes with success and unequips nothing**. The end is the shared one. (checked)
- **Edge cases:** as for `CommandEquipItem`. (checked)

### CommandSheatheWeapons (158)
- **Handler:** 0x0066FB30 (shared with CommandUnsheatheWeapons; the case is `cmp [esp+0x28], 0x9E` at 0x0066FB98).
- **Does:** pops nothing. It builds type **39** (Sheathing) with int 0 = 0. (checked)
- **Start** (0x007429F0) (checked):
  1. Sub-action **36** (0x007A20C0), a wait whose duration is the command's **int 1** (0 from scripts; inferred: the engine's own sheathing commands set a delay there).
  2. Sub-action **48** (0x007A26E0) with a copy of the parameters.
- **Sub-action 48** (checked):
  - int 0 = 0 sheathes, 1 draws, and any other value toggles.
  - It does nothing if the weapons are already in that state (0x007C34C0 on +0xC60).
  - With the animation (101 to sheathe, or 178 when `[+0xC60]` is set; `CCCreature::HasAnimation`), it queues sub-action 37 to play it and sub-action 5 to finish the swap.
  - Without it, it switches the state at once (0x007C33D0).
- **Edge cases:** a creature only (sub-action 48 returns early otherwise). The end is the shared one (table entry 39 left as the default). (checked)

### CommandUnsheatheWeapons (159)
- **Handler:** 0x0066FB30 (shared with CommandSheatheWeapons; any id other than 158).
- **Does:** builds type **39** with int 0 = 1, the draw. (checked)
- **Start:** as for Sheathe. Sub-action 48 draws with animation 100, or 177 when `[+0xC60]` is set. It is done at once if the weapons are already drawn. (checked)
- **Edge cases:** as for Sheathe. (checked)

### CommandSwitchWeaponSet (162)
- **Handler:** 0x0066FA20 (not shared).
- **Does:** pops nWeaponSet and builds type **40** with int 0 = the set. There is no pool null check. (checked)
- **Start** (0x00743570): for a creature only. It adds sub-action **22** (0x007A1ED0 → `CCCreature::ForScript_SwitchWeaponSet` 0x006A8FA0). If the weapons are drawn (0x007C34C0 = 1), it also adds sub-action **48** with int 0 = 1, drawing the new set. (checked)
- **ForScript_SwitchWeaponSet:** -1 (INVALID_WEAPON_SET) picks (active+1) mod the set count. Any set ≥ the count, compared unsigned, does nothing. Otherwise it sets the active set (0x007977D0) and refreshes (0x006951A0). The sub-action always answers 1. (checked)
- **Edge cases:** a non-creature completes with success and does nothing. The end is the shared one. (checked)

### CommandStartConversation (579)
- **Handler:** 0x0066E1F0 (not shared).
- **Does:** pops oTarget, and rDialogFile when paramCount ≥ 2 (otherwise ""). (checked)
  - **Only if the implementer's +4 is 1** does it fill the command: type **18**, object 0 = the target, string 0 = the dialog name. Otherwise the command it pushes has no type (0) and **no parameter block**. (checked)
  - The VM writes +4 and +8 of the implementer per run level (0x00A31F6C). Inferred from NWN's `m_bValidObjectRunScript`/`m_oidObjectRunScript`: +4 is "OBJECT_SELF valid".
- **Start** (0x007452A0), for a creature speaker (checked):
  - **Target's +0x3CC (InteractRadius) = 0:** sub-action 15 with float 0 = max(3.0, the speaker's `PerSpace` + 1.0) and flag 1 = run. A creature is the usual case.
  - **Otherwise:** it computes a spot in front of the target along its facing. If not already there, it adds sub-action 14 to walk to it (float 0 = `PerSpace`) and sub-action 40 to turn toward the target.
  - Then sub-action **12** (0x007A3700).
- **Sub-action 12** (checked):
  - It answers -1 if the target is gone or dead.
  - On its first tick it sets flag 0 and waits.
  - Next it builds a `CScriptEvent` from the parameters: type 18, EVENT_TYPE_DIALOGUE, with creator = the speaker. It **runs it at once on the target's event script** (`RunEventScript` 0x00698960) and answers 1.
  - `rules_core.nss` handles it with `UT_Talk(OBJECT_SELF, creator, GetEventResource(ev,0))`.
- **Edge cases:** **an invalid target faults at start**: `fld [ecx+0x3CC]` at 0x006953D0 runs on the null result of `GetObjectById` (0x00745355), with no check. A non-creature speaker completes with success. (checked)

### InteractWithObject (587)
- **Handler:** 0x0065C110 (not shared). It calls `CClientExoApp::GetCreatureById` and `CCCreature::ForScript_InteractWithObject` (0x006A9390).
- **Does:** pops oCreature, oTarget, nInteractionId, nPose, nLoops, bPlayExit, then bSkipReposition. Both sides are called (checked):
  - If oCreature is a creature: `ForScript_InteractWithObject(target, initiator=1, id, pose, loops, playExit≠0, skip≠0)`.
  - Then, if oTarget is a creature, the same on the target, with the creature and initiator=0.
- **ForScript_InteractWithObject** (checked):
  - It stores id, pose, loops and playExit at +0xC74, +0xC78, +0xC7C and +0xC80.
  - If its current command is already type **37** (Interact), it stops there: the running interaction takes the new values with no new command (as script.ldf says).
  - Otherwise 0x006A3920 builds type 37 and **queues it at the front** (`QueueCommand(cmd, 1, -1)`): object 0 the other party, ints 0/1/2 id/pose/loops, flag 2 initiator, flag 4 playExit, flag 1 skipReposition.
- **Start** (0x00743690) (checked):
  - Unless flag 1 is set, it repositions first. For a placeable it uses table 123's row for the interaction (inferred: SyncPlaceableAnims), with sub-action 14 to the spot and 42 to its facing. Otherwise it uses sub-action 15 to the target and 39 to face it.
  - Then sub-action **53** (0x007A6EA0, not read).
  - End 0x007472E0: for a creature, 0x0069EBB0(1) and vtable slot 94 (inferred: back to idle), then the shared end.
- **Edge cases:** void, always returns 0. If oCreature is not a creature, **nothing happens on either side**, even if the target is one. A placeable target gets only the creature's side. (checked)

## Not read
- Sub-action handlers 52 (multi-location move, 0x007A9BD0), 53 (interaction, 0x007A6EA0), 47 (fly) beyond its done test, 5 (finishing a weapon draw), and the walk inside 4/15/14. So how MoveToMultiLocations advances its waypoints (ints 5 and 6) and loops, and exactly how float 4 makes MoveAwayFromObject move away, are not established.
- 0x00745250 (the movement mode put in int 2), 0x006A2A20 (the stop at MoveToObject's end), 0x006A0E10 (Fly's end) and 0x0069EBB0 with slot 94 (Interact's end).
- Which engine code sets a Sheathing command's int 1 (the delay before sub-action 48).
- Table 123's identity, and the placeable branch of Interact's spot in detail.
- None of the edge cases flagged as faults (RemoveCommand with a null command, StartConversation with an invalid target, Get/SetCommandFloat or Int on a command without parameters) was reproduced in play.
