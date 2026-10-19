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

Addition, read 2026-10-18 by lib-agent-2 in `0x005111B0` (corrected 2026-10-18 by lib-agent-2): on a resize the engine sets NoScale (view +0x58, 0) and a viewport over the client area (+0x50). Below 1024 by 768 the viewport carries a scale, w/1024 or h/768, by the aspect against 4/3. `CreateMovieView` sets alignment centre (+0x60, 0).

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
2. **Corrections to dev/GUI.md.** (a) The scene's `b` (+0xA0) is a bit mask, not "10 for the record": bit 1 = world rendering switched off while shown (loading screens), bit 2 = a background-snapshot call on show, bit 4 = on hide play the outro (`PlayOutro`) instead of closing at once, bit 8 = shows the book background `BookBack` (0x3B) with it. (b) The layer sets draw order and input order: the renderer's and the manager's sets are keyed by it (below). Flags (+0xA4) go to the record's +0x24: 0x20 modal (mouse), 0x10 keeps running while paused/loading, 0x04 restart the movie on each show, 0x02 no frame catch-up, 0x01 = a render-target/cursor-related set-up in the record ctor (unclear).
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

**Loading screen choice** (checked, `GameModeLoading` 0x006C63F0, jump table 0x006C64FC on the loading kind 0..8): kinds 0 and 1 show nothing; 2 and 8 Loading 0xE; 3 Saving 0x24; 4 Loading 0xE unless 0x006C6060 finds the world map usable, in which case WorldMap 0x10 is the loading screen; 5 LightLoading 0x21 (same exception); 6 0x006C60F0 (fade path); 7 world-map-only; CustomLoading 0x23 is picked in 0x006C7F34 (a custom area image, inferred). `SetScreen(id)` is 0x006C5FE0 (hides the old, shows the new at once if the mode is current).

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
- ActionScript of the movies (not decompiled); whether `contextmenu`, `skills`, `mainui` exist in DLC archives.
- Scripts' exact use of every GUI command above (names from the command table only).

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
| PlayMovie 0x0051B790 | movie (video) file name | allocates a 0x20-byte `PlayMovieMessage` holding the name and posts it (0x4D20E0); `ScaleformManager::HandleMessage(PlayMovieMessage)` plays the video | nothing | checked |
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

