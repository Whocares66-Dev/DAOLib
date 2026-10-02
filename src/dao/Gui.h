// The game's Flash UI (Scaleform GFx 3.x), as a plugin talks to it: the
// calls a movie's ActionScript makes into the game, FSCommands and
// ExternalInterface calls, seen, answered, or taken over by name.
//
//   dao::gui::OnExternalInterface("MyMod_GetTurn", [](const dao::gui::Call &call) {
//       call.Return(dao::gui::Value::Number(g_turn));
//   });
//   dao::gui::OnFSCommand("MyMod_EndTurn", [](void *owner, std::string_view args) { EndTurn(); });
//
// And a plugin calls into a movie the game shows:
//
//   dao::gui::Movie movie{dao::gui::MovieOf(owner)};
//   movie.SetVariable("_root.TurnBar._visible", dao::gui::Value::Bool(true));
//   const dao::gui::Value cards[] = {dao::gui::Value::Number(3)};
//   movie.Invoke("_root.TurnBar.SetCardCount", cards);
//
// In ActionScript, `ExternalInterface.call("MyMod_GetTurn")` and
// `fscommand("MyMod_EndTurn", "")` reach those, from any of the game's
// movies or a mod's own.
//
// Read 2026-09-30. The game gives GFx one FSCommand handler and one
// ExternalInterface handler (EclipseGFxFSCommandHandler and
// EclipseGFxExternalInterface, whose callbacks are their vtables' slot 1).
// Both pass the call to the object in 0x00C31264, most likely
// GUI::ScaleformManager (the example plugin logs its class), which asks
// the movie for its user data (GFxMovie's slot 43): the game's object
// that owns the movie, with the movie at its +0x1C. Then it looks the
// call's name up in a registry of handlers by name, at the manager's
// +0xA8, one map for FSCommands and one for ExternalInterface calls,
// keyed by the name's CRC-32. The game registers 67 FSCommands and 17
// ExternalInterface calls there (GetObjectNameByID, HasAbility,
// ListTacticTargets, SendMessage, GetValue, ...). This header detours the
// two lookups, so it sees every call with its name, and a plugin's
// handler for a name runs in place of the game's.
//
// GFx runs FSCommand and ExternalInterface handlers while it advances a
// movie or runs an Invoke on it (Scaleform's documentation, and the game's
// own handlers are shaped for it): a handler runs on the thread that
// advances the movie, and a plugin's Movie::Invoke can have a handler run
// before it returns. Handlers should return quickly.
//
// Not yet seen in play: the example plugin logs the first calls, their
// movies' names and the thread they come on, and every movie's name.
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

// GFx's GFxValue, as ExternalInterface calls pass their arguments and
// take their answer: 16 bytes, the type at +0 and the value at +8 (the
// game's handlers read argument i at 16 * i + 8, and answer a number as
// type 3, the value a double at +8). Only the types a call passes by value
// are read here; objects, arrays and display objects are GFx's own. The
// numbering is GFx 3.x's, and the game's code agrees on three of them (3
// a number, 4 a string argument, 5 a wide string answer); Scaleform 4
// inserted Int and UInt at 3 and 4, so its documentation's numbers do not
// apply here. A string or object GFx hands out is managed (0x40 beside the
// type) and must be released through GFx before its movie goes; how is not
// read yet, which is why nothing here reads a movie's values back.
struct Value
{
    enum class Type : std::uint32_t
    {
        Undefined = 0,
        Null = 1,
        Boolean = 2,
        Number = 3,
        String = 4,
        StringW = 5,
        Object = 6,
        Array = 7,
        DisplayObject = 8,
    };

    std::uint32_t type = 0;
    const void *objectInterface = nullptr; // not read yet; null for plain values
    union {
        double number;
        bool boolean;
        const char *string;
        const wchar_t *wstring;
    } value{};

    // GFx marks a value it manages with 0x40 beside the type.
    Type Kind() const
    {
        return static_cast<Type>(type & 0x0F);
    }
    std::optional<double> AsNumber() const
    {
        return Kind() == Type::Number ? std::optional{value.number} : std::nullopt;
    }
    std::optional<bool> AsBool() const
    {
        return Kind() == Type::Boolean ? std::optional{value.boolean} : std::nullopt;
    }
    std::optional<std::string_view> AsString() const
    {
        return Kind() == Type::String && value.string != nullptr ? std::optional<std::string_view>{value.string}
                                                                 : std::nullopt;
    }
    std::optional<std::wstring_view> AsWString() const
    {
        return Kind() == Type::StringW && value.wstring != nullptr ? std::optional<std::wstring_view>{value.wstring}
                                                                   : std::nullopt;
    }

