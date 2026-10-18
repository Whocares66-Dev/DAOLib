# Script commands: the party group (DAOParagonEngine #27 baseline)

Read 2026-10-18 by lib-agent-2 in GOG's `DAOrigins.exe` 1.05 (`tools\daoexe.py` disassembly, the address library's names). Not seen in play. "Checked" = read in the disassembly; "inferred" = from names, shapes or scripts, not traced to the end.

Common to every handler (checked): arguments are popped with `CVirtualMachine::StackPopObject` 0x00A2F6D0 / `StackPopInteger` 0x00A2F3C0 / `StackPopFloat` 0x00A2F410, and a failed pop returns -639 (0xFFFFFD81, fails the script). Results are pushed with `StackPushInteger` 0x00A2D650 / `StackPushObject` 0x00A2D980 / `StackPushString` 0x00A2D730, and a failed push returns -638. The party is `[[g_clientExoApp 0xC327EC]+0xD0]+0x58` (getter 0x005D57C0, null before a game exists). No handler here checks an object's type unless stated: the follower map and the approval list are keyed by the raw id.

Party object offsets met here (checked unless marked): +0x184 the controlled creatures' ids (vector); +0x194 the same as creature pointers, rebuilt by 0x00635B40 (inferred); +0x1E8 the leader (SmartData attribute); +0x234 the active party; +0x244 the pool; +0x260 the non-party followers (vector of ids); +0x2A8 the follower map, an `stdext::hash_map<id, FollowerInfo>` (list at +0x2AC, size at +0x2B4, buckets at +0x2BC; find 0x006FE370, node value at node+0xC); +0x300 the approval records (a SmartData list of `PartyMemberApproval*`, locked by 0x00403C70 and unlocked by its slot +0x24); +0x330 the notification queue (inferred: the floating "approval changed" and "new tactic slot" messages).

FollowerInfo, the map's value (checked): +0 state (`GetFollowerState`), +4 substate, +8 locked (byte), +9 follow-party-leader (byte).

### GetFollowerSubState (400)
- **Handler:** 0x0065E9E0 → `ScriptBackend::ForScript_GetFollowerSubState` 0x006328E0 (checked).
- **Does:** looks the creature's id up in the follower map (party+0x2A8) and returns FollowerInfo+4 (checked).
- **State:** party+0x2A8 entry +4, read (checked).
- **Edge cases:** no party, an empty map, or an id with no entry (not in the pool, invalid, any non-creature) → 0 (checked). No type check (checked).

### SetFollowerSubState (144)
- **Handler:** 0x0065EA50 → `ForScript_SetFollowerSubState` 0x00632880 (checked).
- **Does:** if the creature has a follower-map entry, stores nSubState at FollowerInfo+4. Nothing else: no event, no GUI, no range check (checked).
- **State:** party+0x2A8 entry +4 (checked).
- **Edge cases:** no party or no entry → nothing, no entry is created (checked). Any int is stored as given (checked). Returns 0.

### IsFollowerLocked (146)
- **Handler:** 0x0065EAD0 → `ForScript_IsFollowerLocked` 0x006329B0 (checked).
- **Does:** returns FollowerInfo+8 (a byte), zero-extended (checked).
- **State:** party+0x2A8 entry +8 (checked).
- **Edge cases:** no party, or no entry → FALSE (checked).