- The observer's coalescing and the `updateDelay` float (the third triple element): stored by the observer constructor 0x00524A80; I assume it rate-limits notifications but did not read it.
- The caller chain that makes the minimap update (`GUIMap` slot 5 and the second call site 0x00634187), so its rate is inferred.
- Arguments of `Tooltips.StartDragging`, `Behaviour.DragAndDropable.StopExternalDrag`, `DebugOptions.*`, and the identity of the message `MovieRecord::Step` posts for `WaitingToBeStopped`.
- The `ActionBarFloaty` per-frame argument construction (names from the strings, shapes from the ActionScript).
- Other movies' ActionScript (inventory, journal, character record, store, map, loading, main menu): only token paths of the five HUD movies were enumerated. The Tooltips and PopupLayer movies were not decompiled.
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
- `ScaleformManager::LoadImage` (0x00516A20; `EclipseGFxImageLoader::LoadImage` slot 0x00517390 = `mov ecx,[0xC31264]; jmp 0x516A20`, `ret 4`) [C]: strips "imgps://" or "img://" (both take the same path, only the exception-state byte differs), finds `?Width=`, `&Height=` and `?Async=` from the end (0x004C0FB0 = find), parses them as ints (0x004C0950), cuts them off the name. If the name contains the wide strings at 0x00AF04A4 / 0x00AF04B4 (read as the 3-letter extensions "jpg" / "png") the image is treated as a file image [I]. If the name starts with `FILE:` the rest is loaded as a file through the resource/texture manager (singleton 0x008472C0; method 0x00845D80, which reads the setting at [0xC31650]); otherwise it is looked up as a texture by name (method 0x00846990), which is the normal resource lookup, so any 'name.dds' texture in the game's archives (or a live render target registered under that name) resolves. The result is wrapped in an image info (ctor 0x0090D340, 0x28 bytes) with width/height: the URL's if given, else the texture's (virtual +0x10 / +0x14 of the texture, from `SmartData::Attribute::Get` at 0x0053CC40 for the texture manager object) [I for the wrapper details].
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
- The texture manager's resolver (0x00846990 / 0x00845D80) and how the portrait render target gets its name registered there; the ".png"/".jpg" branch's purpose.
- Per-table icon columns (abi_base, itm) feeding movies; which movie uses "imgps://" (none found, so possibly only plugin or other-build movies).
- Gamepad: any controller path outside the manager.
- The designer-tag grammar `ReplaceDesignerTags` expands.
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
- The per-view resize routine at **0x005111B0** (called from the loop at 0x00511D95 over every movie, followed by `Invoke("MovieUtils.UpdateAllLayouts")` at 0x00511DC4) builds a viewport struct: buffer width/height = the client size, left/top 0 (or the scissor-like rect at record +0x28..0x34 when its flag at +0x24 is set), a float scale (1.0, or a factor from `[ecx+0x184]` of the application object when the window is smaller than 1024x768, via the 0x400/0x300 compare), then calls vtable **+0x58 with push 0** (SetViewScaleMode, 0 = SM_NoScale in GFx 3.0) at 0x00511330 and **+0x50** (SetViewport(&viewport)) at 0x0051133E. So: NoScale + centre alignment, and the viewport is the whole client area; the movie sees `Stage.width/height` equal to the view's size in pixels (matches GUI.md's "A movie's stage against the view": Stage.height read about 800, not the authored 720). The movie-side layout code (`ScaleToScreen`, `SceneManager.SetSafeZone`, FloatyLayer) then positions against 1024x768-authored coordinates and the live Stage size.
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
- How the engine fills a 1001 image (the code behind `ScaleformManager::LoadImage` 0x00516A20 and the GFx image creator) and what `?Async=1` does; I read the AS side and the atlas files only.
- The vtable slot names for 0x50/0x58/0x60 were taken from my memory of GFx 3.0's header, not from a vtable dump of GFxMovieRoot; the call sites, arguments and order are read.
- The other 56 movies' ActionScript were not decompiled (their shared `__Packages` are the same exports; movie-specific scripts such as tactics, chargen morph UI, worldmap may use more Flash 8 APIs). Whole-exe scan for other SetViewScaleMode/SetViewAlignment callers; what the 0xBF6154 / 0xAF5438 constants in the resize routine are; whether the application object's slot at +0x184 is a GUI scale option.
- Whether 3.0's `Mouse.LEFT+4` convention is a real GFx index scheme or a BioWare add-on (the lib only checks `Mouse.LEFT != undefined`).
- No Ruffle run was done on any movie (nothing was played or rendered).

## Still to read

- The data tree movies bind to. Reading it is `dao/Data.h`: `SmartData::TokenManager` is the root, and its `Find(path)` walks a dotted path to an attribute, which gives its value as text or a number. A plain attribute can be set (`Attribute::Set`, its class's slot 21, which notifies observers when the value changes). Still to read are making one, and the token observers `AddTokenObserver` registers, through which the game pushes a changed value to a movie. All three are needed for a plugin to publish values a movie shows.
- Portraits, read 2026-09-30. A creature's portrait is rendered live into a texture of its own (`CCCreature::UpdatePortrait`, `dao/Addresses.h`). A movie shows it as `img://portrait_<tag><counter>` (the player's is `portrait_player0`), the texture's name. `creature->RenderPortrait()` renders it and returns that URL, for any creature. Not yet seen in play; `UpdatePortraitMessage` and the paperdoll are still to read.
- Releasing a GFx value a movie hands out, to read variables and `Invoke`'s results.
