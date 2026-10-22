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

`dao::gui::Owners()` lists the records through the engine's own iterator over the set. `FindOwner(name)` finds one, `NameOf` reads a record's name, and `MovieOf` gives its movie. `dao::gui::Movie` wraps a movie's `IsAvailable`, `SetVariable`, the array setters and `Invoke`, and reads values back with `Get` (a path) and `Call` (a function's answer). This game's GFx is 3.0, which has no objects in a GFxValue (read 2026-10-04, DAOLib #24): what it hands out, from `GetVariable` and `Invoke`'s answer through one converter (`0x00954910`), is undefined, null, a bool, a number or a string, the string copied into a pool the movie empties at its next such call, and an object, an array or a clip as its string. `Invoke` passes any argument that is not null, bool, number or string to ActionScript as undefined (`0x00956E80`). Nothing is reference-counted. So a call cannot be given an object, and a function that wants one (the level-up screen's `AbilityScene.SpendPoint(oClass, ...)`) is reached through something of the movie's that takes none, such as a button's own handler.

## Loading a movie

`ScaleformManager::OpenMovie(screen, name, layer, flags)` is how a movie comes to be. It looks the record up by name. Failing that, it gets the definition (`GetMovieDef`: `.swf` becomes `.gfx`, from the cache or `GFxLoader::CreateMovie`), makes a view (`CreateMovieView`: the definition's `CreateInstance`, then the view's set-up), builds a record and puts it in the set. Then it shows and activates the record. The GUI system's `OpenScreen` calls it with a screen descriptor's movie name, its +0x9C and +0xA4, and copies the descriptor's +0xBF into the record.

Every screen is a `GUI::GUIScene`, built as `GUIScene(name, filename, layer, b, flags)` and kept by the GUI system by id and name. `OpenMovie`'s first argument is the scene's id. The layer is the stacking order, going by the values the game's 42 scenes use (corrected 2026-10-18 by lib-agent-2: the game makes 61 scenes, see "The scenes" below, and these 42 are the subset that has values): 0 floaty text, 1 the HUD (`GUIMain`), 2–3 in-game menus, 4 item examine, 5 conversation, death and the main menu, 6 options and login, 7 popups, 8 loading screens. Flag `0x20` marks the modal screens, `0x10` loading screens. `b` is 10 for the character record, journal, map, skills and spells, and 0 for most others; its meaning is not read. (Corrected 2026-10-18 by lib-agent-2: `b` is a bit mask, 1 loading, 2 a background call, 4 outro on hide, 8 the book background, so the record's, journal's and map's 10 is 2|8. Flag 0x10 keeps a movie running while paused or loading, not only marking loading screens; 0x04 restarts the movie on each show; 0x20 also stops lower layers taking the mouse.) So a plugin's HUD-like movie belongs on layer 1. The GUI system (the application's +0x158) registers its scenes with `AddScene(id, scene)`, ids 0–0x3F, each scene 0xC0 bytes from the engine's allocator. `dao::gui::AddScreen` does the same with an id from 0x100, and `ShowScreen`, `HideScreen` and `CloseScreen` are the GUI system's own by-name slots 17, 19 and 18. The probe logs every `OpenMovie` to confirm the ids and values in play. Where the game finds a movie's file: GFx asks `EclipseGFxFileOpener`, which opens it through the engine's resource system as a resource of type 3 (`GUI::ScaleformResourceFile`). `GetMovieDef` puts `.swf` on a bare name, and the file opener falls back to `.gfx`, so a scene `turnorder` is the resource `turnorder.gfx`, wherever the resource system finds the game's own. `dao::gui::HasMovie(L"turnorder.gfx")` asks it the same way.

## Which screens are up

Read 2026-10-02, for a tool driving the game without a screenshot (Eluvian's issue 2). `dao::gui::Screens()` lists the GUI system's scenes, `ShownScreens()` those on the screen with the top layer first, and `CurrentFullScreen()` the full-screen GUI open.

- **The scenes.** The GUI system's map at +0x58 is MSVC 8's `std::map<int, GUIScene *>`: a word, the head at +0x5C, the count at +0x60 (`SceneIdByName` walks it from the head's left). The game makes 60 scenes (corrected 2026-10-18 by lib-agent-2: 61; and some have no movie file in this install: Skills, SpellsTalents, ContextMenu, Main/MainUI, CustomLoading and the 3 scoreboards), each on a movie of its own, the scene's name and the movie's mostly alike: `Inventory` on `inventory`, `Options` on `OptionsMenu`, `Main` on `MainUI`, `StartMenu` on `startmenu2`, `Loading` on `load_town`. The loot panel is the inventory's scene, its container shown by the data tree's `GUIInventory.ShowContainer`. A scene keeps its name at +0x58, its movie's name in its `Filename` attribute at +0x74 (the text at +0xC in it), its layer at +0x9C and its flags at +0xA4. The ids the probe saw opened in play are 0x0E `load_town`, 0x12 `startmenu2`, 0x1C `ArmyControl`, 0x1E `PopupLayer`, 0x28 `NavBar`, 0x29 `Quickbar`, 0x2A `Minimap`, 0x2B `Portraits`, 0x2C `Notifications`, 0x2D `Tooltips` and 0x2F `FloatyLayer`.
- **Shown.** A shown scene's movie has a record in the manager under the movie's name, and the record's byte at +0x28 is set while it is active: by `OpenMovie` (0x00518CB0 with 1), cleared by the GUI system's hide (slot 19, the manager's 0x00511990), which also takes the movie off the renderer's list (+0x29 cleared, 0x005187F0), and cleared on every record at once when the interface is put away (0x005118F0's loop). Its +0x38 is 0 when new, 2 once shown (0x00518E00) and 1 once hidden by 0x00518CD0, which only the manager's teardown and a by-name call (0x00511940) reach. So shown is the active byte: what is drawn.
- **The full-screen GUI.** What `ToggleFullScreenGUIMessage <name>` opens and `GUI.HideFullScreenGUI` closes. `SetFullScreenGUIMessage` puts the game in the GUI mode and gives the scene's id to `GameModeGUI` (+0x98, 0x007183C0), which closes every full-screen GUI and opens this one (0x00611740): the GUI system shows the scene by id (its slot 13, which also signals the scripts EVENT_TYPE_GUI_OPENED with the id) and pushes the id on a `std::vector<int>` at +0x1E4 (0x00610CE0). Hiding the last (0x0060F1E0) hides it through slot 14 and pops it, and slot 16 pops them all. `Toggle` hides when the name's id is the vector's last, so the last is the one up.
- **The loot panel's buttons**, read 2026-10-02 by the Eluvian server's session from `inventory.gfx`'s class `Container`: Take All sends `SelectedChar.AcquireAllItems <Container.ID>`, then closes unless the pack is full with items left. Close sets `GUIInventory.ShowContainer` to 0, then sends `Container.InventoryClosedMessage`, `GameModeController.PerformUseObject <SelectedChar.ID> <Container.ID> 2` and `GUI.HideFullScreenGUI`. The movie's `ExternalCommands.SendMessageArgs` joins its arguments with spaces into FSCommand `SendMessage`, so each is a message by its text (`dao::messages::SendText`).
- **Seen in play**, 2026-10-02 (GOG, through the Eluvian server's `inspect kind=gui`): exploring with nothing open, nine screens are shown, Tooltips (layer 8), PopupLayer and Notifications (7), NavBar and Quickbar (4), ArmyControl, Minimap and Portraits (1) and FloatyLayer (0), and no full-screen GUI. `Main` is not among them. PopupLayer's movie stays active with no box up, so `CurrentPopup` is what says a box is up. The loot panel is the full-screen GUI `Inventory`, in the GUI mode. The level-up screen is `LevelUp`, in the CharGen mode, with `BookBack` shown beside it and Notifications gone. `GUI.HideFullScreenGUI`, or the loot panel's Close, gives the nine back and no full-screen GUI. Not yet seen: a store, the map, the inventory opened by its key.
- **The level-up screen's messages**, as the Eluvian server's session drove it 2026-10-02 (its issue 5): `GUI.GoToLevelUp <creature> 0` opens it, `GUILevelUp.AutoLevelUp <creature>` spends the points as the game would and applies them at once, `GUILevelUp.CharGenEnd <creature> 1 1` ends it (1 being `LEVELUP_EXIT_COMPLETE`), and `GUI.HideFullScreenGUI` closes it. Closing with the movie's own exit, an abort, did not take back what AutoLevelUp had applied.
- **A plugin's screen's id.** `AddScreen` takes the id past the largest the map has, from 0x100. Each plugin links its own copy of the library, and a count kept per copy gave two plugins' screens the same id, which `AddScene` ignores for the second.

## Changing one of the game's movies

A screen's contents are often written in its movie's script, where no table or engine call reaches them. The options screen (`optionsmenu.gfx`, read 2026-10-01 from its ActionScript) is the case in point:

- **The tabs** (Game, Controls, Audio, Video, Online) are clips the designer placed. There is no list of sections the engine fills.
- **The Video tab** (read 2026-10-11, DAOLib #79, `patch003.erf`'s movie) is a clip at (82, 155) of two clips, one a column. The left one, at (0, 50) in it, has `OptResolution_mc` (a picker) at (9, 21), `OptFullscreen_mc` at (3, 120), the gamma slider at (44, 153) and `OptVSync_mc` at (3, 302). The right one, at (445, 0), has the three detail sliders at x 72 and `OptFBFX_mc` at (35, 267). On the stage the columns' checkboxes stand at x 85 and 562, the Game tab's 85 and 561, under two bars at the Game tab's places. The Game tab's lowest row is at y 603 of the stage, the panel's corner under it: the Video tab's left column, whose last checkbox is at 507, has two rows free above that, and its right, whose last is at 422, five. So for a section the tab is laid out again (DAOLib #82, the user's decision 2026-10-11): `dao::options::AddSection` with `Tab::Video` takes the left column's controls out of their clip and places them again closer (the picker at (9, -5), the checkbox at (3, 58), the gamma slider at (44, 91) with its picture, a shape at the clip's origin, 62 higher, `OptVSync_mc` at (3, 184)), moves the "Detail" bar, which the main timeline places at frame 31 (sprite 310 at (524, 131), each of the nine frames after a move with no place), to (70, 425) by writing the place where it is, and places the right column's four controls under it in the left clip (sliders at (44, 280), (44, 321) and (44, 362), `OptFBFX_mc` at (3, 398), the Game tab's lowest row). The right clip is then empty, and a section's rows go down it from y -30, where a heading's bar is level with "Screen"'s, to 425: fourteen rows, a heading and twelve, above Restore Defaults, which every tab has. Once: a copy of the library that finds `OptFBFX_mc` in the left clip leaves the tab as it is. FFDec does not draw the movie (its pictures are the game's textures), so the look is checked in play.
- **A checkbox** is the movie's `checkbox_mc` symbol, set up by its own helper `OptionsUtils.MakeOptionToggleInit(option, label, tooltip)`. A click sends `ClientOptions.SetClientOptionMessage <option> 1` (or `0`) to `CClientOptions`. OK sends `ClientOptions.CommitOptions`, and the defaults button `ClientOptions.RestoreDefaults <tab>`, the tab as the movie numbers it (`OptionsMenu.as`, read 2026-10-15, DAOLib #140): Game 0, Online 1, Controls 2, Audio 3, Video 4, Movies 5. `CClientOptions` resets its own options of that group (0x005E49C0; Controls also reloads the key bindings) and nothing of a plugin's; `dao::options` gives each of the tab's plugin controls with a `defaultValue` that value, as a click would. `SliderHorizontal` and `Picker` are exported symbols too.
- **A dropdown** is the `Picker` symbol (`SharedLibrary.Picker`), read 2026-10-11 (DAOLib #66; `guiexport.erf`'s movie and `patch003.erf`'s alike). The Game tab has three, `OptDifficulty_mc` at (11, 0), `OptPlotAssist_mc` at (11, 60) and `OptSubtitles_mc` at (488, 0): 4 right of the column's checkboxes and 60 apart. Its helper is `OptionsUtils.MakePickerInit(option, label, tooltip, new Array(names...), descriptions, callback)`: the descriptions are the entries' tooltips or `null`, and the callback the game gives is `OptionsMenu.PickEntryCallBack`, which sends `ClientOptions.SetClientOptionMessage <option> <index>` as a slider does. The button asks the option's value with `ExternalCommands.GetValue` as it is made and is told it after through a token observer. Its list (`PickerList_mc`) is a clip of the picker's own, filled each time it opens with a `PickerEntry` a name, so it is drawn at the picker's depth: the designer placed the three deepest of the tab (49, 59 and 69, the checkboxes 1 to 43), the upper deeper than the lower, and an open list covers what is under it. A list opens downward unless the picker's `ListDropsUp` is set. `dao::options::Choice` with `ChoiceStyle::Dropdown` places one the same way.
- **The Online tab** (read 2026-10-12, DAOLib #104, `patch003.erf`'s movie) is two clips the main timeline places, one a column: the left at (87, 222) with `OptSendTelemetry_mc` at (0, 0) and `OptAutoDLOldEntitlements_mc` at (0, 36), the right (`OnlineProfileGroup_mc`, which no script names) at (538, 222) with `OptAutoScreenshot_mc` at (0, 0), its notification's narrower box set in at (25, 36), `OptUploadScreen_mc` at (0, 72) and `OptAutoUploadProfile_mc` at (0, 108). They stand under the bars "Connection" and "Profile", which are at the Game tab's places, so the checkboxes are 17 and 14 right of their bars. Down to the Game tab's lowest row on the stage the left column has nine rows free and the right seven: `dao::options::AddSection` with `Tab::Online` places a section there as on the Game tab, each column in its own clip.
- **A button** is the symbol of the screen's own `RestoreDefaults_mc` (`SharedLibrary.ButtonBase` with `Behaviour.PushButton`), 358 wide and 46 high, read 2026-10-12 (DAOLib #104). Its set-up function sets its click's sound (`m_nSoundType = SoundEvents.ST_OPTIONS_BUTTON_CLICK`, 95), its text with `SetText`, which writes it into the clips of each of its looks, and what a click does with `SetButtonAction`; Restore Defaults's sends `ClientOptions.RestoreDefaults` with the tab's number. A click is `onPressHitArea` and `onReleaseHitArea`, which `SimulateButtonPush` calls in turn: the sound, then the action. A plugin's button (`dao::options::Button`) is the same symbol placed under a checkbox's kind of name where the column's checkboxes stand, 52 under the control above it, with a set-up function of the library's that sends what a ticked checkbox sends and adds a checkbox's tooltip (`OptionsUtils.AddTooltip`).
- **The key list** on the Controls tab is built in script: `AddHeader(text)` for a section and `AddKeyBinding("TogglePause")` for each action, 50 of them, the names strings in the script. A row binds its label to the data tree's `InputMappings.Mappings.<name>.Description` and sends `InputMappings.EnableKeyRemappingMessage <name> 0` (or `1`, for the second key) when a key's button is pressed.

So a plugin's row gets into such a list only by adding to the script. `dao::gui::OnMovieLoad(name, patch)` does it in memory:

- **Where.** GFx reads a movie through `GUI::ScaleformResourceFile`, a file object over the resource's bytes in memory: where they begin at +0x30, where it has read to at +0x34, its length from its slot 6. The library detours `EclipseGFxFileOpener::OpenFile`, and for a movie with a patch points the file at the patched bytes and gives it a vtable whose length slot answers theirs.
- **What.** The patch is given the movie uncompressed (`dao/core/GfxMovie.h`, `Unpack`; the game's are `CFX`, a zlib stream after the header, inflated by `dao/core/Inflate.h`) and answers the changed bytes. GFx takes the uncompressed `GFX` as readily.
- **How a script is added to.** `core::gfx::InsertAfter(movie, {"AddKeyBinding", "HideMainInterface"}, code)` finds that statement in the movie's script and puts `code` after it, then makes everything around it fit: the header's length, the tags' lengths, the sizes of the functions it is inside, and any branch that jumps across it. `Statements` makes the code for calls like the game's own.
- **Checked.** On the game's own `optionsmenu.gfx` the patched movie lists as the original does but for the added calls and one branch 118 bytes longer. In play, 2026-10-01, the patched movie opened every time and showed the added header and rows, which rebound as the game's own do.

`dao::input` uses it to list a plugin's actions in the controls screen.

`dao::options` uses it to put a plugin's section on the Game tab (`dao/core/OptionsMovie.h`):

- **Room.** The Game tab is one sprite, its controls in two columns a row every 35 pixels of the 1024 by 768 stage: the left ends with a checkbox at (7, 228), the right at (483, 166), and the tab is empty below them to the screen's buttons. So there are five free rows on the left and six on the right, the same at every resolution, since the movie scales.
- **Placing.** A heading and checkboxes (the tab's own `checkbox_mc`) are placed in those rows by adding `PlaceObject2` tags to the sprite (`core::gfx::PlaceInSprite`). The heading is the game's own: the tab's "Gameplay" is a clip of a gold bar (a shape 437 by 48) and, over it at (24, 13), the text clip the movie exports as `txt_Game`, and a section places the same two. A bar is higher than a row, so a heading takes two rows, and a section's rows are one under the other in one column. A dropdown takes two rows too. A slider takes 41 pixels, not a row's 35 (DAOLib #84): a section's sliders stand 41 apart from the row under its checkboxes, as the game's own Detail sliders do, and the section takes the rows they reach into. They had two rows each at first, 70 apart, which read as loosely set beside the game's and left the Video tab's column room for five; it now holds a heading, two checkboxes and eight. Seen in play 2026-10-12 by the shaders plugin's session (GOG's game at 1600 by 1000, a Video section of two checkboxes and six sliders, driven through the movie and not the mouse): the checkboxes' `_y` 40 and 75, the sliders' 110 to 315 in steps of 41, beside the game's own at 280, 321, 362; every label, value and arrow clear of its neighbours; each slider stepped set its own option alone. Seven and eight sliders, and a drag of a thumb with the mouse, were not tried. (The first heading was the key list's `KeyBindingHeader`, plain centred text, which read as a caption beside the game's bars. The bar was seen in play 2026-10-01: it matches the game's two, with the title in the face and size of "Gameplay".)
- **Setting up.** A clip's set-up function is found by its name with `_init` after it, on the clip or any clip above it (`GetInitFunction`, called by `GUI.Element` when the clip loads). The movie makes its own in one function, and the patch adds a plugin's before the first of them (`core::gfx::InsertBefore`): a checkbox's is the game's `OptionsUtils.MakeOptionToggleInit(option, label, tooltip)`, a heading's text clip's sets its text (the clip's own is `$LOC_GAMEPLAY`, which the movie turns into the language's word).
- **A checkbox's value.** The helper asks to be told the option's value with `ExternalInterface.call("AddTokenObserver", label, "", "ClientOptions.<option>")`, which answers an id, and keeps a callback under the id. The engine tells a movie its tokens' values by setting two arrays in it, `TokenManager.BatchTokenReplacementCallbackIDs` and `TokenManager.BatchTokenReplacementValues`, and invoking `TokenManager.PerformBatchTokenReplacements` (`GuiMovie::DeliverTokens`, `0x00519B00`, run for every movie). For a plugin's option the library takes the `AddTokenObserver` call, answers an id of its own (the engine's count down from 0xFFFFFFFF and a movie's own up from 0; the library's start at 0x40000000), and delivers the value the same way after the engine's own.
- **A click.** The helper sends the FSCommand `SendMessage` with `ClientOptions.SetClientOptionMessage <option> 1` (or `0`). The library takes those that name a plugin's option (`dao::gui::InterceptFSCommand`), runs the plugin's `set` on the frame's thread, and delivers what it answers, so a refused click draws the box back.
- **Checked.** On the game's own movie the patch places the heading and boxes at (7, 263) on and adds their set-up functions before `OptAutoPause_mc_init`. In play, 2026-10-01, the first try had two plugins patching the movie and showed only one's work (below); the second showed the heading and three boxes under the left column, in line with the game's own, each asked its value at every opening, a refused click drawn back within 0.3 s, tooltips, and the same in a fight with the game paused behind the menu. The heading reads as a caption: plain centred text, where the game's own headings are gold bars.
- **Several plugins.** Each plugin links its own copy of the library, so each has its own detour of `OpenFile` and its own patches, and the copies run one inside the other. The inner copy's file has that copy's vtable, which the outer one first took for a stranger's and left alone; it now knows a file by the game's slots other than the length's, and patches what the inner one left. The same holds for every hook the library places: `dao::input`'s copy passes on a key's text that is another copy's action, and `dao::options`' copies give their checkboxes ids that carry the plugin's own address, since one movie keeps them all. So a patch must expect the others' work in the movie: the Game tab's free rows leave out rows something is placed in, a heading is named by its row, and a key section whose heading is in the list already puts its rows under it.

## The tactics screen

Read 2026-10-08 in tactics.gfx's ActionScript (patch003.erf; decompiled under `build\gfx\tactics_as`), for Eluvian's #89. The screen works on the data tree under `GUI.SelectedChar`, so the follower must be the GUI's selected character.

- **What it offers** comes from the engine, through FSCommands that answer in `_root.fsc_retval` as arrays: `ListTacticTargets <creature>` (target types, with `fsc_retval2` the parallel object ids of the per-follower ones), `ListTacticBaseConditions <target>`, `ListTacticConditions <base> <target>` (conditions, with their ids in `fsc_retval2`), `ListTacticActions <creature> <target> <base>` (command types), `ListTacticAbilities <creature> <target> <base>` and `ListTacticModalAbilities <creature>` (ability ids), `ListTacticItems <target>` (item object ids).
- **A row** is `GUI.SelectedChar.Tactics.TacticsList.<n>`, all numbers: `.Target`, `.TargetID`, `.Condition`, `.ConditionID`, `.Command`, `.CommandParam` (the ability), `.CommandItem` (the item's object id, or ""), `.Enabled`. A condition picked is followed by `SendMessage("GUI.SelectedChar.Tactics.DirtyPreset")` and `SendMessageArgs("GUI.SelectedChar.Tactics.ValidateAction", <n>)`, which may change the row's command; an action picked by `DirtyPreset`. Opening sends `RefreshTacticsData 1`; presets and behaviour are `LoadPreset <index> <type>`, `SavePreset` and `SetBehaviour <behaviour>`. Closing sends only `GUI.HideFullScreenGUI`.
- **The rows write through at once**: seen in play 2026-10-09 (Eluvian's `tactics set`, the turn-based mod's session), a row set this way read back through the scripts' `GetTactic*` in the same call, with no screen opened or closed.

## A movie's stage against the view

Seen in play, 2026-10-01, by the turn-based mod's movie on layer 1 (a stage authored 1280 by 720, `Stage.scaleMode = "showAll"`, in a client area of 1600 by 1000): the engine gives the movie the whole client area and the movie's own scale mode does the rest. The stage was laid over the area scaled by one number (1.25) and centred, with 50 pixels of nothing above and below. In the movie `Stage.width` read 1280, the authored width, but `Stage.height` read about 800, the view's height in stage units, not the authored 720. So a movie that turns view pixels (`dao::world::ToScreen`) into stage points must carry its authored size as numbers of its own and not read it from `Stage`. With that done, a line drawn from `ToScreen`'s points lay on the creature's feet and the cursor's ground point, at four places.

Addition, read 2026-10-18 by lib-agent-2 in `0x005111B0` (corrected 2026-10-18 by lib-agent-2): on a resize the engine sets NoScale (view +0x58, 0) and a viewport over the client area (+0x50). Below 1024 by 768 the viewport carries a scale (corrected 2026-10-18 by lib-agent-2, reread at 0x0051127E-0x005112A5): 1024/w when w/h < 4/3, else 768/h, so at least 1; it is not w/1024 or h/768. The `[+0x184]` slot called at 0x00511242 is the display's pixel aspect (from `EnumDisplaySettingsW` and the back buffer) and goes into the viewport's AspectRatio, not its Scale. `CreateMovieView` sets alignment centre (+0x60, 0).

## A movie under the cursor

Explore mode's pick (`dao/GameModes.h`) chooses nothing when the interface says the mouse is over an element of one of its movies: the chooser (`0x0070EAA0`) asks the GUI system (its slot 71, on to the Scaleform manager, `0x00511420`) before it looks at what the ray met. A click there is the interface's and not the world's. This holds for a plugin's movie as for the game's. Seen in play, 2026-10-01: the turn-based mod's layer-1 movie drew a path's end on the cursor's pixel, and the mode's ground point was nothing in half the reads and six of nine clicks to move were lost; with nothing drawn on that pixel the pick was steady. So a movie that draws in the world's part of the screen keeps its shapes off the cursor's pixel, or makes its clip one the mouse does not meet (Scaleform's `hitTestDisable`, with `_global.gfxExtensions` set as the game's own movies set it). The turn-based mod did both at once, the property and a line ending 14 pixels short of the cursor, and the pick was steady; which of the two did it was not told apart.

## What Scaleform's own documentation adds

Autodesk's Scaleform help (help.autodesk.com, for Scaleform 4.x) and the UDK's pages on its GFx 3 integration describe the same interface a generation later. They're good for what things mean, not for names or layouts. They confirmed four points, 2026-09-30:

- The FSCommand callback is `(movie, command, args)` and returns nothing.
- The ExternalInterface callback is `(movie, method, args, count)`, the shapes read from the game.
- GFx runs both kinds of handler during a movie's `Advance` or an `Invoke` on it.
- `SetVariable`'s sticky mode is the default and waits for a variable not yet there.

`SetArrayType` is the same in 4.x: 0 int, 1 double, 2 float, 3 string, 4 wide string, 5 value, which matches what the game passes. The value type numbers are not the same: 4.x inserted `VT_Int` and `VT_UInt` at 3 and 4, and the game's code uses 3.x's numbering (3 number, 4 string, 5 wide string). Scaleform's later versions hand out managed values (0x40 beside the type) to be released; this game's 3.0 has none (above).

## The scenes: movies, stacking, opening and closing

Read 2026-10-18 by lib-agent-2 (for DAOParagonEngine #19).

GOG exe, base 0x400000. "checked" = read in the disassembly or the files. "inferred" = deduced.

### Short answers

1. 61 `GUIScene`s are built in `DragonAgeGUISystem`'s constructor (0x00613B40; the construction run is 0x00614C80-0x006164D0). Each is made with operator new (0xC0 bytes), then `AddScene(id, scene)` (0x005240D0). Ids are 0..0x3F with 2, 0xB and 0xC unused (checked). 33 are built inline as a plain `GUIScene` (GUIScene ctor 0x00A55AC0, 60 call sites), the rest by a subclass constructor that calls the base ctor. Table below. Movie file = `<filename>.gfx` (`.swf` becomes `.gfx`), in `guiexport.erf` and the patch ERFs.
2. **Corrections to dev/GUI.md.** (a) The scene's `b` (+0xA0) is a bit mask, not "10 for the record": bit 1 = world rendering switched off while shown (loading screens), bit 2 = a sound on show, `0x00510CA0(2, "Click")` → 0x00522C30, the GUI sound row 2 "open book" (corrected 2026-10-19 by lib-agent-2; this said a background snapshot), bit 4 = on hide play the outro (`PlayOutro`) instead of closing at once, bit 8 = shows the book background `BookBack` (0x3B) with it, and turns the frame effect `da_blurdesaturate` on (faded out on hide; `dev/FRAME_EFFECTS.md`, the blur behind full-screen screens). (b) The layer sets draw order and input order: the renderer's and the manager's sets are keyed by it (below). Flags (+0xA4) go to the record's +0x24: 0x20 modal (mouse), 0x10 keeps running while paused/loading, 0x04 restart the movie on each show, 0x02 no frame catch-up, 0x01 = a render-target/cursor-related set-up in the record ctor (unclear).
3. Hide and close differ. Every `Hide*` wrapper of `DragonAgeGUISystem` and the by-id hide (slot 14) call the scene's slot 6, which **closes**: it drops the movie record (`GUISystem::CloseScreen`), so a screen is loaded on demand and unloaded. Only the HUD's temporary hide (`ToggleMainInterfaceMessage`) uses slot 7, which keeps the movie loaded but inactive. The parsed movie definition is kept for ids in a "cached" id list (GUISystem +0x6C), so a re-open skips the parse.
4. "Full screen" = whatever `SetFullScreenGUIMessage` opened. It puts the game in `GameModeGUI` (mode 3) and **pauses the game** (`SetPaused(true,false)` in the mode's enter, 0x007187D0). Opening one closes the previous full-screen one first. Modal input has two layers: mouse (record flag 0x20 stops lower-layer movies under the cursor getting the mouse) and keyboard/game messages (input contexts, one per game mode and per movie by `CreateContext`/`ActivateContext` fscommands).

### Table of the 61 scenes (checked from the constructor calls)

Format: id, name, movie, layer, b, flags. Subclass constructor address where there is one.

| id | name | movie | layer | b | flags | ctor |
|---|---|---|---|---|---|---|
| 0x00 | CharacterRecord | characterrecord | 3 | 10 | 0 | 0x0073C440 |
| 0x01 | CharGen | chargen | 3 | 0 | 0 | 0x0073B460 (via base 0x007FD930) |
| 0x03 | ContextMenu | contextmenu (no .gfx found) | 7 | 0 | 0 | plain |
| 0x04 | Conversation | conversation | 5 | 0 | 0 | 0x007336E0 |
| 0x05 | Cutscene | cutscene | 5 | 0 | 0 | 0x007334C0 |
| 0x06 | Debug | debug | 8 | 0 | 0 | plain |
| 0x07 | Main | MainUI (no .gfx found; not shown in play, HUD is split) | 1 | 0 | 0 | 0x00733320 |
| 0x08 | Inventory | inventory | 3 | 8 | 0 | 0x007329E0 |
| 0x09 | Journal | journal | 3 | 10 | 0 | 0x007083F0 |
| 0x0A | SpellsTalents | filename empty (no movie) | 3 | 10 | 4 | 0x00730080 |
| 0x0D | Skills | skills (no .gfx found) | 3 | 10 | 4 | 0x0072FD10 |
| 0x0E | Loading | load_town | 8 | 5 | 0x10 | 0x007289F0 |
| 0x0F | Map | map | 3 | 10 | 0 | 0x0072FA10 |
| 0x10 | WorldMap | worldmap | 3 | 8 | 0x10 | 0x0072ECE0 |
| 0x11 | Options | optionsmenu | 6 | 0 | 0 | 0x0072EA80 |
| 0x12 | StartMenu | startmenu2 | 3 | 0 | 0 | 0x0072E910 |
| 0x13 | DeathScreen | deathscreen | 5 | 0 | 0 | 0x0072E4F0 |
| 0x14 | MainMenu | mainmenu | 5 | 0 | 0x20 | 0x0072DFD0 |
| 0x15 | SaveLoad | saveload | 5 | 0 | 0 | 0x0072DE80 |
| 0x16 | Chanters | chanters | 3 | 0 | 0 | 0x0072DC60 |
| 0x17 | Tactics | tactics | 3 | 8 | 0 | 0x0072DA00 |
| 0x18 | GeneralScoreboard | (no .gfx) | 5 | 0 | 0 | 0x0072D440 |
| 0x19 | ChampionshipScoreboard | (no .gfx) | 5 | 0 | 0 | 0x0072C890 |
| 0x1A | TournamentScoreboard | (no .gfx) | 5 | 0 | 0 | 0x0072BCD0 |
| 0x1B | PartyPicker | partypicker | 3 | 0 | 0 | 0x0072B3F0 |
| 0x1C | ArmyControl | armycontrol | 1 | 0 | 0 | 0x0072B1C0 |
| 0x1D | DebugOptions | debugoptions | 8 | 0 | 0 | 0x0072AFC0 |
| 0x1E | PopupLayer | popuplayer | 7 | 0 | 0x20 | 0x00709C30 |
| 0x1F | FadeMap | fademap | 3 | 0 | 0 | plain |
| 0x20 | Store | store | 3 | 10 | 0 | 0x00729D60 |
| 0x21 | LightLoading | load_light | 8 | 1 | 0x10 | 0x007280B0 |
| 0x22 | FadeToBlackLoading | load_fade | 8 | 0 | 0 | 0x00728ED0 |
| 0x23 | CustomLoading | load_custom (no .gfx) | 8 | 1 | 0x10 | 0x00728BC0 |
| 0x24 | Saving | savescreen | 8 | 6 | 0x20 | plain |
| 0x25 | ItemUpgrade | itemupgrade | 3 | 0 | 0 | 0x00730490 |
| 0x26 | Crafting | crafting | 3 | 8 | 0 | 0x00727E20 |
| 0x27 | Login | login | 6 | 0 | 0x20 | 0x0061E950 |
| 0x28 | NavBar | navbar | 4 | 0 | 0 | plain |
| 0x29 | Quickbar | quickbar | 4 | 0 | 0 | plain |
| 0x2A | Minimap | minimap | 1 | 0 | 0 | plain |
| 0x2B | Portraits | portraits | 1 | 0 | 0 | plain |
| 0x2C | Notifications | notifications | 7 | 0 | 0 | plain |
| 0x2D | Tooltips | tooltips | 8 | 0 | 0 | 0x00725410 |
| 0x2E | LevelUp | levelup | 3 | 8 | 0 | 0x00734230 (via base 0x007FD930) |
| 0x2F | FloatyLayer | floatylayer | 0 | 0 | 0 | 0x00724FE0 |
| 0x30 | Slideshow | slideshow | 5 | 0 | 0 | plain |
| 0x31 | LoginConnecting | loginconnecting | 7 | 0 | 0x20 | plain |
| 0x32 | PurchaseConfirm | purchaseconfirm | 6 | 0 | 0x20 | 0x00724E60 |
| 0x33 | LoginNewAccount | loginnewaccount | 6 | 0 | 0x20 | plain |
| 0x34 | PRCContentManager | prccontentmanager | 5 | 0 | 0x20 | 0x007243C0 |
| 0x35 | LoginViewTOS | loginviewtos | 7 | 0 | 0x20 | plain |
| 0x36 | GUIScreenshots | guiscreenshots | 5 | 0 | 0x22 | 0x007223C0 |
| 0x37 | GUIAchievementUnlocked | guiachievementunlocked | 7 | 2 | 0 | 0x007075E0 |
| 0x38 | GUICommunityFeatures | guicommunityfeatures | 6 | 0 | 0x22 | 0x006273A0 |
| 0x39 | ContentInstalling | contentinstalling | 7 | 0 | 0x20 | plain |
| 0x3A | ItemExamine | itemexamine | 4 | 0 | 0x20 | 0x00720460 |
| 0x3B | BookBack | bookback | 2 | 0 | 0 | 0x007178B0 |
| 0x3C | BookFront | bookfront | 4 | 0 | 0 | plain |
| 0x3D | PurchasingContent | purchasingcontent | 7 | 0 | 0x20 | plain |
| 0x3E | Credits | credits | 6 | 0 | 0x20 | 0x0071F980 |
| 0x3F | NewGame | newgame (only in patch ERFs) | 6 | 0 | 0x20 | plain |

Notes (checked): the movie list in the install (68 .gfx) has no skills.gfx, contextmenu.gfx, mainui.gfx, load_custom.gfx or scoreboard movies, so those scenes have no movie in this build (inferred: dead or optional). `abilities.gfx`, `dialoguepop.gfx`, `loginconnected.gfx`, `fonttest`, `gfxfontlib` and `fonts_*.gfx` are not scene movies (inferred: loaded by other movies' `LoadMovie`, or font libraries). The three scene ids that the layer map shows (probe, GUI.md) all match this table.

### Groups

- **Start / main menu**: StartMenu 0x12 (shown by GameModePregame enter 0x0064CB50, hidden by its leave 0x0064CF30; the script command `ShowStartMenu` also), MainMenu 0x14 (the Escape menu, `ToggleFullScreenGUIMessage MainMenu`), Options 0x11, SaveLoad 0x15, NewGame 0x3F, Credits 0x3E, Login family 0x27/0x31/0x33/0x35, PRCContentManager 0x34, ContentInstalling 0x39, PurchaseConfirm 0x32, PurchasingContent 0x3D, GUICommunityFeatures 0x38.
- **HUD pieces** (layer): FloatyLayer 0x2F (0), ArmyControl 0x1C, Minimap 0x2A, Portraits 0x2B (1), NavBar 0x28, Quickbar 0x29 (4), Notifications 0x2C (7), Tooltips 0x2D (8), PopupLayer 0x1E (7), ContextMenu 0x03 (7). Main 0x07 (layer 1) is not used by play (probe).
- **Conversation / story**: Conversation 4 (layer 5), Slideshow 0x30, Cutscene 5 (its subtitles use scene 4 per the Cutscene mode's show, inferred), DeathScreen 0x13, FadeMap 0x1F.
- **Inventory / character / level-up**: Inventory 8 (also the loot panel), CharacterRecord 0, LevelUp 0x2E, Skills 0xD and SpellsTalents 0xA (no movie), Tactics 0x17, Crafting 0x26, ItemUpgrade 0x25, Store 0x20, ItemExamine 0x3A, BookBack 0x3B and BookFront 0x3C (the book frame).
- **Maps / journal**: Map 0xF, WorldMap 0x10, Journal 9.
- **Chargen**: CharGen 1 (and LevelUp reuses the chargen mode). Party picker: PartyPicker 0x1B (GameModePartyPicker).
- **Loading**: Loading 0xE (load_town), LightLoading 0x21, FadeToBlackLoading 0x22, CustomLoading 0x23, Saving 0x24, WorldMap 0x10 as a loading screen during travel.
- **Debug**: Debug 6, DebugOptions 0x1D, GUIScreenshots 0x36.

### How screens are opened and closed (checked unless marked)

**The by-id slots of `DragonAgeGUISystem`** (vtable 0x00B03504): slot 13 (+0x34, 0x0060AEB0) ShowScene(id); slot 14 (+0x38, 0x00606650) Close(id) (scene slot 6); slot 15 (+0x3C, 0x006066F0) Hide-keep (scene slot 7); slot 16 (+0x40, 0x0060F240) pop all full-screen ids; slot 17 OpenScreen(name) 0x00523BF0; 18 CloseScreen(name) 0x00523FF0; 19 HideScreen(name) 0x00523CB0; 20 CloseById 0x00524140; 21 cache an id 0x00524070. Slots 27-29 (0x00606CA0, 0x00606CF0, 0x00606D50) show / close / keep-hide the seven HUD scenes (0x2F, 0x28, 0x1C, 0x2A, 0x2B, 0x29, 0x2C) and, for hide, first hide ContextMenu. Slots 30-67 and 85-90 are Show/Hide pairs per scene id, in this order of id: 0x12, 0xE, 4, 0x30, 5, 3, 2 (no scene), 8, 9, 0xA, 0xD, 0x11, 0xF, 0x10, 0, 1, 0x16, 0x13, 0x17, 0x1F, 0x2E, 0x1B. `ShowInventory/Journal/CharGen` handlers (0x0060C530, 0x00606780, 0x006067A0) and the hide handlers (0x00606770, 0x00606790, 0x006067B0, 0x00606750 for ContextMenu) are these wrappers.

**ShowScene(id) 0x0060AEB0**: refuses id -1; if the scene's shown byte (+0xBC) is set it does nothing. Otherwise calls the scene's slot 5 (Show: `OpenScreen(name)` through the system, plus the `b` bits), then raises script event `EVENT_TYPE_GUI_OPENED` (84) with the id (event object type 0x54, delivered through 0x0064AD80). Special: id 8 raises `EVENT_TYPE_TRAINING_GUI_INVENTORY_OPENED` (1063, message 0x427) to the player's object unless it is a container view; id 0x2E raises 1062 (0x426), id 0x17 raises 1061 (0x425). id 0x27 (Login) takes a special path.

**Full-screen GUI** (`SetFullScreenGUIMessage` 0x0060B260): name -> id; id -1 pops all. Otherwise `SetGameMode(3,0,0)` (0x005D5E90) and `GameModeGUI::SetScreen(id)` (0x007183C0, stored at +0x98), which if the mode is GUI calls 0x00611740: pop every shown full-screen id (0x0060F1E0 hides through slot 14), then 0x00610CE0 shows the id (slot 13) and pushes it on the vector at +0x1E4. `HideFullScreenGUIMessage` (0x0060B2E0): mode-specific clean-up (0x0060B200) then pop all. `ToggleFullScreenGUIMessage` (0x0060F2A0): hide if the id is the stack's last, else Set. `PushGUIScreenMessage` (0x00610D20) shows by name and pushes without popping (stackable); `PopGUIScreenMessage` (0x0060F270) hides the last and, if the stack is empty, runs the clean-up. `CycleFullScreenGUILeft/Right` (0x0060F3C0 / 0x0060F3B0, via 0x0060EE60) step through the stack/list. Any scene name works for Toggle/Push.

**Game mode and pause**: `GameModeGUI` enter (0x007187D0) calls `SetPaused(true,false)` (0x005D6740), records whether the game was already paused (+0x94), activates the input context "GameModeGUI" (0x00890CA0), shows the id. Leave (0x007188C0) deactivates the context, pops all full-screens, sends the player an 0x692E60 notice, and unpauses only if it was not paused before. Other modes: Dialog enter pauses and its leave unpauses (0x0068FE40, 0x006910D0); CharGen enter pauses (0x00719540) and its leave closes scene 1 and unpauses (0x00719690); Pregame shows StartMenu (0x0064CB50); Dead shows DeathScreen (enter 0x0076E0F0) and hides it on leave (0x0076E2D0); Loading shows the loading scene whose id is at the mode's +0x14C (enter 0x006C5950, leave closes it, 0x006C5A00); Explore (enter 0x0070E820) shows ContextMenu-related set-up and sets pause as needed; PartyPicker enter shows its scene (0x0076CF40, inferred ids 0x1B/0x22). The other modes (Combat, Fixed, FlyCam, Movie) touch no scenes in enter/leave.

**Loading screen choice** (checked, `GameModeLoading` 0x006C63F0, jump table 0x006C64FC on the loading kind 0..8; corrected 2026-10-19 by lib-agent-2, `dev/LOAD_EVENTS.md`, "The loading mode's screen"): kinds 0 and 1 show nothing; 2 and 8 Loading 0xE; 3 Saving 0x24; 4 Loading 0xE unless 0x006C6060 finds the world map usable, in which case WorldMap 0x10 is the loading screen; 5 LightLoading 0x21 (same exception); 6 0x006C60F0 (the fade path, no new screen); 7 nothing but the world map, though that is not what a door shows: the kinds are also the loading mode's states, and slot 14 runs again at each state's change, so a kind-7 request goes on to show 0x21 or 0xE (below). CustomLoading 0x23 and FadeToBlackLoading 0x22 are never chosen: no `SetScreen` of either exists, and the `push 0x23` at 0x006C7F34 is the load-failure routine's error code (0x006C5A60), not a screen. `SetScreen(id)` is 0x006C5FE0 (hides the old, shows the new at once if the mode is current), the only writer of the mode's +0x14C.

**Who sends the messages**: key bindings are text commands registered per input context (checked, registered in one routine near 0x006DD400-0x006E7500): context GameModeGUI: Hide full screen, Toggle CharacterRecord / Journal / Inventory / Skills / SpellsTalents / Options / Map / WorldMap / Tactics, PopGUIScreen, CycleFullScreenGUILeft/Right, ShowAllGuiLabels; the explore context binds the same toggles plus `UserTogglePauseMessage`, `ToggleFullScreenGUIMessage MainMenu` (Escape) and `ToggleMainInterfaceMessage`; ability targeting binds `ToggleFullScreenGUIMessage MainMenu` too. Movies send the same text through the `SendMessage` fscommand (`GUI.HideFullScreenGUI`, `GUI.GoToLevelUp` etc., GUI.md). `GUI.HideGuiScene <name>` (0x0051B840) = CloseScreen(name). Scripts: `OpenInventory`, `OpenStore`, `ShowCraftingGUI` (-> Crafting 0x26 as full screen, 0x0060B300), `ShowPartyPickerGUI` / `SetPartyPickerGUIStatus` (GameModeGUI's `ShowPartyPickerMessage`), `OpenPrimaryWorldMap` / `SetWorldMapGuiStatus`, `ShowStartMenu`, `StartCharGen` (mode 11), `ShowDeathScreen` (mode 2), `BeginSlideshow` (mode 4), `LoadCutscene` (mode 5), `ShowPopup` (PopupLayer), `ShowChantersGUI`, `Show*ScoreboardGUI`, `OpenItemUpgradeGUI`, `OpenFadeMap`, `OpenJournalPRCTab`, `HighlightGuiElement`, `ShowTutorial`, `TakeScreenshot`, `PlayMovie`. `GoToLevelUp` (0x0060C700) does not open the screen itself: it posts `EVENT_TYPE_PLAYERLEVELUP` (24, type 0x18) to the script system, whose script opens it (inferred from the event; GUI.md saw the level-up screen open in CharGen mode). `GoToStore/Forums/OnlineProfile` open web URLs (token strings), not scenes (checked for the store handler 0x0060B4D0: URL with "?token=").

### Stacking and modality

**Draw order = layer, ascending, later-registered above on ties** (checked). A movie is registered with the renderer when it is first stepped active (record +0x29, `0x005187A0`): renderer slot 51 (0x0052B490) takes (view, layer, rect) and inserts into the renderer's map at +0xF4 whose tree compare is `new.layer < node.layer` (0x0052B320), so equal layers go after (above). The renderer draws the map in order calling the view's Display (view vtable +0x7C) in the render passes at 0x00528640 / 0x005287B0. The movie is removed when the record is hidden/closed (`0x005187F0`). The manager's own record set (+0x70) is also ordered by layer (record +0x2C, comparator 0x00510900), duplicates allowed. Record +0x2C is OpenMovie's third argument (the layer); OpenMovie's first argument (scene id) is not stored in that field. A part not read: whether the later-listed Eclipse GUI pass draws these same views a second time; the loading task `UpdateAndRenderLoadingGUITask` (0x00528940) draws the renderer's same map during loads.

**Mouse input** (checked, manager's per-frame 0x00515680): view `HitTest` (view vtable +0xA4) picks the movies under the cursor (0x00515560, active records only); they are visited top layer down; the first record visited with flag 0x20 sets "blocked", and every movie below it gets the mouse event with the enable flag off. So a modal movie blocks only where its own shapes are hit (inferred: PopupLayer, MainMenu, Options-type movies have a full-screen shape). Explore's world pick refuses while the mouse is over any movie element (`MouseOverInterface` 0x00511420, GUI.md).

**Keyboard / game messages**: input contexts. `GameModeGUI` enter activates the context named after the mode; fscommands `CreateContext`, `ActivateContext`, `DeactivateContext`, `PushContext`, `PopContext`, `BlockGameStatesByDefault`, `EnableGameState`, `BlockGameState`, `EnableGameMessage`, `BlockGameMessage`, `BlockGameMessagesByDefault` (0x0051BF70 ... 0x0051FA40) let a movie make its own (names read only; their semantics not decoded).

**Record flags** (checked in `MovieRecord::Step` 0x0051ADA0 and `Rewind` 0x00518E00): 0x04 rewind/restart the movie (`StartSceneFromGame`) on each show; 0x10 the record is stepped even in the "paused/step-only" pass (loading screens run while the game is paused); 0x08 selects the second of the two time deltas passed to Step (inferred: real time against game time); 0x02 frame catch-up off; 0x20 modal mouse; 0x01 extra set-up in the record constructor (0x0051A171, creates an object bound to the view; effect not read). Loading screens carry 0x10; Saving carries 0x20.

**Kept loaded vs on demand**: everything is on demand (Show = OpenMovie loads the definition if absent and creates the view; Close drops the record). Kept resident for the whole game in practice (shown at start and never closed during play): the nine HUD scenes plus PopupLayer (probe, GUI.md). `GUISystem::CloseScreen` drops the cached movie definition too unless the id is in the cached-id list at +0x6C (`0x00524070` adds ids; `0x005241A0` later drops uncached definitions that have no movie). Which ids are on the cached list was not enumerated.

### Not read

- The consumer of the renderer's second use of the layer (whether any code besides the +0xF4 map reorders views).
- Meaning of scene `b` bit 2's call (0x00510CA0 -> 0x00522C30 with a wide string at 0xB05DAC) and record flag 0x01.
- The cached-id list's contents, and what slot 12/24 data keys the by-name lookup (name -> id only seen as slot 12).
- Which exact call sites show PopupLayer (0x1E: seen at 0x005DC158 in CClientExoApp code), Tooltips, ItemExamine, FadeToBlack and the Saving screen (0x0073721D / 0x007373F0), and `ShowDialogBoxMessage` (0x0060BB60); only the callers' addresses were listed: Show 0x24 at 0x0073721D, Close 0x24 at 0x007373F0, Show 0x21 at 0x005CA4FA, 0x007E3EC8, 0x007E44E6, Show 0x22 at 0x00765968, Show 0x1B at 0x0076C86B, Close 0x2D at 0x005D71B4.
- Whether `contextmenu`, `skills`, `mainui` exist in DLC archives.
- Scripts' exact use of every GUI command above (names from the command table only).

## The Chanter's board (read 2026-10-21 by lib-agent-2 with a subagent, for DAOParagonEngine)

Read-only. C = read in code or data this session; I = inferred. Exe disassembly with `tools/daoexe.py`; tables with ghilana; the movie's ActionScript at `%TEMP%\lib-agent-2-guicmd-x\allgfx\as_chanters\scripts\__Packages\Chanters.as`. Prior notes: `dev/script_commands/inventory.md:195-199` (ShowChantersGUI, already had tables 115/116 and the accept handler), `dev/GUI.md:142` (movie record), `dev/gui_data_paths.csv:167-175`, `dev/message_handlers.csv:276-277`, `dev/PLOTS.md:5-16` (Get/SetPlotFlag). New here: the per-entry rules, what fills each field, what Accept does, and the Clear handler (the csv has function_va 0 for it; it exists).

### Short answers

1. `ShowChantersGUI(n)` (0x00662F40) calls `Journal::BuildChanterBoard(boardId)` at 0x006F46A0, then opens the full-screen `Chanters` scene. The board is a new `ChanterBoard` object (0xD0 bytes) held in the journal's attribute at Journal+0xE8, with a vector of entries (0x124 bytes each) at board+0x58. The board comes from table 115 `jobboards` row ID = n (Texture, HeaderStrID, Label); entries come from table 116 `jobboardplots` rows, in table row order, whose `BoardID` = n, subject to three tests per row (below). Per entry: PlotResRef = the row's `PlotRef`; PlotName = the plot file's PlotName text (field 13008 via 0x0075C430); JournalText = the text of the plot flag number `VisibleFlag` (its PlotFlagJournal, field 13004 via 0x0075C720, through 0x0075F520); Accepted = whether that PlotRef is already in the journal's first plot list (0x006EBB70). Not read: the row's `OfferID` (no reference; every shipped row has 0).
2. `AcceptChantersQuestMessage` (0x006EB850, message field +4 = entry index): bounds-checks the index against the board's entry count (false if out of range), sets the entry's Accepted attribute to 1 at once, then calls `SetPlotFlag(plotGuid, entry.AcceptedFlag, 1, 1, X)` (0x0075F7E0). That runs the plot's script with EVENT_TYPE_SET_PLOT (27) before storing the flag (dev/PLOTS.md:9-10). It does not raise another event and does not refresh the board; the movie reads `Accepted` straight afterwards. Returns true.
3. `ClearChanterBoardMessage` handler is at 0x006F3BC0 (the csv shows 0): `call 0x006F2580; mov al,1; ret 4`. 0x006F2580 empties the entry vector (0x0068F840 deletes each entry), deletes the board object, sets Journal+0xE8 to null and notifies the attribute. The movie sends it from `Chanters.FinishScene` (`SendMessage "Journal.ClearChanterBoard"`). 0x006F46A0 also calls 0x006F2580 first, so a stale board never survives to the next open.
4. Reacting to acceptance: whatever the plot's script (the `.ncs` named by the plot file) does on EVENT_TYPE_SET_PLOT for the AcceptedFlag; typically it sets a journal update and spawns the quest. Nothing in the engine or the other game scripts reads the board. Only the movie's seal animation reacts (Accepted==1 -> AcceptedSeal_mc plays, Accept button disabled).

### ShowChantersGUI callers (C, grep of the .nss)

Board IDs by caller: Lothering board `lot100ip_chantry_board.nss:56` -> 3 (on PLACEABLE_ACTION_EXAMINE); Denerim `den200ip_chanter_board.nss:55` -> 5; Redcliffe `arl100ip_chantry_board.nss:36` -> 13; Light Content boxes: `litip_blackstone_box.nss:41/45/49` -> 6/7/8 (fighter boxes), `liteip_rogue_box.nss:41` and `lite_rogue_box.nss:58` -> 9, `litip_mage_bag.nss:41/45/49` -> 10/11/12. Lake Calenhad (4) and boards 1, 2, 20 have table rows but no shipped caller seen (board 20 has no table-115 row at all).

### 0x006F46A0 in order (C unless marked)

`this` = the journal. It clears any old board (0x006F2580), allocates a 0xD0-byte ChanterBoard (ctor 0x006F3070) and stores it at Journal+0xE8, then:

1. Table 115 (`GetTable(0x73)`, 0x004DF580) row `nBoardID`; reads three columns by GDA hash: `Texture` 0xDD5E6F00 (string), `Label` 0xB49CA166 (string), `HeaderStrID` 0x46E25C69 (int) (hashes checked with `tools/gda.py` column_hash). If any of the three reads fails, or table 116 is missing, the board stays empty (the movie then shows "no quests", `LOC_CHANTERS_NO_QUESTS`).
2. Header: board+0x74 (token path `Journal.ChanterBoard.Header`) = HeaderStrID as a talk-table string if non-zero; if zero, a string built by 0x004C35F0 from a format at 0x00AEC7B4 and the Label (not read further, I). Board+0xA8 = Texture.
3. Board+0x58 vector is cleared again, then table 116 (`GetTable(0x74)`) is walked row by row from 0 to GetRowCount (vtable +0x14, rows by +0x38). A row is used when: (a) `BoardID` 0xE24463FF == nBoardID; (b) the columns `VisibleFlag` 0x9623699D, `AcceptedFlag` 0x861798A2 (ints) and `PlotRef` 0x67CF4C46 (string) all read; (c) `Journal::0x006EBBF0(PlotRef)` is false, i.e. the plot is not in the journal's second plot list, which by the ctor order (+0x58 then +0x74 registered after PlotList, CompletedPlots; the Journal class lists are PlotList and CompletedPlots, dao/Journal.cpp:11) I read as CompletedPlots (I for which list is which: the offsets +0x58/+0x74 are C, the names are I). So a quest already completed leaves the board.
4. Then the entry is created by 0x006F3370(plotRef, visibleFlag, acceptedFlag, accepted) (C):
   - `GetPlotGUID(PlotRef)` (0x0075CB30, the plot manager is `[[app+0xD0]+0x58]+0x210`) must succeed, and `GetPlotFlag(plot, VisibleFlag, 0, -1)` (0x0075EA60) must return true. So **an entry is shown only when the plot's flag number `VisibleFlag` is currently set** (plot flag, stored 0-127 in the plot's words). Both can drop the row silently.
   - Entry (ctor 0x006ECC40, attribute names read at 0xB14070.. as wide strings): `PlotName` (+0x58) = 0x0075ED50(plot guid) = the plot's PlotName (13008 via 0x0075C430, text resolved as a LocString); `JournalText` (+0x8C) = 0x0075F520(plot guid, VisibleFlag) = the PlotFlagJournal (13004, via 0x0075C720) of flag number VisibleFlag, as text; `PlotResRef` (+0xD0) = the row's PlotRef; `Accepted` (+0xC0) = the boolean result of 0x006EBB70(PlotRef) i.e. "PlotRef is in the journal's first plot list" (already accepted, I for the list's name); entry+0xF8 = AcceptedFlag (raw int, not exposed to the movie).
   - The entry is appended to board+0x58.
5. Row order is table order (ghilana merges as the game does); there is no sort and no de-duplication; a PlotRef listed under two rows of the same board would appear twice.

Table 116 columns, as ghilana reads it: ID, PlotRef, BoardID, VisibleFlag, AcceptedFlag, OfferID (69 rows). Examples (C): board 3 (Lothering) has lot100pt_bears (Visible 3, Accepted 2), wrd100pt_south_threat (6, 0), wrd100pt_far_afield (6, 2), wrd100pt_merchants_loss (5, 1), wrd100pt_missing_prospe (7, 0), wrd100pt_last_rites (3, 0), lot100pt_bandits2 (5, 0), lot100pt_last_keepsake (3, 0). Light-content rows are Visible 0, Accepted 1. Table 115: 13 rows, all Texture `default_board.tga`; HeaderStrID 337587 (3, Lothering), 337589 (5, Denerim), 392140 (13, Redcliffe Chantry); HeaderTintIndex (columns ShowDonationButton, DonationScript `donate.ncs`, DonationMsgStrID, HeaderTintIndex) are not read by 0x006F46A0 (no hash use seen; the donation/tint columns are probably read elsewhere or unused, not read).

### AcceptChantersQuest in detail (0x006EB850, C)

- Message layout (dev/messages.csv:2): 8 bytes, vtable 0x00B1E784, int at +4 = the entry index; the movie sends `Journal.AcceptChantersQuest <nPlotIndex>` (`Chanters.as`: `SendMessageArgs("Journal.AcceptChantersQuest", Chanters.nPlotIndex)`). Created by the generic text-command path (the factory at 0x0077DA90 copies it).
- Steps: board = `Journal+0xE8` attribute (0x0053CC40); entry vector = board+0x58; if `index >= count` -> return false. entry = vector[index]; if null -> return true without doing anything. `entry+0xC0.Set(true)` (vtable +0x54, arg 1): Accepted = 1 (observers/data path update at once). App `[0x00C327EC]`, plot manager = `[[app+0xD0]+0x58]+0x210` (0x005D57C0 then +0x210); if null, return true. Convert `entry.PlotResRef` (+0xD0) to the plot GUID with 0x0075CB30; if that succeeds, `SetPlotFlag(mgr, guid, entry.AcceptedFlag (+0xF8), 1, 1, [[app+0xD0]+0x5C])` (0x0075F7E0; the last argument comes from 0x005D5CA0, a value in the same object, probably the hero's object id, I). Return true.
- So the "acceptance" is **only the plot flag `AcceptedFlag` set to 1 with the plot script run** plus the in-memory Accepted attribute. The quest's journal entry is produced by the plot (the flag's PlotFlagJournal, handled by the plot system, I), not by this handler. The tick can be repeated: Accept is not blocked by the engine if Accepted is already 1 (the movie disables the button, `Chanters.as` RefreshPlotInfo).
- Movie side (C, Chanters.as): after sending it reads `.Accepted == 1`; if so, plays the seal on the list item and the page and disables the button. On open the button is hidden if there are no entries. Sounds: ST_JOB_BOARD_OPEN/ACCEPT_QUEST/SELECT_QUEST. On close (`FinishScene`) it sends `Journal.ClearChanterBoard`; the close button sends `GUI.HideFullScreenGUI`.

### What reacts to acceptance (scripts)

- The AcceptedFlag numbers come from table 116 and are flags of the board's plot (`<plotref>.plo`, `plt_*.nss` constants); the plot script is run on EVENT_TYPE_SET_PLOT (27) with the hero as OBJECT_SELF, string 0 = plot GUID (dev/PLOTS.md:14-20). The scripts that react are therefore each plot's own (`lot100pt_bears`, `wrd100pt_*`, `lite_*` plot scripts under `toolset\extract\scripts\`, e.g. `Light Content\Plot Scripts\Rogue Board\lite_rogue_box.nss`), plus area scripts that read the plot flags later (e.g. `arl100ar_redcliffe_village.nss:112-113, 274-279` activates the Redcliffe chanter and board and tests `ChanterTurnInPossible()`). I did not read each plot script (not read).
- The VisibleFlag on those plots is set by the area/plot scripts as quests become available (and cleared from the board once the plot reaches CompletedPlots).

### Not read

- Which of Journal +0x58 / +0x74 is PlotList vs CompletedPlots (offsets C, names I); the exact contents of 0x006EBB70 beyond "string compare of element+0x58 with PlotRef".
- The Label/format fallback for a zero HeaderStrID (0x00AEC7B4 string); HeaderTintIndex, ShowDonationButton, DonationScript, DonationMsgStrID, ModuleRef readers (donation UI not in the AS I read).
- Each plot script's reaction to SET_PLOT for the AcceptedFlag; the PlotFlagJournal-to-journal path.
- Why board 4, 20 and 1/2 have no caller; whether `ModuleRef` filters rows (no use seen).
- The type of the last argument of SetPlotFlag (0x005D5CA0).

## Part (a): the 67 fscommands and 17 ExternalInterface calls (GOG exe, address library 9eae06b names)

Read 2026-10-18 by lib-agent-2 (for DAOParagonEngine #19).

### Short answer

- Registration is one constructor, `GuiCommands::GuiCommands` (0x00521AF0), which calls `RegisterFSCommand` (0x005201C0) 51 times and `RegisterExternalInterface` (0x00520280) 12 times with a wide-string name and a handler pointer; the tactics/item group (0x00713F00..0x007163D0) is registered by a second routine (0x00716540, 16 fscommands and 5 externals). Total 67 + 17, checked by walking both routines' disassembly (script in %TEMP%\lib-agent-2-guicmd-x\p2.py).
- Dispatch: `ScaleformManager::DispatchFSCommand` (0x00510980, `ret 0xc`: movie, command char*, args char*) and `DispatchExternalInterface` (0x005109B0, `ret 0x10`: movie, method, args array, count). Both take the movie's record from the movie's user data (movie vtable +0xAC, slot 43), CRC the name (0x4CA960) and look it up in the two maps (`GuiCommands::Find`, 0x0051F4F0). FS handler signature is `cdecl(record, const char *args)`; EI handler is `cdecl(record, const GFxValue *args, int count)`. A name with no handler is silently ignored (checked: `je` over the call). The movies call two names nobody registered: `MoveToFront` and `MapGameState_Float` (the latter from the credits/chanters scroll windows, with `GetGameStateValue_Float` as its reader): no-ops.
- Arguments: an fscommand has ONE string argument; every multi-argument fscommand is the movie joining its arguments with single spaces (`a + " " + b`), and the handler splits on the first space(s) (std::wstring::find(0x20) / substr). Tokens are numbers or names; the LAST token may contain spaces (SendMessage, SetValue/SetValueUnicode, GetTlkString fallback). ExternalInterface calls pass real typed values (number/string).
- Return convention, FS: handlers answer by `GuiMovie::SetFSCommandReturnValue` (0x00518830) = movie->SetVariable("_root.fsc_retval", value, sticky=1) with a GFx value {type, data}; types are 3 = number (double), 5 = wide string, 2 = bool (inferred from the AS using true/false), arrays set by the movie's array setter (slot 13) with SetArrayType into `_root.fsc_retval` (and `_root.fsc_retval2`). The movie's AS clears the variable first (`_root.fsc_retval = null/0/""`), calls `fscommand`, then reads `_root.fsc_retval` immediately, so the handler runs synchronously inside the fscommand call. A handler that finds nothing leaves the movie's pre-set default.
- Return convention, EI: `movie->SetExternalInterfaceRetVal(&value)` (movie vtable +0xA8, slot 42), also synchronous. Two EI calls (GetItemAttributes, GetAbilityAttributes) answer instead by writing an array into a `_root.Returned*` variable the AS reads right after.
- Unused by the 56 non-font movies' ActionScript (checked by grepping all decompiled AS): FS `PostMessage`, `PlaySound`, `RemoveTokenObserver`, `RemoveTokenObserverByID` (the movies batch removals instead, see below), `GetGameStateValue_Bool/Int/Point` (these three are registered to a bare `ret` at 0x005BDCF0, so they do nothing), `EnableGameState`, `BlockGameState`, `BlockGameStatesByDefault`, `BlockGameMessagesByDefault`; EI `AddTokenObserverWithID` (the movies queue observers in the AS array `TokenManager.AddTokenObserverBatchData` instead), `GetItemAttributes`, `GetString`-as-distinct (it is `jmp GetValue`, 0x0051EE00). Also `GetNumM2DARows` shares the handler of `GetNum2DARows` (0x0051C4B0). `HideGuiScene` the movie does not call as an fscommand: ExternalCommands.HideGuiScene only sets `_root.WaitingToBeStopped` (the C++ side polls it; not read).

Movie-side wrappers (the contract in AS, read from `ExternalCommands.as`, `DAExternalCommands.as`, `ContextSystem.as`, `TokenManager.as`, identical in every movie) are the best statement of "arguments as the movie sends them"; below, "arg" is what the string/array holds.

### 1. Messages and movie control (FS)

| Call | Arguments | Engine does | Returns | Read |
|---|---|---|---|---|
| SendMessage 0x0051B5E0 | one string, "MessageName arg arg ..." (movie joins with spaces; `ExternalCommands.SendMessageArgs`) | `Messages::FromCallbackText` (0x524BE0) builds a message object from the text, `Messages::Send` (0x4D20C0) delivers it synchronously, object released via its vtable slot 4 | nothing | checked |
| PostMessage 0x0051B680 | same text | same parse, then `MessageRegistry::Get()` queue (0x4D8C60) instead of Send: queued | nothing | checked (unused by movies) |
| LoadMovie 0x0051B720 | movie name | `ScaleformManager` (global 0xC31264) 0x514AF0 with the name, opens the movie by name | nothing | name+call checked; callee identity inferred |
| PlayMovie 0x0051B790 | a Flash movie's name | allocates a 0x20-byte `PlayMovieMessage` holding the name and posts it (0x4D20E0); its handler (0x00514C70) calls `ScaleformManager::OpenMovie(-1, name, -1, 0)` (0x00514B10), so it opens a **Flash movie**, as `LoadMovie` does, and never reaches Bink (corrected 2026-10-19 by lib-agent-2; a video is `dev/CUTSCENES.md`'s "A movie (.bik)") | nothing | checked |
| StopMovie 0x0051CBF0 | video name (movie sends "" for none) | posts a `StopMovieMessage` (0x20 bytes, 0x51B130 ctor) | nothing | checked |
| HideGuiScene 0x0051B840 | scene name | `app->GUISystem (+0x158)` vtable slot 18 (+0x48) = close/hide screen by name | nothing | checked (movies do not call it) |
| LoadModule 0x0051B8D0 | module name ("single player", `DAConstants.DAO_MODULE_NAME`, `EP1_MODULE_NAME`, "ep1_NewGame" ...) | if the calling movie is `startmenu2`: "single player" with DAO_PRC_EP_1 present sends a NewGame-text message, "ep1_NewGame" sends online telemetry (`SendOnlineTelemetry`); anything else falls to the app's load-module call (0x5D95C0 on global 0xC327EC) with the name | nothing | strings and branch structure checked; what 0x5D95C0 does inferred |
| QuitGame 0x0051BB50 | none | `app singleton (0x402CA0)` -> 0x4031F0, quits the game | nothing | checked |
| AdvanceFrame 0x0051CCC0 | none | if the record's state (+0x104) == 2, takes the movie view and calls its Advance (vtable) once, synchronously | nothing | calls checked, exact slot inferred |
| SetTimeScale 0x0051BEE0 | float string | `StringToFloat`; stores it in the movie record at +0x30 (0x5184E0 clamps against a constant at 0xAE7308) = this movie's playback speed | nothing | checked |
| SetCursor 0x0051C420 | cursor name | stores the name wstring in the record at +0xE4 (the movie's cursor) | nothing | checked |
| Warning 0x0051B3A0 | text | converts the string to a wstring and frees it: a no-op (the logging was removed) | nothing | checked |
| PlayGUISound 0x0051BB60 / StopGUISound 0x0051BC50 | "soundType eventName" (type is an int, `SoundEvents.ET_CLICK` default; `m_nSoundType` e.g. 95) | StringToInt for the type, wstring for the event, then `0x510CA0` (GUI/audio singleton) -> 0x522C30 (play) / 0x522DC0 (stop) | nothing | checked structurally; the callees' meaning inferred |
| PlaySound 0x0051D300 | event name (narrow->wstring) | 0x510CA0 singleton -> 0x522BF0 | nothing | inferred; unused by movies |

### 2. Token tree (SmartData::TokenManager data binding)

All use `SmartData::TokenManager::Get()` (0x455BF0) + `Find(path)` (0x5C6E80), sentinel "not found" = 0xBF99E0.

| Call | Arguments | Engine does | Returns |
|---|---|---|---|
| GetValue (FS) 0x0051D3B0 | dotted path | Find; reads the attribute | `fsc_retval` = number (type 3) or wstring depending on the attribute (the case at 0xBF99E0 is "not found") | checked partially (path, SetFSCommandReturnValue); typing inferred |
| SetValue 0x0051BD40 | "path value" (number or bool 0/1) | Find, then attribute vtable slot 10 (+0x28) `Set` (notifies observers) | nothing | checked |
| SetValueUnicode 0x0051F6E0 | "path text..." (text may contain spaces) | same, string attribute | nothing | checked structurally |
| GetNumMembers 0x0051BE40 | path | Find, attribute vtable +0x40 (child count) | number (3) | checked |
| ListMemberNames 0x00520340 | path ("" = root) | Find (or root list 0x5C6F20), iterates the children (names), builds a string array | `_root.fsc_retval` = array of names via movie vtable slot 13 (+0x34) | checked structurally |
| GetValue (EI) 0x0051EC60 | (path:string) | same Find; arg[0] must be type 4 string | RetVal string(5)/number(3); missing path or bad args = a null string (type 4, null ptr) |
| GetNumber (EI) 0x0051CFD0 | (path) | same | RetVal number (3); not found = number (3, 0?) |
| GetString (EI) 0x0051EE00 | (path) | `jmp GetValue` | same as GetValue |
| AddTokenObserver (EI) 0x0051CD50 | (label, "", tokenPath) | `GuiMovie` 0x51A3C0: if the (movie, token) observer exists, returns its id; else decrements the movie's counter at +0x98 (ids count down from 0xFFFFFFFF), allocates an observer (0xA0 bytes) and stores it in the movie's maps (+0x3C by id, +0x84 by id) | RetVal number (3) = the id. The AS stores `cm_mapCallbacks[id] = callback` |
| AddTokenObserver (FS) 0x0051B3E0 | "label tokenPath" style: three space-split fields | same 0x51A3C0 | `fsc_retval` = id as double (3) (unused by movies) |
| AddTokenObserverWithID (EI) 0x0051CEA0 | (id, tokenPath [, delay]) with count >= 2 | 0x51A4A0 creates/updates an observer with a caller-chosen id | RetVal number (3) (unused; the movies push to `TokenManager.AddTokenObserverBatchData` [id, tokenPath, updateDelay] x n and the engine pulls that array in `GuiMovie::ProcessAddTokenObserverBatch` 0x51A8B0, removals via `RemoveTokenObserverBatchData` in 0x51AC10) |
| RemoveTokenObserver 0x0051B570 / RemoveTokenObserverByID 0x005207A0 | token / id | 0x51AB70 / 0x51ABC0 look the observer up in the movie's two maps, erase it, release it via its vtable | nothing | checked structurally (unused by movies) |
| ReplaceDesignerTags (FS 0x0051F410, EI 0x0051F2F0) | text with designer tags ($LOC_.. etc.) | the global `ReplaceDesignerTags` (0x51F110) | wstring (5) |

Observer delivery (engine to movie) is part (b): `GuiMovie::DeliverTokens` 0x00519B00 sets `TokenManager.BatchTokenReplacementCallbackIDs` and `...Values` and invokes `TokenManager.PerformBatchTokenReplacements`.

### 3. Data tables and strings

| Call | Arguments | Engine does | Returns |
|---|---|---|---|
| GetNum2DARows 0x0051C4B0 (and GetNumM2DARows, same code) | table id (integer = the name's hash, `StringToInt`) | `GdaStore::FindTable` on the global GdaStore (0xC1D090 + 4), table vtable +0x14 row count | number (3) |
| Get2DAValue 0x00520D30 | "table row column" (all ints; table/column are hashes) | FindTable, fetches a cell | wstring (5) of the cell (ints through IntToString, floats formatted) |
| Get2DAValueByColumnName 0x00521320 | "table row columnName" | hashes the name (0x4CA9A0) | wstring (5) |
| Get2DAValueByIndex 0x005215D0 | "table rowIndex columnIndex" | positional lookup (0x51C580) | wstring (5) |
| Get2DAValueByRowIndexColumnHash 0x00520F80 | "table rowIndex columnHash" | 0x51C580 | wstring (5) |
| Get2DAValueByRowIndexColumnName 0x00521120 | "table rowIndex columnName" | hashes, 0x51C580 | wstring (5) |
| GetM2DAValue 0x00520AD0 / GetM2DAValueByIndex 0x00520870 | "table rowID columnID" / "table row column" | FindTable on the merged table | number (3) for numeric columns, wstring (5) for text |
| Get2DAHash 0x0051EA00 | name | string hash 0x4CA9A0 | number (3) (unused by movies) |
| GetTlkString 0x0051EAE0 | "strRefId fallbackText..." (fallback optional; may contain spaces) | talk table lookup (0x4D1C70/0x4D1EB0/0x4D1E90 on the TLK); invalid id gives the fallback | wstring (5) |
| FormatFloat 0x0051D4B0 | "precision value" | StringToInt/Float, 0x620680 formats | wstring (5) |
| GetObjectNameByID 0x007160B0 | object id (int) | `CClientExoApp::GetObjectById` -> the object's name (via 0x403C70/0x4D1EB0), "" if none | wstring (5) |

(Row order of cells checked for the AS wrappers; the engine side of the 2DA handlers was read at the call level only: that the first token is a table id/hash, the types of the others come from the AS wrappers and the callee names, hashing calls seen in ByColumnName/ByRowIndexColumnName.)

### 4. Input contexts (InputModes)

All via `InputModes::Get()` (0x891010); arguments are strings the AS builds. 19 of the 56 movies call these (ContextSystem class), for gamepad/keyboard focus sets: e.g. `SETUP_FOCUS_MANAGER_CONVERSATION` enables messages "InvokeCallbackMessage conversation FocusManager.NavigateUp", "SelectResponseMessage 0..5"; `INPUTS_MAIN_MENU` blocks "ToggleFullScreenGUIMessage CharacterRecord/Journal/Inventory/...".

| Call | Arguments | Engine does |
|---|---|---|
| CreateContext 0x0051BF70 | context name | InputModes: create a named input context (0x890AE0) |
| PushContext 0x005205A0 | name | push on the context stack (0x890CA0) |
| PopContext 0x0051FA40 | name | pop it, re-activating the previous one; looks the name up in the stack first (0x51F550/0x51F910/0x507730) |
| ActivateContext 0x0051C000 / DeactivateContext 0x0051C090 | name | 0x890D40 / 0x88F8B0 |
| EnableGameMessage 0x0051C1B0 | "context messageText" (text keeps spaces, e.g. "SelectResponseMessage 3") | `InputModes::Group(context)` then `InputModeGroup::Allow(message)` (0x88F810) |
| BlockGameMessage 0x0051C2A0 | same | block that message in that context (0x88F860) |
| BlockGameMessagesByDefault 0x0051C390 | context | group default = block messages |
| EnableGameState 0x00521840 / BlockGameState 0x00521940 / BlockGameStatesByDefault 0x0051C120 | "context stateName" / context | like the message ones but for named game states (0x5217D0 / 0x5206A0 map the name) (unused by movies) |
| GetGameStateValue_Float 0x00521A40 | state name ("AnalogScrollValue", "ConversationRadialSelectX/Y") | `Input::Get()` (0x891C30) -> value of the mapped state (0x8925B0) | number (3). The credits/chanters/conversation movies read gamepad stick values here per frame (their `AnalogScroll()` / `RadialSelect()` are timed by an onEnterFrame) | structurally checked |
| GetGameStateValue_Bool/Int/Point | | all registered to a bare `ret` (0x5BDCF0) | nothing |

### 5. Game queries (DragonAgeGUI additions, tactics/items/abilities)

Registered in the second routine; `DAExternalCommands.as` gives the argument shapes. Creature/item ids are the engine's integer object ids.

| Call | Arguments | Engine does | Returns |
|---|---|---|---|
| ListTacticTargets 0x00714860 | creature id | 0x7B0A80 builds target-type list | `fsc_retval` int array (target types) and `fsc_retval2` int array (parallel object ids); set with movie slot 13 (setArray) |
| ListTacticBaseConditions 0x00714A00 | target type | 0x7AF570 | `fsc_retval` int array |
| ListTacticConditions 0x00714AF0 | "baseCondition targetType" | 0x7AEB50 | `fsc_retval` + `fsc_retval2` (condition ids) int arrays |
| ListTacticActions 0x00714E20 | "creature target base" | 0x7AFE80 | `fsc_retval` int array (command types) |
| ListTacticAbilities 0x00714FD0 | "creature target base" | 0x7AED50 | `fsc_retval` int array (ability ids) |
| ListTacticModalAbilities 0x00714D30 | creature | 0x7AEF50 | `fsc_retval` int array |
| ListTacticItems 0x00714770 | target type | 0x7AF6C0 | `fsc_retval` int array (item object ids) |
| CreatureCanUseItem 0x00715180 | "creatureId itemId" | `GetCreatureById`, `GetObjectById`, `CCCreature::CanUseItem` | number (3) 0/1 |
| GetItemStat 0x007152F0 | "itemId statId" | `ForScript_GetItemStat` | number (3) |
| GetItemRequirements 0x00715440 | "itemId creatureId" (creatureId defaults to INVALID_OBJECT_ID) | evaluates the item's requirements against the creature, builds "ok;requirementText;ok;text;..." with table lookups (strength/dex/level, using attribute hashes), falls back to "0;CANNOT_EQUIP_ON_CREATURE;" | wstring (5), AS splits on ";" |
| HasAbility 0x00715CA0 | "creatureId abilityId" | creature's ability attribute list (SmartData Attribute::Get) -> 0x71BEC0 | number (3) |
| HasAbilityPreRequisites 0x00715E10 | same | 0x71BEF0 | bool/number |
| FirstPrereqOf 0x00715F70 | ability id | 0x62CD20 (ability table) | number (3) |
| IsModalAbilityActive 0x00716250 | "creatureId abilityId" | active-modal list `Contains` | number (3) |
| HasAchievement 0x007163D0 | achievement name | 0x47A610 on the global achievement object | number/bool |

EI additions (same routine): `IgnoreItemRequirements(itemId:Number)` 0x00713F00 returns bool (the item's flag, via 0x755E50); `CanLevelUpFromCharacterSheet(creatureId)` 0x00713FE0 returns bool (creature method 0x6ACBC0); `GetGender(creatureId)` 0x007140C0 returns number (3) (0x6A3650); `AbilityExists(abilityId)` 0x0051D130 returns number (ability table 0x62C730); `GetAbilityAttributes(tokenPath)` 0x00714570 sets `_root.ReturnedAbilityAttributes` = [ID, UseType, Icon, GUIType, PrereqAbilityID, DisplayState] (index order read in Ability.as; the engine side read for the six attribute reads); `GetItemAttributes(...)` 0x007141F0 sets `_root.ReturnedItemAttributes` (no movie in the 56 uses it; fields not read).

### 6. Environment / module

`GetCurrentModuleName()` 0x0051EE10 -> RetVal wstring of `app->GetModule()->GetName()` (the AS compares to `DAConstants.DAO_MODULE_NAME` / `EP1_MODULE_NAME`); `GetMainModuleName(0|1)` 0x0051EF50 -> wstring, the base campaign's / expansion's module name (chooses DAO_PRC_EP_1 via a CRC/resource check and, in the expansion, the module record name); `SendTelemetryInfo(module, group, payload)` 0x0051C660 (3 args) -> `SendOnlineTelemetry` script backend; `FilterKeyboardInput(illegalChars)` 0x0051C760 (1 arg) -> sets the illegal-character filter on the input group "GUITextInput" (0x5110F0, inferred); EI `SendMessage(name, a1..a5)` 0x0051C800 (arg 0 must be a string) -> 0x524E30 builds a message from the name and typed args, `Messages::Send` (the typed form of FS SendMessage; used by the shared library's `SendMessageValues`).

### Ruffle/Bevy shim implications

- A replacement must implement `fscommand` -> a synchronous callback returning data by writing `_root.fsc_retval` (and `fsc_retval2`) BEFORE the AS reads it on the next statement, and `ExternalInterface.call` -> synchronous return value. Ruffle's `ExternalInterface` supports the latter natively; `fscommand` is dispatched to a handler that can set `_root` variables, but ordering must be in-call.
- Values: wide strings and doubles dominate; arrays of ints for the tactics lists.
- 39 distinct fscommand names are used by the movies (+2 unregistered); of the 17 externals, 11 are used.
- Token observers: the movie asks the engine to watch a data-tree path; the engine pushes changes back as two arrays + one invoke. Ids are engine-chosen (descending from 0xFFFFFFFF) for `AddTokenObserver`; movie-chosen for the batch form.

### Not read

Per-handler byte-level argument parsing of the 2DA family beyond call-level (which token is the hash vs the index was taken from the AS wrappers and the helper names); the exact meaning of unnamed callees (0x514AF0, 0x5D95C0, 0x522BF0/C30/DC0, 0x5184E0 target field's use, 0x7B0A80 etc.); `GetItemAttributes` entries; the movie vtable slots used for AdvanceFrame; the `HideGuiScene`/`WaitingToBeStopped` poller.


---

## Part (b): what the engine pushes into movies (GOG exe 1.05.13263, read 2026-10-18)

Read 2026-10-18 by lib-agent-2 (for DAOParagonEngine #19).

### Short answer

- The engine writes almost nothing into movies by SetVariable. Every call site of the movie's `SetVariable` (slot 11, +0x2C) with a literal path was found by a whole-.text scan (checked): `_root.fsc_retval` (the fscommand return), `_root.Filename`, `_root.OriginalStageWidth`, `_root.OriginalStageHeight` (at movie creation, `GuiMovie::GuiMovie` 0x0051A040), and `_root.WaitingToBeStopped` (cleared by `MovieRecord::Step`). Everything else is the data-tree token mechanism, a few batch arrays, and `Invoke` of named ActionScript functions.
- **Replacement contract, in one line:** a replacement UI host must (1) own the `TokenManager` mailbox protocol (movie writes `TokenManager.AddTokenObserverBatchData` / `RemoveTokenObserverBatchData` arrays, engine reads and clears them every frame, then pushes changed token values back as `BatchTokenReplacementCallbackIDs` + `BatchTokenReplacementValues` and calls `TokenManager.PerformBatchTokenReplacements()`), (2) fill the Floaty and SharedMap batch arrays and call `FloatyLayer.ProcessBatchUpdates()` / `MiniMap.Instance.UpdateMinimap()`, (3) call about 25 named ActionScript functions on events, and (4) answer fscommand / ExternalInterface (part a).
- HUD bars, portraits, quickbar cooldowns, conversation text and choices are **not pushed by C++ at all**. The movies subscribe to data-tree tokens (strings like `GUI.SelectedChar.Attributes.7.Current`) and the engine pushes the changed value as a **string** whenever the attribute changes (coalesced per observer id, delivered once per frame).
- Flash value types the engine uses (GFx 3.0 numbering, checked in the call sites): SetArray/GetArray element types 0 int, 2 float, 4 wide string, 5 value; GFxValue in an `Invoke` argument: 3 number (double), 5 wide string (pointer to wchar_t). Invoke is `Invoke(path, GFxValue* result, GFxValue* args, count)` (GuiMovie::Invoke 0x00518850 just jumps to vtable +0x48 with the same arguments).

### Movie vtable slots as the engine uses them (checked from call sites)

| offset | slot | use |
|---|---|---|
| +0x2C | 11 | SetVariable(path, value*, sticky=1) |
| +0x30 | 12 | GetVariable(value*, path) (Step reads `_root.WaitingToBeStopped`) |
| +0x34 | 13 | SetArray(type, path, startIdx=0, data*, count, sticky=1): used for every batch array |
| +0x38 | 14 | SetArraySize(path, size, sticky=1): used by the SharedMap batch (resize before filling) and to clear `AddTokenObserverBatchData` |
| +0x3C | 15 | GetArraySize(path) (`ProcessAddTokenObserverBatch`: size/3 entries) |
| +0x40 | 16 | GetArray(type=5, path, startIdx=0, dest*, count) (reads the mailbox out of the movie) |
| +0x48 | 18 | Invoke (the one `GuiMovie::Invoke` uses) |
| +0x78, +0x94 | | per-frame Advance(dt, ...) and a pre-advance call in `MovieRecord::Step` (not identified further) |
| +0x98 | 38 | CreateValue/object helper used by `Invoke` when the stage size is known (inferred) |

### When: the per-frame order (checked unless marked)

1. App update `0x005D5460(dt, flag)` (called from 0x0052A4F7): GUI system slot 8 (+0x20) `Update(dt, 0)`, then, unless the game's state at `[this+0x114]+0x78 == 9`, the floaty layer's update `0x0063E5B0` (singleton from 0x00523880).
2. GUI system update `0x00523B80` (in the GUISystem vtable at 0x00AF6764; the address library calls its neighbour `HandleMessage_SetCursorMessage`): if a stage-size change is pending `0x00511D20` invokes `MovieUtils.UpdateAllLayouts` on every movie (also reached from 0x005274AB/0x0052750E); then ScaleformManager Advance `0x00516760(dt, flag)`, which runs `MovieRecord::Step` (0x0051ADA0) for each record, then `0x00511CA0` = deliver tokens to every movie (`GuiMovie::DeliverTokens`).
3. `MovieRecord::Step` per active movie: advance the movie (+0x94, +0x78 scaled by the record's +0x30), then `ProcessRemoveTokenObserverBatch`, `ProcessAddTokenObserverBatch` (mailbox read), then if `_root.WaitingToBeStopped` holds a non-empty string (type 4) the engine posts a message with it (through global 0x00C31604 vtable +0x48; inferred to be a StopMovie request) and sets the variable back to a falsy value.

### TokenManager (the HUD, portraits, quickbar, conversation, journal ... all of it)

Movie to engine, polled every frame (not an fscommand): `TokenManager.AddTokenObserverBatchData` is an array of triples `[observerId:number, tokenPath:string, updateDelay:number]` pushed by `TokenManager.AddTokenObserverWithID(id, path, callback, delay)` (checked in `TokenManager.as` and in `GuiMovie::ProcessAddTokenObserverBatch` 0x0051A8B0: size/3, element 1 of each triple is a string, element 2 a float). `RemoveTokenObserverBatchData` is an array of ids. The engine reads them with GetArray (type 5), then empties the array with SetArraySize(path, 0). For each triple it creates a `GUI::ScaleformTokenObserver` (vtable 0x00AF689C, ctor 0x00524A80, 0xA0 bytes, keyed in the record by id; the `_cb` string) on the data-tree attribute named by the path. The older per-call route `ExternalInterface.call("AddTokenObserver", label, "", path)` returns an id (engine counts down from 0xFFFFFFFF) and is used by `AddTokenObserverFunc`.

Engine to movie, once per frame per movie when something changed (checked in 0x00519B00): the observer's notify slot (vtable slot 4, 0x00524BA0 then 0x00519AA0) adds the observer id to a pending id list if it is not already there and the attribute's current value as a **wide string** to a parallel list (so a value changed twice in a frame is delivered once). `DeliverTokens` then loops (at most about 11 rounds per frame, `cmp [esp+0x20], 0xA`): SetArray(type 0 int, `TokenManager.BatchTokenReplacementCallbackIDs`), SetArray(type 4 wstring, `TokenManager.BatchTokenReplacementValues`), then `Invoke("TokenManager.PerformBatchTokenReplacements")` with no arguments. The ActionScript calls `cm_mapCallbacks[id](value)` for each and clears both arrays (checked in `TokenManager.as`). A value is the attribute rendered as text: numbers, bools ("0"/"1"), strings, so movies do `Number(v)`. Containers (arrays/lists such as `Conversation.Responses`, `Party.Members.0`, `EffectList`) notify with their string form and the movie then pulls members itself with `GetNumMembers` / `GetValue` / `ListMemberNames` (fscommands, part a).

Tokens the HUD movies observe (read from the movies' ActionScript, FFDec of the patch003 copies; checked):

- **portraits.gfx** (party portraits, layer 1): per portrait `<creature token>` = `Party.Members.N` (with `.Portrait` for the `img://portrait_...` url, `.Attributes.7.Current` / `.Attributes.7.Total` health, `.Attributes.8.Current` / `.Attributes.8.Total` mana or stamina, `.Attributes.19.Total` experience, `.UserSelected`, `.PendingAction`, `.CurrentAction`, `.PortraitProperties.PortraitTint`, `.PortraitLevelUp`, `.Stats.ClassHasMana`, `.AbilityUpkeepCost`), plus `Party.SelectedCount`, `Party.SelectableMemberCount`, `Party.HoldPositions`, `Party.PartyPickerGUIStatus`, `GUI.GuiHighlights.<n>.Highlight`, `InputMappings.Mappings.ToggleHoldParty.PrimaryMappedKey`, `GameModeController.CurrentGameMode`. Health and mana bars are therefore updated on change, not per frame.
- **quickbar.gfx**: `GUI.SelectedChar.Name`, `GUI.SelectedChar.ActiveWeaponSet`, `GUI.SelectedChar.EffectList`, `GUI.SelectedChar.CanSwitchWeaponSets`, `GUI.SelectedChar.CanChangeEquipment`, `<player>.Stats.QuickbarLocked`, `Party.QuickbarLocked`, `Party.QuickbarExpandValue`, experience and level tokens, per slot `<slot token>` plus `.Status`, `.PendingActivation`, `.IsWarning`, `.IsError`, `.ItemIcon`, an item-count token, and a **cooldown percent token** (`m_sCooldownPercentToken`, per ability) and `RemainingTimeToken` per effect icon, `ClientOptions.VideoOptions.ResolutionWidth`.
- **minimap.gfx**: `Area.Minimap`, `Area.Name`, `GUIMain.MinimapDistance`, `GameModeController.CurrentGameMode`, `Journal.PlotGivers[.N.WorldPositionX/Y]`, `Journal.PlotDestinations[.N.Object.WorldPositionX/Y]`, `MapNoteList.MapNotes`.
- **conversation.gfx**: `Conversation.NPCLine` (the NPC's line text), `Conversation.Responses` (change notice only), `Conversation.ResponsesReady` ("0"/"1"), `Cutscene.SubtitleText`, `GameModeController.CurrentGameMode`. After a `Conversation.Responses` notice the movie pulls `GetNumMembers("Conversation.Responses")` (capped at 6 shown) and `GetValue("Conversation.Responses.<i>.Text")` and `.Icon`; it reads `Conversation.HasVO` and `ClientOptions.GameOptions.SubtitlesLevel` with `GetValue`. The player's pick goes out through `SendMessage`.
- **floatylayer.gfx**: only `GameModeController.CurrentGameMode`, `Client.Paused` (pause text and border).

### FloatyLayer (floaty text, health-over-head, icons), engine function `0x0063E5B0` (checked)

Runs once per frame from the app update (above), only while the layer is on. It walks its map of floaty objects (each an object with a data-tree attribute holder; the floaty ids are what the movie sees), compares with the previous frame, and fills **parallel arrays** that it sets only if a given list is non-empty (a flag is kept, and `ProcessBatchUpdates` is invoked once at the end, `0x0063F978`, when anything was set). Sets are SetArray with the type in parentheses; the ActionScript (`FloatyLayer.as` `ProcessBatchUpdates`) consumes them in this order and clears each:

1. `BatchCreateIDs`(int) + `BatchCreateSymbolIDs`(int): create floaty id with symbol 1 stats, 2... (symbols: STATS, MESSAGE, ICON 2, ACTION_BAR 3, OFFSCREEN 4, DEBUG 5, from the movie's `FloatySymbolNames`: FloatyStats, FloatyMessage, FloatyIcon, FloatyActionBar, FloatyOffscreenIndicator, DebugWorldSpaceTextFloaty).
2. `BatchShowIDs`(int).
3. `BatchUpdateIDs`(int) + `BatchUpdatePositionsScalesAlphas`(float, 5 floats per id: x, y, z, scale, alpha) + `BatchUpdateOffsets`(int, 1 per id): applied through `UpdateAllFloatyPositions` -> `SetFloatyPosition(id, x, y, z, scale, alpha, offset)`. The x, y are screen positions the engine computed (world to screen with the camera); the movie only places clips.
4. Messages (floaty text): `BatchUpdateMessageIDs`(int), `BatchUpdateMessageStrings`(wstring), `BatchUpdateMessageStyles`(int: default, description, hit, critical hit, ambient dialog), `BatchUpdateMessageColours`(int RGB), `BatchUpdateMessageDurations`(float seconds) -> `DisplayFloatyMessage(id, text, style, colour, duration)`.
5. Stats: `BatchUpdateStatsIDs`, `BatchUpdateStatsFlags`(int: show name, show health bar, is hostile ...), `BatchUpdateStatsNames`(wstring), `BatchUpdateStatsCurrentHealths`, `BatchUpdateStatsTotalHealths`, `BatchUpdateStatsRanks`(int creature rank) -> `UpdateFloatyStats`.
6. Icons: `BatchUpdateIconIDs`, `BatchUpdateIconNames`(wstring) -> `SetFloatyIcon`.
7. Offscreen arrows: `BatchUpdateOffscreenIDs`, `...Types`(int, hostile or not), `...Xs`, `...Ys`(int) -> `UpdateOffscreenIndicatorFloatyData`.
8. `BatchHideIDs`, then `BatchDestroyIDs` (int).

Also direct Invokes (checked): `FloatyLayer.DisplayStatusMessage(text:wstring, colour:number)` from `0x0063C260` (message handler in the GUI system, event), `FloatyLayer.UpdateActionBarFloatyData(id, progress, interrupt)`, `FloatyLayer.SetFloatyPosition(...)` and `FloatyLayer.UpdateDebugWorldSpaceTextFloatyData(id, text)` from the `ActionBarFloaty` class (vtable 0x00B1CFEC, per frame while the bar is up; args inferred from the ActionScript signatures).

### Minimap and world map (checked)

Function `0x00610D50` (called from `GUIMap` vtable 0x00B19FB0 slot 5 = 0x0072F410, and one more call site; scene id 0x2A = Minimap is tested first): SetArraySize then SetArray for `SharedLibrary.SharedMap.BatchCreatureIDs`(int, one per creature), `SharedLibrary.SharedMap.BatchCreaturePositionsOrientations`(float, 3 per creature: world x, y, orientation; element 0..2 is also the camera icon), `SharedLibrary.SharedMap.BatchCreatureFlags`(int bit flags: hostile, in party, show-as-ally, player, within map radius, from `CreatureMapIcon.as`), then `Invoke MiniMap.Instance.UpdateMinimap()`. Frequency: on the map object's update; I did not trace its caller to a per-frame loop (inferred per frame while the minimap is shown). The world map: `WorldMap.GetTravelling`, `WorldMap.GetAnimating`, `WorldMap.AreaLoadedNotify` Invokes (event; `0x0072EDA0`, `ForScript_AddGeneralScoreboardEntry`'s neighbour).

#### Where `Area.MinimapLeft`, `Right`, `Top`, `Bottom`, `Width` and `Height` come from (read 2026-10-19 by lib-agent-2)

- **Copied, not computed,** by `CCArea`'s load (0x0068B3C0–0x0068BB48) from a small file of the level's, `<layout>_mmi.gff` (GFF4 `mmi`, 128 bytes, in the level's `.rim`) [checked]:
  - Left and Bottom are `EnvMinimapLowerLeftPoint` (3138) x and y; Right and Top are `EnvMinimapUpperRightPoint` (3139) x and y: the world's x and y at the picture's corners.
  - Width and Height are `EnvMinimapTextureMapCoords` (3137) x and y: the fraction, 0 to 1, of the power-of-two texture the picture covers. `minimap.gfx` multiplies its image's size by them (`SharedMap.as:65-66`).
  - Each point's z is not used. The layout itself holds none of these fields.
- **With no `_mmi`,** they are the layout's bounds, and Width and Height are 1.
- **An example:** `brc100d_mmi.gff` holds Width and Height (0.77362, 0.63579), the lower left (−8.2475, 32.0926) and the upper right (255.8165, 249.1079), as the live game served them [checked].
- Not read: the map's North, and `LvlMinimapPositionX`/`Y` and the other fields 3740 to 3743, which this load does not read. Core's level reader does not read `_mmi` yet.

#### What the engine serves as the data tree's `Area.Name` (areaname) (lib-agent-2, read 2026-10-19)

##### Short answers

1. `Area.Name` is the attribute `Name` of the **area object the player's creature is in** (a `CCArea`). It is filled from the area's own `.are` GFF3 field **`Name`**, a LocString (a strref into the talk table), resolved to a wide string. It is not `EnvAreaName` (3012), not the area list's entry, not the module's and not a data table. (checked: exe + files)
2. It is set once, when the area is loaded from its `.are` (or from its save), and the `Area` node is (re)registered under the data-tree root when the area-load machine runs its first step. It is not updated per frame. If the strref is -1/0 or the lookup fails it is the empty string `""`; there is no fallback to the tag or the resref. (checked for the code; the player-creature part of "which area" is inferred, see below)
3. Yes, a wide string from the talk table, resolved at area load in the language of the loaded talk table. `minimap.gfx` does **not** uppercase it (no `toUpperCase` anywhere in the movie's AS). It does call `ExternalCommands.ReplaceDesignerTags` on it, but only if the text contains `<` (and the text field is html). (checked in AS)

Examples (checked with ghilana): `bhm600ar_fade_harrowing.are` Name strref 21857 = "The Fade" (the mage origin's Harrowing Fade); `cir300ar_fade.are` strref 21913 = "The Raw Fade"; `arl100ar_redcliffe_village.are` strref 16042 = "Redcliffe". All in `singleplayer_en-us.tlk` (the campaign's table, not `core_en-us.tlk`).

##### Prior work consulted

- `dev/GUI.md:407` lists `Area.Name` for minimap.gfx (already known to the peer); `dev/gui_data_paths.csv:1880,1961,2814` list the users (map.gfx GetValue, minimap.gfx token observer, saveload.gfx GetValue). Nothing on the source of the value, so the rest is new.
- `dev/gff4_fields.csv:86` `3012,EnvAreaName` and `src/dao/core/Level.cpp:398,619` (`kAreaName = 3012`, `LevelLayout.name`) are the **level layout** (.lvl, GFF4) field, a different thing: the engine's `Area.Name` does not read it (this was the peer's lead, and it is not the source).
- `dev/LOAD_EVENTS.md:14-30` and `src/dao/Addresses.h:1196-1210` for the area-load machine `0x006C8EF0` (state 6).
- `dev/GUI.md:281-284,401,718` for the token observer (the movie watches a path; the value arrives as a wide string).
- DAOEluvianMCP / DAOTurnBased / the address library: grep for `Area.Name`/`AreaName` found nothing relevant (only `SETSHAREDCHARAREANAME`, an unrelated script command).

##### Evidence

###### The `Area` node and its `Name` (checked, exe GOG 1.05, `tools\daoexe.py --disasm`, no Ghidra)

- `CCArea`'s constructor (vftable `0x00B0DE04`, the function around `0x00688CC1`-`0x00689D2B`, `ret 0x4c`) builds the area's attributes by name into the container at `this+0xD4` (the vtable `0x00B0DDE0` stored at `[esi+0xD4]`). The wide-string pushes there: `ResRef` (0x00B0DD4C) -> `this+0x480`; **`Name` (0x00AEDA08, push at 0x006893FA) -> `this+0x4A8`**; `Minimap` (0x00B03828) -> `this+0x4D0`; `North` (0x00B0DD40); `MinimapBottom/Left/Top/Right/Width/Height` (0x00B0DD24 .. 0x00B0DCA0), which are the `Area.Minimap*` paths of `dev/gui_data_paths.csv:1873-1879`. So `Area.Name` = `CCArea+0x4A8`, `Area.Minimap` = `+0x4D0`.
- The attribute at `+0x4A8` is a string attribute; its destructor and copy are at `0x0068AD2F`, `0x0068C6D9`.

###### What it is filled from (checked)

- The `.are` loader (a `CCArea` method around `0x00682B00`-`0x00683373`; it reads the GFF3 fields by narrow label: `Comments` 0x00B0DC34, `Expansion_List`, `Expansion_ID`, `Tag` 0x00B0DBD4, `ScriptResRefID`, `OnExit`, ...) reads the field **`Name`** (narrow string at 0x00AE5954, `push 0xae5954` at **0x00682DD6**) with the LocString reader `0x00A37300` (call at 0x00682DEB). The result is copied into the area's LocString member at **`this+0x158`**: flag byte `[+0x158]`, strref `[+0x15C]`, text `[+0x160]`, gender flags `+0x17C..+0x17E` (0x00682DF7-0x00682E3B).
- Right after (0x00682E5F-0x00682E84) it calls the attribute's `Set` (vtable slot at `+0x54` of the attribute's vtable) on `this+0x4A8` with the wstring returned by `0x004D1EB0(this+0x158)`.
- `0x004D1EB0` is the LocString's "get text": if the resolved flag `[+0]` is 0 it calls `0x004D1D50` (resolve), then returns `this+8`, the wide string. `0x004D1D50` resolves through the talk table manager (global `0x00C1D08C`, lookup `0x004E4990(strref, ..)`; strref `[+4]`), and the result is assigned to `+8` (`0x004D1DC4-0x004D1DCA`). If the global is null or the lookup fails (`0x004D1E41`): the text is set to the empty wide string (`0x00ADEF14`, L""). The default LocString (`0x004D1B80`) is strref `-1`, text `""`. If the strref is -1 or 0 the text stays `""`.
- A debug path in `0x004D1D50` (`0x004D1DD8`: the call `0x00838210` on `global 0x00C32938 + 0x674`) rewrites the text as `{%u - %ls}` (format at 0x00AF0DB0: strref and text), apparently a "show string ids" debug mode. Not read further; it is off in play (inferred, since the minimap shows plain "The Fade" in play-sessions described elsewhere is not verified here).
- Data: the `.are` has `Name: {strref: N}` (read with ghilana `resource`): `arl100ar_redcliffe_village.are` 16042 "Redcliffe"; `cir300ar_fade.are` 21913 "The Raw Fade"; `bhm600ar_fade_harrowing.are` 21857 "The Fade"; `char_stage.are` (chargen stage) 311656. (Strings read with `mcp__ghilana__strings`, `id: [16042, 21913, 21857]`; `singleplayer_en-us.tlk`.) Other `.are` fields next to it: `Tag`, `TemplateResRef`, `ScriptResRefID`, `Comments`, `Map`, `North`, `Layout`, `ReverbPreset`, `MusicPreset`. There is no `LocName`/`DisplayName`/`EnvAreaName` in the `.are` and the name is not taken from a 2DA/GDA.
- The other readers of the same LocString: `0x006675B6`-`0x006675F5` builds `<path>.Area` (wide `Area` at 0x00B08240 appended by `0x004C3030`) and fills it with `0x004D1EB0(areaObject+0x158)` of the area found by object id (`0x005D5960`), or the text `OBJECT_INVALID` (0x00B082C0) when none: this is what `<save or party member>.Area` returns (the saveload movie reads `ExternalCommands.GetValue(_loc3_ + ".Area")`, `as_saveload/.../SaveFile.as:43`). Same source, so the name in the save list and `Area.Name` agree. (checked for the code, inferred for the exact path owner)

###### When the node `Area` points at an area (checked for the code, inferred for the detail)

- `0x006C6FD0` (ID 15085 + ..., unnamed in the address library; GOG `0x006c6fd0`, EA `0x005b8a90` for ID 15093's neighbour; I checked only the GOG address) takes the controlled creature via `0x00694990` (`mov ecx,[0xC327EC]; jmp 0x5D5CC0`, the ClientExoApp's player creature), calls its vtable `+0xA0` (its area), and registers `area + 0xD4` under the name `Area` (0x00B08240) in `SmartData::TokenManager` (`0x00455BF0` + `0x00456540`, the register call at 0x006C7036).
- Its only caller is `0x006C8F7B`, in the area-load machine **`0x006C8EF0`**, the first case of its jump table (`jmp [eax*4+0x6C9654]` at 0x006C8F55, `eax` = the step at `[ebp+0x74]`; the case at 0x006C8F5C). `dev/LOAD_EVENTS.md:17` numbers the steps 1 to 8; this is step 1. So on every area load/transition, `Area` is re-pointed at the new area's `+0xD4` container before the rest of the load runs.
- `CCArea`'s destructor (the code at 0x0068A93C-0x0068A9D2) looks up `Area` in the TokenManager (`0x005C6230`), and if it still points at this area's `+0xD4` it removes it (`0x005C6710`): so `Area.Name` is not found (the shared missing attribute: "" / 0, `dev/GUI.md:852`) between an area's destruction and the next area-load step 1.
- The Name itself is set earlier, when the area is built from its template/save (the `.are` loader above), not at step 1.
- Consequence for the port: serve `Area.Name` as the LocString text of the `.are` `Name` of the player's current area; publish it at area load (and a movie that observes it gets the new value once, with the coalesced per-frame delivery of `dev/GUI.md:401`). Whether a movie's observer is notified when the `Area` node is swapped for another area's (rather than only when the same `Name` attribute is Set) I did not read: see Not read. In play the minimap re-creates its field per area load (it is part of the scene that is reloaded), so a fresh `GetValue`-style refresh happens on the observer's registration (`0x0051A4A0` refreshes at once, `dev/GUI.md:718`). (inferred)

###### The movie side (checked, `%TEMP%\lib-agent-2-guicmd-x\allgfx\as_minimap`)

- `MiniMap.as:46-56`: `AreaName_mc_init` sets `TextField.autoSize = "center"`, `wordWrap = true`, and registers `TokenManager.AddTokenObserverWithID(_OBSERVER_ID.MINIMAP_AREANAME, "Area.Name", function(a_sName){ areaNameField.SetText(a_sName); areaNameField.SetOutline(true, 3745298); })`. So it only shows the string: no case change, no localization lookup.
- `SharedLibrary/TextField.as:22-53` `SetText`: null -> ""; `removeString("\r\n","\r")`; if the field is not html (or `GUISystem.ShowRawFormattingTags`) the text is assigned raw; else `&` -> `&amp;` and `htmlText = ProcessText(text)`. `ProcessText` (`TextField.as:127-135`) calls `ExternalCommands.ReplaceDesignerTags(text)` (`ExternalCommands.as:125-130`, `fscommand("ReplaceDesignerTags", ...)`) only if the text contains `<`. Area names such as "The Fade" have none, so the text is displayed as is.
- No `toUpperCase` in `as_minimap` (grep). Any capitals come from the text field's font/format in the SWF, not from code (not read).
- map.gfx (`Map.as:172-174`) uses `SetText(ExternalCommands.GetValue("Area.Name"))` and saveload.gfx (`SaveLoad.as:368`, `SaveFile.as:36-43,96,100`) `GetValue("Area.Name")` for the current area; the saveload list uses `<slot>.Area` for other saves. These want the same wide-string text.

##### Not read

- The code path that makes observers on `Area.Name` fire when the `Area` node is replaced by the next area's container (the TokenManager's register call `0x00456540` and `0x005C7CE0`'s re-resolve), so whether the minimap's text changes on a transition by notification or only by the movie being rebuilt.
- What `0x006C6FD0`'s `0x00694990` returns exactly when there is no player creature (loading screens, chargen before the hero exists), and the case where the node `Area` is registered earlier than step 1 elsewhere (the other refs to the `Area` string at `0x006C7010` (this function), `0x006CCD1D` (save/loading state) and `0x007ABD34/0x007ABE22/0x007B9585` were not opened).
- The debug `{strref - text}` branch (`0x004D1DD8`) and which global/flag turns it on.
- Area names for areas of the 15 enciphered add-in archives (ghilana cannot read them); the Fade areas read here are the campaign's.
- The `Area_List`/`Area_ID`/`Area_Name` labels at `0x006FC0xx` (`CSAreaList` reading a list from a save/module file; `Area_Name` there is a plain string, apparently the area's tag, inferred) are not what `Area.Name` shows.
- The text field's font and any fixed-case formatting in `minimap.gfx`'s timeline/XML.
- Other languages: only `en-us` strings were read.

#### The area map's explored cells and its notes (read 2026-10-20 by lib-agent-2, DAOParagonEngine #19)

Checked in the disassembly, the movies' ActionScript (map.gfx's `Map.as`, `SharedMap.as`, `MapNoteIcon.as`, `PlotMapIcon.as`) and the data; [I] inferred. Nothing was run.

##### The explored cells

- **A bit a 6 m cell, on the area** (`CCArea+0x4F8`, built at the area's load by 0x007C9890): its rectangle is the `_mmi` one above (Left, Bottom, Right, Top). `0x007C9580` sizes it: W = trunc((Right − Left) / 6), H = trunc((Top − Bottom) / 6), the cell the double 6.0 at 0xB1B7C0; (W·H)/8 + 1 bytes, zeroed.
- **Only the main controlled creature marks it** (`MarkPosition` 0x007C9D20): from its position sync (0x006B5EF0) and when an area becomes the current one (0x0068819A). A cell is marked only when the creature's cell changes: cell = round to nearest of ((x − Left)/(Right − Left)·W, (y − Bottom)/(Top − Bottom)·H) (0x007C9740), clamped to 0..W and 0..H, and its bit set if inside the grid, idx = y·W + x, `bits[idx >> 3] |= 1 << (idx & 7)` (0x007C99C0). Followers mark nothing; nothing depends on what is seen.
- **Saved raw** as `SavegameAreaMap` (16016), a list of the bytes in the area's save record (0x6845C5). At the load (0x68BB75) it is taken only if its size is the grid's, then **OR-merged with every area `LinkAreaMiniMaps` links it to** (`CCModule+0x340`, 0x00602AF0; 0x68BBC4–0x68BDC0), so explored ground is shared between linked areas (Kings Camp and its night variant, for one). Nothing clears it; `DisableMiniMap` (519) has no backend.

##### The `mapmask` texture

- **`GUI::GUIMap`'s builder** (0x0072F5D0) makes a 256 × 256 render texture named **"mapmask"** (the string's only use, 0xB19F1C), and the brushes `mapvisbrush` (`GUIMaterial_MaxAlpha.mat`: BLENDOP_MAX, src and dest ONE; `mapvisbrush.dds` 64 × 64, black, alpha a soft irregular blob) and `mapfogbrush` (MIN; never drawn).
- **Redrawn from nothing on every update while the full map screen (scene 0xF) is open** (`GUIMap` slot 5 0x0072F410 → 0x007C9D70, run on the render thread by 0x007C9C60): cleared to (0, 0, 0, 0), then for every set cell the brush quad is stamped (0x0072F040) at clip (2·ix/W − 1, 2·iy/H − 1), the cell's lower-left corner, y up; its edge 180 / (Right − Left) clip units, about 90 m whatever the height; turned `rand() % 360` degrees with `srand` seeded by the bits of (x·10 + y), so a repaint is the same picture. The texture covers the whole `_mmi` rectangle, not the Width/Height fraction. It is not saved; it is redrawn from the bits.
- **In the movie:** `Map.InitializeAreaMap` loads `img://mapmask.dds?Width=<OriginalWidth>&Height=<OriginalHeight>` into `MapMask_mc` (OriginalWidth = the map picture's width × `Area.MinimapWidth`, and the same for the height) and sets its blend mode to **"erase"**. The picture is loaded twice beneath and above it: `MapImageAlpha_mc` with "erase", `MapImage_mc` with "alpha" (depths 1 the mask, 3, 5, then the icons at 7). `ScaleformManager::LoadImage` (0x00516A20) finds the name as a texture [I for the lookup finding this live render texture; no "mapmask" resource exists].
- **Polarity is not settled:** the engine leaves alpha 0 where the party has not been and up to 255 inside the stamps. What GFx makes of the three layers' "alpha" and "erase" modes was not run. A screenshot of a fresh area against one walked, through Eluvian, settles it.

##### The notes on the map

- **MapNotes** are waypoints alone, held in the singleton `MapNoteList` (0x00C38D30) at +0x58: those with `HasMapNote` (+0x440) and `MapNoteEnabled` (+0x444) in the player's area, added or removed by `CCWayPoint::UpdateMapNoteList` (0x007C4370) at the waypoint's load (0x007C4860), its area set (0x007C491F), `SetMapPinState` (0x007C493A) and its destructor. Nothing sweeps the list on an area change [I: entries of an area left loaded stay]. `.MapNote` is the waypoint's LocString (+0x44C); `.Icon` (+0x480) is `waypointtypes`' (table 106) `Icon` column for its MapNoteType (`ico_mm_mapPin`, `ico_mm_exit`, `ico_mm_store`), a library symbol the movie adds as a child.
- **PlotGivers** (`Journal+0x150`, rebuilt by 0x006F1FE0): the objects whose plot-giver flag (+0x25A) is set, in the player's area and in the world, only while the PlotAssist option (`[0x00C32938]+0xA8`) is on.
- **PlotDestinations** (`Journal+0x16C`, rebuilt by 0x006F20E0): for each tag in the journal's destination map, each object of that tag in the player's area and in the world, as an entry with `.Object`, `.Tooltip` (the plot's name: the active plot's if one of the tag's plots is active, else the first's) and `.ActiveQuest`. The map is filled when a plot flag is set true from its `PlotAssistList` (13019; `PlotAssistTag` 13020, `PlotAssistAdvancesPlot` 13021: false adds the tag, true removes it), and saved (16513–16515). The movie reads `.Object.Name`, `.Object.WorldPositionX/Y` and `.Object.MapNoteVisible`.
- **When they are rebuilt** (no list per frame): the area load's step 1 (0x006C7061, 0x006C7068), an object made active (0x00698D30, 0x00698D4B), `SetPlotGiver`, a plot flag with a `PlotAssistList`, the PlotAssist option (0x005E173D, 0x005E174F), and 0x006F4135 [I: the active plot changing]. Positions follow through per-entry observers on WorldPositionX and Y, not a rebuild.
- **`MapNoteVisible`** (an object's +0x25C, false at construction) is written by 0x007C9BF0 → 0x00697970 from 0x007C9A50, inside 0x007C9E30's pass over the area's objects and the party on each map update: **true when an explored cell lies within 34.2 m** (0.95 × 36, the global 0xC38DF4) of the object [C for the write]. A party member's is set straight to 1 when `[0xC32938]+0xA8` > 1 (that setting not identified). The movie hides a note, giver or destination whose MapNoteVisible is false only on the full area map (`ApplyMask`, `Map.as:201`); the minimap ignores it.

##### Not read

- The mask's polarity on screen; the render texture's creation flags (1, 3, 1, 0, 0) and pixel format; the bodies of 0x4EE200 and 0x4D3E30 (the stamp's rotation and transform).
- Whether the HUD minimap's own `GUIMap` (scene 0x2A) makes a "mapmask" too (only scene 0xF's is repainted); `MinimapRenderOperation` (+0x28), registered but written by nothing found [I: dead].
- The linked-areas merge's exact name pairing; how the bit list is laid into the save record.
- What 0x0075F160 and 0x0075EE60 return exactly (taken as the plot's key and its name); `CCObject +0x1AC` and +0x421; whether `Name` resolves for creatures with an empty name; the PlotAssist option's place on the options screen; whether the `ico_mm_*` symbols exist in map.gfx and minimap.gfx.

### Event Invokes (checked paths; argument shapes read from the call sites)

- Scene lifecycle (`GuiMovie`): `_root.StartSceneFromGame()` (0x00518D20, when OpenMovie shows a scene), `_root.FinishSceneFromGame()` (0x00518CD0, on hide), `_root.Rewind()` (0x00518E00), `_root.PlayScene("Outro")` (`PlayOutro` 0x005119F0, one wide-string argument, then record byte +0x101 set), `MovieUtils.UpdateAllLayouts()` (stage size/layout change), `MovieUtils.IsMovieOnStage`, `MovieUtils.CallFunctionOnMovie` (the GUI system's slots 80 and 81: a generic path and argument forwarded), `MovieUtils.PrintHierarchy` (debug, `0x00606C70`).
- Popups: `PopupLayer.ShowDialogBoxFromGame()` (no arguments: the box's contents are in the data tree), `PopupLayer.ShowItemSplitDialogFromGame()` (no arguments).
- Conversation: `Conversation.OnNPCLineFinished()` (no arguments) from `0x0068CC60` when the NPC's line (voice) ends, called from the dialogue game mode's skip handling and one other place.
- Drag and drop: `Tooltips.StartDragging(...)` (on `StartDraggingMessage`, one argument), `Behaviour.DragAndDropable.StopExternalDrag(...)` (on `StopDraggingMessage`).
- Generic: `InvokeCallbackMessage` (GUI system 0x0060ED40) calls the function named in the message on the message's movie with one wide-string argument; the GUI highlight script command (`0x0060EBF0`, `ScriptBackend::ForScript_HighlightGuiElement`'s helper) calls a movie function with one wide-string argument.
- Screens: `CharGen.GoBack()` (chargen go-back message), `DebugOptions.AddSlider`, `DebugOptions.AddToggle` (debug options screen builds).
- The names exist in the exe with no argument read at the call site for: `GUI.HideFullScreenGUI` (a message, not an Invoke), `ForceInventoryRefresh` (0x00606C70 path).

### Not pushed by the engine into movies (so the replacement fills them from the game state itself)

Inventory, journal, character sheet, store, level-up, tactics and the options screen: they run on tokens plus the fscommand/ExternalInterface queries (part a), which return data on demand (`ListTactic*` answer in `_root.fsc_retval` / `fsc_retval2`; `GetItemAttributes`/`GetAbilityAttributes` answer in `_root.ReturnedItemAttributes` / `_root.ReturnedAbilityAttributes`, set by SetArray at 0x00714452C and 0x00714732).

### Evidence and method

- Method: all indirect calls through movie vtable offsets 0x2C..0x4C near a pushed string, over the whole .text with capstone (script `scan.py` in `%TEMP%\lib-agent-2-guicmd-x\`), plus a call-graph index of rel32 calls (`callers.pkl`). Names of containing functions in the address library are the nearest named address before the site and are often wrong (e.g. the floaty update sits under `ScriptBackend::SetFollowerState`'s name); the real starts are given above as addresses.
- Exe read: 0x00518830 (SetFSCommandReturnValue: SetVariable `_root.fsc_retval`), 0x00518850 (Invoke), 0x00519B00 (DeliverTokens), 0x0051A8B0 / 0x0051AC10 (observer mailbox), 0x0051ADA0 (Step), 0x00511CA0, 0x0051A4A0, 0x00519AA0, 0x00524BA0, 0x0063E5B0, 0x0063C260, 0x00610D50, 0x0068CC60, 0x0060ED40, 0x0060EBF0, 0x0060BB60, 0x005D5460, 0x00523B80.
- Movies read (FFDec, patch003.erf copies, extracted under `%TEMP%\lib-agent-2-guicmd-x\gfx\as_<name>\`): floatylayer, conversation, quickbar, minimap, portraits; `TokenManager.as`, `FloatyLayer.as`, `Conversation.as`, `SharedMap.as`, `CreatureMapIcon.as`.

### Not read

- `updateDelay` (the third triple element) throttles re-reading a changed value; it is not a resend interval (see the calls section). Its unit is not read.
- The caller chain that makes the minimap update (`GUIMap` slot 5 and the second call site 0x00634187), so its rate is inferred.
- Arguments of `Tooltips.StartDragging`, `Behaviour.DragAndDropable.StopExternalDrag`, `DebugOptions.*`, and the identity of the message `MovieRecord::Step` posts for `WaitingToBeStopped`.
- The `ActionBarFloaty` per-frame argument construction (names from the strings, shapes from the ActionScript).
- Whether any per-frame engine Invoke exists beyond those listed (the scan finds literal-string call sites only; a path built at run time, e.g. `InvokeCallbackMessage`'s, is not enumerable statically).

## Fonts, images, localisation and input

Read 2026-10-18 by lib-agent-2 (for DAOParagonEngine #19).

Scratch: %TEMP%\lib-agent-2-guiext-* (x = extracted movies, all = all 68 .gfx, sw = FWS copies, as = FFDec ActionScript of tooltips, quickbar, portraits, conversation, inventory, saveload, notifications, popuplayer, optionsmenu). Marks: [C] checked in the exe or the files, [I] inferred.

### Short answers

(a) Fonts. The vanilla game has two fonts, both vector (DefineFont3), "Nogard" (titles) and "Dragon Text" (body). They live in fonts_<lang>.gfx (10 files: cs-cz de-de en-us es-es fr-fr hu-hu it-it pl-pl pseudo ru-ru; each is only two DefineFont3 tags, 740 glyphs; ru-ru 947) and in gfxfontlib.gfx (243 glyphs, exports $TitleFont and $BodyFont). Movies do not embed them: an edit text names a font id that is an ImportAssets2 from "gfxfontlib.swf" ($TitleFont etc.). `ScaleformManager::Initialize` (0x00515FE0) builds a GFxFontLib (filled from fonts_<lang>.gfx, falling back to the literal "fonts_en-us.gfx") and a GFxFontMap with two entries, "$BodyFont" -> "Dragon Text" and "$TitleFont" -> "Nogard" (scale 1.0). The game never sets any GFxFontCacheManager parameter [I: the only code referencing that vtable is inside the GFx library], so Scaleform's defaults apply. For a replacement UI: bundle Nogard and Dragon Text (extract from fonts_en-us.gfx) and map $TitleFont/$BodyFont to them.

(b) Images. Every dynamic image is a movie `loadMovie("img://<name>.dds[?Width=W&Height=H][?Async=1]")` or `MovieClipLoader.loadClip` of the same. The AS builder is `MovieUtils.GetImageUrl` (same file in every movie). Names are texture/resource names: a name without "." gets ".dds" appended; a name containing "?" is replaced by the missing texture. "FILE:<path>" as the name is a file on disk (save-game screenshots in saveload.gfx). The exe never builds an "img://" string itself; it only parses them (EclipseGFxImageLoader::LoadImage 0x00517390 -> ScaleformManager::LoadImage 0x00516A20). "imgps://" is accepted identically to "img://" by the engine and does not appear in any vanilla movie's script (byte-grepped all 68 movies). Portraits are rendered live into a texture by the engine; the token `<creature>.Portrait` carries the name and the movie calls MovieUtils.LoadImageFixedSize with it.

(c) Localisation. There is no GFxTranslator (no such class in the RTTI). Movies call the engine: `Localization.Strings.LOC_X` (an AS `__resolve` on a static object) looks the constant's number up in the movie's own `Localization` class (about 614 constants, e.g. LOC_OK = 325483) and calls `ExternalCommands.GetTlkString(id, "#id")` = `fscommand("GetTlkString", "<id> <fallback>")`, answered through `_root.fsc_retval`; handler `GuiFSCommand::GetTlkString` 0x0051EAE0. `GuiExternal::GetString` (0x0051EE00) is the ExternalInterface string getter for data-tree values. Data-driven strings come through TokenManager observers (the engine pushes values). Markup like `<...>` in text is expanded by `ReplaceDesignerTags` (fscommand, handler named GuiFSCommand::ReplaceDesignerTags; also GuiExternal::ReplaceDesignerTags). The language switch is a file name: fonts_<lang>.gfx; the talk table language is the engine's.

(d) Input. Only mouse and text/keyboard events go into GFx, all by `ScaleformManager`, to `GFxMovieView::HandleEvent` (the view's vtable slot at +0x98). Mouse: eleven GUICursor* messages (see list) each forward to the movies; a per-frame text/key forwarder (0x00514C90) feeds the front movie. Gamepad: nothing found. The game's own keybindings are the engine's input system (Eclipse GroupedInputMappings), not movies; movies only list them.

### (a) Fonts, evidence

- Strings [C]: 0x00AF5588 `fonts_en-us.gfx` (literal), 0x00AF5598 L"fonts_" (prefix; the language string is appended, then ".gfx" at 0x00AF5540), 0x00AF55A8 `$BodyFont`, 0x00AF55B4 `Dragon Text`, 0x00AF55C0 `$TitleFont`, 0x00AF55CC `Nogard`, 0x00B490A8 `gfxfontlib.swf` (the import name the loader treats as "delegate to the font library"; used at 0x0091C4B7 and 0x0091CA3C inside GFx).
- Initialize's flow [C by disassembly, 0x00516400..0x0051655E]: new GFxFontMap (0x18 bytes, ctor 0x0090E440) -> loader SetState(0x14); `MapFont` (0x0090E4E0) called twice (name, mapped name, flags 0, scale 1.0f: `$TitleFont` -> `Nogard`, `$BodyFont` -> `Dragon Text`). Then a GFxFontLib (ctor 0x0090D970) -> SetState(0x12). The font file is loaded with `GFxLoader::CreateMovie` (0x0090C470) as "fonts_<lang>.gfx"; if it returns null, CreateMovie("fonts_en-us.gfx"); then `AddFontsFrom(movieDef, pin=1)` (0x0090D910) and a call 0x00515A70. The language wstring's source I did not trace [I: the engine's language setting; the same "en-us" text sits at 0x00ADE200 and is read by 0x005F676A].
- Other loader states Initialize sets (vtables [C], state ids from the push before each SetState call): 4 `EclipseGFxLog` (0xAF53EC); 0xA `EclipseGFxFileOpener` (0xAF54F4); 8 `EclipseGFxFSCommandHandler` (0xAF53BC); 9 `EclipseGFxExternalInterface` (0xAF53C8); 7 `EclipseGFxUserEventHandler` (0xAF53D4; its event method 0x00738C90 is `ret 8`, an empty handler); a 0x2c-byte object from ctor 0x0090E5F0 (state 1; sized from the engine object at [0xC31650]+0xC8; I take it for the render/translator-style config [I]); 2 (vtable 0xAF5358); 0xC `EclipseGFxImageCreator` (0xAF53F8); 5 `EclipseGFxImageLoader` (0xAF5404).
- fonts_*.gfx content [C by a tag walker]: CFX (compressed). ExporterInfo (1000) + two DefineFont3 (75): id 1 "Nogard" and id 3 "Dragon Text", flags 0x8C. No tag 1002/1005 (no FontTextureInfo, no DefineCompactedFont): the glyphs are outline vectors, rasterised through GFx's vector/raster glyph cache.
- gfxfontlib.gfx [C]: ExportAssets (56) of 2 entries, id 1 "$TitleFont" (Nogard, 243 glyphs), id 2 "$BodyFont" (Dragon Text, 243 glyphs).
- Movies [C]: inventory.gfx has ImportAssets2 (71): `gfxfontlib.swf` -> `$TitleFont`, and 27 DefineEditText (37) with HasFont (font id 45 for the first, height 280 twips). tooltips.gfx imports and also holds its own DefineFont3 (75). Of the 3 tooltips edit texts one, and of 9 in conversation one, has UseOutlines (flags2 bit 0) clear, i.e. a device-font text; the engine strings show device handling: "Searching GFxFontLib without [Device] flag", "Missing font ... Search log", "GFxFontProvider not installed" (0x00B4FADC; the game installs no provider [I]).
- Glyph cache [C strings, no code of the game]: "Increase raster glyph cache capacity - TextureConfig" (0x00B4E080), "Increase vector glyph cache capacity - SetMaxVectorCacheSize()" (0x00B4E0D8). `GFxFontCacheManager` vtable 0x00B4E0D0 referenced only at 0x009746DA and 0x009778AB (inside GFx), so there are no game-set cache parameters.
- Text field settings seen as AS property names [C]: fontSize, fontScaleFactor, embedFonts, readingWindowFontSize (IME), htmlText in 53 of 57 movies.

### (b) Images, evidence

- Prefixes: `imgps://` at 0x00AF5614 (8 chars), `img://` at 0x00AF5620 (6 chars). Referenced by the engine at 0x00516A78/0x00516AC9 (LoadImage) and by GFx library code (0x00923833, 0x009237E5, 0x009544C4/0x009544EB, 0x00958EAB/0x00958EF5, 0x009A568A/0x009A56B7, 0x009A5998/0x009A59BD): there the library treats both prefixes as "user image, do not resolve against the movie's path" (an 8-char/6-char `repe cmpsb`, then a flag, e.g. 0x0092383C). Other schemes: none; `FILE:` is a prefix inside the name part (wide string at 0x00AF55D4), and "?Async=" 0x00AF55E0, "&Height=" 0x00AF55F0, "?Width=" 0x00AF5604 are the parameters (all wide) [C].
- `ScaleformManager::LoadImage` (0x00516A20; `EclipseGFxImageLoader::LoadImage` slot 0x00517390 = `mov ecx,[0xC31264]; jmp 0x516A20`, `ret 4`) [C]: strips "imgps://" or "img://" (both take the same path, only the exception-state byte differs), finds `?Width=`, `&Height=` and `?Async=` from the end (0x004C0FB0 = find), parses them as ints (0x004C0950), cuts them off the name. If the name contains the wide strings at 0x00AF04A4 / 0x00AF04B4 (read as the 3-letter extensions "jpg" / "png") the image is treated as a file image [I]. If the name starts with `FILE:` the rest is loaded as a file through the resource/texture manager (singleton 0x008472C0; method 0x00846990) (corrected 2026-10-18 by lib-agent-2: the two loaders were the other way round here; the FILE: loader is 0x846990 and the by-name loader is 0x845D80, which reads the setting at [0xC31650], from the pushes at 0x517119-0x517126 and 0x51718C-0x517193); otherwise it is looked up as a texture by name (method 0x00845D80, with a missing name falling back to `default_missing.dds`, corrected 2026-10-18 by lib-agent-2, and `?Async=1` returning an empty texture at once while the load is queued), which is the normal resource lookup, so any 'name.dds' texture in the game's archives (or a live render target registered under that name) resolves. The result is wrapped in an image info (ctor 0x0090D340, 0x28 bytes) with width/height: the URL's if given, else the texture's (virtual +0x10 / +0x14 of the texture, from `SmartData::Attribute::Get` at 0x0053CC40 for the texture manager object) [I for the wrapper details].
- What `imgps` means: not found in the engine or in any movie. In Scaleform 3.x, `imgps://` is the "image with pixel snapping" counterpart of `img://` [I from memory of the SDK, not verified]; in this exe the two are processed identically.
- Where names come from [C]: `.dds` file names. Movies list their static pictures with tag 1001 (DefineExternalImage): e.g. inventory.gfx has 182 such images: `dxt1_Spells_blood3_crp.dds`, `paperdoll.dds`, `ico_silver.dds`, `ico_gold.dds`; portraits.gfx 53 (`Portraits_I16.dds`, `abilities_I1A.dds`); quickbar.gfx 33 (`abilities_IE.dds`, `abilities_I34.dds`); tooltips 10 (`tooltip_linebreak.dds`, `inventory_I8D.dds`); conversation 2. These textures are the exporter's pre-extracted bitmaps named <movie>_I<hex id>.dds, in the game's texture archives; the record layout is id(u16), format(u16 =0x0E), width(u16), height(u16), then two length-prefixed strings (name, file name) [C by bytes].
- Dynamic names: ability, item and talent icons come from the data tables' icon columns (ico_*/abi_* names) and reach the movie through `GetValue` or tokens; the movie calls LoadImage with the name [I: not traced per table here]. Creature portraits: AS `SharedLibrary.Portrait` registers a token observer on `<creatureToken>.Portrait` (and `.PortraitProperties.PortraitTint`, `.PortraitLevelUp`) and, when the value is non-empty and not ending with "?", does `MovieUtils.LoadImageFixedSize(Portrait_mc, name, w, h)` else `unloadMovie()` [C, portraits.gfx SharedLibrary/Portrait.as:137-150]. The engine side is `CCCreature::UpdatePortrait` / `PortraitRenderer` (render-to-texture, per dev/GUI.md `img://portrait_<tag><counter>`).
- MovieUtils.GetImageUrl [C]: empty name -> `MISSING_TEXTURE_NAME`; "?" in name -> same; no "." -> add ".dds"; both `a_nWidthOverride` and `a_nHeightOverride` -> `?Width=W&Height=H`; async -> `?Async=1`; result `"img://" + name + size + async`. Callers: LoadImage, LoadImageWithCallback and LoadImageAsync (MovieClipLoader.loadClip with onLoadInit), LoadImageFixedSize, LoadImageWithMaxSize (scales down after load). All 57 non-font movies contain the string "img://"; none contains "imgps://".
- Where it is `FILE:` [C]: saveload.gfx (SaveFile.as:120 and SaveLoad.as:253) loads save screenshots as `"FILE:" + sScreenshot`; guicommunityfeatures.gfx has it too.

### (c) Localisation, evidence

- Per-movie `Localization` class [C, portraits scripts/__Packages/Localization.as]: `static var Strings = new Object()`, constants like `LOC_OK = 325483`, `LOC_NAV_INVENTORY = 363256`; `Initialize()` sets `Strings.__resolve = LookupStringByName`; `LookupStringByName(name)` returns `GetString(Localization[name])` (in game) or the name (in the editor), or "?NAME" with a trace if the constant is missing; `GetString(id)` -> `ExternalCommands.GetTlkString(id, "#"+id)` (id 0 -> "#0"); `ToggleStringIDs()` shows raw "(NAME)" for debugging. Usage: `Localization.Strings.LOC_TAKE_ALL` as a text value.
- `ExternalCommands.GetTlkString` [C]: `_root.fsc_retval = ""; fscommand("GetTlkString", id + " " + invalid); return _root.fsc_retval;`. Handler `GuiFSCommand::GetTlkString` 0x0051EAE0 [C]: splits the argument string, looks the id up in the talk table (calls 0x004D1C70/0x004D1EB0/0x004D1E90) and, if found, writes the text to the movie variable (0x00518830); otherwise uses the fallback text.
- Designer tags [C]: SharedLibrary.TextField sends any string containing "<" through `ExternalCommands.ReplaceDesignerTags` (fscommand with `_root.fsc_retval = null`).
- TokenManager (AS class) [C]: `AddTokenObserverFunc(label, token, cb)` -> `ExternalInterface.call("AddTokenObserver", label, "", token)` (returns the id); `AddTokenObserverWithID(id, token, cb, delay)` pushes (id, token, delay) triples to `TokenManager.AddTokenObserverBatchData`, sent in batch; `RemoveTokenObserverByID` likewise. The engine delivers values by setting `TokenManager.BatchTokenReplacementCallbackIDs` and `...Values` and invoking `TokenManager.PerformBatchTokenReplacements` (strings at 0x00AF5730, 0x00AF575C, 0x00AF5788, 0x00AF5814, 0x00AF583C; `GuiMovie::DeliverTokens` 0x00519B00 from GUI.md). Tokens are dotted data-tree paths, e.g. "Party.Members.0", "Party.SelectedCount", "GameModeController.CurrentGameMode", "GUI.GuiHighlights.<n>.Highlight", "<creature>.Portrait", "ClientOptions.<option>". There is no `$`-prefixed token language in the exe (no "$LOC" strings in the exe or the movies' scripts; the `$LOC_GAMEPLAY` string noted in dev/GUI.md is an AS constant name).
- Language switch: the fonts file name only [C]; the string table is the engine's talk table (CTlkTable::GetString 0x004E4990 per the address table).

### (d) Input routing, evidence

- Mouse [C]. `ScaleformManager` has 13 message handlers with vtable slot 1 = 0x00511350 (the shared invoke thunk): PlayMovieMessage (0xAF5444), StopMovieMessage (0xAF5450), GUICursorScrollUp (0xAF545C -> member 0x005125E0), ScrollDown (0xAF5468), Left/Right/Middle ButtonDown, ButtonDblClick, ButtonUp (0xAF5474...0xAF54D4; the Left Down handler vtable is built at 0x00512E31 with member 0x00512960; the thunks 0x00512960.. push (button index, down/up) and call 0x00512340 for the click and 0x00512480 for the wheel). 0x00512340 [C]: if the manager is running (+0x15), takes the focus/capture movie, tests the mouse position (0x00510850, returns -1,-1 when no cursor), and sends to the top movie (this+0x80 list's first), to every movie in the vector at +0x90..+0x98 and to the capture movie at +0xA0, through the movie record method 0x00518970 (button down) / 0x00518A50 (button up). Each builds a GFxMouseEvent (type value 2 for the down path; x, y converted with `fild`/`fstp` to floats; button; delta 0) and calls the GFxMovieView's vtable slot at +0x98 (HandleEvent, `GFxMovieRoot` has 52 slots, at 0x00B4CA6C). The record keeps a bitmask of pressed buttons at +0x34 (button bit, +4 shifted for the up variant). The handlers for the wheel call HandleEvent too (0x00512697, 0x005127A7). Mouse move: 0x005188E6 (in 0x00518850, the per-frame move).
- Mouse-over-UI test [C from GUI.md and the code above]: `ScaleformManager::MouseOverInterface` 0x00511420 (cmp [esi+0x15] running; list at +0x80; ... returns whether a movie is hit); the game's pick asks it through the GUI system's slot 71.
- Keyboard / text [C for the structure, I for the semantics]. The per-frame Update 0x00516760 begins with `call 0x514C90`, which: takes the first movie of the list at +0x80 (the front/focused one), reads the text typed this frame from the input layer (0x005145E0), and for each character (wide, from a string at the record; filter string at +0x1DC; a character in the filter is skipped via 0x004C1000 find, -1 = allowed) constructs a char event (a byte flag, the character word, a float pair 0) and calls HandleEvent (0x00514DA8). Then 0x0087DE40/0x00514640 run for key down/up events. The key-code translation is 0x00510930: VK 'A'-'Z' (0x41-0x5A), F1-F15 (0x70-0x7E) and the numpad digits (0x60-0x69) pass through; the table at 0xBF6228 maps (VK -> GFx key code): 0D->0D, 1B->1B, 25-28->25-28 (arrows), 09->09, 08->08, 22->21 and 21->22 (PageDown/PageUp swapped), 23->23, 24->24, 2D->2D, 2E->2E; any other value is returned unchanged.
- The text filter [C]: `GuiExternal::FilterKeyboardInput` (0x0051C760) sets the characters that cannot be typed (the manager's +0x1DC string, via 0x005110F0) for the front movie. PopupLayer's text field calls it on focus with `DAConstants.NAME_RESTRICTED_CHARS = " \t\\:*,=\'^@!%_<>{}()[]$/;\".+|~?#&-"` and clears it with "" on kill-focus. That popup uses `Selection.setFocus(TextField)`, `Key.addListener` and `TextField.onKeyDown` with `Key.getCode() == 13` (Enter).
- Movie-side focus [C]: every movie has its own AS FocusManager (static class: containers, `SetCurrentFocus`, hierarchy of focus containers, `m_bActionHandled`), driven by the game's keyboard/gamepad navigation messages the movies get as fscommands/tokens, and `Selection.setFocus(null)` in SharedLibrary.TextField. GUI.Element installs one global Mouse listener (`sm_fnMouseListener`) that picks the cursor shape: `ExternalCommands.SetCursor("standard"|"standard_pressed"|"ibeam"|"ibeam_pressed"|...)` (fscommand SetCursor), from Mouse.HAND/ARROW/IBEAM. Scrollables use `Mouse.addListener` (wheel). Only 4 movies use `onKeyDown` (credits, login, popuplayer, slideshow); `_global.gfxExtensions` and `hitTestDisable` appear in 56 and 57 movies respectively.
- Keybindings: not in the movies. optionsmenu/quickbar list them ("KeyBind" strings; `AddKeyBinding("TogglePause")` per dev/GUI.md); the engine's `Eclipse::GroupedInputMappings` (vtables 0x00B311E0 etc.) and Enable/DisableKeyRemappingMessage own them. GFx key events are only fed to the front movie for text entry and a few keys (above).
- IME [C strings]: `showLanguageBar` (0x00B4B2D8), `getInputLanguage` (0x00B4B2E8), "Error in loading language bar from" (0x00B50DC0), `readingWindowFontSize` are GFx IME code; the only game import near is `GetKeyboardLayout`. No `Imm*` imports. The game's IME support is therefore Scaleform's library code with no game-side feeding found [I].
- Gamepad: no GFx gamepad handling found in the manager (no joystick events to HandleEvent) [I].

### Not read

- The language string's source for `fonts_<lang>.gfx` (how 0x004C35F0 gets it); which ids 1, 2 and 0x14/0x12 are in the loader state enum (Scaleform 3.0's numbering, which I did not check).
- The exact GFxMouseEvent / GFxCharEvent layouts (type constants beyond the 2 seen), and the full body of 0x00514640 and 0x00512480.
- What the 0x2c-byte state object (ctor 0x0090E5F0, state 1) is exactly; the state id 2 object (vtable 0xAF5358).
- How the portrait render target gets its name registered in the texture manager; the ".png"/".jpg" branch's purpose.
- Per-table icon columns (abi_base, itm) feeding movies; which movie uses "imgps://" (none found, so possibly only plugin or other-build movies).
- Gamepad: any controller path outside the manager.
- The designer-tag grammar `ReplaceDesignerTags` expands (read since: gui4b below).
- Movies other than the 8 decompiled were only byte-grepped.

## GFx's features the movies depend on

Read 2026-10-18 by lib-agent-2 (for DAOParagonEngine #19).

Marking: [C] checked (read in the files or the exe), [I] inferred. Scratch: `%TEMP%\lib-agent-2-guigfx-*` (68 movies extracted to `-x\`, 12 decompiled to `-as\` with FFDec 26.3 via `java -jar ffdec.jar -cli -export script`, tag walkers `-walk*.py`). Note: `ffdec-cli.exe` fails here (wants a JDK on its own path); `java -jar ffdec.jar -cli` works. All 68 movies are read; 12 decompiled: startmenu2, mainmenu, conversation, inventory, optionsmenu, floatylayer, chargen, tooltips, quickbar, load_town, portraits, minimap.

### Short answers

1. [C] The movies are almost plain SWF 8 (AS2). Every one of the 68 .gfx is `CFX` (zlib) version 8. The GFx-only parts are few: (a) every bitmap is an external image, tag 1001 DefineExternalImage (2,014 of them across the 67 non-font movies); (b) the `_global.gfxExtensions = true` extension properties and methods in the shared AS2 library (`hitTestDisable`, `textAutoSize`, `Mouse.getTopMostEntity`, mouse button indices); (c) `img://` URLs loaded with `MovieClipLoader.loadClip`; (d) fonts imported through ImportAssets2 from `gfxfontlib.swf` and supplied by the engine; (e) the stage size reported in view pixels because the engine uses NoScale / centre alignment.
2. [C] No GFx extension tag other than 1000 (ExporterInfo, once per file, 68) and 1001 (2,014) occurs. No DefineSubImage (1008), DefineCompactedFont (1005), FontTextureInfo (1002), DefineExternalGradient (1003), gradient map (1004), external sounds (1006, 1007), or tag 1009. Fonts are plain DefineFont3 vector glyphs. ExporterInfo flags are all 0 (no stripped shapes or textures).
3. [C] No `gfx.controls.*`, `gfx.managers.*`, or CLIK components, no `Extensions.*` class, no `Stage.scaleMode` / `Stage.align` assignment in any decompiled movie. The UI framework is BioWare's own AS2 (`GUI.Element`, `SceneManager`, `ExternalCommands`, `TokenManager`, `SharedLibrary.*`), duplicated as `__Packages.*` in 56 of 68 movies.
4. [C+I] The engine, not the movie, sets the stage mode: each time a view is laid out it calls the view's SetViewScaleMode(0) = NoScale then SetViewport; at creation it calls SetViewAlignment(0) = centre. Movies then lay themselves out against `Stage.width` / `Stage.height` (the view's size in pixels) and the authored 1024x768 (`_root.OriginalStageWidth/Height`), via `MovieUtils.UpdateAllLayouts` which the engine invokes after each resize. Details below.

### Per-movie facts [C]

All: tag-walk of the game's copy (patch003.erf where present). Size = frame size in pixels (twips/20), rate fps, frames.

| movie | size | fps | frames | tag1001 images | tag78 (scale9) | DefineFont | imports from gfxfontlib |
|---|---|---|---|---|---|---|---|
| startmenu2 (main menu) | 1024x768 | 60 | 274 | 44 | 1 | 0 | $TitleFont (+Body) |
| mainmenu | 1024x768 | 60 | 10 | 11 | 0 | 0 | $TitleFont |
| conversation | 1024x768 | 30 | 2 | 2 | 0 | 1 (DefineFont3 "Dragon Text") | $BodyFont |
| inventory | 1024x768 | 60 | 60 | 182 | 8 | 0 | both |
| chargen | 1024x768 | 60 | 203 | 114 | 2 | 0 | both |
| optionsmenu | 1024x768 | 60 | 60 | 43 | 4 | 0 | both |
| load_town (loading) | 1280x768 | 30 | 249 | 16 | 0 | 0 | both |
| floatylayer | 1024x768 | 30 | 1 | 5 | 1 | 0 | both |
| tooltips | 1024x768 | 30 | 1 | 10 | 4 | 1 | |
| quickbar (HUD) | 1024x768 | 30 | 1 | 33 | 2 | 0 | |
| minimap / navbar / portraits | 1024x768 | 30 | 1 | 21 / 29 / 53 | 0 | 0 | |
| fonts_en-us.gfx (all 10 fonts_* alike) | 550x400 | 12 | 1 | 0 | 0 | 2 (DefineFont3: "Nogard" id 1, "Dragon Text" id 3; 231 KB and 199 KB of glyph data in en-us) | none |
| gfxfontlib.gfx | 640x480 | 60 | 1 | 0 | 0 | 2, exported as `$TitleFont` (Nogard) and `$BodyFont` (Dragon Text) | exports them |

Whole set [C]: 68 files, all SWF 8 `CFX`. 64 are 1024x768 or 1280x768 (credits, deathscreen, load_town are 1280x768); frame rate 60 for 47, 30 for the HUD-like and loading pieces. Tag totals: 1000 x68, 1001 x2014, 78 (DefineScalingGrid) x94 in 34 of 68 movies, 70 (PlaceObject3) x35 (all with a blend mode, none with filters, 2 with cacheAsBitmap), 37 (DefineEditText) x706, 39 sprites 8,699, 75 (DefineFont3) x36, 74 (CSMTextSettings) x432. No AS3 (no tag 72/82), no DefineBits/JPEG at all (tags 6, 20, 21, 34, 35, 36 are absent), no sounds.

### External images [C]

- ExporterInfo (tag 1000): version 0x0214 (2.20), flags 0, bitmap format 14 (DDS), no prefix, then the movie's name.
- Tag 1001 layout as read: u16 character id, u16 format (always 14), u16 target width, u16 target height, u8-length export name, u8-length file name. Example, startmenu2: id 1, 1022x314 (0x07FE x 0x013A), export `dxt1_cloud2.png`, file `dxt1_cloud2.png.dds`. Inventory: `dxt1_Spells_blood3_crp.dds`, and `paperdoll` / `paperdoll.dds` (16x16 placeholder). Mainmenu/conversation: files named `MainMenu_I1C.dds`, `conversation_I1F.dds` ("<movie>_I<hex charid>"), also cross-movie ones like `LoginNewAccount_IAB.dds` in mainmenu. 1,079 distinct file names, all `.dds`.
- Where they live [C]: not as loose DDS under those names. All 1,079 resolve through texture atlases inside `packages\core\data\guiexport.erf` (and the patch ERFs): 20 `atl_<movie>_dxt1|dxt5_NN.dds` 2048-wide atlases (DXT1/DXT5, 2 MiB / 4 MiB each) plus `atl_*_dat.xml` mapping `<SourceTexture Name="dxt1_cloud2.png.dds" OffsetAndScale="x y w h" AtlasSize="2048 512" OriginalImageSize="2046 314"/>` to a sub-rectangle. 2,243 source names are listed; every one of the 1,079 file names requested by the movies is among them (0 unresolved, checked). `gui.erf` (186 MB) holds icon DDS (178 DXT5) and the cursors (`.cur` x70), the two TTFs (`nogard.ttf`, `nogard_text.ttf`) and the credits text; it does not hold the movie images.
- So the engine (not the file) turns a 1001 image's name into an atlas sub-rectangle [I from the xml shape plus ScaleformManager::LoadImage at 0x00516a20]. A Ruffle port must resolve 1001 by (file name -> atlas, UV rect) and draw sub-rectangles, i.e. the DefineSubImage function done by the host; with it, 1001 behaves like a normal DefineBitsLossless/JPEG bitmap fill.
- `img://` runtime images [C in AS, I for the host]: `MovieUtils.GetImageUrl` builds `"img://" + name + (".dds" if no extension) + ["?Width=w&Height=h"] + ["?Async=1"]`; `MovieUtils.LoadImage/LoadImageAsync` load it with `MovieClipLoader.loadClip` / `MovieClip.loadMovie` into a clip, 24 loadClip sites in the 12 decompiled movies, called through the shared lib in all of them. Hosts: DDS in the atlases/textures by name, and live-rendered portraits `img://portrait_<tag><n>` (GUI.md, `CCCreature::UpdatePortrait`); a missing name falls back to `MovieUtils.MISSING_TEXTURE_NAME`. This is the render-to-texture path. No BitmapData, attachBitmap, beginBitmapFill, or filter use appears in any decompiled script [C, grep over 12 movies].

### Fonts [C]

Text uses imported fonts: ImportAssets2 (tag 71) from `gfxfontlib.swf` of `$TitleFont` (48 imports across movies) and `$BodyFont` (43). `gfxfontlib.gfx` exports them as DefineFont3 ids 1 and 2 (Nogard flags 0x8c, Dragon Text 0x84). The per-language `fonts_<lang>.gfx` (10 files: cs-cz de-de en-us es-es fr-fr hu-hu it-it pl-pl pseudo ru-ru) hold the same two names as full DefineFont3 sets for that language (en-us 151 KB; ru-ru 188 KB). The exe has the strings `fonts_en-us.gfx` (0x00AF5588), `gfxfontlib.swf` (0x00B490A8) and "Searching GFxFontLib..." / "GFxFontProvider not installed" (0x00B4FAAC...), so the engine installs a GFxFontLib and a movie of `fonts_<language>.gfx` supplies the `$...Font` import; the exact swap is not read. A Ruffle port can instead register the two fonts (`nogard.ttf`, `nogard_text.ttf` in gui.erf, 172 KB and 145 KB) or the DefineFont3 sets, and satisfy ImportAssets2 with them. Font tags are plain, so no DefineCompactedFont decoder is needed.

### ActionScript 2 survey (12 movies, 1,986 .as files; the shared lib is the same in all) [C]

Counts are movies out of 12 unless noted.

- `_global.gfxExtensions = true` (12/12), also `_global.noInvisibleAdvance = true` (12/12).
- Stage: no `scaleMode`/`align`/`addListener` assignment anywhere. `Stage.width` / `Stage.height` are read (47 files): `SceneManager.SetSafeZone`, `Behaviour.ScaleToScreen.UpdateLayout`, `FloatyLayer` (`GUI.UpdateFloatyLayerStageSize` sent with Stage.width/height), and the 1024x768 fallback when `GUISystem.RunningInGame` is false. `_root.OriginalStageWidth/Height` is saved first thing (12/12).
- GFx extension properties: `MovieClip.hitTestDisable` (24 sites, set true on drag impostors etc.); `TextField.textAutoSize = "shrink"/"none"` (12/12); `Mouse.getTopMostEntity(false)` (12/12, `GUI.Element` custom mouse listener, which also dispatches `onPress/onRelease/onReleaseOutside(button, false)` itself); mouse button indices `Mouse.LEFT/RIGHT/MIDDLE` and `+4` variants (GFx's per-button mouse events; the lib checks `Mouse.LEFT != undefined` first, so it degrades); `Mouse.setCursorType`, `Mouse.forceCursorUpdate` are defined by the movie, not GFx. Not found: `topmostLevel`, `noAdvance`, `focusGroupMask`, `getExactCharBoundaries`, `verticalAlign`, Selection.captureFocus/moveFocus, `Extensions.*`, `gfx.*` imports.
- Focus: only `Selection.getFocus()`/`Selection.setFocus(null)` (12/12); `tabEnabled=false` set on `TextField.prototype` and `MovieClip.prototype`. Own `FocusManager` class, no CLIK focus.
- Text: `htmlText`/`html` heavily, `autoSize`, `wordWrap`, `selectable`; `getTextFormat/setTextFormat` in 1 movie. Plain Flash 8 features. Custom Date-less `ProcessText` for `$LOC_...` tokens.
- Misc Flash 8 usage: `flash.geom.Point/Rectangle`, `getBounds`, `localToGlobal`, `startDrag/stopDrag`, `attachMovie`, `getNextHighestDepth`, `setInterval`, `getTimer`, `ASSetPropFlags` on `Array.prototype` (12/12), `blendMode = "overlay"` (1 site) plus 35 PlaceObject3 blend modes, `new Color` (1). No ContextMenu, LocalConnection, SharedObject, XML, Key listeners (Key.* is absent; keyboard comes from the engine through ExternalInterface `FilterKeyboardInput` and messages).
- Scale9: DefineScalingGrid tag 78 in 34 of 68 movies (94 total; inventory 8, guicommunityfeatures 15, loginnewaccount 7, characterrecord 6, store 3...). Ruffle supports DefineScalingGrid (scale9) for AS2 sprites, so low risk [I]. No `scale9Grid` property use from script.
- FSCommand (all via the lib's `ExternalCommands`, so every movie that has the lib has all of these): 53 distinct names seen in 12 movies (SendMessage, SetValue, SetValueUnicode, PlayGUISound, StopGUISound, GetTlkString, FormatFloat, ListMemberNames, GetNumMembers, ReplaceDesignerTags, LoadMovie, PlayMovie, StopMovie, MoveToFront, AdvanceFrame, SetCursor, SetTimeScale, Warning, the context calls Create/Push/Pop/Activate/Deactivate/BlockGameMessage/EnableGameMessage, and tactics/ability queries like ListTactic*, HasAbility, GetItemStat...). Answers come back by setting `_root.fsc_retval` [GUI.md].
- ExternalInterface.call (15 names): SendMessage (60 sites), GetNumber, GetValue, GetString, AddTokenObserver, SendTelemetryInfo, FilterKeyboardInput, GetCurrentModuleName, GetGender, IgnoreItemRequirements, CanLevelUpFromCharacterSheet, GetAbilityAttributes. Ruffle has ExternalInterface and FSCommand callbacks, so only the host side is needed.
- The engine calls into movies by `Invoke` / SetVariable of: `_root.PlayScene`, `_root.StartSceneFromGame`, `_root.FinishSceneFromGame`, `_root.Rewind`, `_root.Filename`, `_root.OriginalStage*`, `MovieUtils.UpdateAllLayouts` (strings at 0x00AF5510, 0x00AF56E8, 0x00AF5704, 0x00AF5720, 0x00AF57F0, 0x00AF5520) [C strings]; the token mechanism sets `TokenManager.BatchTokenReplacement*` arrays (GUI.md).

### How the engine sets stage scaling and alignment [C for the call sites, I for the slot names]

- `GUI::ScaleformManager::CreateMovieView` (0x00514940): after `CreateInstance(0)` on the movie def (def flags 0x8006 passed to GetMovieDef), calls the view's vtable +0x8C (SetPause-like, 1 if not already), +0x84 with float 0.0 (Advance(0)), then **+0x60 with 0** at 0x00514AA7. In GFx 3.0's GFxMovieView layout slot 24 is SetViewAlignment and 0 is Align_Center. Slot numbering by the header is [I]; the call, its argument and place are [C].
- The per-view resize routine at **0x005111B0** (called from the loop at 0x00511D95 over every movie, followed by `Invoke("MovieUtils.UpdateAllLayouts")` at 0x00511DC4) builds a viewport struct: buffer width/height = the client size, left/top 0 (or the scissor-like rect at record +0x28..0x34 when its flag at +0x24 is set), a float scale (1.0, or below 1024x768 1024/w when w/h < 4/3 else 768/h; corrected 2026-10-18 by lib-agent-2: the `[+0x184]` call at 0x00511242 is not a factor of the scale but the display's pixel aspect, EnumDisplaySettingsW against the back buffer, stored as the viewport's AspectRatio), then calls vtable **+0x58 with push 0** (SetViewScaleMode, 0 = SM_NoScale in GFx 3.0) at 0x00511330 and **+0x50** (SetViewport(&viewport)) at 0x0051133E. So: NoScale + centre alignment, and the viewport is the whole client area; the movie sees `Stage.width/height` equal to the view's size in pixels (matches GUI.md's "A movie's stage against the view": Stage.height read about 800, not the authored 720). The movie-side layout code (`ScaleToScreen`, `SceneManager.SetSafeZone`, FloatyLayer) then positions against 1024x768-authored coordinates and the live Stage size.
- No other SetViewScaleMode call with a different mode was found in the 0x510000 range [C, visible scan limited to 3,140 lines of disassembly]; the whole exe was not scanned for other callers.
- Consequences for Ruffle [I]: run each movie with `scaleMode = noScale`, `align = ""` (centre) and a viewport of the window's size, so `Stage.width/height` equals the window; and call `MovieUtils.UpdateAllLayouts` on every resize. Ruffle's AS2 `Stage.width/height` in noScale mode follows the player size, so this should match, but is untested.

### What a Ruffle port would shim or replace, ranked by movies needing it

1. External images (tag 1001): 67 of 68 movies (all but fonts). Needs a loader that maps the file name to the atlas DDS and sub-rect (`atl_*_dat.xml`), decodes DXT1/DXT5, and feeds it in place of the missing bitmap fill, or convert once offline: extract each movie's 1001 images from the atlases into PNG/DefineBitsLossless and rewrite the SWF (the ExporterInfo-flags-0 files carry nothing else GFx-specific in the display list). Highest-value shim.
2. The GFx extension property set under `_global.gfxExtensions` in the shared lib: `Mouse.getTopMostEntity` and `MovieClip.hitTestDisable` (every movie with GUI.Element, 56 of 68, and the drag/tooltip code), mouse button indices (`Mouse.LEFT+4` style, which Ruffle's AS2 mouse listener lacks the second parameter for), `TextField.textAutoSize`. Ruffle ignores unknown properties silently, but the custom mouse dispatch in `GUI.Element` relies on `getTopMostEntity`, so without it no button works; implement the three as a host-provided polyfill (hit test against the display list, honouring hitTestDisable) or replace GUI.Element's listener.
3. Fonts via ImportAssets2 from `gfxfontlib.swf`: ~60 movies. Ruffle supports ImportAssets2 from another SWF url; the host must serve `gfxfontlib.swf` (the .gfx renamed to FWS) or register the two DefineFont3 fonts under `$TitleFont`/`$BodyFont`; the per-language `fonts_*.gfx` choice is the host's.
4. `img://` through `MovieClipLoader.loadClip` / `loadMovie`: any movie showing item/ability/portrait icons (inventory, store, abilities, characterrecord, portraits, ...: all that call the lib's `MovieUtils.LoadImage`). Needs a scheme handler returning an image, including live portrait textures (host render-to-texture in the port) and `?Width=&Height=&Async=1`.
5. Engine-side stage rules: NoScale + centre, viewport = window, `MovieUtils.UpdateAllLayouts` after each resize and `_root.OriginalStageWidth/Height`, `_root.PlayScene/StartSceneFromGame/FinishSceneFromGame/Rewind` calls, FSCommand `_root.fsc_retval` return convention. All movies.
6. The host API itself: 67 FSCommands + 17 ExternalInterface names, the token manager arrays (`TokenManager.BatchTokenReplacement*`); every movie.
7. Low risk: DefineScalingGrid (34 movies, Ruffle supports it), PlaceObject3 blend modes (35 tags, `overlay` etc.; Ruffle supports blend modes), cacheAsBitmap (2 tags), CSMTextSettings (432), DefineEditText html text, `ASSetPropFlags`, 60 fps timeline frame rates, SWF version 8 (no AS3).

Not needed at all: DefineCompactedFont/FontTextureInfo, DefineSubImage tag, external sounds, gradients, CLIK, `Extensions.*`, filters, BitmapData, render of 3-D in the movie (the 3-D paperdoll is a live-rendered `paperdoll` image fed by the host).

### Not read

- Whether `fonts_<language>.gfx` is loaded as a separate movie or through GFxFontLib alone, and the exact string pairing (only the strings were seen).
- How the engine fills a 1001 image (the code behind `ScaleformManager::LoadImage` 0x00516A20 and the GFx image creator); I read the AS side and the atlas files only.
- The vtable slot names for 0x50/0x58/0x60 were taken from my memory of GFx 3.0's header, not from a vtable dump of GFxMovieRoot; the call sites, arguments and order are read.
- Whole-exe scan for other SetViewScaleMode/SetViewAlignment callers; whether the application object's slot at +0x184 is a GUI scale option.
- The engine sends a double click as a second down event with button + 4; whether the convention is GFx's own or BioWare's is not read (the lib only checks `Mouse.LEFT != undefined`).
- No Ruffle run was done on any movie (nothing was played or rendered).


## GUI follow-ups, part a (gui2a): scene start protocol, fsc_retval2, the token batch (lib-agent-2, read 2026-10-18)

Read-only. Exe: GOG `DAOrigins.exe` (addresses are GOG's). Movies: decompiled AS2 under `%TEMP%\lib-agent-2-guicmd-x\allgfx\as_<movie>\scripts\` (the `__Packages` classes are identical across movies, so one path is given as `as_abilities/...` or `as_tactics/...`; `AS:` prefix below means that tree). Claims are marked [C] checked (read in the exe or the files) or [I] inferred. Scratch: `lib-agent-2-gui2a-*` in %TEMP% (call scanner `-calls.py`, SWF frame-label walker `-labels.py`).

GFx call conventions used below (all [C] by the pushed arguments, [I] for the names, which are Scaleform GFx 3.x's `GFxMovie`): vtable slot +0x2C SetVariable(path, value, sticky), +0x30 GetVariable(&value, path), +0x34 `SetVariableArray(type, path, index, ptr, count, sticky)`, +0x38 `SetVariableArraySize(path, count, sticky)`, +0x3C `GetVariableArraySize(path)`, +0x40 `GetVariableArray(type, path, index, ptr, count)`, `GuiMovie::Invoke` 0x00518850 (name, result*, args*, nargs). Array element type codes: 0 = int (32-bit), 4 = wide string (`wchar_t*`), 5 = GFx value (16-byte {type, ..., double/pointer at +8}). GFx value types seen: 3 = number, 4 = narrow string, 5 = wide string.

### Short answers

1. **StartSceneFromGame.** `_root.StartSceneFromGame()` is invoked with **no arguments** (`GuiMovie::StartSceneFromGame` 0x00518D20: `Invoke("_root.StartSceneFromGame", 0, 0, 0)`), only by `GuiMovie::Rewind` 0x00518E00, whose only caller is `ScaleformManager::OpenMovie` 0x00514B10 (after it has made or found the record and set it visible). It is called when the record's state (+0x38) is not 2 (0 = new, 1 = finished) or the record flag 0x04 is set. It is **skipped** when OpenMovie is called for a record already started and not finished (state 2) and without flag 0x04: that is exactly the HUD toggle (hide-keep then show) and a second Show of a visible scene. No scene in the 60-scene table has record flag 0x04 except SpellsTalents 0x0A and Skills 0x0D, which have no movie, so in practice no real movie is restarted by the flag. The engine never passes a scene label to StartSceneFromGame; the label comes from the movie (`PlayScene(null)` → `""` → `StartScene_Default`) and, afterwards, from the movie's own AS (chargen/levelup/journal ...). The only scene label the engine ever sends is **"Outro"**, via `_root.PlayScene(L"Outro")` (wide string, GFx type 5), and only from the scene's Hide when the scene's `b` mask has bit 4 (Loading 0x0E b=5, Saving 0x24 b=6, per the scene table). `FinishSceneFromGame` (0x00518CD0, no args) is invoked on Close and on shutdown. `_root.Rewind()` (AS: `gotoAndPlay(1)`) is invoked at the next show only if an Outro was played before (record byte +0x101).
2. **fsc_retval2** is written by exactly two handlers (`ListTacticTargets` 0x00714860, `ListTacticConditions` 0x00714AF0; string 0x00B16AF4 has two code refs, 0x0071494A and 0x00714C6A), both as an int array (type 0) next to `fsc_retval` (int array). It is read by exactly one movie: **tactics.gfx** (`ConditionPicker.as` lines 41-43, 116-118, 291-295), through the `DAExternalCommands.ListTacticTargets/ListTacticConditions` wrappers that return `{Targets, IDs}` / `{Conditions, IDs}`. It holds object ids (per-follower targets: `GetObjectNameByID(id)`; -1 = none) and condition ids (-1 or 2147483647 = none; otherwise an object id substituted for `%s` in the condition's name). `ListMemberNames` writes only `fsc_retval` (string array, type 4) and is read only by `debug.gfx` (`Debug.as:297`, then `.sort(Array.NUMERIC)`); it never touches fsc_retval2. No other movie reads fsc_retval2 (all the other copies of `DAExternalCommands.as` carry the same wrapper, never called).
3. **Token batch.** Per movie per frame, in `GuiMovie::Step` (0x0051ADA0) after the movie's own Advance: **RemoveBatch** (0x0051AC10) then **AddBatch** (0x0051A8B0); then, after all records have stepped, **DeliverTokens** (0x00519B00) from 0x00511CA0. AddBatch reads `TokenManager.AddTokenObserverBatchData` as a flat array of triples [id: number, tokenPath: **narrow string** (GFx type 4), delay: number], whole array in one call, then **empties it** with `SetVariableArraySize(path, 0, sticky)`. Deliver writes `BatchTokenReplacementCallbackIDs` as an **int array** (SA type 0) and `BatchTokenReplacementValues` as a **wide-string array** (SA type 4, always strings, numbers and bools as text), then `Invoke("TokenManager.PerformBatchTokenReplacements")` with no arguments, up to 11 rounds a frame; the AS clears both arrays itself. Details and file:line below.

### 1. Scene start protocol

#### Engine side [C]

| Function | Address | What |
|---|---|---|
| `ScaleformManager::OpenMovie(id, name, layer?, flags?)` | 0x00514B10 | `GetRecord(name)` 0x511730; if none, `CreateMovieView` 0x514940 + `GuiMovie` ctor 0x51A040 (state +0x38 = 0, visible +0x28 = 1) and insert in the set; then `SetVisible(1)` 0x518CB0, **`GuiMovie::Rewind` 0x00518E00**, then 0x510850 / 0x518860 (set the stage/view). Callers: `GUISystem::OpenScreen` 0x00523BF0 (`call 0x514B10` at 0x523C3F), and a small wrapper 0x514C70 (name from a message). |
| `GuiMovie::Rewind` | 0x00518E00 | `if (state != 2 \|\| (flags & 4)) StartSceneFromGame(); if (byte +0x101) { Invoke("_root.Rewind"); } byte +0x101 = 0; state = 2`. |
| `GuiMovie::StartSceneFromGame` | 0x00518D20 | `if (view) Invoke("_root.StartSceneFromGame", null, null, 0)`. Only caller: Rewind (0x518E0F). |
| `GuiMovie::FinishSceneFromGame` | 0x00518CD0 | calls 0x518860 (stage), `if (view) Invoke("_root.FinishSceneFromGame")`, byte +0x101 = 0, state = 1. Callers: 0x511940 (Close: `HideAndFinish(name)`, from `GUISystem::CloseScreen` 0x00523FF0 at 0x52402D, then the record is dropped by 0x5151C0), and 0x5151F0 (every record, game shutdown / unload all; at 0x517F0D). |
| `HideScreen` (hide-keep) | 0x00523CB0 → 0x00511990 | 0x518860, `Step(0)` once, `SetVisible(0)`. **No** FinishSceneFromGame: state stays 2, the movie is kept loaded, a hidden record does not Step (`Step` returns at once if +0x28 == 0). Used by the HUD's temporary hide (scene vtable slot 7 = 0x00A55990, `ToggleMainInterfaceMessage`). |
| `ScaleformManager::PlayOutro(name)` | 0x005119F0 | `Invoke("_root.PlayScene", ..., args = [{type 5, L"Outro" at 0x00AF5504}], nargs 1)`, then record byte +0x101 = 1. Reached through the GUI system vtable slot 81 (0x144 = 0x00523C70, which maps name to the record). |
| `GUIScene` Hide, slot 6 | 0x00A55910 | `if (b & 1) 0xC31650->slot 0x98(1)`; **`if (b & 4) GUISystem.slot 0x144 (PlayOutro)(name) else GUISystem.slot 0x48 (CloseScreen)(name)`**; `if (b & 8) hide BookBack` and fade `da_blurdesaturate` out. (Show, 0x00A55830: `b & 1` → slot 0x98(0); GUISystem slot 0x44 (OpenScreen)(name); `b & 2` → the "open book" sound (0x510CA0(2, "Click") → 0x522C30; corrected 2026-10-19 by lib-agent-2); `b & 8` → show BookBack 0x3B and turn `da_blurdesaturate` on.) Hide-keep, slot 7 0x00A55990: GUISystem slot 0x4C (HideScreen). |
| `MovieRecord::Step` outro tail | 0x0051AE72.. | only if byte +0x101 is set (an Outro was played): `GetVariable("_root.WaitingToBeStopped")`; if it is a non-empty narrow string (type 4), call GUISystem slot 0x48 (**CloseScreen**)(that name) and `SetVariable("_root.WaitingToBeStopped", "")`. This is what the movie's `ExternalCommands.HideGuiScene("<scene>")` (`_root.WaitingToBeStopped = name`) ends. |

So the contract for a replacement host, per screen: `OpenScreen` ⇒ [create view if needed, set `_root.Filename`, `_root.OriginalStageWidth/Height` (0x0051A040)] ⇒ `StartSceneFromGame()` iff first show or after a Close; `Close` ⇒ `FinishSceneFromGame()` then drop; HUD hide-keep ⇒ no AS call at all.

Scene Outro users (b mask bit 4) [C from table + tag walk]: **Loading 0x0E** (b=5; `load_town` label `Outro` at frame 211; its frame 249 script `ExternalCommands.HideGuiScene("Loading")`, `AS: load_town/scripts/frame_249/DoAction.as:1`), **Saving 0x24** (b=6; `savescreen`, label `outro` at frame 13, `frame_31/DoAction.as:1` `HideGuiScene("Saving")`; the engine sends "Outro", the label is `outro`: AS2 frame labels match case-insensitively [I]; savescreen's InitMovie takes an undefined class `MySceneName`, so `PlayScene("Outro")` goes through `IsSceneLabel` false → plain `_root.gotoAndPlay("Outro")`). LightLoading 0x21 and FadeToBlackLoading 0x22 have `Outro` labels and `HideGuiScene` scripts (`load_light` / `load_fade` frame 69) but b=1 / 0 in the table, so their Hide does not send PlayScene by this path; how their Outro is started was not found (Not read). `conversation` has `StartScene_Outro` (label `Outro` at frame 2, `MainTextContainer` frame 32 script `HideGuiScene("Conversation")`) but Conversation's b=0 and its Hide slot is the base's (0xB1A950 slots 6/7 = 0xA55910 / 0xA55990), so who sends it "Outro" was not found (Not read); likewise `slideshow` (`FinishScene_Outro`), `startmenu2` (`StartScene_Outro`), `newgame` (labels Intro/Outro), `bookback` (own `_root.gotoAndPlay("outro")`, `BookBack.as:51`).

#### Scene labels the movies define (root timeline frame labels, tag 43 walk of the 56 movies [C]; engine does not send them, the movie's own AS does via `SceneManager.PlayScene`)

| Movie | Frame labels (frame) |
|---|---|
| chargen | CharacterGeneration 1, RaceGender 24, SoundsetAppearance 44, Attributes 64, Skills 84, SpellsTalents 104, Import 124, OtherImport 144, Review 164, Specialization 184 (`StartScene_<label>` in `CharGen.as:230-278`; `PlayScene("...")` calls in the `*Scene.as` classes) |
| levelup | hidden 1, levelup_intro 7, Attributes 30, Skills 53, SpellsTalents 76 (`LevelUp.as:56-65`) |
| journal | hidden 1, CodexIndex(_intro) 7/29, CurrentQuests 30/52, CompletedQuests 53/75, ConversationHistory 98, PRC 99/121 (`Journal.as:470-556`) |
| characterrecord | hidden 1, stats_intro 7, stats 29, heroic deeds 30 (`CharacterRecord.as:587,592`) |
| inventory / itemexamine | hidden 1, equipment_intro 7, equipment 29, container_intro 30, container 52 (itemexamine: left 2, right 9) |
| optionsmenu | Game 1, Controls 11, Audio 21, Video 31, Online 41, Debug 51 |
| saveload | Save 1, Load 21, LoadEmpty 41, SelectHero 61, SelectHeroEmpty 82 |
| startmenu2 | MainMenuIntro 1, MainMenu 241, ModuleList 242 |
| load_town | Outro 211; load_light / load_fade: Intro 1, Loading 56, Outro 57 |
| savescreen | intro 2, refresh 11, outro 13 |
| newgame | Intro 1, Outro 11; conversation Outro 2; slideshow Outro 2; bookback intro 1, show 2, outro 31 |
| abilities, armycontrol, crafting, map, store, tactics, worldmap | hidden 1 plus `*_intro` / named sections |
| guicommunityfeatures | Achievements 1, ScreenshotsManager 10; prccontentmanager OfferList 1, ContentProgress 10, ContentStatus 20 |

(The remaining movies have no labels.)

#### AS side [C]

Every scene movie's frame 1 (`as_<movie>/scripts/frame_1/DoAction.as:121-133`, identical in all 56 except `dialoguepop`, which has no frame-1 script) defines `_root.StartSceneFromGame = function(){ GUISystem.StartSceneFromGame(); }`, `_root.FinishSceneFromGame`, `_root.PlayScene = function(x){ SceneManager.PlayScene(x); }`, then `GUISystem.InitMovie(<SceneClass>)`.

- `GUISystem.InitMovie(cls)` (`GUISystem.as:93-102`): `Localization.Initialize()`, `SceneManager.SetCurrentSceneClass(cls)`, defines `_root.Rewind = function(){ _root.gotoAndPlay(1) }`, and `setInterval(ExternalInitCheck, 100)`: after 100 ms, if `RunningInGame` is still false the movie starts itself standalone (`StartSceneFromFlash`: stage size, Platform "PC", `BeginMovie`). So a host must call StartSceneFromGame within 100 ms of load or the movie self-starts in "flash" mode (with its mock data).
- `GUISystem.StartSceneFromGame` (`GUISystem.as:111-119`): `GuiShuttingDown = false; Platform = ExternalCommands.GetValue("Scaleform.Platform"); IsMac = GetValue("Scaleform.IsMac"); _root.Platform = same; RunningInGame = true; SceneManager.BeginMovie()`. **`Scaleform.Platform` and `Scaleform.IsMac` are not strings in the exe** (checked: no `--strings Scaleform.` hit, only RTTI), so `GuiExternal::GetValue` (0x0051EC60) finds nothing and answers its not-found value, a number 0 (type 3; code at 0x0051ED5A) [I for the final value]; `InConsoleMode()` (`GUISystem.as:141`) is `Platform != "PC" || EnableController == 1`. A port should answer "PC" and false/0 explicitly and treat console mode as off unless `DebugOptions.EnableController` is set; what vanilla actually did with Platform = 0 was not traced (Not read).
- `SceneManager.BeginMovie` (`SceneManager.as:48-63`): creates `_global.GameData`, calls `<SceneClass>.StartScene()` if defined, and if no scene is current, `PlayScene(null)`.
- `SceneManager.PlayScene(label)` (`:19-35`): null → "". If the label differs from the current and `IsSceneLabel(label)` (label is "" or the class has `StartScene_<label>` / `FinishScene_<label>`; "" maps to "Default"): `EndCurrentScene()` (runs `FinishScene_<current>`), `_root.gotoAndPlay(label)`, `BeginScene(label)` (safe zone 1.0 on PC, `FocusManager.ResetFocusHierarchy`, runs `StartScene_<label>`, initialises queued elements). Otherwise just `_root.gotoAndPlay(label)`.
- `SceneManager.EndMovie` (via FinishSceneFromGame): `EndCurrentScene`, `<SceneClass>.FinishScene()`, clears every interval in `_global.ActiveIntervalIDs`.

### 2. fsc_retval2 [C]

Exe: `ListTacticTargets` sets `_root.fsc_retval` (0x007148F0..: `push 0xAF56D0` = `_root.fsc_retval`, SA type 0) then `_root.fsc_retval2` (0x00714944..: `push 0xB16AF4`, SA type 0). `ListTacticConditions` writes the pair at 0x00714C1A / 0x00714C6A the same way. `ListMemberNames` 0x00520340 writes `_root.fsc_retval` with type 4 (wide strings) at 0x005204F5 and nothing else. The other `ListTactic*` (BaseConditions, Actions, Abilities, ModalAbilities, Items) set only `fsc_retval` (0x714A7E, 0x714DAE, 0x714F3A, 0x7150EA, 0x7147EE).

AS, wrapper (`AS: as_tactics/scripts/__Packages/DAExternalCommands.as:6-25`; same text in abilities, characterrecord, chargen, crafting, inventory, itemexamine, itemupgrade, levelup, notifications, popuplayer):
```
ListTacticTargets(id):    fsc_retval = null; fsc_retval2 = null; fscommand(...); return {Targets: fsc_retval, IDs: fsc_retval2}
ListTacticConditions(b,t): same with {Conditions: fsc_retval, IDs: fsc_retval2}
```
Readers (all in tactics.gfx):
- `ConditionPicker.as:39-52`: `_loc3_.Target = Targets[i]; _loc3_.TargetID = IDs[i]; Name = Tactics.GetTargetName(Targets[i], IDs[i])`. `Tactics.as:381-391`: if the 2DA `TACTICS_TARGET_TYPES` row has `GENERATEPERFOLLOWER != 0` the name is `DAExternalCommands.GetObjectNameByID(IDs[i])` (the follower), else a talk-table string. So IDs are **object ids** of the follower the target type is generated for (-1 for the others, as the Flash mock shows, `Tactics.as:53-56`: `IDs:[-1,-1]`).
- `ConditionPicker.as:116-146, 291-298`: `Condition = Conditions[i]; ConditionID = IDs[i]`; `Tactics.GetConditionName(c, id)` (`Tactics.as:398-420`): `id != -1 && id != 2147483647` → `%s` in the condition's text is replaced by `GetObjectNameByID(id)`. So IDs here are object ids of the item/creature a condition names, or -1 / 0x7FFFFFFF for none. The ids are also sent back to the engine through `Tactics.PickCondition(row, target, condition, targetID, conditionID)`.
Not read in any other movie (grep of all 56 decompiled trees for `fsc_retval2`: only the `DAExternalCommands` wrapper copies). Port: set both variables before the AS reads them, `fsc_retval2` must be an array the same length as `fsc_retval` (indexed in step) and must be reset to null before each call as the wrapper does.

### 3. The token batch protocol [C unless marked]

#### AS side (`AS: as_tactics/scripts/__Packages/TokenManager.as`, identical in all movies)
```
:3-8   static vars BatchTokenReplacementCallbackIDs, BatchTokenReplacementValues (set by the engine),
       cm_mapCallbacks = {}, AddTokenObserverBatchData = [], RemoveTokenObserverBatchData = []
:12-17 AddTokenObserverFunc(label, token, cb): id = Number(ExternalInterface.call("AddTokenObserver", label, "", token)); cm_mapCallbacks[id] = cb
:18-29 AddTokenObserverWithID(id, token, cb, delay): delay = delay ?? 0; cm_mapCallbacks[id] = cb;
       AddTokenObserverBatchData.push(id); .push(token); .push(delay)       // triples
:30-33 RemoveTokenObserverByID(id): RemoveTokenObserverBatchData.push(id)    // flat ids
:42-52 PerformBatchTokenReplacements(): for i < IDs.length: cm_mapCallbacks[IDs[i]](Values[i]);  IDs.clear(); Values.clear()
```
`Array.prototype.clear = function(){ this.splice(0) }` is defined in each movie's frame 1. Observer ids are the `_OBSERVER_ID` constants (`_OBSERVER_ID.as`, e.g. TACTICS 501.., SLIDESHOW 805/806) or the engine's for `AddTokenObserverFunc`. Callers: 134 `AddTokenObserverFunc`, 89 `AddTokenObserverWithID` (80 pass `a_fUpdateDelay` through unchanged = mostly undefined → 0; 9 in `SharedMap.as` pass literal `1`).

#### Engine: per-frame order [C]
`GUISystem update 0x00523B80(dt, dt2, flag)` → `ScaleformManager::Advance 0x00516760(dt, flag)` → for each record `GuiMovie::Step 0x0051ADA0(dt, dt2, flag)`:
1. return if the record is hidden (+0x28 == 0) or (`flag` set and record flag 0x10 clear);
2. `view->Advance(...)` (slot 0x78, runs the AS timeline and any AS pushes into the batch arrays);
3. **`0x0051AC10` RemoveBatch**, then **`0x0051A8B0` AddBatch** (this order);
4. the outro tail (above).
Then `0x00511CA0` (from 0x523BE6) calls **`GuiMovie::DeliverTokens 0x00519B00(flag)`** for each record: `if (flag && !(record flags & 0x10)) return` (same gate), so while paused only 0x10 movies get values.

#### AddBatch 0x0051A8B0 [C]
1. `n = view->GetVariableArraySize("TokenManager.AddTokenObserverBatchData") / 3` (0xAAAAAAAB multiply); if 0 return.
2. Resize a scratch vector (this+0xC4) of 16-byte GFx values to 3n; `view->GetVariableArray(type 5 = values, path, 0, scratch, 3n)`.
3. For each triple: id = element 0 read **as a double at +8 with no type check**, truncated to int (FPU control word forced to round-toward-zero, `fistp`); **element 1 must be type 4 (narrow string), else the whole triple is skipped**, the string pointer at +8 is copied into an `ECString` (0x4C4140); element 2 (delay) is read as a double at +8, converted to float. Then `RegisterObserver 0x0051A4A0(id, token, delay)`.
4. `view->SetVariableArraySize("TokenManager.AddTokenObserverBatchData", 0, sticky 1)`: the AS array is **emptied by the engine** (length 0). A trailing partial triple is ignored but also dropped.
`RegisterObserver 0x0051A4A0`: if the id is already in this movie's map (0x5191E0): replace its token path (0x4C2AB0) and re-evaluate it (0x5C7CE0 = refresh, no new object); else create a `GUI::ScaleformTokenObserver` (0xA0 bytes, vtable 0x00AF689C, ctor 0x00524A80, base 0x005C7E80 with `delay` stored as a float at base +0x3C), insert it in the id map (this+0x84) and the observer list (this+0x3C) and refresh it (0x5C7CE0). The refresh resolves the data-tree attribute and notifies [I], so the first value arrives in the same frame's DeliverTokens (Step precedes it).

#### RemoveBatch 0x0051AC10 [C]
`n = GetVariableArraySize("TokenManager.RemoveTokenObserverBatchData")`; if 0 return; resize scratch int vector (this+0xD4); `GetVariableArray(type 0 = int, path, 0, scratch, n)`; for each id `0x0051ABC0(id)` (look up in the id map 0x51F4F0, unlink from the observer list and from the map, destroy the observer via vtable slot 2 `(1)`); then `SetVariableArraySize(path, 0, 1)`. (0x0051AB70 is the same removal keyed by the observer's own pointer, for 0x51A4A0's id replacement path.) Removing an id the movie never added is a no-op; the AS callback map entry `cm_mapCallbacks[id]` is **not** deleted by the AS (only the engine side is).

#### Notify: observer to pending lists [C]
Observer vtable (0x00AF689C): slot 0 0x005C7550 (changed: sets dirty +0x31; if +0x32 and the argument differs from +0x34, resets +0x30/+0x31), slot 1 0x005C7580, slot 2 0x00524BC0 (dtor), slot 3 0x005C8050, **slot 4 0x00524BA0 (value ready: `movie->0x00519AA0(id = observer+0x9C, value wide string)`)**. `0x00519AA0`: if `id` is already in the movie's pending id vector (this+0xA4; search 0x63C4B0) replace the value in the parallel `wstring` vector (this+0xB4, 0x1C-byte elements; assign 0x4C2AB0); else append id (0x6F1340) and value (0x40AB40). So a token changing twice before delivery is sent once with the latest value.

#### Observer delay (`updateDelay`) [C code, I units]
Base observer fields: +0x3C delay (float, set from the batch's third element), +0x40 accumulator (starts 0.0), +0x30 busy, +0x31 dirty. The per-frame tick 0x005C7CF0(dt): if dirty: if delay > 0, accumulator += dt and the refresh waits until accumulator >= delay (then accumulator is reset to delay's value / the comparison 0x5C7D1D-2C); delay 0 refreshes in the same tick; a refresh (0x5C7AC0(0)) re-reads the attribute and notifies, clearing dirty. So **delay is a minimum time between refreshes of a changed attribute (a throttle on the re-read), in the unit of the tick's dt (seconds, presumably) [I]; it is not a resend interval and does not delay the first value**. 0 (the AS default when omitted or null) = refresh as soon as dirty.

#### DeliverTokens 0x00519B00 [C]
Pending ids at this+0xA8..0xAC (dword vector), pending values at this+0xB8..0xBC (wstring vector, stride 0x1C; index beyond size reads a static empty string at 0xC1D23C).
```
rounds = 0; sent = 0
n = pending ids count
if n == 0 -> goto clear
do {
  count = n - sent
  build tmp = [ pointer to each pending value's wchar_t buffer (SSO buffer if capacity < 8 else heap), for i in sent..n ]   // no copy, valid until the vector is cleared
  view->SetVariableArray(type 0 (int),  "TokenManager.BatchTokenReplacementCallbackIDs", 0, &ids[sent], count, sticky 1)
  view->SetVariableArray(type 4 (wstr), "TokenManager.BatchTokenReplacementValues",      0, tmp,         count, sticky 1)
  Invoke("TokenManager.PerformBatchTokenReplacements", null, null, 0)      // pushes: 0,0,0,name
  sent = n
  rounds++
  n = current pending count (callbacks may have caused new Notify while running)
} while (rounds <= 10 && n != sent)           // at most 11 rounds per frame per movie
clear: pending ids vector (0x5198D0) and values vector (0x4A68B0)   // ALWAYS, also after the 11th round
```
Consequences: (a) each round sends only the items added since the previous round, indexed from 0 in the AS arrays; (b) the AS clears both arrays after each Perform, so `SetVariableArray` never has leftovers (GFx's SetVariableArray writes elements from `index` and the AS array was emptied, so it behaves as "replace" [I: Scaleform's own code is not in the exe's game code; a host should set the arrays to exactly `count` elements]); (c) items still pending after round 11 are **discarded** (cleared at the end) and not retried until their attribute changes again [I from the code path: exit at 0x519D76 jumps to the clear at 0x519D8F]; (d) IDs are ints, values are strings: AS callbacks get "1", "0.5", names, and do `Number(v)` themselves; (e) the whole batch for a frame is one Perform per round, so callbacks run synchronously inside `Invoke`.

#### Not a movie-to-engine poll
Nothing is polled by timer: the engine reads the movie's `Add/RemoveTokenObserverBatchData` arrays once per Step, and the movie never reads engine state except through `fscommand` / `ExternalInterface`. `GuiExternal::AddTokenObserver` (0x0051CD50) and `GuiExternal::AddTokenObserverWithID` (0x0051CEA0) also exist as ExternalInterface handlers; the movies use the first (id returned as a number) and the batch arrays instead of the second (no AS caller found).

### Port checklist from this part
- Call `StartSceneFromGame()` after the movie's frame 1 has run (the `_root` functions exist) and before 100 ms; never with arguments; call it again only after a `FinishSceneFromGame()`. HUD hide/show: no call, just toggle visibility and stop stepping.
- Answer `GetValue("Scaleform.Platform")` with "PC" and `("Scaleform.IsMac")` with false/0 explicitly.
- `PlayScene("Outro")` (wide string) only for Loading and Saving screens when hiding; then watch `_root.WaitingToBeStopped` each step and close that screen when it is a non-empty string, then clear it.
- Implement the token mailbox exactly: read the whole `AddTokenObserverBatchData` array (triples id/number, token/narrow string, delay/number), empty it, same for `RemoveTokenObserverBatchData` (ints); refresh newly added observers immediately; coalesce per id; deliver ints + strings and `PerformBatchTokenReplacements()` at most 11 rounds a frame, then drop what is left; only to visible movies, and while paused only to movies with record flag 0x10.

### Not read
- Who sends `PlayScene("Outro")` to `conversation`, `slideshow`, `startmenu2`, `newgame`, `load_light` and `load_fade` (their scene `b` has no bit 4); all other callers of GUISystem slot 81 (0x144) were not enumerated (the `[reg+0x144]` scan is dominated by unrelated classes; only 0xA55945 is a GUI scene).
- The value `GuiExternal::GetValue` returns for an unknown path was read at the code level (a type-3 value built from a zero), not seen in play; the effect of `Platform` = 0 on `InConsoleMode()` was not traced.
- Whether the first value of a newly added observer is delivered in the same frame was inferred from the refresh call, not traced to the notify (0x5C7AC0's tail).
- The unit of the observer delay and the exact accumulator reset at 0x5C7D2C; the tick's caller (which dt it passes) was not located.
- Scaleform's own `SetVariableArray` semantics for leftover elements (library code, not read); behaviour inferred from the AS clearing the arrays.
- `GuiMovie` fields +0x30/+0x34 (visibility/flags checked by 0x518BD0/0x518970) and the stage set-up calls 0x510850 / 0x518860 were only skimmed.

## gui2c: mouse, img://, viewport scale, ImportAssets2 ids (lib-agent-2, read 2026-10-18)

Marks: [C] read in the exe or the files, [I] inferred. Addresses are GOG's. AS paths are under `%TEMP%\lib-agent-2-guicmd-x\allgfx\`.

### Short answers

5. **Mouse.** The engine sends GFx four kinds of event through the view's `HandleEvent` (vtable +0x98): move (type 1), down (2), up (3), wheel (4). Buttons are left 0, right 1, middle 2. A double click is a *second down event with button + 4* (4, 5, 6), and the matching up is sent as button + 4 and then again as the plain button. Wheel is ±3.0 per notch, sent to the front movie only. The movies read exactly this: `GUI.Element` (`button == Mouse.LEFT + 4` = double click). `Mouse.getTopMostEntity` is needed: 71 movies use it (Element.as's cursor and right/middle press logic in every movie; drag-drop targets; pickers; radial menu). A port must define `Mouse.LEFT/RIGHT/MIDDLE = 0/1/2`, `Mouse.getTopMostEntity`, listeners' `onMouseDown/Up(button, target)`, `onMouseWheel(delta)`, and a double-click (button + 4) event, or the whole `SetupMouseListener` is skipped.
6. **img:// Async=1.** It asks the texture loader to *queue* the load: the loader (0x00845D80) makes an empty texture object at once (0x6C bytes, "not loaded" flag at +8 = 0) and hands a job to a loader object; the caller gets the object back immediately, so the image has its size from the URL's `?Width/&Height` (the reason those exist) and fills in later. Not found: the loader tries the name with extensions in a list, then falls back to the texture named `default_missing` (exists as `default_missing.dds` in `textures.erf`); never a null image or 1x1.
7. **Viewport scale below 1024x768.** `[ecx+0x184]` is not a GUI-scale option or DPI. The object is the D3D graphics driver (`g_graphics` +0xBC, or +0xB4 off the render thread), slot 97 returns a float at driver +0x7368: the **display pixel aspect ratio** (current desktop mode's w/h against the back buffer's, from `EnumDisplaySettingsW`, 1.0 unless fullscreen with a mode the monitor stretches). It goes in the viewport's **AspectRatio**, not Scale. The viewport **Scale** is 1.0, or below 1024x768 `1024/w` when the window is narrower than 4:3, else `768/h` (the reciprocal of the earlier note's w/1024; see the evidence).
8. **ImportAssets2 ids.** No collision exists. Quickbar's id 28 is only the `$BodyFont` import (used as the font of DefineEditText 29 and 68); the quickbar's 33 DefineExternalImage ids are 1, 4, 7, 10, 20, 24, 33 ... 166, with no 28. I checked all 56 movies: no imported id equals an id defined in the same movie. The id of an import is the movie's own id for the resource, in the movie's one id space, so a port needs no remap and drops nothing; its own synthesised ids should come from a range no movie uses.

### 5. The mouse, evidence

- Messages [C]. `ScaleformManager` handlers (vftables 0x00AF545C..0x00AF54D4; thunks push (button, double-click flag) and call `0x00512340`): LeftDown 0x512960 (0,0), LeftDblClick 0x512980 (0,1), LeftUp 0x512970 (calls `0x512480`, arg 0), RightDown 0x512990 (1,0), RightDblClick 0x5129B0 (1,1), RightUp 0x5129A0, MiddleDown 0x5129C0 (2,0), MiddleDblClick 0x5129E0 (2,1), MiddleUp 0x5129D0. Mapped by the call sites in the vtable builders 0x512E3A, 0x512EC A, 0x512F5A, 0x512FEA, 0x51307A, 0x51310A, 0x51319A, 0x51322A, 0x5132BA.
- Down `0x00518970(record, button, &pos, dbl)` [C]: builds a GFxMouseEvent on the stack: type 2 (`mov [esp+0x10],2` at 0x5189D8), a byte 0, x and y as floats (`fild`/`fstp` of the integer cursor), scroll delta 0.0, button = `button + 4` if the flag is set, else `button`, mouse index 0; calls `[view->vtable+0x98]` (0x5189F7). It then sets bit (button or button + 4) in the record's pressed mask at +0x34.
- Up `0x00518A50` [C]: if the double-click bit (button + 4) is set in the mask it first sends type 3 with button + 4; then always sends type 3 with the plain button (0x518B4D, button field = ebx). The mask is cleared afterwards (the exact masking is unclear in the listing; [I] it clears the bits).
- Distribution `0x00512340` [C]: only if the manager runs (+0x15) and the cursor is on screen (0x510850 gives -1,-1 when not); sent to the front movie of the list at +0x80, to every movie in the vector at +0x90..+0x98, and to the capture movie at +0xA0.
- Wheel [C]: `0x005125E0` (ScrollUp) and `0x005126F0` (ScrollDown): type 4 (`mov [esp+0x1c],4`), x, y, scroll delta = float at 0x00AE7130 = +3.0 (up) and 0x00AF553C = -3.0 (down); to the front movie only (first of the list at +0x80), cursor must be on screen. Dispatched through the same slot +0x98 (0x512697, 0x5127A7).
- Move `0x00518850` [C]: type 1 with the cursor's x, y; when the cursor is invalid, the movie is not the receiver of the mouse (the modal "blocked" case, see GUI.md) or the record's flag at +0x38 is 1, it sends a move to the float at 0x00AF56E4 = -999.0 (a position off the screen) so rollovers clear.
- The movies' use of indices [C]: `GUI/Element.as` (the same file in every movie; abilities copy: lines 219-241) `fnEventHandler(nEventType, button)`: `Mouse.LEFT` and no argument = left, `Mouse.RIGHT` = "Right...", `Mouse.MIDDLE` = "Middle...", `Mouse.LEFT + 4` / `RIGHT + 4` / `MIDDLE + 4` = the same buttons with a flag (the handler is called with `true` = double click; `_root.IsDoubleClick` is cleared on a Press). Handlers named `onPressHitArea`, `onRightPressHitArea`, `onMiddleReleaseHitArea`, and so on (lines 195-260).
- `SetupMouseListener` (Element.as:272-355) [C]: runs only if `Mouse.LEFT`, `Mouse.RIGHT` and `Mouse.MIDDLE` are all defined, which is GFx's extension (`_global.gfxExtensions = true`). Its `onMouseDown(button, target)`: LEFT only sets `bMousePressed` and the cursor type (the movie clip's own `onPress` handles the left button); any other button does `mouseDownTarget = Mouse.getTopMostEntity(false)` and calls `mouseDownTarget.onPress(button, false)`. `onMouseUp(button)`: same with `onRelease(button,false)` if the same entity is still topmost, else `onReleaseOutside(button,false)`. So in GFx 3.0 right and middle presses do not reach `onPress` on their own; the listener turns them into `onPress(button)` calls. [I] The left-button `onPress` gets no button argument from the engine, and the double click arrives as `button == Mouse.LEFT + 4` through the Mouse listeners (a second `onMouseDown`), though no AS reads it from `onPress` for left; only Element.as's `fnEventHandler` tests `+4`.
- `Mouse.setCursorType(type, force)` / `Mouse.forceCursorUpdate()` are defined by the listener setup itself (Element.as:287-352), not by GFx; they call `Mouse.getTopMostEntity(false)` and `entity.GetRolloverCursor()` / `GetRolloverPressedCursor()` then `ExternalCommands.SetCursor("standard" | "standard_pressed" | "ibeam" | "ibeam_pressed")`. The port only needs `Mouse.HAND/ARROW/IBEAM` constants (they exist in Flash 8) and the cursor names fscommand.
- `onMouseWheel` [C]: 22 hits; e.g. `Behaviour/Scrollable.as:85`: `onMouseWheel(a_nDelta)` scrolls `YPosition.ChangeValueBy(-a_nDelta * 20)` if `this.hitTest(_root._xmouse,_root._ymouse)`. Registered with `Mouse.addListener(this)` (Scrollable.as:29, 8 movies' copies). With the engine's +3 up, the content moves by -60 px of y per notch up (up = scroll toward the top). Other listeners: `Map.as:78` (map zoom), `Tooltips.as:212` (`MouseUpListener` while dragging), `Credits.as:57-65` and `Slideshow.as:66` (`onMouseDown(button)` to skip), `ItemExamine.as:25` (`_root`), `RadialMenu.as:159`, `Debug.as:321`.

#### `Mouse.getTopMostEntity`: uses (file:line, what is done with the result)

All [C], argument false unless noted.
- `GUI/Element.as:290/304/326` (every movie): the entity under the mouse on a right/middle press and release; compared across down and up to choose `onRelease` against `onReleaseOutside`; and for the cursor (`GetRolloverCursor`).
- `SharedLibrary/Picker.as:108` (inventory, chargen, community features): in `onMouseUp`, closes the picker unless the entity is the picker's button or its `HitArea_mc`.
- `SharedLibrary/RadialMenu.as:119` (inventory, itemexamine, itemupgrade): in `onMouseUp`, finds which of the radial menu's `NUM_RADIAL_BUTTONS` buttons the entity (or its `_parent`) is, and runs its `ExecuteAction()`; none = the cancel button.
- Drag and drop (no argument, so the default): `Inventory.as:115/127` (`IconDestroyDropTarget_mc.IsDropAcceptable` / `IsExternalDropAcceptable` return `Mouse.getTopMostEntity() == this`), `AbilityScene.as:188`, `QuickbarScene.as:29`, `Store.as:166/178`. So a drop target accepts only if it is the topmost thing under the cursor, with the drag impostor excluded because it has `hitTestDisable = true` (`Behaviour/DragAndDropable.as:329`). The other drop targets use `hitTest` (`DropTarget.as:40-107`, `DragAndDropable.as:152`).
- Behaviour in GFx [I from memory of the Scaleform docs, not verified here]: the call returns the topmost clip, button or text field under the cursor; the argument says whether to count entities with no mouse handler (`false` = only those with handlers). A port can implement it as a hit test over the display list in reverse depth order, skipping clips with `hitTestDisable`, and, for `false`, requiring a mouse handler on the clip or an ancestor; when nothing is hit the AS gets `undefined` (the code tests `.onPress`, which is safe on undefined).

### 6. `img://` loader, evidence

Function map [C], `ScaleformManager::LoadImage` (0x00516A20):
1. Strip `img://` or `imgps://` (compares with the 6/8-char strings at 0xAF5620 / 0xAF5614).
2. Parse `?Width=` (0xAF5604), `&Height=` (0xAF55F0), `?Async=` (0xAF55E0) from the end of the name: width in [esp+0x24], height in [esp+0x20], async = (value == 1) in a byte at [esp+0x1c] (0x516C2F-0x516C35); cut them off the name.
3. `0x523580(this+0xD0, name)` looks the name up in a table of the manager's (an image registered in advance, with its own two scale floats at [esp+0x1C4]/[esp+0x1C8]); if found, take the texture at once with `0x845D80(name, 1, 0)` (async = 0), scale its width/height by those floats (0x516CD8-0x516D07), the URL's Width/Height override if > 0 (0x516D0E-0x516D22). [I] This is the table of images the game or a plugin registered under a name (live portrait renders).
4. Not found: the `FILE:` test. The wide `FILE:` string at 0xAF55D4; `find` (0x4C0F70) at index 0 gives the file branch: strip the prefix, `0x846990` (the file loader); otherwise `0x845D80(name, 1, async)` (0x517174: `push async; push 1; push name; push ret`; 4 arguments, `ret 0x10`). (This reverses the order of the older note in GUI.md, which gave 845D80 to FILE: and 846990 to names; the pushes at 0x517119-0x517126 and 0x51718C-0x517193 are what I read.) A name ending "jpg" / "png" (0xAF04B4 / 0xAF04A4) sets a byte [esp+0x1b] that makes it call `0x844FB0(width, height)` first (a size hint for decoded images).
5. Then the size: URL width/height if given (> 0), else the texture's virtual +0x10 / +0x14; wraps in the image info (alloc 0x28 bytes, GUID words 0x56471E89 / 0x9FE1234A at +4/+8, ctor 0x90D340(texture, w, h)).

`0x00845D80(ret, name, sync, async)` [C]:
- If `sync` (arg 3) is 0 it is forced to 1 when `g_graphics+0xC8` byte is 0 or the byte at 0xC39D00 is 0 (0x845DE2-0x845E00).
- Strips the extension after the last "." (0x845E15 `find` of 0x2E), looks the name up in the cache (0x845600; two caches at 0xC39D30 keyed by thread), and returns the cached texture. [I] "cache per thread" from the two 0x1C-byte records and `GetCurrentThreadId` at 0x845EBD.
- Otherwise it makes a texture object (0x6C bytes, ctor 0x53C1D0) and loops over four extension strings (0xC39E30, 0xC39E4C, 0xC39E68, 0xC39E84; one is ".jpg", set from 0xB067D0 at 0xAD3A00) searching the resource type 2 (textures) with `0x4D5A50` (0x846122-0x84619F). First hit goes to 0x84635F.
- **Async** (0x84635F: `cmp byte [ebp+0x14],0`): if true and the conditions hold (current thread is the main thread [0xC2B590], the renderer vtable +0xF4 says ready, record flag +0xA8 and byte 0xC39D00 are set, the texture has not been checked in already (0x4D5630)) it calls `0x84ACC0(…, sync, …)` on the object at +0xA4: that makes a texture object, flag +8 = 0, a record with the name (adds ".dds" if no extension, 0xAEC570), and hands it to the object at `[0xC2B588]` (0x4BBE00 getter, 0x4BE610 call) = a queued job, then returns the empty texture. With async 0 or the conditions unmet: `0x846B90(…, sync, 0)`, the synchronous load. [I] The queue is processed on another thread (no thread named here); the caller does not wait.
- **Not found** (0x8461AE-0x8461FB): `0x4C0E50` compares the name with the global at 0xC39DA0 (initialised to the wide string "default_missing" at 0xB2B2CC by the static init at 0xAD3A20, and `0x4C0E50` returns 1 when different); if it differs, `0x845D80` is called again with that name (same `sync`, async 0) and the result replaces the answer. So a missing image becomes the game's `default_missing.dds` (core, `textures.erf`, 5588 bytes; its `.mat.xml` is in `materials.erf`), a real picture, not a null or 1x1. If `default_missing` itself is missing the result stays empty [I].
- Also `MovieUtils.GetImageUrl` (AS) already sends `Default_Missing` for an empty name or a "?" (MovieUtils.as:3, 157, 163), so the same picture is shown by either path.

### 7. The viewport scale, evidence

`0x005111B0` (called with the view in [esp+0x5c]; the loop at 0x511D95 calls it per movie) [C]:
- `GViewport` laid out on the stack at E+0x28 (E = esp after the two pops at 0x5112E6): +0 BufferWidth = client width (edi), +4 BufferHeight (ebp), +8 Left 0, +0xC Top 0, +0x10 Width, +0x14 Height, +0x18..+0x24 scissor 0 (or the record's rect at +0x28..+0x34 if the flag +0x24 is set), +0x28 Scale (float), +0x2C AspectRatio (float), +0x30 Flags 0. [I] the order Scale, AspectRatio, Flags is GFx 3.0's; the code's writes fit it: the +0x28 slot gets `[esp+8]` at 0x511318, the +0x2C slot gets the vtable result at 0x51132A.
- Scale: starts 1.0. Only if `w < 1024` or `h < 768` (0x511252-0x511260): aspect = w/h; if aspect < 4/3 (constant 1.3333334 at 0xBF6154) Scale = 1024/w (0xAEBBF8 = 1024.0); else Scale = 768/h (0xAF5438 = 768.0). I traced the x87 stack through 0x511262-0x5112AB: the final `fdivr` is constant/value, so the Scale is >= 1 and is the amount by which the window falls short of 1024x768. (GUI.md's "w/1024 or h/768" has this the wrong way round.) What GFx does with Scale (shrinks `Stage.width` to w/Scale or enlarges the picture) was not read; [I] it keeps a small window's UI at 1024x768 proportions.
- AspectRatio: `call 0x402ca0` (returns `g_eclipseGame`, 0xC1D0A0) non-null and its byte +0x259 == 0 → `0x5278C0()` returns the graphics driver (`[g_graphics+0xBC]` on the main thread of 0xC2B590, `+0xB4` otherwise), then `call [driver->vtable+0x184]` returns a float in st0 which is stored at +0x2C. Otherwise 1.0. Slot 0x184/4 = 97 in `D3DDriver::D3DGraphicsDriver`'s vftable (0x00B2E174) is 0x0086B530 = `fld [ecx+0x7368]` (the base `Graphics::GraphicsDriver`'s is `fld1`).
- The driver's +0x7368 is set to 1.0 in the constructor (0x86BB2C-0x86BB31) and recomputed at 0x86A540-0x86A6F3 [C]: only if a game object exists with +0x30 == 0 and the flag in cl false (windowed state not set); it calls `EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS (-2), &devmode)` (import slot 0xADA480; `dmSize` 0xDC), requires dmFields to carry DM_PELSWIDTH (0x80000) and DM_PELSHEIGHT (0x100000), picks the orientation (DM_DISPLAYORIENTATION 0x80; the two ways of dividing at 0x86A5B0-0x86A661 swap w and h for 90/270 degrees), and computes a ratio from the desktop mode's w/h and the back buffer's w/h (`[esi+0x12c]` +0x10/+0x14); if `|ratio - 1| < epsilon` (double at 0xAF70B0) it stores 1.0, else the ratio. [I] So it is the pixel aspect of a fullscreen mode the monitor stretches (a 4:3 mode on a 16:9 panel), so the UI can be drawn undistorted. Not an option from the options screen or an ini value, not the DPI.
- Port: scale 1.0 and aspect 1.0 at 1024x768 and above (the case that matters); below that, no movie is positioned using Scale unless Ruffle's own scaler is told; leave it out unless small windows are a goal.

### 8. ImportAssets2 ids, evidence

- quickbar.gfx [C] (532 tags, `CFX` version 8): ImportAssets2 imports from `gfxfontlib.swf` `$BodyFont` as id 28 and `$TitleFont` as id 153. DefineEditText 29 and 68 use font id 28; 154 uses font 153 (read from the tag: HasFont flag then the id). The 33 DefineExternalImage (1001) ids are 1, 4, 7, 10, 20, 24, 33, 36, 40, 43, 47, 50, 54, 62, 73, 80, 82, 86, 105, 110, 114, 116, 125, 128, 136, 139, 142, 145, 149, 156, 159, 163, 166 (file names `abilities_IE.dds`, `Quickbar_I3E.dds`, `inventory_I8D.dds`, ...). There is no 1001 with id 28 in quickbar, and 28 = 0x1C is not in the `_I<hex>` file names either. Only one other duplicate id: a pair of DefineFont tags with the id 2048 (two tags, both size 22, probably text-field substitution fonts).
- All 56 `.swf` copies (the movies, `%TEMP%\lib-agent-2-guicmd-x\allgfx\*.swf`): `lib-agent-2-gui2c-coll.py` lists every ImportAssets2 (id, name) and every top-level tag that defines an id (shapes, sprites, fonts, edit texts, buttons, morph shapes, 1001/1009/1003/1005); **no import id is defined in the same movie**, in any of them. Imports are one or two per movie, `$TitleFont`/`$BodyFont` only.
- GFx 3.0 behaviour [C strings in the exe]: the loader logs `"  import: id = %d, name = %s"` (0xB4893C) under `importAssets2: SourceUrl = %s, count = %d` (0xB4895C) and `importAssets` (0xB4898C), `Import error: GFxResource '%s' is not exported from movie '%s'` (0xB49068), `Error: Recursive import detected` (0xB489E8), and `Imports: "%s" %s import delegates to font library.` (0xB4F98C, `GFxFontLib`, "GFxFontLib not installed" 0xB4FB1C). So an import binds the *movie's own* id to the named export of the source movie, and the import is delegated to the font library by font name when the library is present. [I] Standard SWF semantics: the ids stay in the movie's single id space (a collision would be an exporter fault), the imported font is a resource reached through the movie's id.
- `Warning: static text uses imported font! FontId = %d, import name = %s` (0xB4BED0) [C string]: static text (DefineText) cannot use an imported font in GFx; the movies use DefineEditText only (the quickbar has none of tag 11) [C for quickbar].
- What a port should do: (1) keep the movie's own import ids; load `gfxfontlib.gfx` (the file with its first bytes changed to FWS/CWS, which exports `$TitleFont` as DefineFont3 id 1 and `$BodyFont` as id 2) as the Ruffle import source, or register those two fonts under the names; Ruffle then binds `$BodyFont` to the local id 28. (2) A collision seen in the port means the port's own id allocation (for example its ids for 1001 images, which Ruffle does not know) overlapped a movie's: allocate synthesised ids from above the movie's highest used id (or from 0xF000 up; movies here stay below a few thousand, quickbar's top id [I] < 1000). (3) Per language the fonts come from `fonts_<lang>.gfx` instead of the lib's glyphs (GUI.md's note), so the host chooses the file.

### Not read

- The exact masking of the pressed-button mask at 0x518B24 / 0x518BAC (a `sete`/`and` pair that does not obviously clear one bit).
- Whether the left button's `onPress` receives a button argument from the engine (GFx 3.0 extension behaviour); only the AS side and the event structs were read.
- What the queued Async job's worker thread is; the code of 0x4BE610 and the job object at [0xC2B588] was not followed; the placeholder texture's width/height before loading.
- The not-found result of 0x846990 (the `FILE:` loader), and what `LoadImage` does when `default_missing` is itself absent.
- What GFx does with `GViewport::Scale` (field order assumed from 3.0's header from memory), and which state flag in the game object gates the pixel-aspect computation at 0x86A540; the ratio's exact operands.
- The `0x523580` table (what registers names there) and its scale floats.
- GFx 3.0's `Mouse.getTopMostEntity` semantics: from memory, not verified.

## The data-tree paths the movies read (lib-agent-2, read 2026-10-18)

The ActionScript of the 56 non-font movies was scanned for the data-tree paths it reads (observed with a token observer, or pulled with `GetValue`, `GetNumber`, `GetString`, `GetBool`, `GetNumMembers`, `ListMemberNames`) and writes (`SetValue`, `SetValueUnicode`). The inventory is [gui_data_paths.csv](gui_data_paths.csv): one row per movie, path and way of reading, with the file and line and whether the call site is in a framework file shared by 5 or more movies (3,355 rows: 2,294 reads and 1,061 writes). The paths are templates, a creature token written `<creature>.Attributes.7.Current`.

- 765 distinct read templates: 533 absolute (they start at a root node) and 232 relative to a token. 170 are observed, the rest pulled. 125 more templates are written.
- 48 roots among the absolute ones. The largest by templates: `GUI` 85, `Party` 56, `ContentManager` 52, `Journal` 32, `GUICommunityFeatures` 31, `GUIPopupLayer` 23, `WorldMap` 23, `GUILogin` 20, `CharGenCreature` 15, `ClientOptions` 14, `GUIJournal` 13, `SaveLoad` 12, `GUITooltips` 12, `InputMappings` 11, `Area` 10, then 33 small roots of 1 to 8 each.
- Four templates are read by all 56 movies (`Scaleform.Platform`, `Scaleform.IsMac`, `DebugOptions.EnableController`, `DebugOptions.EnableDebugHelpers`, from the shared `__Packages` classes); only 40 occur solely in shared framework files.
- The relative templates are 8 node shapes (creature, item, ability, list item, map pin, map, offer, and an untyped token) whose fields repeat across screens.
- Heaviest screens by distinct read templates: chargen 162, inventory 124, quickbar 112, levelup 111, journal 97, characterrecord 96, store 89, saveload 84. The heaviest observers are portraits, inventory, quickbar and characterrecord.
- A path is resolved by `SmartData::TokenManager::Get()` and `Find(path)`; a missing path gives the shared null attribute, answered as a null string or 0.

## gui3a: the stage at creation, and ResolutionWidth (lib-agent-2, read 2026-10-18)

### Short answers

1. **_root.OriginalStageWidth/Height are the movie's own header frame size, not the window.** `GuiMovie::GuiMovie` 0x0051A040 sets them with `SetVariable` to `ftol(GFxMovieDef::GetWidth())` and `ftol(GetHeight())`, passed in by `OpenMovie` 0x00514B10 (checked in the disassembly; that the two calls are the movie def's GetWidth/GetHeight is inferred from slots +0x14/+0x18). Every HUD movie's header frame is 1024x768 (checked: quickbar, portraits, minimap, navbar, floatylayer, notifications, popuplayer, tooltips, tactics, inventory, mainmenu all 1024.0 x 768.0, read from the .gfx RECT).
   - The ActionScript does NOT record it in the game. `GUISystem.InitMovie` (GUISystem.as:~93) only installs `_root.Rewind` and `setInterval(ExternalInitCheck, 100)`. The only AS that writes `_root.OriginalStageWidth/Height` is `GUISystem.StartSceneFromFlash` (GUISystem.as:103-110, `= Stage.width/height`), reached only from `ExternalInitCheck` 100 ms later and only if `GUISystem.RunningInGame == false`. In the engine `Rewind` runs `_root.StartSceneFromGame()` straight after creation, which sets `RunningInGame = true` (GUISystem.as:117), so the Flash-side path never fires.
   - So the peer's assumption is **corrected**. "Viewport first equals the movie's size, so InitMovie records 1024x768" is not the mechanism: InitMovie records nothing. If the port does not set the two variables itself and does not call StartSceneFromGame before the 100 ms interval, `StartSceneFromFlash` records `Stage.width/height` at that moment, which is the window size if the window is already applied. That is wrong for vanilla's layout (see below). Safe contract: the host sets `_root.OriginalStageWidth = 1024` and `_root.OriginalStageHeight = 768` (the movie def's header size) before calling `StartSceneFromGame`, and has the window size in place as the stage by then. If the port sees a correct layout today, the likely reason is that its viewport was still 1024x768 at the 100 ms tick, or it sets them already.
   - **Order in vanilla** (checked): `CreateMovieView` 0x00514940: `GetMovieDef` (flags 0x8006), `CreateInstance(0)`, a call at +0x84 with float 0.0 (Advance(0), name by GFx header, inferred), SetViewAlignment(0) (+0x60), then `0x005111B0` (the resize routine: SetViewScaleMode(0)=NoScale at +0x58, SetViewport at +0x50 over the client area). Then in `OpenMovie` the `GuiMovie` ctor: `_root.Filename`, `_root.OriginalStageWidth`, `_root.OriginalStageHeight` by SetVariable (strings 0x00AF57F0, 0x00AF57D4, 0x00AF57B8), then `Rewind` 0x00518E00 which invokes `StartSceneFromGame`. So the first Advance(0) runs with the view's default viewport (the movie's own size, inferred), and NoScale + the window-sized viewport are set before the two SetVariable calls. The variables are written once, at creation, after the viewport; they are not rewritten on a resize (`UpdateAllLayouts` is invoked instead).
   - What the constants do (AS, checked): `SceneManager.SetSafeZone` (SceneManager.as:144-153): `sm_rScreenRect.x = (OriginalStageWidth - Stage.width)/2` (negative on a wide window: the movie's 1024-wide frame is centred in the viewport, so screen left is at x = (1024 - W)/2 in movie coordinates), `sm_rSafeZone.width = Stage.width * 0.9 (or 1)`, `.x = (OriginalStageWidth - safe.width)/2`. `AlignedLayout.as:33-62` uses `OriginalStageWidth/2 - x`, `DisplayTooltip.as:101` uses `(OriginalStageWidth - 1024)/2`, `ScaleToScreen.as:23-24,58,94` use width/OriginalStageWidth ratios. With OriginalStage = window size these would sit off by (W-1024)/2 and the tooltip correction would be 0; with 1024x768 they match the centred noScale coordinate system. In Ruffle that needs `scaleMode = noScale`, align centre (`""`), `Stage.width/height` = window size.

2. **ClientOptions.VideoOptions.ResolutionWidth is the configured width from DragonAge.ini, not the live render target.** It is an integer member of `CClientOptions` (+0x420; ResolutionHeight at +0x430) registered under the name in the constructor with a default of 1024 (0x400) and 768 (0x300). The data-tree path is `ClientOptions.VideoOptions.ResolutionWidth`; the ini key is `[VideoOptions] ResolutionWidth=`, loaded generically by the options loader (the ini on this machine has `ResolutionWidth=1920`, `ResolutionHeight=1200`, `FullScreen=0`, checked). The options screen writes it via `SetClientOption` when the player chooses a resolution (inferred: no other code names the attribute). The engine never refreshes it from the back buffer or a window resize (inferred: the strings have only two code users, below). So windowed at a different client size, it holds the configured size, not the actual one.
   - **Quickbar use** (checked, Quickbar.as:76-79, 263-267): the observer calls `ExpandQuickbarFromGameData(width)`, which sets `ExpansionHandle_mc._x = Math.min(width + m_aValidHandlePositions[MIN_SLOT_COUNT], GetSnapPosition(m_MaxDragWidth))`. Positions are `20 + 47*n` for n = 0..50 (MAX_QUICK_SLOTS = 50), MIN_SLOT_COUNT = 14, so `positions[14]` = 678. `m_MaxDragWidth = sm_rSafeZone.width - handle._width - (qbar global x - safeZone.x)` (Quickbar.as:`UpdateMaxDragWidth`). The value is just a large number: `width + 678` exceeds the max drag width for any width at or above 1024 (at 1024 the safe zone is 0.9 x 1024 = 921, MaxDragWidth is far below 1702), so the bar expands to the widest slot position that fits. There is no threshold: it is "clamp to full width". The same `ExpandQuickbarFromGameData` also takes `Party.QuickbarExpandValue` (the saved drag position; observer at line 244 and `UpdateLayout`), whichever token is delivered last wins.
   - **So 1920 -> full width is exactly what vanilla shows** (any real resolution does). The number's size is irrelevant beyond being >= about the drag range, and what the full width is depends on `Stage.width` (the safe zone), i.e. on the window size, not on this value. If the port delivered a value near 0 or the player had dragged the handle in (Party.QuickbarExpandValue), the bar would be narrower.
   - **Corrected 2026-10-18 by lib-agent-2 (gui4a below):** the `Party.QuickbarExpandValue` observer is registered after `ResolutionWidth`'s and feeds the same `ExpandQuickbarFromGameData`, so its value is delivered last and wins. In vanilla it is 0 (CCParty+0xB4, default 0.0): the bar shows 14 slots, not the full width, and `UpdateLayout` applies it again after a resize.

### Evidence

Checked in the exe (GOG 1.05):
- `GuiMovie::GuiMovie` 0x0051A040: `fild [esp+0x60]` -> SetVariable(0xAF57D4 `_root.OriginalStageWidth`, type 3 number, sticky 1) at 0x0051A268-0x0051A28D; `fild [esp+0x64]` -> SetVariable(0xAF57B8 `_root.OriginalStageHeight`) at 0x0051A28F-0x0051A2AF; before them `_root.Filename` (0xAF57F0) from arg1. Args 4 and 5 are the width and height; the ctor is `ret 0x18`.
- `OpenMovie` 0x00514B10: at 0x00514B9C-0x00514BBB two virtual calls on the movie def, `[vtbl+0x18]` then `[vtbl+0x14]`, each followed by `0x008E34F0` (the CRT float-to-int, `ftol`), pushed as the ctor's height then width; ctor called at 0x00514BE7; then 0x005145C0, `SetVisible` 0x00518CB0, `Rewind` 0x00518E00 at 0x00514C38.
- `CreateMovieView` 0x00514940: viewport routine `0x005111B0` called at 0x00514AB3 after the +0x84 call (0x00514AA1) and the +0x60 call (0x00514AAA); 0x005111B0 calls +0x58 (push 0) at 0x00511330 and +0x50 at 0x0051133E, width/height from `[esp+0x28..]` = the client size (also in dev/GUI.md).
- `ResolutionWidth` (0x00B01040) and `ResolutionHeight` (0x00B0102C) have two code users each: `CClientOptions` constructor `0x005E7DD0` (vtable 0x00B011C0 stored at 0x005E7E27; default `mov [eax], 0x400` at 0x005EB483 and `0x300` at 0x005EB540, registered through 0x004564F0 at 0x005EB4C7) and `0x005E1370` (compares attribute pairs of two option sets, per name, in order: the dirty check of the options screen).
- No other SetViewport/size source for these attributes was found by string reference.

Checked in the files:
- Frame sizes: all 1024.0 x 768.0 (script `%TEMP%\lib-agent-2-gui3a-fs.py` over `allgfx\*.gfx`).
- AS: GUISystem.as:93-140, SceneManager.as:144-153, Behaviour/AlignedLayout.as:25-62, DisplayTooltip.as:101-102, ScaleToScreen.as:23-24,43-44,58,94-95, Quickbar.as:35-38,76-79,173-190,244-267,288-290,348 (all in `%TEMP%\lib-agent-2-guicmd-x\allgfx\as_quickbar\scripts\__Packages` and the sibling `as_*` folders; the same GUISystem/SceneManager in all movies).
- `Documents\BioWare\Dragon Age\Settings\DragonAge.ini` `[VideoOptions]`: 1920x1200 windowed.

### Not read
- Whether `+0x84` in CreateMovieView is Advance (named by the GFx 3.0 header only), and whether GFx's default viewport before SetViewport is the movie's own size.
- The options loader that fills `+0x420`/`+0x430` from the ini (the generic reader was not followed) and the SetClientOption path that updates them when the options screen applies a mode; whether a window resize writes them back (nothing found that does).
- What the movie def's slots `+0x14/+0x18` are by name (inferred as GetWidth/GetHeight from the values: movie header size, which the files confirm is 1024x768).
- What the peer's port actually sets and when. (Which of `Party.QuickbarExpandValue` and the resolution token wins: the first, gui4a below.)

## gui3b: token observers on missing paths, and Mouse.getTopMostEntity (lib-agent-2, read 2026-10-18)

Read in the GOG exe (checked = disassembled and followed, inferred = reasoned from it). Nothing was run and nothing was written to a repo.

### Short answers

1. **Observers.** A new observer is refreshed at once and ALWAYS notifies, so there is always a first delivery for its id in the next DeliverTokens. For a path the tree does not have, the value is the EMPTY STRING "" (checked, `0x005C7F7C`: the "unresolved" branch assigns the static string at `0x00ADEF14`, which is two zero bytes, and notifies). No "null", no skipped delivery. The peer's "" is the same as vanilla. What vanilla's movies use to tell "nobody there" is NOT the observer value but `ExternalInterface GetValue`'s null (see 1c). A path missing at registration is re-resolved when an ancestor it was registered on reports a change (mechanism checked; that array/parent attributes send that report when a child appears is inferred). A removed attribute delivers "" the same way, after the re-resolve (inferred from the same code).
2. **getTopMostEntity.** It is GFx's `GFxMovieRoot::GetTopMostEntity(pt, testAll, ignore)` (`0x0094E6D0`) over `GFxSprite::GetTopMostMouseEntity` (`0x0096B610`). Arguments: no argument = `testAll` TRUE; one boolean = `testAll`; (x, y[, testAll]) is a point instead of the mouse. With testAll true it returns the enclosing clip of the topmost visible shape under the point, whatever its handlers. With testAll false it returns only a clip that has mouse event handlers (button-like), and a handler clip captures hits on its own descendants. So the peer's "topmost clip under the mouse, ignoring hitTestDisable" is right only for the no-argument / true case, and wrong for the `false` that the HUD uses everywhere (the movies' mouse listeners and cursor code). Invisible clips are skipped in both cases; a flag-15 clip (probably hitTestDisable) is skipped whole in both.

### 1. Observers

#### 1a. Construction and first value (checked)

- `GuiMovie::ProcessAddTokenObserverBatch` 0x0051A8B0 -> `RegisterObserver` 0x0051A4A0 -> `ScaleformTokenObserver` ctor 0x00524A80 (vtable 0x00AF689C; stores movie at +0x60, token path string at +0x64, a second string at +0x80, the id at +0x9C). Its base ctors 0x005C7E80 -> 0x005C7A20 (vtable 0x00AFFD70 then 0x00AFFDA0) set: path +0x14, delay float +0x3C, accumulator +0x40, +0x30 = 0 (resolved), +0x31 = 0 (dirty), +0x34 = 0 (bound attribute), **+0x32 = 1 (plain attribute observer; the other mode, +0x32 = 0, watches a text with embedded tokens)**, and puts itself in a global list (0x00C32628) with global flag 0x00BF9B51 set. The EI route (AddTokenObserver 0x0051A3C0) ends in the same 0x0051A4A0 / ctor.
- RegisterObserver then calls the refresh `0x005C7CE0` -> `0x005C7AC0(0)`. In the +0x32 mode it: splits the path on '.' (0x005C80F0), asks the TokenManager singleton (0x00455BF0 gives the singleton at 0x00C325B0) for the attribute with `0x005C6C80` (a find that also fills a list of the attributes met along the path), stores the result at +0x34, walks the list and registers the observer on each attribute met (`0x004E9730`, attribute vtable +0x48, then the subject's AddObserver) **stopping at the first slot that is null or the NullAttribute (0x00BF99E0)**; +0x30 = 1 only if nothing was missing. It then ALWAYS calls its own vtable slot 3 (+0x0C) = 0x005C8050.
- Slot 3 -> `0x005C7F50` (plain mode): if +0x34 is null or the NullAttribute, the value string at +0x44 is assigned from `0x00ADEF14` (empty); else it is the attribute's vtable slot +0x2C "get as string". Either way it then calls slot 4 (+0x10) = `0x00524BA0` -> `GuiMovie::0x00519AA0(id = +0x9C, value)` which adds (or replaces) the id and wide-string value in the pending vectors that DeliverTokens reads. `0x00524BA0` does nothing only if the observer's movie pointer (+0x60) is null.
- So: every path, present or not, gets one delivery in the same frame's DeliverTokens (Step runs the add batch before 0x00511CA0 delivers). A missing path delivers "", present ones the attribute's string.

#### 1b. Late appearance and removal (mechanism checked; the trigger inferred)

- Slot 0 `0x005C7550` (called with the changed attribute): sets dirty +0x31; in plain mode, if the reporting attribute differs from the bound one (+0x34) it clears +0x34 and +0x30, so the next tick (0x005C7CF0; delay is a throttle) does a FULL refresh 0x005C7AC0, which re-splits the path, re-finds and re-registers, and re-notifies. If the bound attribute itself reported a change (+0x30 = 1), the tick only calls slot 3 (re-read and notify). Slot 1 `0x005C7580` (the subject going away): unregisters, +0x30 = 0, dirty, global flag set: the next tick re-resolves, so a removed attribute becomes the NullAttribute branch and delivers "".
- A missing leaf is registered on the existing ancestors only (the loop stops at the first missing slot), so it re-binds when an ancestor tells the observer of a change. Whether the engine's array/list attributes (Party.Members) call that when a child is added or removed was NOT traced; the vanilla UI depends on it working (the portraits show a joining member), and PARTY.md notes that the party's list attributes notify observers when they change.

#### 1c. What vanilla's movies do with the missing case (checked, portraits.gfx AS)

- `__Packages/SharedLibrary/Portrait.as:90-98`: `SetCreatureToken` registers an observer on the creature token itself (`PORTRAIT_VISIBILITY_RANGE + LocalIndex`, path `Party.Members.N` or `Party.Summons.N`). The callback ignores the delivered value and asks `ExternalCommands.GetValue(token) == null`: null -> `MovieUtils.HideMovie`, else `ShowMovie`. So the rings hide by **GetValue's null** (EI GetValue of a missing path answers a null string, GUI.md's table, checked at 0x0051EC60), not by the observer value. The observer only has to FIRE (for any value, including "") when the slot's state changes.
- `__Packages/Portraits.as:73-91, 258-277`: observers on `Party.Members.0..3` call `UpdateVisibleMemberCount`, which counts `GetValue("Party.Members.N") != null`. Same pattern.
- Per-member observers (HP `Attributes.7.Current/Total`, Portrait, PortraitTint) on a missing token get "" in vanilla. Handlers: HP callbacks re-read with GetValue (Portrait.as:108-120, not read in full). `.Portrait` at Portrait.as:137-150: `a_sPortrait != null && length > 0 && last char != "?"` -> LoadImageFixedSize, else `unloadMovie()`; "" takes the unload branch. `PortraitTint` callback does `self.gotoAndStop(a_nTintIndex)` with the delivered string: "" goes to gotoAndStop("") which is a no-op/ignored (inferred; a hidden slot, harmless).
- The "?" suffix is NOT added by the observer path. It comes from the token-text expander (`0x005C7000`: when the lookup finds nothing or the NullAttribute it appends '?' (0x3F) to the token name via 0x004BF350, and the observer's text mode 0x005C7050 uses it). It is a different feature (text with embedded tokens); the AS test for "?" presumably catches a creature whose Portrait attribute itself holds a marker (inferred).
- Conclusion for the peer: delivering "" for a missing path matches vanilla's observer. The bit that matters is that (a) the observer fires for registration and for every membership change and (b) `GetValue` of a missing `Party.Members.N` / `Party.Summons.N` answers null/undefined (not "" and not 0); if GetValue returned "" the slot would show, because `"" == null` is false in AS2 for a string (inferred from AS2 semantics).

### 2. Mouse.getTopMostEntity

#### 2a. Where it is (checked)

- The string "getTopMostEntity" is at 0x00B530F0, referenced once, from the registration at 0x009BFC4A; the native is **0x009BF800** (the Mouse class's method table: a `Mouse` extension, GFx 3.x). Another string, "hitTestDisable", is at 0x00B4E4AC (in a property table at 0x00819B8C; its setter was not followed).
- Argument parsing (0x009BF800): `testAll` byte starts at 1 (TRUE) and the mouse index at 0. One argument of type boolean (arg type byte 2): it is testAll. One argument of another type: it is the mouse index. Two arguments or more: arg0, arg1 are x, y (numbers, each through `0x00920140`, a twips-to-pixels or similar conversion), arg2 if present is testAll. Without x, y it takes the position of mouse N (the movie root's array at +0x8F8, stride 0x30, +0x20/+0x24), only if N is below the mouse count. It calls `GFxMovieRoot::GetTopMostEntity(pt, testAll, 0)` at 0x0094E6D0 and sets the return value to the object (0x00983EA0), or leaves it undefined if nothing is hit.

#### 2b. Walk over levels (checked, 0x0094E6D0)

- It tries the movie's loaded-level array at +0xA80 (count at +0xA84) from the last index to the first, calling each level root's virtual slot 13 (+0x34) = `GetTopMostMouseEntity(pt, testAll, ignore)`; the first non-null wins. If none, it tries the second array at +0x18 (count +0x1C, 8-byte entries, pointer in the second dword) from the last to the first. So the highest level (_levelN) first, then down to _level0 (the order inside the second array was not tied to level numbers).

#### 2c. Per clip (checked structure, 0x0096B610 = GFxSprite's slot 13; flag names inferred)

Return 0 at once when: bit 11 of the word at +0x8C is set (probably "unloaded"); the clip is invisible (slot 3, `+0x8C & 1`) and it has no +0x1C0 link; slot 38 (+0x98, `+0x8C` bit 15, probably **hitTestDisable**) is true; the clip is the `ignore` argument. Then the point goes to local space (matrix at +0x48..+0x5C). Mask (clip-depth) handling: a vector of per-child "inside mask" bytes is built (0x0096B020); a child under a mask is skipped when outside it; children whose flag byte `+0x6A & 2` is set are skipped as well (probably hit-test disabled children or the mask layers themselves). The display list is walked from the TOP depth to the bottom (index count-1 down to 0), each child asked for its own entity with the same testAll.
- **testAll TRUE**: the first child that returns an entity is returned as it is (0x0096B7ED). A static shape child answers with its enclosing sprite (inferred from the pattern; the shape class's slot was not read), so the result is the clip that holds the topmost shape under the point, handlers or not.
- **testAll FALSE**: a child's entity is kept only when it is button-like; if the current clip is itself button-like (`0x00967F80`) then a hit anywhere below returns THIS clip (or its +0x1C0 link), so a clip with onPress captures its children; a non-button clip returns the descendant found. After the children, if the clip has its own drawing (+0x1B4) and is button-like, the drawing is hit-tested (`0x009DBE40`) and the clip returned on a hit.
- "Button-like" is `0x00967F80`: not a mask (the dword at +0x1A8 negative), bit 4 of the flags at +0x8C set (probably mouseEnabled), and then `0x00966E00` over the clip's event-handler chain (nodes of kind 0xB, with the flag bytes at +0x38/+0x39 = "has a mouse event handler"). In AS terms: the clip (or its handler chain) defines one of onPress, onRelease, onRollOver, onRollOut, onDragOver, onDragOut, onReleaseOutside, onMouseDown and the like (which events set those bytes was not read).

#### 2d. Calls in the ActionScript (checked by grep over %TEMP%\lib-agent-2-guicmd-x\allgfx\as_*; every movie)

- `Mouse.getTopMostEntity(false)`: `GUI/Element.as:290, 304, 326` (in all 56 movies; the mouse listener's onMouseDown / onMouseUp, and `Mouse.setCursorType`), and `SharedLibrary/Picker.as:107/108` (dropdown close), `SharedLibrary/RadialMenu.as:119`. At Element.as:290-326 the result is used as a button: `.onPress`, `.onRelease`, `.onReleaseOutside`, `GetRolloverCursor`, `GetRolloverPressedCursor`, compared with the press target. This is the handler-clip lookup, so these all need testAll = FALSE semantics.
- `Mouse.getTopMostEntity()` (no argument = testAll TRUE): `as_quickbar QuickbarScene.as:29`, `as_inventory Inventory.as:115,127`, `as_itemexamine Inventory.as:115,127`, `as_store Store.as:166,178`, `as_abilities AbilityScene.as:188`. In QuickbarScene.as:29 and AbilityScene.as:188 (`IconDestroyDropTarget_mc.IsDropAcceptable`) the code is `return Mouse.getTopMostEntity() == this`: a drop-target clip with no handlers (the destroy slot) is found by its shape. This is why the true case matters.
- `getTopMostEntity(true)` with an explicit true: **not used anywhere**. No call passes a point or a mouse index either.
- Also relevant: `MovieUtils.DisableHitTest` / `EnableHitTest` (used in Portraits.as UpdatePartySelection on SelectAllToggle_mc / HoldPositionToggle_mc) set the clip's hitTestDisable; such a clip is not returned in either mode.

#### 2e. Confirm or correct the peer

- "Topmost clip under the mouse, ignoring hitTestDisable": right for the no-argument form (testAll true; it also skips invisible clips and honours masks and levels from the top). Wrong for `false`, which the HUD's every mouse press, release and cursor change calls: there the answer must be the nearest clip that has a mouse handler (onPress etc., not a mask, mouseEnabled), the outermost one if handler clips nest, never a plain art clip, and `undefined` over bare art (so a cursor over art falls back to the default). If the peer returns the plain topmost clip for `false` as well, `Element.as` will try `.onPress` on a decoration clip (not a handler, harmless) but the real button underneath the decoration is lost whenever art clips lie over it, and the rollover cursors never update.

### Not read

- The sprite virtual that shapes use (what a static shape child returns); the setter of "hitTestDisable" (the identity of flag bits 11/15 and of `+0x6A & 2` is inferred); the mouseEnabled flag at bit 4; the type bytes in the handler chain; how the mask-byte vector (0x0096B020) decides.
- Whether the Party.Members / Party.Summons list attributes notify their observers when a child is added or removed (inferred from vanilla's behaviour and PARTY.md); the conversion in `0x00920140` for the x, y form; the order of the second array at +0x18.
- Portrait.as lines 100-136 and 160-end in full; what `gotoAndStop("")` does in Scaleform's player (Ruffle's differs possibly).

## gui3c: key labels (InputMappings.Mappings.<Action>.PrimaryMappedKey) (lib-agent-2, read 2026-10-18)

Claims are marked **checked** (read in the exe, the game's data tables or the decompiled movies) or **inferred**. Addresses are GOG 1.05.13263 VAs. English text is `core_en-us.tlk`.

### Short answers

1. The HUD movies observe only `PrimaryMappedKey` (never `SecondaryMappedKey`; that one is the options menu's). Actions seen: `ActivateQuickslot1..10` (quickbar), `SwapWeaponSets` (quickbar's EffectBar and inventory), `ToggleFullPartySelect`, `ToggleHoldParty` (portraits), `Toggle<Scene>` for the navbar buttons and portraits' party-picker button, `PerformPrimaryAction` and `PerformSecondaryAction` (tooltips). Checked.
2. The value is plain display text, not an ini name: a talk-table string per key, with modifier prefixes. Vanilla English: slot 1 shows `1`, slot 10 shows `0`, swap weapons shows `/`, full party select shows `=`, hold party shows `H`, main menu shows `Esc`, tactics shows `\`. Table below. Checked.
3. An unbound action gives the empty string `""`. The movies treat `""` and any string ending in `?` as "no key" (`InputUtils.IsValidKey`), and then show the label with no `(key)`. Actions the engine does not have at all (slots 11 to 50, `ToggleOptions`, `TogglePartyPicker`) have no tree path, and their observers are delivered as empty: `""` (gui3b below, checked at `0x005C7F7C`), which `QuickBarIconSlot.UpdateBoundKey` shows as an empty label (corrected 2026-10-18 by lib-agent-2; this said the observers never fire). Checked that those actions have no row in `keymap.gda` and no name string in the exe.

### (a) Paths observed, per movie (checked, AS under `%TEMP%\lib-agent-2-guicmd-x\allgfx\as_*`)

| Movie | Path | Where | Use |
|---|---|---|---|
| quickbar | `InputMappings.Mappings.ActivateQuickslot<N>.PrimaryMappedKey`, N = slot index + 1 | SharedLibrary/QuickBarIconSlot.as:53 (observer id `ABILITY_ICON_SLOT_KEY_RANGE` + index) | `UpdateBoundKey`: text of the slot's `BoundKey_mc` is the value, or `""` if the value ends in `?` |
| quickbar | `...SwapWeaponSets.PrimaryMappedKey` | EffectBar.as:96 | tooltip `LOC_SWAP_WEAPONS + " <b>(" + v + ")</b>"` if valid, else the label alone |
| inventory | `...SwapWeaponSets.PrimaryMappedKey` | WeaponSetToggleButton.as:42 | same tooltip |
| portraits | `...ToggleFullPartySelect.PrimaryMappedKey` | Portraits.as:141 | tooltip `LOC_TOGGLE_FULL_PARTY_SELECT + " <b>(" + v + ")</b>"` (no validity test: an empty value gives `()`) |
| portraits | `...ToggleHoldParty.PrimaryMappedKey` (observer, and `GetValue` at :189 and :193) | Portraits.as:172 | tooltip `LOC_PARTY_MOVEMENT_DISABLED` / `_ENABLED + " <b>(" + GetValue(path) + ")</b>"` (no validity test) |
| navbar | `InputMappings.Mappings.Toggle<SceneName>.PrimaryMappedKey` | NavButton.as:29 (observer id `NAVBAR_BUTTON_RANGE` + SceneID) | `UpdateBoundKey`: `IsValidKey` then tooltip `name + "  <b>(" + v + ") </b>"`, else the name alone |
| navbar, portraits | `Toggle` + `"PartyPicker"` | SharedLibrary/PartyPickerButton.as:27,39 | same; there is no such action, so never fires |
| tooltips | `...PerformPrimaryAction.PrimaryMappedKey`, `...PerformSecondaryAction.PrimaryMappedKey` (observers :170-171 and `GetValue` :126, :130) | Tooltips.as | placeable tooltip lines `key + ": " + action` |
| optionsmenu (not HUD) | `InputMappings.Mappings.<name>.{Description,PrimaryMappedKey,SecondaryMappedKey,RemappingPrimary,RemappingSecondary}` for 50 names, plus `InputMappings.RemappingError` | OptionsMenu.as:315-377, 469 | the Controls tab |

Navbar `SceneName` per button (NavBar.as:35-43): CharacterRecord, SpellsTalents, Skills, Inventory, Journal, Options, Tactics, Map, MainMenu. So the paths are `ToggleCharacterRecord`, `ToggleSpellsTalents`, `ToggleSkills`, `ToggleInventory`, `ToggleJournal`, `ToggleOptions` (no such action), `ToggleTactics`, `ToggleMap`, `ToggleMainMenu`. `ToggleWorldMap` exists as an action but no movie observes it.

The quickbar has up to 50 slots in script (`MAX_QUICK_SLOTS = 50`, Quickbar.as:35) but the engine knows `ActivateQuickslot1..10` only: wide strings `ActivateQuickslot10..1` at 0xB137F0..0xB13958 and `InvokeCallbackMessage Quickbar Quickbar.ActivateQuickslot 0..10` at 0xB116F0..0xB11BA8 (checked). There is no quickslot 11 to 21 binding in vanilla.

### (b) Default bindings (checked)

**Source: the data table `keymap.gda`** (table id 292, label `keymap`, in `packages/core/data/2da.erf`, read through ghilana `table keymap`): 104 rows, `Label` = `<Action>_0` (primary) or `_1` (secondary), the `en-us` column (identical in all nine languages) = the default as an ini key name such as `Keyboard::Button_Slash`, `Control+Keyboard::Button_A`, `Mouse::WheelUp`, empty if none. The user's `Documents\BioWare\Dragon Age\Settings\KeyBindings.ini` (read: it is `Action_0=Keyboard::Button_X` lines under `[Key Bindings]`, the engine's own writer at 0x00895AD0 with `(UNASSIGNED)` for none) agrees with `keymap.gda` row for row on every vanilla action, so the user's file has no remaps for these; it also holds plugin rows (`TurnBased*`, `DAOLibExample*`, `PhotoCam*`) and lacks the debug row `TeleportPlayer--DEBUG_0` = T. The defaults are `keymap.gda`'s; the ini is the player's overrides on top (inferred: that the ini overrides the table; the table's content is checked).

**Not in `keymap.gda`:** `PerformPrimaryAction` and `PerformSecondaryAction`. The engine adds them in code at 0x006E2FD0 (checked): primary = mouse code 0x0D, no modifier; secondary = mouse code 0x0D with modifier flag 0x100000 (Shift). They are not remappable (not among the options menu's 50 names).

### (c) How a binding becomes the string (checked unless marked)

- The tree: `Eclipse::GroupedInputMappings` (vtables 0x00B311E0 and others; its element `GroupedInputs` 0x00B310A0) holds per action the data-tree fields `Description` (wstring at element +0x78), `PrimaryMappedKey` (+0xA0), `SecondaryMappedKey` (+0xC8), `RemappingPrimary` (+0xF0), `RemappingSecondary` (+0x100). Field names are the wide strings `PrimaryMappedKey` 0xB30FF4, `SecondaryMappedKey` 0xB30FCC, `RemappingPrimary` 0xB30FA8, `RemappingError` 0xB31138, `InputMappings` 0xB31108, `Mappings` 0xB31124 (serialiser at 0x00893EC0, field registration 0x008942F0..).
- Setting a binding: `0x00893A40` (slot 0/1, key word) stores the key word at +0x110/+0x114 and sets the +0xA0 (slot 0) or +0xC8 (slot 1) string to `0x00881090(key)`, the display-string function (the setter is a virtual call, slot +0x54, which notifies the observers).
- **The display function 0x00881090** (`key word` = device in the top nibble, key code in the low 16 bits, modifiers as flags 0x100000 Shift, 0x200000 Ctrl, 0x400000 Alt):
  - key word 0 (nothing bound): returns the empty string `""` (0x00ADEF14 is an empty wide string).
  - device 0x10000000 (keyboard): the key's text is looked up in the keyboard table (static map at 0xC3B654, filled at 0x00882680 and 0x008827xx onward) and gives the talk-table string of `inputmap.gda` (table id 267, label `InputMappings`, columns `ID`, `StrRef`; the row is the key's alphabetical index among the keyboard ini names, e.g. row 2 = "1", row 111 = Slash). I checked the code to row link for the keys below by reading the registration blocks (DirectInput code 53 Slash to row 111, 57 Space to 113, 13 Equals to 35, 43 Backslash to 21, 15 Tab to 118, 35 H to 55, 59 F1 to 38, 183 Sysrq to 116, 28 Return to 105, and so on).
  - device 0x20000000 (mouse): table at 0xC3B68C, codes 0x0C Left-click (row 1000), 0x0D Right-click (row 1001), wheel and others in rows 1002-1011; if the mouse-handedness flag (byte at the options object +0x350+0xC, checked as the test; its option name not read) is set, codes 0x0C and 0x0D are swapped before the lookup.
  - device 0x30000000 (axis table 0xC3B6C4, rows 2000/2001) and 0x40000000 (gamepad table 0xC3B6FC, rows 3000..3035): same pattern.
  - a key with no table entry: the string "Unknown Key" (TLK 369761, 0x5A461 via 0xC3B7AC).
  - then modifiers are put in front, Shift first, then Alt, then Ctrl on the outside: so the text is `Ctrl + Alt + Shift + <key>`. Prefix strings: Shift = TLK 369760 `Shift + `, Ctrl = TLK 373286 `Ctrl + `, Alt = TLK 373287 `Alt + ` (0xC3B734, 0xC3B75C, 0xC3B784).
- It is a talk-table text, so it follows the game's language (here en-us); there is no DirectInput name, no ini name (`Keyboard::Button_1`) and no upper-casing in the string. Digits are `1`..`9`, `0` (TLK 369298..369306 and 369297); punctuation is the character itself.
- `0x61727` = TLK 399143 "No key mapped" is used by the engine for the key-remapping dialog (0x00894710 and a fallback object), not for `PrimaryMappedKey`: the string for an unbound action is `""`. Mapping errors during remapping (TLK 387764 and 387765) go to `InputMappings.RemappingError`; not read.

### The table the peer can copy (en-us; checked)

Action, default primary key (`keymap.gda` `_0`), exact `PrimaryMappedKey` string, TLK id.

| Action | Default (`_0`) | PrimaryMappedKey | TLK id |
|---|---|---|---|
| ActivateQuickslot1 | Keyboard::Button_1 | `1` | 369298 |
| ActivateQuickslot2 | Button_2 | `2` | 369299 |
| ActivateQuickslot3 | Button_3 | `3` | 369300 |
| ActivateQuickslot4 | Button_4 | `4` | 369301 |
| ActivateQuickslot5 | Button_5 | `5` | 369302 |
| ActivateQuickslot6 | Button_6 | `6` | 369303 |
| ActivateQuickslot7 | Button_7 | `7` | 369304 |
| ActivateQuickslot8 | Button_8 | `8` | 369305 |
| ActivateQuickslot9 | Button_9 | `9` | 369306 |
| ActivateQuickslot10 | Button_0 | `0` | 369297 |
| ActivateQuickslot11 and up | none (no action, no tree path) | no value delivered (the slot's key text stays as authored, not read; do not send one) | - |
| SwapWeaponSets | Button_Slash | `/` | 369413 |
| ToggleFullPartySelect | Button_Equals (`_1` = Control+A) | `=` | 369331 |
| ToggleHoldParty | Button_H | `H` | 369351 |
| ToggleCharacterRecord | Button_C | `C` | 369318 |
| ToggleSpellsTalents | Button_P | `P` | 369394 |
| ToggleSkills | Button_K | `K` | 369356 |
| ToggleInventory | Button_I | `I` | 369353 |
| ToggleJournal | Button_J | `J` | 369355 |
| ToggleMap | Button_M | `M` | 369366 |
| ToggleMainMenu | Button_Escape | `Esc` | 369332 |
| ToggleTactics | Button_Backslash | `\` | 369317 |
| ToggleWorldMap (no HUD movie observes it) | Button_N | `N` | 369374 |
| ToggleOptions, TogglePartyPicker | no action | none delivered | - |
| PerformPrimaryAction | mouse 0x0D (set in code) | `Right-click` (`Left-click` if the left-handed flag swaps) | 369711 (369710) |
| PerformSecondaryAction | Shift + mouse 0x0D | `Shift + Right-click` (`Shift + Left-click` swapped) | 369760 + 369711 |

Other key texts of the table, in case the player's bindings differ or other movies need them (checked from `inputmap.gda` and the TLK): Space `Space` 369415; Tab `Tab` 369420; Return `Enter` 369405; F1..F12 `F1`..`F12` (369334..369345); Print Screen (Sysrq) `Print Screen` 369418; Up/Down/Left/Right `Up` 369424, `Down` 369328, `Left` 369362, `Right` 369406; Page Up `Page Up` 369400 (key name Prior), Page Down `Page Down` 369375 (Next); Numpad divide `Numpad /` 369327 (Button_Divide); Grave `` ` `` 369350; mouse wheel `Scroll forward` 369720 (WheelUp), `Scroll backward` 376568 (WheelDown), Middle-click 369712.

Other defaults (from `keymap.gda`, not shown by the HUD movies): TogglePause Space, QuickSave F5, QuickLoad F9, HighlightObjects Tab, Screenshot Sysrq, OpenConsole Grave, HideMainInterface V (and Alt+Z), SelectPartyMember1..4 F1..F4 (Shift+ for Inclusive), PanCamera Up/Down/Left/Right arrows, RunCharacter W/S/Q/E, ToggleRunWalk Divide, MoveCameraLeft/Right A/D (Shift+ second), PanCamera middle-click, ZoomIn wheel up or Next, ZoomOut wheel down or Prior.

### Hints for the host

- Deliver `InputMappings.Mappings.<Action>.PrimaryMappedKey` as a string for exactly the 10 quickslot actions, SwapWeaponSets, ToggleFullPartySelect, ToggleHoldParty, the eight `Toggle*` actions above, PerformPrimaryAction and PerformSecondaryAction; deliver `""` for the rest, as vanilla delivers an absent path. This is vanilla's behaviour for the quickslot labels: slots 11+ show nothing. The engine side that creates the paths (the GroupedInputs elements) exists only for these actions.
- The portraits tooltips for ToggleFullPartySelect and ToggleHoldParty do not test validity, so give them real strings (they are bound by default). `ToggleHoldParty` is also read with `GetValue`, so the path must answer `GetValue` too.
- A game-faithful host reading the player's `KeyBindings.ini` can build the same text: ini name to key, then the talk-table text, then `Shift + `, `Alt + `, `Ctrl + ` in front in that nesting (Ctrl outermost). The ini writes modifiers as `Shift + Keyboard::Button_F1`, `Control + ...`, `Alt + ...` (0x00B30F44, 0x00B30F1C, 0x00B30F34) but `keymap.gda` writes them without spaces (`Control+Keyboard::Button_A`).

### Not read

- The authored initial text of `BoundKey_mc` in quickbar.gfx (what a slot with no token value shows); what `GuiExternal::GetValue` returns for an absent mapping path in play (it was read at the code level only, GUI.md line 760); in-game play check of any of this.
- Which options-screen setting is the mouse-handedness byte at +0x350+0xC read by 0x00838210 and its default; so `Right-click` for the primary action is inferred (default flag false).
- Where the engine loads `keymap.gda` into the mappings and how `KeyBindings.ini` overrides it (the table and the ini agree; the loader code was not followed); the 0x00894710 / 0x00894E44 "No key mapped" paths and the remapping error strings.
- The talk-table strings of the language other than en-us (the table is the same, only the TLK language differs); the full 188-row `inputmap.gda` beyond the rows listed.
- Rows for DirectInput codes whose registration block my byte scan missed (Esc code 1, "0" code 11 and others): taken from the alphabetical order of the TLK strings, which agrees with every key checked.

## gui3d: portraits (lib-agent-2, read 2026-10-18)

Marks: [C] checked in the exe or files, [I] inferred. Addresses are GOG VAs. Scratch: %TEMP%\lib-agent-2-gui3d-*.

### Short answers

1. **Name.** `<creature>.Portrait` (the creature's attribute at creature+0x4C0, which `Party.Members.N.Portrait` resolves to) holds exactly `portrait_` + the creature's tag + a decimal counter, as a bare wide string: no `img://`, no `.dds`, tag not lowercased by this code (the tag is the creature's string at +0xB8 as stored; in play always lowercase) [C]. The counter is ONE GLOBAL int (0xC3905C), post-incremented once per portrait RENDERER CREATION (not per render, not per creature): `portrait_player0`, `portrait_bhn200cr_howe_landra33` [C]. A re-render of the same creature keeps the same name and the same texture (the texture is redrawn in place); a new name appears only when the renderer is destroyed (ReleasePortrait) and made again [C]. So "name changed" means "new renderer/new texture"; "same name, new picture" is a content change the token does NOT announce (the Set happens on each Render too, see below, but with an equal value; whether observers fire on an equal value was not read) [I]. The movie turns it into `img://portrait_player0.dds?Width=w&Height=h` (MovieUtils.GetImageUrl adds ".dds" when there is no "." [C, MovieUtils.as:165]).
   - **Static portraits.** Before any rendering, Render looks through the portraits table (table 7) for a row whose `Appearance` column equals the creature's appearance (creature+0x7B0 record, word at +0x58) and reads that row's `Override` column (hash 0x66B0206F; Appearance is 0x052BEA01 [C: tools/gda column_hash]). If the string is non-empty it Sets the creature's Portrait attribute to it (e.g. `portrait_09`) and returns at once: NO renderer texture, no camera [C, 0x7F4775-0x7F4847]. Core rows 30-39: Bear 9 -> `portrait_09`, Mouse 37, Spider 59, Swarm 14, Arcane Spirit 12, Burning Man 28, Golem 6 -> `portrait_06`, Wolf 49, Bear Beareskan 68, Spider Corrupted 57. Rows 40-41 (Bear Great 8, Wolf Blight 74) have empty Override, so they render live. These are real textures `portrait_NN.dds` in packages/core/data/gui.erf (and shape-form ability rims) [C via ghilana]. The movie's token value for them is simply `portrait_09` and the same GetImageUrl path loads `portrait_09.dds`. A mod row of ours (tb_portrait_fade_tear, appearance 43001) works the same.
   - **A creature with no Portrait value / "?"**: the movie unloads the clip when the value is empty or ends with "?" [C, Portrait.as:139-145].
   - **Triggers (who calls UpdatePortrait / Render)** [C by call-site scan, names from the address library]:
     - `CCCreature::UpdatePortrait` (0x6AE2D0) callers: ScriptBackend::SetFollowerState (0x63B9E1 via 0x637560, a follower's state set, i.e. joining), ScriptBackend::ForScript_RemoveNonPartyFollower neighbourhood (0x631F1E via 0x631710: loops the party list and renders each), GameModePartyPicker select-member handler (0x768B50 via 0x766040), GUICharGen (0x7355D0 / 0x73A08B: appearance scene start), GUIPaperdoll (UpdatePortraitMessage handler 0x7BC6C0, CommandCreateCreature 0x7C0BF0).
     - Re-render of an EXISTING renderer, with no new name: (a) the creature's model rebuild (0x6BDC70, called from 0x6C056F/0x6C16B8) calls `Render(1,1,1)` at its end if the creature has a renderer (+0x78C != 0) and the GUI flag [[0xC327EC]+0x150]+0x181 is 0 (0x6BE2A0-0x6BE2C0): so an appearance or equipment change that rebuilds the model refreshes the portrait; (b) a small CCCreature method 0x69ED50 does `Render(0,1,1)` if a renderer exists (caller not traced). Nothing is on a timer; no level-up hook found [C for the call sites, I for the exact gameplay events].
     - The renderer is released by `ReleasePortrait` (0x69F470) and by ~CCCreature; the party's renderers otherwise live as long as the creature [C].

2. **Rendering** [C unless marked]. Setup (0x7F3E20, run once per renderer, guarded by byte +0xD44):
   - Render target 146 x 146 px (0x92 x 0x92) [C], created through the texture manager (0x8472C0 singleton, 0x847330 create: args out, name, 146, 146, 1, 3, 1, 0, 0 -> 0x846460(out,name,w,h,1,8,3) -> device vtable+0xAC(w,h,...)). The pixel format is the device call's argument; the value 3 was passed but NOT decoded to a D3DFORMAT [not read]. The target is registered in the texture manager's name cache under the bare name (0x846460 -> 0x845D40) [C].
   - Viewport (0,0,146,146), z 0..1 set on the renderer's device (0x7F3E9D, vtable+4) [C].
   - Scene: a SceneCamera and two lights loaded by name (wide strings at 0xB24714 / 0xB246EC / 0xB21D6C / 0xB21D1C): `portrait_lightrig` from `el_lr_new_day2` and `portrait_lightprobe` from `el_lp_outside_day`, by the files (settled 2026-10-19 by lib-agent-2, "The portrait's lights" below). A clear/background vector of zeros is set on the scene (0x7F4165-0x7F4189 -> 0x4597B0, scene+0x84, +0x8C=1) [C that zeros are passed; I that it means clear to 0 / transparent black: the alpha behaviour was NOT read]. Ask the live game (Eluvian) or a RenderDoc-like dump for the real background; the movies draw a portrait frame over it.
   - Posing: the renderer holds a CLONE creature (+0x20, built with id -1). Render(p1,p2,p3) with p1 = 1 (UpdatePortrait, model rebuild) refreshes the clone from the source's appearance and equipment (0x7F4440, 0x7F41D0); p1 = 0 takes the other branch, which plays one of two animations by gender-like byte (0x2A5, or 0x2A6 when 0x6A3650 == 2) on the clone (vtable +0xB0, ids 0x2A5/0x2A6, blend 1.0) and sets the expression from the source's +0x598 settings (value at +0x7C of the attribute) [C for the calls, I for "idle/face pose" meaning]. So UpdatePortrait's p1 = 1 path is the one that does not run that animation call.
   - Camera (p2 = 1): PlaceCamera (0x7F3130, ID 21341) with the eight values from table 7, "unset" (65505.0) for an ordinary creature so PlaceCamera reads them itself; for creatures whose tag is one of the two char-gen strings (0xC3907C / 0xC39060) the source's own settings (+0x598) are passed (x 0.017453 for one of them via 0xAF0E48) [C]. Row: by creature tag (Tag column), else the Default row; for the Default row, the row whose Appearance equals the creature's if any [C, Portraits.cpp / Addresses.h]. The camera aims at the clone's `head` node (CCCreature::HeadTransform 0x6A1EF0), is rotated by CameraRotation (yaw, degrees) and CameraPitch, backed off by CameraDistance (metres), shifted by CameraZAdjust (vertical) and CameraXAdjust (sideways); light offset LightX/Y/ZAdjust.
   - FOV: not read (the SceneCamera's default; set outside Render/Setup) [not read].
   - Default row (table 7 ID 1, Tag "default", portraits.gda) [C via ghilana]: CameraPitch -7.5, CameraRotation 23.5, CameraDistance 0.675, CameraZAdjust 0.038, CameraXAdjust 0.007, PortraitTint 1, Expression 0, Appearance 0, LightXAdjust -5, LightYAdjust 80, LightZAdjust 5. (Row 0 is the blank all-zero row.)
   - Examples: Alistair (gen00fl_alistair) pitch 10, rot -15, dist 0.62, Z 0.04, X -0.004, light -5/80/5; Morrigan pitch -5.88, rot 30, dist 0.60, Z 0.055, light 3/45/-55; Dog pitch 0, rot -40, dist 0.95, Z -0.035, X -0.11, light 1/95/15; Sten pitch 0, rot 20, dist 0.70, Z 0.03, light 0/10/-3; Zevran 10/25/0.60/0.04/-0.005, light -5/65/-20; Wynne -9/-19.96/0.57/0.061/-0.007, light -1.5/1.75/0.5; Shale -9.88/-10/0.70/0.01/0, light 0/95/25; Oghren 12.75/19.83/0.72/0.02555/0.015, light -5/2.5/2.5; Leliana -15/-30/0.60/0.056/-0.007, light 25/85/17. The other rows (Loghain onward) read the zeros in this install for the camera? Not all rows paged for camera columns: rows 11-41 camera values were not read beyond the above (only Tag/Appearance/Override), read them with the table tool if needed.
   - Dirty flag: at the end Render sets `renderer.scene+0x11 = 1` (0x7F4A19): the scene draws the portrait on the next frame, so the texture is updated next frame, not inside the call [C for the flag, I for meaning].

3. **img:// and a not-ready name.**
   - For portraits the question mostly does not arise: portraits.gfx loads with `MovieUtils.LoadImageFixedSize` -> `LoadImage` -> `loadMovie(GetImageUrl(name, w, h))` with NO `?Async=1` [C, MovieUtils.as:199-209, Portrait.as:141]. And the engine registers the render target in the texture manager's name cache BEFORE it Sets the Portrait attribute (0x7F3F9A register, then 0x7F3FC3 Set) [C], so the token never announces a name that is not already resolvable.
   - Lookup order [C, 0x845D80]: the loader cuts the name at the first "." (0x845E15-0x845E3F: find 0x2E, substring), so `portrait_player0.dds` becomes `portrait_player0`, then looks it up in the manager's cache (0x845600); a hit returns that texture AT ONCE, before the Async branch is even considered (0x845E95 -> 0x4F71A0 assign, jump 0x8462BB). That is how the live render target is found by an img:// URL. (Before that ScaleformManager::LoadImage also checks a table of pre-registered images, 0x523580 on this+0xD0 [I, GUI.md].)
   - Async=1 on a name NOT in the cache: an empty texture object (0x6C bytes, loaded flag +8 = 0) is returned at once and the load is queued; it is a fresh, empty (not the previous texture of that name, not default_missing) object that fills in later [C as GUI.md; I that "empty" means blank]. If the load fails the retry is with `default_missing` (GUI.md). The code that completes the queued job and how the Scaleform image is told to refresh were NOT read.
   - New portrait name = new texture each time: the counter changes only with a new renderer, whose target is a new texture object [C]. The old renderer is destroyed by ReleasePortrait: 0x7F3D00 destroys the clone and renderer; whether it also unregisters the old name from the texture manager's cache and releases the target was not read [not read; I: yes, via the texture's refcount]. A movie still showing the old name keeps its own image-info reference.
   - Port advice [I]: key the texture by the bare name; strip the extension from `img://<name>.dds?...` like 0x845E15 does, serve the live target if the name is registered, else the archive texture, else `default_missing`; treat a changed name as "load new", same name as nothing to do (redraw the live texture in place each frame the engine marked dirty).

### Evidence index

- UpdatePortrait 0x6AE2D0: creates PortraitRenderer (0xD70 bytes, ctor 0x7F3D80) if creature+0x78C is 0, calls 0x6ADA50 (prepares the creature; requires game state [0xC327EC]+0x14C and a flag at creature+0x7D4 == 1), then Render(1,1,1) 0x7F46E0, returns 1 [C].
- Render 0x7F46E0 (`ret 0xC`; params at [esp+4C] p1, [esp+50] p2, [esp+54] p3 after the prologue): needs source +0xD40, its +0x70 interface and +0x7B0 appearance record; p3 true -> static-Override lookup (above); then Setup 0x7F3E20; name string at renderer+0xD4C; if the creature's Portrait attribute's value differs from +0xD4C it is Set (0x7F4871-0x7F48B4: get current value via 0x403C70, compare 0x4C0E50, Set via vtable+0x54, notify via +0x24); p1 true -> 0x7F4440 clone refresh else animation; p2 true -> camera (PlaceCamera 0x7F3130); finally dirty flag [C].
- Setup name build 0x7F3F1A-0x7F3F62: `[0xC11C48]` -> wide "portrait_" at 0xB246B4 (read from the exe's data), concatenated with the creature tag (creature+0xB8) by 0x4C35F0, then the counter appended by 0x4C3580 (int -> text) with the post-incremented global [0xC3905C] [C]. The counter starts at 0 (player0 seen in play, dev/GUI.md).
- Table: ghilana `table 7` (portraits.gda in 2da.erf, plus our turnbased mod's portraits_turnbased.gda row 1000000 "Fade Tear", appearance 43001, Override `tb_portrait_fade_tear`; two add-in files, portraits_str/portraits_gib, are enciphered and unread).
- AS: portraits/scripts/__Packages/SharedLibrary/Portrait.as:130-148 (observer on `<creature>.Portrait`); MovieUtils.as:152-209 (GetImageUrl, LoadImage). Same files as `%TEMP%\lib-agent-2-guicmd-x\allgfx\as_portraits`.

### Not read

- The D3D format and usage of the 146x146 target (the argument 3 / 8 of 0x846460), whether it has mips or alpha, the clear colour/alpha, the camera's FOV and near/far, which of the four scene strings is the camera/rig/light.
- Whether `Attribute::Set` with an equal string notifies observers (matters for "redraw without a name change"), and so how a movie learns that a re-rendered portrait changed.
- 0x7F4440 (the clone refresh) and 0x7F41D0 in detail; what the animation ids 0x2A5/0x2A6 are; the caller of 0x69ED50 (Render(0,1,1)).
- The exact gameplay events that reach 0x6BDC70 (model rebuild) and so refresh a portrait (equipment, appearance, armour tint); no level-up trigger.
- ReleasePortrait's release of the texture and cache entry; the queued Async job's completion and how Scaleform refreshes the clip.
- Camera columns of table 7 rows 11-41 (Loghain onward; most are zeros or the Default values; not paged).

## PortraitRenderer::PlaceCamera: what it aims and orients by (lib-agent-2, read 2026-10-19)

Marks: [C] checked in the exe's disassembly or data, [I] inferred. GOG VAs. Disassembly kept in %TEMP%\lib-agent-2-placecam-dis.txt / -dis2.txt.

### Short answer

1. **The head node supplies only a position.** PlaceCamera never reads the head node's rotation or axes [C]. It hands `CCCreature::HeadTransform` (0x006A1EF0) the homogeneous point (0, 0, 0, 1) and gets back that point carried by the node named `head` (wide string at 0x00B0F450): for the origin, that is the node's world translation and nothing of its rotation. The result is divided by w and is the camera's aim point [C for the call and the use; I that the node's `vt+8` is a plain transform of the point, which the origin input makes a translation].
2. **The camera's direction comes from the pitch alone, in the portrait scene's fixed world axes** [C]. A back-off vector (0, D, 0) is turned by a quaternion built from the pitch (`FromYawPitchRoll(0, pitch, 0)`), the camera looks along its negative, and its yaw is that vector's heading (`Vector::ToAngle`). No creature facing, no node axis goes into it.
3. **The rotation (CameraRotation) is given to the clone, not the camera** [C]: `CCObject::SetOrientation` (0x00694CE0) on the clone (`renderer+0x20`) with the facing vector (sin(-rot), cos(-rot), 0), every call. That resets the clone's facing absolutely (rot 0 = (0, 1, 0), +Y), whatever the clone had; the camera, at +Y of the head, then looks at the clone's front.
4. **For the hero's morph head**: the portrait shows the face frontally if the main view does, because both turn the model by the creature's orientation (the object's facing vector), and neither takes the head node's own -Z. A head node a quarter turn off the body's forward does not tilt this camera [C for the camera, I that the model root follows the orientation vector in both scenes]. It would show from the side only if some code built the camera from the node's rotation, and PlaceCamera does not. What a quarter-turned node does change: the node's world translation (the aim point) is the same point whatever its rotation, so the aim is unaffected. A mesh that compensates for the node (as in the main view) compensates the same in the portrait.

DAOTurnBased's reading (dev/UI.md:67, dev/DAOLIB_NEEDS.md:121-122, 124) agrees on every point it made and is now settled by the disassembly, with two corrections: its "the rotation is given to the clone ... the function itself was not read" is confirmed (below), and HeadTransform's point (0,0,0,1) set "just before the call" is at 0x007F35BF-0x007F35DE area (`[esp+0x50..0x5c]` = 0,0,0,1 written at 0x007F356C-0x007F357C, passed to 0x006A1EF0 at 0x007F35D9).

### Prior work used (cited)

- C:\project\DAOTurnBased\dev\UI.md:67 (back-off 3 x CameraDistance, 10 degrees, pitch turns about the head, Z and X along the camera's own up and right, 0.15-40 m), UI.md:70 ("the rotation turns the view round the creature"), UI.md:77 (no head node: aims at the clone's root).
- C:\project\DAOTurnBased\dev\DAOLIB_NEEDS.md:121 (distance formula `(distance + [0x00C39054]) * 30 / [0x00C11C4C]`), :122 (rotation given to the clone via 0x00694CE0 with a sine/cosine vector; not read further), :124 (HeadTransform (0,0,0,1), clone at scene origin, seen in play).
- DAOLib master src/dao/Portraits.cpp (header comment, HeadPosition), %TEMP%\lib-agent-2-gui3-portrait.md (Render calls PlaceCamera with 8 values, "unset" = 65505.0 at 0x00B1D9C8; Default row; the clone is rebuilt each Render with p1).
- New here: the camera's axes and the clone's orientation reset, read from the instructions.

### Evidence [C unless marked]

Constants read from the exe: 0xB1D9C8 = 65505.0 (unset), 0xAF7068 = 30.0, 0xC11C4C = 10.0 (FOV degrees; also given to the scene camera), 0xC39054 = 0.0, 0xAF14C0 = pi/180, 0xAE7108 = 2.0, 0xB00618 = 90.0, 0xAF0E50 = 180/pi.

Helpers (identified by their instructions):
- 0x004EE200 `Quaternion::FromYawPitchRoll(out, a, b, c)`: multiplies the three angles by pi/180 (degrees in).
- 0x004CB6D0 rotate a vector by a quaternion (out, quat, vec).
- 0x004EE570 `Vector::ToAngle` (heading of a vector in XY, degrees).
- 0x004CBCE0 the quaternion's X axis column (1-2(y2+z2), 2(xy+wz), 2(xz-wy)); 0x004CBD80 its Z axis column (2(xz+wy), 2(yz-wx), 1-2(x2+y2)); 0x00451A30 normalise with a fallback; 0x004CB640 quaternion multiply; 0x004CBCB0 conjugate.
- 0x00694CE0 `CCObject::SetOrientation(vec4*)`: stores the vector at `this+0x130` and calls virtual +0x80 (update).
- 0x006A1EF0 `CCCreature::HeadTransform(point*)`: finds node `head`, updates it (node vt+0x18), transforms the point (node vt+8) [the transform's identity I]; returns false with the point untouched when there is no such node.

Flow of 0x007F3130 (`ret 0x20`; args pitch +8, rotation +0xC, distance +0x10, zAdj +0x14?, xAdj ..., lightX/Y/Z +0x1C/+0x20/+0x24; the Z/X order below):
- 0x007F3130-0x007F3381: any argument equal to 65505.0 is read from the portraits table row (cols 0x5B84AEC7 pitch, 0x97A92B07 rotation, 0xEFBEFD6C distance, 0xDB253899, 0x3A4BFC34, 0x8327EA19, 0x1E280B6F, 0x62492EB4), by tag row then appearance (the chargen tags 0xC3907C / 0xC39060 use the source's own settings). Not new.
- 0x007F3381-0x007F33CD: `D = (distance + 0) * 30 / 10`; `Q = FromYawPitchRoll(0, pitch, 0)`; `R = Rotate(Q, (0, D, 0))`.
- 0x007F33DB-0x007F3424: `dir = -R`; `heading = ToAngle(dir)`; `Q2 = FromYawPitchRoll(heading, -pitch, 0)`.
- 0x007F3429-0x007F34E7: `pos = R + normalise(Q2.X-axis) * s1 + normalise(Q2.Z-axis) * s2` (the two adjusts; the camera's own right and up; by my stack count s1 is the argument at +0x18 and s2 the one at +0x14, which is the reverse of the names DAOLib and DAOTurnBased give them; not reconciled, see Not read).
- 0x007F34EC-0x007F3538: `Q3 = FromYawPitchRoll(...90.0 ...)`; `final = Q2 * Q3` with a conjugate: a fixed 90-degree camera-convention turn (the camera is not looking down its own Y). [C that 90.0 is the constant; I for its meaning.]
- 0x007F3541-0x007F35D9: the point (0,0,0,1) goes into `[esp+0x50..0x5C]`; `angle = -rotation * pi/180`; `fsincos`; facing vector = (sin, cos, 0) at 0x007F35A1-0x007F35C6 (sin stored first = x, cos second = y); `SetOrientation(clone = renderer+0x20, facing)` at 0x007F35CD; `HeadTransform(clone, point)` at 0x007F35D9.
- 0x007F35DE-0x007F3634: `iw = (w != 0) ? 1/w : 1`; `head = point.xyz * iw`; `lightpos = head + (the vector held at [esp+0x20], the aim-relative offset)`. Both lights (`renderer+0x10`, `+0x14`) get vt+0x34 with positions then vt+0x10 with (lightX, lightY, lightZ) (0x007F363C-0x007F3699).
- 0x007F369B-0x007F3732: the scene camera (`renderer+0xC`): vt+4 with two pointers (position and rotation as built above), vt+0xC with the FOV 10 deg * pi/180, vt+0x18 with near 0.15 (0x00ADEEA8) and far 40.0 (0x00AE7534); then the renderer's `+0x1C` object is asked for a camera (vt+0x28) and its virtual +8 target is set (viewport/dirty); not read in detail.

Pseudo-code:

```
D     = (distance + 0.0) * 30.0 / 10.0
Q     = YawPitchRoll(yaw=0, pitch=pitch, roll=0)
R     = Q * (0, D, 0)                   // back-off vector, world axes, in the clone's scene
dir   = -R
Q2    = YawPitchRoll(heading(dir), -pitch, 0)
eye   = R + Xaxis(Q2)*adj_a + Zaxis(Q2)*adj_b      // then + head (see below)
face  = (sin(-rotation), cos(-rotation), 0)
clone.SetOrientation(face)              // absolute; rotation lives here, not in the camera
p     = (0,0,0,1); clone.HeadTransform(&p)   // node "head" world position; unchanged if no node
head  = p.xyz / p.w                     // (0,0,0) of the scene when there is no head node
camera.pos = head + (the eye offset), camera.rot = Q2 * fixed90
camera.fov = 10 deg, near 0.15, far 40
```

(The final add of `head` into the camera position is at 0x007F35F9-0x007F3632 for the lights; for the camera, `vt+4`'s first pointer is built at 0x007F36B7-0x007F36C5 from `[esp+0x24]`, which I took to be the same head-plus-offset position; [I], not traced to the last operand.)

### Why the hero's morph head does not matter here

- The camera's yaw and pitch contain no quantity read from the clone: the clone's orientation is set from the rotation argument alone (so any facing it had is overwritten) and the camera is built from the pitch alone. A head node whose -Z is a quarter turn from the body forward changes nothing in them.
- The head node enters through one translation. Morph heads built by the morph path (dev/CHARACTERS.md 0x0055EC80 / 0x0054EB20, %TEMP%\lib-agent-2-morph-build.md) still have a node named `head`; if the morph-built head node is found by name `head` in the clone's model, the aim is its position, same as Alistair's. If a morph-built model has no node of that name, the engine falls back to the scene origin (the clone's root), as for the deepstalker (DAOTurnBased dev/DAOLIB_NEEDS.md:124) [I for the morph case].
- So in the portrait the face is frontal exactly when the clone, facing +Y at rotation 0, shows its face along its body's forward, which is what the main view does with the object's orientation. Rotation values from the portraits table then turn the clone by that many degrees from frontal; the Default row's 23.5 is a three-quarter view of a head that is frontal at 0 (inferred from the table values, not from play). (Corrected 2026-10-19 by lib-agent-2: the Default row's framing is for creatures other than the player; the hero and the paperdoll use the creature's own camera record, below.)

### Not read

- The last operands of the camera's position (vt+4's first argument at 0x007F36B7) and the exact `[esp+0x20]` offset vector fed to the lights; I read the structure, not every stack slot, so the sign convention of the lateral adjust and which of the X/Z adjusts maps to which scalar is unreconciled with DAOTurnBased's "Z up, X sideways" (seen in play there); the arithmetic above may be off by one slot. (Resolved 2026-10-19 by lib-agent-2, below: the X adjust is -1.0 × PositionH, the constant at 0xAF0E48 being the double -1.0, and Z is up and X right, with no swap.)
- The node class behind `HeadTransform` (vt+0x10 lookup by name, vt+0x18, vt+8): I did not resolve the class or confirm vt+8 is a point transform; a transform of the origin is a translation if so.
- Whether the clone's model root yaw follows `CCObject+0x130` as the main view's does (the vt+0x80 update at 0x00694CF5), and the convention (which body axis faces along the vector); inferred equal for both scenes since both are CCCreature.
- The meaning of the fixed 90.0 turn, the `renderer+0x1C` target branch at the end, and whether a morph-built hero's clone has a `head` node of that name.
- No game was run and Ghidra was not used.

### PortraitRenderer::PlaceCamera read through: the framing, the clone's turn, the light (lib-agent-2, portcam, 2026-10-19)

Marks: [C] checked in disassembly or data, [I] inferred. GOG VAs. Scratch: %TEMP%\lib-agent-2-portcam-*.txt. Builds on %TEMP%\lib-agent-2-placecam.md (cited as placecam.md) and corrects it where noted.

#### Short answers

1. **Probable cause of the mismatch: the hero is not framed by the Default row at all, but by its own saved portrait settings.** Render (0x007F46E0) tests the source creature's tag against two strings (the wide strings at 0x00AECC9C `paperdoll` and 0x00AECCB0 `player`, copied into globals 0x00C3907C and 0x00C39060 by static initialisers at 0x00AD3550 and the like) with the equality test 0x004C0DE0. For a creature tagged `player` (the hero) or `paperdoll` it does NOT pass "unset" (65505.0): it passes the creature's own record at `creature+0x598` (record pointer at +0x5A4, via 0x0053CC40): pitch = rec+0x58, rotation = rec+0x5C, distance = rec+0x60, Z adjust = rec+0x68, X adjust = rec+0x64 times -1.0 (the constant at 0x00AF0E48 is the double -1.0, not 0.017453), the three light values left "unset" (so they come from the table row). [C: Render 0x007F4943-0x007F4A14; the stores that fill the record, 0x00739FED-0x0073A042.]
   The record is filled at character creation from the Default row (CharGenStartAppearanceScene 0x00739D20, dev/CHARGEN.md:112) and is **in the save game**: `SavegamePlayerPortraitPitch/Yaw/Tint/Expression/Distance/PositionH/PositionV` (dev/gff4_fields.csv:1889-1895, 16332-16338). Read from the saves on this machine [C, tools/gff4.py --dump]: Lyna/AutoSave_1 and Lyna2/QuickSave_1: pitch -7.5, yaw 23.5, distance 0.675, H 0.007, V 0.038 (the Default row). **Leliana/AutoSave_1: pitch 0, yaw 0, distance 0.6, H 0, V 0, tint 1, expression 0.** A hero with those values is framed exactly as the peer sees vanilla: frontal (rotation 0), 0.6/0.675 = 11% nearer, so the head is 12.5% larger (a visible height of 0.315 m against 0.354 m, a length scale of 1.125; 15-20% with the head not being the only thing that changed), and the aim sits on the head node's origin, 0.038 m lower than the Default row's, so the face rides about 0.038/0.354 = 11% of the picture's height higher (about 16 px of 146). **Check the peer's save/hero for `SavegamePlayerPortrait*` first**; if it is a Default-row hero, the exe says vanilla's net turn is 23.5 degrees as the port has it, and this is not the cause.
2. **The camera is not turned by CameraRotation; only the clone is.** Camera on fixed scene axes from the pitch alone; the clone is turned to face (sin(-rot), cos(-rot), 0); the model follows its facing as yaw = atan2(-x, y) about Z, so rot 0 shows the front to a camera at +Y, and rot 23.5 turns the face 23.5 degrees away from the camera (toward the camera's right) [C for the formulas; I that the model's front is its +Y at yaw 0]. For the Default row the net angle between view direction and face direction is exactly the row's rotation, 23.5 degrees. Nothing cancels it.
3. **X/Z adjusts are not swapped** (placecam.md's doubt resolved): arg +0x14 (CameraZAdjust, hash 0xDB253899) scales the camera's UP axis, arg +0x18 (CameraXAdjust, 0x3A4BFC34) scales its RIGHT axis [C: column hashes recomputed as CRC32 of the lowercase UTF-16 names]. In metres, not scaled by the distance. The camera's final position is `head + R + right*X + up*Z`, which also moves the aim (the view direction is fixed), so the aim line passes through `head + right*X + up*Z`.
4. **Framing numbers**: back-off `D = 3 x CameraDistance` (`(dist + 0) * 30 / 10`; the 0 is the float at 0x00C39054, never written; the double added after it, 0x00ADE210, is 0.0), vertical FOV 10 degrees (float 0x00C11C4C = 10.0, written nowhere, converted by pi/180 at 0x007F36A6), near 0.15 and far 40.0, target 146x146 so aspect 1, aim = the `head` node's world translation (the clone stands at the scene origin) plus the two adjusts. Nothing else (no node other than `head`, no further Z offset).
5. **Lights**: two scene objects loaded by name in Setup ("portrait_lightrig" from "el_lr_new_day2", "portrait_lightprobe" from "el_lp_outside_day", strings at 0x00B24714, 0x00B21D6C, 0x00B246EC, 0x00B21D1C). PlaceCamera sets the same vector (LightXAdjust, LightYAdjust, LightZAdjust) on both with a virtual call (+0x10) after a first virtual call (+0x34) on each, in the scene's frame, not relative to the head or the camera and not turned with the clone. Their classes were not identified, see Not read.

#### Pseudo-code a port can copy

```
// inputs: (pitch, rot, dist, zAdj, xAdj, lightX, lightY, lightZ)
// hero (tag "player") or tag "paperdoll": (rec.pitch, rec.yaw, rec.distance, rec.positionV, -rec.positionH, table.lightX/Y/Z)
// anyone else: all eight from the table row (by tag, else the Default row; the row with Appearance == the creature's when on the Default row)
// where the table rows apply: any argument equal to 65505.0 is read from the row.

D     = dist * 30 / 10                                   // = 3 * dist
R     = Rot(Qx(pitch), (0, D, 0))                        // Qx: rotation about X by pitch degrees. R = (0, D cos p, D sin p)
dir   = -R
head_ang = atan2(-dir.x, dir.y) in degrees               // 4EE570 ToAngle; 180 when R.x == 0
Q2    = Qz(head_ang) * Qx(-pitch)                        // 4EE200: Q = Qz(a1) * Qx(a2) * Qy(a3), angles in degrees
right = normalise(Q2.Xaxis)                              // 4CBCE0 (first column of the rotation matrix)
up    = normalise(Q2.Zaxis)                              // 4CBD80
E     = R + right * xAdj + up * zAdj
clone.SetOrientation((sin(-rot), cos(-rot), 0))          // every call; absolute
head  = clone.node("head").worldTranslation              // 0x006A1EF0 -> node vt+0x18 (update) and vt+0x8 (0x0055DDF0: copies m[0][3],m[1][3],m[2][3],m[3][3])
head  = head.xyz * (w != 0 ? 1/w : 1)
camera.position = head + E
camera.rotation = conjugate(Q2 * Qx(90))                 // a camera looking along its local -Z with +Y up; the 90 is the float at 0x00B00618
camera.fov = 10 deg (vertical); near 0.15; far 40
light(rig).pos = light(probe).pos = (lightX, lightY, lightZ)   // scene frame
```

Clone yaw: model yaw = atan2(-fx, fy) degrees about Z (0x006955C0 -> 0x00A5CBE0 uses 0x004EE570), so facing (0,1,0) is yaw 0, front toward +Y; facing (sin(-rot), cos(-rot)) is a yaw of +rot, turned counter-clockwise seen from above, i.e. toward -X.

##### Default row worked through (pitch -7.5, rot 23.5, dist 0.675, Z 0.038, X 0.007, light -5/80/5)

- D = 2.025. R = (0, 2.025 cos 7.5, -2.025 sin 7.5) = (0, 2.0077, -0.2643): the camera is 2.01 m in front (+Y) of the head and 0.264 m below it.
- dir = (0, -2.0077, 0.2643); head_ang = 180 (+-180, the same rotation); Q2 = Qz(180) * Qx(+7.5). Forward = (0, -0.9914, 0.1305): looks toward -Y and 7.5 degrees upward. right = (-1, 0, 0); up = (0, 0.1305, 0.9914).
- E = R + right*0.007 + up*0.038 = (-0.007, 2.0127, -0.2266). Camera at head + (-0.007, 2.0127, -0.2266).
- Aim line through head + (-0.007, 0.0050, 0.0377): the aim is 3.8 cm above and 0.7 cm to the camera's right of the head node origin.
- Visible height at the head's distance 2.025 m: 2 * 2.025 * tan 5 degrees = 0.354 m (the picture is 146 px, so 2.4 mm a pixel).
- Clone facing (sin -23.5, cos -23.5, 0) = (-0.3987, 0.9171, 0): the face points 23.5 degrees off the line to the camera, toward the camera's right (-X). Net angle between view and face: 23.5 degrees.
- Hero with the Leliana-save values (0, 0, 0.6, V 0, H 0): D = 1.8, R = (0, 1.8, 0), E = R, camera at head + (0, 1.8, 0), level, face frontal, visible height 0.315 m, aim on the node origin.
- Hero with the Default values: Z = 0.038, X passed as -0.007 (not +0.007) because of the -1.0 factor, so the aside is (+0.007, ...) in the camera's right (+X world); the table path (non-hero) uses +0.007.

#### Evidence

##### Argument order and the table columns [C]
- PlaceCamera 0x007F3130, `ret 0x20`, args at ebp+8..+0x24: pitch +8 (col 0x5B84AEC7 CameraPitch), rotation +0xC (0x97A92B07), distance +0x10 (0xEFBEFD6C), +0x14 (0xDB253899 CameraZAdjust), +0x18 (0x3A4BFC34 CameraXAdjust), +0x1C (0x8327EA19 LightXAdjust), +0x20 (0x1E280B6F LightYAdjust), +0x24 (0x62492EB4 LightZAdjust). Hashes recomputed here with CRC32 of the lowercased UTF-16LE column name.
- Table logic 0x007F3140-0x007F337F: source creature = this+0xD40, row = this+0xD48. If the tag != `paperdoll` (0x004C0E50 is inequality): flag = (appearance word [rec7B0+0x58] != [rec7B0+0x5A]); if row == 1 and appearance != 0xF and tag != `player`, flag |= 1. With the flag set every argument is replaced by the table's value from the row whose Appearance column equals the creature's appearance (loop 0x007F31F0, column 0x052BEA01), else the Default row. Without the flag only arguments equal to 65505.0 (double at 0x00B1D9C8) are read from the row.

##### Which settings the hero passes [C]
- Render 0x007F4938-0x007F4A14: when tag equals `paperdoll` (0x007F495B jne) or `player` (0x007F4975), the settings branch at 0x007F499E; otherwise 0x007F4977 pushes seven 65505.0. Branch: 0x007F49C0 first lookup (0x0053CC40 returns [attr+0xC], the record), pushes in order rec+0x64 * (-1.0) (fmul qword [0xAF0E48], the double -1.0), rec+0x68, rec+0x60, rec+0x5C, rec+0x58 on top of three 65505.0 slots, then calls 0x007F3130. Argument order from the top of the stack: pitch rec+0x58, rotation rec+0x5C, distance rec+0x60, Z rec+0x68, X = -rec+0x64, then the lights.
- The record's fields as chargen fills them, 0x00739F49-0x0073A042: CameraPitch -> +0x58, CameraRotation -> +0x5C, CameraDistance -> +0x60, CameraXAdjust -> +0x64, CameraZAdjust -> +0x68, PortraitTint -> +0x6C (attribute set), Expression -> +0x7C. So the save's PositionH = X (+0x64), PositionV = Z (+0x68) [I for the save field to field map; the names and the values (0.007, 0.038) agree].
- Tag strings: 0x00AECC9C `paperdoll`, 0x00AECCB0 `player` [C data]; the globals 0x00C3907C and 0x00C39060 are initialised from them by 0x00AD3550 and 0x00AD3570 [C for the first push; the second inferred by symmetry of the pair of initialisers]. Placecam.md/gui3-portrait.md called them "chargen tags"; they are the hero's tag and the paperdoll's.
- Saves read: C:\Users\tianx\OneDrive\Documents\BioWare\Dragon Age\Characters\{Lyna,Lyna2,Leliana}\Saves\...\savegame.das, with `python tools\gff4.py --dump FILE --depth 4 --items 5`.

##### The math [C]
- 0x007F3381-0x007F33CD: vec (0, D, 0) built at S0+0x20..0x28 (D at +0x24 = `(0.0f(0xC39054) + dist) * 30.0(0x00AF7068, double) / 10.0f(0xC11C4C) + 0.0(0x00ADE210, double)`). `0x004EE200(out, 0, pitch, 0)` then `0x004CB6D0(out, Q, vec)`.
- 0x004EE200(out, a1, a2, a3): degrees to radians (double 0x00AF14C0 = pi/180); three axis-angle quaternions (0x004CBC40: (axis*sin(t/2), cos(t/2))): a1 about Z (axis from 0x00C1EED0 = (0,0,1)), a2 about X (0x00C1E870 = (1,0,0)), a3 about Y (0x00C30260 = (0,1,0)); the product `Qz(a1) * Qx(a2) * Qy(a3)` by two calls to 0x004CB640 (Hamilton product with out.x = e.x c.w + c.x e.w + e.y c.z - e.z c.y). So the call (0, pitch, 0) is a turn about X by the pitch, and (heading, -pitch, 0) is Qz(heading) * Qx(-pitch). The name "FromYawPitchRoll" fits only as (yaw=a1 about Z, pitch=a2 about X, roll=a3 about Y).
- 0x004CB6D0: the standard q v q* (terms 2(xy -/+ zw) etc.).
- 0x007F33DB-0x007F3424: dir = -R at S0+0x30; 0x004EE570(&dir) = atan2(-x, y) in degrees (fld x; fchs; fld y; call 0x008E893A; times 180/pi 0x00AF0E50 = 57.29578); second 0x004EE200(out, heading, -pitch, 0).
- 0x007F3429-0x007F34E7: `0x004CBCE0(Q2)` writes (1-2(y2+z2), 2(xy+wz), 2(xz-wy)), the first column; 0x00451A30 normalises (fallback (1,0,0)); multiplied by arg +0x18 (stored at S0+0x14 at 0x007F342F) and added to R; then `0x004CBD80(Q2)` = (2(xz+wy), 2(yz-wx), 1-2(x2+y2)), normalised, multiplied by arg +0x14 (stored S0+0x18 at 0x007F348B) and added: P2 = R + right*arg18 + up*arg14.
- 0x007F34EC-0x007F3541: Q3 = 0x004EE200(out, 0, 90.0f(0x00B00618), 0) = Qx(90); F = Q2 * Q3 (0x004CB640(out, Q2, Q3) = e*c with e = first = Q2); the conjugate (0x004CBCB0) of F is the camera's rotation.
- 0x007F3541-0x007F35D9: point (0,0,0,1) at S0+0x50; angle = -rot * pi/180; `fsincos`: cos stored to S0+0x1C, sin to S0+0x10; facing = (sin, cos, 0) at S0+0x90..0x98; `0x00694CE0(clone = this+0x20, &facing)` then `0x006A1EF0(clone, &point)`.
- 0x00694CE0 stores the vector at +0x130 and calls the virtual +0x80 (0x006A0190), which calls 0x006955C0: with the model owner `0x00A5CBE0(&obj+0x130, pitchflag)`: yaw = 0x004EE570(vector) (atan2(-x, y)), pitch 0x004EE5A0 or 0x004EE370, quaternion 0x004EE200(yaw, ...) set as the model's rotation (owner vt+0x1C). ANIMATION.md:63 reads the same for ordinary creatures.
- 0x007F35DE-0x007F3632: iw = (w == 0) ? 1 : 1/w; head = point.xyz * iw; `S0+0x20 = head + P2` (the camera position; stored by movaps over P2).
- Head node: 0x006A1EF0 finds the node by the wide string `head` (0x00B0F450) with ObjectNode vt+0x10 on the model, then node vt+0x18 (0x0055DB50, update) and vt+8 = **0x0055DDF0, which does not transform the input point: it overwrites it with the node's matrix column 3 (+0x6C, +0x7C, +0x8C, +0x9C)**. So the aim is exactly the head node's world translation, w normally 1 [C]. (Placecam.md said "a transform of the origin"; the result is the same.)
- Camera calls 0x007F369B-0x007F3732: vt+4 (SetTransform(position*, rotation*)) with position = S0+0x20, rotation = S0+0x40 (the conjugate F); vt+0xC (FOV in radians: 10 * 0.0174533 = 0.17453); vt+0x18 (near 0.15 = 0x00ADEEA8, far 40.0 = 0x00AE7534). The SceneCamera (0x00AF77AC) forwards to an inner camera at +4. CAMERA.md:37 has the explore camera's FOV (same call) as vertical degrees [C there], so this 10 degrees is vertical.

##### What was not changed by the clone's turn
- The clone is reoriented each call (absolute), so any facing it had is lost. Render's animation 0x2A5/0x2A6 (anim_base rows "Portrait Expression - Male/Female", AnimName portrait_exp_m / portrait_exp_f, Priority 0, DisableLookAt 0) are facial expression clips and were not decoded for a head turn [I].

#### Cited prior notes
- %TEMP%\lib-agent-2-placecam.md:7-10 (the aim, the camera from the pitch alone, the rotation given to the clone) confirmed; :33-41 flow confirmed; :60 (last operand) now settled as head + P2; :70 (X/Z doubt) settled, no swap.
- %TEMP%\lib-agent-2-gui3-portrait.md:20 (said the two strings are chargen tags; they are `paperdoll` and `player`; "x 0.017453 via 0xAF0E48" is wrong, the constant is the double -1.0 and applies to the X adjust only) and :22 (Default row values).
- C:\project\DAOTurnBased\dev\UI.md:67-70 (3 x distance, 10 degrees, adjusts along the camera's up and right in metres; "side profile ... as we do for humans") agrees. :70 "the rotation turns the view round the creature" is the clone's turn seen from the camera [consistent].
- DAOLib dev/CHARGEN.md:112 (copying the Default row into +0x598; now known to be what the hero's portrait uses afterwards, and that it is saved), dev/gff4_fields.csv:1889-1895.

#### Not read
- The classes of the two light objects (Setup's scene-manager call vt+0x80 at 0x007F4064/0x007F40CD, the scene manager is the object at 0x00C31650) and what their vt+0x34 and vt+0x10 do: PlaceCamera calls vt+0x34(buf) then, after overwriting buf's x, y, z with (lightX, lightY, lightZ), vt+0x10(buf); the first looks like a getter of the light's own 4-vector (so its w is kept) and the second a setter, so the pair would set the light's position to the three values; the scene's frame, the lights' type, colour and range come from the rigs el_lr_new_day2 and el_lp_outside_day (resources not located by name in ghilana's search, whose output was too long to read here) [I; (-5, 80, 5) is a position 80 m toward the camera's side, so effectively a directional light from the camera's side].
- Whether a hero loaded from an older save without the portrait fields gets a record, and what the record's defaults are (the creature's ctor, 0x006B9F39, leaves the attribute's pointer null; where it is allocated was not found).
- Whether the model's front is +Y at yaw 0 for every model (taken from the facing-vector convention and DAOTurnBased's play).
- The save-to-record mapping of the field names (only the values were matched).
- The head node's position in the head's mesh (the face's centre is above the bone's origin by an amount not measured); the peer's actual save and hero.
- No game was run and Ghidra was not used.

### Portrait clone: does its head turn, what frames it (lib-agent-2, read 2026-10-19, prefix portclone)

Marks: [C] checked in the exe's disassembly or in data, [I] inferred. GOG VAs. Scratch: %TEMP%\lib-agent-2-portclone-* (decoded clip, skeleton, swf xml).

#### Short answers

1. **The vanilla hero's ring does not use the Default row at all.** Render (0x007F46E0, 0x007F4950-0x007F4A14) passes the source creature's OWN portrait camera (creature +0x598 block) to PlaceCamera when the creature's tag is `paperdoll` (string at 0xAECC9C, global 0xC3907C) or `player` (0xAECCB0, global 0xC39060) [C]. The hero's tag is `player`. The block is saved with the game: GFF4 fields SavegamePlayerPortraitPitch/Yaw/Tint/Expression/Distance/PositionH/PositionV (16332-16338, dev/gff4_fields.csv:1889-1895). Real saves in this install, read with tools/gff4.py [C]:
   - Lyna (made in character creation; chargen copies the Default row into the block, dev/CHARGEN.md:112): pitch -7.5, yaw 23.5, distance 0.675, H 0.007, V 0.038, tint 1, expression 0 (= the Default row; this hero looks 23.5 degrees turned).
   - Leliana and HurlockVanguard (heroes made without the appearance scene): pitch 0, yaw 0, distance 0.6, H 0, V 0, tint 1, expression 0.
   So a hero whose save holds 0/0/0.6/0/0 is frontal (no rotation), 12.5% closer (0.675/0.6, a face 12.5% larger linearly) and, with V = 0 instead of 0.038 and pitch 0 instead of -7.5, sits a little higher: exactly the symptom the peer sees. Nothing "cancels" the rotation; the vanilla hero it compared against had yaw 0. Ask the port's source: which save fields does the hero have? The port should read the hero's SavegamePlayerPortrait* and pass them through the same mapping as below. (Which save the peer's vanilla screenshot came from was not known to me; the fix is to compare the same save. If that save says 23.5 the ring would be turned in vanilla too.)
2. **The clone's head does not turn by animation or look-at.** At the pose Render sets (the `portrait_exp_m/_f` clip at Expression 0) the head, neck and chest rotations are the model's bind pose; no look-at controller runs on the clone [C for the pose, I for "no look-at": nothing in the renderer ticks the clone, see below].
3. **Nothing else in the renderer sets the clone's scale, LOD or head framing.** The camera is the only framing. The picture is 146x146, shown by portraits.swf in a 72x72 px circle (a circular mask the same size and place as the placeholder), so it is scaled to about 0.49 and the corners cut by the circle; no zoom, no crop of the middle [C].

#### Q1. The clone's pose, animation, look-at

- The clone is a full `CCCreature` embedded in the renderer at +0x20 (constructed by 0x006B9ED0 with id -1; renderer ctor 0x007F3D80, `lea ecx,[esi+0x20]` at 0x007F3DB6) [C]. Setup (0x007F3E20, once per renderer) gives it an animation host (0x00694DB0(8) at 0x007F3FFC, creates the host 0x00A61770(8)), a mode (0x00695220(0xE) = host +0x15C set to 14, 0x00A5CAE0), vtable +0x144(0) and registers it in the world's by-kind list (0x0064B240 on [0xC327EC]+0x1AC) [C that these are called; the meaning of 8, 0xE and +0x144 not read].
- The only animation the renderer ever starts is **0x2A5 (677) or 0x2A6 (678)** [C]. anim_base (ghilana, table "anim") rows 677 `Portrait Expression - Male` AnimName `portrait_exp_m`, 678 `Portrait Expression - Female` `portrait_exp_f`, Type 0, Priority 0, Replacement 1, DisableLookAt 0 [C]. Female when the appearance record's byte +0x50 is 2 (0x006A3650) [C]. The clip behind them is `default.hm_exp.ani` / `default.hf_exp.ani` (global.rim and anims.erf; my identification by the name pattern, the .anb link not read [I]).
- That clip, decoded (tools/gff4.py --dump, and Clip.h's 32-bit smallest-three decode) [C]: **AnimLength 0.1333 s, five keys at 1/30 s steps (key times 0, 16383, 32767, 49151, 65535 of 65535)**, 151 channels (face bones: brows, cheeks, lips, jaw, lids, eyes; plus head, neck1, chest, god, legs and arms). Key 0 equals the bind pose of the neck, head and chest: neck1 z 0.082 (bind in hm_uhm_basa_0.mmh: 0.083), head z 0.106 (bind 0.106), chest y 0.077 (bind 0.077), god identity. Keys 1-4 are other expressions with head rotations of 9-25 degrees (head quaternions: key1 (-0.117,-0.010,0.140,0.983), key2 (0.03,0.039,0.06,0.997), key3 (0.001,-0.024,0.214,0.977), key4 (0.066,-0.042,0.11,0.991)).
- **The time picked is the Expression value**: `SetTime(animId, t)` 0x00A5C980 (-> host vtable +0x78) with t = the source's block +0x7C (the Expression, 0 for the Default row, 0 in the saves above) [C: 0x007F4915-0x007F4933 and 0x007F4650-0x007F4698; the global 0xC39058 compared with 0 is never written, so it is the +0x7C path]. So at Expression 0 the clone is in key 0 = bind pose, face neutral, head not turned relative to the body [C data, I that SetTime at t = 0 samples key 0]. A chargen Expression above 0 would scrub through the five poses (seconds, 0 to 0.1333) [I for the units].
- Played with `PlayAnimation` = CCCreature vtable +0xB0 (0x006AA7E0): args (id, 0.0, -1.0, 1) (the float at 0xAE2E00 is -1.0) [C]: the speed/advance argument 0.0 and duration -1 are the same shape ANIMATION.md gives the look-at clips (do not advance, no end) [I]. The clip is held at the time SetTime sets; nothing advances it.
- Two entry points: `Render(p1=0)` plays it unconditionally (0x007F48D3-0x007F4933). `Render(p1=1)` (UpdatePortrait, the rebuild) calls the refresh 0x007F4440: it rebuilds the clone's model from the source's appearance record (0x007F41D0 copies the record field by field from the source +0x7B0), re-applies the same host setup, builds the model (0x006C1E00 on the clone), and plays 0x2A5/0x2A6 and SetTime **only if** the appearance's word +0x58 equals +0x5A (appearance unchanged) and 0x006A25A0 is true (the appearance table's string column 0xDE80B418, hash read by 0x00A2CC40 from table 2, equals the narrow string `combat_humanoid` at 0xB0F318) [C]. Otherwise (a creature that is not a combat humanoid: ogre, dog, spider) the clone plays no portrait clip and stands in its model's default pose [C that nothing plays; I that the default is the bind/idle].
- **Look-at**: the look-at object is lazy (`CCCreature::GetLookAt` 0x0069E9D0 at +0x768) and is driven by `UpdateLookAt` 0x0069EAF0, called from only two places (0x006BD575, the creature's update, and 0x00765898) [C by scanning every E8/E9 in .text for these targets]. The renderer never calls the clone's update; it only sets facing (SetOrientation), plays, SetTime, builds the model and draws it through the scene (the dirty flag at scene +0x11, 0x007F4A19). So no look-at object exists on the clone and nothing would move the head toward the camera [I: absence of calls in the renderer's functions; the call list of 0x007F3130-0x007F4A44 contains no update, tick or look-at function]. Even the expression clip does not key `eyelookat`/`eyelookbackat` toward a target: they are constants in the clip [C names, values not decoded].
- **Facing**: `CCObject::SetOrientation` (0x00694CE0) stores the vector at +0x130 and calls vtable +0x80 = 0x006A0190 -> 0x006955C0, which hands the vector to the animation host 0x00A5CBE0 (yaw = `Vector::ToAngle` 0x004EE570 = atan2(-x, y) in degrees, so (0,1,0) is 0 degrees and (sin(-r), cos(-r), 0) is exactly r degrees; a quaternion built with `FromYawPitchRoll(yaw, pitch, roll)` and set on the model's root node immediately, no turn rate) and to the +0xA8 member (vt+0x54 with the same heading) [C]. `FromYawPitchRoll` 0x004EE200 (degrees in): the 2nd argument is a rotation about X (the portrait pitch), the 1st about Z (the yaw), the 3rd about Y [C: axis constants (1,0,0)/(0,0,1)/(0,1,0) derived from the static init at 0x004EE236-0x004EE29A, third mapped by the order of the products]. So yes: the clone's body (and with it the face) is turned by exactly CameraRotation about the vertical, and the camera is on the +Y side of the head; with rotation 0 the face points at the camera. That matches the earlier PlaceCamera reading; the missing piece was that the hero's rotation is not the Default row's.
- A head-tracking flag, a "portrait mode" flag or a facing-from-the-camera on the clone: none found [C within the renderer's functions: ctor 0x007F3D80, 0x007F3740, setup 0x007F3E20, 0x007F41D0, 0x007F4440, Render, PlaceCamera]. The one per-creature flag the renderer sets is the fade/visibility pair `[clone+0x18C]=[clone+0x18D]=1` with 1.0 via 0x006951D0 (a fade-in of 1.0, i.e. fully shown) [C].

#### Q2/Q3. What frames the face, and the displayed picture

- **PlaceCamera's table forcing** (0x007F3175-0x007F31D1 [C], matches Addresses.h:1675-1681): `0x004C0E50` is "strings differ" and `0x004C0DE0` "strings equal" (both read: `sbb/neg` forms at 0x004C0E02-0x004C0E04 / 0x004C0E72-0x004C0E76). Table forcing (every value read from table 7 by appearance) applies when the tag differs from `paperdoll` AND (the appearance word +0x58 != +0x5A, OR (renderer row +0xD48 == 1 AND appearance != 15 AND the tag differs from `player`)). For the hero (tag `player`): forcing happens only when its appearance has changed since the last record (+0x58 != +0x5A, e.g. after an armour/appearance change that updates the pair); otherwise the hero's own block is used. The port should keep both paths; a hero whose appearance changes is, for that render, framed by the table's row for its appearance or the Default row.
- **Render's argument mapping for the hero** (0x007F499E-0x007F4A14) [C]: pitch = block+0x58, rotation = +0x5C, distance = +0x60, Z adjust = +0x68 (V), X adjust = **-1.0 * (block +0x64)** (H; the multiplier is the double at 0xAF0E48, which reads -1.0, so earlier notes' "0.017453" was wrong), lights = unset (65505.0, 3 values), so the lights come from the table row `renderer+0xD48` (Default 1) [C]. Save field to block: Pitch +0x58, Yaw +0x5C, Distance +0x60, PositionH +0x64, PositionV +0x68, Tint +0x6C, Expression +0x7C [I from the order of the save fields and the argument order].
- **Block's chargen ranges**: Look up/down -30..30, turn -45..45, distance 0.5..0.72, move left/right +-0.036, up/down -0.02..0.1 (dev/CHARGEN.md:255).
- **0x007F3740** (run from the renderer ctor) only sets the block's tint (+0x6C) and Expression (+0x7C) from the table row it finds (for tag `player` it searches table 7 for a row whose Tag column 0x88578372 equals the creature's tag; else row 1) [C: 0x007F3785-0x007F3914]; it never writes the camera fields. So Tint and Expression of the hero are overwritten from the table at each renderer creation (the Default row: 1 and 0), but pitch, yaw, distance, H and V stay as saved.
- **The scale of the clone**: nothing scales the clone; the camera distance is the only zoom (`D = (distance + [0xC39054 = 0.0]) * 30 / 10`, FOV 10 degrees, near 0.15, far 40; from lib-agent-2-placecam.md:23, :35). No LOD choice for portraits was found in the renderer's functions [C within them; the engine's general model LOD set by host +0x15C = 14 at Setup is a possible portrait LOD mode, the host method 0x00A67A20 not read].
- **The displayed picture** (swf from lib-agent-2-guicmd-x\allgfx\portraits.swf, converted with FFDec -swf2xml, scratch portraits.xml) [C]: `portrait_mc` (sprite 115, the party bar's) has the mask shape 109 (clipDepth 12 covers Portrait_mc and the alert clips) and `Portrait_mc` (sprite 111, the placeholder, scale 1.0003). Shape 109 (the mask): a circle, bounds x 440..1880 twips, y 400..1840 = 72 x 72 px at (22, 20) px. Shape 110 (the placeholder inside sprite 111): a circle, 1440 twips = 72 x 72 px, placed at (22, 20) px. `Picture_mc_init` (Portrait.as:133-141) takes `nWidth = Portrait_mc._width`, `nHeight = Portrait_mc._height` (about 72.03, from the 1.0003 scale; the URL formats them as numbers) and loads `img://<name>.dds?Width=72.03&Height=72.03` at 100% scale into it (MovieUtils.LoadImage, MovieUtils.as:215-224; GetImageUrl :152-176). The 146 x 146 texture is therefore shown at about 72 x 72 px (a factor 0.493) and the circle (diameter 72) clips its corners only: **the picture is not enlarged or cropped toward the face**. The summon portrait (`portrait_sum`, sprite 237) uses scale 0.5610 on the same shapes, the same logic [C].

#### Pseudo-code a port can copy

```
// per portrait render of creature C (source), clone K at the scene origin
tag = C.tag
if tag in ("paperdoll", "player"):                   // equality, case-sensitive as the string compare
    cam = C.portrait_block                           // hero: saved SavegamePlayerPortrait* (pitch, yaw, distance, H, V)
    args = (pitch=cam.pitch, rot=cam.yaw, dist=cam.distance,
            zadj=cam.V, xadj=-cam.H,                  // the -1.0 multiplier
            light=unset)                              // lights from the table row (Default row 1 unless C.tag has one)
else:
    args = all unset                                  // read from table 7: row by Tag, else Default (1), or by Appearance
if table_forced(C): args = table row by Appearance    // tag != paperdoll and (appearance changed or (row==1 and appearance != 15 and tag != "player"))

if K needs a rebuild: K.appearance = C.appearance; K.equipment = C.equipment
if appearance_unchanged and C is a combat humanoid:   // table 2 col 0xDE80B418 == "combat_humanoid"
    K.Play(female ? 678 : 677, speed=0.0, duration=-1.0, restart=1)   // portrait_exp_f / _m
    K.SetTime(id, C.expression)                      // seconds in 0..0.1333; Default row 0 -> key 0 = bind pose
// no look-at, no update tick on K: K stays in that pose
K.SetOrientation(sin(-rot), cos(-rot), 0)             // yaw = rot degrees about Z, immediate
head = K.node("head").world_position                  // HeadTransform(0,0,0,1); scene origin if no head node
D  = (dist + 0.0) * 30 / 10
R  = Rx(pitch) * (0, D, 0)                            // camera offset on the +Y side
cam.pos = head + R + right*xadj_scaled + up*zadj      // see lib-agent-2-placecam.md:36-38 for the right/up axes and the unreconciled X/Z slot order
cam.fov = 10 deg, near 0.15, far 40
display: texture 146x146 -> 72x72 px circle in the movie
```

Numbers: the Default row (table 7, ID 1): pitch -7.5, rotation 23.5, distance 0.675, Z 0.038, X 0.007, light -5/80/5. A hero with a default save (Leliana, HurlockVanguard): pitch 0, yaw 0, distance 0.6, X 0, Z 0, tint 1, expression 0, lights from the Default row (-5/80/5). Hero from chargen (Lyna): the Default row's camera values, with X = -0.007 (the -1.0 multiplier).

#### Evidence index

- Render 0x007F46E0 (disassembly /tmp/lib-agent-2-portclone-render.txt, regenerate with `python tools\daoexe.py --disasm 0x007F46E0 400`): the tag test 0x007F494F (`push 0xC3907C`; `call 0x4C0DE0` equality; `jne 0x7F499E`) and 0x007F4969 (`push 0xC39060`), the branches 0x007F4977 (all unset) and 0x007F499E (the block).
- Tag strings: static initialisers 0x00AD3550 (`push 0xAECC9C` = "paperdoll", `mov ecx,0xC3907C`) and 0x00AD3570 (`push 0xAECCB0` = "player", `mov ecx,0xC39060`), wide strings read at the VAs.
- Hero tag `player`: dev scripts (toolset\extract\scripts\Landsmeet (Denerim)\_Includes\den_functions_h.nss: `if (sTag == "player")`, "If oFollower is the player "player" is substituted for (tag of party member)") [C text; the creature's actual tag in a save not read].
- Saves read: `C:\Users\tianx\OneDrive\Documents\BioWare\Dragon Age\Characters\{Lyna\Saves\AutoSave_1, Lyna\Saves\PostCampaign_1, Leliana\Saves\Slot_1, HurlockVanguard\Saves\Slot_1}` with `python tools\gff4.py --dump FILE --depth 3 --items 3 | grep PlayerPortrait`.
- Animation rows: ghilana `table anim id 677,678`. Clip: ghilana extract of default.hm_exp.ani / default.hf_exp.ani (16330 / 16382 bytes), decoded (scratch lib-agent-2-portclone-x\dump.txt); bind from hm_uhm_basa_0.mmh (scratch skel.json: Neck1 z 0.083, Head z 0.106, Chest y 0.077, Neck (-0.704,-0.063,-0.063,0.704)).
- Slot addresses: CCCreature vtable 0x00B10484 +0xB0 -> 0x006AA7E0 (PlayAnimation), +0x80 -> 0x006A0190, +0x144 -> 0x0069F180, +0x104 -> 0x00696F00; the +0x70 interface vtable 0x00B1035C +0x60 -> 0x0069A000 (returns the animation host `[this+0xE4]`).
- Prior notes cited: lib-agent-2-placecam.md:7-13, :23, :33-41 (camera maths, unchanged); lib-agent-2-gui3-portrait.md:19-20, :35-37 (Render shape; the "chargen tags" reading is refined here to paperdoll + player); dev/CHARGEN.md:112, :255; dev/ANIMATION.md:728-757 (look-at; GetLookAt 0x0069E9D0, update 0x006CAD80); C:\project\DAOTurnBased\dev\UI.md:67-81 (the rotation turns the view round the creature, consistent); Addresses.h:1675-1685 (forcing rule, consistent with this reading).

#### New versus prior notes

- The hero's ring is framed by its saved camera, not the Default row (the central finding).
- The X adjust multiplier is -1.0, not 0.017453.
- The 677/678 clip, its length (0.1333 s), poses, key 0 = bind, and that the time is the Expression value.
- No look-at, no tick on the clone; FromYawPitchRoll's axes; the displayed picture is a 72 px circle with no crop of the face.

#### Not read

- The Attribute at creature +0x598: how its target object (`Get()` = 0x0053CC40 returns `[this+0xC]`) is created and loaded from the save fields, and whether load maps the 7 save fields onto +0x58..+0x7C exactly as I listed (inferred from order). The default when a save lacks them.
- Which save the peer's vanilla screenshot came from; its hero's tag in a save.
- The `Attribute::Set` path that the sliders use and 0x0069ED50's use (Render(0,1,1)).
- Host methods 0x00A5CAE0/0x00A67A20 (mode 14), vtable +0x144 / +0x104 of the clone, the by-kind list at 0x0064B240; whether they enable any LOD or tick for the clone.
- Whether the engine's per-frame model update (skinning, animation host update) runs for the clone when the scene draws, and so whether SetTime's pose is applied that frame (it must be, since vanilla shows the neutral face; the host update was not read).
- The .anb that maps AnimName `portrait_exp_m` to `default.hm_exp.ani` (named by pattern).
- The ghilana table for the scene's lights/camera objects (strings 0xB24714/0xB246EC/0xB21D6C/0xB21D1C), the render target's format and clear alpha, and the SetTime unit for Expression above 0.
- Game not run; Ghidra not used.

### The portrait's lights: what PlaceCamera does with LightX/Y/ZAdjust, what a figure's shader makes of them, and the ambient (lib-agent-2, portlight, read 2026-10-19)

Marks: [C] checked in disassembly or data, [I] inferred. GOG VAs. Scratch: %TEMP%\lib-agent-2-portlight-* (ch1.asm and char1.asm are `fxc /dumpbin` of vanilla's `ch1.psh` and `Char1.psh`'s vertex shader `Char1.vsh`, extracted with ghilana from shaders.erf, the vanilla copies, not DAOElgaraShaders' overrides).

#### Short answers

1. **The adjust is a POSITION, not angles.** The two objects (renderer +0x10 the rig, +0x14 the probe) are `GraphicsStructures::Object`s (the IBaseObject interface at Object+8). PlaceCamera's virtual +0x34 is a position getter (0x0054F8C0 -> 0x0054F6F0, copies the 4 floats at iface+0xF8 out) and +0x10 the position setter (0x00543470, copies a 4-vector into iface+0xF8, sets the dirty bytes +0xE4/+0xE5) [C]. PlaceCamera calls get into a stack vec4, overwrites x, y, z with (LightXAdjust, LightYAdjust, LightZAdjust) (w kept), then set [C]. That is `rig.position = (lightX, lightY, lightZ)` in the portrait scene's own frame: the clone stands at the scene origin, so it is "relative to the clone's feet", NOT to the head, the camera, or turned with the clone [C for the code; I that the Object's world matrix is that position with identity rotation: the sibling SetPosition(vec3) at slot 5, 0x00543590, calls this one with w = 1.0, which is a position's form, and the table's radii of 195-200 m only make sense for a rig set 80 m away]. The same vector goes to the probe object. The six lights are at the rig's position plus each node's own local translation (the rig's GOB root and every node have identity rotation, read in the .mmh), so for the Default row (-5, 80, 5) the **three figure lights are at (-5.3, 77.5, 8.5), (-3.2, 82.4, 6.45), (-7.2, 82.0, 5.0)**: some 78-82 m out on +Y, the camera's side, behind the camera, 4-8 m above the floor. Seen from the head (about z = 1.6, not read) they are all within ~5 degrees above and ~2-5 degrees toward -X of the camera's own direction: a nearly frontal light from behind the camera, not a key, rim and fill (the rig's own local offsets of 2-4 m are nothing at 80 m). The "rim" is no rim.
2. **Falloff is cos^2(pi/2 * d / radius), times N.L, no inverse square, no linear** [C, ch1.psh]: `d = max(|L - P|, 0.001)`, `t = saturate(d / radius)`, `falloff = cos(t * pi/2)^2` (the shader writes it as frc/sincos of `t/4 + 0.5`, which is exactly `t * pi/2`). At 78-82 m of a 195-200 m radius it is 0.62-0.67 (not 1): KEY 0.669, RIM 0.625, FILL 0.620. The colour is `node colour * flicker (+0x1BC) * intensity (+0x1B4) * fade`; the node constructor (0x0053CCA4) sets +0x1B4, +0x1B8, +0x1BC all to 1.0 and the nplt data has no intensity field, so **intensity is 1.0 and the colour is the .mmh's** (1.25, 1.10, 1.00 is above 1 and stays so) [C ctor; I that the instance maker does not change +0x1B4]. The fade is min(1, 4 t), full after 0.25 s.
3. **Only the three AffectDomain-1 lights light the clone**, all three in the figure's "static" group (IsStatic 1, +0x6C list, capacity 3), registers [0]/[1], [5]/[6], [7]/[8] (shader booleans b1, b2 for the second and third; the first is unconditional) [C, EFFECTS.md:1028-1080 and ch1.psh]. The three domain-2 lights (KEY_L, RIM_L, FILL_L) go to the level's lists and never reach a figure (the level lists are `ChooseForLevel`'s) [C table, I that the clone is chosen for as a figure, which it is by being a CCCreature model]. **The ambient is the surprise: the portrait probe's inline irradiance arrays are all zero, and the engine reads the inline arrays, not the .mtx, so the figure has NO probe ambient.** The loader (0x0058E6CA..0x0058E8E8) asks for field 0x18C0 (MMHLightProbeIrradianceRed): present (even all zeros) -> the three 16-float arrays go into the blueprint at +0x160/+0x1A0/..; absent -> only the file name (field 0x186D) is stored and no matrices [C branch at 0x0058E725-0x0058E727]. The portrait's `nirr` struct carries the three arrays (zeros) and the name rmtst_probe_3915.mtx, so the engine takes zeros [C data and branch; I that the blueprint's matrices are what `LightState::Fill` copies to g_mLightParams (it copies matrices got by 0x0053C8F0 and 0x0053C900 from the probe node, 0x0046B77B-0x0046B7B7)]. A level's own probe (amn100d_ll.mmh) has real inline numbers; that is why the .mtx looks like "the" irradiance and is not. **So a port that takes ambient from rmtst_probe_3915.mtx (a cool, blue-topped probe: at +Y 0.30/0.27/0.30, at +Z 0.51/0.59/0.77) lights the shadow side of the face, blues it and flattens it: the opposite of vanilla's warm, contrasty ring.** Other ambient terms: `g_vLightParams[4]` (the ambient lights' sum, added to the vertex's ambient, Char1.vsh `add o2.xyz, r0, c217`) is the zero vector from `LightState::Fill` plus ambient lights; the rig has none (the rig's nodes are the six `nplt`), so zero [C for the shader and Fill's reset of +0x80 to (0,0,0,1); the base passed in as Fill's argument is "zero in every case read" (DAOLib Lights.h) and not read for this scene]. The sun ([2]/[3], boolean b3): not written by `LightState::Fill`; the portrait scene's Setup (0x007F3E20) gives the scene no sun, fog or sky that I could find, so I take it off [I; Not read]. The probe's reflection cube (rmtst_probe_3915.dds, an indoor stone hall with two blue windows) is still read, for the specular only. **No frame effect, tone map or grade is applied on the portrait target** [I: the frame effects run at the frame's end over the back buffer and its copy (FRAME_EFFECTS.md:18, 36-38), and the portrait draws into its own 146x146 target; ch1.psh itself clamps with `mul_sat` and has no tone map]. A gamma/sRGB difference: not read (the format passed to the target's create is 3, undecoded, GUI.md:1127).

#### What a port can copy

```
// per portrait render, in the portrait scene's frame (clone at the origin, +Z up)
rig_pos   = (LightXAdjust, LightYAdjust, LightZAdjust)        // the table row's, every call, set on rig and probe alike
lights    = [ (colour, radius, local) ]  // AffectDomain-1 nodes only, from el_lr_new_day2.mmh
           [ KEY_C  (1.25,1.10,1.00) r 200 local (-0.3,-2.5, 3.5),
             RIM_C  (0.52,0.54,0.84) r 197 local ( 1.8, 2.4, 1.45),
             FILL_C (0.60,0.40,0.30) r 195 local (-2.2, 2.0, 0.0) ]   // world = rig_pos + local (identity rotation)
// (KEY_L, RIM_L, FILL_L are domain 2: level meshes only, not the figure)

ambient   = 0                      // NO probe irradiance: the portrait probe's inline matrices are zero
                                   // (do not read rmtst_probe_3915.mtx); g_vLightParams[4] = 0; no sun

// per pixel, ch1.psh (Char1.vsh gives v6 = world position, v5 = view vector, v0 = ambient = 0)
for each light i (all three, none sorted away):
    d     = max(length(Lpos_i - P), 0.001);  L = (Lpos_i - P) / d
    fall  = cos(saturate(d / radius_i) * pi/2) ^ 2
    NdotL = max(dot(N, L), 0)                    // below 0: both diffuse and specular are 0
    H     = normalize(V + L)                     // V from the surface to the eye
    diff += fall * NdotL * colour_i
    spec += fall * NdotL_gate * pow(max(dot(N, H), 0), gloss) * colour_i
gloss  = specmask.a * 255 + 1                    // the highlight's power
spec   = spec * 3 + texCUBElod(probe_cube, reflect(-V, N), lod = 9 * (1 - gloss / 255)) * mml_fSpecularReflectionMult
out    = albedo * (diff + ambient) + spec * specmask.rgb * vertexEdge     // vertexEdge = Char1.vsh's o7.w, the material's falloff at the edge
out    = saturate(out)                            // then the fog lerp with a clear fog: no change [I]
```

(`NdotL_gate`: the shader's `cmp` zeroes the specular too when N.L < 0, so the specular term needs N.L >= 0.)

##### The Default row (-5, 80, 5), head at (0, 0, H), H taken as 1.6 only to put numbers on it (not read)

| Light | World position | To the head: vector, length | Direction | falloff | colour * falloff (before N.L) |
|---|---|---|---|---|---|
| KEY_C | (-5.30, 77.50, 8.50) | (-5.3, 77.5, 6.9), 78.0 | (-0.068, 0.994, 0.089) | 0.669 | (0.836, 0.736, 0.669) |
| RIM_C | (-3.20, 82.40, 6.45) | (-3.2, 82.4, 4.85), 82.6 | (-0.039, 0.998, 0.059) | 0.625 | (0.325, 0.338, 0.525) |
| FILL_C | (-7.20, 82.00, 5.00) | (-7.2, 82.0, 3.4), 82.4 | (-0.087, 0.995, 0.041) | 0.620 | (0.372, 0.248, 0.186) |

Sum at N.L = 1: (1.53, 1.32, 1.38), red 1.00 : green 0.86 : blue 0.90 (the key's warmth carries it; the "rim" blue is lost in it). The directions differ by under 3 degrees, so across a head 20 cm wide the light is a single parallel one from the camera's side, 5-6 degrees above and 4 degrees to the camera's right (camera right = -X, camera.md). Against a face turned 23.5 degrees to -X (facing (-0.399, 0.917, 0)): N.L at the face's front for the key = 0.938; the far cheek (normal turned toward +X) loses, the near one (toward -X) gains: the contrast is the face's own shape lit from the front, with black where N.L < 0, because there is no ambient to fill it. Rows with other values move the rig and so the direction: Morrigan (3, 45, -55) lights from below; Wynne (-1.5, 1.75, 0.5), Sten (0, 10, -3) and Oghren (-5, 2.5, 2.5) put the rig within a few metres, where the local offsets of the rig do matter (the falloff there is ~0.99 and the directions swing by tens of degrees); Dog (1, 95, 15), Shale (0, 95, 25) and Leliana (25, 85, 17) are the 80-95 m kind.

#### Evidence

Prior work used (cited): %TEMP%\lib-agent-2-placecam.md:40-41 (the 0x007F363C-0x007F3699 calls, lights at renderer +0x10/+0x14, vt+0x34 then vt+0x10); %TEMP%\lib-agent-2-portrait2-camera.md (camera at head + (-0.007, 2.013, -0.227), right = (-1,0,0), "the adjust is a position" was its [I]); master dev/GUI.md:1130, :1248, :1323 (the "Not read" this note settles: the classes and the two virtuals); master dev/EFFECTS.md:1028-1080 (AddLights' sort, ChooseForMoving, capacities, registers); DAOElgaraShaders dev/NOTES.md:7 and :13 (a figure's shader and its registers; "portraits ... a scene of their own with their own light manager and three lights of some 200 m"); DAOElgaraShaders shaders/effect_lights.hlsl:117 (the same cos falloff in its own code); DAOLib src/dao/Lights.h (the ambient register [4]). New here: the object class and its slots, the loader's branch, the shader's maths, the probe's data.

##### The objects and the two virtual calls [C]
- Setup 0x007F3E20: `[0x00C31650]` (g_graphics) vt+0x80 (0x00527EF0, which forwards to the GraphicsStructuresManager, vtable 0x00AFA474, whose vt+8 = 0x005784A0 makes the object) is called twice, 0x007F4064 and 0x007F40CD, with the strings 0x00B21D6C and 0x00B21D1C; the results go to renderer +0x10 and +0x14, then `ISceneInstance::AddObject` (0x0053A260), which subtracts 8 from the pointer (0x0053A270 `add edi, -8`): so the pointer is Object+8, the `IBaseObject` interface (vtable 0x00AF8434, 95 slots, of `GraphicsStructures::Object`, ctor 0x0054F9A0 whose last stores are 0x00AF85DC at +0, 0x00AF8434 at +8).
- Its slot 13 (+0x34) is 0x0054F8C0 = `sub ecx, 4; jmp 0x0054F6F0`: copies 4 floats from [ecx+0xFC..0x108] (iface+0xF8..0x104) to the caller's pointer. Slot 4 (+0x10) is 0x00543470: copies 4 floats from the caller into iface+0xF8, sets +0xE4 and +0xE5 to 1, and, when a child object at +0x1B4 exists, passes it the moved distance through its vt+0x3C. Slot 5 (0x00543590) takes a vec3 and calls slot 4 with (x, y, z, 1.0). Slot 14 (0x0054F8D0 -> 0x0054F720) reads the 4 floats at iface+0x118 (the rotation quaternion; identity by default).
- PlaceCamera's use: 0x007F3606 `mov ecx, [esi+0x10]`, 0x007F363C `call vt+0x34` with the buffer at [esp+0x70]; the second object at +0x14 with the buffer at [esp+0x80] (0x007F3646-0x007F364E); then 0x007F3650-0x007F367B write (lightX, lightY, lightZ) = [ebp+0x1C], [ebp+0x20], [ebp+0x24] into x, y, z of both buffers, and 0x007F3687 / 0x007F3699 `call vt+0x10` on each.
- The rig's nodes (ghilana, el_lr_new_day2.mmh, patch002.erf copy): GOB root with trsl (0,0,0,1) and rota identity; nplt nodes with the colours, radii, locals and AffectDomain the task gives; IsStatic 1; IntensityVariation 0; Version 0; CanBeOccluded 0; UseVariationTint 0 (so the colour is not tinted).
- Node constructor 0x0053CCA4 (vtable 0x00AF7ACC at +0): +0x1A0 colour, +0x1B0 radius = 0; +0x1B4 = +0x1B8 = +0x1BC = 1.0 (0x0053CCFB-0x0053CD09). EFFECTS.md:1069-1080's register formula then gives `colour * 1 * 1 * fade`.

##### The shader [C, ch1.psh.fxo of shaders.erf, 4,496 bytes, fxc /dumpbin; listing in %TEMP%\lib-agent-2-portlight-ch1.asm]
- Registers: g_vLightParams c0..c11 (13 entries; [i] at c_i), the four lights' blocks as `add r, c0, -v6; ...; max d, 0.001; mul_sat t = d * rcp(c0.w); frc(t * 0.25 + 0.5) * 2pi - pi; sincos; mul x, x` (= cos^2 of t*pi/2). Light 0 (c0, colour c1) is unconditional; light [5] (c5/c6) is under `if b1`; [7] (c7/c8) under `if b2`; [9] (c9/c11) under `if b0` (the dynamic light); the sun ([2]: direction, [3]: colour) under `if b3`.
- Per light: `L = normalize(Lpos - v6)`, `H = nrm(v5/|v5| + L)` (v5 = the view vector), `NL = dp3(N, L)`, `NH = max(dp3(N, H), 0)`; `cmp` zeroes both NL and NH when NL < 0; diffuse += fall * NL * colour; spec += fall * pow(NH, gloss) * colour. gloss = specmask.a * 255 + 1 (c31.w = 255, c31.z = 1).
- Composition: `r3 = diffuse_b0 + v0` (v0 = the vertex's ambient), `r3 += other diffuse`; `r4 = (spec_b0 + spec_rest) * 3 + cubemap_lod * c28.x` (c34.y = 3; c28 = mml_fSpecularReflectionMult; lod = 9 * (1 - gloss/255)); `out = albedo * r3 + r4 * specmask * v5.w`; then `mul_sat` against 1 - tint (mml_vTintColor), then `out = c24 + max(v7.x, c25.x) * (out - c24)`, the fog lerp. oC1 is a second target (a distance). No tone map, no gamma operation in the shader.
- Vertex shader Char1.vsh (listing %TEMP%\lib-agent-2-portlight-char1.asm): v0 (`o2`, ambient) = `[n, 1] M_c [n, 1]` per colour from g_mLightParams (c201..c212: three 4x4 matrices) times `(1 + an edge term of mml_vFalloffParams and the eye's angle)`, plus g_vLightParams[4] (c217); n is the skinned world-space normal. So the ambient is zero when the three matrices are zero and [4] is.

##### The probe [C data, I for the last link]
- el_lp_outside_day.mmh: nlpr "outside_day" with child nrfl EnvLightProbeEnvMap rmtst_probe_3915.dds and nirr MMHLightprobeIrradianceRes rmtst_probe_3915.mtx with MMHLightProbeIrradianceRed/Green/Blue = 16 zeros each.
- The blueprint reader (the function containing 0x0058E6CA-0x0058E8E8): `0x4D54D0` finds field 0x18C0 in the nirr struct, `0x4D5230` returns its value or 0; 0 jumps to 0x0058E846 (store the file name of field 0x186D via 0x0055FCB0 and nothing else); non-zero reads 0x18C0, 0x18C1, 0x18C2 through 0x0058C9D0 into 16 floats each and stores them at node +0x160.. +0x1F0.. (the first of the three at 0x0058E75F-0x0058E799, the third at 0x0058E820-0x0058E844). Probe node's instance getters 0x0053C8F0 (red) and 0x0053C900 are copied by Fill to the state's +0x130/+0x170/+0x1B0 (0x0046B77B-0x0046B7B7); Fill skips the copy when the probe is the one kept at state +0x110.
- amn100d_ll.mmh (a level's own probe, ghilana): both the name and non-zero inline arrays: the inline path is the normal one.
- rmtst_probe_3915.mtx (lightprobedata.erf, text): R, G, B 4x4 matrices; evaluated at n = (0,1,0): (0.302, 0.265, 0.299); n = (0,0,1): (0.507, 0.586, 0.773) (a cool, blue-topped hall). That is what a port gets if it reads the .mtx and not vanilla's zeros.

#### Checked / inferred
- Checked: the two virtual slots and their bodies; the Object class; the get-modify-set; the rig's data; the falloff formula, the specular, the ×3, the lod, the composition and the clamp in ch1.psh; the node constructor's intensity 1.0; the probe's data and the loader's present/absent branch; the vertex shader's ambient term.
- Inferred: that the Object's position is its world translation with identity rotation (strongly supported, see answer 1); that the three domain-1 lights are all chosen (no zero score: d is under the reach, the nodes have no room); that the instance maker leaves +0x1B4 at 1.0; that Fill's matrices are the blueprint's inline ones (the getters' bodies not opened); that frame effects do not touch the portrait target; that the portrait scene has no sun or fog.

#### Not read
- The head node's height in a hero's clone (H is 1.6 only for the table); the head's x and y.
- Where the sun registers [2]/[3] and the boolean b3 come from, and whether the portrait scene gives them anything; the fog parameters mml_vFogParams the portrait scene hands the shader (v7.x, c24, c25); Fill's incoming base ambient for this scene.
- The 0x4D54D0 / 0x4D5230 pair's own bodies (taken as "find field" and "its value or 0" from their use).
- The render target's pixel format (3, undecoded) and any sRGB-write state of the character material's states (`Character.sta`); the texture sampler states (sRGB reads) of the decal.
- The edge term of Char1.vsh (mml_vFalloffParams) in numbers; mml_fSpecularReflectionMult's value for the skin and armour materials.
- No game was run, Ghidra not used; nothing seen in play.

## gui4a: the quickbar's length, and a quickbar slot's token (lib-agent-2, read 2026-10-18)

Read in the GOG exe and the decompiled AS (`%TEMP%\lib-agent-2-guicmd-x\allgfx\as_quickbar\scripts`). checked = read in the exe or the AS; inferred = reasoned. No game was run.

### Short answers

1. **The ResolutionWidth observer is a quirk, not a maximum: vanilla feeds the screen width into a function that takes "pixels beyond 14 slots", so it always means "full width". What gives vanilla 14 slots is `Party.QuickbarExpandValue`, delivered AFTER it and winning.** In your port the same two deliveries arrive in the other order (or the Expand token is never delivered / is delivered as something else), so the width wins.
   - `ExpandQuickbarFromGameData(v)` (Quickbar.as:263-268, checked): `handle._x = Math.min(Number(v) + positions[14], GetSnapPosition(m_MaxDragWidth))`, then `UpdateExpansionMaskPosition()`. `positions[n] = 20 + 47*n` (n = 0..50), so `positions[14] = 678`. The argument `v` is therefore the handle's offset past the 14-slot minimum, in pixels. Both observers call this same function (Quickbar.as:76-79 for `ClientOptions.VideoOptions.ResolutionWidth`, id QUICKBAR_SCREEN_WIDTH 1613; Quickbar.as:244-248 for `Party.QuickbarExpandValue`, id QUICKBAR_EXPAND 1609). The ResolutionWidth callback passes 1920 where an offset is expected, so it clamps to `snap(m_MaxDragWidth)`: full width. It is not conditional, not a branch, not a maximum. Any width at or above about 1024 does the same (checked by reading; this is the engine's own AS, written as a bug that never shows because of the order below).
   - **14 slots as a handle value:** `handle._x = 678`, i.e. `Party.QuickbarExpandValue = 0`. Slot count = the `i-1` of the first `positions[i] > handle._x` (UpdateExpansionMaskPosition, Quickbar.as:333-347): x = 678 gives 14, x = 725 gives 15 (value 47), and so on, `14 + floor(value / 47)` up to 50. The value is `handle._x - 678` (Quickbar.as:348, written back by the movie after every change). Snap: GetSnapPosition(x) returns the largest `positions[k]` not above x (Quickbar.as:297-309).
   - **Max length:** `m_MaxDragWidth = sm_rSafeZone.width - handle._width - (quickbar global x - sm_rSafeZone.x)` (Quickbar.as:291-296), recomputed in `UpdateLayout` (line 286-290: also calls `ExpandQuickbarFromGameData(Number(GetValue("Party.QuickbarExpandValue")))`). Safe zone: `Stage.width * 0.9` (or 1), from SceneManager.SetSafeZone (see gui3a). So the full width depends on the stage width, not on the value delivered. Handle drag range: `startDrag(false, positions[14], y, snap(m_MaxDragWidth), y)` (line 197); double click on the handle jumps to `snap(m_MaxDragWidth)` (line 190-194).
   - **Where `Party.QuickbarExpandValue` comes from** (checked): a FLOAT attribute in the Party object, at `CParty+0xB4`, built in the Party constructor `0x0063A3C0` (called from `0x0069D24C`): `fldz; fstp [esp+0x84]; lea ecx,[esi+0xB4]; call 0x005DDB90` (the float attribute ctor, vtable 0x00B00CC4), so **the default is 0.0**. It is registered under the wide name `QuickbarExpandValue` (0x00B05F90) at `0x0063B416`, between `PartyPickerList` and `HoldPositions`/`QuickbarLocked`. The same name is also listed once more in `0x00719B70` (the creature Stats attribute list, with `QuickSlots` and `QuickbarLocked`; the movie never reads a per-creature one, `<token>.Stats.QuickbarLocked` is the only per-creature neighbour it reads).
   - **Saved?** Not per game and not in the ini (inferred, strong): the name has exactly two code users (above), no `DragonAge.ini` key (the options loader names only `[VideoOptions]` etc. attributes), and `dev/gff4_fields.csv` has `SavegamePartyQuickbarLocked` (16292) and `SavegamePartyHoldPositions` beside it but no Quickbar-expand field. So the value lives in the Party object for the session: 0 at every launch (a new game and a loaded game alike, until the player drags the handle), and it is lost on quit. Not verified by loading a save after a drag.
   - **What the movie sends back:** no fscommand beyond `SetValue`. `UpdateExpansionMaskPosition` (Quickbar.as:327-351) runs after every change and does `ExternalCommands.SetValue("Party.QuickbarExpandValue", handle._x - 678)` and `SetValue("GUI.QuickbarXMax", globalX + handle._x + handle._width)`. `SetValue` (FS 0x0051BD40, checked in GUI.md) finds the attribute and calls its `Set` (vtable +0x28), which notifies the observers. So the engine stores a number; the observer then re-delivers the current value.
   - **Order and winner (vanilla, new game, 1920 wide):**
     1. `Quickbar.FinalizeInstance` (Quickbar.as:48) pushes the ResolutionWidth observer (line 76) to `AddTokenObserverBatchData`. `InitializeExpansionHandle()` (line 70) only DEFINES `ExpansionHandle_mc_init`. Then `Element.Initialize` calls `UpdateLayout` (Element.as:98-101) which expands to `Number(GetValue(ExpandValue))`.
     2. The handle clip's own Initialize runs `ExpansionHandle_mc_init` (found by `GetInitFunction` on the parent chain, so necessarily after step 1): `handle._x = min(snap(max), 678 + Number(GetValue("Party.QuickbarExpandValue")))` = 678, `UpdateExpansionMaskPosition()` (14 slots, `SetValue(ExpandValue, 0)`), and only then (line 244) pushes the Expand observer. So the batch order is ResolutionWidth (1613) first, Expand (1609) second (checked in AS).
     3. The engine processes the batch in array order (`ProcessAddTokenObserverBatch` 0x0051A8B0) and each new observer notifies at once (`0x00519AA0` adds/replaces by id in the pending vectors, keeping order, GUI.md gui3). `PerformBatchTokenReplacements` (TokenManager.as) calls the callbacks in vector order. So `ExpandQuickbarFromGameData("1920")` (full width) runs first, then `ExpandQuickbarFromGameData("0")` (the Expand observer's first value, the float 0 as its string) runs second and sets the handle back to 678: **14 slots**. (Each call writes `SetValue(ExpandValue, handle - 678)`; the engine's next re-read delivers the attribute's current value, which has converged to 0, so it stays at 14.) The order step is checked in the AS and the engine's append order; that both observers' first deliveries fall in the same DeliverTokens is inferred (both batch entries sit in the array when the engine pulls it, if the handle clip initialised in the same frame; if the handle's came a Step later, the order is still Resolution first).
   - **Rule a port can copy:** (a) `Party.QuickbarExpandValue` is a float, default 0, string form of 0 (`"0"`); deliver it on registration like any attribute (a missing path would deliver `""`, which `Number("")` also turns to 0, so even that is safe). (b) Give `SetValue("Party.QuickbarExpandValue", n)` a real store and re-deliver the current value to the observer after it (the engine does; this is also how a drag persists). (c) Keep delivery in the movie's registration order, the Expand observer after the ResolutionWidth one; or simply never deliver ResolutionWidth before ExpandValue's first value (if the port delivers observers in id order (1609 < 1613) or delivers ResolutionWidth last, it gets full width: that is the likely cause). (d) Deliver ResolutionWidth as the configured number as a string (1920), nothing else; it only matters in vanilla if it arrives last (it would, if the player changes resolution later: then the bar jumps to full width. Inferred from the AS; vanilla quirk).
   - **Vanilla's value at 1920, new game:** `Party.QuickbarExpandValue` = `"0"` (float 0.0), handle at 678, **14 slots**; then `GUI.QuickbarXMax` is written back by the movie (`globalX + 678 + handle._width`).

2. **A quickbar slot token `GUI.SelectedChar.Stats.QuickSlots.N` is a composite attribute (an `AbilityInstance` in the engine) whose own value the movie reads as a number: nonzero = a slot holds an ability, 0 or empty = empty; its children carry everything else.** I could not read the node's own "get as string" with certainty; see Not read.
   - **What the movie builds** (checked): `Quickbar.MakeSlotInitFunction` calls `SetQuickSlotIndex("GUI.SelectedChar", n)` (Quickbar.as:318-326) so `m_sQuickSlotToken = "GUI.SelectedChar.Stats.QuickSlots." + n` (QuickBarIconSlot.as:197), n = 0..49. It adds a `QuickBarIcon` child with `AbilityToken = m_sQuickSlotToken` (line 62), and an observer id `ABILITY_ICON_SLOT_RANGE + n` (32000 + n) on the token (line 207): its callback IGNORES the delivered value and does `Number(GetValue(token)) != 0` -> `ShowIcon` else `ClearIcon` (lines 209-217). The icon also observes the token itself under `ABILITY_ICON_RANGE + n` (13000 + n; AbilityIcon.as:168) with `valid = v != null && v != 0` (the pie, highlight and disabled overlays show only when valid). And `UpdateValidToken(GetNumber(token) != 0)` at start (line 164). `QuickBarAbilityIcon.as:99` compares `GetString(token) == 0` too (AS2 loose equality: "0" or "" or the number 0).
   - **So the token's value is an integer-valued string**, 0 for an empty slot. What the nonzero value is (ability id vs instance handle) the movie does not tell: it never uses the token value itself, only its truthiness; it uses `.AbilityTemplate.ID` as the ability id (QuickBarAbilityIcon.as:213, 383). **For a port: deliver the ability's id (the `ABI_BASE` row id) as the token's value, 0 for empty; this satisfies every use above.** (inferred for "ability id"; checked for "nonzero = occupied".)
   - **Sub-attributes the movie reads, with the engine's own fields** (checked: names from the wide strings in the AbilityInstance constructor `0x0075A230`, vtable 0x00B1C9C8, 0x17C bytes; field offsets from the registrations at 0x0075A3EA-0x0075A9C8 paired by hand):

     | child path | offset | type / default (ctor) | meaning and use in the movie |
     |---|---|---|---|
     | `.AbilityTemplate` | +0x58 | pointer-like attribute (ctor 0x0075A010, vtable 0x00B1C8EC); set to the ability template (`0x0062D760`: `0x0062C730(abilityId)`) | the ability's template; the movie reads children `.AbilityTemplate.ID`, `.Name`, `.NameStrRef`, `.DescStrRef`, `.TooltipStrRef`, `.AbilityType`, `.UseType`, `.Cost`, `.CostUpkeep`, `.Cooldown`, `.Conditions`, `.ConditionMode`, `.PrereqLevel` etc. (tooltip; also Ability.as:39-55) |
     | `.Status` | +0x68 | int, 0 | 1 = the ability is active (a toggled mode): the ActiveHighlight shows (AbilityIcon.as:131, 144-155, compares `"1"`) |
     | `.IsError` | +0x78 | bool, 0 | the disabled overlay (AbilityIcon.as:208, 214) |
     | `.Error` | +0x88 | int, -1 | a TLK string id for the reason (AbilityIcon.as:269) |
     | `.IsWarning` | +0x98 | bool, 0 | the "prerequisite" overlay (AbilityIcon.as:202, 260) |
     | `.Warning` | +0xA8 | int, -1 | TLK id (AbilityIcon.as:274) |
     | `.SourceItem` | +0xB8 | object id attr, -1 | the item an item ability comes from, -1 = `DAConstants.INVALID_OBJECT_ID` (DAConstants.as:3) when none; the movie reads `.SourceItem.ID` (the item's id, third argument of `GameModeController.PerformAbility`, QuickBarAbilityIcon.as:166-167) and `.SourceItem.StackSize` (the count text "xN", line 279-311) |
     | `.SourceItemTag` | +0xC8 | string, "" | the item's tag (dragData ItemTag; also the check for a missing item showing red "x0", line 296) |
     | `.SourceItemName` | +0xF0 | string, "" | tooltip name for an item ability (AbilityType 4) |
     | `.RefTemplate` | +0x118 | string, "" | the item template's resref (drag data) |
     | `.ItemIcon` | +0x140 | string, "" | the icon's image name; observer id `ABILITY_ICON_ITEM_RANGE + n` (29000 + n), `UpdateIcon(v)` loads it as `img://`-style image 42x42 (line 385-396) |
     | `.PendingActivation` | +0x168 | bool, 0 | pending-highlight (AbilityIcon.as:94-97) |
     | `.CooldownPercentage` | set up later (strings 0x75A8D2-0x75A9C8, `CooldownTime` beside it) | number | 0 = ready; (0,1] = cooling, the pie fills `(1 - v) * 2*PI` (PieCooldown.as:56-110); begin at v in (0,1], end at 0 |

   - **Ability slot vs item slot** (checked in `0x0062D760`, the engine function that builds the instance for a slot; args read from its stack: ability id, an item id at the third argument, others): for an ability slot (no item) the item attributes keep their ctor defaults (SourceItem -1, strings empty) and `.ItemIcon` is the ABILITY template's icon (the field at template +0xD0, set at `0x0062D878-0x0062D892` into the attribute at +0x140). For an item slot (an item id given, the item object found through the object table at `0xC327EC`, `0x005D5870`): `SourceItem` = the item id, `SourceItemTag` = the item's tag (item +0xB8), `RefTemplate` = item +0x1CC, `ItemIcon` = item +0x48C (the item's own icon, overriding the ability's), `SourceItemName` = the item's name from its strref (item +0xD4 via `0x004D1EB0`). Reading by offsets and by the stored-to fields; the names of the item fields are inferred from the order and from the movie's use.
   - **Empty slot:** value 0 / empty and `.AbilityTemplate...` absent: `ClearIcon`; the engine's array slot is a null `AbilityInstance*` (the script command `GetQuickslot` reads `creature+0x7C4 -> +0xC -> +0x1B4`: bar index at `+0x1B0`, per bar a 20-byte vector at `+0x14C + bar*20`, element `AbilityInstance**`; `0x0071C620`). For an empty slot a port should deliver `""` or `"0"` and may leave children undelivered (missing paths deliver `""`; the movie takes `GetValue == null`/0 as empty).
   - **Setting a slot (what the movie sends):** `SetQuickslot` (QuickBarIconSlot.as:155-163): `SendMessageArgs(<player token> + ".SetQuickslot", creatureId (GUI.SelectedChar.ID), slotIndex, abilityId, itemTag, refTemplate)`, i.e. the message name is `GUI.SelectedChar.SetQuickslot`; abilityId 0 clears. The engine's handler is `CCreature::HandleMessage(SetQuickslotMessage)` `0x006A74B0` (dev/message_handlers.csv:11; the message fields: +0x2C a string, +0x10 a string, +8, +0xC two ints) feeding `0x0071F230` then updating the lists. Drops use `AbilityID != 0 && != ITEM_ABILITY_INVALID (200000, ItemUtils.as:9)` as the test for a real ability (IsDropAcceptable lines 94-103).
   - **Per-slot keys:** `InputMappings.Mappings.ActivateQuickslot<N+1>.PrimaryMappedKey` (QuickBarIconSlot.as:53; already in dev/GUI.md).

### Evidence

- AS: Quickbar.as:35-38 (MAX_QUICK_SLOTS 50, MIN_SLOT_COUNT 14, spacing 47), 48-80, 170-258, 259-268, 286-309, 327-351; QuickbarScene.as; GUI/Element.as:56-104 (`Initialize`: `GetInitFunction`, init functions, `FinalizeInstance`, `UpdateLayout`); frame_1/DoAction.as:103-117 (`GetInitFunction` walks `_parent`); TokenManager.as (`AddTokenObserverWithID` pushes to the batch array; `PerformBatchTokenReplacements` walks ids in order); `_OBSERVER_ID.as` 94-107 (QUICKBAR_EXPAND 1609, QUICKBAR_SCREEN_WIDTH 1613), 209-232; QuickBarIconSlot.as; QuickBarAbilityIcon.as; AbilityIcon.as; PieCooldown.as; Ability.as. Scripts: `%TEMP%\lib-agent-2-gui4a-*.py` (wide-string finder `-r.py`, function-start finder `-f.py`, caller finder `-c.py`, push-string lister `-p.py`).
- Exe (GOG): wide strings `QuickbarExpandValue` 0x00B05F90 (refs 0x0063B416, 0x00719BE7), `QuickbarLocked` 0x00B05FD4 (refs 0x0063B340, 0x00719C20, 0x0071ABBF), `QuickSlots` 0x00B17578 (refs 0x00719BA7, 0x0071AB47), `QuickbarXMax` 0x00B03910 (ref 0x00614A9C). Party ctor 0x0063A3C0 (RTTI of this class not read; its attribute list is Leader, Hero, Members, ..., HoldPositions +0x84, QuickbarLocked +0x94, QuickbarExpandValue +0xB4 float 0.0 via `fldz` at 0x0063A4B2 and 0x005DDB90). AbilityInstance ctor 0x0075A230 (called from `0x0062D83B` and `0x0062D9C9`, the builders `0x0062D760` / `0x0062D960`; allocation size 0x17C). `GetQuickslot` command `0x0066B800` reads `creature+0x7C4`.

### Not read

- The AbilityInstance node's own "get as string" (the slot token's value): the sub-vtable at +0x1C has a trivial slot 11 (`0x007268D0`, returns its argument) and the outer vtable (0x00B1C9C8, slots 0x004A9C40, 0x004A9C80, 0x00402B30, 0x005BDCF0, 0x0075A880) was not followed; the value being the ability id is inferred from the movie's use only. A run that prints the delivered `QuickSlots.0` string in vanilla would settle it.
- Whether the engine's `Party.QuickbarExpandValue` survives a save/load or a quit (concluded from absence of any other code user and any save field, not seen in play).
- That both of the quickbar's observers' first deliveries fall in the same DeliverTokens (the order is the same either way); the exact GFx constructor order of the quickbar and handle clips (inferred from `GetInitFunction` needing the parent's init function to exist).
- How the Stats-level `QuickbarExpandValue` listed at 0x00719BE7 is used (not read by any movie).
- The `CooldownTime`/`CooldownPercentage` attribute registrations in full (offsets, type) and what values the engine computes (the movie's use is clear: 0 ready, (0,1] cooling).
- `GetSnapPosition` of the real `m_MaxDragWidth` at 1920 (depends on the quickbar's global x and the handle's width at runtime).

## gui4b: ReplaceDesignerTags (lib-agent-2, read 2026-10-18)

### Short answers

1. The engine does almost nothing to a tooltip string. It knows no `<header>`, `<title>` (as font markup), `<red>`, `<dmg>`, `<font>`, `<br>`, `<p>`. Echoing the text is nearly right. It does exactly two things, in this order [checked, 0x0051F110]:
   - Stage 1: a plain, case-sensitive replace-all, in row order, of each row of the GDA table `dialog_tags` (table id 143, `packages/core/data/2da.erf`, 16 rows, columns ID, DialogTag, MarkupTag) from DialogTag to MarkupTag. Not hard-coded; the table is read through the data-table manager (0x004DF580 with id 0x8F), so an add-in's rows merge in.
   - Stage 2: for each `<` in the (new) string, find the next `>` (none: stop), take `<...>` including the brackets, hash it (CRC-32 of the UTF-16LE text, brackets included, case-sensitive; 0x004CA9A0, table at 0xAF07E0) and run it through the big switch 0x0051D620. A match is replaced by the token's text. Anything else (every `<font ...>`, `</font>`, `<img .../>`, `<b>`, `<i>`) comes back unchanged (default path 0x0051E8C4 copies the input to the output).
   - If the app's "game" object is missing (0x005D57C0 returns null) the tag is returned unchanged.
2. Nothing else is touched: no newline normalisation (\r, \n, \r\n kept), no stripping, no escaping of `&`, no `<br>`/`<p>` added, no `<textformat>`/leading. So the overlapping lines do not come from the engine's answer. Look at the AS pre-processing (below), and at how Ruffle renders `<img ... vspace=12 align=baseline/>` between `\n`s.
3. The answer is returned in `_root.fsc_retval` as a string for the fscommand [checked: 0x0051F410 calls 0x00518830, which sets the movie variable `_root.fsc_retval` (string at 0x00AF56D0), the same helper GetTlkString uses]. The ExternalInterface twin (0x0051F2F0) wraps the result in a GFxValue of type 5 (string) and calls a vtable slot +0xA8 of the object from 0x00510FA0, which I take to be the external-interface return value [inferred; not followed].

### (a) What stage 1 holds [checked, ghilana `table dialog_tags`]

| ID | DialogTag | MarkupTag |
|---|---|---|
| 0 / 1 | `<emp>` / `</emp>` | `<b>` / `</b>` |
| 2 / 3 | `<desc>` / `</desc>` | `<i>(` / `)</i>` |
| 4 / 5 | `<act>` / `</act>` | `<i>(` / `)</i>` |
| 6 / 7 | `<attrib>` / `</attrib>` | `<i>` / `</i>` |
| 8 / 9 | `<title>` / `</title>` | `</i>` / `<i>` (as shipped: open and close are swapped, a vanilla data bug; copy it) |
| 10 / 11 | `<bodytitle>` / `</bodytitle>` | `<i>` / `</i>` |
| 12 / 13 | `<excerpt>` / `</excerpt>` | `<i>` / `</i>` |
| 14 / 15 | `<center>` / `</center>` | 39 spaces / empty string |

Only one copy of the table exists (core 2da.erf, priority 100005). The pairs are applied in row order with no escaping, so `<title>` yields `</i>` ahead of the text.

### Stage 2 token table (switch 0x0051D620) [checked: every case constant read from the disassembly, 40 of 43 names recovered by hashing candidate names, brackets included]

Names (all written `<Name/>`):
- Party hero (via the game object): `FirstName` 0x7295CD60 (call 0x62ECA0 on the object), `LastName` 0xCAAD80AD, `FullName` 0x8595D073, `DogName` 0x8473B14C (0x62ED50). The result is plain text, no markup.
- Keys as bound to actions (the action name is a wide string pushed before a binding lookup 0x00894710): `InventoryKey` (ToggleInventory), `JournalKey` (ToggleJournal), `PauseKey` (TogglePause), `TacticsKey` (ToggleTactics), `AreaMapKey` (ToggleMap), `MainMenuKey` (ToggleMainMenu), `CharacterRecordKey` (ToggleCharacterRecord), `SkillsKey` (ToggleSkills), `PartySelectKey` (ToggleFullPartySelect), `HoldPositionsKey` (ToggleHoldParty), `RunWalkKey` (ToggleRunWalk), `QuickSaveKey`, `QuickLoadKey`, `ScreenshotKey`, `SwapWeaponsKey` (SwapWeaponSets), `HighlightKey` (HighlightObjects), `MoveForwardKey` (RunCharacterForward), `MoveBackwardKey` (RunCharacterBackward), `MoveLeftKey` (RunCharacterLeft), `MoveRightKey` (RunCharacterRight), `CameraLeft` (MoveCameraLeft), `CameraRight` (MoveCameraRight), `ZoomInMouse` / `ZoomOutMouse` (ZoomIn / ZoomOut), `SelectChar1`..`SelectChar4` (SelectPartyMember1..4), plus `SpellsTalents` is in the code but its tag name was not recovered.
- Mouse wording: `LeftClickDefault`, `RightClickDefault`, `LeftClickingDefault`, `RightClickingDefault`, `LeftButtonDefault`-style `RightButtonDefault`, and the `...Capped` variants. They use talk-table strings 399133..399138 ("right-clicking", "left-clicking", "Right-clicking", "Left-clicking", "right", "left") chosen by a flag from the 0x00838210 (mouse buttons swapped option) call, with sentence-case variants for the Capped ones (bit tests 0x8000/0x10000/0x200 on a local flag word) [checked in structure, wording of each case inferred].
- 3 constants (0x47C01040, 0x7174B5F8, 0xCB64B7E5) were not matched to a name (not in the English talk table; probably other key tags such as camera up/down or the spells key).
- Anything else, including `<name/>`, `<cost/>`, `<description/>`, `<header>`: unchanged. Those template tags in the ability text are expanded by the AS, not the engine.

Mechanics of stage 2 [checked 0x0051F240..0x0051F2C2]: scans from index 0; at a `<` it searches the next `>`; if none, the loop ends; otherwise it replaces `[pos, '>']` with the token's result and moves on by one character (so the replacement text itself is rescanned from pos+1, and an unmatched tag is simply left in place). The `&` is never touched.

Source of replacements: hard-coded tag names (hashes) in the exe for stage 2, the GDA table for stage 1. No XML/CSS/stylesheet is involved. The font markup of tooltips is not engine side at all (next section).

### (b) Return path and how the AS uses it [checked]

- AS wrapper: `as_*/scripts/__Packages/ExternalCommands.as:125-130`: `_root.fsc_retval = null; fscommand("ReplaceDesignerTags", a_sText); return _root.fsc_retval;`
- Sole caller: `SharedLibrary/TextField.as:127-135` (`ProcessText`: only if the text contains `<`), called from `SetText` at `TextField.as:43`:
  ```
  a_sText = _loc5_.removeString("\r\n","\r");          // :30   every CRLF becomes a bare CR
  if(html == false || GUISystem.ShowRawFormattingTags) TextField.text = a_sText;      // :36-39
  else { a_sText = StringUtils.ReplaceString(a_sText,"&","&amp;");                       // :42
         TextField.htmlText = this.ProcessText(a_sText); }                                // :43
  ```
  So order in the movie is: CRLF -> CR, `&` -> `&amp;`, then (if there is a `<`) the engine call, then `htmlText`. Never `.text`, never `condenseWhite` in the script; multiline/wordWrap/autoSize are set elsewhere (FloatingTooltip, below). Whether the editText in the SWF has condenseWhite or multiline set was not read.
- The `<header>` markup is the AS's own, in the tooltip movie: `tooltips/scripts/__Packages/SharedLibrary/FloatingTooltip.as:9-10,54-63`. `DisplayTooltip` splits the text at `<header>` and `</header>` and writes `HTML_HEADER_OPEN = "<font size='14'>"` + header + `HTML_HEADER_CLOSE = "</font>\n<img src='tooltip_linebreak' align='baseline' vspace='12'/>\n"` + rest. Then `Text_mc.TextField._width = MinWidth; autoSize = "left"; wordWrap = false; Text_mc.SetText(text)` (:64-67); for the multi-line type, if wider than MaxWidth, it sets `_width = MaxWidth; wordWrap = true` and calls SetText again (:76-81). The placeable tooltip (Tooltips.as:123-132) builds `"<header>" + name + "</header>" + "\n" + key + ": " + action`.
- Reaching the floating tooltip: the quickbar movie sets `GUITooltips.*` (DisplayTooltip.as:100-114, `SetValueUnicode("GUITooltips.TooltipText", ...)`), the tooltips movie observes `GUITooltips.TooltipText` (Tooltips.as:61-65) and `UpdateFloatingTooltips` (Tooltips.as:265-304) passes it to `SimpleFloatingTooltip_mc.DisplayTooltip` / `FloatingTooltip_mc.DisplayTooltip`.
- The peer's echo is therefore correct for `<font>`/`<img>`/`<b>`; what it must add is only stage 1 and the tokens above. The overlap is more likely a rendering matter: (1) the AS makes line breaks of `\r` (from CRLF) and a bare `\n` (the header close and the placeable tooltip); a player that counts both `\r` and `\n` as breaks and an `<img vspace=12 align=baseline>` between two `\n` produces a blank line after the header, and if the text then gets `\r\n` unchanged (peer sends text with the AS's step :30 skipped), CR plus LF count twice; (2) `autoSize="left"` + `wordWrap=false` measured before `_width` is set. These are guesses [inferred], not read.

### (c) Worked example: quickbar tooltip for Shield Pummel (ability ID 1)

Inputs [checked from abi_base, guitypes, ranges and the talk table]: Name 379897 "Shield Pummel", ToolTipStrRef 367458, Desc 379403, GUIType 10 -> guitypes.StringID 371182 "Weapon and Shield", UseType 4 -> 380401 "Activated", Range 0 -> ranges.StrID 380382 "Personal", Cost 30, Cooldown 20. The player's attribute 41 total is taken as 0, and requirements and conditions as empty (inferred; they hold colour markup only when something fails: `UpdateTooltip` QuickBarAbilityIcon.as:169-268, `GenerateTooltipText` Ability.as:241-266).

Template (tlk 367458, as stored, CRLF line ends):
```
<name/> (<guitypename/>)\r\n<usetype/>\r\nRange: <range/>\r\nActivation: <cost/>\r\nCooldown: <cooldown/>s\r\n\r\n<requirements/>\r\n<conditions/>\r\n\r\n<description/>
```
1. `GenerateTooltipText` (AS) replaces the `<xxx/>` placeholders (name becomes `<header>Shield Pummel</header>`) and collapses `\r\n\r\n\r\n` runs to `\r\n\r\n`:
```
<header>Shield Pummel</header> (Weapon and Shield)\r\nActivated\r\nRange: Personal\r\nActivation: 30\r\nCooldown: 20s\r\n\r\nThe character follows up an attack with two hits from the shield, ...
```
2. `FloatingTooltip.DisplayTooltip` rewrites the header:
```
<font size='14'>Shield Pummel</font>\n<img src='tooltip_linebreak' align='baseline' vspace='12'/>\n (Weapon and Shield)\r\nActivated\r\nRange: ...
```
3. `TextField.SetText`: CRLF -> CR, `&` -> `&amp;` (none here), the string has `<`, so the engine is asked.
4. Engine input and output: stage 1 finds none of `<emp>`, `<desc>`, ... so nothing changes; stage 2 sees `<font size='14'>`, `</font>`, `<img src=... />` which match no token hash. Output equals the input byte for byte:
```
<font size='14'>Shield Pummel</font>\n<img src='tooltip_linebreak' align='baseline' vspace='12'/>\n (Weapon and Shield)\rActivated\rRange: Personal\rActivation: 30\rCooldown: 20s\r\rThe character ...
```
5. `htmlText = that`. (Here `\r` stands for a single CR left by step 3.)

A case where the engine does change text: a conversation or journal line `You see <emp>this</emp>, <FirstName/>.` becomes `You see <b>this</b>, Hawke.` (the hero's first name; for an origin hero the creature's name from the party object). An item/ability description with `<title>x</title>` becomes `</i>x<i>` (the data bug).

### Not read

- The SWF editText flags of `Text_mc.TextField` (html, multiline, condenseWhite, embedded font, `<img>` source) and the `tooltip_linebreak` image.
- The 3 unnamed token hashes, and each mouse-token case's exact wording per flag.
- Whether the table `dialog_tags` is also reachable through a localisation-specific row set (only the English core copy was read; ghilana showed no override).
- GuiExternal::ReplaceDesignerTags' return call (vtable +0xA8) was not followed.
- What 0x005D57C0 returns when false (I only saw the branch return the input unchanged).
- How the real Scaleform renders `\r` versus `\n` versus `<img>` in an auto-sized field: no play or Ruffle test.

## gui4c: a right click over the quickbar (lib-agent-2, read 2026-10-18)

Read in the GOG exe and the decompiled AS (`%TEMP%\lib-agent-2-guicmd-x\allgfx\as_*`, paths below are relative to `as_<movie>\scripts\__Packages\`). Nothing was run, nothing written to a repo. checked = read; inferred = reasoned.

### Short answers

1. **Vanilla does nothing on a right click on a quickbar slot** (ability, item-ability or empty), and nothing in the world either. No movie defines `onRightPressHitArea` on a quickbar class; the engine's right-click action (`PerformPrimaryAction`) is refused while the mouse is over any movie element. There is no details panel, no clear-slot, no radial menu on the quickbar (the radial menu clip exists only in inventory, itemexamine, itemupgrade and store). Slots are cleared by dragging the icon onto `IconDestroyDropTarget` (QuickbarScene.as:29, `Mouse.getTopMostEntity() == this`). (checked)
2. **A correction to the question: the key label "Right-click" belongs to `PerformPrimaryAction`, not `PerformSecondaryAction`.** TLK 369711 = "Right-click", 369710 = "Left-click", 369760 = "Shift + " (read through ghilana). Primary = mouse code 0x0D, secondary = Shift + 0x0D (set in code, `Input::RegisterActions` 0x006E30D0, 0x006E2FD0 region; dev/GUI.md section on key labels). Mouse code 0x0D is the right button, so in vanilla the right button is the world's command button (move, attack, use, talk) and the left button selects. In the world, secondary = Shift + right click. (checked)
3. **How a right press reaches a movie:** `GUICursorRightButtonDown` is bound to the same mouse code 0xD as the action `PerformPrimaryAction` (checked, string `GUICursorRightButtonDown` at 0xB12C90 is bound with code 0xD at 0x006E4D56, next to the action's own code at 0x006E6AF1 / 0x006E3019). ScaleformManager forwards it as a GFx mouse-down event with button index 1 (dev/GUI.md section 5). The movie's `Element.as` turns it into `onRightPressHitArea(isDouble)` on the element under the pointer, which the quickbar does not define, so the AS side is a no-op. The engine side never sees the click as a quickbar click.
4. **The rule a port can copy:** (a) forward right press/release to the movies with button 1 (double 5), and run `Element.as`'s own listener; (b) before the world handles a right click, ask "is the mouse over a movie element" (the engine's `MouseOverInterface`: any hit record in the movie hit list, see section 2); if yes, the world gets no target and no ground point, so no move/attack/use; (c) the only HUD element that lets the world act "through" it is a portrait: its rollover sets `GameModeExplore.GUITargetedPlayer` to the creature id (0 on rollout / drag out) and the engine then uses that creature as the cursor's target (section 3). A right click on a portrait then acts on that creature (for another active party member: a talk/dialogue order, see section 3), a right click on a quickbar slot, which sets no such value, acts on nothing.

### 1. How a right button press reaches the movie, and Element.as's dispatch

Engine side (dev/GUI.md section 5, re-confirmed by the messages table): `GUICursorRightButtonDown/Up/DblClick` (vftables 0xB1DCD4 / 0xB1DD04 / 0xB1DD34) are handled by `ScaleformManager` (thunks 0x512990 down (1,0), 0x5129A0 up, 0x5129B0 dbl (1,1)), which builds a GFxMouseEvent with button 1 (or 5 for a double click) and calls `GFxMovieView::HandleEvent` (slot +0x98), through record method 0x00518970 (down) and 0x00518A50 (up). (checked, GUI.md)

Movie side, `GUI/Element.as` (identical in every movie; quickbar copy):

- `SetupOnPressListeners` (Element.as:193-205): installs a native `onPress` on the hit area if the element has ANY of `onPressHitArea`, `onMiddlePressHitArea`, `onRightPressHitArea`; likewise `onRelease` for the three `...ReleaseHitArea` names and `onReleaseOutside` for the three `...ReleaseOutsideHitArea` names. Each is `FuncUtils.Bind(this, fnEventHandler, EVENT)` (FuncUtils.as:80-89), so it is called as `fnEventHandler(EVENT, ...args)` with the native call's own arguments appended. (checked)
- `fnEventHandler(nEventType, button)` (Element.as:206-271): `button` undefined or `Mouse.LEFT` -> prefix ""; `Mouse.RIGHT` -> "Right"; `Mouse.MIDDLE` -> "Middle"; `LEFT+4` / `RIGHT+4` / `MIDDLE+4` -> the same prefixes with the double-click flag `_loc6_ = true`. Handler name = "on" + prefix + ("PressHitArea" | "ReleaseHitArea" | "ReleaseOutsideHitArea"). A press also sets `_root.IsDoubleClick = false`. It calls `this[name](isDouble)` only if that member exists (lines 267-270). The nine names it can reach: `onPressHitArea`, `onReleaseHitArea`, `onReleaseOutsideHitArea` and their `Right` and `Middle` forms; rollover-type handlers are separate (`onRollOverHitArea`, `onRollOutHitArea`, `onDragOverHitArea`, `onDragOutHitArea`, lines 120-170; no button argument). Nothing named `onPressAux`, `onRightPress` (without HitArea) or `onSecondary...` is dispatched; `Mixin.as:19` even prints an error if a behaviour defines `onpress/onRightpress/...` directly. (checked)
- `SetupMouseListener` (Element.as:272-358), only if `Mouse.LEFT/RIGHT/MIDDLE` all exist: `Mouse.addListener` with `onMouseDown(button, target)`: LEFT only sets `bMousePressed` and the cursor and returns (the clip's native `onPress` serves the left button); any other button (including every `+4` double click, LEFT+4 = 4 as well) does `mouseDownTarget = Mouse.getTopMostEntity(false)` and `mouseDownTarget.onPress(button, false)` (lines 282-295). `onMouseUp`: LEFT clears; else `getTopMostEntity(false)` again, and `onRelease(button,false)` if it is the same entity as at the press, else `onReleaseOutside(button,false)` (296-314). So the right button reaches `fnEventHandler` as `(PRESS_EVENT, 1, false)` through the listener and the bound `onPress` on the hit-area clip; the native clip `onPress` must NOT also fire for the right button, or the handler would run twice (gui3 notes: GFx 3.x fires native onPress for the left button only; inferred). (checked for the AS; the GFx behaviour inferred)
- So for a port: native `onPress/onRelease` (left, no arguments) plus the listener for buttons 1, 2 and the `+4` forms; `getTopMostEntity(false)` must return the nearest clip with mouse handlers (gui3b section 2).

Who defines a right handler (grep over all 56 movies, `function onRight...`): `PaperDoll.as:41/51/61` (characterrecord, chargen, inventory, saveload: `onRightPress/Release/ReleaseOutsideHitArea`), `EquipIcon.as:68` and `SharedLibrary/ItemEntry.as:276` (inventory, store, itemupgrade: opens `_root.RadialMenu_mc`), `Rune.as:151/149` (itemexamine, itemupgrade), `RuneEntry.as:15`, `UpgradeableItemEntry.as:80` (itemupgrade), and `ItemExamine.as:28` (`button == Mouse.RIGHT`, a root listener). The quickbar, portraits, minimap, navbar, abilities, tooltips, conversation movies have none. (checked: no `onRight*` function and no `Mouse.RIGHT` use outside those files)

### 2. How the engine decides: world or GUI

- The input table binds the right button twice, in different contexts: the movie-side event `GUICursorRightButtonDown` (code 0xD, string at 0xB12C90, bound at 0x006E4D56) and the action `PerformPrimaryAction` (code 0xD, no modifier; 0x006E3019-0x006E304D) which is mapped to the message `PerformPrimaryActionOnTargetUnderCursor` (string 0xB12AD8, mapping at 0x006E6AC5-0x006E6AF1). The same click therefore produces both; the movie gets its event regardless, the world message is gated below. (checked the bindings; that both fire for one click and are not exclusive by context is inferred)
- The gate is not on the message but on the target the cursor update computes. `GameModeExplore::UpdateCursor` (0x0070F270, ID 16280, per frame): calls the pick `0x0070EAA0`, then clears `explore+0xCC` (cursor object id = -1), `+0xD0` (ground hit) and `+0xD4`. The pick begins by asking the GUI system's slot 71 (offset 0x11C of the object at `[app+0x158]`); true -> returns 0 (0x0070EABF-0x0070EAC3). Slot 71 goes to `ScaleformManager::MouseOverInterface` 0x00511420: false when the manager is not running; true when the movie hit list at +0x80 is not empty (the list the manager fills each frame with `HitTest` hits, GUI.md "Mouse input"); when empty, true only if the capture movie at +0xA0 is set. (checked; dev/GUI.md "A movie under the cursor")
- With no pick result (`edi = 0`): `UpdateCursor` then reads the attribute object at the static 0xC35FF0 with `SmartData::Attribute::Get` 0x0053CC40 (`mov eax,[ecx+0xC]`): if it is nonzero and not -1 it becomes `explore+0xCC` directly (0x0070F2A2-0x0070F2C0). The static 0xC35FF0 is the one `GameModeExplore`'s constructor registers under the name `GameModeExplore.GUITargetedPlayer` (wide string 0xB162C4, registrations at 0x0070B2CD and 0x00713A6F, with 0xC35FF0 pushed at 0x00713AA4). (checked the code; that 0xC35FF0 is that attribute's storage is read from the constructor's registration order)
- `PerformPrimaryActionOnTargetUnderCursor` (0x007132B0): looks up `explore+0xCC` with `0x005D5BE0`; null -> returns false; else calls `0x00712830(target, kind = 0x0070CAD0(target), 0)`. The ground-move path needs `+0xD0/+0xD4`, set only when the pick found ground (record kind 1), so it too is empty over the GUI (dev/PARTY.md "Move click", dev/PLACEABLES.md "A placeable clicked"). (checked)
- No per-slot "hovered" token exists for the quickbar: `grep GUITargetedPlayer` over all movies hits only `SharedLibrary/Portrait.as:71, 76, 81`. The quickbar's rollover does a tooltip and a highlight, nothing in the data tree. (checked)

### 3. What happens, by element

| Element | Right press | Where it comes from |
|---|---|---|
| Quickbar slot with an ability | nothing: `onRightPressHitArea` is not defined, so `fnEventHandler` finds no member (Element.as:267); the engine's world action is refused (MouseOverInterface) and `GUITargetedPlayer` is 0 / unchanged, so `explore+0xCC` stays -1 and `PerformPrimaryActionOnTargetUnderCursor` returns false | `SharedLibrary/QuickBarAbilityIcon.as`: behaviours DragAndDropable, PushButton, Disablable, FocusItem (:54-57); only `onPressHitArea`/`onReleaseHitArea`/`onReleaseOutsideHitArea` exist (DragAndDropable.as:74-93, PushButton.as:116-148, QuickBarAbilityIcon.as:413). The left click casts: `OnButtonAction` (QuickBarAbilityIcon.as:163-168) -> `GameModeController.PerformAbility(abilityId, caster id, source item id)` on release. A drag starts after a distance or time threshold (DragThreshold_Time 0.6, line 25) and only for the left button (DragAndDropable.onPressHitArea ignores double click: `!dblClick`) |
| Quickbar slot with an item | the same: item quickslots are the same class, an ability whose `SourceItem` is the item (`GetValue(token + ".SourceItem")`, QuickBarAbilityIcon.as:171-188). There is no item menu on right click | same file |
| Empty quickbar slot | nothing: `SharedLibrary/QuickBarIconSlot.as:28-30` adds only DropTarget, RollOver, Highlightable, which define no press handler; there is no native `onPress` on it, so `getTopMostEntity(false)` does not return it as a handler clip | QuickBarIconSlot.as, Behaviour/DropTarget.as (drop logic via `onMouseMove`, :29), Behaviour/Highlightable.as |
| Lock/expansion handle, weapon-set toggle (quickbar's other elements) | nothing for right: `ToggleButton`/`PushButton` define only unprefixed handlers (`OnSecondaryButtonAction` is the toggle-off action, called from the LEFT release when the state flips: Behaviour/ToggleButton.as:33-49, Quickbar.as:110) | |
| Portrait | the engine acts, not the movie: the portrait's rollover sets `GameModeExplore.GUITargetedPlayer` = the creature's id (Portrait.as:69-72), rollout and drag-out set it 0 (:74-82); `UpdateCursor` then makes that creature the cursor object (section 2), so the right click is "PerformPrimaryAction on that creature": the kind comes from `0x0070CAD0(creature)` (0 for the controlled creature itself; for another creature of the active party, per the code: 3 = talk when the controlled creature has no current order, else -1 = nothing; the creature-type branches were read, their meaning is inferred) and `0x00712830` handles it (kinds in dev/PARTY.md "The primary click"). The movie's own portrait handlers (Portrait.as) define none for the right button; `OnButtonAction` of a plain portrait is empty (Portrait.as:279-281); only the level-up badge (Portrait.as:214-221) sends `GUI.GoToLevelUp` on the left button |
| Portraits' toggles (select-all, hold) | left only: PushButton/ToggleButton; `OnSecondaryButtonAction` = the second state | Portraits.as:152-158, 196-202 |

Left click on a portrait works the same way for selection: the portrait has no select code, `SelectPartyMemberUnderCursorMessage` (0x0070C0B0, `Select(explore+0xCC, ...)`) takes the creature from the same hover value (inferred from the call and the hover token; the handler was read in dev/PARTY.md, the left-button mapping of that message was not traced).

### 4. What a port needs, concretely

1. Mouse: down/up with buttons 0/1/2 and `+4` for double clicks, plus `Mouse.addListener` events with `(button, target)`; the native `onPress`/`onRelease` only for the left button without arguments. `Mouse.getTopMostEntity(false)` returning handler clips (gui3b).
2. World gate: one flag, "pointer is over a movie hit" = any visible hit-testable shape of any open movie under the pointer (the engine uses the movie hit list of the frame; a port with Ruffle can use `getTopMostEntity(true)` over the HUD movies, any non-undefined result, minus `hitTestDisable` clips). When the flag is set, no world command (move, attack, use, talk, select) is generated from a mouse click.
3. Portrait hover: the `GameModeExplore.GUITargetedPlayer` SetValue/GetValue of the data tree has to exist and feed the port's cursor-target logic (cursor object := that id when nonzero and not -1, over the pick result); that is how a click on a portrait selects (left) or commands (right) a party member.
4. The quickbar needs no right-click code. Do not add a right-click menu, details panel or slot clearing: vanilla has none (a right press on a slot is silent, no sound either: `fnEventHandler` plays none, and `PushButton`'s click sound is in the left press handler, PushButton.as:116-121).
5. The label "Right-click" for `PerformPrimaryAction` in the tooltips stays (the key text table in dev/GUI.md), with the left-handed swap flag.

### Not read

- Whether, for one right click, the world's `PerformPrimaryAction` mapping and the `GUICursorRightButtonDown` mapping are both always active (input contexts could block one; the bindings were read, the contexts' enable/block lists were not).
- The meaning of the creature-kind branches of `0x0070CAD0` (the `0x631830` test = "is in the active party" and the `+0xC60` word 0 test are inferred); a play test of a right click on another portrait would settle talk vs nothing.
- The `SelectPartyMemberUnderCursor` mapping (left click on a portrait) and what a click on a quickbar slot does to `GUITargetedPlayer` / the selection (the hover value stays at what the last portrait left it, 0 after the rollout).
- Right click while a drag is in progress or while ability targeting is active (cancel?): no `Mouse.RIGHT` use in the quickbar or portraits AS; the engine's targeting-mode cancel was not looked for.
- GFx's native `onPress` for non-left buttons (inferred not fired); `GUI/Element.as` `_root.IsDoubleClick` readers.

## Still to read

- The data tree movies bind to. Reading it is `dao/Data.h`: `SmartData::TokenManager` is the root, and its `Find(path)` walks a dotted path to an attribute, which gives its value as text or a number. A plain attribute can be set (`Attribute::Set`, its class's slot 21, which notifies observers when the value changes). Still to read are making one, and the token observers `AddTokenObserver` registers, through which the game pushes a changed value to a movie. All three are needed for a plugin to publish values a movie shows.
- Portraits, read 2026-09-30. A creature's portrait is rendered live into a texture of its own (`CCCreature::UpdatePortrait`, `dao/Addresses.h`). A movie shows it as `img://portrait_<tag><counter>` (the player's is `portrait_player0`), the texture's name. `creature->RenderPortrait()` renders it and returns that URL, for any creature. Not yet seen in play; `UpdatePortraitMessage` and the paperdoll are still to read.
- Releasing a GFx value a movie hands out, to read variables and `Invoke`'s results.
