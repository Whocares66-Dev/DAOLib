# Turn-based mode: the first use case

The mode itself is built in the sibling repo `DAOTurnBased`. This file keeps what was read of the engine for it, which is the library's to know: pause, the command queue, the AI's loop.

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
| `FUN_0073D000`, `0x0073D054` | 32 (22), 39 (5), and attack, move, ability, animation, use object | the engine's own queueing, to the front as often as not. The crash of 2026-09-30 had it under `SEffectsHolder` and `CCCreature`, so effects (a knockdown, a stun) queue here. Types 32 and 39 are not in `script.ldf`; the game's commands table names them: 32 is ExecuteEffect, 39 Sheathing (read 2026-09-30). The player's orders come through here too, player issued (below). |

Read 2026-09-30, all ten calls of `0x0073CD10` (`dao/Addresses.h`, `kCCObject_QueueCommand`). `FUN_0073D000` is the engine's make-and-queue (`CCObject::QueueNewCommand`), called from 13 places: effects, weapon draws with their animation, modal abilities' animations, jumps, moves, conversations, and `CCObject::QueuePlayerCommand` (`0x0073D060`), which explore mode's order handlers call and which marks its commands player issued. So the player's orders and the engine's reactions share a return address, and `dao::Command::PlayerIssued()` tells them apart. The others: the client command table's `AddCommand` (`0x006DA2C4`), turning (`0x006A23C5`), interacting (`0x006A3A19`), and `SetActive`'s animation and wait, in both command tables.

`QueueCommand` always keeps the command: it returns 0 only for a null one, and any other goes into the queue. No caller frees a command after it. A detour that refuses one must free it, whichever caller it came from.

So a turn-based mode has its gate: hold a creature's commands at `0x0073CD10` (or let them queue, and hold the start of the head of the queue) until its turn. The AI's rhythm is visible too: in combat, a creature re-decides after each attack or wait, about once a second each.

## Other things the engine exposes

- `GameModeController` also answers `PauseMessage`, `UnpauseMessage` and `SetGameModeMessage` (`0x006416E0`). The modes are classes: `GameModeExplore`, `GameModeDialog`, `GameModeCutscene`, `GameModeDead`, `GameModeCharGen`. Explore mode takes the player's actions as messages: `PerformAbilityMessage`, `MoveCharacterToLocationMessage`, `PerformAbilityOnSelectedTargetMessage`, and others. These are the points where a turn-based mode would gate a party member's action.
- The script commands (`dev/SCRIPT_COMMANDS.md`) include `TOGGLEGAMEPAUSE` (188), `GETAUTOPAUSECOMBATSTATUS` (189), and `GETCOMBATSTATE` / `SETCOMBATSTATE` (526 / 525). Of the 43 `COMMAND_COMMAND*` creature-command constructors, 22 are implemented in this exe: `CommandAttack`, `CommandUseAbility`, `CommandMoveToLocation`, `CommandWait` and the rest of what combat needs. The other 21 are registered with no implementation.

## What a turn-based mode probably needs

1. **A clock to stop and start**: the per-frame update pause halts (above).
2. **Whose turn it is**: an initiative order over the combatants. The party and enemies are enumerable through script commands (`GETPARTYLIST`, `GETNEARESTOBJECT*`, `GETCOMBATANTTYPE`).
3. **One action per turn**: let the current combatant's queued action run, then stop the clock. The player's side arrives as Explore-mode messages. The AI's side is the command-complete event (below): hold it for a creature until its turn, and the AI does not pick its next action until then.

## The AI's loop, from the scripts' source (2026-09-30)

