// Works with the game's Flash UI, which runs on Scaleform GFx 3.0. With it you can:
// - answer, watch or take over the calls a movie's ActionScript makes into the game, through
//   fscommand() (Flash's way to send the host a command name and a text argument) and
//   ExternalInterface.call();
// - read and set variables in the game's movies, and call their functions;
// - add screens of your own;
// - patch or replace the game's movies as they load.
//
//   dao::gui::OnExternalInterface("MyMod_GetTurn", [](const dao::gui::Call &call) {
//       call.Return(dao::gui::Value::Number(g_turn));
//   });
//   dao::gui::OnFSCommand("MyMod_EndTurn", [](void *owner, std::string_view args) { EndTurn(); });
//
// In ActionScript, `ExternalInterface.call("MyMod_GetTurn")` and
// `fscommand("MyMod_EndTurn", "")` reach those handlers, from any of the game's movies or a mod's
// own. To call into a movie the game is showing:
//
//   dao::gui::Movie movie{dao::gui::MovieOf(owner)};
//   movie.SetVariable("_root.TurnBar._visible", dao::gui::Value::Bool(true));
//   const dao::gui::Value cards[] = {dao::gui::Value::Number(3)};
//   movie.Invoke("_root.TurnBar.SetCardCount", cards);
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::gui
{

// Notes on Value: GFx's GFxValue is 16 bytes, the type at +0 and the value at +8 (the game's
// handlers read argument i at 16 * i + 8, and answer a number as type 3, the value a double at
// +8). The numbering is GFx 3.x's, and the game's code agrees on three of them (3 a number, 4 a
// string argument, 5 a wide string answer); Scaleform 4 inserted Int and UInt at 3 and 4, so its
// documentation's numbers do not apply here.
//
// This game's GFx is 3.0, which has no objects in a GFxValue (read 2026-10-04, DAOLib #24): the
// values it hands out, from GetVariable and from Invoke's answer (both through 0x00954910), are
// undefined, null, a bool, a number or a string, the string copied into a pool the movie keeps
// and empties at its next GetVariable or answered Invoke; an object, an array or a clip comes
// back as its string. And Invoke takes null, bool, number, string and wide string arguments,
// passing any other type, an object's included, to ActionScript as undefined (0x00956E80).
// Nothing is reference-counted, and +4 is not used. So Object, Array and DisplayObject are never
// given or taken.

// A value passed between the game and a movie (GFx's GFxValue): the arguments and return value
// of an ExternalInterface call, and a Movie's variables and arguments.
// This version of GFx passes only undefined, null, bools, numbers and strings. An object, array
// or clip read from a movie comes back as its string, and one passed into a movie reaches
// ActionScript as undefined.
struct Value
{
    // A value's type, numbered as in GFx 3.0.
    enum class Type : std::uint32_t
    {
        Undefined = 0,
        Null = 1,
        Boolean = 2,
        Number = 3,
        String = 4,        // a UTF-8 string
        StringW = 5,       // a wide string
        Object = 6,        // never used by this version of GFx
        Array = 7,         // never used by this version of GFx
        DisplayObject = 8, // never used by this version of GFx
    };

    std::uint32_t type = 0;                // a Type plus GFx's flags; read it through Kind
    const void *objectInterface = nullptr; // unused by this version of GFx; always null
    // The value. Which member holds it depends on the type.
    union {
        double number;
        bool boolean;
        const char *string;
        const wchar_t *wstring;
    } value{};

    // Returns the value's type, without the flag (0x40) that GFx adds to values it manages.
    Type Kind() const
    {
        return static_cast<Type>(type & 0x0F);
    }
    // Returns the number, or nullopt if the value is of another type.
    std::optional<double> AsNumber() const
    {
        return Kind() == Type::Number ? std::optional{value.number} : std::nullopt;
    }
    // Returns the bool, or nullopt if the value is of another type.
    std::optional<bool> AsBool() const
    {
        return Kind() == Type::Boolean ? std::optional{value.boolean} : std::nullopt;
    }
    // Returns the UTF-8 string, or nullopt if the value is of another type or a null string.
    std::optional<std::string_view> AsString() const
    {
        return Kind() == Type::String && value.string != nullptr ? std::optional<std::string_view>{value.string}
                                                                 : std::nullopt;
    }
    // Returns the wide string, or nullopt if the value is of another type or a null string.
    std::optional<std::wstring_view> AsWString() const
    {
        return Kind() == Type::StringW && value.wstring != nullptr ? std::optional<std::wstring_view>{value.wstring}
                                                                   : std::nullopt;
    }

    // Makes a number value.
    static Value Number(double n)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::Number);
        v.value.number = n;
        return v;
    }
    // Makes a bool value.
    static Value Bool(bool b)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::Boolean);
        v.value.boolean = b;
        return v;
    }
    // Makes a UTF-8 string value. The text is not copied, so keep it alive until the call you
    // pass it to returns; GFx copies it there.
    static Value String(const char *text)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::String);
        v.value.string = text;
        return v;
    }
    // Makes a wide string value. The text is not copied, so keep it alive until the call you pass
    // it to returns.
    static Value WString(const wchar_t *text)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::StringW);
        v.value.wstring = text;
        return v;
    }
};
static_assert(sizeof(Value) == 16);

