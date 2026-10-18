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
