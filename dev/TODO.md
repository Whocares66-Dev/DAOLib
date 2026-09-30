# What to reverse engineer next

Dragon Age's gameplay rules are mostly scripts and data. An attack's result is worked out in script (`rules_core.nss` handles `EVENT_TYPE_COMMAND_PENDING` through `Combat_HandleCommandAttack`, and `combat_h.nss` and `combat_damage_h.nss` hold the hit and damage formulas), and costs, ranges and damage values are rows of the data tables. Both are open to modders already: the toolset compiles scripts, and the override folder takes scripts and tables. Rebalancing and new formulas are done there, and DAOLib need not reach for them.

DAOLib earns its place where those cannot reach:

- **Engine state scripts cannot read or write.** No script command gives an effect's duration, for one.
- **Engine behaviour scripts cannot change.** The camera, input, time, the UI, movement and the command queue are examples.
- **Where script overrides do not compose.** A mod that changes `combat_h.nss` must recompile the 28 scripts that include it and ship them all, and two such mods cannot both be installed. A native plugin can change what those scripts do at the boundary instead (their script commands, their events), and several plugins can do it at once.

Each item says why it needs native work, what is known, and when it is done. Most valuable first. A finished item moves to `dev/LIBRARY.md`'s table, and gets an example in `examples/` when it is something a plugin would use (`dev/PLUGINS.md` says how).

## 1. Effects: duration and time left

**Read, not yet seen in play (2026-09-30):** `CGameEffect::Duration`, `Remaining`, `SetRemaining`, and the clock every temporary effect ages on, `dao/Effects.h` (`Hold`, `Advance`, `OnAgeing`), through one engine function, `AddElapsed`. Regeneration read too, and held and advanced with the effects by `dao/Clock.h`. Left: seeing both in play; giving `OnAgeing` the object the effect is on (AgeEffect, `0x007A0B10`, has it as its first argument, and AddElapsed does not); an example.

- **Why native:** scripts read an effect's type, flags, creator, ability and duration type, but no command gives its duration or how long it has left. Buff and debuff displays, and the turn-based mod's conversion of durations to rounds, need both. Cooldowns are not on this list: `GetRemainingCooldown` and `SetCooldown` are script commands, callable through `dao::script::api`.
- **Leads:** `CGameEffect` (`dao/Structures.h`, fields from the effect getter commands); `Engine_ApplyEffectOnObject`'s handler, which takes the duration; the object's effect list (`SEffectsHolder`); whatever expires effects each frame.
- **Done when:** `CGameEffect::Duration()` and `Remaining()` read in play agree with what the effect was applied with, and fall as the game runs.

## 2. Combat, at the scripts' boundary

**Done in code, not yet seen in play (2026-09-30):** `CommandCall::SetInt`, `SetFloat` and `SetObject` change a command's arguments before it runs, and `examples/damage` halves the party's damage by intercepting `SetCreatureProperty`, which `effect_damage_h.nss` calls as the one place health drops. Left: seeing it in play; a killing blow, which the scripts turn into a death effect instead.

