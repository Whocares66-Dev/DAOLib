# Script command handlers: cutscenes (DAOParagonEngine #27)

Read 2026-10-18 by lib-agent-2 in the GOG exe; not seen in play. Each section points to the note that describes the function first, with the handler's edge cases that note lacks.

### LoadCutscene (647)
- **Handler:** 0x0066D230 (own handler, registered 0x00678D8E); builds via 0x00692150 and `GameModeController::SetGameMode(5, 1, data)` (as `dev/CUTSCENES.md` has it).
- **Described in:** `dev/CUTSCENES.md` ("LoadCutscene (647)"), `dev/PLOTS.md` (a line's cut is built as this one's player is).
- **Does:** finds the `.cut` by its full name, and either runs its end script at once (the skip option, app+0x208) or starts cutscene mode, with the replacement actors' appearances given.
- **Edge cases:** see the note: a name without `.cut`, or a file needing an area while none is loaded, plays nothing and does not fail the script; `oPlayer`, `bPlayImmediately` and `bActorsHaveWeapons` are popped and unused. Re-checked here: the handler is a void command and returns 0 on those paths; a failed pop returns -639.

### PlayCutscene (648)
- **Handler:** 0x00651240 (own handler, registered 0x00678DA4) (checked).
- **Described in:** `dev/CUTSCENES.md` ("PlayCutscene (648)"): it does nothing, since loading is playing.
- **Does:** pops one object (default -1 in the local) and returns; no engine call is made (checked).
- **Edge cases:** the only failure is a failed pop, -639 (checked, the `neg/sbb/and 0x27F/add -639` idiom returns 0 or -639). The object is never looked up, so any value, valid or not, is accepted (checked). A script that calls it after `LoadCutscene(..., bPlayImmediately=0)` does not start the cutscene (checked: nothing is done).

### PlayMovie (173)
- **Handler:** 0x0065ECB0 (own handler, registered 0x00678DBA); calls `ScriptBackend::ForScript_PlayMovie` 0x006DC680 on the object at `*(*(app+0x114)+0x58)+0x28` (checked).
- **Described in:** `dev/CUTSCENES.md`, "A movie (.bik) as the engine plays it" (read 2026-10-19 by lib-agent-2).
- **Does:** pops the movie's resource name (the name as given, no '.' cut) and passes it to the script backend, which, if its byte +0x99 is 0, queues the name (0x006DC660, when the string is nonempty), asks the game mode controller for mode 10, `GameModeMovie`, starts the duck `audio_categories/ducking/OnMovie` (0x005B9020, 0x005BC7F0, the string at 0xB11538) and sets its +0x84 to 1 (checked; read 2026-10-19 by lib-agent-2). The backend is `GameModeMovie` itself, and 0x006DC660 pushes the name onto its queue at +0x88.
- **Edge cases:** failed pop -639 (checked); void command, returns 0 otherwise. When the backend's +0x99 byte is set (what sets it: inferred a no-movies switch) it does nothing and does not fail the script (checked). A name that is not a movie is not tested in the handler (checked).

## Not read
- Nothing left of this command: 0x006DC660 and mode 10 are read (`dev/CUTSCENES.md`, "A movie (.bik) as the engine plays it").
