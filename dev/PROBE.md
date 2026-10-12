# The probe, and how to read a session

`src/plugins/probe` is a plugin that changes nothing in the game and records what it does. Built 2026-09-29. Install it with `.\tools\deploy.ps1`, play, close the game, and read the files it left in `Documents\BioWare\Dragon Age\daolib\` (under OneDrive on this machine). Each launch starts the files afresh, so copy a session you want to keep.

## What it records

| File | What |
|---|---|
| `probe.log` | Every pause and its caller, the pause key, every command queued on a game object (`QueueCommand`, with the object's and the command's classes, and the chain of callers above it), and what the probe attached. It has wall-clock times. |
| `probe-calls.tsv` | Once a second: the pause state and game mode, and how many times each script command and message handler ran. |
| `probe-samples.tsv` | Per ten-second window: where the game's threads were (`eip`) and the return addresses on their stacks (`stack`), from about 60 samples a second of every thread; and the same again (`usamples`, `ueip`, `ustack`) for just the samples taken while a thread ran the game's update. |

The **census** (`Census.h`) counts calls to the roughly 870 functions the two tables name: every implemented script command and every message handler with a known method. Each is detoured to a ten-byte stub that bumps a counter and jumps on, so it costs two instructions per call and works for any calling convention. Detours refuses a function too short to patch, and `probe.log` lists any it refused. Handlers marked `trivial` in `dev/message_handlers.csv` are left out: a one-line stub like `xor al, al; ret 4` is shared by the compiler with every function of the same body, so its count says nothing about the handler.

The **sampler** (`Recorder.h`) suspends each thread for the moment it takes to copy its registers and 16 KiB of stack, then resumes it. The first session copied 2 KiB, which missed the game's own frames whenever a thread was deep in the graphics driver, so its comparisons showed mostly rendering. It allocates and logs nothing while a thread is suspended: a frozen thread might hold the heap lock. A value on the stack counts as a return address if it points into the exe's code just after a call instruction (`dao/core/CallSite.h`). So the samples show not only the leaf a thread was in, but the functions that called it, which is where the game logic is.

The game's logic runs as `Eclipse::UpdateTask` on some of the engine's worker threads, among fifty-odd threads that mostly wait or draw. In the first sessions that thread made up under 2% of all samples, so comparisons over all of them showed rendering and little else. Since 2026-09-30 the recorder also counts, apart, the samples with the update task's frame on the stack. `compare --update` compares those alone.

Times in the two `.tsv` files are milliseconds since the probe loaded. The first line of each gives the local time that was, to line up with `probe.log` and with what you remember of the session.

## Reading it

`tools/probe_report.py` does the arithmetic. It needs Ghidra's function list in `game\ghidra\functions.tsv`. Export it with the MCP server's `eval_python`, or with the server stopped:

```powershell
.\tools\ghidra-mcp.ps1 -Query "export-functions C:\project\DAOLib\game\ghidra\functions.tsv"
```

Then:

```powershell
python tools\probe_report.py timeline                  # every 10 s: paused, mode, calls, busiest commands
python tools\probe_report.py pauses                    # every SetPaused, and the function that asked
python tools\probe_report.py calls 120-300             # what ran between 120 s and 300 s
python tools\probe_report.py compare 400-520 60-300    # functions on the stacks while fighting, not exploring
python tools\probe_report.py compare 400-520 60-300 --update   # the same, over the game's update only
```

The timeline shows the stretches. A fight shows up as a burst of combat script commands (`GETCOMBATSTATE`, ability and AI commands) and, with auto-pause on, a `SetPaused` from a new caller. `compare` with the fight first and exploring second ranks functions by how much more often they were on a stack while fighting. The top of that list is combat code, most of it unnamed until then. Each name found goes into `src/dao/Addresses.h` like any other.

## Script checks

Since 2026-09-30 the probe also checks the library's fields against the game (`ScriptChecks.h`, `dev/LIBRARY.md`). It detours `CVirtualMachineCommands::InitializeCommands`, and as soon as the engine has filled the command table, it puts one shadow handler in the slot of each getter in `dao/generated/ScriptFields.h`. When a script calls one, the shadow peeks at the object argument (pops it and pushes it back), calls the engine's handler, peeks at the answer the same way, looks the object up as the getter does, and reads our field from it. The script gets exactly what the engine gave it. `DAOLIB_PROBE_SCRIPT_CHECKS=0` turns this off.

In `probe.log`:

- `script checks: 18 of 18 getters shadowed` once at start-up, or why not.
- `script check GetTeamId (807): first call, ...` the first time a getter's answer and our field agree.
- A warning with both values for each of the first five mismatches of a getter.
- Since 2026-09-30, a check of the object itself on every call that found one: its id at +0x10 is the id asked for, its type byte is the getter's class's, and the engine's cast, from the object as `GetGameObjectById` returns it, gives the same pointer as the typed lookup. A warning for each of the first five objects that fail it.
- Every minute, with the recorder's health line: per getter called so far, its calls, matches, mismatches, calls on an id that named no object of its class, and calls where the engine's handler failed. Then how many getters were called and how many matched every time.

Since 2026-09-30 it also shadows commands of the other value types (`kPeeks` in `ScriptChecks.cpp`). `GetTag`, `GetName`, `GetPosition` and `GetLocation`: after the engine answers, the answer is popped with the library's wrapper, pushed back, popped and pushed again, and the two compared; a location's area must name an area. The first three values of each are logged. `GetEffectType` and its six siblings: the effect argument is popped into a copy of the probe's own and pushed back first, then the engine's answer is compared with the library's accessor on the copy. Ids that name no object of a getter's class are logged too, the first five per getter: they say what scripts pass for no object. Also since 2026-09-30, events and arrays: `GetCurrentEvent`'s answer must be a `CScriptEvent` and round-trip the same; `GetEventType`, `GetEventCreator` and `GetEventTarget` are compared with the library's accessors on the probe's copy of the event; `GetTeam`'s `object[]` must hold only members of the team the script asked for, `GetEffects`' `effect[]` only `CGameEffect`s, and `GetNearestObject`'s `object[]` only live objects, each popped with the library's `Array` and pushed back.

Since 2026-09-30 the probe also watches the engine's event queue (`Events.h`): a detour on `AIMaster::ProcessEvent` counts every event it delivers, by the queue's id and, for script events (id 10, a `CScriptEvent`), by `EVENT_TYPE_*`, and logs the first three of each kind with target and caller. The counts go out with the recorder's health line. The question it answers is whether `EVENT_TYPE_COMMAND_COMPLETE` goes through the queue, where a turn-based mode could hold it.

The probe also calls commands through the library's `Invoke` (`dao/generated/ScriptApi.h`) from inside its shadows, on the scripts' thread: `api::GetObjectType` on every object a getter check finds, against the scripts' bit for its type byte; `api::GetStringLength` on every `GetTag` and `GetName` answer, against its length; `api::GetPositionFromLocation` on every `GetLocation` answer, against the location's position. Their counts go out with the rest as `invoke check` lines.

A getter no script calls in a session stays unchecked. The census saw `GETCOMBATSTATE` run through every fight; the placeable getters want chests and locks, and `GetMapPinState` the world map.

## Loading screens

Since 2026-10-10 the probe times every step of a loading screen (`Loading.h`, DAOLib #64). The loading game mode runs one state a frame, each a machine in numbered steps (`src/dao/Addresses.h`, "The loading mode's states"): state 5 is an area transition within the area list in memory, state 4 a swap to another list, state 3 a save, 2 and 8 a game starting or a save loading, 6 what follows each of them. A detour on the mode's update reads the state and its step before the engine runs them and times the call. In `probe.log`:

- `loading watch: detoured GameModeLoading::Update at 0x006c9680` once at start-up.
- `loading: begins in state 7, for area "<tag>" at waypoint "<tag>"; ...` at a loading screen's first update, with the rooms of the area being left.
- `loading: state 5 (area transition) step 10: 1 update(s), X ms inside, Y ms in all` as each step ends. "Inside" is the time the engine spent in the step's updates, work that holds the frame; the rest of "in all" is frames going by while something else is waited for, a background load most often.
- `loading: done in X ms, ... longest (state/step): ...; now in area "<tag>", R rooms, L loaded, N loading` on the first frame out of the loading mode: the whole screen, its five longest steps, and the rooms of the area arrived in.

The question it answers is which steps a loading screen's time is in: the rooms and models of the area arrived in, which could be loaded ahead of a door, or the saving and rebuilding of an area list, which could not.

**Seen in play**, 2026-10-10 (GOG, the play tester's session, the file cache warm from earlier that day; seven loading screens, each with its `begins` and `done`, 131 lines in all). A door's screen begins in state 7, a save's load in state 2. Times are the `done` line's; the game then takes another 0.12 to 0.23 s to its first frame out of the mode, by Eluvian's clock.

| Loading screen | In all | Where it went |
|---|---|---|
| A save loading into Redcliffe village | 3.6 s | state 2, 23 steps, the longest 0.9 s |
| Village to its tavern, the same area list, twice | 0.7 s, 0.5 s | state 5 step 10 (the old area's rooms out, the new layout in, the party placed): 0.39 s, 0.24 s |
| Tavern back to the village, twice | 2.4 s, 1.5 s | state 6 step 4, the area's script on `EVENT_TYPE_AREALOAD_PRELOADEXIT`: 0.85 s, 0.70 s in one update. State 6 step 6, waiting on rooms: 1.17 s, 0.45 s |
| Village to the castle by the world map, another list | 1.8 s | state 4: the old list out 0.18 s, the new list's files 0.46 s, its areas built 0.25 s, the layout 0.18 s |
| Castle to the village by the world map | 2.1 s | state 4: the list's files 0.32 s, its areas built 0.62 s; the area's script 0.21 s |

So what loading areas ahead of a door could have ready, the rooms of the area arrived in, was 0.45 to 1.2 s of the longest screen and next to nothing of the others; the rest is the area's own script, the saving and rebuilding of an area list, and a few frames either side. Into the tavern the door's opening took a second before the loading began, longer than the loading. Not seen: a cold file cache, a long session, the larger areas later in the game.

**By function** (DAOLib #68, since 2026-10-11). The step times say how long, not what. While a loading screen is up the recorder samples the thread that runs the loading mode's update about every millisecond (the timer set to one for the screen), and keeps its samples by state and step, apart for inside the update and between two. Per step it keeps the function the thread was in, the module where that was outside the game's code, and its first four callers in their order. With it, as often, it samples three of the engine's own threads, known by the names they give themselves (`recorder: thread T is DragonAge::Render` in `probe.log`): the render thread, the streaming thread and the file thread, since what a loading waits for is theirs to do; and it notes each thread's processor time over the screen, with its name. Nothing else is sampled while a screen is up: a pass over every thread took half of a short screen's time (in play, 2026-10-11). One block per screen goes to `probe-loading.tsv` (`Recorder.h` has the rows), and `probe.log` gets `recorder: loading screen N: S samples of its thread over M ms` with the recorder's own health: its turns, the longest gap between two, and the samples it could not take. To read it:

```powershell
python tools\probe_report.py loading                 # every loading screen of the session, step by step
python tools\probe_report.py loading --screen 3      # one of them
```

For each step with a hundredth of the samples or more: its share and about how long that is, the functions the thread was under (on its stack: time in them and what they called) and in (its leaf, or for a sample outside the exe, in the system or a library, the game's function that had called out). Then the same for the other threads while busy. A thread suspended a thousand times a second runs a little slower, so the times of a sampled screen are a little long; the shares are what it is for. Not seen in play yet.
