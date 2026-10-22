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

- `AI_DetermineCombatRound` returns at once if the creature is a non-combatant, if the game is in neither combat nor explore mode (then it queues a wait), if the creature has a current command or anything queued (`GetCurrentCommand`, `GetCommandQueueSize`), or if its AI is off (`dev/AI.md` has that switch). Otherwise it runs the creature's tactics (`_AI_ExecuteTactic` over its package's table) and queues what they choose.
- Every choice is queued through `WR_AddCommand` (27 places in `ai_main_h_2`), which checks its arguments and calls the script command `AddCommand` (`dao::script::id::AddCommand`).
- The next choice comes when that command completes: the engine sends `EVENT_TYPE_COMMAND_COMPLETE`, and `rules_core` calls `AI_DetermineCombatRound` again.

So one intercept holds the whole AI side. Intercepting `AddCommand` (`dao/ScriptHooks.h`), a mode can keep the command of any combatant whose turn it is not (a copy, `OwnedCommand`, taken from the call's argument) and answer the script as though it were queued. The creature then does nothing and gets no `COMMAND_COMPLETE`, so it decides nothing more. When its turn comes, the mode queues the kept command (`dao::script::api::AddCommand`); when that completes, the AI decides again, and the intercept keeps that too. The player's side is the explore mode's messages (`dao/Messages.h`: `PerformAbilityMessage` and its kin, `MoveCharacterToLocationMessage`), held the same way through their handlers.

The mode's own clock is `dao::OnFrame`, which runs on the scripts' thread and keeps running while the game is paused (both seen 2026-09-30), so it may call script commands directly: whose turn it is, `GetCombatState`, `GetTeam`, and `AddCommand` for the kept command.

Still to read: what else queues commands without `AddCommand` (effects, the engine's own reactions: `QueueCommand`'s other callers, `dev/TURN_BASED.md` above), and whether a command kept past a pause or an area change stays valid.

## How close a melee attack strikes from (read 2026-10-02)

The turn-based mod measured its player's dagger strikes against a giant spider landing from 2.4 m between centres, a metre closer than its own reach rule (the scripts' `AI_MELEE_RANGE`, 3.5, which is the AI's judgement of "a melee target", not the engine's reach). The engine's own, read in the exe:

