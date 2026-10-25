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
  - Read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2 [C]:
    - **The head:** when the current command has no sub-actions left (the list's size, cmd record +0x14, 0x0073D360), it is ended with `ClearCurrentCommand(1, 1, 1, 0)` (0x0073C730: success; its COMMAND_COMPLETE is run at once, inside the end, below) (corrected 2026-10-24 by lib-agent-2), and the next command is started (0x0073C7D0). Then the current command is run (0x0073D3AC).
      - Added 2026-10-24 by lib-agent-2, from paragon-agent-2's reading [C]:
      - **The gate:** the whole of StepCommands is gated on the object's active byte +0x2C5 (0x0073D34D). An inactive object takes no step, so its finished command stays current.
      - **The empty queue:** after the head's clear, it returns without running anything when the queue is empty (+0x2D4, 0x0073D37C–0x0073D383). The count is read after the clear, so a command queued by the COMMAND_COMPLETE handler is started and run in the same step.
      - **Where a command starts:** `StartNextCommand` (0x0073C7D0) has one caller, 0x0073D38B. So a command is started only at a step's head, whenever it was queued.
      - **Who calls StepCommands:** it is the last call of each object's Update (slot 87):
        - `CCCreature::Update` 0x006BE4B0, at 0x006BE958;
        - CCPlaceable's 0x0074DA40 (ID 17888);
        - CCItem's 0x00752B40 (ID 17964);
        - CCTrigger's 0x00798DB0 (ID 19507, its subclasses' too);
        - CCAreaOfEffectObject's 0x007E4AF0 (ID 21061).
      - **COMMAND_COMPLETE** (0x00743DD0, from the shared end 0x00746900, only when the command's +8 is set, 0x007469EA):
        - With the end's immediate flag (+0x4C, ClearCurrentCommand's second argument, stored by `Commands::End` 0x00742190) at 1, the event is run at once on the owner's event script (`RunEventScript` 0x00698960, 0x00743EFA). Otherwise it is queued (`AIMaster::AddEventDeltaTime(0, 0, self, self, 10, ev)`, 0x00743F2C) for the next drain. (The rule is also at "Order against COMMAND_COMPLETE" below; skipped here: its text. The table is new.)
        - Ended by / ClearCurrentCommand / COMMAND_COMPLETE:
          - the step's head, success: (1, 1, 1, 0), 0x0073D377, at once;
          - the step's failure end: (answer, 1, 1, 0), 0x0073D436, at once;
          - ClearAllCommands: (−2, 0, x, y), 0x0073C9C6, queued;
          - RemoveCommand: (−2, 0, 1, 0), 0x0073CA0D, queued;
          - command 540, ClearCurrentCommand: (−2, 0, 1, 0), 0x0065BF8A, queued.
        - So a success and a failure are handled inside the owner's step, and a command its handler queues runs in that step. A clear's COMMAND_COMPLETE comes at the next drain.
      - **ClearCurrentCommand (a1 result, a2 immediate, a3, a4):**
        - A static command (+0xC == 1) is cleared only when a2 == 1 or a4 != 0.
        - Otherwise it is cleared when its sub-action list is empty (0x00697BF0's first element null), or a3 == 1, or a3 == 0 and the first sub-action's byte +4 is 0.
        - So a finished command is taken by any clear, a soft one too. A clear of a finished command ends it with −2, never with 1.
    - **After the run:** an answer of 1 or 0 does nothing more. Any other answer ends the command with that status.
    - **So a command whose last sub-action answers 1 ends at its next step,** after that frame's event drain. An event its last sub-action queued (an impact, a USE) is delivered while it is still current. If that event queues a clearing command, such as ExecuteEffect's 32, the command ends with -2 (COMMAND_FAILED_COMMAND_CLEARED) and not with 1. RunCurrentCommand has one caller (0x0073D3AC) and never clears the command itself. The other callers of 0x0073C730 are 0x0065BF8A (its context not read), 0x0073C9C6, 0x0073CA0D, 0x0073D377 and 0x0073D436, so only an explicit clear, from a handler or an event, ends a command sooner.
    - **The runner, `CCObject::RunCurrentCommand`** (0x0073CB60), walks the sub-action list with the frame's dt and a running total of the time used:
      - **The fields:** a sub-action's +0x18 is its start delay left, +0x14 its step's length and +0xC its time spent. 0x004F25A0 sets +0x18.
      - **The delay:** with a delay and a length, the time used becomes at least the delay, and the delay drops by dt, not below 0.
      - **The skip:** a sub-action is skipped while the time used has reached dt or its delay is not yet out.
      - **The run:** otherwise its time spent advances, the time used rises (at most to dt), and its handler runs (`[[app+0x1AC]+0x70]` slot 2).
      - **The answer:**
        - A command cleared under the handler (+0x2DC → +4) makes the answer -2.
        - A non-zero answer pops the sub-action (0x00745240, 0x0073C810).
        - On 1 the walk restarts at the list's head (0x0073CC84–0x0073CC97), not at the next element. Any other non-zero answer is returned.
        - On 0 the time used is set to dt (0x0073CC9D), so the later sub-actions are skipped this frame by the time test; their start delays are still counted down.
      - **It returns the last answer,** starting at 1. An empty list, or one whose remaining sub-actions were skipped for time, gives 1, so a returned 1 does not mean the list is empty.
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
- **Edge cases:** a null command pushes a null command. An index at or past the size grows the float array to index+1. A negative index makes the resize argument 0 (the floats are cleared, checked 2026-10-22), and the write goes to a dummy global.
  - [C] SetCommandFloat, a negative index (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169): `CDataArrays::SetFloat` (0x00A5B7D0) compares the index unsigned with the float list's size (list pointer at CDataArrays+8, begin +4, end +8) and, when the index is not below it, calls 0x00534B10(index+1), which is a `std::vector` resize(n) on a 4-byte element (0x00533920): growth appends zero-valued elements, a smaller n erases the tail. For index -1 the argument is 0, so the list is emptied. It then re-tests the index, finds it out of range and writes the value to the global dummy at 0x00C31858. So a negative index clears the floats and the value is lost, the resize's meaning is read (it was inferred). For -2 and below the argument is 0xFFFFFFFF or less (a request for ~4G elements): the growth path (0x00533920 into 0x005336C0) was not read, so "an allocation failure" stays [I].

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

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not followed to the end. The shared facts of `dev/script_commands/commands.md` apply: every handler is `__thiscall (commandId, paramCount)`, returns 0, -639 on a failed pop and -638 on a failed push; a command on the stack is a copy (`CDataArrays` at command+4: `SetInteger` 0xA5B710, `SetObject` 0xA5B950, `SetVector` 0xA5A9B0, `SetFloat` 0xA5B7D0, `SetString` 0xA5BA10, `SetFlag` 0xA5B890); a start handler that adds no sub-action makes its command complete with success. Start and end handlers are at table offset 4 x type (filled at 0x00747480). The constructors below all leave the command null (type 0, no parameters) when the commands system (`app+0x1AC -> +0x6C`) is absent or the pool gives none, and still push it (checked).

### AddCommand (533)
- **Handler:** 0x0066E0E0 (not shared). Pops the object, the command (structure 2), bAddToFront, bStatic and, only when paramCount > 4, nOverrideAddBehavior (default -1 in the handler); calls `GetObjectById` 0x005D5BE0 and `CCObject::QueueCommand` 0x0073CD10.
- **Described in:** `dev/TURN_BASED.md` ("Every queued action seems to pass through one function"; the `WR_AddCommand` paragraph) and `dev/COMBAT.md` ("Queuing it clears the queue").
- **Does:** queues the popped copy on the object, taking ownership of it, and pushes 1 on success or 0 when the object is gone. The command is not checked in any way: its type, and whether the object is a creature, are left to the start handler.
- **Edge cases:**
  - **The handler overwrites the command's static word (+0xC) with bStatic** before queuing, so a command that its constructor made static (`CommandDeathBlow` 246 sets +0xC = 1, as does `CommandFly` 811) is queued non-static unless the script passes bStatic = TRUE. (checked, 0x0066E19B)
  - No object for the id: the command copy goes back to the pool (0x0066DE30) and 0 is pushed; the script goes on. A null command with a valid object makes `QueueCommand` return 0 and queue nothing. (checked)
  - `QueueCommand`: with override -1 it reads the override from `commands.gda` (table 94 through `g_gdaTables`, row = command type, column hash 0xED36668A, or 0x7569547A when the object's current command has the same type as the new one; the lookup is skipped for an explicit override). Override 1 = `ClearAllCommands(soft)`, 2 = hard, 3 = hard only if the new command's object 0 differs from the creature's attack target (combat data +4), 0 = none. Then bAddToFront == 1 exactly puts it at the head of the queue (+0x2CC, 0x0073CCD0), anything else at the tail (0x00698C30). (checked)
  - script.ldf says adjacent duplicate commands are deleted: neither insert nor the code between does so (checked); anything of the kind would be at command start (not read). A player-issued command (+0x10) raises an event (type 0x5E) before queuing, and a type-25 command on a creature calls 0x0071DEE0 after (neither followed). (inferred)

### SetCommandResult (716)
- **Handler:** 0x0065C0B0 (not shared). Pops the object and nResult, calls `GetObjectById` 0x005D5BE0.
- **Described in:** `dev/COMBAT.md` (the attack, "answers with `SetCommandResult(owner, COMMAND_RESULT_SUCCESS)`"); `dev/ABILITIES.md` (the COMMAND_PENDING answer).
- **Does:** if the id names an object, writes nResult at `object+0x3D4`. Void; returns 0. (checked)
- **State:** `CCObject+0x3D4`.
- **Edge cases:** no object: nothing. There is no type check, no range check on nResult, and no check that a command is running, so the word is simply left there. The note in COMBAT.md that the command layer reads it is still inferred; this handler never touches a command. (checked)

### ClearAllCommands (538)
- **Handler:** 0x0065BDE0 (not shared). Pops the object, then nHardClear (always popped, no count test; the handler's own default is 1). Calls `GetObjectById` and `CCObject::ClearAllCommands` 0x0073C930 as `(hard = (nHardClear == 1), 0)`.
- **Described in:** `dev/TURN_BASED.md` ("A soft ClearAllCommands mid-swing: the blow lands") for what soft and hard do; `dev/COMBAT.md` for the queue override.
- **Does:** clears the queue and the current command; pushes 1 when the object was found, else 0 (script.ldf's "TRUE on success"). Only the value 1 is hard; 0, 2, -1 or any other value is soft. (checked)
- **Edge cases:** no object: nothing is cleared and 0 is pushed. The push is the only result, so a script cannot tell a clear that left a static or uninterruptible command from a full one. (checked)

### ClearAmbientDialogs (626)
- **Handler:** 0x006510F0 (not shared). Pops one object; calls the ambient dialog manager's getter 0x006FEFA0 as `(id, id == OBJECT_INVALID, 1)` and its `Clear` 0x006FF8E0.
- **Described in:** `dev/PLOTS.md` ("A second bark").
- **Does:** removes the ambient entries (barks) for that owner, or all of them for OBJECT_INVALID, and always pushes 1. (checked)
- **Edge cases:** the id is not looked up, so a stale or non-existent id just clears nothing and still gives 1; the handler never pushes 0 although script.ldf calls 0 failure. The getter creates the manager (0x44 bytes, global 0x00C35CF8) on first use, so the first call is also its construction. (checked)

### GetCommandQueueSize (724)
- **Handler:** 0x0066D1C0 (not shared). Pops the object, `GetObjectById`, reads the queue's count through 0x007B51E0 (`[queue+8]`, the queue at `object+0x2CC`).
- **Described in:** `dev/TURN_BASED.md` (AI_DetermineCombatRound returns at once if the creature has a current command or anything queued).
- **Does:** pushes the number of commands waiting, not counting the current one (which is at +0x2DC, outside the list), matching script.ldf. (checked)
- **Edge cases:** no object: 0. No type check (any object has the queue). Static commands count like any other. A push failure gives -638. (checked)

### GetCurrentCommand (539)
- **Handler:** 0x0065BEF0 (not shared). `GetObjectById`, then `CCObject::GetCurrentCommand` 0x0073C5F0 (`[[object+0x2DC]]`); pushed as structure 2.
- **Described in:** `dev/TURN_BASED.md` (the call table, and `AI_DetermineCombatRound`'s checks); `commands.md` (the record at +0x2DC).
- **Does:** pushes a copy of the command that is running now; the copy is independent, so changing it does nothing to the running one. (checked)
- **Edge cases:** no object, or nothing running: a null pointer is pushed, which scripts read as a command of type 0 (`GetCommandType` 0, `COMMAND_TYPE_INVALID`) with no error. script.ldf says to test with `GetCommandType`, which is right. Any object type works. (checked)

### ClearCurrentCommand (540)
- **Handler:** 0x0065BF50 (not shared). Pops the object, `GetObjectById`, calls `CCObject::ClearCurrentCommand` 0x0073C730 as `(-2, 0, 1, 0)`.
- **Described in:** `dev/TURN_BASED.md` (the soft-clear paragraph, for the `ClearCurrentCommand(-2, 0, hard, 0)` form) and `dev/COMBAT.md` ("the current command, if not static, is cleared even mid-sub-action").
- **Does:** ends the current command with result -2 (COMMAND_FAILED_COMMAND_CLEARED), which runs its end handler and sends COMMAND_COMPLETE(-2). The hard flag is fixed at 1 here, so a sub-action that cannot be interrupted does not hold it. (checked)
- **Edge cases:** void; returns 0. No object: nothing. A static current command is not cleared (`commands.md`, `RemoveCommand`). The queue is left alone, so the next queued command starts on the next step. (checked)

### CommandAttack (566)
- **Handler:** 0x0066E710 (not shared). Pops oTarget, then nForcedResult only when paramCount > 1 (the handler's own default 0; script.ldf's `COMBAT_RESULT_INVALID` is 0 as well).
- **Described in:** `dev/COMBAT.md` ("One command is one swing"; the COMMAND_PENDING paragraphs).
- **Does:** builds type 1 (Attack): object 0 = the target, int 2 = nForcedResult. It is not static. (checked)
- **Edge cases:** the target is not looked up here. **Start** (0x00742FD0): the mover must be a creature and the target id must name an object; otherwise it returns before adding any sub-action, so the command ends with success next step. Past those checks, and before anything is queued, it writes the target's id at `mover+0xC60 -> +4` (the creature's attack target, what `GetAttackTarget` reads: inferred) and reads float 5 (the approach timeout, `WR_AddCommand` sets it) from the parameters. (checked)

### CommandDeathBlow (246)
- **Handler:** 0x0066E860 (not shared). Pops an object (-1 until popped), nDeathType (default 0), and, when paramCount > 2, **a second object that replaces the first** (undocumented; scripts, with two parameters, never send it).
- **Described in:** `dev/COMBAT.md` ("For a deathblow, ... the deathblow command is put at the front of the queue").
- **Does:** builds type 38 (Deathblow): int 0 = nDeathType, object 0 = the first parameter (script.ldf calls it the attacker, not the target), and **sets static (+0xC) = 1**. (checked)
- **Edge cases:** the start (0x00745E10) does nothing but succeed (no sub-action) unless the executor is a creature that passes 0x006A25A0; if the object is the executor itself a deathblow attack animation plays, else the damage animation. With nDeathType 0 it looks up the animation by the attacker's weapon in the data tables (hashes 0xDC9C928D and 0x8A442AFE; not followed), nonzero goes another way (0x00746180, not read). Being static, `AddCommand` clears it (see AddCommand) unless the script passes bStatic. End handler 0x00747360 not read. (checked for the constructor; the start inferred)

### CommandWait (578)
- **Handler:** 0x0066FDB0 (not shared). Pops fSeconds.
- **Described in:** `dev/ANIMATION.md` (waits) and `dev/TURN_BASED.md` (the AI queues a wait when idle).
- **Does:** builds type 13 (Wait) with int 0 = the seconds as milliseconds: negative seconds become 0, then `ftol(seconds x 1000.0)` (constant 0x00AEBC00). Not static. This matches script.ldf's "a negative value gives a wait time of zero". (checked)
- **Start** (0x00742950): sub-action 36 (0x007A20C0) with a copy of the parameters, for any object; it counts the milliseconds down (0x007A1AE0, not read) and answers 1 when none are left, or -1 when the mover is null or 0x00695300 gives 1. A wait of 0 ends at once. A null parameter block makes the start return 0 with no sub-action. (checked)
- **Edge cases:** NaN and huge values go through `ftol` unchecked (inferred).

### CommandJumpToObject (580)
- **Handler:** 0x0066FEF0 (not shared). Pops oTarget.
- **Described in:** `dev/LOAD_EVENTS.md` (facing: a random angle at the two personal spaces' distance for any creature but the controlled one).
- **Does:** builds type 9 with object 0 = the target; not static. **Start** (0x007426D0): sub-action 32 (0x007A3C80). (checked)
- **Edge cases:** an invalid target is not caught at construction. The sub-action answers -3 (invalid data) if the mover or the target is missing, the mover is dead (slot 0x1D0 = 1), or the target is the mover itself; the command then fails with -3 and COMMAND_COMPLETE(-3). A target in another area goes by a transition message (0x005D5E90) instead of an in-place move; in the same area the mover's slot 0xF8 places it, the spot depending on the target's kind (a creature, a placeable (type 10) and one more kind are special-cased, not read). (checked up to the branches)

### CommandJumpToLocation (581)
- **Handler:** 0x00670010 (not shared). Pops the location (structure 1).
- **Described in:** `dev/LOAD_EVENTS.md` and `dev/ANIMATION.md` (a jump to within 0.01 m of the controlled creature).
- **Does:** builds type 10: vector 0 = position (+0..+8), vector 1 = orientation (+0x10..+0x18), flag 0 = 1 (meaning not followed; the sub-action does not appear to read it), object 0 = the location's area id (+0x20). Not static. The popped location is freed. **Start** (0x00742770): sub-action 33 (0x007A4100). (checked)
- **Edge cases:** the sub-action looks the area up by id (0x005D5960) and ends with -3 if the mover is missing or dead, or the area does not exist; a location with no area (id -1) therefore always fails. The position is not checked against the walk grid in the part read. (checked up to the first branches)

### CommandUseAbility (582)
- **Handler:** 0x006701B0 (not shared). Pops, always all five and in this order: nAbilityId, oTarget (default -1), vTarget (3 floats), fConjureTime (default -1.0, constant 0x00AE2E00) and sAbilitySourceItemTag; no count test.
- **Described in:** `dev/ABILITIES.md` ("Queuing them: the UseAbility step", 0x00743F50).
- **Does:** builds type 25: int 0 = ability, object 0 = target, vector 0 = vTarget, float 0 = conjure time, **object 1 = -1** (the item; the start finds it by the string), string 0 = the item tag. Not static. (checked)
- **Edge cases:** the construction checks nothing: an unknown ability id, a target of the wrong type or a negative conjure time is accepted, and the start (0x00743F50) then adds no sub-action, so the command ends with success without casting (ABILITIES.md; the rule from commands.md). Both oTarget and vTarget are kept in the record; which one is used is for the step (ABILITIES.md: a ground target takes the point). (checked for the constructor)

### CommandUseObject (586)
- **Handler:** 0x0066F770 (not shared). Pops oTarget and nAction.
- **Described in:** `dev/PLACEABLES.md` ("From a script. `CommandUseObject` builds the same command and checks nothing").
- **Does:** builds type 31 (UseObject): object 0 = the target, int 0 = the action; not static. (checked)
- **Edge cases:** nAction is not range-checked (script.ldf: PLACEABLE_ACTION_*; the start handler 0x00745920 passes it on). **Start:** it needs a creature to run it and the target to exist; it then builds a sub-action block with object 0 = the target, flags 1 and 2 set and its float 0 and 1 zero (the approach and the facing, then the use, as script.ldf lists: not followed past the first lines). Without a creature or a target it returns early, so the command succeeds with nothing done (inferred from the early returns). (checked for the constructor)

## Not read
- The end handlers: 0x00747360 (Deathblow), 0x00746B30 (Attack), 0x00746BA0 (UseAbility), the UseObject and Wait ends (shared end 0x00746900 only).
- The remaining half of `CommandDeathBlow`'s start (0x00745E10 past 0x00746180) and the UseObject start past the first sub-action block.
- What `QueueCommand` does with 0x0071DEE0 is answered (read 2026-10-23 by lib-agent-2): for a UseAbility command (type 25) it calls 0x0071DEE0(0, the ability, 0) at 0x0073CEF3, which switches off the modes in that ability's Condition_Group (ABILITIES.md; the event 0x5E is read: PARTY.md, "EVENT_TYPE_PLAYER_COMMAND_ADDED"); any de-duplication of queued commands.
- 0x007A1AE0 (the wait's counter), and the kind-by-kind spots of sub-action 32.
