# The Flash UI

Dragon Age's interface is Scaleform GFx 3.0: Flash movies (`.gfx`, from the game's `guiexport` resources) played by the engine, which the movies call into and which calls into them. This is what has been read of it, 2026-09-30, and what `dao/Gui.h` offers over it. Every address named here is in `src/dao/Addresses.h` with how it was found. None of it is seen in play yet; the example plugin and the probe log what a session needs to show.

## The pieces

- **`GUI::GUISystem`** (the game's is `GUI::DragonAgeGUISystem`) owns the UI. It holds the Scaleform manager at +0x120. Its slot 5 initializes the manager, and slot 17 opens a screen by id (`OpenScreen`, below). Slots 80 and 81 are `IsMovieOnStage` and `CallFunctionOnMovie`, which run the ActionScript of `MovieUtils` in a movie.
- **`GUI::ScaleformManager`**, the object in the global `0x00C31264`. It is inferred from where its methods sit, and the example plugin logs its class to confirm. It keeps:
    - +0x18, a cache of loaded movie definitions by name;
    - +0x44, a cache of views;
    - +0x70, the set of movie records;
    - +0xA8, the registry of FSCommand and ExternalInterface handlers.
- **A movie record** is the manager's entry for one movie, 0x108 bytes with no vtable. It holds the movie's name at +0 (an engine `std::wstring`), the movie at +0x1C (GFx's `GFxMovieView`), what `OpenMovie` was given as its third argument at +0x2C, and a byte from the screen's descriptor at +0x100. It is the "owner" every call from a movie carries: GFx's user data for the movie.
- **The movie** is GFx 3.0's `GFxMovieView`, class `GFxMovieRoot`. Its vtable was read from the game's own calls, each slot's `ret` agreeing: 10 `IsAvailable`, 11 `SetVariable`, 12 `GetVariable`, 13–16 the array getters and setters, 17–19 the `Invoke` family, 42 `SetExternalInterfaceRetVal`, 43 `GetUserData`.

## Movies calling the game

GFx calls the game's one FSCommand handler and one ExternalInterface handler. Both hand the call to the manager, which takes the movie's record and looks the call's name up in its registry. The registry is two maps keyed by the name's CRC-32: FSCommand handlers `(record, args)` and ExternalInterface handlers `(record, args, count)`. The game registers 67 and 17 at start-up: `GetObjectNameByID`, `HasAbility`, `ListTacticTargets`, `SendMessage`, `GetValue`, `AddTokenObserver` and the rest. An ExternalInterface handler answers through the movie's `SetExternalInterfaceRetVal`. An FSCommand handler answers by setting `_root.fsc_retval` to an array of ints, which the ActionScript reads after its `fscommand()`.

`dao::gui::OnFSCommand` and `OnExternalInterface` take a name and run a plugin's handler in place of the game's. `OnAnyFSCommand` and `OnAnyExternalInterface` see every call. `Call::Return` and `ReturnFSCommand` answer the way the game does.

## The game calling movies

`dao::gui::Owners()` lists the records through the engine's own iterator over the set. `FindOwner(name)` finds one, `NameOf` reads a record's name, and `MovieOf` gives its movie. `dao::gui::Movie` wraps a movie's `IsAvailable`, `SetVariable`, the array setters and `Invoke`. Values are not read back yet: GFx hands strings and objects out as references of its own to release, and how to release them is not read.

## Loading a movie

`ScaleformManager::OpenMovie(screen, name, layer, flags)` is how a movie comes to be. It looks the record up by name. Failing that, it gets the definition (`GetMovieDef`: `.swf` becomes `.gfx`, from the cache or `GFxLoader::CreateMovie`), makes a view (`CreateMovieView`: the definition's `CreateInstance`, then the view's set-up), builds a record and puts it in the set. Then it shows and activates the record. The GUI system's `OpenScreen` calls it with a screen descriptor's movie name, its +0x9C and +0xA4, and copies the descriptor's +0xBF into the record.

Every screen is a `GUI::GUIScene`, built as `GUIScene(name, filename, layer, b, flags)` and kept by the GUI system by id and name. `OpenMovie`'s first argument is the scene's id. The layer is the stacking order, going by the values the game's 42 scenes use: 0 floaty text, 1 the HUD (`GUIMain`), 2–3 in-game menus, 4 item examine, 5 conversation, death and the main menu, 6 options and login, 7 popups, 8 loading screens. Flag `0x20` marks the modal screens, `0x10` loading screens. `b` is 10 for the character record, journal, map, skills and spells, and 0 for most others; its meaning is not read. So a plugin's HUD-like movie belongs on layer 1. The GUI system (the application's +0x158) registers its scenes with `AddScene(id, scene)`, ids 0–0x3F, each scene 0xC0 bytes from the engine's allocator. `dao::gui::AddScreen` does the same with an id from 0x100, and `ShowScreen`, `HideScreen` and `CloseScreen` are the GUI system's own by-name slots 17, 19 and 18. The probe logs every `OpenMovie` to confirm the ids and values in play. Where the game finds a movie's file: GFx asks `EclipseGFxFileOpener`, which opens it through the engine's resource system as a resource of type 3 (`GUI::ScaleformResourceFile`). `GetMovieDef` puts `.swf` on a bare name, and the file opener falls back to `.gfx`, so a scene `turnorder` is the resource `turnorder.gfx`, wherever the resource system finds the game's own. `dao::gui::HasMovie(L"turnorder.gfx")` asks it the same way.

## Changing one of the game's movies

A screen's contents are often written in its movie's script, where no table or engine call reaches them. The options screen (`optionsmenu.gfx`, read 2026-10-01 from its ActionScript) is the case in point:

- **The tabs** (Game, Controls, Audio, Video, Online) are clips the designer placed. There is no list of sections the engine fills.
- **A checkbox** is the movie's `checkbox_mc` symbol, set up by its own helper `OptionsUtils.MakeOptionToggleInit(option, label, tooltip)`. A click sends `ClientOptions.SetClientOptionMessage <option> 1` (or `0`) to `CClientOptions`. OK sends `ClientOptions.CommitOptions`, and the defaults button `ClientOptions.RestoreDefaults`. `SliderHorizontal` and `Picker` are exported symbols too.
- **The key list** on the Controls tab is built in script: `AddHeader(text)` for a section and `AddKeyBinding("TogglePause")` for each action, 50 of them, the names strings in the script. A row binds its label to the data tree's `InputMappings.Mappings.<name>.Description` and sends `InputMappings.EnableKeyRemappingMessage <name> 0` (or `1`, for the second key) when a key's button is pressed.

So a plugin's row gets into such a list only by adding to the script. `dao::gui::OnMovieLoad(name, patch)` does it in memory:

- **Where.** GFx reads a movie through `GUI::ScaleformResourceFile`, a file object over the resource's bytes in memory: where they begin at +0x30, where it has read to at +0x34, its length from its slot 6. The library detours `EclipseGFxFileOpener::OpenFile`, and for a movie with a patch points the file at the patched bytes and gives it a vtable whose length slot answers theirs.
- **What.** The patch is given the movie uncompressed (`dao/core/GfxMovie.h`, `Unpack`; the game's are `CFX`, a zlib stream after the header, inflated by `dao/core/Inflate.h`) and answers the changed bytes. GFx takes the uncompressed `GFX` as readily.
- **How a script is added to.** `core::gfx::InsertAfter(movie, {"AddKeyBinding", "HideMainInterface"}, code)` finds that statement in the movie's script and puts `code` after it, then makes everything around it fit: the header's length, the tags' lengths, the sizes of the functions it is inside, and any branch that jumps across it. `Statements` makes the code for calls like the game's own.
- **Checked.** On the game's own `optionsmenu.gfx` the patched movie lists as the original does but for the added calls and one branch 118 bytes longer. In play, 2026-10-01, the patched movie opened every time and showed the added header and rows, which rebound as the game's own do.

`dao::input` uses it to list a plugin's actions in the controls screen.

`dao::options` uses it to put a plugin's section on the Game tab (`dao/core/OptionsMovie.h`):

- **Room.** The Game tab is one sprite, its controls in two columns a row every 35 pixels of the 1024 by 768 stage: the left ends with a checkbox at (7, 228), the right at (483, 166), and the tab is empty below them to the screen's buttons. So there are five free rows on the left and six on the right, the same at every resolution, since the movie scales.
- **Placing.** A heading and checkboxes (the tab's own `checkbox_mc`) are placed in those rows by adding `PlaceObject2` tags to the sprite (`core::gfx::PlaceInSprite`). The heading is the game's own: the tab's "Gameplay" is a clip of a gold bar (a shape 437 by 48) and, over it at (24, 13), the text clip the movie exports as `txt_Game`, and a section places the same two. A bar is higher than a row, so a heading takes two rows, and a section's rows are one under the other in one column. (The first heading was the key list's `KeyBindingHeader`, plain centred text, which read as a caption beside the game's bars.)
- **Setting up.** A clip's set-up function is found by its name with `_init` after it, on the clip or any clip above it (`GetInitFunction`, called by `GUI.Element` when the clip loads). The movie makes its own in one function, and the patch adds a plugin's before the first of them (`core::gfx::InsertBefore`): a checkbox's is the game's `OptionsUtils.MakeOptionToggleInit(option, label, tooltip)`, a heading's text clip's sets its text (the clip's own is `$LOC_GAMEPLAY`, which the movie turns into the language's word).
- **A checkbox's value.** The helper asks to be told the option's value with `ExternalInterface.call("AddTokenObserver", label, "", "ClientOptions.<option>")`, which answers an id, and keeps a callback under the id. The engine tells a movie its tokens' values by setting two arrays in it, `TokenManager.BatchTokenReplacementCallbackIDs` and `TokenManager.BatchTokenReplacementValues`, and invoking `TokenManager.PerformBatchTokenReplacements` (`GuiMovie::DeliverTokens`, `0x00519B00`, run for every movie). For a plugin's option the library takes the `AddTokenObserver` call, answers an id of its own (the engine's count down from 0xFFFFFFFF and a movie's own up from 0; the library's start at 0x40000000), and delivers the value the same way after the engine's own.
- **A click.** The helper sends the FSCommand `SendMessage` with `ClientOptions.SetClientOptionMessage <option> 1` (or `0`). The library takes those that name a plugin's option (`dao::gui::InterceptFSCommand`), runs the plugin's `set` on the frame's thread, and delivers what it answers, so a refused click draws the box back.
- **Checked.** On the game's own movie the patch places the heading and boxes at (7, 263) on and adds their set-up functions before `OptAutoPause_mc_init`. In play, 2026-10-01, the first try had two plugins patching the movie and showed only one's work (below); the second showed the heading and three boxes under the left column, in line with the game's own, each asked its value at every opening, a refused click drawn back within 0.3 s, tooltips, and the same in a fight with the game paused behind the menu. The heading reads as a caption: plain centred text, where the game's own headings are gold bars.
- **Several plugins.** Each plugin links its own copy of the library, so each has its own detour of `OpenFile` and its own patches, and the copies run one inside the other. The inner copy's file has that copy's vtable, which the outer one first took for a stranger's and left alone; it now knows a file by the game's slots other than the length's, and patches what the inner one left. The same holds for every hook the library places: `dao::input`'s copy passes on a key's text that is another copy's action, and `dao::options`' copies give their checkboxes ids that carry the plugin's own address, since one movie keeps them all. So a patch must expect the others' work in the movie: the Game tab's free rows leave out rows something is placed in, a heading is named by its row, and a key section whose heading is in the list already puts its rows under it.

## What Scaleform's own documentation adds

Autodesk's Scaleform help (help.autodesk.com, for Scaleform 4.x) and the UDK's pages on its GFx 3 integration describe the same interface a generation later. They're good for what things mean, not for names or layouts. They confirmed four points, 2026-09-30:

- The FSCommand callback is `(movie, command, args)` and returns nothing.
- The ExternalInterface callback is `(movie, method, args, count)`, the shapes read from the game.
- GFx runs both kinds of handler during a movie's `Advance` or an `Invoke` on it.
- `SetVariable`'s sticky mode is the default and waits for a variable not yet there.

`SetArrayType` is the same in 4.x: 0 int, 1 double, 2 float, 3 string, 4 wide string, 5 value, which matches what the game passes. The value type numbers are not the same: 4.x inserted `VT_Int` and `VT_UInt` at 3 and 4, and the game's code uses 3.x's numbering (3 number, 4 string, 5 wide string). A managed value (a string or object GFx hands out) must be released before its movie goes.

## Still to read

- The data tree movies bind to. Reading it is `dao/Data.h`: `SmartData::TokenManager` is the root, and its `Find(path)` walks a dotted path to an attribute, which gives its value as text or a number. A plain attribute can be set (`Attribute::Set`, its class's slot 21, which notifies observers when the value changes). Still to read are making one, and the token observers `AddTokenObserver` registers, through which the game pushes a changed value to a movie. All three are needed for a plugin to publish values a movie shows.
- Portraits, read 2026-09-30. A creature's portrait is rendered live into a texture of its own (`CCCreature::UpdatePortrait`, `dao/Addresses.h`). A movie shows it as `img://portrait_<tag><counter>` (the player's is `portrait_player0`), the texture's name. `creature->RenderPortrait()` renders it and returns that URL, for any creature. Not yet seen in play; `UpdatePortraitMessage` and the paperdoll are still to read.
- Releasing a GFx value a movie hands out, to read variables and `Invoke`'s results.