- **The attack command's step** (`0x00742FD0`, ID 17685) asks whether the weapon is ranged and queues an approach, then the attack (sub-action 50). It tests no reach itself; the approach does, on its first run, and walks only if needed.
- **A creature's personal space** is `PerSpace` from `apr_base`, read once per creature (`0x00748A40`, beside `Height`, or `Height_F` for a female body) into its size record (the creature's +0x7B0, `PerSpace` at +0x60, `Height` at +0x64); the creature's vtable slot 39 (`0x006A21A0`) answers it. Elves, humans and dwarves 0.35, children 0.30, giant spiders 0.70.
- **Melee** (sub-action 4, `0x007AA560`): the target gives the attacker a place on its melee ring (slot 38, `0x006A8EE0`; every creature has a ring, not only the large monsters), and its slot 33 (`0x006A3D00`, ID 14499) judges reach: the distance between centres at most **attacker's `PerSpace` + target's `PerSpace` + 1.2 m**, **and** the attacker within 1.25 m of that place, on the ground (x and y; the centres' distance counts height). Corrected 2026-10-04: the second test was first written here as a facing test; it is the ring place. Out of reach, it walks to the place. Corrected 2026-10-18 by lib-agent-2: the walk itself ends the approach, and that is the usual end. The move update `0x007A9130` (stop distance 0.05, flags 7) answers 1 within float 2 (never set, so 0) + 0.05 m of the place in x and y. `InMeleeReach` is only an early-out: on the first run (`0x007AA749`), and before the re-path check (`0x007AA8CF`). See "Why an attacker with a ring offset is never in reach at its place" below.
- **Ranged** (sub-action 15, `0x007AA200`, a staff, bow or crossbow in the main hand: `bitm_base`'s `Type` 5 or 6, its `WeaponWield` not; corrected 2026-10-04, staves first written here as not ranged): in range when the distance on the ground (x and y) is at most the weapon's `Range` from `bitm_base` (capped at 60 m) and at least both personal spaces, and the attacker has line of sight (`0x0067C750`). Corrected 2026-10-04: the spaces are a minimum, not added to the range. Out of range or sight, it walks toward the target until within `Range` less both spaces.
- **The swing** (sub-action 1, `0x007A87C0`), after a turn to face (sub-action 39, to within 1 degree), fails the command at once (−1, 0x007A8965) when the target is out of striking reach (`InStrikingReach`, 0x006A3DC0), more than 60 degrees above or below the attacker, or out of its line of sight (corrected 2026-10-22, this said it waits); otherwise it sends `EVENT_TYPE_COMMAND_PENDING` for the scripts to resolve the blow. Corrected 2026-10-04: the 60 degrees were first read here as facing; they are elevation (`0x004EE5A0`). The melee attack's own set-up (`0x00745E10`, ID 17720) places the swing at `-(sum + 1.2)` along the attacker's facing, with 0.15 m of play.

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

## An approach: its two kinds, an ability's, and how it ends (read 2026-10-10)

Asked by the turn-based mod (DAOLib #50, #51, #52): where a ranged approach stops, whether a talent approaches as an attack does, and what the engine does with an approach that is not getting there. Read in the GOG exe; `examples/attack` logs it in play.

- **Two handlers.** Sub-action 4 (`0x007AA560`, ID 19889) approaches a place on the target's melee ring; sub-action 15 (`0x007AA200`, ID 19887) approaches to within a range. Both end their walk through the move update (`0x007A9130`), called with the flags 7.
- **An ability's approach is chosen by its `Range` in `abi_base`, not by the weapon** (the UseAbility step, `0x00743F50`). `Range` 0 at another object queues sub-action 4 with what a melee attack gives it, so the same ring, claim and `COMMAND_FAILED_NO_SPACE_IN_MELEE_RING`. Any other `Range` queues sub-action 15 with that row's `PrimaryRange` in `ranges` (capped at 60 m), a least distance of 0, and the need of a line of sight only where the ability's `Projectile` is not 0. A point on the ground queues sub-action 14. Nothing is queued where the target is the user. By this, Dirty Fighting (`Range` 0) begins with a 4 where it must walk; the note above did not record its approach.
- **After an ability's approach the ring is not asked again.** The use (sub-action 8, `0x007A7440`) tests `InStrikingReach` alone for a `Range` 0 ability at another creature, and fails the command with -1 where it does not hold. A plain attack's blow does ask again (above), so `dao::attack::ApproachOf(user, target, ability)` gives `findsRoom` where the ring has a place now and the user is in reach or the filled ring still has one.
- **The place is chosen again on every update**, not kept by index: all the attacker's places are released, then the free place nearest its own bearing is claimed; the claim is released at the command's end (`0x00746A65` → `0x006A3610` → `0x007CC060`). Read 2026-10-18 by lib-agent-2, and the rings in full below.
- **The ring is asked on every update of a melee approach at a creature**, claiming (`MeleeRingSpot`, the target's slot 38), and the walk is re-pathed when the place given has moved from the one pathed to: by any amount within 4 m of it, by more than 1.5 m further off (`0x007A50F0`; the floats at `0x00C0F6BC` and `0x00C0F6C0`). While the ring gives any place the approach walks to it, so an attacker can follow a moving place round a full ring for as long as it likes: there is no engine word for "no place to arrive at".
- **A ranged approach's stop distance** is the range less the least distance at first, and is cut by 1 m on every update that finds the walker within it without a line of sight, where one is needed. Once a cut would bring it to both personal spaces the move update ends the walk as though it had arrived, and no shot follows. From inside the range the distance so falls to the walker's own within a second (`dao::attack::Walk` follows this an update's run at a time).
  - **When the sight is tested** (read 2026-10-22 by lib-agent-2, for DAOParagonEngine #163) [C]:
    - **The first run** (sub-action int 0 unset): float 2 is set to the range less the least distance. When the command needs a line of sight (int 7), `Combat::LineOfSight` 0x0067C750 is called at 0x007AA411. If the attacker is within the band (0x007A3090) and sees the target, the run returns 1 at once (0x007AA47B), with no walk and no stop. Otherwise it paths (0x007A6CA0) and walks.
    - **Every later run** goes to the move update 0x007A9130 with flags 7 (0x007AA546), after the re-path check 0x007A50F0 and the timeout 0x007A2210. There, on each run that is not an early return:
      1. The band test 0x007A3090 is run with the current float 2 (0x007A964C). It compares the ground (xy) distance to the goal point with float 2 and the least distance.
      2. Only when the attacker is within it and int 7 is set is `LineOfSight` 0x0067C750 called (0x007A969B), or 0x0067C700 for a point.
      3. **Sight:** the walk ends as an arrival (stop 0x006A9430, return 1).
      4. **No sight:** float 2 −= 1.0 (the double 0xADE218, 0x007A9700). If the new float 2 is still above the sum of the two personal spaces (0x00695320), it is stored (0x007A9732) and the walk goes on that run. Otherwise the walk ends as an arrival, with −6 overwritten by 1 (0x007A973E, 0x007A976E).
    - **So the sight is tested while walking, not at stops, and only when the attacker is within float 2** [C]. After each failed test float 2 shrinks by 1 m. While the attacker is still within it, the next run tests again and cuts again, so from deep inside the band float 2 falls by a metre per run until it is just below the attacker's distance. From then on the walk goes on with no test until the attacker has closed by the next metre, and the test comes again at about each metre of approach [I for the pacing, from the code's flow].
    - **The band test** [C]: `0x007A3090(position, vec 1, float 2, least 0)` is true when the ground (xy) distance to vec 1 is at most float 2 + 0.05 (0xAF70B0). It has no near edge, since the least distance passed is 0. Vec 1 is the target's position, stored at the first run (0x007AA2B4..0x007AA375) [I from the stores].
    - **Float 2 persists** [C]: the first run writes it (0x007AA3D5), and after that only the cut writes it (0x007A9732). The re-path (0x007A50F0, 0x007A6CA0) does not reset it.
    - **So nothing paces the test by time, path nodes or distance walked** [C]:
      - Every run inside the band tests.
      - Deep inside the band, each run cuts 1 m, so a burst of tests runs at the update rate until float 2 is just under the distance.
      - After that a test comes once per metre of closing on the target. A walker not heading straight at the target closes more slowly than it walks [I for the fit].
    - **Seen in play** (2026-10-22, DAOLib #234's counting watch 9a12bf0, paragon-agent-2's run with eluvian and probe alone, Ishal floor 1; log read by lib-agent): walking at about 4.7 m/s, the move update's test (return address 0x007A96A0) came about every 0.32 s, about one per 1.5 m walked; creeping or nearly standing, 14–16 tests in 0.25 s. Both fit the rule.
    - **Runs that skip the test** [C]:
      - An ordinary mid-walk run goes straight to the band test (0x007A93D5).
      - These skip it: a movement record whose first word is 4 (returns 0) [I: "path search still running"]; a path request still running (returns 0 and asks again next run); and 0x007A3810 answering 2, a blocker with sub-action flag 0xC (stops and returns 1).
      - 0x007A3810 answering 0 (none in the way) or 1 (a creature in the way, handled by 0x007A3A20) still reaches the test.
    - **How often a creature's command runs** [C]:
      - The chain is the AIMaster pass 0x0064B010 → `CCCreature::Update` 0x006BE4B0 (slot 87) → `StepCommands` 0x0073D320 → `RunCurrentCommand` 0x0073CB60 → `CSSubActionHandler::Execute` 0x007A7300 → the sub-action's handler.
      - Each Update runs the current sub-action's handler once. An answer of 0 ends that Update's loop.
      - No throttle by AI level or timer lies inside Update.
    - **The throttle is the AIMaster's time slice** [C]:
      - There is one list per AI level, at AIMaster + 0x14 × level. They are walked 4, 3, 2, 1, 0 with budgets of 3000, 1200, 480, 192 and 128 µs.
      - Each level's first creature always runs. The rest run, round-robin from a cursor kept across frames, while the level's budget (less the earlier levels' overruns) lasts.
      - In loading mode (9) everything runs once.
      - So a creature runs every frame unless its level's list does not fit its budget [I for a hostile in combat being level 2].
    - **dt** [C]: the creature's `ObjectTimer` (+0x2B8) is refreshed once per Update (0x006FA1D0 at 0x006BE58D). Its +8 is the world-clock milliseconds since that creature's last Update.
- **What ends an approach otherwise.** -5, the ring gives no place on some update. -4, no path, or standing: an update shorter than 300 ms in which the walker moved under 2 cm adds the creature's own milliseconds to a counter (the sub-action's integer 1), and past 1,000 ms (`0x00C0F6C8`; doubled unless the creature's combat state is set and its byte at +0xCFC is too, not read further) the command fails. An update that moved 2 cm starts the counter again, and so does every new path, so a moving goal never trips it. -10, a timeout, only where the command carries one in its float 5: `WR_AddCommand` gives the AI's commands 10 s (`AI_COMMAND_TIMER`), 5 s or 1 s, and followers' none. It starts at the approach's first run (the creature's +0xCEC, in milliseconds) and counts down by the creature's own milliseconds (`0x006A2D10`). -7 the target gone, -8 disabled.

Seen in play 2026-10-10 (the turn-based mod's session, Awakening's Wending Wood in real time): enemy archers' approaches ended `COMMAND_FAILED_TIMEOUT` after 9.99 and 10.01 s, and one after 0.99 s. Of 67 abilities used at another creature, every one seen approaching with `Range` 0 ran step 4, and four of those failed for want of room, two before any step; every one with another `Range` ran step 15. In a second run, a hurlock with six wolves round it, `dao::attack::ApproachOf(user, target, ability)` gave `findsRoom` false for 414 abilities and every one failed for room; of the 25 it gave true, 16 succeeded and 6 failed for room later in their walk.

## The melee rings (read 2026-10-18 by lib-agent-2)

### Melee ring: places and the spot choice (GOG exe, read 2026-10-18)

Claims are checked (read in disassembly or data) unless marked inferred. Hex addresses are GOG VAs. Angles are degrees.

#### Short answers

1. A ring (0x007CB400) always has 40 places, whatever its radius or the attacker. The count is the one-entry table at 0xB230F8 (value 40; the index at ring+0x18 is forced to 0, and ring+0x18 is set to 0 at creation). Step = 360/40 = 9 degrees. Angles are world-fixed (no facing in them): place 0 starts at +Y (north) and they run clockwise seen from above (toward +X). A "place" is the arc between two consecutive points on a polyline of 40 points; place i spans 9i..9i+9 degrees from +Y, its middle at 9i+4.5. Point j is at centre + (sin(9j), cos(9j), 0) * R, with R = ring.+0x10 (already + 0.1, see below). The creature's facing only enters through the ring's centre offset and the disabled arc.
2. MeleeRingSpot (0x006A8EE0) takes the target's rings, sorts them by (distance from attacker to the ring's centre minus its radius) ascending, and tries each in that order. The first ring where a run of free places is found wins. In the ring, the run is centred on the place where the attacker already is (its angle from the ring's centre), widening alternately -1, +1, -2, +2, ... until a place whose run fits is found. The run's width is odd, 1 + 2m, m counted from the attacker's width (PersonalSpace) as below. The returned point is on the ray from the ring's centre through the chosen place's middle, at radius (ring radius_without_0.1 + attacker PersonalSpace + attacker's ring-0 MeleeRingOffset), checked against the walk mesh. Nothing free in any ring: returns false (the -5 NO_SPACE path is the caller's). Corrected 2026-10-18 by lib-agent-2: the radius includes the attacker's own ring-0 MeleeRingOffset (`0x006A8EE0` to `0x007CC2B0`), so for an attacker with an offset `InMeleeReach` is false at its place by that offset; `InStrikingReach`'s limit, PersonalSpaces + 1.2 + 1.0 (`0x00ADE218`, checked), still allows the blow.

#### 1. Ring layout (object at creature+0xB44's vector; ring pointers 4 bytes each)

Ring fields (read in 7CB400, 6AC2A0, 6A3060, 7CB240, 7CC2B0, 7CBCA0):

| offset | meaning |
|---|---|
| +0x00 | centre, Vector4 (x,y,z,w); written by 6A3060 every Update |
| +0x10 | radius. Creation: ring 0 = PersonalSpace (slot 39) + 1.2 (double at 0xAFEBC0), rings 1..9 = meleering Radius + 1.2; 7CB400 then adds 0.1 (double 0xAE7D70) in place, so the stored radius is space + 1.3 (after 7CACE0 first clamps it to at least 1e-6) |
| +0x14 | step = 360/40 = 9.0 |
| +0x18 | count index (0 -> table 0xB230F8 = 40) |
| +0x1C, +0x20 | MeleeRingDisabledStart, MeleeRingDisabledEnd (column hashes 0x6B668B36, 0x126D9F8A), degrees relative to the creature's facing |
| +0x24 | MeleeRingOffset (hash 0xED58B0C9), ring 0 only; centre = creature position (+0x120) + facing vector (+0x130..0x138) * this |
| +0x28 | MeleeRingUpdateAlways (hash 0x7ACD13EE) |
| +0x30 | claimants list (attacker ids) |
| +0x40 | vector of 40 pointers to 16-byte points (offsets from the centre, world axes) |
| +0x48 (count at +0x4C) | vector of 40 pointers to place entries |

Points (7CB400): for j = 0..39, dir = rotate (0,1,0) about Z by -9j degrees (quaternion from Euler (a1=-9j about Z, 0, 0); 4EE200 takes three Euler angles, a1 about Z, a2 about X, a3 about Y), point_j = dir * R, z = 0. Handedness: rotation is by -9j and the index finder (7CAF60) uses the signed angle from +Y that is positive toward +X, so point_j = (R sin 9j, R cos 9j, 0) (inferred from the two being consistent: place 10 = east).

Place entry (0x20 bytes, 7CB400 and 7CAED0):

| offset | meaning |
|---|---|
| +0x00 | own index i |
| +0x04 | next index (i+1, the last one's is 0): the place's end point |
| +0x08 byte | disabled (set for places in the disabled arc by 7CB240, cleared by 7CB180) |
| +0x09 byte | near-only (wall or steep slope; set by RefreshMeleeRing, cleared by 7CB1A0) |
| +0x0C | id of the perceived body standing there (-1 none; 7CC870) |
| +0x10 | float distance of that body (65505.0 when none) |
| +0x14 | -1 or an id (set -1 by body marking; a guard-margin slot) |
| +0x18 | the claim: attacker id (-1 free), written by 7CC2B0, cleared by 7CC000 |
| +0x1C | -1 or an id; the margin slot 7CB870 fills (see claim) |

Index of a position (7CAF60): d = pos - centre, normalised; angle = signed angle between d and (0,1,0) (4D4D10/4D4CC0, sign = sign of d.x), wrapped to [0,360) (4EDD70), index = floor(angle / 9) (7CAE30; floor). 7CAE80 is the ceil version. 7CAFF0 wraps an index: negative adds the count (single wrap, not a modulo).

Pseudo-code:

```
RingMakePlaces(ring):          // 0x7CB400
  ring.radius = max(ring.radius, 1e-6) + 0.1
  count = 40 ; ring.step = 360/40
  for j in 0..39: ring.points[j] = (sin(9j) , cos(9j), 0) * ring.radius
  for i in 0..39: entry{ idx=i, next=(i+1)%40, disabled=0, nearOnly=0,
                         id=-1, dist=65505.0, s14=-1, s18=-1, s1C=-1 }
PlaceOf(ring, pos): a = signedAngleFromPlusY(pos - ring.centre) mod 360 ; return floor(a/9)
Wrap(i): i<0 ? i+40 : i  (7CAFF0; indices stay within one turn in all callers)
```

#### 2. MeleeRingSpot

`MeleeRingSpot(this=target, attackerId, out, unread, claim, unread)` (0x6A8EE0, ret 0x14):

```
att = lookup(attackerId); if null return false
ring0a = att.rings.At(0); offA = ring0a[+0x24]              // attacker's own ring-0 MeleeRingOffset
(if the attacker's obstacle byte is set, lift it: 0x6A2570 / 0xA627D0, restored afterwards via 0x6A3BC0)
space = att.PersonalSpace()                                  // slot 39
ok = target.rings.Choose(out, attackerId, att+0x254, att.pos(+0x120), space, offA, claim)   // 0x7CC5B0
return ok
```

Choose (0x7CC5B0, this = target's rings):

```
if claim: Release(attackerId)                  // 0x7CC060: for every ring, 7CC000 below
keys = for each ring i: (i, |att.pos - ring[i].centre| - ring[i].radius)   // 0x7CBCA0; centre includes z; radius includes the +0.1
sort keys ascending by key (0x7CC0B0; insertion sort for <=32, stable; ties keep ring order)  // inferred stable
for (i, _) in keys: if ring[i].Claim(out, attackerId, area=att+0x254, pos, space, offA, claim): return true
return false
```

Which ring is used is therefore not decided by PersonalSpace: all of the target's rings (ring 0 = its own body at space+1.3, rings 1..9 = body parts such as a dragon's head, radius+1.3) are tried nearest-edge first. A humanoid has only ring 0 (apr_base PerSpace 0.35 for elf/dwarf: radius 0.35+1.2+0.1 = 1.65; MeleeRingOffset and disabled arc 0 for them; large creatures have MeleeRingOffset, e.g. 0.3/0.25; meleering1..9 rows have Radius, UpdateAlways, DisabledStart/End, e.g. Dragon High FR radius 0.3, disabled 180..270).

Ring Claim (0x7CC2B0, this = ring; args out, id, area, pos, space w0, offA, claim):

```
if wrapSpan(ring.disabledStart, ring.disabledEnd) >= 360: return false    // 7CAD50: end-start, +360 if negative
p   = (pos.x, pos.y, ring.centre.z)
r0  = ring.radius - 0.1
den = r0 + space + offA                       // the radius the attacker stands at
w   = space * ring.radius / den               // the half-width test distance
N   = WidthInPlaces(ring, w)                  // odd, 0x7CB9E0
half= N - 1                                   // "[esp+0x10]" ; the run is c-half/2 .. c+half/2
c0  = PlaceOf(ring, p)
dist= |p - ring.centre|                       // z flattened
nearOK = (den > dist)                         // attacker closer than the stand radius: near-only places allowed
for n = 1 .. 40:                              // 0x7CC3B3 loop; offsets 0, -1, +1, -2, +2, ...
    off = (n>>1) * (n odd ? ... )             // n=1:0, n=2:-1, n=3:+1, n=4:-2, n=5:+2 ...
    c = Wrap(c0 + off)
    if !Free(place[c], id, nearOK): continue
    if !RunFree(c, half, id, nearOK): continue          // 0x7CB010: for k=1..half/2: place[c+k] and place[c-k] free
    if StandPoint(out, c, id, dist, den, space, nearOK):  // 0x7CBA90, below
        if claim: mark_claim(c, half); return true      // see below
return false
```

Free(place, id, nearOK) (0x7CAED0): false if place.disabled(+8) != 0; false if place.nearOnly(+9) and not nearOK; then each of +0x0C (the perceived body), +0x14, +0x1C, +0x18 must be -1 or equal to id. So an attacker's own earlier claim (id in +0x18) never blocks it, and another's claim, a perceived creature's body, or a margin mark by another all do. Near-only places are allowed only while the attacker is within `den` of the centre.

WidthInPlaces (0x7CB9E0): P0 = centre + dirToMiddleOf(place 0) * ring.radius (7CB6B0: middle of the place's two points, minus centre, normalised); m = 0; for k = 1.. while k < 40 and SegmentWithin(place k's two points, P0, w): m++ ; return 1 + 2m. SegmentWithin (0x7CB790) is true if either end point is within w of P0 (4EB240) or the segment passes within w (4EB070). With chord geometry the test for k is 2R sin((9k-4.5)/2 deg) <= w. Checked example for a 0.35 humanoid on a 0.35 humanoid: R = 1.65, den = 1.9, w = 0.35*1.65/1.9 = 0.304; k=1 gives 0.130 (inside), k=2 gives 0.388 (out): m = 1, N = 3 places. Only one side is tested (symmetry assumed).

StandPoint (0x7CBA90): dir = 7CB6B0(place c) (unit vector from the centre to the middle of place c); P = centre + dir * den; z found by the walk mesh (0x7CB0E0: area+0x118 call 0xA473A0 fills z, then a slope/elevation test and the walk-grid line test 0xA46FC0 from the attacker, id/area given). If P passes, out = P (as Vector4, w from the stack, not meaningful). If it fails and nearOK: radius steps inward from den: step = (den - dist) * 0.1 floored at 0.01, max((den - dist) * 0.1, 0.01) (constants 0xC111D4 = 0.1, 0xC111D8 = 0.01; corrected 2026-10-22: it was noted as clamped to at most 0.01, inferred from the jp branch; see "Melee ring: bodies" below), P retried at radius den - step, den - 2 step ... down to dist (the attacker's current distance); the first that passes is out. Else fail and the outer loop tries the next centre place. The out point is thus the place's middle direction at the attacker-width standing radius, not the stored point and not offset along the attacker's radius beyond `space`.

Claim marking (end of 0x7CC2B0, when claim and success): h = half>>1 (half = N-1, so h = m): for k = -h..h: place[Wrap(c+k)].+0x18 = id. Then 7CB870(c-h-1, c+h+1, id): from c-h-1 walking down and from c+h+1 walking up, `[0xC111CC]+1` = 2 places each (the dword at 0xC111CC is 1, loops run while counter <= 1), sets +0x1C = id where it is -1 (a guard margin of two places each side; the "+0x1C" slot). Then id is appended to ring+0x30 claimants (0x698C30). Note that claim only marks the ring that won.

Release (0x7CC060 over all rings; 0x7CC000 per ring): every place with +0x18 == id gets -1; then 7CB1C0 (clears +0x1C == id marks, inferred from the name pattern, not read) and the id is removed from the claimants (0x745230). Choose with claim=1 releases first, so an attacker's own prior claim never blocks it and it re-picks fresh (placing it again nearest to its current angle, i.e. usually the same place).

Disabled arc (0x7CB240, run by RefreshMeleeRing before): clears +8 on all places, then arc start = ring.+0x1C + creature facing angle (signed angle from +Y, same convention), places floor(wrap(start)/9) .. + ceil(span/9) get +8 = 1. Near-only +9 flags from walls and slopes are set in RefreshMeleeRing (not re-read here). Perceived bodies mark +0x0C/+0x10 (7CC870; not re-read here).

#### Port summary

```
places: 40, 9 degrees each, place i spans [9i, 9i+9) clockwise from +Y (north), world-fixed.
ring radius = space(target ring) + 1.3 ; stand radius = (space+1.2) + attackerSpace + attackerRing0Offset
ring centre = target.pos + target.facing * target.ring0Offset   (ring 0)
ring choice: sort target rings by |attPos - centre| - radius ; first ring with a fit wins
run width: N = 1 + 2m ; m = number of places k=1.. with 2R sin((9k-4.5)/2) <= attackerSpace*R/stand
order: attacker's own place c0 = floor(angle/9), then c0-1, c0+1, c0-2, c0+2 ...
free: not disabled, near-only only when attacker within stand radius, no other id in the 4 slots
claim: id into run, 2 guard places each side; release before re-claiming
out: centre + unitdir(middle of place c) * stand radius, z from the ground, inward retry if blocked
```

#### Not read

- RefreshMeleeRing's wall/slope marking, 0x7CB1C0's exact effect, 0x7CC0B0's sort in full (insertion-sort part read at 0x7CBEB0; ties' stability inferred), 0x7CB0E0's elevation threshold (double at 0xB03BF8) and 0xA46FC0 contents, the exact inward-retry loop condition in 0x7CBA90 (read roughly), whether a ring whose pointer is missing is skipped (the dummy at 0xC38E00 is used), the sign convention of 4EE200's Euler rotation (derived from consistency, not from running it), what the unread a3 and a5 of MeleeRingSpot are.

### Melee ring: the centre, the disabled arc, and what moves when the target moves (lib-agent-2, 2026-10-18)

GOG exe, base 0x400000. "checked" = read in disassembly or data; "inferred" = not. Not seen in play.

#### Short answers

**Q3, the centre (checked).** `0x006A3060` (`CCCreature`, __thiscall, arg ring index, `ret 4`) sets `ring.centre = anchor + ring.offset * facing`. The offset is a *distance along the creature's facing vector*, not a rotated 2-D offset:
- anchor = the creature's position (+0x120..+0x128) for ring 0, and for rings 1..9 the position of a model node (a "crust" node, id = the meleeringN `CrustID`, ring +0x2C) if the model has it, else the creature's position.
- ring +0x24 is `MeleeRingOffset` (apr_base), clamped to +-0.5 (`0x00C02204`), ring 0 only; rings 1..9 have +0x24 = 0 (never written, the vector is built zeroed; inferred). `facing` = creature +0x130..+0x138 (the unit facing vector, the one `TurnToward` and `SetOrientation` use). There is no AttackFL/FR/BL/BR in it.
- `MeleeRingOffset` values: Dragon, Normal 0.3; Ogre A 0.25; golems 0.25; bears 0.5; werewolf/shriek/ash wraith 0.5; wolf, mabari 0.4; giant spider 0.22; deepstalker 0.46; Pride Demon -0.25 (behind); humans, elves, dwarves, hurlock, genlock 0; High Dragon, Archdemon 0.

**Q3, the ring.** Ring radius R = `PersonalSpace` (ring 0) or `Radius` (meleeringN) + 1.2 + 0.1 (`0x007CB400` adds the 0.1 to what `0x006AC2A0` stored as PersonalSpace + 1.2). Every ring has **40 places**, 9 degrees apart (`table 0x00B230F8[0] = 40`, step = 360/40 stored at ring +0x14). Place vectors are built once, at ring creation, and are **not rotated with the facing**: local vector i = rotate (0,1,0) about z by `-9*i` degrees (`0x004EE200`, degrees times 0.0174533, a quaternion rotation), times R. Index i is a compass direction in world axes (CW from +Y if the rotation is the usual right-handed one: i=10 east, 20 south, 30 west).

**Q3, `MeleeRingDisabledStart/End` (checked: read and direction; the direction corrected 2026-10-22, see "The angle's sign").** Column hashes: Start `0x6B668B36`, End `0x126D9F8A`, Offset `0xED58B0C9`, UpdateAlways `0x7ACD13EE`, Radius `0xAE283C02`; read at `0x006AC2A0` (apr_base for ring 0, meleeringN for 1..9). They are **degrees of an arc of the ring's 40 places that have none**, **relative to the creature's facing**, in the same sense as the place indices (9 degrees an index). Stored at ring +0x1C and +0x20 by `0x00586420`. Every `RefreshMeleeRing` call recomputes them with the current facing (below): the arc is the only thing about a ring that turns with the creature.
- The arc runs from Start to End: span = (End - Start) wrapped into [0,360) by `0x007CAD50`; start index = floor(wrap(Start + facingAngle) / 9) (`0x007CAD20` wraps by 360, `0x007CAE30` floors); count = ceil(span / 9) (`0x007CAE80`). The `count` places from there (index wrapped by `0x007CAFF0`, a modulo 40) get byte +8 = 1 ("disabled"; `0x007CB180` zeroes every +8 first, so the flag is rebuilt on each refresh).
- Start 0 / End 0 means span 0, nothing disabled (the common case, nearly every creature). Start 0 / End 360 (High Dragon, Archdemon, Spirit Apparatus Head ring 0): all 40 places disabled, so those creatures use no ring-0 place: their places are on rings 1..4 instead. Broodmother: Start 75, End -67: span 218 degrees, a wide arc disabled (a creature's back). Rings 1..4 of High Dragon/Archdemon: FR (ring 1, CrustID 40, Radius 0.30) 180..270; FL (ring 2, CrustID 41, R 0.30) 90..180; BR (ring 3, CrustID 42, R 0.35) -90..0; BL (ring 4, CrustID 43, R 0.35) 0..90. Read as clockwise from the facing these are the quadrants toward the body, which fits (inferred).
- **The angle's sign** (corrected 2026-10-22 by lib-agent-2; this said facing east gives −90, from the cross product's arguments read the wrong way round) [C]:
  - `0x004D4CC0(a, b)` signs the angle by `a.x·b.y − a.y·b.x`. `RefreshMeleeRing` (0x006AC5B9) and the place-index finder (0x007CAF60) both pass the vector first and (0, 1, 0) second, so the sign is that of `a.x`.
  - The angle is a compass bearing from +Y, positive toward +X: north 0, east +90, west −90, clockwise seen from above with +z up. The facing and the place indices run the same way: place i covers bearings 9i..9i+9, clockwise from north.
  - **The disabled arc** (0x007CB240): span = End − Start, plus 360 if negative. It closes n = ceil(span / 9) places counting up from floor(wrap360(Start + F) / 9). So it closes the bearings F + Start through F + Start + span, clockwise from the facing F.
  - **The Broodmother** (apr row 10, Start 75, End −67): span 218, 25 places closed, from 75° right of her nose clockwise round her back to 67° left. Her open side is the front, from 67° left to 75° right. She has no meleering rows, so ring 0 only.
  - **The dragons' rings 1–4** are open away from the body: ring 1 closed at F+180..270, ring 2 at F+90..180, ring 3 at F−90..0, ring 4 at F+0..90.
- **Rings 1–9's anchor** (read 2026-10-22 by lib-agent-2 with a subagent) [C]:
  - **Creation** (0x006AC2A0): the apr row is the word at `[creature+0x7B0]+0x58`. For N = 1..9 it reads tables 125..133 (a dword table at 0x00B230FC). A missing table skips to the next N; a missing row (no `CrustID`, hash 0x0CC54D0C) ends the loop. So the rings stop at the first N without a row.
  - **The node** (0x006A3060): the graphics object (creature +0x70 slot 0x60, then `[[obj+0x18]+4]`), then its slot 0x7C, 0x00547D20, the first node of that crust id in its crust map. A `CrustID` of 40–43 is an MMH `crst` node's `MMHNodeCrustHookID`, not a `crusts.gda` row.
    - High Dragon and Archdemon: 40 is `CrustHook33` under `RightHandSpawn`, 41 `LeftHandSpawn`, 42 `RightFootSpawn`, 43 `LeftFootSpawn`, animated dummies under `GOD`.
    - The Spirit Apparatus Head's 40 is a static `CrustNode01` under `GOB` at (0.03, 1.71, 0.03).
  - **The position is the animated node's:** the node's slot 2 (0x0055DDF0) takes column 3 of its world matrix (+0x6C, +0x7C, +0x8C, +0x9C), divided by w (0x004CB4D0). It is the position as of the graphics' last update, not recomputed; that it can lag the game thread is [I].
  - **The fallback:** with no node, no crust map or a `CrustID` of 0, the anchor is the creature's position (+0x120). The centre is the anchor plus the ring's offset times the facing, and the offset is 0 for rings 1–9.
  - **Re-read:** `RefreshMeleeRing` (0x006AC510) re-reads every ring on each call, unless the creature's byte +0x1AC is set.
- **A ring disabled whole** (start 0, end 360, as the dragons' ring 0) [C]:
  - 0x007CC2B0 returns false at once for a span ≥ 360 (0x00AF2670, at 0x007CC459).
  - The choice (0x007CC5B0) sorts all the rings by distance minus radius, with no arc test, and claims on each in turn. So that ring is skipped and the next is tried.
  - If every ring fails, `MeleeRingSpot` returns false and sub-action 4 (0x007AA560) returns −5 (0x007AA654), the command's failure for no room on the ring. There is no fallback to sub-action 15; the weapon's or ability's range chose sub-action 4 earlier.
  - So the High Dragon and the Archdemon are attacked in melee only at rings 1–4, the hands and feet, and the Spirit Apparatus Head at its one ring.
- **Not read by the exe:** apr_base's `AttackFL`, `AttackFR`, `AttackBR`, `AttackBL`, `AttackRing`, `AttackWidth` and `AttackReach`. Their hashes occur nowhere in it [C].

**Q3, the tables.** apr_base: `PerSpace`, `MeleeRingUpdateAlways` (u8; ring +0x28), `MeleeRingDisabledStart/End` (f32), `MeleeRingOffset` (f32; centre offset along facing, above). `AttackFL/FR/BR/BL` (centre angle), `...Width`, `...Ring` (0..4) and `AttackReach` are **not read by ring creation or the centre** (they are the attack-direction/reach data for the big monsters; their reader was not looked for). meleering1..9 (tables 125..133; the row ID = the creature's *apr_base row ID*, `[creature+0x7B0]+0x58`; ring N is made only if the row exists, and it stops at the first ring N without one): columns `ID`, `Label`, `CrustID` (the model node the ring is centred on, ring +0x2C), `Radius` (f32, the ring's radius before +1.2/+0.1), `MeleeRingUpdateAlways`, `MeleeRingDisabledStart`, `MeleeRingDisabledEnd`. Rows: meleering1: IDs 66 High Dragon FR, 81 Spirit Apparatus Head (CrustID 40, R 0.5, no arc), 82 Archdemon FR; meleering2: 66, 82 (FL, CrustID 41); meleering3: 66, 82 (BR, CrustID 42, R 0.35); meleering4: 66, 82 (BL, CrustID 43, R 0.35); meleering5..9 are empty. All rows have UpdateAlways 1 except 81. So only the High Dragon, the Archdemon (and the wounded one, which shares the row with 82) and the Spirit Apparatus Head have more than ring 0. A dragon (High) ring 0: PerSpace 3, UpdateAlways 1, Start 0 End 360, Offset 0. Ogre A: PerSpace 0.95, UpdateAlways 1, Offset 0.25, no arc. A human (any human/elf/dwarf): PerSpace 0.35, UpdateAlways 0, Offset 0, no arc, so R = 1.65 (0.35 + 1.2 + 0.1) and ring radius for an attacker standing at "1.9 m centre to centre" is inside... see the note on the claim below. Also: Dragon, Normal PerSpace 1.4 offset 0.3, no arc, no meleering row; Broodmother PerSpace 1.8, UpdateAlways 1, arc 75..-67.

**Q4, what moves with the target (checked from the disassembly; callers of Update/RefreshMeleeRing from master's Addresses.h note).**
1. **Every frame**: `RefreshMeleeRing(ringIndex)` (`0x006AC510`) recomputes **every ring's centre** (`0x006A3060` for each ring, only skipped when creature byte +0x1AC is nonzero; not further read) and, for the ring asked, recomputes the **disabled flags** with the current facing (`0x007CB240`). So a claimed place's world point follows the target's position and (through the offset along facing) its facing; the *place indices stay fixed in world directions* (not rotated by the target turning). A target that turns in place moves the centre only by Offset * (change of facing), and rotates only the disabled arc (it can switch places on or off); it does not carry the places round. A creature with Offset 0 and no arc (every humanoid) has a ring that does not react to its turning at all.
2. **A place's world point** (`0x007CB620`, entry i): centre + (local[i] + local[next(i)]) / 2 (the midpoint of the two neighbouring ring points; `0x00A38A20` adds the centre, `0x004D47E0` divides by 2.0), at R*cos(4.5 deg) from the centre.
3. **The claim is re-chosen on every call that sets claim** (checked). `ApproachMelee` (`0x007AA560`, at `0x007AA63x`) calls the target's slot 38 `MeleeRingSpot(attackerId, out, attackerPersonalSpace, claim = 1, 0)` on **every update** of the approach (it starts after finding the target, in the path-update part of the sub-action). With claim set, `0x007CC5B0` first calls `0x007CC060` which releases **every place this attacker holds on every ring** of the target, then picks again from scratch: rings sorted by `(distance from the attacker's position to the ring centre) - R` (`0x007CBCA0`, nearest first); in each ring (`0x007CC2B0`) the **first candidate is the place whose compass bearing from the ring centre is nearest to the attacker's own bearing** (`0x007CAF60`, bearing of the attacker minus the centre, then `0x007CB9E0` turns the wanted angle into an index), and then goes 0, -1, +1, -2, +2... places away (the loop at 0x007CC3B3 / 0x007CC446; corrected the same day); a candidate is taken if it is free (`0x007CAED0`: not disabled (+8), not near-only (+9) unless the attacker is inside the radius, and no other holder in +0xC/+0x14/+0x18/+0x1C) for the whole run of places the attacker's width covers (`0x007CB010`), and passes the fit test (`0x007CB0E0`: slope under 60 degrees, no unwalkable cell or other creature's stamp, `0x00A46FC0`). The claim writes the attacker's id into holder +0x18 of the run and adds the id to the ring's claimant list (+0x30/+0x38, `0x00698C30`). So it is **not a claim on a fixed place index**: the attacker re-asks and gets the free place nearest to *where it now stands*, so it stays on "its" place while it keeps its bearing and drifts to the next free place if the target or it moves round. With claim = 0 (`InMeleeReach`-style queries, `dao::attack::ApproachOf`) nothing is released or written.
4. **The re-path rule** (`0x007A50F0`, 4 m / 1.5 m) compares the place just given with the one the walk was pathed to, so a place that moved because the target moved, or because the attacker was given a different place, re-paths; master's TURN_BASED.md line 183 is right and needs only one added fact (see "Correction").
5. **When the attacker arrives and swings** (corrected 2026-10-18 by lib-agent-2: the walk's arrival is the usual end, `InMeleeReach` only an early-out, see the subsection after this section's pseudo-code): the approach handler ends at once when `InMeleeReach(attacker, place)` holds (it has just claimed the place in that same update), so no further `MeleeRingSpot` calls are made in the approach sub-action; the turn (39) and the swing (1) do not call it (the swing tests `InStrikingReach`, `0x006A3DC0`, on the attacker's own point and the target's slot-29 point). The attacker's claim is **not released on arrival**: it stays in holder +0x18 and in the claimant list through the swing, so the ring keeps being refreshed (claimants > 0), and the place stays held. If the target moves during the swing, nothing re-chooses it until another approach runs.
6. **Release** (checked): the shared command end (`0x00746900`, which then queues COMMAND_COMPLETE) calls `0x006A3610(this = attacker, command target)` at `0x00746A65`: it looks the target object up and calls `0x007CC060` on the target's rings (`creature+0xB44`) with the attacker's id (`0x007CC000` per ring; `0x00745230` is the generic list-remove it ends in, which takes the id out of the ring's claimant list at +0x30). It runs for **every end of the command**, including a clear, a failure and a death of the attacker (the command is ended). A ring with no claimants and UpdateAlways 0 is **emptied** on its next refresh (`0x007CBDD0`; `MeleeRing::Clear`); with UpdateAlways 1 (ogres, dragons, Broodmother, tentacles) it stays active and filled with perceived bodies. I did not look for a release on the *target's* death or on the target changing (the command changes target only by ending).
7. **Bodies**: for every creature the target perceives (its +0xC18 set, excluding itself) `0x007CC870` (called from the refresh) marks the places that creature's body covers with its id and distance, and resets holders +0x14/+0x18/+0x1C to -1 over them, which **clears any claim there**: a perceived creature standing on a claimed place pushes the claim off (the claimant's next approach update re-claims elsewhere). Also each refresh marks a place near-only (+9) when `0x00A471E0` (a wall test from the ring's point) or a slope over 60 degrees (`0x00B03BF8` = 60.0) cuts it off.

#### Pseudo-code a port can follow

```
// constants
PLACES = 40; STEP = 9.0 deg; EXTRA = 1.2 + 0.1;
// ring creation (from the appearance row), once
ring0.R = PersonalSpace + EXTRA; ring0.offset = clamp(MeleeRingOffset, -0.5, 0.5)
ring0.start = DisabledStart; ring0.end = DisabledEnd; ring0.always = UpdateAlways
for n in 1..9: if meleeringN has row appearanceId:
    ringN.R = Radius + EXTRA; ringN.crustNode = CrustID; ringN.offset = 0
    ringN.start/end/always from the row ; else stop
local[i] = rotZ(-i*STEP) * (0, R, 0)            // i in 0..39, never changes

// every frame, per creature C, per ring N
anchor = C.pos;  if ringN.crustNode and C has node: anchor = world(node.origin)
ring.centre = anchor + ring.offset * C.facingVector          // 3-D, facing is the unit vector
ring.disabled[*] = 0
span  = wrap360(ring.end - ring.start)
first = floor(wrap360(ring.start + facingAngleDeg) / STEP)    // facingAngleDeg: sign to check
for k in 0 .. ceil(span/STEP)-1: ring.disabled[(first+k) % 40] = 1
if ring.claimants.empty() and not ring.always: ring.clear(); return
mark near-only places (wall or slope > 60 deg); mark places under perceived creatures' bodies
    and drop any claim there
placePoint(i) = ring.centre + (local[i] + local[(i+1)%40]) / 2

// attacker A approaches target T each update (ApproachMelee)
T.releaseAll(A.id)                                    // claim = 1
for ring in T.rings sorted by (|A.pos - ring.centre| - ring.R):
    start = indexOfBearing(A.pos - ring.centre)       // nearest place to A's bearing
    for k in 0,1,2...: cand = start + (k odd ? +(k+1)/2 : -(k/2))   // alternating
        if free(cand over A's width in places) and fits(placePoint(cand)): claim(A.id); return placePoint(cand)
return NO_PLACE (-5)
if InMeleeReach(A, place): end approach; else walk to place (re-path 4 m / 1.5 m rule)   // corrected 2026-10-18 by lib-agent-2: the walk's arrival ends it too, see below

// command end (any reason): T.releaseAll(A.id)
```

### Why an attacker with a ring offset is never in reach at its place (GOG exe, read 2026-10-18, lib-agent-2)

Claims are checked (read in disassembly or data) unless marked inferred. Not seen in play.

#### Short answers

1. **The approach does not end on InMeleeReach.** It ends when the walk arrives: the move update `0x007A9130` answers 1 when the attacker is within (sub-action float 2 + 0.05) in x and y of the sub-action's goal (vec 1 = the ring place), and ApproachMelee returns that 1 (`0x007AA7D1` only special-cases -9). Float 2 of sub-action 4 is never set (AttackStep sets floats 0, 1, 3, 5 only; the sub-action is zero-made), so it is 0: arrive within 0.05 m of the place. InMeleeReach is only an early-out (first run at `0x007AA749`, and before the re-path check on later updates at `0x007AA8CF`). So the wolf stands exactly on its place (2.40 m from a human), InMeleeReach is false there, and it does not matter.
2. **WithinBand (`0x007A3090`)** is a flat distance test: d = sqrt(dx^2 + dy^2) of (b - a) (z is multiplied by 0 and ignored); true when max(0, arg4 - 0.05) <= d <= arg3 + 0.05 (the double 0.05 at `0x00AF70B0`; arg4 is the "least", 0.0 in every call seen, so the lower test is vacuous). InMeleeReach calls it (attacker pos, place, 1.2f, 0.0f): the attacker within 1.25 of the place in x,y.
3. **Walk goal = the place itself.** `0x007A6CA0` reads vec 1 (the place), floats 0 (=0), 1 (=0), 4 (=0). Float 4 == 0 selects `0x007A31C0`, which returns goal + unit(attacker - goal) * float1; float1 = 0, so the goal is the place unchanged (the extra `0x0073E370`/`0x0073E7F0` calls in it only test and fill a scratch buffer, not the output). Float 4 != 0 picks `0x007A5450`, the away goal of `CommandMoveAwayFromObject` (PATHFINDING.md, "The away goal"; a random angle only when the mover stands on the centre), not used by AttackStep (corrected 2026-10-22). The move update hands the controller the stop distance 0.05f (stored at [controller + 0x90]). Sub-action 4's float 3 is the attacker's PersonalSpace and is only the "unread" argument passed to MeleeRingSpot; it is not a stop distance. No offset by the attacker's MeleeRingOffset is applied to the goal beyond the one already inside the ring's stand radius.
4. **-9 is not arrival.** The move update returns -9 when the path search answered 6 (path action required, e.g. a door; `dev/PLACEABLES.md` lists -9 PATH_ACTION_REQUIRED). `0x007AA7E0` then (if the `0x006A7E10` value equals the saved target id and the target's slot-4 flag is zero) re-asks MeleeRingSpot with claim = 1 (`0x007AA802`), re-runs `0x007A6CA0`, sets int 3 = 1500 (0x5DC) and returns 0 (keep going); else it returns -9 as the failure.
5. **After the approach**, sub-action 39 turns, then the swing (sub-action 1) tests InStrikingReach (`0x006A3DC0`, this = attacker, arg target, only if the target's type byte (+0x14) is 6): false when the flat-then-3D distance from the attacker to the target's slot-29 point (vtable +0x74, called with the attacker's position) exceeds PersonalSpaces(attacker, target) + 1.2 + 1.0 (double `0x00ADE218` = 1.0). That is 1.0 m more than InMeleeReach's. False fails the command at once (−1, COMBAT.md "The swing"). Slot 29's point and PersonalSpaces (read 2026-10-22 by paragon-agent-2's helper, checked by lib-agent-2, for DAOParagonEngine #169):
   - **PersonalSpaces(a, b) is the sum of the two objects' slot 39 (+0x9C), with no 1.2 in it** [C, `CCObject::PersonalSpaces` 0x00695320]: for a non-null a, `a.slot39() + [double 0x00ADE210]` (that double is 0.0), for a non-null b `+ b.slot39()`. The 1.2 (0x00AFEBC0) is added by the callers (`InStrikingReach` 0x006A3DC0, the approach, the placement). Slot 39 of `CCObject` is 0x0083A430 (`fldz`: 0.0) and `CCItem`, `CCTrigger` and `CCPlaceable` inherit it (vtables 0x00B0EE9C, 0x00B1C61C, 0x00B1FEF4, 0x00B1C084); `CCCreature`'s is 0x006A21A0 (`CCCreature::PersonalSpace`, apr_base `PerSpace`, EFFECTS.md:565). So a target that is not a creature has a space of exactly 0.0, by `CCObject`'s default slot.
   - **Slot 29 (vtable +0x74) is not the position for a creature** [C, `CCCreature` slot 29 = 0x006A4EA0, `__thiscall(out vec3, point)`, `ret 8`]: it first copies the creature's position (+0x120, +0x124, +0x128) into `out`; then it asks the melee rings (creature +0xB44) `0x007CC1A0(rings, point)` for a ring and, when one comes back, overwrites `out` with the ring's first three floats (the ring's centre, +0x00, TURN_BASED.md:203). `0x007CC1A0` sorts the rings by `|point - centre| - radius` ascending (`0x007CBCA0`, the sort of "keys" in the placement) and returns the first whose disabled arc `0x007CAD50(+0x1C start, +0x20 end)` is below 360.0 (float 0x00AF2670): the arc `end - start` folded into [0, 360) with the double at 0x00AF2658 added when it is negative; so a ring is skipped only when its whole circle is disabled; returns null when none. So the point is the centre of the nearest usable ring (ring 0's centre is the position plus the facing × `MeleeRingOffset`, or a model node's), else the position. Not a nearest surface point. `CCPlaceable`'s slot 29 is 0x0074D580, its aim point (PLACEABLES.md, "The placeable's aim point"); `CCObject`'s (items, triggers) is 0x0069A090 (the plain position +0x120..+0x128).
   - **Consequence for `InStrikingReach`** [C]: its distance (xy) from the attacker to that point at most `PersonalSpaces + 1.2 + 1.0` is to the nearest usable ring's centre. For a creature with `MeleeRingOffset` 0 (humans, elves, dwarves: offset 0, TURN_BASED.md:430) it is the position; the wolf's 0.4 shifts it 0.4 m along the wolf's facing. Ring 0's centre is set by `RefreshMeleeRing` (0x006A3060) each update.

#### The wolf on a human, numbers

apr_base (ghilana, patch003 copy): Wolf ID 49 PerSpace 0.45, MeleeRingOffset 0.4; Human 15 / Elf / Dwarf PerSpace 0.35, offset 0. (Wolf, Blight 0.45 / 0.3; Dog Mabari 0.45 / 0.4.)

```
ring0 radius (stored)  = 0.35 + 1.2 + 0.1  = 1.65         r0 = 1.55
place stand radius den = r0 + 0.45 + 0.4   = 2.40  (from the human's ring centre = its position, offset 0)
InMeleeReach limit     = 0.45 + 0.35 + 1.2 = 2.00  and place within 1.25 of the wolf (xy)
wolf at its place      : distance 2.40 > 2.00  -> InMeleeReach FALSE (by 0.40)
approach result        : arrives (within 0.05 of the place) -> returns 1, done
InStrikingReach limit  = 2.00 + 1.0        = 3.00  -> 2.40 <= 3.00 -> TRUE (if slot 29 is the centre)
human vs human, for contrast: den = 1.55 + 0.35 = 1.90 = the reach limit 1.90: InMeleeReach holds on the place.
```

So the exe does leave the wolf "out of InMeleeReach" at the place, by exactly the attacker's offset, and that is by design: the approach never asks InMeleeReach after the walk. (The offset exists because the wolf's own ring centre is its position + 0.4 along its facing, the snout; den puts that point on r0 + space. Inferred reason.) The swing then passes InStrikingReach with 0.6 m to spare. For any attacker: strike limit - stand = (ps_t + 1.2 + 1.0 + ps_a) - (ps_t + 1.2 + ps_a + off_a) = 1.0 - off_a, so any offset below 1.0 (apr_base's largest is 0.5) is inside striking reach.

#### When the approach would not arrive (loop and counters)

- Re-path rule `0x007A50F0` (4 m / 1.5 m) and the claim re-ask live in the "later update" path (`0x007AA86D` onward); on a true result it asks MeleeRingSpot(claim = 1) again (`0x007AA969`), and if the ring answers false returns -5 (`0x007AA654`). While the place stays, nothing re-asks.
- Standing counter: in `0x007A9130` (`0x007A9785` onward): if the attacker moved under 0.02 (double `0x00B211A8`) in an update shorter than 300 ms (0x12C), the update's ms are added to int 1; past 1 or 2 x [`0x00C0F6C8`] (1000) it returns -4. Any real move resets it. This is what a port that never lets the walk arrive (because it waits for InMeleeReach) ends up in: the wolf reaches the place, stands, the counter passes 1000 ms, -4. That is the peer's stall.
- Timer: int 3 = 1500 ms is set on the first run, on a re-ask in the -9 branch, and in the later-update path (`0x007AA8DD`..: it takes `0x00A5A220`(3) minus the creature's ms at [+0x2B8]+8; when it goes negative it sets a flag and refills 1500). It is the re-ask throttle of the "later update" path (inferred: its consumers beyond this were not read). The command timeout (float 5, -10) is separate.
- Early-out on later updates (`0x007AA893`..`0x007AA8D7`): when the target's slot 9 object (vtable +0x24) is non-null and its byte +0x638 != 1, InMeleeReach(attacker, vec 1 as stored) true returns 1 at once. What byte +0x638 is was not read.

#### Pseudo-code a port can copy

```
// sub-action 4 made by AttackStep: f0 = 0, f1 = 0, f2 = 0 (unset), f3 = attacker slot39, f4 = 0, f5 = timeout; bool1 = 1, int2 = 1
ApproachMelee(T, A, sub):                  // 0x7AA560
  place = T.MeleeRingSpot(A.id, claim=1)   // false -> -5
  sub.goal = place                         // vec1 (f1 = 0 so no offset)
  if firstRun:
      if T.InMeleeReach(A, place): return 1
      sub.timer3 = 1500
  else:
      if InMeleeReach(A, sub.goal) [gate above]: return 1
      if repathRule(sub.goal vs asked place):      // 0x7A50F0
          place = T.MeleeRingSpot(A.id, claim=1);  if !place return -5
          sub.goal = place
  r = MoveUpdate(A, sub, stop=0.05, flags=7)       // 0x7A9130
  if r == -9 and sameTarget: re-ask ring (claim=1), set goal, timer3 = 1500, return 0
  return r                                          // 1 arrived, 0 going, -4 stuck/no path

MoveUpdate: request path to sub.goal (stop dist 0.05); each update
  arrived = flatDist(A.pos, goal) <= sub.f2 + 0.05     // WithinBand(.., f2, 0)
  if arrived return 1
  stand counter: if update < 300 ms and moved < 0.02: sub.int1 += ms; if int1 > 1000 (x2 unless combat byte): return -4
                 else sub.int1 = 0

Reach(T, A) = dist3(A.pos, T.pos) <= A.ps + T.ps + 1.2  &&  flatDist(A.pos, place) <= 1.25   // early-out only
Strike(A, T) = dist(A.pos, T.slot29Point(A.pos)) <= A.ps + T.ps + 1.2 + 1.0                 // the swing
```

#### Not read

- Slot 4 / slot 9 of the target, the byte +0x638 and `0x006A7E10`; the exact meaning of the int 3 refill in the later-update path; `0x0073EA80` return codes beyond 5, 6, 8 (-4, -9, -4 as above) and state 1 (path finished, `0x007A9356` branch) and what the controller does when its path ends short of the goal (the next update re-requests; inferred); `0x0073E370` / `0x0073E7F0` purpose in `0x007A31C0` (they do not change its output).
- Whether sub-action floats default to 0 when never set: read (`0x00A5A250` answers fldz when the index is beyond the list), checked.

Corrections to this section's earlier text, 2026-10-18 by lib-agent-2: the approach's end is the walk's arrival; the stand radius includes the attacker's ring-0 MeleeRingOffset; -9 from the move update is PATH_ACTION_REQUIRED (a door), not arrival; "within 1.25 of the place" for InMeleeReach is right (checked 2026-10-18 by lib-agent-2: the band passed is 1.2f at `0xAE7540`, and WithinBand adds 0.05, d <= band + 0.05).

### Melee ring: bodies, the stand point's retry, the standing counter and the release (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169)

GOG addresses.

- **Body marking `0x007CC870`** [C]: called once per perceived creature from `RefreshMeleeRing` (`0x006AC510`, call at `0x006AC959`), `this` the ring (`MeleeRings::At`, `0x007CCC30`), arguments the creature's id, its position (+0x120 vector) and a float from the creature's vtable +0x9C (slot 39, the PersonalSpace of the ring table's layout). It first calls `0x007CBBF0(id)`, which sets every place whose +0x0C is that id back to id -1 and distance 65505.0 (`0x477FE100`), then `0x007CB200(id)`. So the marks of that creature are rebuilt from nothing each refresh.
- **Where the body counts** [C]: d is the distance from the ring's centre (+0x00) to the creature's position (via `0x007CAF30`). If d is under the ring's radius (+0x10, space + 1.3 as stored) and the offset has length, the point used is moved out along the offset to exactly the radius; otherwise it is the position itself. `0x007CC730(out list, point, width)` then answers nothing unless the 2-D (x and y) squared distance from the centre to that point is strictly under (width + radius)^2 (`0x004EB340`). So a creature counts when its centre is within the ring's stored radius plus its own PersonalSpace of the centre, and inside the radius it is treated as standing on the radius. The bound uses the ring's stored radius (+0x10), not the radius less 0.1 and not the creature's own ring offset.
- **The run of places** [C for the flow, I for the geometry tests' meaning]: `0x007CC730` takes the place under the point's bearing (`0x007CAF60`), then walks up from it (`0x007CAFF0` wraps the index) adding each next place, and then down from it, each direction stopping at the first place that has the near-only byte (+0x09) set, or for which `0x007CB700(place, point)` is true (a sign test: a dot product of the place's direction from `0x007CB6B0` against the normalised vector from the centre to the point, true when negative), or for which `0x007CB790(place, point, width)` is false (three segment tests `0x004EB240`, then `0x004EB070`: the body's circle against the place's triangle between the centre and the place's two end points). So the run is where the body's circle meets the place wedges, not the 1 + 2m run an attacker's claim takes. The two helpers' exact geometry was not read further [I].
- **What marking does to a place** [C]: for each place of the run, if its id (+0x0C) is -1 or the distance (+0x10) is more than this body's, it takes this id and distance; in every case +0x14, +0x18 and +0x1C are set to -1 (a claim on the place is gone, as "7. Bodies" above reads). Then `0x007CB920(first - u - 1, last + u + 1, id)` (u = (count - 1) >> 1 of the run, the run's middle place found again by `0x007CAF60`) writes the id to +0x14 on the places beyond each end that still have -1 there, the number each side being max(`0x00C111CC`, `0x00C111D0`) + 1; both constants are 1 (read from the data), so 2 places each side.
- **The stand point's inward retry `0x007CBA90`** [C] (corrected 2026-10-22: the step was noted as clamped to at most 0.01, inferred from the jp branch). The disassembly (`0x007CBB30` to `0x007CBB59`) compares 0.01 (`0x00C111D8`) with step = (den - dist) * 0.1 (`0x00C111D4`) and replaces the step with 0.01 when 0.01 is the greater (at 0x007CBB44..0x007CBB55, with 0.01 below the step, `fstp st(0)` drops the 0.01 and keeps the step): the step is at LEAST 0.01, max(0.1 * (den - dist), 0.01), not at most. The retry is made only when the byte argument at +0x20 (nearOK) is non-zero, after the first point at radius den failed `0x007CB0E0`. Then r = den - step; it gives up (returns false) when r <= dist (`TEST AH, 0x41` after the compare, at `0x007CBB79` and `0x007CBBDC`), else tests the point at r and, if that fails, r -= step and compares again.
- **The standing counter's doubling `0x007A9130`** [C] (see "What ends an approach otherwise"): the limit is `0x00C0F6C8` times 1 when the first dword of the creature's combat data (`*(creature+0xC60)`, decompiled as `param_1[0x318]`) is non-zero AND the byte at creature +0xCFC is non-zero, else times 2 (decompiled: `iVar10 = 1; if (*(int *)param_1[0x318] == 0 || (char)param_1[0x33f] == 0) iVar10 = 2`). So the existing "doubled unless the creature's combat state is set and its byte at +0xCFC is too" is right. The byte +0xCFC is written by `0x006A42E0` (also read by `0x006A5200`, which uses it to skip a capsule test): it is cleared, then set to 1 when the object from the creature's `CCObject::vfunction68` has bit 0x20 set in its flag word (that object's vtable +0xA4) and (the creature's +0xD8 object is null or its word +0x58 is not 0xE), or else when the object whose id is at creature +0x32C (`GetObjectById`) has that bit 0x20. What bit 0x20 and the +0xD8 word mean was not read [I].
- **Release** [C, a .text scan for call and jmp]: `0x007CC060` (all rings, per attacker id) has exactly two callers, `0x006A3610` (the command end; its one caller is `0x00746A65`) and `0x007CC5E5` (inside the choice `0x007CC5B0`, the release before a claim); `0x007CC000` (per ring) has one caller, `0x007CC096` inside `0x007CC060`. No code path releases a place on the target's death or deletion. A dead target ends each attacker's command through the swing's -7 (`COMBAT.md`, "The swing", point 1), and that command end is what releases.
- **The facing the centre and the arc use** [C, already in this file at "Q3, the centre"]: the creature's +0x130 vector as stored; `Vector::FromAngle` = (-sin a, cos a, 0) (`script_commands/posorient.md`) is how an engine angle becomes that vector.

## Next readings

1. A fight with the next probe (queue hook, deeper stacks): what `0x0073CD10` is called on and with, from scripts and from player orders, and a `compare` of fighting against exploring that reaches the game's own frames.
2. The readers of the paused flag's value: what pause actually stops, and how the script scheduler checks it.
3. `SetGameModeMessage`'s handler and the object at `app+0x114`: the mode controller, and the full list of mode numbers.
4. The engine side of a queued command: where the head of a creature's queue is started, and the event raised when it finishes.

Each address found goes into `src/dao/Addresses.h` with the date and how it was found. Each layout learned goes into a class header under `src/dao/` like `ClientExoApp.h`.

## Game modes, conversations and clears: read 2026-10-18

Read in the GOG exe and the scripts by lib-agent-2 (with subagents) for DAOParagonEngine's Harrowing. Not seen in play. The key claims below were checked in the disassembly.

### The game mode's change and EVENT_TYPE_GAMEMODE_CHANGE (60)

**`SetGameMode` (0x006414A0; mode, force, data) only files a request.**
- A mode whose slot 10 (+0x28) answers true goes into a single base slot (+0x8C), a later request overwriting a waiting one. These modes are Explore, Combat, Dead, FlyCam and Fixed.
- Every other mode is pushed on a stack (+0x9C): GUI, Dialog, Cutscene, Loading, Movie, CharGen, PartyPicker and Pregame.
- This corrects `src/dao/GameModes.cpp`'s comment that slot 10 is asked "whether to refuse".

**The controller takes a request on its update (0x00640C60).** It takes one only while the current mode is a base mode, is finished (+0x2C), or can be interrupted (+0x30).
- The stack is taken first in, first out (a new request is inserted at the front, the back is taken); the base slot only when the stack is empty.
- Dialog and Cutscene can't be interrupted, so requests wait until they end.
- The new mode can refuse there (+0x24, then slot 5 +0x14; dialogue's ambient start is such a refusal).
- **A request for the current mode with force 0 is dropped**: no switch, no event. The script's `SetGameMode` (530) passes force 0. Dialogue, cutscenes and explore after a load pass 1.

**The switch (0x00641D30), in order:**
1. The old mode's leave (slot 7).
2. The mode numbers are stored: +0x78 the new, +0x88 the old.
3. The new mode's enter (slot 6).
4. **EVENT_TYPE_GAMEMODE_CHANGE (60)** goes to the module, if there is one, through `AIMaster::DeliverNow` (0x00641F72), so **run at once**:
   - creator and object 0 are the module;
   - **int 0 is the new mode, int 1 the old** (0x00641F3E, 0x00641F4E).

It is the only builder of event 60, so every real switch sends it, whoever asked: the scripts, the engine's conversation and cutscene starts, a load, the death screen. An overlay is a full switch each way: Explore→GUI, then GUI→Explore. A base change asked for under an overlay shows up only as overlay→new base when the overlay ends.

**Who asks for which mode:**
- **The engine:**
  - 4 at a conversation's start (0x00697765);
  - 5 from the loading code (0x006C94F6);
  - 0 with force 1 after a load (0x006C8F91);
  - 8, Pregame, at start-up;
  - 10, Movie, and 12, PartyPicker.
- **The scripts' commands:**
  - `SetGameMode` (530), any mode;
  - `BeginSlideshow` 4, `LoadCutscene` 5, `ShowDeathScreen` 2, `StartCharGen` 11.
- **Combat (1) and explore (0) are the scripts' alone:**
  - `rules_core.nss`:1085–1113, a party member perceiving a hostile, sets GM_COMBAT;
  - `module_core.nss`:310–356, `EVENT_TYPE_DELAYED_GM_CHANGE`, goes back to explore every 2 s until no hostile is perceived.
  - Both go through `WR_SetGameMode` → SET_GAME_MODE (1021) → `module_core`:370 → the command.

`module_core.nss`:185–296 handles event 60: it returns early for a new GUI, FLYCAM, INVALID or PREGAME, and sets the party's combat state from new == COMBAT (:246).

A cutscene and the mode controller (requests refused during a cutscene, Cutscene to Loading directly, a transition filed from a cutscene's end script) are in `dev/CUTSCENES.md`'s "A cutscene, the area transition after it" (read 2026-10-18 by lib-agent-2).

### What runs during a conversation

**Dialogue mode doesn't pause.**
- Its enter (`GameModeDialog` slot 6, 0x0068FE40) keeps whether the game was paused, and unpauses (`SetPaused(0,0)`).
- Its leave (0x006910D0) pauses again only if it was paused before.
- It clears no commands, ends no combat and changes no hostility.

**The world pass runs.** The controller's update (0x00641F90) runs the world pass (0x0064B010: `AIMaster::Update`, then every object's `Update`) when the mode's slot 18 (+0x48) answers true, and pause (app +0x100) isn't set. Dialog, Explore and Combat answer true; Cutscene answers false.

**The conversation's participants are frozen.** In mode 4, `CCCreature::Update` (0x006BE4B0) returns at once for a creature in the conversation (0x006BE500–0x006BE51C, checked: 0x0068CC30 on the dialogue mode).
- So their commands, effect ageing, regeneration and heartbeat don't run.
- The participants are the conversation's owner and the player's character (0x007F19E0), or a staged cutscene's actors.

**Everyone else goes on, without perception.** Commands (attacks, casts, projectiles), effect ageing (`AgeEffect`, no mode check), regeneration and events all run. Perception (0x006B89B0) is gated by the mode's slot 19 (+0x4C), which Dialog, Dead, Cutscene and GUI answer false, so no new APPEAR events come.

**So a hostile outside the conversation keeps attacking the frozen hero, in vanilla as in Paragon.** Ambient barks stop (slot 21 +0x54 false in Dialog).

**`BeginConversation` answers 1 when the request is queued, not when the conversation starts.** It checks only that the object exists and isn't dead. The 2026-09-30 sighting in a fight (1 answered, nothing started) wasn't found on this path; the likely cause is that no start line's condition passed, which plays as an empty ambient conversation and refuses the mode.

**A second conversation waits for the first, and is tested again then** (read 2026-10-21 by lib-agent-2 for DAOParagonEngine's Harrowing). Neither `BeginConversation` (0x006976B0: object +0x2C5 set, not dead) nor `UT_Talk` tests for a conversation already pending or playing, and `SetGameMode`'s replace-in-place case is for loading (mode 9) alone. So a second request is queued behind the first. Dialog is neither a base mode nor interruptible, so the request is taken only when the first conversation has finished. Force 1 escapes the same-mode drop. Dialogue's check (slot 5, 0x0068FDA0) then calls `Conversation::Start` (0x007F2210), which picks the start line at that moment. With no passing line, the request is refused and dropped silently, with no switch and no event 60, and the controller takes the next request in the same update (0x00640DD3–0x00640DEC). Otherwise the second conversation plays after the first. In vanilla Sloth's surrender asks twice, and the second plays as a bark. `bhm600cr_sloth.utc` has `SURR_INIT_CONVERSATION` = 0, so `creature_core`'s COMBAT_END (:549–594) only sets `SLOTH_SURRENDERS` (flag 2), and that flag's SET case in `bhm600pt_harrowing.nss` (:169–177) makes the first `UT_Talk`. But `bhm600_sloth.dlg` line 0's action clears flag 2, and the script's SET switch doesn't test the value, so the case calls `UT_Talk` again during the first conversation. When that request is taken, line 0 fails, and line 1 (flag 23) and its only reply 80 have empty text and no cut, so they are passed over (`dev/PLOTS.md`, "The skip"). Of 80's children, line 3 (flag 18, set by line 0's reply 114) is reached: "I've taught the mouse…", which is ambient. `Conversation::Start` tests ambient on the line `PlayLine` reached (+0x3C, 0x007F2342–0x007F2352), not on the start list's entry, so it answers false: the line plays as a bark and dialogue mode is refused, with no event 60.

### A soft ClearAllCommands mid-swing: the blow lands

`ClearAllCommands(o, nHardClear)` (0x0065BDE0) calls `ClearAllCommands(nHardClear == 1, 0)` (0x0073C930), so only 1 is hard.
- Queued commands that aren't static are always freed, with no COMMAND_COMPLETE.
- The current command (`ClearCurrentCommand(-2, 0, hard, 0)`, 0x0073C730) is cleared if it isn't static and either the clear is hard or the running sub-action's byte +4 (not interruptible) is 0. Otherwise it is left alone completely: nothing is deferred.

The melee attack (sub-action 50, 0x007A6150) adds the turn (39, +4 = 0), then **the swing, sub-action 1, with +4 = 1** (0x007A63E2, checked).
- So a soft clear before the swing cancels the attack.
- A soft clear during the swing does nothing: the animation, the impact (0x006A6230 → 0x006A6080 → ATTACK_IMPACT, which tests no command, only the hand record) and the rest go on, and the command ends with 1.
- A hard clear ends it (-2). Every command's end (0x00746900) zeroes the hand records, so a later impact frame raises nothing.

### `ResourceToString` keeps the extension

The compiler keeps a resource constant whole: `bhm600pt_harrowing.ncs` holds `bhm600im_valor_sword.uti` as a 0x60 constant. `ResourceToString` (0x00655E90) copies the resource's text unchanged into a string, so `ResourceToTag`'s cut at "." holds. A resource that comes from an engine command holds what that command made; not checked.

### Valor's damage: no factor missing (the scripts' numbers)

- **Melee damage** (`combat_damage_h.nss` `Combat_Damage_GetAttackDamage`:245–379):
  - weapon damage, plus Strength's bonus;
  - the difficulty's bonus, for the player's rank only;
  - critical ×1.5+;
  - minus armour (70% fixed, 30% random, less AP);
  - plus `PROPERTY_ATTRIBUTE_DAMAGE_BONUS`;
  - × `autoscale.gda FDamageScale`[rank], **only when the damage is above `difficulty.gda DmgScalingThresh`** (Normal 7; :363);
  - at least 1.
- The engine has no damage factor of its own and no native rank or level scaling.
- **Neither side regenerates health in combat** (`REGENERATION_HEALTH_COMBAT_DEFAULT` 0).
- **Valor** (`bhm600cr_valor.utc`: Rank 1, Immortal, a warrior with an iron longsword and kite shield) has Rank 2 in the area's instance, which overrides the template (above).
  - Autoscale makes him level 1 (area 62's MinLevel 1), STR 15, CON 14, **110 health**, attacking every 1.9 s.
  - Against a level-1 mage in robes: about 5.9–8.8 a hit, about 72% to hit, about 2.8 health a second (inferred).
- **The mage** has 85 health; Arcane Bolt is about 32 spirit damage on a 6 s cooldown. Valor surrenders below 2 health (`creature_core`:145–150), so a mage should win narrowly by the scripts (inferred).
- **The fight is optional.** Valor's `GEN_WILL_MED` line (WIL ≥ 15) gives the reward with no fight. The "attack" line (VALOR_ATTACKS) heals him and clears the surrender while he stays Immortal, so it is unwinnable by design (inferred).