### SetFollowerLocked (147)
- **Handler:** 0x0065EB40 → `ForScript_SetFollowerLocked` 0x00632950 (checked).
- **Does:** stores (bLocked != 0) at FollowerInfo+8 if the entry exists (checked). The engine reads the flag nowhere in these handlers. Who else does was not traced (the party GUI's lock, inferred).
- **State:** party+0x2A8 entry +8 (checked).
- **Edge cases:** no party or no entry → nothing (checked). Any non-zero int is stored as 1 (checked).

### GetFollowPartyLeader (768)
- **Handler:** 0x0065EBC0 → `ForScript_GetFollowPartyLeader` 0x00632A80 (checked).
- **Does:** returns FollowerInfo+9 (checked).
- **State:** party+0x2A8 entry +9 (checked).
- **Edge cases:** party exists but no entry (or an empty map) → TRUE (the backend's default is 1). No party at all → FALSE (the handler's default is 0) (both checked).

### SetFollowPartyLeader (769)
- **Handler:** 0x0065EC30 → `ForScript_SetFollowPartyLeader` 0x00632A20 (checked).
- **Does:** stores (bFollow != 0) at FollowerInfo+9 if the entry exists (checked). Movement code that reads it was not traced.
- **State:** party+0x2A8 entry +9 (checked).
- **Edge cases:** no party or no entry → nothing (checked).

### AdjustFollowerApproval (324)
- **Handler:** 0x006610D0 → 0x006311A0 (approval add, unnamed); notification 0x006298F0 on party+0x330 (checked).
- **Does:** pops bNotify only when the call has more than 2 arguments (the argument count is the handler's second argument; default FALSE) (checked). Walks party+0x300 for the `PartyMemberApproval` whose ObjectID attribute (+0x58) is the follower, and sets its Approval attribute (+0x68) to clamp(old + nAmount, -100, 100) (checked). If bNotify and `CCParty::IsInPartyPool` 0x006316F0 holds the follower, it queues a type-8 notification for the follower on party+0x330 with the raw nAmount (checked). A pending one for the same follower has the amount added to it, and is removed if the sum is 0 (checked). That this is the floating approval text is inferred.
- **State:** party+0x300 record +0x68; party+0x330 (+0x68 list) notifications (checked).
- **Edge cases:** no record (approval not enabled for it) → nothing changes, but with bNotify a pool member still gets the notification (checked). nAmount 0 → no notification (checked). No party → nothing (checked).

### GetFollowerApproval (622)
- **Handler:** 0x00661190 → `ForScript_GetFollowerApproval` 0x006341C0 (checked).
- **Does:** copies the record list, finds the record for the follower, and returns its Approval (+0x68) (checked).
- **State:** party+0x300, read (checked).
- **Edge cases:** no party or no record → 0 (checked). The value is always in -100..100 if set only through 324 (checked: 324 clamps). No other writer was looked for.

### SetFollowerApprovalEnabled (382)
- **Handler:** 0x006611F0 → `ForScript_SetFollowerApprovalEnabled` 0x00635960 (checked).
- **Does:** enabled and no record → `new PartyMemberApproval` (0xAC bytes, ctor 0x00630580: Approval 0, Desc empty), ObjectID = follower, appended to party+0x300 (checked). Disabled and a record → erased from the list and deleted (slot +0x10), **losing the approval value** (checked). Otherwise nothing (checked).
- **State:** party+0x300 (checked). The record's attributes are named "ObjectID", "Approval" and "Desc" in the data tree, which is how the GUI shows them (names checked; the GUI reading inferred).
- **Edge cases:** no party → nothing (checked). Any id gets a record, creature or not (checked).

### SetFollowerApprovalDescription (829)
- **Handler:** 0x00661250 → `ForScript_SetFollowerApprovalDescription` 0x00631290 (checked).
- **Does:** builds a localized string from nDescStrRef (0x004D1C70, inferred), then copies it into the follower's record at +0x78 (Desc: a byte, an int, a wstring at +8 and bytes +0x24..+0x26) (checked).
- **State:** party+0x300 record +0x78 (checked).
- **Edge cases:** no party or no record → nothing (checked). The strref is not validated here (checked).

### GetPartyLeader (624)
- **Handler:** 0x0066AC20 (no arguments) (checked).
- **Does:** returns the leader attribute at party+0x1E8 (checked).
- **State:** party+0x1E8, read (checked).
- **Edge cases:** no party → OBJECT_INVALID (-1) (checked).

### SetPartyLeader (625)
- **Handler:** 0x00661320 → `ForScript_SetPartyLeader` 0x00636180 (checked).
- **Does:** `CClientExoApp::GetCreatureById`; then sets party+0x1E8 to the id (checked). It calls the talk table (g_tlkTable 0xC1D08C, 0x004E5CF0) with 1 when the creature's +0x7B0 record has byte +0x50 == 2, else 0 (checked). That this selects female text for gendered strings is inferred. If the creature is in the active party (+0x234) and not already first, it is moved to index 0 (erase 0x004E9640, insert 0x006CE860). Then 0x00635B40 (controlled list +0x184 → creature pointers +0x194) and 0x00635B60 (four-slot rebuild, inferred: the party bar) run (checked: the calls).
- **State:** party+0x1E8, +0x234 order, +0x194 (checked).
- **Edge cases:** not a creature or not found → nothing (checked). A creature outside the active party still becomes the leader and sets the talk table's gender, with no reorder (checked). No party → nothing (checked).

### AddNonPartyFollower (351)
- **Handler:** 0x00661370 → `ForScript_AddNonPartyFollower` 0x006342E0 (checked).
- **Does:** for a creature in neither the non-party list (+0x260) nor the pool (+0x244): appends it to +0x260, calls its slot +0x154, sets creature+0x240 = 0, and activates it (slot +0x140(1)) if the +0x70 test says it is inactive (checked). Then it sets the float at 0xC33B44 to -1.0 and calls the party's slot +0x64 (checked; their meaning not read). Returns 1 (checked).
- **State:** party+0x260; creature +0x240, the active flag (checked).
- **Edge cases:** not a creature, already a non-party follower, or in the pool → 0 (checked). No party → 0 (checked).

### RemoveNonPartyFollower (352)
- **Handler:** 0x006613E0 → `ForScript_RemoveNonPartyFollower` 0x00631E00 (checked).
- **Does:** if the id is in +0x260 it is erased. If the creature exists, the handler calls its slot +0x154, sets attribute +0xB70 to 0, calls 0x00695220(0) (its +0x70 base's slot +0x60, then 0x00A5CAE0), sets +0x240 = 1, and calls 0x006AE780 (a follow/position update, inferred). Then it calls the party's slot +0x64 (checked).
- **State:** party+0x260; creature +0xB70, +0x240 (checked).
- **Edge cases:** not in the list → nothing (checked). It is not deactivated (checked). No party → nothing (checked).

### IsControlled (714)
- **Handler:** 0x00660C90 → `ForScript_IsControlled` 0x006316D0 (checked).
- **Does:** returns whether the id is in the party's selection +0x184 (`std::vector<unsigned>::Contains` 0x008663A0) (checked). So `IsControlled` means "selected": with the whole party selected every member answers TRUE, not only `GetMainControlled`, the selection's first (corrected 2026-10-18 by lib-agent-2, `dev/PARTY.md`).
- **State:** party+0x184, read (checked).
- **Edge cases:** OBJECT_INVALID → FALSE without touching the party (checked). **The handler does not test the party for null**: with no party and a valid id it reads [0+0x184] and faults (checked; a game-less call only).

### IsPartyAIEnabled (787)
- **Handler:** 0x0066AC60; the creature's tactics object from 0x007AD0D0 (checked).
- **Does:** 0x007AD0D0 = `GetCreatureById(id)`, then the creature's attribute +0xC00, a pointer to its tactics object (inferred `PartyTactics`). Returns that object's bool attribute +0x5C (0x00838210 reads the attribute's byte +0xC) (checked).
- **State:** creature +0xC00 → tactics +0x5C (checked).
- **Edge cases:** not a creature, or no tactics object → FALSE (checked).

### SetNumTactics (773)
- **Handler:** 0x0066D600 → 0x007AFB70 (resize), 0x00629870 (notification) (checked).
- **Does:** pops bSendNotification only when there are more than 2 arguments (default FALSE) (checked). It resizes the creature's current tactic list (tactics +0x6C) to nNumTactics, and every preset's entry list (16-byte records in the vector at tactics +0xA8..+0xAC) too, through 0x007AF2E0 (checked). Shrinking deletes the `PartyTacticEntry` objects past n. Growing makes new 0x160-byte entries (ctor 0x007AD310) (checked). If bSendNotification and n is above the old count, it queues a type-6 notification for the creature with (n − old) on party+0x330 (checked).
- **State:** tactics +0x6C, +0xA8 (checked).
- **Edge cases:** no clamp at all (checked). A negative n is taken as unsigned, so it means a huge resize (inferred: a fault). Not a creature → nothing (checked).

### GetTacticTargetObject (372)
- **Handler:** 0x0066AE10 → 0x007ADCF0 → 0x007ADA40 (entry by index) (checked).
- **Does:** nIndex is 1-based (index − 1, checked). It returns the entry's attribute +0x78, the object a per-follower target type names (checked; the meaning is from dev/GUI.md's `fsc_retval2`, inferred).
- **State:** tactics +0x6C entry +0x78 (checked).
- **Edge cases:** an index out of range (unsigned compare, so 0 and negatives too), not a creature, or no tactics object → **object id 0, not OBJECT_INVALID** (the handler's default is `xor esi,esi`) (checked).

### GetTacticConditionObject (373)
- **Handler:** 0x0066AF30 → 0x007ADCF0 (checked).
- **Does:** the same lookup. It returns entry attribute +0x98 (checked).
- **State:** tactics +0x6C entry +0x98 (checked).
- **Edge cases:** out of range or no creature → OBJECT_INVALID (-1) (checked). Note the asymmetry with 372.

### GetTacticCommandItemTag (374)
- **Handler:** 0x006614F0 → 0x007ADCF0 (checked).
- **Does:** the same lookup (1-based). It returns the wstring at entry +0x128 (the item's tag) (checked).
- **State:** tactics +0x6C entry +0x128 (checked).
- **Edge cases:** out of range or no creature → "" (checked).

### SetTacticEntry (319)
- **Handler:** 0x006615F0; validity 0x007B0040 (named `ForScript_SetTacticEntry` in the library but a check returning bool); entry 0x007ADCF0 (checked).
- **Does:** pops all seven arguments, nCommandParam included, unconditionally (checked). Index 1-based. It reads column `FollowerAI` (hash 0x2A1AF533) for: nCondition in table 137 (`TacticsConditions`), its `ConditionBase` (0x56BB2EC7) in table 139 (base conditions), nTargetType in table 140 and nCommandType in table 141 (checked). It writes only if all four are non-zero and 0x007B0040(creature, targetType, base FollowerAI value, commandType, param, -1) says the command is valid for them (checked). For command 4 (Use Ability) the param must be in the creature's valid ability list, and for 7 (Use Item) with param ≠ -1, in its item list (checked: the branches; the lists' builders 0x007AFE80/0x007AED50/0x007AF6C0 not read). It then sets entry +0x58 enabled (nEnabled != 0), +0x68 target type, +0x88 condition, +0xA8 command type, +0xB8 command param (checked).
- **State:** tactics +0x6C entry +0x58/+0x68/+0x88/+0xA8/+0xB8 (checked). The target/condition objects (+0x78/+0x98) and the item tag are not set here (checked).
- **Edge cases:** an invalid row, a row with FollowerAI 0 (e.g. command 1 INVALID, 8, 9, 13, 14), an invalid combination, or an index out of range → silently nothing, returns 0 (checked). The preset is not marked customized here (checked: no write to +0xE4).

### GetTacticPresetID (365)
- **Handler:** 0x00651650 → `ForScript_GetTacticPresetID` 0x007ADAB0 (checked).
- **Does:** returns the id of preset record [tactics+0xC4] in the preset list (tactics +0x88), only when tactics +0xD4 == 2 and the bool +0xE4 (inferred: customized/dirty) is false (checked).
- **State:** tactics +0x88, +0xC4, +0xD4, +0xE4, read (checked).
- **Edge cases:** customized, mode ≠ 2, index < 0 or past the list, no creature → 0 (checked). That matches ldf's "0 = none or customized".

### SetTacticPresetID (366)
- **Handler:** 0x006516B0 → `ForScript_SetTacticPresetID` 0x007ADB40 (checked).
- **Does:** finds nPresetID in the preset list (+0x88). If found, it resets every current entry (0x007ABEC0: enabled 1, target, condition, command, param 0…), then sends `EVENT_TYPE_LOAD_TACTICS_PRESET` (83) with object 0 = the creature (tactics +0x58) and int 0 = nPresetID, delivered at once (`AIMaster::DeliverNow`), so the scripts fill the rows (checked). Then +0xC4 = the preset's index, +0xD4 = 2 (checked).
- **State:** tactics +0x6C entries, +0xC4, +0xD4 (checked). +0xE4 is not cleared here (checked).
- **Edge cases:** an id not added by 367 → nothing at all (checked). No creature → nothing (checked).

### AddTacticPresetID (367)
- **Handler:** 0x00651710 → `ForScript_AddTacticPresetID` 0x007AF4E0 (checked).
- **Does:** appends nPresetID to the selectable preset list (+0x88) unless already there (find 0x00768B80 == -1, append 0x005E3880) (checked).
- **State:** tactics +0x88 (checked).
- **Edge cases:** a duplicate → nothing. Any int is accepted, with no table check (checked). No creature → nothing (checked).

### SetAtmosphereRGB (801)
- **Handler:** 0x00664D60 (checked; `src/dao/core/Sky.h` already records it).
- **Does:** on the controlled creature's area (`ScriptBackend::GetControlledArea` 0x0062FD10), at area+0x658's sky block (+0x120): 20 `ATM_PARAM_SUN_COLOR_RGB` → +0x00..+0x08, 21 `CLOUD_COLOR_RGB` → +0x68..+0x70, 23 `FOG_COLOR` → +0x28..+0x30, and the changed flag +0x84 = 1 (checked).
- **State:** the area's sky block (checked). Floats are stored with no clamp (checked).
- **Edge cases:** **any other nParamId returns -640 (0xFFFFFD80), failing the script** (checked). No controlled area → nothing, returns 0 (checked).

### SetLookAtEnabled (805)
- **Handler:** 0x0066BE50, the shared `ScriptCommand::SetObjectProperty`; jump-table case at 0x0066BFD8 → 0x0069EA50 (checked).
- **Does:** stores (nEnabled != 0) at creature+0x76C. When turning it off from on, it first resets the look-at controller (0x0069E9D0 → 0x006CAB90) if one exists (checked; that it stops head-tracking is inferred).
- **State:** creature +0x76C (checked).
- **Edge cases:** pops nEnabled only when there are more than 1 argument, else 0 (checked). Not a creature → it falls through to the placeable cases and does nothing (checked). Missing object → nothing (checked).

### SetStealthEnabled (758)
- **Handler:** 0x0066BE50, case 0x2F6 at 0x0066BFA4 → 0x006A2400 (checked).
- **Does:** stores nEnabled (the raw int) at creature+0x7F0. On a change (or always when the app state 0x005D5E70 is 9): enabling sets +0xD00 = -1.0. It also sends online telemetry ('GAME'/'GSTH') when the state ≠ 9 and the bool +0x4F8 is set. Each of the four visuals +0x948/+0x94C/+0x95C/+0x960 gets 0x005B9450(f), where f = a config float (+0x28 of 0x005B9020's object) when enabling and 0.0 when disabling. Then +0x7F8 = 1 (checked; f as the stealth shimmer is inferred).
- **State:** creature +0x7F0 (`StealthMode` in the template, the perception's "stealthed", dev/COMBAT.md), +0xD00, +0x7F8 (checked).
- **Edge cases:** no change → nothing, unless the app state is 9 (checked). Not a creature → nothing (checked). No skill or ability check (checked).

### GetStealthEnabled (759)
- **Handler:** 0x00668A20, the shared `ScriptCommand::GetObjectProperty`; case 0x2F7 at 0x00668BC9 (checked).
- **Does:** returns creature+0x7F0 as stored (checked).
- **State:** creature +0x7F0, read (checked).
- **Edge cases:** missing object or not a creature → 0 (checked).

### SetCreatureCanBeTracked (764)
- **Handler:** 0x0066BE50, jump-table case at 0x0066BFF3 (checked).
- **Does:** stores nEnabled (the raw int) at creature+0xB94. Nothing else (checked).
- **State:** creature +0xB94 (checked). dev/COMBAT.md reads +0xB94 in perception as "a stealth detection term, not understood". By this command it is the trackable flag (inferred; the template's `Trackable` likely loads it).
- **Edge cases:** not a creature or missing → nothing (checked).

### GetCreatureCanBeTracked (765)
- **Handler:** 0x00668A20, case 0x2FD at 0x00668BBE (checked).
- **Does:** returns creature+0xB94 as stored (checked).
- **State:** creature +0xB94, read (checked).
- **Edge cases:** missing or not a creature → 0 (checked).

## Not read
- What creature +0x240, +0xB70, slot +0x154, the party's slot +0x64 and the global float 0xC33B44 mean (AddNonPartyFollower / RemoveNonPartyFollower), and 0x006AE780's full effect.
- Who reads FollowerInfo's substate, locked and follow-leader bytes (movement, the party GUI); where map entries are created (0x0063A020, per dev/PARTY.md) and their initial values.
- 0x00635B60 (four-slot rebuild after SetPartyLeader) and the talk table call 0x004E5CF0 beyond its argument.
- The notification queue's consumer on party+0x330 (types 6 and 8).
- SetTacticEntry's valid-list builders (0x007AFE80, 0x007AED50, 0x007AF6C0); who sets tactics +0xE4 (customized), +0xD4's other values, +0x5C (party AI on); the class at creature +0xC00 (`PartyTactics` inferred from its message handlers).
- The stealth visuals' function 0x005B9450, the config at 0x005B9020+0x28, and app state 9.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05). Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. Pops and pushes as `dev/script_commands/Party.md` says (-639 a failed pop, -638 a failed push). The party is `[[app 0xC327EC]+0xD0]+0x58` (getter 0x005D57C0). The tactics rules (index 1-based, odd defaults) are on master's `dev/PARTY.md`, "The script commands" table under section 1; that note already records `GetNumTactics` 0 while party AI is off and `IsTacticEnabled` needing Target, Condition and Command non-zero, so `%TEMP%\lib-agent-2-party-rules.md` need not be cited for them.

### GetFollowerState (399)
- **Handler:** 0x0065E670 (not shared) -> `CCParty` 0x00632810 (checked).
- **Described in:** `dev/PARTY.md`, "SetFollowerState and RemoveFromParty" and its pool section (the map at party+0x2A8, find 0x006FE370, count at +0x2B4).
- **Does:** pops the object and pushes the FollowerInfo state (+0) of the follower map entry for that id: 0 INVALID, 1 ACTIVE, 2 AVAILABLE, 3 UNAVAILABLE, 5 SUSPENDED, 6 LOADING, 7 LOCKEDACTIVE (script.ldf's constants).
- **Edge cases:** no party, an empty map (count tested first), or no entry for the id (any invalid or non-creature id included): 0 (checked). No type check. It answers for the pool, not for the active party: AVAILABLE is "in the pool, not active".

### SetFollowerState (706)
- **Handler:** 0x0065E6E0 (not shared) -> `CCParty` 0x0063B9A0, a switch (table 0x0063BA0C) (checked).
- **Described in:** `dev/PARTY.md`, "SetFollowerState and RemoveFromParty" (the state table).
- **Does:** pops object and state; with a party, runs the switch (0 removes from party and pool and erases the map entry; 1 and 7 add to the pool and the active party; 2, 3, 5 leave the active party and store the state; 4 and 6 do nothing).
- **Edge cases:** a state above 7 does nothing (`ja` past the table) (checked). No party: nothing. When the gate `0x00401F30` answers non-zero the handler first builds a telemetry-like record keyed by a four-character tag chosen from the state and the pool test ('NULL' by default; 'FLEV' for state 0 with the id in the pool; 'FJON' for a non-zero state with the id not in the pool; constants 0x4E554C4C, 0x464C4556, 0x464A4F4E), and in every case then runs the switch (checked as flow; the record's meaning inferred). No object-type check: the raw id is put in the map (checked). Returns 0.

### IsFollower (129)
- **Handler:** 0x00660CF0 (not shared) -> `CCParty` 0x00631830 (checked).
- **Described in:** `dev/PARTY.md`, the pool section ("`IsFollower` 0x00631830 asks only" the active party at +0x234); `dev/script_commands/Party.md` for the shared offsets.
- **Does:** pushes TRUE when the id is in the **active party** (+0x234) (vector contains, zero-extended to 0 or 1).
- **Edge cases:** AVAILABLE and other pool-only members: FALSE (they are in the pool, not +0x234). No party, or an invalid or non-creature id: FALSE (checked). The hero in the active party answers TRUE. A non-party follower (`AddNonPartyFollower`'s list at +0x260) is FALSE.

### GetPackage (317)
- **Handler:** 0x00661430 (not shared) (checked).
- **Described in:** `dev/PROPERTIES.md` (the creature's template load reads `Package` into +0x7E8); `dev/SCRIPT_COMMANDS.md` (the field table: `CCCreature` +0x7E8, `GetPackage`, int).
- **Does:** `GetCreatureById`, then pushes the int at creature+0x7E8, the UTC template's `Package` (the class package row used for levelling and AI tables).
- **Edge cases:** invalid or non-creature: 0 (checked). The value is whatever the template or `SetPackage`-style write left; the engine does not validate it against a table here.

### GetPackageAI (318)
- **Handler:** 0x00661490 (not shared) (checked).
- **Described in:** `dev/PROPERTIES.md` (template load, `PackageAI` at +0x7EC); `dev/PARTY.md` (the AI's use: a package table, `<= 10011` becomes 10011).
- **Does:** `GetCreatureById`, then pushes the int at creature+0x7EC (the `PackageAI` field).
- **Edge cases:** invalid or non-creature: 0 (checked). Seen in play 330 times in a 2026-09-30 session returning the field (`dev/LIBRARY.md`).

### GetNumTactics (772)
- **Handler:** 0x0066D580 (not shared); the creature's tactics object from 0x007AD0D0 (checked).
- **Described in:** `dev/PARTY.md`, the commands table ("size of `TacticsList`, but 0 when `Enabled` (+0x5C) is false"); `dev/LIBRARY.md`.
- **Does:** pushes the count of the current tactic list at tactics+0x6C ((end - begin) / 4 over the pointer vector).
- **Edge cases:** no creature, no tactics object, or `Enabled` (+0x5C, the party-AI switch `IsPartyAIEnabled` reads) false: 0 (checked at 0x0066D5B6). A non-follower has no tactics object, so 0. An empty list (begin pointer null): 0.

### IsTacticEnabled (774)
- **Handler:** 0x0066ACC0 (not shared); entry lookup 0x007ADCF0 with index - 1 (checked).
- **Described in:** `dev/PARTY.md`, the commands table (774: entry Enabled && Target != 0 && Condition != 0 && Command != 0; it does not read +0x5C).
- **Does:** pushes 1 when all four hold on the 1-based entry (the target type at +0x68, condition +0x88 and command +0xA8 each tested non-zero, and the enabled flag at +0x58), else 0.
- **Edge cases:** index 0, negative or past the list: no entry, 0 (the lookup compares unsigned). No creature or tactics object: 0. A non-zero but invalid trio still counts as enabled (nothing validates here).

### GetTacticTargetType (775)
- **Handler:** 0x0066AD80 (not shared) (checked).
- **Described in:** `dev/PARTY.md`, commands table (775: entry +0x68; missing -> 0); the value names are table 140 (`dev/LIBRARY.md`).
- **Does:** pushes the entry's target type attribute (+0x68) for the 1-based index.
- **Edge cases:** out-of-range index (unsigned compare), no creature or no tactics object: 0 (the default is `xor esi,esi`) (checked). Not gated by +0x5C: it answers while party AI is off.

### GetTacticCondition (776)
- **Handler:** 0x0066AEA0 (not shared) (checked from the table in PARTY.md; same shape as 775).
- **Described in:** `dev/PARTY.md`, commands table (776: entry +0x88; missing -> 0); names in table 137.
- **Does:** pushes the entry's condition attribute (+0x88).
- **Edge cases:** missing entry or creature: 0 (inferred identical to 775; the OBJECT variant 373 differs, it answers -1).

### GetTacticCommand (777)
- **Handler:** 0x0066AFC0 (not shared) (same shape).
- **Described in:** `dev/PARTY.md`, commands table (777: entry +0xA8; missing -> 0); names in table 141.
- **Does:** pushes the entry's command type attribute (+0xA8).
- **Edge cases:** missing entry or creature: 0.

### GetTacticCommandParam (778)
- **Handler:** 0x0066B050 (not shared) (same shape).
- **Described in:** `dev/PARTY.md`, commands table (778: entry +0xB8; missing -> 0).
- **Does:** pushes the entry's command parameter (+0xB8): an ability id for Use Ability, an item reference for Use Item (`Party.md`, SetTacticEntry).
- **Edge cases:** missing entry or creature: 0. A parameter of -1 is stored as -1 and returned as such (for Use Item).

### SetAtmosphere (800)
- **Handler:** 0x00664A10 (not shared); `ScriptBackend::GetControlledArea` 0x0062FD10 on the party; the sky block at area+0x658's +0x120 (checked).
- **Described in:** `src/dao/core/Sky.h` (the field table and refusals), `src/dao/Atmosphere.cpp` (the head), `dev/LIBRARY.md` (the area's look), `dev/SKY.md`.
- **Does:** pops the parameter id (int) and the float; one float is written into the controlled creature's area's live sky block and the changed flag at +0x84 is set to 1 (jump table 0x00664CE4, 30 entries, one case per id; offsets in `Sky.h`).
- **Edge cases:** ids 8, 19, 20, 21 and 23 (the RGB ids, which only `SetAtmosphereRGB` takes) and any id above 29: **returns -640 (0xFFFFFD80), failing the script** (checked). Id 29 writes block +0x7C (no ldf constant names it). No controlled area (no party or controlled creature): nothing, returns 0. No clamp. The effect is on the live block only; the area's own copy at layout+0x2A0 is untouched (see `ResetAtmosphere`).

### ResetAtmosphere (802)
- **Handler:** 0x00664E80 (no arguments, not shared) (checked).
- **Described in:** `src/dao/Atmosphere.cpp` head (0x00664E80, 0x005CFBD0 copies the area's own block at layout+0x2A0 over the live one, then works the scattering out again) and `dev/LIBRARY.md`.
- **Does:** the controlled creature's area's layout (area+0x658) `0x005CFBD0`: restores the sky from the area's own block, undoing every `SetAtmosphere` and `SetAtmosphereRGB`.
- **Edge cases:** no controlled area: nothing, returns 0. It resets the sky only, not the sunlight set by a plugin, and not the framebuffer effects.

### FB_SetEffectResource (803)
- **Handler:** 0x00651880 (not shared); manager at the global 0x00C1E8B0 (checked).
- **Described in:** `src/dao/Framebuffer.cpp` head (the effect and resource lookups) and `dev/LIBRARY.md`.
- **Does:** pops effect name, resource name (two strings) and a float; finds the effect by name (0x00456CA0, case-insensitive per the play check in the file), the resource by name in the effect's map (0x004602D0); if its type (+4) is 3 (float) sets it (0x0045ED90).
- **Edge cases:** no manager yet (global null), unknown effect, unknown resource, or a resource that is not a float: **silently nothing**, returns 0 (checked). A pop failure of either string or the float returns -639. The value is not validated or clamped.

### FB_SetEffectEnabled (804)
- **Handler:** 0x006519B0 (not shared) (checked).
- **Described in:** `src/dao/Framebuffer.cpp` head and `dev/LIBRARY.md`.
- **Does:** pops effect name and an int, finds the effect and calls 0x0045FA10(nEnabled != 0), which switches it on or off and tells its stages.
- **Edge cases:** no manager or unknown effect: nothing, returns 0. Any non-zero int is TRUE. Turning `da_hsvmatrix` off leaves the picture dimmer than the area's grade (play note in the file).

## Not read
- The tactics getters 776 to 778 were not disassembled (shape taken from PARTY.md's table and 775/774's code); only 772, 774 and 775 were read.
- What the tagged record in `SetFollowerState` does, and the gate 0x00401F30.
- The `CCParty` body behind 0x00632810's map find beyond what `Party.md` records.