The toolset's database holds the source of the game's scripts (`dev/TOOLSET.md`, `tools/toolset_scripts.py`), and it answers "what queues the AI's next action". Creatures run `rules_core.nss`, whose header calls it "the main entry point into combat AI, ability use and any event that is global to all creatures". When a creature's command finishes, the engine sends it `EVENT_TYPE_COMMAND_COMPLETE`, with the finished command's type, its status and a sub-command as the event's integers 0, 1 and 2, and a blocking object as object 2. `rules_core` handles that event by calling `AI_DetermineCombatRound` (`ai_main_h_2.nss`, the AI proper, 155 KB), for followers and for any creature in combat, which chooses the next action and queues it through `AddCommand`, the `QueueCommand` the probe watched. So combat is a chain of command-complete events, one per creature, and there is no round: a turn-based mode can hold a creature's next decision by holding its command-complete event (or its `AI_DetermineCombatRound`) until its turn, and release it then. The event is an engine structure the library now reads (`dao::CScriptEvent`), and events were checked in play the same day (`dev/LIBRARY.md`).

## A design from the AI's source (2026-09-30)

What `ai_main_h_2.nss` does, read for where a turn-based mode takes hold:

- `AI_DetermineCombatRound` returns at once if the creature is a non-combatant, if the game is in neither combat nor explore mode (then it queues a wait), if the creature has a current command or anything queued (`GetCurrentCommand`, `GetCommandQueueSize`), or if its AI is off. Otherwise it runs the creature's tactics (`_AI_ExecuteTactic` over its package's table) and queues what they choose.
- Every choice is queued through `WR_AddCommand` (27 places in `ai_main_h_2`), which checks its arguments and calls the script command `AddCommand` (`dao::script::id::AddCommand`).
- The next choice comes when that command completes: the engine sends `EVENT_TYPE_COMMAND_COMPLETE`, and `rules_core` calls `AI_DetermineCombatRound` again.

So one intercept holds the whole AI side. Intercepting `AddCommand` (`dao/ScriptHooks.h`), a mode can keep the command of any combatant whose turn it is not (a copy, `OwnedCommand`, taken from the call's argument) and answer the script as though it were queued. The creature then does nothing and gets no `COMMAND_COMPLETE`, so it decides nothing more. When its turn comes, the mode queues the kept command (`dao::script::api::AddCommand`); when that completes, the AI decides again, and the intercept keeps that too. The player's side is the explore mode's messages (`dao/Messages.h`: `PerformAbilityMessage` and its kin, `MoveCharacterToLocationMessage`), held the same way through their handlers.

The mode's own clock is `dao::OnFrame`, which runs on the scripts' thread and keeps running while the game is paused (both seen 2026-09-30), so it may call script commands directly: whose turn it is, `GetCombatState`, `GetTeam`, and `AddCommand` for the kept command.

Still to read: what else queues commands without `AddCommand` (effects, the engine's own reactions: `QueueCommand`'s other callers, `dev/TURN_BASED.md` above), and whether a command kept past a pause or an area change stays valid.

## How close a melee attack strikes from (read 2026-10-02)

The turn-based mod measured its player's dagger strikes against a giant spider landing from 2.4 m between centres, a metre closer than its own reach rule (the scripts' `AI_MELEE_RANGE`, 3.5, which is the AI's judgement of "a melee target", not the engine's reach). The engine's own, read in the exe:

- **The attack command's step** (`0x00742FD0`, ID 17685) asks whether the weapon is ranged and queues an approach, then the attack (sub-action 50). It tests no reach itself; the approach does, on its first run, and walks only if needed.
- **A creature's personal space** is `PerSpace` from `apr_base`, read once per creature (`0x00748A40`, beside `Height`, or `Height_F` for a female body) into its size record (the creature's +0x7B0, `PerSpace` at +0x60, `Height` at +0x64); the creature's vtable slot 39 (`0x006A21A0`) answers it. Elves, humans and dwarves 0.35, children 0.30, giant spiders 0.70.
- **Melee** (sub-action 4, `0x007AA560`): the target gives the attacker a place on its melee ring (slot 38, `0x006A8EE0`; every creature has a ring, not only the large monsters), and its slot 33 (`0x006A3D00`, ID 14499) judges reach: the distance between centres at most **attacker's `PerSpace` + target's `PerSpace` + 1.2 m**, **and** the attacker within 1.25 m of that place, on the ground (x and y; the centres' distance counts height). Corrected 2026-10-04: the second test was first written here as a facing test; it is the ring place. Out of reach, it walks to the place.
- **Ranged** (sub-action 15, `0x007AA200`, a staff, bow or crossbow in the main hand: `bitm_base`'s `Type` 5 or 6, its `WeaponWield` not; corrected 2026-10-04, staves first written here as not ranged): in range when the distance on the ground (x and y) is at most the weapon's `Range` from `bitm_base` (capped at 60 m) and at least both personal spaces, and the attacker has line of sight (`0x0067C750`). Corrected 2026-10-04: the spaces are a minimum, not added to the range. Out of range or sight, it walks toward the target until within `Range` less both spaces.
- **The swing** (sub-action 1, `0x007A87C0`), after a turn to face (sub-action 39, to within 1 degree), waits while the target is more than 60 degrees above or below the attacker or out of its line of sight, then sends `EVENT_TYPE_COMMAND_PENDING` for the scripts to resolve the blow. Corrected 2026-10-04: the 60 degrees were first read here as facing; they are elevation (`0x004EE5A0`). The melee attack's own set-up (`0x00745E10`, ID 17720) places the swing at `-(sum + 1.2)` along the attacker's facing, with 0.15 m of play.

So Lyna against a giant spider: 0.35 + 0.70 + 1.2 = 2.25 m between centres, and with the 0.15 m of play the 2.4 m measured; two humans 1.9 m. Weapons add nothing to melee: every melee weapon's `Range` is 0. `dao/Attack.h` (DAOLib #16) calls all of this instead of a plugin copying it: a copy of the first version of this note drifted in the turn-based mod and stopped 67 archers' shots in one ambush.

## Traps' gas and damage over time on the turn clock (read 2026-10-02)

The user's report from play: a poison gas trap in the Dalish origin's Elven Ruins (near the undead) kept hitting the party all through a turn-based fight. What it is, from the scripts and the exe:

- The trap is `TRAP_GAS_TOXIC` (traps.gda row 207): an area of effect (`EffectAreaOfEffect`, 30 s, its script `trap_gas_toxic`). A creature entering it gets `TRAP_EFFECT_GAS` (row 420): `TRAP_EFFECT_DOT`, 30 damage of nature over 30 s (`sys_traps_h`, `ApplyEffectDamageOverTime`). The area itself does no damage each pulse; the poison does.
- Damage over time is a script timer, not the effect's own clock: applying one sets the creature's `CREATURE_RULES_FLAG_DOT` and sends it `EVENT_TYPE_DOT_TICK` (1010) "next frame" (`DelayEvent(0.0)`); each tick hurts by every DoT effect on it and re-arms itself with `DelayEvent(1.5)` (`effect_dot2_h`, `CREATURE_RULES_TICK_DELAY`), until none is left. So with effects' time held (`dao::clock::Hold`) the poison never wore off, and its ticks went on every 1.5 s of real time: hurting while the player thought, and through every creature's turn.
- A lasting area's pulses are the same kind of timer: the area's update runs its script once with `EVENT_TYPE_AOE_HEARTBEAT` (74) as it starts (0x007E4B6A), and every handler of it in the game's scripts (17 of them: the spells' and talents' clouds, `orz540_gas_aoe`, the fire traps) re-arms the next with `DelayEvent` at an interval of its own (4 s for the Anvil's gas).
- `DelayEvent` counts on the world timer (`dao/Addresses.h`, "The queue as it runs"), which pause stops and the game's speed scales.

`dao::timers` (2026-10-02) holds those two kinds of event with the clock and delivers them as it is advanced, each re-armed from when it came due: a 4 s round gives the poison two or three ticks, and it wears off after 30 s of rounds. Not yet seen in play.

## A command's timed phases and the creature's clock (read 2026-10-02, not yet probed)

Asked by the turn-based mod: can a spell be handed to the engine at once and held in its conjuring phase, moved on by its caster's clock, so the installed `rules_core` interrupts it itself? What the exe says, statically:

- `CCObject::StepCommands` (0x0073D320, each update) runs the current command through 0x0073CB60, __thiscall (two of its own arguments, then the milliseconds), `ret 0xC`, handing it the creature's own milliseconds since its last update: its object timer's +8, the same figure its effects age by (`dao/Effects.h`).
- 0x0073CB60 walks the command's actions, a list reached through the creature's +0x2DC record (its +0xC, by a key lookup, 0x004DF670). Each action has a delay at +0x18, a duration at +0x14 and an elapsed time at +0xC, all in milliseconds. A delay is spent first. A timed action (duration more than 0) has the milliseconds left added to its elapsed time, never past its duration. Then the commands system (the application's +0x1AC, its +0x70) runs the action through its slot 2 (creature, action), which answers whether the command goes on.
- Once the frame's milliseconds are spent, an action is not run that frame: given 0, the current command does not move at all.
- `IsConjuring` (`CCCreature::ForScript_IsConjuring`, 0x0073C7B0) reads the same list through the same lookup (0x00697BF0) and answers the first action's first word.

So a timed phase of a command is counted in the creature's own milliseconds through one function. An intercept there, as AddElapsed is for effects, could hold a creature's current command and move it by its own clock.

Seen in play the same day, by the probe's conjure watch (90a7ce8), run by the turn-based mod's session with the mod off, 16:29 to 16:33, Lyna's Chain Lightning (10211), its command's conjure time 0 (the ability's own):

- A UseAbility's sub-actions, in order: type 15, 39 (run twice, answering 0 then 1), 50, 7, 8, 37. All are untimed (duration 0) except 7 and 37.
- **The conjure is type 7**, timed: duration 3,866 ms, the 3.87 s the animation files give for the draw-in spells. Its elapsed time counted up in game milliseconds (17 to 3,866 over 4.3 s of wall time), and it answered 1, done, when the elapsed time reached the duration.
- **The cost is taken at type 8**, untimed, after the conjure: mana 120.0 through the whole conjure, 58.0 at type 8. Type 37 is the cast, 1,707 ms.
- `IsConjuring` answers the running sub-action's type every time: 7 through the conjure.
- A talent (Dirty Fighting, 603) has the same list with type 7 at duration 0, its stamina taken at type 8 (73 to 47), and type 37 for 1,440 ms. The ogre's specials (90066, 90036) begin with a type 4.
- A conjure ended early (an ogre's knockdown about a second in, the caster's command going to ExecuteEffect) never reached type 8, and the mana stayed. `rules_core`'s interruption ends the command the same way while conjuring (`WR_ClearAllCommands`), so an interrupted spell costs nothing in the game as it is: not staged in this run (the caster's threshold was 16, the spiders bit for 7 or 8).

So holding a spell in its conjure is holding sub-action type 7's elapsed time: the caster's own milliseconds as its time is paid, none otherwise. The sub-actions after it do not run until it is done.

## Next readings

1. A fight with the next probe (queue hook, deeper stacks): what `0x0073CD10` is called on and with, from scripts and from player orders, and a `compare` of fighting against exploring that reaches the game's own frames.
2. The readers of the paused flag's value: what pause actually stops, and how the script scheduler checks it.
3. `SetGameModeMessage`'s handler and the object at `app+0x114`: the mode controller, and the full list of mode numbers.
4. The engine side of a queued command: where the head of a creature's queue is started, and the event raised when it finishes.

Each address found goes into `src/dao/Addresses.h` with the date and how it was found. Each layout learned goes into a class header under `src/dao/` like `ClientExoApp.h`.