    static Value Number(double n)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::Number);
        v.value.number = n;
        return v;
    }
    static Value Bool(bool b)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::Boolean);
        v.value.boolean = b;
        return v;
    }
    // The text must outlive the call it is passed to; GFx copies it there.
    static Value String(const char *text)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::String);
        v.value.string = text;
        return v;
    }
    static Value WString(const wchar_t *text)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::StringW);
        v.value.wstring = text;
        return v;
    }
};
static_assert(sizeof(Value) == 16);

// A Flash movie: GFx's GFxMovieView (its class GFxMovieRoot), as the game's
// UI objects hold it (MovieOf). Its vtable, read 2026-09-30 from the
// game's own calls (the paths it passes, _root.OriginalStageWidth,
// _root.fsc_retval, the floaty layer's batch updates), is GFx 3.0's
// GFxMovie in order, and each slot's `ret` agrees: 9 GetVisible, 10
// IsAvailable(path), 11 SetVariable(path, value, setType), 12
// GetVariable(out, path), 13 SetVariableArray(type, path, index, data,
// count, setType), 14 SetVariableArraySize(path, count, setType), 15
// GetVariableArraySize(path), 16 GetVariableArray(type, path, index, data,
// count), 17 the variadic Invoke (__cdecl), 18 Invoke(method, result,
// args, count), 19 InvokeArgs, 42 SetExternalInterfaceRetVal, 43
// GetUserData. No slots for GFx 3.1's CreateString and CreateObject sit
// before SetVariable, so this is the 3.0 interface.
//
// SetArray and Invoke are seen in play: dao/Options.h delivers its
// checkboxes' states with them (SetArray on TokenManager's two arrays, then
// Invoke("TokenManager.PerformBatchTokenReplacements"), 2026-10-01), and the
// boxes showed them. A path needs no "_root." in front. Call it on the
// thread the movie's own calls come on (an FSCommand or ExternalInterface
// handler runs there); the frame's thread is not shown to be that one.
class Movie
{
  public:
    explicit Movie(void *view) : view_(view)
    {
    }
    explicit operator bool() const
    {
        return view_ != nullptr;
    }
    void *View() const
    {
        return view_;
    }

    // How a set variable holds (GFx's SetVarType). The game sets with
    // Sticky, GFx's default: kept for a clip not loaded yet, and set on it
    // when it is. Normal is cheaper, for a variable already there.
    enum class SetType : std::int32_t
    {
        Normal = 0,
        Sticky = 1,
        Permanent = 2,
    };

