# Script commands, group "events": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm` and scratch call/displacement scans, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts, checked unless marked:
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`, on `CVirtualMachineCommands`; none is shared. It returns 0, -639 (0xFFFFFD81) on a failed pop, -638 (0xFFFFFD82) on a failed push (`dev/script_commands/commands.md`). Pops: `StackPopEngineStructure` 0x00A2F650 (structure 0 = event, 1 = location), `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410, `StackPopString` 0x00A2F4C0, `StackPopResource` 0x00A2F5F0, `StackPopObject` 0x00A2F6D0. Pushes: `StackPushEngineStructure` 0x00A2D920, `StackPushInteger` 0x00A2D650, `StackPushFloat` 0x00A2D690, `StackPushString` 0x00A2D730, `StackPushResource` 0x00A2D890.
- **An event on the stack is a copy, like an effect.** Pop and push both copy through `CopyGameDefinedStructure` (0x00674AF0) → `CScriptEvent` copy constructor 0x0069CB10 into a new 0x50-byte object; every handler deletes its popped copy (vtable slot 0, `push 1`). So each `Set*` returns a changed copy and changes nothing already queued or being handled; `GetCurrentEvent` (108) pushes a copy of the object's current event (CCObject +0x2A8, area +0x3C4, module +0x390), so changing it does not touch the event in flight.
- **CScriptEvent (0x50 bytes; ctor 0x0069BAC0):** `CDataArrays` at +0 (ints +4, floats +8, flags +0xC, objects +0x10, strings +0x14, vectors +0x18; `src/dao/Structures.h`); +0x20 type (0); +0x24 creator (-1); +0x28 target (-1); +0x2C a `std::wstring`, the event's own script, its length at +0x40 (the "+0x40 set" of `dev/LOAD_EVENTS.md`'s `RunEventScript` is this string being non-empty); +0x48 a vector of resource names (`std::wstring`, 0x1C each); +0x4C another list (element type not read). **The copy constructor copies the parameters, type, creator, target and both lists, but not the own script (+0x2C): it is default-constructed** (0x0069CB66). So an event a script holds, signals or delays never carries its own script.
- **Null events reach handlers** (an array element past the end reads as a null structure, `dev/NCS.md`; a null copies to null). Only IsEventValid, SetEventCreator, SetEventType and GetEventInteger test for one; the rest call into the null object and fault (each marked below).
- **Delivery (`RunEventScript` 0x00698960, then 0x006985F0):** the script is chosen in this order (checked): `engineevents.gda` (table 114, `g_gdaTables` 0x00C1D090) `Script` column (hash 0xF333FE09) at the row whose ID is the event type, if non-empty; else the event's own script (+0x2C); else the object's (+0x26C). The shipped table names `rules_core` for 33 APPLY_EFFECT, 34 REMOVE_EFFECT, 35 COMMAND_PENDING, 36 COMMAND_COMPLETE and 65 ITEM_ONHIT, so these go to `rules_core` whatever the object's script (read with ghilana; the routing not seen in play). Then 0x006985F0: the gate 0x00698490 (the object's disabled set +0x28C, `dev/LOAD_EVENTS.md`) and an empty script name each delete the event and stop; otherwise it saves and sets +0x2A8 (current event), sets the target (+0x28) to the object if it is -1, runs the script at once (`CVirtualMachine::RunScript` 0x00A31E70 (script, self, 1, type)), calls 0x00707000(event) (not read), notifies the listeners (below), deletes the event and restores +0x2A8.

### IsEventValid (107)
- **Handler:** 0x006581B0.
- **Does:** pops an event; pushes 1 when it is non-null and its type (+0x20) is non-zero, else 0; frees the copy. (checked)
- **State:** none.
- **Edge cases:** a null event or type 0 (EVENT_TYPE_INVALID) gives 0. A type set to any non-zero number, even one no handler knows, is "valid". Nothing about the creator or target is checked. (checked)

### SetEventCreator (110)
- **Handler:** 0x00658310.
- **Does:** pops the event, then the object id; writes the id at +0x24 of the copy and pushes the copy (copied again by the push); frees its own. (checked)
- **State:** the returned event only.
- **Edge cases:** the id is not looked up: OBJECT_INVALID or a stale id is stored as given. A null event pushes a null event, no failure. (checked)