- **Why native:** composable changes to combat without replacing `combat_h.nss`. The scripts apply damage and effects through script commands (`Engine_ApplyEffectOnObject`, `ApplyEffectOnObject`), which `dao::script::InterceptCommand` can already see and change. What is missing is reading and changing the effect in flight (its type, its floats: a damage effect's amount) and a worked example.
- **Leads:** the effect getters and setters (`GetEffectFloat`, `SetEffectEngineFloat`); `dao/ScriptHooks.h`'s `CommandCall::Structure`; `effects_h.nss` and `combat_damage_h.nss` for which floats mean what.
- **Done when:** `examples/damage` changes the damage the party takes in play, and composes with another plugin doing the same.

## 3. Input and key bindings

- **Why native:** scripts cannot read keys. A plugin polls `GetAsyncKeyState` today (the turn-based mod does), which ignores the game's own bindings and the controls screen.
- **Leads:** `GameModeController`'s `RawInputMessage` handler (`0x0063FE70`); `KeyBindings.ini`, whose bindings name messages ("SaveGameMessage -1 2"), sent through the engine's registry of message factories (`0x0078D1A0`); the data tree's `InputMappings`.
- **Done when:** a plugin registers an action, the player binds it in the controls screen, and a callback runs on the key.
- **Read so far (2026-09-30):**
  - How a key reaches the game. `KeyBindings.ini` gives each action its two keys (`TogglePause_0=Keyboard::Button_Space`). `Eclipse::GroupedInputMappings` turns a key into its action, and the action, in the current game mode, names a message as text: `SaveGameMessage -1 2`, `UserTogglePauseMessage`, `SelectPartyMemberIndexMessage 0 1`. The texts and their modes (by class name, `GameModeExplore`, `GameModeFixed`) are registered at start-up by one large function (`0x006DC900`, through `0x00891010`, `0x00890C60` and `0x0088F810`), and the message is built by name through the registry's factories and delivered.
  - What a plugin can do now. It can react to any action the player has bound by watching the message the action sends (`dao::messages::OnMessage`); the pause key's `UserTogglePauseMessage` was seen in play.
  - `RawInputMessage` is no key feed. It carries a raw input's code at `+4`, and is sent while the controls screen is capturing a key to bind (its handler, `0x00894ED0`, compares the code with Escape, Enter and the mouse buttons). It was never seen in play otherwise.
  - The actions' names in the controls screen are `inputmap.gda`, 188 rows of an id and a string reference.
- **Left:** a plugin's own action. Either register one (a row of `inputmap.gda` for its name, the registration call for its keys and message, and a message the plugin can take: an existing class it intercepts), or have a binding name a message only the plugin knows and catch it where the registry's factory is asked for it by name (`0x0078D1A0`).

## 4. Time: game speed and the clock

- **Why native:** scripts cannot change how fast the game runs. The turn-based mod wants fast AI turns, and other plugins want slow motion.
- **Leads:** the update's frame time (`Eclipse::UpdateTask`, `EclipseGame::Update`, `dao/Frame.h`); `GetLowResTimer`. The Flash UI's `SetTimeScale` scales only the menus' animations (read 2026-09-30, `dev/GUI.md`), and `GetCreatureSpeed` and `SetCreatureSpeed` are empty in this build.
- **Done when:** `dao::SetTimeScale(0.5)` slows animation, movement and effect timers together in play.

## 5. World queries

- **Why native:** line of sight, path length and what lies between two points are engine questions. Scripts have a little of this, but not the pathfinder's path or a raycast.
- **Leads:** `World::Raycast` (`0x006497A0`, arguments read, `dao/Addresses.h`); the pick's hits (`dao/GameModes.h`, `PickHit`); explore mode's walkable test (`0x0070C840`) and the area's check (`0x0073DE70`).
- **Done when:** `dao::world::Raycast(from, to)` and a path length agree with what the game does in play.

## 6. Dialogue

- **Why native:** scripts run inside a conversation's callbacks, and conversations are authored in the toolset. Changing a conversation at run time (a line, a choice, a condition) is out of their reach.
- **Leads:** `GameModeDialog`; the dialogue's messages (`SelectResponseMessage`); the conversation resource, a GFF file (`tools/gff4.py` reads the format).
- **Done when:** a plugin sees each line and choice as it is shown, and can add or hide a choice.

## 7. The engine's messages, typed

- **Why native:** every message class a plugin can read is a hook point (`dao/Messages.h`). 11 of the 296 have fields in C++.
- **Leads:** `dev/messages.csv`, with each class's size from its clone (`tools/messages.py`); each handler's reads.
- **Done when:** the messages for abilities, the party, inventory and dialogue are typed, and each field has been seen in play.

## 8. A plugin's own strings

- **Why native:** a plugin's text today is English in code. A talk table of its own, beside the game's, would give it the player's language.
- **Leads:** `CTlkTable` (`dao/Tlk.h`, `0x004E4990`); `tools/tlk.py` reads the format; how the game loads an add-on's talk table.
- **Done when:** `dao::tlk::String` finds a plugin's string by its id, in the player's language.

## Also

- **Inventory and items.** Much is scriptable already (`GetItemsInInventory`, `CreateItemOnObject`). Native work waits for a need scripts cannot meet.
- **Party and follower AI.** Members, tactics presets.
- **Saves.** Why one quicksave in play wrote its files without `SaveGame::Write` (`dev/SAVES.md`).
- **The examples.** `src/plugins/example/` checks many parts at once; each part moves to a focused example in `examples/`, and writing each reviews its API.