// A Flash movie the game is showing (GFx's GFxMovieView). Use it to read and set the movie's
// ActionScript variables and to call its functions. Get one from MovieOf or ScreenMovie.
// Paths work with or without "_root." in front. Call a movie on the thread its own calls arrive
// on: FSCommand and ExternalInterface handlers run there, and in the GOG build so do frame
// callbacks.
class Movie
{
  public:
    // Wraps a GFxMovieView, as MovieOf returns it. A null view makes an empty Movie, on which
    // every call fails.
    explicit Movie(void *view) : view_(view)
    {
    }
    // Returns whether it holds a movie.
    explicit operator bool() const
    {
        return view_ != nullptr;
    }
    // Returns the GFxMovieView it wraps.
    void *View() const
    {
        return view_;
    }

    // How a variable keeps a value that is set (GFx's SetVarType).
    enum class SetType : std::int32_t
    {
        Normal = 0,    // sets only what exists now; cheaper, for a variable that already exists
        Sticky = 1,    // also applied to a clip not loaded yet, once it loads
        Permanent = 2, // GFx's Permanent mode
    };

    // Returns whether a path names something in the movie right now; false for an empty Movie.
    bool IsAvailable(const char *path) const;
    // Sets an ActionScript variable at a path, such as "_root.TurnBar._visible".
    // @param type how the value is kept; Sticky is GFx's default and what the game uses
    // @return true if GFx set it; false if it did not or the Movie is empty
    bool SetVariable(const char *path, const Value &value, SetType type = SetType::Sticky);
    // Sets elements of an ActionScript array at a path to ints, the way the game fills
    // _root.fsc_retval.
    // @param index the first element to set
    // @return true if GFx set them; false if it did not or the Movie is empty
    bool SetArray(const char *path, std::span<const std::int32_t> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    // Sets elements of an ActionScript array at a path to numbers, starting at index.
    bool SetArray(const char *path, std::span<const double> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    // Sets elements of an ActionScript array at a path to wide strings, starting at index.
    bool SetArray(const char *path, std::span<const wchar_t *const> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    // Sets elements of an ActionScript array at a path to values, starting at index.
    bool SetArray(const char *path, std::span<const Value> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    // Sets the length of an ActionScript array at a path.
    // @return true if GFx set it; false if it did not or the Movie is empty
    bool SetArraySize(const char *path, std::uint32_t count, SetType type = SetType::Sticky);
    // Returns the length of an ActionScript array at a path, as GFx reports it; 0 for an empty
    // Movie.
    std::uint32_t ArraySize(const char *path) const;
    // Calls an ActionScript function by its path, such as "_root.TurnBar.SetCardCount".
    // @return true if it ran
    //
    // Call does the same and also returns the function's return value. An FSCommand or
    // ExternalInterface handler can run before Invoke returns.
    bool Invoke(const char *method, std::span<const Value> args = {});

    // A value read back from a movie, from a path or a function call. An object, array or clip
    // comes back as text, as ActionScript's String() would give it; a clip gives its path.
    struct Answer
    {
        Value::Type type = Value::Type::Undefined;
        double number = 0;    // the value, for a Number
        bool boolean = false; // the value, for a Boolean
        std::string text;     // the value, for a string: UTF-8, copied out of the movie
    };
    // Returns the value at a path, for example a list's scroll position, a checkbox's state or a
    // label.
    // @return the value; nullopt if the movie has nothing at the path or the Movie is empty
    std::optional<Answer> Get(const char *path) const;
    // Calls an ActionScript function by its path and returns its return value.
    // @return the return value, of type Undefined if the function returns nothing; nullopt if the
    // function did not run
    std::optional<Answer> Call(const char *method, std::span<const Value> args = {});
    // Returns the number at a path, or nullopt if there is nothing there or it is not a number.
    std::optional<double> GetNumber(const char *path) const;
    // Returns the bool at a path, or nullopt if there is nothing there or it is not a bool.
    std::optional<bool> GetBool(const char *path) const;

  private:
    void *view_;
};

// An ExternalInterface call, as a handler receives it.
struct Call
{
    // The movie record of the calling movie: the game object that owns the movie (see Owners).
    // Its class name, from dao::TypeName, tells you which screen made the call.
    void *owner = nullptr;
    std::string_view name;       // the name the movie called
    std::span<const Value> args; // the arguments, valid while the handler runs

    // Sets the value ExternalInterface.call returns to the movie. Does nothing if the owner has
    // no movie.
    void Return(const Value &answer) const;
};

// Handles an FSCommand. It is given the calling movie's owner (as in Call::owner) and the
// command's argument text.
using FSCommandHandler = std::function<void(void *owner, std::string_view args)>;
// Handles an ExternalInterface call.
using ExternalInterfaceHandler = std::function<void(const Call &call)>;

// Handles every FSCommand with a given name, replacing the game's own handler for that name if
// there is one.
// @param name the command's name; case-sensitive, like the game's
//
// Call it at plugin load or later. A second handler for the same name replaces the first. The
// handler runs on the thread that advances the movie, possibly inside a plugin's call to
// Movie::Invoke, so it should return quickly.
void OnFSCommand(std::string name, FSCommandHandler handler);
// Handles every ExternalInterface call with a given name, replacing the game's own handler for
// that name, like OnFSCommand. The handler sets the return value through Call::Return.
void OnExternalInterface(std::string name, ExternalInterfaceHandler handler);

// Lets a filter take some of the FSCommands with a given name. The filter returns true to take a
// call.
// Filters for a name are asked in the order they were added. A call that no filter takes goes on
// to the OnFSCommand handler, or to the game's own. For example, dao/Options.h uses this to take
// its checkboxes' clicks out of the SendMessage command that every button in the game sends.
void InterceptFSCommand(std::string name, std::function<bool(void *owner, std::string_view args)> filter);
// Lets a filter take some of the ExternalInterface calls with a given name, like
// InterceptFSCommand.
void InterceptExternalInterface(std::string name, std::function<bool(const Call &call)> filter);

// Calls an observer for every FSCommand, before any filter or handler. The call then goes on to
// its handler as usual.
void OnAnyFSCommand(std::function<void(void *owner, std::string_view name, std::string_view args)> observer);
// Calls an observer for every ExternalInterface call, before any filter or handler.
void OnAnyExternalInterface(ExternalInterfaceHandler observer);

// Returns the GUI manager's movie records, one for each movie the game has open. A movie record
// is the game object that owns a movie; it is the `owner` passed with each FSCommand or
// ExternalInterface call.
// @return the records; empty before the GUI manager exists
std::vector<void *> Owners();
// Returns the record of the movie with a given name, such as L"inventory".
// @return the record, or null if there is none
void *FindOwner(std::wstring_view name);
// Returns the name of a record's movie, or an empty view for null. The view points into the
// record.
std::wstring_view NameOf(const void *owner);
// Returns a record's movie (GFx's GFxMovieView), ready to wrap in a Movie.
// @return the movie, or null for a null record or one without a movie
void *MovieOf(const void *owner);

// The layers a screen can be shown on, from the bottom up, and what the game's own screens use
// them for.

inline constexpr std::int32_t kLayerFloaty = 0;     // the floating combat text
inline constexpr std::int32_t kLayerHud = 1;        // the HUD (GUIMain)
inline constexpr std::int32_t kLayerMenu = 3;       // in-game menus: the map, journal and skills
inline constexpr std::int32_t kLayerDialog = 5;     // conversations, death, the main menu
inline constexpr std::int32_t kLayerOptions = 6;    // the options screen
inline constexpr std::int32_t kLayerPopup = 7;      // message boxes
inline constexpr std::int32_t kLayerLoading = 8;    // loading screens
inline constexpr std::uint32_t kScreenModal = 0x20; // AddScreen's flag for a modal screen

// Adds a screen of your own to the GUI system, alongside the game's. A screen is kept by name,
// and the GUI system opens its movie on a layer when the screen is shown (dev/GUI.md).
//
//   dao::gui::AddScreen(L"TurnOrder", L"turnorder", dao::gui::kLayerHud);
//   dao::gui::ShowScreen(L"TurnOrder");
//
// @param name the screen's name, for ShowScreen, HideScreen and CloseScreen
// @param movie the movie's name without its extension. For "turnorder", turnorder.gfx is looked
// for in the game's resources (its override folders and archives), and then in the plugin's own
// folder as daolib\plugins\<name>\turnorder.gfx or .swf
// @param layer the stacking order: one of the kLayer constants
// @param flags kScreenModal for a modal screen, 0x10 for a loading screen, or 0
// @return false if the GUI system is not up yet or the name is already taken
//
// The GUI system exists from the first frame of the main menu, so a frame callback can add a
// screen. HasMovie tells you whether a movie can be found.
bool AddScreen(std::wstring_view name, std::wstring_view movie, std::int32_t layer, std::uint32_t flags = 0);
// Opens a screen's movie, loading it if needed, and shows it.
// @return false before the GUI system is up; otherwise true, even for a name it does not know
//
// Call it on the game thread, like HideScreen and CloseScreen.
bool ShowScreen(std::wstring_view name);
// Hides a screen but keeps its movie loaded. Returns false before the GUI system is up.
bool HideScreen(std::wstring_view name);
// Closes a screen and releases its movie. Returns false before the GUI system is up.
bool CloseScreen(std::wstring_view name);
// Returns whether a movie file can be found in the places a screen's movie is looked for: the
// game's resources and the plugin's own folder.
// @param file the file's name with its extension, such as L"turnorder.gfx"
bool HasMovie(std::wstring_view file);

// A screen the GUI system keeps, either the game's or a plugin's.
struct Screen
{
    std::int32_t id = -1;    // the GUI system's ID: 0-0x3F the game's, 0x100 and up plugins'
    std::wstring name;       // the name ShowScreen and the full-screen messages use: L"Inventory"
    std::wstring movie;      // its movie's name, as NameOf gives it: L"inventory"
    std::int32_t layer = 0;  // the stacking order: kLayerHud and the rest
    std::uint32_t flags = 0; // kScreenModal, or 0x10 for a loading screen
    bool shown = false;      // its movie is active and drawn; not the same as logically open
};
// Returns every screen the GUI system keeps, the game's and plugins', ordered by ID.
// @return the screens; empty before the GUI system is up
//
// Call it on the game thread, like ShownScreens and CurrentFullScreen.
std::vector<Screen> Screens();
// Returns the screens being shown, top layer first. A tool that cannot see the screen can use it
// to find out whether the inventory, a store or the map is open.
//
//   for (const dao::gui::Screen &screen : dao::gui::ShownScreens())
//       log(screen.name, screen.layer);
//
// The popup layer counts as shown even with no box up; CurrentPopup tells you whether a box is
// up.
std::vector<Screen> ShownScreens();
// Returns the full-screen interface that is open: the inventory, the character record, the map,
// the journal, the tactics screen or the level-up screen. At most one is open at a time.
//
//   if (const auto full = dao::gui::CurrentFullScreen()) ...  // full->name == L"Inventory"
//
// @return the screen; nullopt if none is open or the GUI system is not up yet
std::optional<Screen> CurrentFullScreen();

// Returns the movie of a shown screen, by the screen's name. Use it to drive the screen's own
// controls through its ActionScript, as a click would, where no engine message does the job.
//
//   if (auto options = dao::gui::ScreenMovie(L"Options"))
//   {
//       const dao::gui::Value page[] = {dao::gui::Value::String("Controls")};
//       options->Invoke("SceneManager.PlayScene", page); // a function of optionsmenu.gfx
//       const dao::gui::Value bottom[] = {dao::gui::Value::Number(100)};
//       options->Invoke("_root.KeyBindings_mc.SetScrollPercentage", bottom);
//   }
//
// @return the movie; nullopt if the screen is not shown or has no movie
std::optional<Movie> ScreenMovie(std::wstring_view screen);

// Calls an observer when a full-screen interface opens or closes, whatever opened it.
// @param observer given the screen's name as CurrentFullScreen gives it, such as "Inventory" or
// "Tactics", and true when it opened or false when it closed
//
// The screens are checked every frame, so the observer runs on the first frame after the change,
// on the game thread, with the screen's movie open. ScreenMovie and LoadInto work inside it. When
// one screen closes as another opens, the observer hears about the close first.
void OnFullScreen(std::function<void(std::wstring_view name, bool shown)> observer);

// Loads your plugin's own movie into a new clip on the root of a game movie. Use it to add an
// element to a game screen, drawn with the screen's own art. The loaded movie shares the game
// movie's _root, so its script can attachMovie the game movie's exported symbols, for example the
// tactics screen's "OnOffButton", and read the screen's values in the data tree (dao/Data.h). It
// talks to the plugin through fscommand and ExternalInterface (OnFSCommand, OnExternalInterface).
//
//   dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
//       if (name == L"Tactics" && shown)
//           if (auto movie = dao::gui::ScreenMovie(L"Tactics"))
//               dao::gui::LoadInto(*movie, "MyPluginTactics_mc", L"myplugin_tactics");
//   });
//
// @param movie the game's movie, as ScreenMovie returns it
// @param clip the new clip's name on the movie's root; do not use a name the loaded script
// attaches inside it
// @param ownMovie your movie's file name without its extension, from the plugin's folder (.gfx
// or .swf)
// @return true if the load was requested, or if the clip is already there with its movie loaded;
// false if the plugin's folder has no such movie or the clip could not be created
//
// The movie arrives on the game movie's next update. Closing the screen removes the clip, so call
// this each time the screen opens; a clip whose load never arrived is created again. In the
// loaded script, wait for a game element's m_bLoaded before using it, as the screen's own rows
// do.
bool LoadInto(Movie movie, std::string_view clip, std::wstring_view ownMovie);

// Returns whether the mouse is over an element of one of the interface's movies, or one of them
// is holding a mouse press. Explore mode makes the same check before it works out what is under
// the cursor, and selects nothing when the answer is true (dao/GameModes.h).
// @return the engine's answer; false before the interface is up
//
// A plugin that draws at the point the cursor is over in the world can use this to tell "over
// the interface" from "over nothing". A plugin's own movie under the cursor counts, like the
// game's. The answer changes with mouse events, not every frame.
bool MouseOverInterface();

// A patch to a movie. It is given the movie uncompressed (dao/core/GfxMovie.h) and returns the
// changed movie, or nullopt to leave it unchanged.
using MoviePatch = std::function<std::optional<std::vector<std::byte>>(std::span<const std::byte> movie)>;
// Patches one of the game's movies as the game opens its file. Use it to add to a screen whose
// contents are built by its script; for example, dao/Input.h adds rows to the controls screen
// this way.
//
//   dao::gui::OnMovieLoad(L"optionsmenu", [](std::span<const std::byte> movie) {
//       return dao::core::gfx::InsertAfter(movie, {"AddKeyBinding", "HideMainInterface"}, code);
//   });
//
// @param name the movie's file name without its extension, in any case: "optionsmenu" for
// optionsmenu.gfx
//
// The patch runs once, on the thread that opens the file, the first time the game opens it. The
// game uses the result from then on, so add patches at plugin load. Patches for one movie run in
// the order they were added, each on the previous one's result. Other plugins' patches run too,
// in no guaranteed order. Nothing is written to disk.
void OnMovieLoad(std::wstring_view name, MoviePatch patch);

// Replaces one of the game's movies with a file from your plugin, for as long as the plugin is
// loaded. The file is a copy of the game's movie changed in a Flash editor such as FFDec, as SWF
// or GFx, compressed or not.
// @param name the movie's name, as OnMovieLoad takes it
// @param file the replacement, read during the call
// @return false, leaving the game's movie alone, if the file cannot be read or is not a movie
//
// Use it where a change needs graphics of its own or more than a script can add. To leave room
// for other plugins' changes to the same movie, add script through OnMovieLoad instead
// (core::gfx::AppendDoAction). A copy of the game's movie is BioWare's file, so a plugin that
// ships it is distributing that file. A patch added later for the same movie applies to this
// file; one added earlier is set aside.
bool ReplaceMovie(std::wstring_view name, const std::filesystem::path &file);

// A message box the game is showing: its text and its buttons.
struct Popup
{
    // A button in the box.
    struct Button
    {
        std::wstring label;    // the button's text
        std::wstring callback; // the message pressing it sends, as text for messages::SendText
    };
    std::wstring text; // the box's text, looked up from its string reference
    std::vector<Button> buttons;
};
// Returns the message box the game is showing. A tool that cannot see the screen can use it, for
// example a test whose load has stopped at "this save requires the following content".
// @return the box; nullopt when no box is up, or before the GUI system is up
//
// Call it on the game thread. The box's input field, checkbox and picker are not read.
std::optional<Popup> CurrentPopup();

// Presses a button of the message box that is up, as a click does. The game receives the
// button's callback, then is told that the box has closed, and the popup layer's screen is
// hidden.
// @param index the button's position in CurrentPopup's list, from 0
// @return false, with nothing done, when no box is up, for a button the box does not have, or
// for a callback the engine cannot turn into a message
bool PressPopupButton(std::size_t index);

// Returns values to an FSCommand the way the game's own handlers do: sets an array of ints at the
// calling movie's _root.fsc_retval, which the ActionScript reads after its fscommand() call.
// @param owner the owner the FSCommand handler was given
// @return false if the owner has no movie or GFx could not set the array
bool ReturnFSCommand(void *owner, std::span<const std::int32_t> values);

} // namespace dao::gui