### SetEventType (112)
- **Handler:** 0x006583F0.
- **Does:** pops the event, then nType; writes nType at +0x20 and pushes the copy. (checked)
- **State:** the returned event only.
- **Edge cases:** no range check: 0, negative and custom numbers are stored. A null event pushes null. Setting 0 makes `IsEventValid` answer 0. (checked)

### GetEventInteger (113)
- **Handler:** 0x00650740. It calls `CDataArrays::GetInteger` (0x00A5A220).
- **Does:** pops the event, then nIndex; pushes `ints[nIndex]`. (checked)
- **Edge cases:** a null event gives **-1** (as script.ldf says); an index past the end or negative (compared unsigned) gives **0**, not script.ldf's -1. (checked)

### SetEventInteger (114)
- **Handler:** 0x006507C0. It calls `CDataArrays::SetInteger` (0x00A5B710).
- **Does:** pops the event, nIndex, nValue; `SetInteger(nIndex, nValue)` on the copy; pushes it. The list grows to nIndex+1, new slots 0. (checked)
- **Edge cases:** a null event is skipped and null pushed. A negative index, compared unsigned, resizes the list to nIndex+1 (for -1, to 0: the ints are emptied, inferred from the resize call 0x005AE960) and writes the value to a dummy global (0x00C2A8D4); -2 and below ask for a list of ~4G entries (inferred: an allocation failure). (checked apart from what is marked)

### GetEventFloat (115)
- **Handler:** 0x00650860. It calls `CDataArrays::GetFloat` (0x00A5A250).
- **Does:** pops the event, then nIndex; pushes `floats[nIndex]`. (checked)
- **Edge cases:** out of range or negative gives 0.0, not script.ldf's -1.0. **A null event is not tested**: GetFloat reads +8 of null and faults. (checked)

### SetEventFloat (116)
- **Handler:** 0x006508F0. It calls `CDataArrays::SetFloat` (0x00A5B7D0).
- **Does:** pops the event, nIndex, fValue; `SetFloat(nIndex, fValue)` on the copy; pushes it. (checked)
- **Edge cases:** **no null test** (faults, checked that there is none). Negative indices as for SetEventInteger (inferred: SetFloat has the same shape).

### SetEventObject (118)
- **Handler:** 0x00650A10. It calls `CDataArrays::SetObject` (0x00A5B950).
- **Does:** pops the event, nIndex, oValue; stores the id at nIndex of the copy's objects; pushes it. (checked)
- **Edge cases:** the id is not looked up (OBJECT_INVALID is stored). **No null test** (checked). `GetEventObject` (117, 0x00650990, not this group) gives OBJECT_INVALID out of range (`Structures.h`) and also does not test for null (checked).

### GetEventString (119)
- **Handler:** 0x00650AB0. It calls `CDataArrays::GetString` (0x00A5A2E0).
- **Does:** pops the event, then nIndex; pushes a copy of `strings[nIndex]`. (checked)
- **Edge cases:** out of range gives an empty string (`Structures.h`). **No null test**: GetString reads +0x14 of null (checked).

