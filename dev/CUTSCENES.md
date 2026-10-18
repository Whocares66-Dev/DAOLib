# Cutscenes

How the engine plays a cutscene, read in the exe; the files themselves are `dao/core/Cutscene.h`'s (DAOLib #170).

## As the engine plays them

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe and `script.ldf`, for DAOParagonEngine's mage origin (#22). Builds on `src/dao/Dialogue.cpp`'s note (LoadCutscene `0x0066D230`, PlayCutscene `0x00651240`, read 2026-10-01 and seen in play: loading is playing, `SkipCutscene` returns the game to exploring with the hero where she stood). "Read" is what the code shows; "inferred" is marked.

### LoadCutscene (647, `0x0066D230`)

- Declared `void LoadCutscene(resource rCutscene, object oPlayer, int bPlayImmediately, string[] aTargetTagsToReplace, object[] aReplacementObjects, int bActorsHaveWeapons)`. Pops in that order: resource, object, int, array, array, int. Read.
- It finds the resource by its full name (`Resources::Find(name, 3)`; a name without `.cut` finds nothing, seen in play 2026-10-01) and checks its header (`0x00A38EA0`). Read.
- If the application's byte +0x208 is set (inferred: the "skip cutscenes" option), it does not play: it reads the file's `CutsceneEndScript` (field 5001) and runs that script at once with `ExecuteScript(script, OBJECT_INVALID, 0, -1)`. Read.
- Otherwise it builds the cutscene (`0x00692150`: a 0x250-byte player made by `0x007E26E0`, wrapped in `GameModeCutsceneData`) and, if that loaded (its state +0x94 is not 6), sets the game mode to 5, cutscene, at once (`GameModeController::SetGameMode(5, 1, data)`). Read.
- The player fails to load (state 6), and nothing happens, when the file's `CutsceneAreaRequired` (5019) is set and there is no current area (`0x007E1910`). Read; matches play 2026-10-01: a cutscene of an area not loaded did nothing.
- `oPlayer` and `bPlayImmediately` are popped and not used. `bActorsHaveWeapons` is popped and not used either: each replacement is made with "weapons" 1. Read.
- Replacements: for each index i, `0x007E3390(aTargetTagsToReplace[i], aReplacementObjects[i], 1)` finds the cutscene's actor with that tag (`0x007D0510`) and hands the replacement object to it (`0x007E3330` → `0x007E30B0`). Read. As `script.ldf` says, the replacement gives the actor its appearance ("the objects will be used to take the appearance only of the creatures"): the cutscene's actors are objects of the cutscene's own (`0x007E0D20` makes one per actor in the file, 0x560 bytes, `0x007DFC60`), not the area's creatures. Inferred from the ldf and the separate actor objects; `0x007E30B0`'s body not read.

### PlayCutscene (648, `0x00651240`)

- Pops its object and does nothing: loading is playing (read 2026-10-01).

### Entering cutscene mode (`GameModeCutscene` slot 7, `0x00692980`)

- Takes the pending cutscene (+0x50 → +0x4C), saves the camera (the mode's +0x70..+0xA0 into its own +0x0..+0x1C) and starts the player (`0x007CDC90`, `0x006C4AF0`, `0x006C2890(0, 1)`, `0x007CD0D0`). Read.
- Calls the application's `0x005D5540(1)`: it stops timers (`0x00A35DE0`, `0x00A35A10`) and tells the graphics (slot 0x6C) and an object (slot 0x60) a cutscene began; the world timer at +0xD4 is saved on leaving. Inferred: the game's clock is held during a cutscene. Read in outline.
- Calls `0x006A0D40` on each party member (`0x006300B0`, party at `0x005D57C0()`). Not read what it does; inferred: stops their movement or actions.
- Opens the cutscene GUI (`*(app+0x158)` slot 0x88) and sets the input mode "GameModeCutscene". Read.
- Whether the area's creatures that are not actors are hidden is not read; the file's `CutsceneShowAreaDynamics` (5009) is a likely switch (inferred).

### The file (`0x007E1910`, the player's load)

- Fields read from the `.cut` (GFF4, `dev/gff4_fields.csv`): `CutsceneAnimatic` (5008), `CutsceneAreaRequired` (5019), `CutsceneStageResRef` (5017), `CutsceneRunTime` (5000, seconds), `CutsceneFPS` (5016, default 30), `CutsceneEndScript` (5001), `CutsceneBlendtree` (5007), the actors' list (field 0x1450) and more (fields 0x1393..0x139D). Read.

### The end, and the end script (`GameModeCutscene` slot 8, `0x00692450`)

- When cutscene mode is left, slot 8 restores the camera, takes the player's end script (`0x00691CF0`, the file's `CutsceneEndScript`), deletes the player, and, if the script is not empty, runs it at once: `ExecuteScript(script, OBJECT_INVALID, 0, -1)`. Read. So the end script runs with no OBJECT_SELF of the cutscene's choosing (OBJECT_INVALID passed; what the VM falls back to is not read) and no event; the generic one, `gen00cs_cutscene_end`, calls `CS_CutsceneEnd()`, which reads module variables. There is no `EVENT_TYPE_CUTSCENE_END` in `script.ldf`.
- Then `0x005D5540(0)` restarts the timers, the cutscene GUI is closed (slot 0x8C), the input mode restored, and `GameMode` slot 8 runs. Read. The mode the game returns to is the controller's (not read); in play it was exploring (2026-10-01).

### Skipping (`GameModeCutscene::HandleMessage_SkipCutsceneMessage`, `0x00691D60`)

- Sends a telemetry line once, then, unless the player is already ending (`0x007CF2E0`), sets the player's skip flag (+0x1B4) and calls `0x007CD0F0`. Read. Inferred: the player ends at once and the mode is left, so the end script runs the same way as at a natural end (slot 8). Actors are the cutscene's own objects, so the area's creatures stay where they were; seen in play 2026-10-01: the hero where she had stood.

### Classes

- `GameModeCutscene` (vftable `0x00B0E76C`), `GameModeCutsceneData` (`0x00B0E6BC`, holding the player), `GUI::GUICutscene`, and the actors' actions `EC::Cutscene::Actor::Action*` (PlayAnimation, MoveSpline, JumpToPlace, ChangeVisibility, DrawWeapon, CameraFOV, PlayMovie, Light*, FrameBufferEffect and kin, vftables `0x00B231C8`..`0x00B23630`). Read.

### A conversation line's cut

A line's cut plays in dialogue mode, not this one: built as `LoadCutscene`'s player is, no game mode asked for, the line's length its `CutsceneRunTime` less 0.3 s, and never for a Skip line. `dev/PLOTS.md` has it.
