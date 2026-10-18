# Script commands, group "printlog": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2's subagent in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm` and scratch scans, for DAOParagonEngine #27. No Ghidra, no play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts (checked):
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`, on `CVirtualMachineCommands`, and returns 0, -639 on a failed pop or -638 on a failed push (`dev/script_commands/commands.md`). Pops: `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410, `StackPopVector` 0x00A2F460, `StackPopString` 0x00A2F4C0, `StackPopResource` 0x00A2F5F0, `StackPopObject` 0x00A2F6D0. Strings are `std::wstring` (UTF-16).
- **Most of this group is compiled out of the shipped game.** PrintInteger, PrintFloat, PrintString, PrintObject, PrintVector, PrintResource, PrintToLogAndFlush, Warning, SendToRunDatabase, DEBUG_PrintToScreen, PrintToLogWindow, SendGameEventRunDatabase and Screenshot pop their arguments and do nothing else. A port matching vanilla can make them no-ops, but it must still pop the same arguments.
- **The engine log** is the one live output. It is a global (`[0x00C2BDF8]`, its log object at `+4`, constructor 0x004BFD00) with 48 channels, whose names are a table at 0x00BF4698 ("Error", "Assert", ..., 20 "Script", 22 "General", ...). The constructor turns every channel on, then turns 13 off, Script (+0x848+20) among them. It then reads `.\ECLog.ini`, section `[LogTypes]`, one key per channel name: a value above 0 turns the channel on (0x004BFAA0, the value read with `ForScript_StringToInt`). `0x005C5E80` (`xor al,al; ret`) is a build flag that is 0 in this exe, and both paths check it. A write (0x004BFA20, `(log, file, line, channel, fmt, ...)`) does nothing for a channel that is off. Otherwise it formats into a 1024-character buffer (+0x48), and 0x004BF790 writes the line as `"%ls\t%ls%ls\r\n"` (prefix from 0x004BF0B0) when the log is open (+0x878, the file at +0x38). The folder is `%Documents%\BioWare\Dragon Age\Logs\` (SHGetFolderPathW CSIDL 5), or `..\Logs\` when that call fails. The base name is "DragonAge". (checked; the file's full name, and when it is opened, not read.) script.ldf agrees: "Script=1 in the [LogTypes] section" of ECLog.ini.

### PrintInteger (0)
- **Handler:** 0x0064DF50 (not shared).
- **Does:** pops nInteger, then bPrepend. Nothing else. (checked)
- **Edge cases:** no output in this build, despite script.ldf's "prints to the log file". Both pops always happen, so a failed pop of bPrepend returns -639. (checked)

### PrintFloat (1)
- **Handler:** 0x0064DFA0 (not shared). It calls the wide format helper 0x004C3030.
- **Does:** pops fFloat, nWidth, nDecimals and bPrepend. It clamps the width to 0..18 and the decimals to 0..9. It builds the format string `"<PL> %18.09f\n"` (`"<PL> PRINTFLOAT: ..."` with bPrepend; `%1d` for widths under 10, else `%2d`), then frees it. The float is never formatted or written. (checked)
- **Edge cases:** no output. The clamp is the same as FloatToString's ("conversions"). (checked)

### PrintString (2)
- **Handler:** 0x0064E0F0 (not shared).
- **Does:** pops sString and bPrepend, then frees them. Nothing else. (checked)

### PrintObject (3)
- **Handler:** 0x0064E1A0 (not shared).
- **Does:** pops oObject and bPrepend. The object is never looked up. Nothing else. (checked)

### PrintVector (4)
- **Handler:** 0x0064E1F0 (not shared).
- **Does:** pops vVector (three floats) and bPrepend. Nothing else. (checked)

### PrintToLog (5)
- **Handler:** 0x00668820 (not shared). It calls the engine log's write (0x004BFFD0 → 0x004BFA20).
- **Does:** reads the string on top of the stack in place (0x00A2D1E0, a typed peek at depth 0) and writes it with `"%ls"` to channel 20 "Script", from `svmachinecommands.cpp` line 0x1025. It then drops the cell (0x00A2F780(1)). (checked)
- **Edge cases:** written only when ECLog.ini turns Script on. It is off by default, so a stock install writes nothing. A message longer than 1,023 characters is cut (the 0x400 buffer, inferred). A non-string on top fails the script with -639. (checked)

### LogTrace (143)
- **Handler:** 0x006688A0 (not shared). It calls `CClientExoApp::GetGameObjectById` (0x005D5840) and the log write.
- **Does:** pops nChannel, sLogEntry and oTarget. It looks the object up and calls its vtable slot +0x18 (the template, whose +0xBC is the tag, as `GetTag` 0x006596A0 reads it), then discards the result. It writes sLogEntry alone with `"%ls"` to channel 20 "Script". (checked)
- **Edge cases:** **nChannel is ignored**: the write is always to Script, whatever LOG_CHANNEL_* is passed. The target never appears in the line (the remains of compiled-out formatting, inferred). It is off unless ECLog.ini turns Script on. An invalid object is skipped. (checked)

### PrintResource (733)
- **Handler:** 0x00653650 (not shared).
- **Does:** pops rResource and bPrepend. Nothing else. (checked)

### PrintToLogAndFlush (130)
- **Handler:** 0x0064E250 (not shared).
- **Does:** reads the top string in place (0x00A2D1E0), then drops it (0x00A2F780(1)). **No write and no flush**: unlike PrintToLog, the log call is compiled out. (checked)
- **Edge cases:** a non-string on top returns -639. (checked)

### Warning (674)
- **Handler:** 0x0064E450 (shared with SendToRunDatabase 174; there is no switch on the number).
- **Does:** pops sWarning and frees it. Nothing is shown. (checked)

### GetTime (705)
- **Handler:** 0x00653C50 (not shared). It calls the world timer's vtable +0x14 (0x004CB250, `fld qword [ecx+0x18]`), then `_ftol2` (0x008E34F0).
- **Does:** pushes the world clock's total, in seconds, truncated to an int. The clock is `EC::WorldTimer` at app+0xD4 (`Addresses.h`, kWorldTimer). (checked)
- **State:** +0x18 is a double. Each update (slot 6, 0x00A358F0) adds `min(real seconds since the last update, 0.1) * scale(+0x54)`, and nothing while it is paused (+0x58). So it runs with the game's speed, stops while the timer is paused, and falls behind real time when frames are slower than 10 a second. (checked) Whether a load resets or restores it was not read.
- **Edge cases:** pushes 0 when there is no app or no timer. Truncation is toward zero. (checked)

### GetLowResTimer (176)
- **Handler:** 0x0064E960 (not shared). It calls 0x0041E870, `jmp [GetTickCount]`.
- **Does:** pushes `GetTickCount()` as a signed int: milliseconds since Windows started. (checked)
- **Edge cases:** it has nothing to do with game time. After 24.8 days of uptime it is negative, and it wraps after 49.7 days. (inferred from the API)

### DEBUG_PrintToScreen (81)
- **Handler:** 0x0064E290 (not shared).
- **Does:** pops sString, nPosFromTop and fLife, all three unconditionally. Nothing is shown. (checked)

### PrintToLogWindow (82)
- **Handler:** 0x0064E370 (not shared).
- **Does:** pops sString. It pops sLogType only when paramCount ≥ 2. Nothing else; the GUI's `PrintToLogWindowMessage` class exists but is not used here. (checked)

### SendToRunDatabase (174)
- **Handler:** 0x0064E450 (shared with Warning 674, no switch).
- **Does:** pops sString and frees it. Nothing is sent, and script.ldf's 960-character limit is not checked. (checked)

### SendGameEventRunDatabase (175)
- **Handler:** 0x0064E4E0 (not shared).
- **Does:** pops nEventId, oidObject, oidTarget, nParam1, nParam2, sParam1 and sParam2. It pops sType only when paramCount ≥ 8, otherwise setting it to "xE" (0x00B07D50). Then it frees them all; nothing is sent. (checked)

### Screenshot (820)
- **Handler:** 0x0064E7B0 (not shared).
- **Does:** pops sFileName and sPath, and bHideConsole only when paramCount > 2. Nothing else: no screenshot is taken. (checked)

### TakeScreenshot (599)
- **Handler:** 0x00653700 (not shared). It calls the talk string helpers (make 0x004D1C70, found 0x004D1E90, text 0x004D1EB0), the message's constructor 0x00650CE0 and destructor 0x00650EF0, `GameModeController::HandleMessage_TakeScreenshotMessage` (0x00642050), and the party's notification 0x00629C20.
- **Does:** when paramCount > 0, pops bHideInterface, nTitleStrRef, nDescriptionStrRef, nType and sId. It then builds a `TakeScreenshotMessage` and hands it to the game mode controller (app+0x114). The message holds: hide = bHideInterface ≠ 0; the title's and the description's text; a type label from nType (0 "Story" 398835, 1 "Achievement" 398834, 2 "Story: Awaiting Upload" 400316, else 398835 when that is empty; 3 "Achievement: Awaiting Upload" 400315, else 398834; any other: empty); and sId. (checked)
- **State:** it is gated on `ClientOptions` ([0x00C32938]) +0x2B0 `AutoScreenshots` (`+0xC` byte). If +0x300 `AutoScreenshotsNotification` is set and a party exists, the party's notifications (party+0x330) get "New screenshot" (396028), except in game mode 9. (checked)
- **Edge cases:** **nothing happens unless both strrefs are in a talk table** (each looked up; a missing one aborts). So the defaults (0, 0) do nothing: strref 0 is in no table. Nothing happens either with the AutoScreenshots option off. paramCount 0 skips all pops, leaving every argument 0. The talk texts are the male forms. (checked) Where the file goes (`Screenshots\Story\`, `\Achievements\`, `\User\` strings) is in the message handler, not read.

### GetDebugHelpersEnabled (331)
- **Handler:** 0x00653C30 (not shared).
- **Does:** always pushes 0. (checked)

## Not read
- The engine log's file name and when it is opened (0x004BF4A0 area), the line prefix 0x004BF0B0 (a time stamp per script.ldf), and +0x87C's other write path.
- What `HandleMessage_TakeScreenshotMessage` (0x00642050) does with the message: the capture, the folder and file name, hiding the interface.
- Whether the world timer's total (GetTime) is saved, restored or reset on load or on an area change.
- The world object's vtable slot +0x18 that LogTrace calls, beyond its use by GetTag.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe. Not seen in play. `printlog.md` says most of its group is compiled out of the shipped game; `Probe` is another of those.

### Probe (680)
- **Handler:** 0x0064E8B0 (not shared). `StackPopString` 0x00A2F4C0, `StackPopFloat` 0x00A2F410.
- **Described in:** not in master's notes under this name (`dev/CRASHES.md` and `dev/PROBE.md` use "probe" for DAOLib's own plugin, which is unrelated).
- **Does:** pops sOutput, then fValue, destroys the string, and returns 0. Nothing is written, logged or recorded. (checked: no call between the pops and the epilogue)
- **State:** none.
- **Edge cases:** the float is popped **always**, whatever paramCount says, so a call passing one argument still pops a second cell: the compiler fills the default 0 (script.ldf's `fValue = 0`), so a compiled script never misses it. A failed pop returns -639. Like the other `printlog` commands, a port may make it a no-op if it keeps the pops.

## Not read
- Whether any shipped script calls it (not grepped); it is declared in script.ldf's printlog group.