### SetEventString (120)
- **Handler:** 0x00650BC0. It calls `CDataArrays::SetString` (0x00A5BA10).
- **Does:** pops the event, nIndex, sValue; stores the string at nIndex; pushes the copy. (checked)
- **Edge cases:** **no null test** (checked). The list grows as needed (inferred from SetInteger's shape).

### GetEventResource (735)
- **Handler:** 0x00659090. It calls 0x0069C0F0 (the event's resource at an index, list +0x48) and the name check 0x004C55F0.
- **Does:** pops the event, then nIndex; takes the resource name at nIndex (an empty static one, 0x00C35368, when out of range). A non-empty name is **checked as a resource name** (0x004C55F0 with 0): 1 to 32 characters, each `A–Z a–z 0–9 _ - ( ) space` or `.`, with a `.` at index 1 or later and at most 3 characters after the last one. A name that fails is replaced by the global 0x00C2BE00 (inferred: the empty string, used in 425 places). Pushes it as a resource. (checked)
- **Edge cases:** so a resource without an extension, longer than 32, or with other characters reads back empty. **No null test**: 0x0069C0F0 reads +0x48 of null (checked). Index compared unsigned. (checked)

### SetEventResource (734)
- **Handler:** 0x006591D0. It calls 0x0069CF90.
- **Does:** pops the event, nIndex, rValue; makes the +0x48 list if there is none, grows it to nIndex+1 and assigns the name as given; pushes the copy. (checked)
- **Edge cases:** no name check on write (only `GetEventResource` checks). **No null test** (checked). Negative index: unsigned compare, resize to nIndex+1, then an element access at the index (not read; inferred unsafe).

### GetEventLocation (131)
- **Handler:** 0x00658470. It calls `CDataArrays::GetVector` (0x00A59F10), `GetObjectById` (0x005D5BE0), the location constructor 0x007ABD00.
- **Does:** pops the event, then nIndex; builds a location whose **position** is `vectors[nIndex]` (0,0,0 when out of range), **orientation (0,0,0)**, and **area the creator's area**: the event's creator (+0x24) looked up and its +0x254 (`Objects.h`'s area id). Pushes it. (checked)
- **State:** reads only.
- **Edge cases:** the location's area is never stored in the event: it is the creator's at the time of the call, and **0 (not OBJECT_INVALID) when the creator is invalid** (`xor edi, edi`, checked). The facing is lost. **No null test** (reads +0x18 of null; checked). A negative index gives the zero vector (GetVector compares unsigned). (checked)

### SetEventLocation (132)
- **Handler:** 0x006585F0. It calls `CDataArrays::SetVector` (0x00A5A9B0).
- **Does:** pops the event, nIndex, the location (structure 1); stores **only the location's position** as vector nIndex; pushes the copy and frees the location. (checked)
- **Edge cases:** the location's area and orientation are dropped (so `GetEventLocation` gives the creator's area and facing 0). **No null test for the event or the location** (checked). The slot is the event's vectors list (+0x18), shared with any vector parameter the engine put there. (checked)

### HandleEvent_String (133)
- **Handler:** 0x006588E0. It calls `CClientExoApp::GetGameObjectById` (0x005D5840) on OBJECT_SELF (`[this+8]`), then 0x006985F0 (CCObject), 0x0067BFB0 (area) or 0x005FE840 (module, with a third argument 1).
- **Does:** pops the event, then sName; hands the event to OBJECT_SELF **at once (inline)**, with sName as the script, or the object's own script when sName is empty: CCObject +0x26C, area +0x38C, module +0x1F8. Ownership of the popped copy passes to the callee. (checked; that the area and module paths run it at once is inferred)
- **State:** for a CCObject, 0x006985F0 as in the shared facts: the gate, +0x2A8 current event during the run, target set to self if -1, the listeners notified, the event deleted. (checked)
- **Edge cases:** it skips `RunEventScript`, so `engineevents.gda` is not consulted. The event's +0x2C is never used (a copy has none anyway). An empty sName on an object with no script, or an event type in the object's disabled set, runs nothing and deletes the event (checked). An OBJECT_SELF that is neither a CCObject (cast slot +0x18), an area nor the module, or none: the event is freed, nothing runs (checked). There is no `.ncs` stripping here: sName is the script name as given (checked: no find on `.`). script.ldf has no comment for it; `HandleEvent` (121, 0x006586D0) is the resource form.

### SetEventScript (122)
- **Handler:** 0x00658AA0.
- **Does:** pops the object, then rScriptName; cuts the name at its first `.` (`find('.')`, `substr(0, pos)`: `R"x.ncs"` becomes `x`); stores it as the event script: CCObject +0x26C with +0x288 = 1 (marked overridden, saved, `dev/LOAD_EVENTS.md`), area +0x38C, module +0x1F8. (checked)
- **State:** the object's script; future events use it at delivery, so events already queued run the new script (`dev/LOAD_EVENTS.md`). Note `engineevents.gda`'s Script still wins for 33–36 and 65 (shared facts).
- **Edge cases:** an invalid object or another kind: nothing (checked). An empty resource clears the script, so events are then deleted unrun (checked: 0x006985F0's empty-name test). No check that the script exists. (checked apart from that)

### EnablevEvent (123)
- **Handler:** 0x00658CC0. It calls 0x00698C00 (CCObject), 0x00681750 (area), 0x00601610 (module), each `(nEventType, bEnable)`.
- **Does:** pops the object, bEnable, nEventType. For a CCObject: bEnable 0 inserts the type into the disabled set +0x28C (0x00600E40), non-zero erases it (0x006010C0). Area and module keep their own sets (inferred from the same shape). (checked for CCObject)
- **State:** +0x28C, saved as `EnabledEvents` (`dev/LOAD_EVENTS.md`); the gate 0x00698490 reads it at every delivery, also for EVENT_TYPE_LISTENER (21) (below).
- **Edge cases:** invalid object: nothing. No range check on the type. **script.ldf says events do not fire for PCs until enabled: the engine has no such default**; the set starts empty, so everything fires for everyone (`dev/LOAD_EVENTS.md`). No shipped script calls it. (checked)

### RegisterEventListener (124)
- **Handler:** 0x00658DB0. It calls 0x0069B270 (CCObject), 0x00682930 (area), 0x00601D00 (module), each `(oListener, nEventType)` on oTarget.
- **Does:** pops oTarget, oListener, nEventType. **Ignores type 21 (LISTENER) and any type ≥ 105 (0x69)** (signed compare, so negative types are accepted). On a CCObject target it finds or makes the record for the type in the vector at +0x298 (records of 0x14 bytes, ctor 0x00706270, +0 the type, +4 a vector of listener ids; kept sorted by type) and **appends** the listener's id. (checked)
- **State:** the target's +0x298, read back by the object's load as `EventListeners` (0x0069AF82; the string is also used at 0x0068841A and 0x0060232D, inferred the area's and module's), so listeners persist in saves (save side inferred).
- **When they receive:** in 0x006985F0, right after the target's own script for an event of that type has run, synchronously, in registration order: for each listener id that still resolves, if **the target's** gate passes type 21, a new event of type 21 with creator = the target is made and run on the listener at once (CCObject: `RunEventScript`, so the listener's own gate and script; area 0x0067D6D0; module 0x005FEB00), while the original event is parked at the listener's +0x2AC (area +0x3C8, module +0x38C) and cleared after. (checked)
- **Edge cases:** no duplicate check: registering twice delivers twice (checked). The id is not validated at registration; a destroyed listener is skipped at delivery but stays in the list (checked; script.ldf says it is removed). **No data from the original event reaches the listener's script**: the 21 event carries no parameters, and no script command reads +0x2AC (only the delivery and a cleanup at 0x0069A265 touch it; displacement scan of the whole exe), contrary to script.ldf's "with the event data" (inferred from the scan). Listeners hear only events that reach the target's script: a gated event, or a target without a script, notifies nobody. Custom script events (1000+) cannot be listened to. No shipped script uses listeners. (checked)

### UnregisterEventListener (125)
- **Handler:** 0x00658F20. It calls 0x006983A0 (CCObject), 0x0067FAF0 (area) or the module's counterpart (not read).
- **Does:** pops oTarget, oListener, nEventType; finds the record for the type (0x00697350) and removes the listener id from it (0x004E9640, `erase(remove(...))`: every copy, inferred from its shape). (checked apart from that)
- **Edge cases:** no type filter (unlike Register). Invalid target, no record or the listener absent: nothing. The empty record stays in +0x298. (checked)

## Not read
- 0x00707000, called with every event delivered to a CCObject (a log or profiler, guessed).
- The area's and module's listener delivery and disabled sets (0x0067BFB0, 0x005FE840, 0x0067D6D0, 0x005FEB00, 0x00681750, 0x00601610, 0x00682930, 0x00601D00): taken to mirror the CCObject's.
- What +0x4C of `CScriptEvent` holds; the save side of `EventListeners`.
- The element access 0x0069BD60 for a negative index in `SetEventResource`.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not followed to the end. The shared facts of `dev/script_commands/events.md` apply (an event on the stack is a copy; `CScriptEvent` is 0x50 bytes with type +0x20, creator +0x24, target +0x28 and own script +0x2C; the copy constructor drops the own script; null events reach handlers; failed pop -639, failed push -638). "Current event" below is the event the calling object is running: a game object with a CCObject view (slot 0x18) -> `+0x2A8`; type byte 4 (area, through slot 8) -> `+0x3C4`; type 3 (module, through slot 0x1C) -> `+0x390`.

### SignalEvent (105)
- **Handler:** 0x00657E40 (not shared). Pops oObject, the event (structure 0) and, only when paramCount > 2, bProcessImmediate (else 0). Looks the target up with `GetGameObjectById` 0x005D5840 and needs the commands/events system (`app+0x1AC`).
- **Described in:** `dev/LOAD_EVENTS.md` ("A non-immediate `SignalEvent` (0x00657E40) queues the same way (0x00657EDE); only an immediate one is delivered at once (`DeliverNow`, 0x00657EFC)"), with uses in `dev/COMBAT.md` and `dev/CHARGEN.md`.
- **Does:** bProcessImmediate == 0: queues the event on the target through 0x0064ACE0 with a delay of (0, 0), the sender OBJECT_SELF (`this+8`), the target id and a kind word 0xA. Non-zero: delivers it through 0x0064AD80, with the same sender, target and kind. The queue takes the copy. Returns 0. (checked)
- **Edge cases:** an unknown target, or no system, deletes the event copy and the script goes on with nothing delivered (no failure, no result). Any object kind the table at `app+0xCC` holds works, areas and the module among them. The event's creator is not set: it is whatever `Event()` stored (the OBJECT_SELF at construction), so signalling someone else's event keeps the first sender. Any non-zero bProcessImmediate is immediate (the test is `!= 0`). A null event with a valid target is queued as null (not followed). (checked; the null case inferred)

### DelayEvent (128)
- **Handler:** 0x00657F30 (not shared). Pops fSeconds, oObject, the event and, only when paramCount >= 4, scriptname (a `std::wstring`; empty by default).
- **Described in:** `dev/TURN_BASED.md` ("`DelayEvent` counts on the world timer ..., which pause stops and the game's speed scales"; the DoT and area-of-effect timers).
- **Does:** if the target exists and the commands system is present: **a non-empty scriptname is stored in the event copy's own script (+0x2C), assigned as given** (no `.ncs` stripping, no cut at a dot), then fSeconds is split by the time manager (`app+0xD4`: 0x00A35D80 gives the whole units, 0x00A35CF0 the milliseconds; the unit length is its +0x50) and the event is queued through 0x0064ACE0 with that time, sender OBJECT_SELF, kind 0xA. Returns 0. (checked)
- **Edge cases:** this is the one command that gives an event its own script: the copy constructor drops +0x2C, delivery (events.md) looks at `engineevents.gda` first, then the event's own script, then the object's, so a scriptname is used unless the event type has a table script (33-36, 65). An unknown target deletes the event and gives no error. A zero or negative fSeconds converts to (0, 0) (0x00A35CF0 returns 0 for a value that is not above 0; the whole-units half is inferred the same), so it is delivered on the next pass of the queue, as the DoT code's `DelayEvent(0.0)` relies on. The delay is on the world timer, not real time. (checked, the negative case inferred)

### Event (106)
- **Handler:** 0x006580D0 (not shared). Pops nEventType.
- **Described in:** `dev/CHARGEN.md` and `dev/LOAD_EVENTS.md` (events the scripts make: `Event(1027)`), and `events.md` (the `CScriptEvent` layout, constructor 0x0069BAC0).
- **Does:** makes a `CScriptEvent`, **type (+0x20) = nEventType and creator (+0x24) = OBJECT_SELF (`this+8`)**, with target -1 and an empty own script, and pushes it (copied). (checked)
- **Edge cases:** the creator is OBJECT_SELF as an id, whether or not it names a live object (no lookup, unlike `Effect`). The type is not checked: 0 is an invalid event and any other number, known to no handler or not, makes a "valid" one. No parameters (arrays) yet: the script sets them with `SetEvent*`. An allocation failure is not tested (the constructor's result is written through). (checked)

### GetCurrentEvent (108)
- **Handler:** 0x00658210 (not shared). Looks up OBJECT_SELF (0x005D5840) and reads the current event (above).
- **Described in:** `dev/TURN_BASED.md` (call counts), `dev/PROBE.md` (the answer is checked in play as a `CScriptEvent` that round-trips), `dev/script_commands/events.md` (shared facts: the offsets).
- **Does:** pushes a copy of the event the object is handling, or a null event when there is none. (checked)
- **Edge cases:** an OBJECT_SELF that is not found, or is not a CCObject view, an area or the module, gives a null event; so does an object that is running no event (the pointer is null outside delivery: 0x006985F0 restores it). A null event reads as type 0 and creator -1, but `GetEventTarget` and `GetEventObject` on it fault (see them). It is a copy: `SetEvent*` on it changes nothing in flight. The area's slot is +0x3C4 and the module's +0x390 (`dev/COMBAT.md`'s paragraph on `GetCurrentEffect` has them the other way round). (checked)

### GetEventCreator (109)
- **Handler:** 0x006582A0 (not shared). Pops the event, pushes `+0x24` with `StackPushObject`, frees the copy.
- **Described in:** `dev/PROBE.md` and `dev/BUILDS.md` (checked in play against the library's accessor: 495 calls agreed); the field is in `events.md`.
- **Does:** returns the creator id: whoever was OBJECT_SELF when `Event()` made it, or what `SetEventCreator` stored, or what the engine set when it built the event. (checked)
- **Edge cases:** **a null event gives -1** (OBJECT_INVALID), no fault. The id is not validated, so the creator may be destroyed since. (checked)

### GetEventType (111)
- **Handler:** 0x00658390 (not shared). Pops the event, pushes `+0x20` as an int, frees the copy.
- **Described in:** `dev/TURN_BASED.md` (the hottest of the event commands in play, with `GetCurrentEvent`), `dev/PROBE.md`, `events.md`.
- **Does:** returns the event's type, the value that `Event()` or the engine wrote. (checked)
- **Edge cases:** **a null event gives 0** (EVENT_TYPE_INVALID), no fault. Types the engine does not know come back as they were set. (checked)

### GetEventObject (117)
- **Handler:** 0x00650990 (not shared). Pops the event, then nIndex; calls `CDataArrays::GetObject` 0x00A5A2B0 (the event is its own `CDataArrays` at +0) and pushes the answer with `StackPushObject`.
- **Described in:** `dev/CHARGEN.md` (`oChar = GetEventObject(ev, 0)` in `sys_chargen`) and `events.md` (`SetEventObject`).
- **Does:** returns the object id stored at that index of the event's object list (+0x10). (checked)
- **Edge cases:** an index at or past the end, a negative index (compared unsigned), or an event with no objects list returns -1 (OBJECT_INVALID). **A null event faults**: `GetObject` reads `[ecx+0x10]` with `ecx = 0` and nothing tests for it. The id is not validated. The popped copy is freed. (checked)

### HandleEvent (121)
- **Handler:** 0x006586D0 (not shared). Pops the event (structure 0), then the resource; cuts it at the first `.`; calls one of three delivery functions on OBJECT_SELF.
- **Described in:** `dev/NCS.md` (the first-dot cut, by stem) and `events.md` (`HandleEvent_String`, the resource-name sibling).
- **Does:** runs the event **at once, inline,** on OBJECT_SELF with that script (the stem of rScriptName), or with the object's own default script when rScriptName is empty (CCObject +0x26C, area +0x38C, module +0x1F8): CCObject 0x006985F0, area 0x0067BFB0, module 0x005FE840 (with a third argument 1). (checked)
- **Edge cases:** it calls the delivery function directly, so **`engineevents.gda` is not consulted**, as for `HandleEvent_String`; the event's own script is not used; the object's disabled-event gate (`EnablevEvent`) applies for a CCObject. An OBJECT_SELF that is none of the three kinds, or none, deletes the event and runs nothing. A null event passes through quietly for a CCObject (the delivery function tests for it). A failed pop of either argument gives -639. (checked)

### GetEventTarget (126)
- **Handler:** 0x00659030 (not shared). Pops the event, pushes `+0x28` with `StackPushObject`, frees the copy.
- **Described in:** `dev/PROBE.md` and `dev/BUILDS.md` (compared in play with the library's accessor); the field in `events.md` ("target set to the object if -1").
- **Does:** returns the event's target id. For an event that has been delivered, 0x006985F0 had set it to the receiving object if it was -1; before delivery it is -1 unless the engine set it. (checked)
- **Edge cases:** **a null event faults**: `[ecx+0x28]` is read with no null test (0x00659052), unlike `GetEventCreator` and `GetEventType`. For an event the script built with `Event()` the answer is -1 (OBJECT_INVALID). (checked)

## Not read
- 0x0064ACE0 and 0x0064AD80 (what the event queue does with the delay, the kind word 0xA and the sender, and whether a queued event survives the object's removal), and the time manager's unit (+0x50) and the rest of 0x00A35D80.
- What `SignalEvent` does with a null event copy and a live target.
- The area's and the module's delivery functions (0x0067BFB0, 0x005FE840) beyond their call.
