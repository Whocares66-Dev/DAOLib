# Script commands, group "sounds": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, ghilana for the tables, for DAOParagonEngine #27. Not seen in play. "(checked)" is read in the disassembly; "(inferred)" is not.

Shared facts:
- Handlers are `__thiscall (commandId, argCount)`, `ret 8`; pops through the VM at `[0x00C1D080]` (`StackPopObject` 0x00A2F6D0, `StackPopString` 0x00A2F4C0, `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410). A failed pop returns -639 (0xFFFFFD81) and fails the script; otherwise 0. None of these pushes anything. (checked)
- **The sound manager** is a lazily made singleton, getter 0x005B9020 (global 0x00C323B4, 0x2D0 bytes, constructor 0x005B8DF0, no vtable). Every handler but the sound-object pair calls the getter. Fields used here: +0x94 the soundset variation bags, +0xAC/+0xB0 a vector of live sound-event instances, +0x120 the current music event (null when none), +0x5C a state compared with 9 by the sound-object tick. (checked; meanings inferred from use)
- **A sound-event instance** is a 0xC8-byte record (constructor 0x005BA240, destructor 0x005BA350, no vtable) wrapping an FMOD event at +0x70 (FMOD's `Event` interface; the exe imports `fmod_event.dll`). +0x38 its name, +0x54 a reverb/environment string, +0xBC/+0xBD flags. Start is 0x005BAC80, stop 0x005B9330, set 3D position 0x005B67D0. (checked as calls; field meanings partly inferred)
- **Event names** are FMOD event paths, `project/group/.../event`, wide strings. 0x005BA530 resolves one: it splits on `/` (string 0x00AEBF7C), and a name containing `/vo/` (0x00AFECBC) is retried under each voice-over project, `%s%s` (0x00AFECB0) with the table at 0x00BF9820: `VOconversation`, `VOambient`, `VOambient_ss`, `VOnarration`. (strings and loop checked; the exact retry rule inferred)
- **A sound object** (`CCSoundObject`, vftable 0x00B2208C; `CCSoundObjectPannable` 0x00B223E4; type byte 15, cast slot 16; lookup `GetSoundObjectById` 0x005D59B0). Fields, from its load 0x007C21C0 and setters (checked): +0x120 position, +0x130 orientation, +0xB8 tag (as every object), +0x440 its sound-event instance (null until made), +0x444 `Sound` (the event name), +0x460 the reverb environment given to the event, +0x47C `Volume`, +0x480 `Pitch`, +0x484 `FadeIn`, +0x488 `FadeOut`, +0x48C `MaxDistanceMult`, +0x490 `ConeInside`, +0x494 `ConeOutside`, +0x498 `ConeVolume`, +0x49C `Priority`, +0x4A0 `Active` (byte), +0x4A1 playing (byte), +0x4A2 in the world (byte, vtable slot 0x140 0x007C2090: false stops and frees +0x440, true makes it through slot 0x1C0), +0x4A3 `Occludable`, +0x4A4 a byte set by 0x007C2060 (unread).
  - `Play` 0x007C20F0 (`ScriptBackend::ForScript_PlaySoundObject`): nothing without +0x440; sets +0x4A1 = 1; if the object has an area, the area (0x0067DCB0) writes the reverb for the object's position into +0x460 (the area's default at area+0x424, or that of a reverb trigger from area+0x34C containing it: inferred), passed to the event (0x005B69A0); then starts the event. (checked)
  - `Stop` 0x007C1FF0 (`ForScript_StopSoundObject`): with +0x440, sets +0x4A1 = 0 and stops the event (0x005B9330(0)). (checked)
  - `SetActive` 0x007C2190: if the byte changes, stores +0x4A0 and calls Play (true) or Stop (false). The load calls it with the saved `Active`. (checked)
  - A tick, vtable slot 0x160 (0x007C2150): if Active, not playing, in the world, and the manager's +0x5C ≠ 9, it calls Play. (checked; who calls the slot and when not read)
- **"The player's area"** (ActivateSoundByTag, SetMusicVolumeStateByTag): `0x005D5CC0` = `GetCreatureById([[app+0xD0]+0x5C])`, the controlled creature (dev/LOAD_EVENTS.md calls it the leader), then its vtable +0xA0 (the area). The area's id vector at +0x694/+0x698 is walked, each id looked up by kind, and the object's tag (+0xB8) compared with the string by `std::wstring` compare (0x004C0DE0, exact and case-sensitive: inferred from the plain compare). (checked)

### PlaySound (477)
- **Handler:** 0x0065CEB0 (not shared). Calls `GetObjectById` 0x005D5BE0, the manager getter, and `ScriptBackend::ForScript_PlaySound` 0x005BC7F0 (a manager method). (checked)
- **Does:** pops oTarget, then sSoundEventName. Copies the object's position (+0x120..+0x128) and calls `PlaySound(name, &pos, 0, 0)`: a new 0xC8 instance is made, the name resolved (0x005BA530), the position set, and the event started; on failure the instance is destroyed. (checked)
- **State:** the sound plays at the position the object had at the call; it does not follow the object (checked). The instance is kept in the manager's list +0xAC that StopSound walks (inferred: registration not read).
- **Edge cases:** an invalid object, or one that is not a `CCObject` (an area, the module), does nothing; an empty string returns at once (length +0x14 = 0). An unknown event name: the start fails and the instance is freed, silently. No failure reaches the script. (checked)

### StopSound (478)
- **Handler:** 0x00651190 (not shared). Calls `ScriptBackend::ForScript_StopSound` 0x005BA100 on the manager. (checked)
- **Does:** pops sSoundEventName. Walks the manager's instance vector (+0xAC begin, +0xB0 end); for each, builds its full path (0x005B6860: the event's group path up to its last `/`, plus the FMOD event's name, via the event's vtable +0xA8) and, when it equals the string, stops it (0x005B9330(0)). Every match is stopped, not just the first. (checked)
- **Edge cases:** the compare is exact (the full path, case-sensitive: inferred), so a short name matches nothing. An instance with no FMOD event (+0x70 null) gives an empty name. Sound objects' own events are not in this list (inferred), so it does not stop them. No game script calls it. (checked except as marked)

### PlayMusic (484)
- **Handler:** 0x0065CFD0, **shared with StopMusic (485)**; there is no switch on the command number. (checked)
- **Does: nothing.** It pops sMusicName, calls the manager getter (making the manager if it did not exist), discards the result, frees the string and returns 0. (checked)
- **Edge cases:** script.ldf says "Play music"; the engine ignores it. Music is driven by the area and its music-volume triggers (below), not this command. One game script calls it. (checked)

### StopMusic (485)
- **Handler:** 0x0065CFD0, the same body as PlayMusic (above). (checked)
- **Does: nothing**: pops the string and returns 0. script.ldf's "Stop specified or all music" is not implemented. No game script calls it. (checked)

### SetMusicIntensity (486)
- **Handler:** 0x0065D070 (not shared). Calls `ScriptBackend::ForScript_SetMusicIntensity` 0x005B6CD0 on the manager's music event (+0x120). (checked)
- **Does:** pops nIntensity; if the manager has a current music event, calls `SetParameter((float)nIntensity, 0.0f)`: the value goes to event+0x8C and the 0.0 to event+0x88 (inferred: the transition time, here immediate). (checked)
- **State:** the same two fields SetMusicVolumeStateByTag's trigger writes (0x005B6E80 → 0x005B6CD0): "intensity" and the volume state are one parameter of the music event, the last writer wins. (checked)
- **Edge cases:** no music event (+0x120 null): nothing. Any integer, negative included, is converted to float and stored; no clamp. (checked)

### ActivateSoundByTag (487)
- **Handler:** 0x0065D0C0 (not shared). Calls 0x005D5CC0 (the controlled creature), its area (vtable +0xA0), then the area's 0x0067C230. (checked)
- **Does:** pops sSoundTag, then nActivate. For each id in the area's +0x694 vector that is a sound object (`GetSoundObjectById`) with that tag, calls `SetActive(nActivate != 0)` (0x007C2190): +0x4A0 is stored and the sound is played or stopped when the value changes. (checked)
- **State:** only the player's current area; sound objects elsewhere are untouched. Active is saved (`Active` field), so the change persists. (checked)
- **Edge cases:** no controlled creature or no area: nothing. Setting the value it already has does nothing (it does not restart a stopped sound). A sound with no event instance stores the flag and starts nothing. Any nonzero nActivate is true. (checked)

### SetMusicVolumeStateByTag (488)
- **Handler:** 0x0065D1B0 (not shared). Calls the controlled creature's area and its 0x0067C2E0. (checked)
- **Does:** pops sMusicVolumeTag, then nMusicState. For each id in the area's +0x694 vector that is a trigger (`0x005D5AA0`, type byte 8) whose vtable +0x1DC answers a `CCTriggerAudioMusicVolume` (vftable 0x00B20B54, slot 0x1DC returns `this`; other triggers answer null) and whose tag matches, calls 0x00799350: stores nMusicState at +0x510 and, if the player is inside (+0x520), re-applies it (slot 0x1E4 0x00799380 → manager 0x005B6E80 → the music event's parameter, with the trigger's +0x518 as the transition, inferred). (checked)
- **State:** the trigger's state is overwritten, so entering it later applies the new state (inferred from slot 0x1E4 being the enter path, which also sets +0x520). (checked except as marked)
- **Edge cases:** no controlled creature or area: nothing; no matching trigger: nothing. No range check on nMusicState. With no music event playing the trigger's value is stored but nothing sounds. (checked)

### PlaySoundSet (479)
- **Handler:** 0x006624C0 (not shared). Calls `GetCreatureById` 0x005D58C0, then `ScriptBackend::ForScript_PlaySoundSet` 0x006B5740 `(entry, probability, bForce)` on the creature. (checked)
- **Does:** pops oTarget, nSoundSetEntry, and fProbabilityOverride only when argCount > 2 (else 0.0). If GUI scene 8 is open (gui vtable +0x60(8), scene +0xBC) and its +0xC0 object's +0x58 is this creature, the line is played on that object's stand-in (+0x5C → +0xC) instead (inferred: a preview character of a GUI stage). (checked)
- **The backend (checked):**
  - Entry 18 (COMBAT DEATH) is forced. Unforced, it returns if the creature is already speaking (its effects-holder's +0x60 emitter, 0x00A5C9E0: +0x1AC instance playing).
  - The creature's soundset name is creature+0x96C; empty means nothing. Tables `sst_<name>` (0x00B102FC; column `Count` by entry) and `sse_<name>` (0x00B102EC; column `StrID`, row entry*100 + variation) are loaded by name. Count ≤ 0, or no row entry*100: nothing.
  - Count > 1: a shuffle bag per (soundset, entry) in the manager (+0x94; next 0x005B8890, filled by 0x005B8970 with every row's StrID), so variations do not repeat until all are used. Count 1: row entry*100.
  - Probability: a nonzero fProbabilityOverride is used as is. Zero: 1.0 if creature+0x969 is set, otherwise `ss_types` (table 138) `Probability` for the entry, times `ss_probabilitymodif` (table 281) `ProbabilityModifier` at row Count (1: 0.2, 2: 0.33, 3: 0.5, 4: 0.8, 5: 1.0; no row: unchanged) when `BUseProbablityModifier` is set.
  - Probability ≤ 0: nothing. Otherwise r = (rand(100)+1)/100; the line plays (0x0069FE50(StrID, bForce): the talk-table line's voice) when probability ≥ r, else 0x005C5E80 (unread).
- **Edge cases:** a non-creature or invalid object: nothing (checked). An override above 1.0 always plays; a negative one never does (checked). The entry is not range-checked; a missing row gives Count 0 and nothing (checked).

### PlaySoundObject (480)
- **Handler:** 0x0065BD40 (not shared). `GetObjectById`, then the object's vtable +0x40 (cast to `CCSoundObject`), then `Play` 0x007C20F0 (shared facts). (checked)
- **Does:** plays the sound object's event at its position with its area's reverb, and sets +0x4A1 playing. **It does not set Active (+0x4A0)**, so it is not saved as on, unlike ActivateSoundByTag. (checked)
- **Edge cases:** an invalid object or one that is not a sound object: nothing. A sound object whose event was never made (+0x440 null, e.g. not in the world) does nothing. Calling it while playing starts the event again (0x005BAC80, no playing check). (checked; the restart's audible effect inferred)

### StopSoundObject (481)
- **Handler:** 0x0065BD90 (not shared). As PlaySoundObject, then `Stop` 0x007C1FF0. (checked)
- **Does:** stops the event and clears +0x4A1; Active (+0x4A0) is left set. (checked)
- **Edge cases:** an invalid or non-sound object: nothing. **An Active sound stopped this way can be restarted by the sound object's tick** (slot 0x160 plays an Active, non-playing, in-world sound when the manager's +0x5C ≠ 9); to keep one silent, ActivateSoundByTag(tag, FALSE) clears Active. (checked as code; when the tick runs not read)

## Not read
- How 0x005BA530 resolves a name inside FMOD in full (the `/vo/` retry order, the manager's +0x58 voice limit compared there) and when an instance leaves the manager's +0xAC list.
- The music event at manager+0x120: who sets it (the area's music on load, inferred) and what its +0x88/+0x8C feed in FMOD.
- When the sound object's slot 0x160 tick runs, and the manager's state +0x5C (the value 9).
- GUI scene 8's identity in PlaySoundSet's stand-in substitution, creature+0x969's meaning, 0x005C5E80 (the roll lost) and 0x0069FE50 past its call.
- The area's +0x694 vector's full contents (it holds sound objects and triggers at least) and the reverb lookup 0x0067DCB0 in detail.