    // Whether a path names something in the movie now.
    bool IsAvailable(const char *path) const;
    bool SetVariable(const char *path, const Value &value, SetType type = SetType::Sticky);
    // An ActionScript array at a path, from index on, as the game fills
    // _root.fsc_retval and the floaty layer's batch updates (GFx's
    // SetArrayType: 0 int, 1 double, 4 wide string, 5 value).
    bool SetArray(const char *path, std::span<const std::int32_t> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    bool SetArray(const char *path, std::span<const double> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    bool SetArray(const char *path, std::span<const wchar_t *const> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    bool SetArray(const char *path, std::span<const Value> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    bool SetArraySize(const char *path, std::uint32_t count, SetType type = SetType::Sticky);
    std::uint32_t ArraySize(const char *path) const;
    // Calls an ActionScript function by its path; true if it ran. What it
    // returns is not read: GFx hands strings and objects out as references
    // of its own to release.
    bool Invoke(const char *method, std::span<const Value> args = {});

  private:
    void *view_;
};

// An ExternalInterface call as a handler sees it.
struct Call
{
    // The game's object that owns the calling movie (the movie's user
    // data); its class names the screen (dao::TypeName).
    void *owner = nullptr;
    std::string_view name;
    std::span<const Value> args;

    // Answers the call: what ExternalInterface.call returns to the movie
    // (the movie's slot 42, as the game's handlers answer).
    void Return(const Value &answer) const;
};

using FSCommandHandler = std::function<void(void *owner, std::string_view args)>;
using ExternalInterfaceHandler = std::function<void(const Call &call)>;

// A handler for an FSCommand or ExternalInterface call by name, run in
// place of the game's own handler for that name if it has one. Names are
// case-sensitive, as the game's are. At plugin load or later; the first
// call detours the engine's lookup.
void OnFSCommand(std::string name, FSCommandHandler handler);
void OnExternalInterface(std::string name, ExternalInterfaceHandler handler);

// The same, for a plugin that wants only some of the calls of a name: each
// is asked in the order added and says whether it took the call. One it
// did not take goes on to the next, then to a handler above or the game's
// own. dao/Options.h takes its own checkboxes' clicks this way out of the
// SendMessage every button of the game's sends.
void InterceptFSCommand(std::string name, std::function<bool(void *owner, std::string_view args)> filter);
void InterceptExternalInterface(std::string name, std::function<bool(const Call &call)> filter);

// Every FSCommand or ExternalInterface call, before it is handled. These
// observe; the call goes on to its handler.
void OnAnyFSCommand(std::function<void(void *owner, std::string_view name, std::string_view args)> observer);
void OnAnyExternalInterface(ExternalInterfaceHandler observer);

// The game's movies. Each is a record the manager keeps (dao/Addresses.h),
// the `owner` a call above carries: its name at +0, its movie at +0x1C.
// All of them, as the manager has them now; the one of a name; a record's
// name; and its movie, GFx's GFxMovieView (null for none).
std::vector<void *> Owners();
void *FindOwner(std::wstring_view name);
std::wstring_view NameOf(const void *owner);
void *MovieOf(const void *owner);

// A screen of a plugin's own, as the game has its 61: a GUI::GUIScene the
// GUI system keeps by name, whose movie it opens on a layer when shown
// (dev/GUI.md). The layer is the stacking order the game's scenes use;
// the flags are theirs too (0x20 modal, 0x10 a loading screen).
//
//   dao::gui::AddScreen(L"TurnOrder", L"turnorder", dao::gui::kLayerHud);
//   dao::gui::ShowScreen(L"TurnOrder");
//
// The movie is found as the game finds its own: a scene "turnorder" is the
// resource "turnorder.gfx", looked up by the engine's resource system in
// the places it looks for every resource (its override folders and the
// game's archives; HasMovie says whether it finds one). Add a
// screen once the game is up (the GUI system exists from the first frame
// of the main menu: a frame callback will do); show, hide and close it
// from the scripts' thread. Read 2026-09-30, not yet seen in play.
inline constexpr std::int32_t kLayerFloaty = 0; // the floating combat text
inline constexpr std::int32_t kLayerHud = 1;    // the HUD (GUIMain)
inline constexpr std::int32_t kLayerMenu = 3;   // the in-game menus: map, journal, skills
inline constexpr std::int32_t kLayerDialog = 5; // conversation, death, the main menu
inline constexpr std::int32_t kLayerOptions = 6;
inline constexpr std::int32_t kLayerPopup = 7;
inline constexpr std::int32_t kLayerLoading = 8;
inline constexpr std::uint32_t kScreenModal = 0x20;

// False if the GUI system is not up yet, or the name is taken.
bool AddScreen(std::wstring_view name, std::wstring_view movie, std::int32_t layer, std::uint32_t flags = 0);
// Opens the screen's movie, loading it if need be, and shows it.
bool ShowScreen(std::wstring_view name);
// Hides it, keeping it loaded.
bool HideScreen(std::wstring_view name);
// Closes it: the movie is let go.
bool CloseScreen(std::wstring_view name);
// Whether the resource system has a movie file by this name
// (L"turnorder.gfx"), as GFx would ask for it.
bool HasMovie(std::wstring_view file);

// Whether the mouse is over an element of one of the interface's movies,
// or one of them holds a mouse press: the engine's own test, which explore
// mode asks before it picks what is under the cursor, choosing nothing when
// the answer is yes (dao/GameModes.h). A plugin that draws at the cursor
// from the world's pick asks this to tell "over the interface" from "over
// nothing"; and a plugin's own movie under the cursor makes it true as the
// game's do. It changes with the mouse's events, not each frame. False
// before the interface is up. Seen in play, 2026-10-01 and 02, in the GOG,
// EA and Steam games alike (examples/world logs each change): yes at the
// main menu, no with the cursor over the world, yes while the game's menu
// was open and no when it closed.
bool MouseOverInterface();

// A movie of the game's own, changed as the game opens its file: the way
// to add to a screen whose contents are written in its script, as the
// controls screen's list of actions is (dao/Input.h does it with this).
//
//   dao::gui::OnMovieLoad(L"optionsmenu", [](std::span<const std::byte> movie) {
//       return dao::core::gfx::InsertAfter(movie, {"AddKeyBinding", "HideMainInterface"}, code);
//   });
//
// The patch is given the movie uncompressed (dao/core/GfxMovie.h) and
// answers the changed movie, or nothing to leave it be. It runs once, the
// first time the game opens that file, on the thread that opens it, and
// what it answers is what the game reads then and every time after; so
// add patches at plugin load. Several patches for one movie run in the
// order they were added, each on what the one before left; and so do the
// patches of several plugins, each plugin's on what the others' left, in
// no promised order (seen in play, 2026-10-01: one plugin's key rows and
// another's section on the Game tab, in one opening of the screen).
// Nothing is written to disk.
//
// The name is the movie's file without its extension ("optionsmenu" for
// optionsmenu.gfx), in any case. Read 2026-10-01 (dao/Addresses.h,
// EclipseGFxFileOpener::OpenFile): the game's file object reads from the
// resource's bytes in memory, and this points it at the patched bytes and
// answers their length. Seen in play the same day through dao/Input.h's
// rows in the controls screen: the patched options movie opened each
// time, twice in one session and again after a restart, its other tabs as
// ever.
using MoviePatch = std::function<std::optional<std::vector<std::byte>>(std::span<const std::byte> movie)>;
void OnMovieLoad(std::wstring_view name, MoviePatch patch);

// A movie of the game's replaced whole by a file of the plugin's: a copy of
// the game's movie changed in a Flash editor (FFDec, as SWF or GFx,
// compressed or not), shipped beside the plugin and given to the game only
// while the plugin is loaded, so that removing the plugin brings the game's
// back with nothing left in the game's folders. Where a change wants
// graphics of its own, or more than a script can add, this is the way; a
// few lines of script added to whichever movie the game has
// (core::gfx::AppendDoAction, through OnMovieLoad) is the way that leaves
// room for other plugins' changes to the same movie. The file is read now,
// at the call; false, and the game's movie left alone, if it cannot be read
// or is not a movie. A patch another plugin adds for that movie later
// applies to this file; one added before is set aside by it. Not yet seen
// in play.
bool ReplaceMovie(std::wstring_view name, const std::filesystem::path &file);

// A message box the game has up: its text and its buttons, for a tool that
// cannot see the screen (a test whose load has stopped at "this save
// requires the following content").
//
// Read 2026-10-01. The game's message boxes are one movie, the popup
// layer (GUI::GUIPopupLayer, built by 0x0070A071's function), which keeps
// what it shows in the data tree under GUIPopupLayer: DialogBoxText,
// NumButtons, and for each button a Label0 to Label3 and a Callback0 to
// Callback3, the message the game is sent when it is pressed, as text
// ("CClientExoApp.GUIDialogResult 1 0"); also an input box, a checkbox and
// a picker, not read here. The GUI system keeps whether a box is up at
// +0x1FA: set when it shows one (0x0060BA90, which then has the movie
// show it, PopupLayer.ShowDialogBoxFromGame) and cleared by
// DialogBoxClosedMessage. PressPopupButton presses one from a plugin.
// Seen in play, 2026-10-01, through the Eluvian server: after the scripts'
// ShowPopup(379897, 1) it gave the text "Shield Pummel", the string's own
// and not its number, and two buttons, Yes with the callback
// "GUIPopupLayer.ScriptDialogResult 1 1 -1" and No with "... 1 2 -1";
// nothing before the box was up. A script's box holds nothing: loads,
// orders and a fight went on under it.
struct Popup
{
    struct Button
    {
        std::wstring label;
        std::wstring callback;
    };
    std::wstring text;
    std::vector<Button> buttons;
};
// Nothing when no box is up, or before the GUI system is. On the game's
// thread.
std::optional<Popup> CurrentPopup();

// Presses a button of the box that is up, for a tool that drives the game:
// the game is sent the button's callback, as the movie sends it when the
// button is clicked (dao/Messages.h, SendText), and then, as the movie
// tells it once the box has gone, that the box is closed
// (DialogBoxClosedMessage), and the popup layer's screen is hidden. False,
// and nothing done, when no box is up, for a button the box has not, or a
// callback the engine cannot make a message of. Read 2026-10-01; what the
// movie itself does at a click is ActionScript and not read. Seen in play
// the same day, through the Eluvian server: with a script's Yes and No box
// up, pressing No answered true, CurrentPopup was nothing at once, and the
// game's own screenshot showed the box gone and the interface back.
// Whether the script heard the answer (EVENT_TYPE_POPUP_RESULT) is not yet
// seen.
bool PressPopupButton(std::size_t index);

// An FSCommand's answer, as the game's own handlers give theirs: an array
// of ints in the calling movie's _root.fsc_retval, which the ActionScript
// reads after its fscommand().
bool ReturnFSCommand(void *owner, std::span<const std::int32_t> values);

} // namespace dao::gui
