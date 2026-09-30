# Turn-based mode: the first use case

The game is real time with pause. The idea is a mode in which time advances in turns: each combatant acts in an order, and the world waits between actions. Whether that is a mod of scripts, of engine hooks, or of both is what the first reading has to answer. This page is the plan for that reading and what it has found so far.

## Pause, as read 2026-09-29 (not yet verified in play)

The pause key is a message. `GameModeController` has a handler for `UserTogglePauseMessage` (`dev/MESSAGE_HANDLERS.md`), at `0x006403C0`:

```
bool GameModeController::HandleMessage(const UserTogglePauseMessage&)
{
    CClientExoApp* app = g_clientExoApp;        // 0x00C327EC
    app->SetPaused(!app->IsPaused(), true);     // 0x005D6740
    return true;
}
```

It ignores its controller entirely. Pause belongs to **`CClientExoApp`**, the client application singleton, named by the RTTI of the vftables its constructor (`0x005DC510`) stores. The name is Aurora's, Neverwinter Nights' engine, which Eclipse descends from.

`CClientExoApp::SetPaused(bool paused, bool second)`, `__thiscall`, `ret 8`, does this:

- It returns at once if the flag already says `paused`.
- The flag is an object at `+0xD8`. Its value is the byte at `+0xC`, and it is set through its vtable slot `+0x54`. A second such object at `+0xE8` takes `second`.
- It keeps the pause's start time at `+0xF8` and a running total of paused time at `+0xFC`, both from `GetTickCount`, in milliseconds.
- In between it reads the game mode from `(this+0x114)->+0x78` and sets the audio for it (read in Ghidra's decompilation, 2026-09-29). In modes 0, 1, 2, 4 and 5, pausing applies the sound category `audio_categories/ducking/OnGamePause`. In mode 3, a menu screen (below), it leaves `VOambient`, `VOconversation` and `VOnarration` playing. Unpausing undoes both.
- Ghidra's decompiler shows the `+0xE8` flag taking `paused`. The disassembly (`mov eax, [esp+0x60]` at `0x005D69EC`) says `second`. The decompiler has the stack depth wrong across the audio calls there, so trust the disassembly. Having the probe log the `+0xE8` flag's byte after each call would settle it.

`src/dao/ClientExoApp.h` wraps these facts: `Get()`, `IsPaused()`, `PausedMilliseconds()`, `SetPaused()`. The probe hooks the key handler and the setter, and it logs each `SetPaused` with its caller's address. A session of play with the probe installed will therefore list every source of pause (the key, auto-pause on combat, menus, dialogue) by where it comes from.

What pause freezes is the next question. It is some per-frame update that checks this flag, the clock a turn-based mode would stop and start between actions. The way in is from the other end: the readers of `+0xD8`'s value.

## What the first session showed, 2026-09-29 (in play, no combat yet)

The wider probe (`dev/PROBE.md`) ran through startup, the main menu, loading a save, a conversation, exploring, and opening menu screens three times.

**The game modes, by number.** The mode at `(app+0x114)->+0x78`, logged once a second, lines up with the `SetPaused` callers and with what was on screen:

| Mode | What | Evidence |
|---|---|---|
| 0 | Exploring | Unpaused, script commands running |
| 3 | A menu screen: inventory, character | Entered as `GameModeGUI`'s slot 7 paused the game, left as its slot 8 unpaused it, three times |
| 4 | Dialogue | Entered as `GameModeDialog`'s slot 7 ran, twice |
| 9 | Loading | A save loading, and an area transition |
| 11 | The main menu | Paused, from start-up until the save was loaded |
| 8, 10 | Start-up | Briefly, before the main menu |

So the guess from the decompilation that mode 3 was dialogue was wrong. Mode 3 is the menu screens, and the voice categories kept playing there are for a conversation left open under a menu.

**Every game mode has an enter and a leave.** Slot 7 of a `GameMode*` class's vtable runs when the mode is entered and slot 8 when it is left: `GameModeGUI` pauses in its slot 7 (`0x007187D0`) and unpauses in its slot 8 (`0x007188C0`), `GameModeDialog` unpauses in its slot 7 (`0x0068FE40`), `GameModeCharGen` pauses in its slot 7 (`0x00719540`). A turn-based mode can hook these pairs to know when combat, dialogue or a menu starts and ends, or add a mode of its own.

**Pausing stops the scripts.** While exploring, 200 to 300 script commands run a second. While paused on a menu screen, none do: every ten-second window paused all the way through counted zero. So the pause clock stops the script scheduler, and with it any AI that is scripted. That is most of what a turn-based mode needs from a clock, and it is already the engine's own.

**Loading pauses too.** `FUN_006C8800` pauses and unpauses around loading a save. `FUN_006C6520` sets pause about forty times in a second during an area's loading screen.

The stack samples so far separate rendering, not simulation: the 3D world stops drawing under a menu screen, and that difference fills the comparison. A fight against exploring, both unpaused, is the comparison that should show the combat code.

## Combat, from the same session (2026-09-30, two fights with rats)

**Combat is mode 1**, almost certainly `GameModeCombat`: the mode went to 1 as each fight began and back to 0 or 4 after. The mode classes the RTTI lists are Pregame, Explore, Combat, Dialog, Cutscene, Movie, GUI, Loading, Dead, CharGen, PartyPicker, Fixed and FlyCam.

**Auto-pause is a script's doing.** The pause as the first fight began came from the script command `ToggleGamePause` (`0x0066A7F0` inside it), not from the engine: a game script notices combat and pauses when the player's auto-pause option says so.

**The combat scripts run hard, and on events.** Unpaused combat ran about 3,600 to 5,400 script commands a second against 200 to 300 exploring, in the same shape both fights:

| Per second | Fighting | Exploring |
|---|---|---|
| `GetCurrentEvent` / `GetEventType` | ~200 | ~15 |
| `HandleEvent` | ~98 | ~4 |
| `GetCombatState` | ~39 | ~3 |
| `GetCurrentCommand` / `GetCommandQueueSize` | ~30 | ~3 |
| `AddCommand` | ~5 | ~0.6 |
| `CommandAttack` | ~4 | 0 |

Commands that ran only in combat: `GetThreatEnemy`, `IsHostile`, `GetCreatureRank`, `GetPackageAI`, `ClearEnemyThreat`, `GetAttackTarget`, `GetNearestObjectByHostility`, `RandomFloat`: the threat system and the AI packages. So the AI is event-driven. The engine raises events (an action finished, a creature was hit or perceived), the scripts handle them, look at a creature's command queue, choose its next action, build it (`CommandAttack`, ...) and queue it with `AddCommand`.

**Every queued action seems to pass through one function.** `AddCommand` (`0x0066E0E0`) looks the object up by id on the application (`0x005D5BE0`) and calls `0x0073CD10` on it with the command and two more arguments, `__thiscall`, `ret 0xC`. Nine other places call `0x0073CD10` too, the player's own orders among them by their addresses. If that holds in play, it is the one place a turn-based mode needs to hold a creature's next action until its turn. The next probe logs every call to it with the object's class, the command's first words and the caller.

## The queue in play (2026-09-30, a fight of 80 s unpaused, with the queue hook)

Every one of about 200 commands went through `0x0073CD10`, all on `CCCreature`s, `result` always 1. So far it is the one entry point, and the decoding of its arguments holds:

- **The command.** Its first word packs a global sequence number in its low 16 bits, one higher each command in the whole game, and the command type in bits 16-23. The types are the script's own `COMMAND_TYPE_*` numbers from `script.ldf`: 1 attack, 13 wait, 14 play animation, 20 move to location, 21 move to object, 25 use ability, 31 use object, 33 drive, 40 switch weapon sets. The top byte varies and is not read yet.
- **The second argument** is `bAddToFront`: `AddCommand` passes the script's, and it was 1 for a script that asked for the front.
- **The third** is -1 from scripts. The engine's own calls pass 0 or 2 now and then, which matches `AddCommand`'s `nOverrideAddBehavior` default of -1.

Three callers, three roles:

| Caller | Commands | What it is |
|---|---|---|
| `AddCommand`, `0x0066E1B4` | attack 75, wait 22, use ability 8, move to object 6, play animation 2, switch weapons 1 | the scripts: the AI and the tactics choosing each creature's next action |
| `FUN_006A21C0`, `0x006A22AE` | drive 18, all on the player's character | the player's direct control: moving the character yourself |
| `FUN_0073D000`, `0x0073D054` | 32 (22), 39 (5), and attack, move, ability, animation, use object | the engine's own queueing, to the front as often as not. The crash of 2026-09-30 had it under `SEffectsHolder` and `CCCreature`, so effects (a knockdown, a stun) queue here. Types 32 and 39 are not in `script.ldf`: the engine's own. The player's orders given while paused are likely here too, still to confirm by the caller's caller. |

So a turn-based mode has its gate: hold a creature's commands at `0x0073CD10` (or let them queue, and hold the start of the head of the queue) until its turn. The AI's rhythm is visible too: in combat, a creature re-decides after each attack or wait, about once a second each.

## Other things the engine exposes

- `GameModeController` also answers `PauseMessage`, `UnpauseMessage` and `SetGameModeMessage` (`0x006416E0`). The modes are classes: `GameModeExplore`, `GameModeDialog`, `GameModeCutscene`, `GameModeDead`, `GameModeCharGen`. Explore mode takes the player's actions as messages: `PerformAbilityMessage`, `MoveCharacterToLocationMessage`, `PerformAbilityOnSelectedTargetMessage`, and others. These are the points where a turn-based mode would gate a party member's action.
- The script commands (`dev/SCRIPT_COMMANDS.md`) include `TOGGLEGAMEPAUSE` (188), `GETAUTOPAUSECOMBATSTATUS` (189), and `GETCOMBATSTATE` / `SETCOMBATSTATE` (526 / 525). Of the 43 `COMMAND_COMMAND*` creature-command constructors, 22 are implemented in this exe: `CommandAttack`, `CommandUseAbility`, `CommandMoveToLocation`, `CommandWait` and the rest of what combat needs. The other 21 are registered with no implementation.

## What a turn-based mode probably needs

1. **A clock to stop and start**: the per-frame update pause halts (above).
2. **Whose turn it is**: an initiative order over the combatants. The party and enemies are enumerable through script commands (`GETPARTYLIST`, `GETNEARESTOBJECT*`, `GETCOMBATANTTYPE`).
3. **One action per turn**: let the current combatant's queued action run, then stop the clock. The player's side arrives as Explore-mode messages. The AI's side is still to find: the per-tick decision that queues an enemy's or a follower's action.

## Next readings

1. A fight with the next probe (queue hook, deeper stacks): what `0x0073CD10` is called on and with, from scripts and from player orders, and a `compare` of fighting against exploring that reaches the game's own frames.
2. The readers of the paused flag's value: what pause actually stops, and how the script scheduler checks it.
3. `SetGameModeMessage`'s handler and the object at `app+0x114`: the mode controller, and the full list of mode numbers.
4. The engine side of a queued command: where the head of a creature's queue is started, and the event raised when it finishes.

Each address found goes into `src/dao/Addresses.h` with the date and how it was found. Each layout learned goes into a class header under `src/dao/` like `ClientExoApp.h`.
