# Script commands, group "game_modes": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" is read in the disassembly; "(inferred)" is not.

Shared facts: handlers are `__thiscall (commandId, paramCount)`, `ret 8`. A failed pop returns -639 and a failed push -638; either fails the script. The mode controller is `GameModeController` at `app+0x114` (`app` = `g_clientExoApp` 0x00C327EC). `SetGameMode` (0x006414A0) and the switch's order and event 60 are in `dev/TURN_BASED.md` ("The game mode's change"). The numbers are script.ldf's `GM_*`: -1 INVALID, 0 EXPLORE, 1 COMBAT, 2 DEAD, 3 GUI, 4 DIALOG, 5 CUTSCENE, 6 FLYCAM, 7 FIXED, 8 PREGAME, 9 LOADING, 10 MOVIE, 11 CHARGEN, 12 PARTYPICKER. The client options singleton is `[0x00C32938]`.

### GetGameMode (529)
- **Handler:** 0x0065E490 (not shared). Calls `GameModeController::GetGameMode` (0x00640060). (checked)
- **Does:** pushes the current mode's number: the current mode object (`controller+0x74`) and its +0x5C. If there is no current mode object, it pushes -1 (GM_INVALID). (checked)
- **State:** controller+0x74 (current mode object), +0x88 (the previous mode's number, `dev/TURN_BASED.md`). (checked)
- **Edge cases:**
  - **FlyCam is hidden.** When the current mode is 6 (GM_FLYCAM), it pushes the previous mode (+0x88) instead. So a script never sees 6 unless FlyCam followed FlyCam. (checked)
  - A mode only requested (`SetGameMode` files a request taken on the controller's next update) is not seen until the switch. (checked in `dev/TURN_BASED.md`'s reading)
  - It takes no arguments and can only fail on the push. script.ldf's "for a specific player" has no player argument. (checked)

### ShowStartMenu (362)
- **Handler:** 0x0066A5A0 (not shared). Calls 0x005DD310 on the client app, the same routine the engine runs at start-up to reach the main menu: it is the `SetPaused(false,false)` caller at 0x005DD34F that CLAUDE.md records. (checked)
  - [C] (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) 0x005DD310 (arg 2 = bShowCredits): SetPaused(false, false); when a config field at +0x1E8 is set it restarts the LCD base object (0x005DCD00); when a config field at +0x104 is set it asks 0x005D95C0 and, if that returns 1, skips the switch altogether; with credits it builds module and GUI strings; it then makes a GameModePregameData with +4 = (-(credits != 0) & 0x3F) - 1, that is 62 for credits and -1 for none, and +8 = 0, and calls GameModeController::SetGameMode(data, 8).
  - [C] Entering GM_PREGAME (GameModePregame vfunction7, 0x0064CB50): when a module exists it first calls 0x005D6A50, the module's teardown: the GUI system's slot 0x28, the module's deleting destructor (slot 0, argument 1), the controlled-creature id +0x2B8 = -1, AIMaster::ClearEvents, the party object at +0x1B0 deleted, the area manager at +0x14C deleted (unless a config byte +0x68 says otherwise), then the transient state reset. So the switch unloads the module. It then sets the input mode "GameModePregame", starts the music event "music/_menu", and from the data pushes the credits' scene (62) as a full-screen GUI (0x00610CE0).
  - [I] What the config fields +0x1E8, +0x104 and +0x68 stand for was not read.
- **Does:** pops bShowCredits only when an argument was passed (default 0). (checked)
  - If the sound manager (0x005B9020) has a current music object (+0x120), it sets that object's byte +0x94 to bShowCredits. (checked; what the byte does was not read)
  - 0x005DD310(bShowCredits) unpauses (`CClientExoApp::SetPaused` 0x005D6740, (0,0)). (checked)
  - It then requests **mode 8, PREGAME** (force 0) with a `GameModePregameData` (vtable 0x00B00278) whose +4 is **62 with credits, -1 without**: the GUI scene to show (62 is the credits; inferred from its use). (checked)
  - With credits, it gives GUI scene 62 (+0xF8) a name: "dao", or a name the module supplies through 0x0051B370 when that is not empty. (checked; what the name selects is inferred)
- **Bypass:** if `g_eclipseGame` (0x00C1D0A0) holds a pending module name (+0xF0, its length at +0x104), it is loaded instead (0x005D95C0) and cleared. When that load answers 1, no PREGAME is requested. (checked; what sets the name was not read)
- **Telemetry, with credits only:** it sends `ScriptBackend::ForScript_SendOnlineTelemetry` (0x0047D4D0) twice: "GAME"/"GMNY" with the hero's money (`+0xBF0` inventory → money location → +0x90, as `%d`) and "GAME"/"CASQ" with a float from GUI scene 0's +0xC4 (`%.2f`). Both are sent only if `g_eclipseGame` exists. (checked; the codes are the four-character constants pushed)
- **Edge cases:** the module is unloaded by the switch to PREGAME, not by the handler (read 2026-10-22, above). Only `epipt_main.nss`:99/105 calls it (credits, then the main menu). Returns 0. (checked)

### GetAutoPauseCombatStatus (189)
- **Handler:** 0x0065E520 (not shared). Calls 0x00838210 (`mov al,[ecx+0xC]`). (checked)
- **Does:** pushes the **AutoPauseCombat** option as 0 or 1: the options singleton `[0x00C32938]`+0xB8 is that option's object, and its value byte is at +0xC (so +0xC4 overall). (checked)
- **State:** the option is registered under the name "AutoPauseCombat" (0x00B01438, at 0x005E6CF2) and loaded and saved with the other options (0x005E8D43, 0x005E8DF1). Its value is cleared before the load (0x005E8D57), so it defaults to off (inferred). (checked except the default)
- **Edge cases:** it takes no arguments and does not check the options pointer for null, which is set from start-up (inferred). Only `config_h.nss`:72 reads it, as `!= 0`. **The pause itself is the scripts' doing** (`ToggleGamePause`, `dev/TURN_BASED.md`). (checked)

## Not read
- The module-name bypass in 0x005DD310: what sets `g_eclipseGame`+0xF0/+0x104, and the `+0x1E8` branch, which calls app 0x005DCD00 with `app+0x58`.
- The music object's +0x94 byte; the credits name given to GUI scene 62; what GUI scene 0's +0xC4 float holds.
- `SetGameMode` (530) and `ToggleGamePause` (188) are in this group in script.ldf but were not asked for. 530's force-0 request is in `dev/TURN_BASED.md`.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe. Not seen in play. Both are named at the end of `game_modes.md` as "not asked for".

### SetGameMode (530)
- **Handler:** 0x0065E4E0 (not shared). `GameModeController::SetGameMode` 0x006414A0 on the object at `[app+0x114]`, called as `(mode, 0, 0)`.
- **Described in:** `dev/TURN_BASED.md` ("`SetGameMode` only files a request": the base slot and the stack, the update that takes a request, "a request for the current mode with force 0 is dropped", the switch and event 60) and `dev/COMBAT.md` (the death screen).
- **Does:** pops one int and calls the controller with force 0 and data 0. Nothing switches yet: the request is filed and taken on the controller's next update (`TURN_BASED.md`). Returns 0 at once.
- **State:** the controller's base slot (+0x8C) or stack (+0x9C), per `TURN_BASED.md`.
- **Edge cases:** mode -1 (GM_INVALID) is filed as nothing (0x006414CF jumps over the work, checked). A mode outside 0 to 12 indexes the controller's mode table at +0x58 with no bound (0x006414D8, checked), so it reads past the array (inferred: undefined). **GM_LOADING (9) with no data** reads through the null data pointer (0x006414F3, checked; the fault itself inferred), so a script should not ask for it. The controller pointer `[app+0x114]` is not tested for null. A failed pop is -639. Setting the mode already current is dropped by the update (`TURN_BASED.md`).

### ToggleGamePause (188)
- **Handler:** 0x0066A7A0 (not shared). `CClientExoApp::SetPaused` 0x005D6740 `(bPause, 0)`.
- **Described in:** `dev/TURN_BASED.md` ("Auto-pause is a script's doing": the pause at the first fight came from this command), `dev/SCRIPT_COMMANDS.md`.
- **Does:** pops bPause; non-zero calls `SetPaused(true, false)`, zero calls `SetPaused(false, false)`. (checked) Returns 0.
- **State:** the app's pause state (through `SetPaused`).
- **Edge cases:** **despite the name it does not toggle** when the argument is there: it sets. Only when the pop **fails** (an empty stack, which a compiled call never gives, since the default is filled) does it toggle: it reads the byte at `app+0xD8+0xC` (0x00838210) and calls `SetPaused(!that, false)` (checked at 0x0066A7B4 to 0x0066A7D4) and returns 0 instead of -639. Any non-zero int pauses. The second `SetPaused` argument is 0 always.

## Not read
- 0x006414A0 beyond its first lines, `SetPaused`'s own effects (`TURN_BASED.md`), and what `app+0xD8` holds.
