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

Every screen is a `GUI::GUIScene`, built as `GUIScene(name, filename, layer, b, flags)` and kept by the GUI system by id and name. `OpenMovie`'s first argument is the scene's id. The layer is the stacking order, going by the values the game's 42 scenes use: 0 floaty text, 1 the HUD (`GUIMain`), 2–3 in-game menus, 4 item examine, 5 conversation, death and the main menu, 6 options and login, 7 popups, 8 loading screens. Flag `0x20` marks the modal screens, `0x10` loading screens. `b` is 10 for the character record, journal, map, skills and spells, and 0 for most others; its meaning is not read. So a plugin's HUD-like movie belongs on layer 1. The GUI system (the application's +0x158) registers its scenes with `AddScene(id, scene)`, ids 0–0x3F, each scene 0xC0 bytes from the engine's allocator. `dao::gui::AddScreen` does the same with an id from 0x100, and `ShowScreen`, `HideScreen` and `CloseScreen` are the GUI system's own by-name slots 17, 19 and 18. The probe logs every `OpenMovie` to confirm the ids and values in play. Where the game finds a movie's file (`guiexport.erf`, an add-in's resources, an override folder) is the other: GFx asks `EclipseGFxFileOpener`.

## Still to read

- The data tree movies bind to. Reading it is `dao/Data.h`: `SmartData::TokenManager` is the root, and its `Find(path)` walks a dotted path to an attribute, which gives its value as text or a number. A plain attribute can be set (`Attribute::Set`, its class's slot 21, which notifies observers when the value changes). Still to read are making one, and the token observers `AddTokenObserver` registers, through which the game pushes a changed value to a movie. All three are needed for a plugin to publish values a movie shows.
- Portraits: `UpdatePortraitMessage`, `GUI::GUIPaperdoll`, `EclipseGFxImageLoader`.
- Releasing a GFx value a movie hands out, to read variables and `Invoke`'s results.
