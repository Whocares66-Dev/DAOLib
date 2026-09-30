# The probe, and how to read a session

`src/plugins/probe` is a plugin that changes nothing in the game and records what it does. Built 2026-09-29. Install it with `.\tools\deploy.ps1`, play, close the game, and read the files it left in `Documents\BioWare\Dragon Age\daolib\` (under OneDrive on this machine). Each launch starts the files afresh, so copy a session you want to keep.

## What it records

| File | What |
|---|---|
| `probe.log` | Every pause and its caller, the pause key, every command queued on a game object (`QueueCommand`, with the object's and the command's classes and the caller), and what the probe attached. It has wall-clock times. |
| `probe-calls.tsv` | Once a second: the pause state and game mode, and how many times each script command and message handler ran. |
| `probe-samples.tsv` | Per ten-second window: where the game's threads were (`eip`) and the return addresses on their stacks (`stack`), from about 60 samples a second of every thread. |

The **census** (`Census.h`) counts calls to the roughly 870 functions the two tables name: every implemented script command and every message handler with a known method. Each is detoured to a ten-byte stub that bumps a counter and jumps on, so it costs two instructions per call and works for any calling convention. Detours refuses a function too short to patch, and `probe.log` lists any it refused. Handlers marked `trivial` in `dev/message_handlers.csv` are left out: a one-line stub like `xor al, al; ret 4` is shared by the compiler with every function of the same body, so its count says nothing about the handler.

The **sampler** (`Recorder.h`) suspends each thread for the moment it takes to copy its registers and 16 KiB of stack, then resumes it. The first session copied 2 KiB, which missed the game's own frames whenever a thread was deep in the graphics driver, so its comparisons showed mostly rendering. It allocates and logs nothing while a thread is suspended: a frozen thread might hold the heap lock. A value on the stack counts as a return address if it points into the exe's code just after a call instruction (`dao/core/CallSite.h`). So the samples show not only the leaf a thread was in, but the functions that called it, which is where the game logic is.

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
```

The timeline shows the stretches. A fight shows up as a burst of combat script commands (`GETCOMBATSTATE`, ability and AI commands) and, with auto-pause on, a `SetPaused` from a new caller. `compare` with the fight first and exploring second ranks functions by how much more often they were on a stack while fighting. The top of that list is combat code, most of it unnamed until then. Each name found goes into `src/dao/Addresses.h` like any other.
