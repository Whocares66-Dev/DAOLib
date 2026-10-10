// The Flash UI's calls into the game, the movies' records and views, the GUI system's screens, and
// the movie files as GFx opens them.
//
// Read 2026-09-30. The game gives GFx one FSCommand handler and one ExternalInterface handler
// (EclipseGFxFSCommandHandler and EclipseGFxExternalInterface, whose callbacks are their vtables'
// slot 1). Both pass the call to the object in 0x00C31264, most likely GUI::ScaleformManager (the
// example plugin logs its class), which asks the movie for its user data (GFxMovie's slot 43):
// the game's object that owns the movie, with the movie at its +0x1C. Then it looks the call's
// name up in a registry of handlers by name, at the manager's +0xA8, one map for FSCommands and
// one for ExternalInterface calls, keyed by the name's CRC-32. The game registers 67 FSCommands
// and 17 ExternalInterface calls there (GetObjectNameByID, HasAbility, ListTacticTargets,
// SendMessage, GetValue, ...). This file detours the two lookups, so it sees every call with its
// name, and a plugin's handler for a name runs in place of the game's.
//
// GFx runs FSCommand and ExternalInterface handlers while it advances a movie or runs an Invoke
// on it (Scaleform's documentation, and the game's own handlers are shaped for it): a handler
// runs on the thread that advances the movie, and a plugin's Movie::Invoke can have a handler run
// before it returns. Handlers should return quickly.
//
// Not yet seen in play (as written 2026-09-30): the example plugin logs the first calls, their
// movies' names and the thread they come on, and every movie's name.
#include "dao/Gui.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Data.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/Resources.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/core/GfxMovie.h"
#include "dao/core/Msvc8.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dao::gui
{

namespace
{

using AnyFSCommand = std::function<void(void *owner, std::string_view name, std::string_view args)>;

std::mutex g_mutex;
std::map<std::string, FSCommandHandler, std::less<>> g_fsCommands;
std::map<std::string, ExternalInterfaceHandler, std::less<>> g_externalCalls;
std::vector<AnyFSCommand> g_anyFSCommand;
std::vector<ExternalInterfaceHandler> g_anyExternal;
using FSCommandFilter = std::function<bool(void *owner, std::string_view args)>;
using ExternalFilter = std::function<bool(const Call &call)>;
std::map<std::string, std::vector<FSCommandFilter>, std::less<>> g_fsCommandFilters;
std::map<std::string, std::vector<ExternalFilter>, std::less<>> g_externalFilters;

// The registry's lookups (dao/Addresses.h): __thiscall on the registry,
// the owner, the call's name, and its arguments, `ret 0xC` and `ret 0x10`.
using FSCommandFn = void(__fastcall *)(void *registry, int edx, void *owner, const char *command, const char *args);
using ExternalFn = void(__fastcall *)(void *registry, int edx, void *owner, const char *method, const Value *args,
                                      std::uint32_t count);
Detour<FSCommandFn> g_dispatchFSCommand;
Detour<ExternalFn> g_dispatchExternal;

void __fastcall DispatchFSCommand(void *registry, int edx, void *owner, const char *command, const char *args)
{
    const std::string_view name = command != nullptr ? command : "";
    const std::string_view text = args != nullptr ? args : "";
    std::vector<AnyFSCommand> observers;
    std::vector<FSCommandFilter> filters;
    FSCommandHandler handler;
    {
        const std::lock_guard lock(g_mutex);
        observers = g_anyFSCommand;
        if (const auto found = g_fsCommands.find(name); found != g_fsCommands.end())
        {
            handler = found->second;
        }
        if (const auto found = g_fsCommandFilters.find(name); found != g_fsCommandFilters.end())
        {
            filters = found->second;
        }
    }
    for (const auto &observer : observers)
    {
        detail::Guard("an FSCommand observer", [&] { observer(owner, name, text); });
    }
    for (const auto &filter : filters)
    {
        bool taken = false;
        detail::Guard("an FSCommand filter", [&] { taken = filter(owner, text); });
        if (taken)
        {
            return;
        }
    }
    if (handler)
    {
        detail::Guard("an FSCommand handler", [&] { handler(owner, text); });
        return;
    }
    g_dispatchFSCommand.Original()(registry, edx, owner, command, args);
}

void __fastcall DispatchExternal(void *registry, int edx, void *owner, const char *method, const Value *args,
                                 std::uint32_t count)
{
    const Call call{owner, method != nullptr ? method : "",
                    args != nullptr ? std::span<const Value>(args, count) : std::span<const Value>{}};
    std::vector<ExternalInterfaceHandler> observers;
    std::vector<ExternalFilter> filters;
    ExternalInterfaceHandler handler;
    {
        const std::lock_guard lock(g_mutex);
        observers = g_anyExternal;
        if (const auto found = g_externalCalls.find(call.name); found != g_externalCalls.end())
        {
            handler = found->second;
        }
        if (const auto found = g_externalFilters.find(call.name); found != g_externalFilters.end())
        {
            filters = found->second;
        }
    }
    for (const auto &observer : observers)
    {
        detail::Guard("an ExternalInterface observer", [&] { observer(call); });
    }
    for (const auto &filter : filters)
    {
        bool taken = false;
        detail::Guard("an ExternalInterface filter", [&] { taken = filter(call); });
        if (taken)
        {
            return;
        }
    }
    if (handler)
    {
        detail::Guard("an ExternalInterface handler", [&] { handler(call); });
        return;
    }
    g_dispatchExternal.Original()(registry, edx, owner, method, args, count);
}

// Under g_mutex.
void AttachLocked()
{
    if (g_dispatchFSCommand.Original() == nullptr)
    {
        g_dispatchFSCommand.Attach(addr::kGuiCommands_DispatchFSCommand, &DispatchFSCommand);
    }
    if (g_dispatchExternal.Original() == nullptr)
    {
        g_dispatchExternal.Attach(addr::kGuiCommands_DispatchExternalInterface, &DispatchExternal);
    }
}

} // namespace

namespace
{

// The engine's iterator over its set of movies (dao/Addresses.h).
struct SetIterator
{
    void *set = nullptr;
    std::byte *node = nullptr;
    bool valid = false;
};

void *Manager()
{
    return Global<void *>(addr::kGuiManager);
}

} // namespace

// The game's movies: each is a record the manager keeps (dao/Addresses.h), the `owner` a call
// carries: its name at +0, its movie at +0x1C.
std::vector<void *> Owners()
{
    std::vector<void *> owners;
    void *manager = Manager();
    if (manager == nullptr)
    {
        return owners;
    }
    void *set = static_cast<std::byte *>(manager) + 0x70;
    using StepFn = void *(__fastcall *)(void *, int, SetIterator *);
    const auto begin = reinterpret_cast<StepFn>(Module::Address(addr::kGuiMovieSet_Begin));
    const auto next = reinterpret_cast<StepFn>(Module::Address(addr::kGuiMovieSet_Next));
    SetIterator it;
    begin(set, 0, &it);
    // Past this many, something is wrong: the game has a few dozen.
    for (int guard = 0; it.valid && it.node != nullptr && guard < 1024; ++guard)
    {
        owners.push_back(*reinterpret_cast<void **>(it.node + 0xC));
        next(set, 0, &it);
    }
    return owners;
}

std::wstring_view NameOf(const void *owner)
{
    return owner != nullptr ? static_cast<const core::WStringRep *>(owner)->View() : std::wstring_view{};
}

void *FindOwner(std::wstring_view name)
{
    for (void *owner : Owners())
    {
        if (NameOf(owner) == name)
        {
            return owner;
        }
    }
    return nullptr;
}

void *MovieOf(const void *owner)
{
    if (owner == nullptr)
    {
        return nullptr;
    }
    const auto movie = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(owner) + 0x1C);
    return movie ? reinterpret_cast<void *>(*movie) : nullptr;
}

namespace
{

template <class Fn> Fn Slot(void *view, std::size_t slot)
{
    return (*static_cast<Fn *const *>(view))[slot];
}

// SetVariableArray, slot 13: __thiscall (type, path, index, data, count,
// setType), `ret 0x18`.
bool SetArrayOf(void *view, std::int32_t kind, const char *path, std::uint32_t index, const void *data,
                std::uint32_t count, Movie::SetType type)
{
    if (view == nullptr)
    {
        return false;
    }
    using Fn = bool(__fastcall *)(void *, int, std::int32_t, const char *, std::uint32_t, const void *, std::uint32_t,
                                  Movie::SetType);
    return Slot<Fn>(view, 13)(view, 0, kind, path, index, data, count, type);
}

} // namespace

// Movie is GFx's GFxMovieView (its class GFxMovieRoot), as the game's UI objects hold it
// (MovieOf). Its vtable, read 2026-09-30 from the game's own calls (the paths it passes,
// _root.OriginalStageWidth, _root.fsc_retval, the floaty layer's batch updates), is GFx 3.0's
// GFxMovie in order, and each slot's `ret` agrees: 9 GetVisible, 10 IsAvailable(path), 11
// SetVariable(path, value, setType), 12 GetVariable(out, path), 13 SetVariableArray(type, path,
// index, data, count, setType), 14 SetVariableArraySize(path, count, setType), 15
// GetVariableArraySize(path), 16 GetVariableArray(type, path, index, data, count), 17 the
// variadic Invoke (__cdecl), 18 Invoke(method, result, args, count), 19 InvokeArgs, 42
// SetExternalInterfaceRetVal, 43 GetUserData. No slots for GFx 3.1's CreateString and
// CreateObject sit before SetVariable, so this is the 3.0 interface. The game sets variables
// with Sticky, GFx's default.
//
// SetArray and Invoke are seen in play: dao/Options.h delivers its checkboxes' states with them
// (SetArray on TokenManager's two arrays, then
// Invoke("TokenManager.PerformBatchTokenReplacements"), 2026-10-01), and the boxes showed them.
// A path needs no "_root." in front, and works with it. Seen in play 2026-10-02 by the
// turn-based mod too: from inside its own movie's ExternalInterface handler it invoked the
// quickbar movie's XPBar_mc.SetValue(left, whole), SetTooltipSource and
// EffectBar_mc.UpdateXP("GUI.SelectedChar"), the game's experience bar showing what it set. Call
// it on the thread the movie's own calls come on (an FSCommand or ExternalInterface handler runs
// there). In the GOG game that was the frame's thread as well (the same thread id in the mod's
// handler and its frame callback, two runs); a handler is still the surer place.
bool Movie::IsAvailable(const char *path) const
{
    using Fn = bool(__fastcall *)(void *, int, const char *);
    return view_ != nullptr && Slot<Fn>(view_, 10)(view_, 0, path);
}

bool Movie::SetVariable(const char *path, const Value &value, SetType type)
{
    using Fn = bool(__fastcall *)(void *, int, const char *, const Value *, SetType);
    return view_ != nullptr && Slot<Fn>(view_, 11)(view_, 0, path, &value, type);
}

// An ActionScript array at a path, from index on, as the game fills _root.fsc_retval and the
// floaty layer's batch updates (GFx's SetArrayType: 0 int, 1 double, 4 wide string, 5 value).
bool Movie::SetArray(const char *path, std::span<const std::int32_t> values, std::uint32_t index, SetType type)
{
    return SetArrayOf(view_, 0, path, index, values.data(), static_cast<std::uint32_t>(values.size()), type);
}

bool Movie::SetArray(const char *path, std::span<const double> values, std::uint32_t index, SetType type)
{
    return SetArrayOf(view_, 1, path, index, values.data(), static_cast<std::uint32_t>(values.size()), type);
}

bool Movie::SetArray(const char *path, std::span<const wchar_t *const> values, std::uint32_t index, SetType type)
{
    // GFx takes the array of string pointers as its data.
    return SetArrayOf(view_, 4, path, index, static_cast<const void *>(values.data()),
                      static_cast<std::uint32_t>(values.size()), type);
}

bool Movie::SetArray(const char *path, std::span<const Value> values, std::uint32_t index, SetType type)
{
    return SetArrayOf(view_, 5, path, index, values.data(), static_cast<std::uint32_t>(values.size()), type);
}

bool Movie::SetArraySize(const char *path, std::uint32_t count, SetType type)
{
    using Fn = bool(__fastcall *)(void *, int, const char *, std::uint32_t, SetType);
    return view_ != nullptr && Slot<Fn>(view_, 14)(view_, 0, path, count, type);
}

std::uint32_t Movie::ArraySize(const char *path) const
{
    using Fn = std::uint32_t(__fastcall *)(void *, int, const char *);
    return view_ != nullptr ? Slot<Fn>(view_, 15)(view_, 0, path) : 0;
}

namespace
{

// A value as GFx handed it out, its text copied before the movie's next
// GetVariable or answered Invoke empties the pool it is in. `out` is given
// with type 0, so GFx gives each value its own type (0x10 with a type
// would ask for that one).
Movie::Answer ToAnswer(const Value &value)
{
    Movie::Answer answer;
    answer.type = value.Kind();
    answer.number = value.AsNumber().value_or(0);
    answer.boolean = value.AsBool().value_or(false);
    if (const auto text = value.AsString())
    {
        answer.text = std::string(*text);
    }
    else if (const auto wide = value.AsWString())
    {
        answer.text = Utf8(*wide);
    }
    return answer;
}

} // namespace

// GFx's GetVariable, slot 12, __thiscall (out, path), `ret 8`. This GFx has nothing to give but
// undefined, null, a bool, a number or a string (dao/Gui.h, Value).
//
// Get and Call seen in play 2026-10-05 through the Eluvian server, over seven rounds of the
// level-up screen's reads: numbers (nPointsToSpend 3, m_nMode 1), booleans (bLevelUp, _visible),
// a string (m_sRootToken "CharGenCreature.Stats.TalentChains"), null and undefined each where
// expected; Call's answer to Scrollable_mc.getNextHighestDepth a number, 3. A clip comes back as a
// String holding its path, never as DisplayObject.
std::optional<Movie::Answer> Movie::Get(const char *path) const
{
    if (view_ == nullptr)
    {
        return std::nullopt;
    }
    Value out;
    using Fn = bool(__fastcall *)(void *, int, Value *, const char *);
    if (!Slot<Fn>(view_, 12)(view_, 0, &out, path))
    {
        return std::nullopt;
    }
    return ToAnswer(out);
}

// Invoke, slot 18, its answer given.
std::optional<Movie::Answer> Movie::Call(const char *method, std::span<const Value> args)
{
    if (view_ == nullptr)
    {
        return std::nullopt;
    }
    Value out;
    using Fn = bool(__fastcall *)(void *, int, const char *, Value *, const Value *, std::uint32_t);
    if (!Slot<Fn>(view_, 18)(view_, 0, method, &out, args.data(), static_cast<std::uint32_t>(args.size())))
    {
        return std::nullopt;
    }
    return ToAnswer(out);
}

std::optional<double> Movie::GetNumber(const char *path) const
{
    const auto value = Get(path);
    return value && value->type == Value::Type::Number ? std::optional{value->number} : std::nullopt;
}

std::optional<bool> Movie::GetBool(const char *path) const
{
    const auto value = Get(path);
    return value && value->type == Value::Type::Boolean ? std::optional{value->boolean} : std::nullopt;
}

bool Movie::Invoke(const char *method, std::span<const Value> args)
{
    using Fn = bool(__fastcall *)(void *, int, const char *, Value *, const Value *, std::uint32_t);
    return view_ != nullptr &&
           Slot<Fn>(view_, 18)(view_, 0, method, nullptr, args.data(), static_cast<std::uint32_t>(args.size()));
}

namespace
{

// The application's GUI system, GUI::DragonAgeGUISystem, or null before
// it is made.
void *GuiSystem()
{
    const auto *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return nullptr;
    }
    const auto system = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + 0x158);
    if (!system || *system == 0)
    {
        return nullptr;
    }
    void *gui = reinterpret_cast<void *>(*system);
    return TypeName(gui).starts_with("GUI::") ? gui : nullptr;
}

// The ids a plugin's screens take, past the game's 0-0x3F.
constexpr std::int32_t kFirstPluginScreen = 0x100;

// The GUI system's scenes, id to GUI::GUIScene (dao/Addresses.h).
const core::TreeRep<std::int32_t, std::byte *> &Scenes(const void *gui)
{
    return *reinterpret_cast<const core::TreeRep<std::int32_t, std::byte *> *>(static_cast<const std::byte *>(gui) +
                                                                               0x58);
}

// The id for a plugin's new screen: past every id the GUI system has. Each
// plugin links its own copy of this library, so a count of its own would
// give two plugins' screens the same id, and AddScene ignores an id it has.
std::int32_t FreeScreenId(const void *gui)
{
    std::int32_t id = kFirstPluginScreen;
    Scenes(gui).ForEach([&](std::int32_t taken, const std::byte *) { id = taken >= id ? taken + 1 : id; });
    return id;
}

// The id the GUI system knows a scene by, or -1 (its slot 12).
std::int32_t SceneId(void *gui, std::wstring_view name)
{
    WString text(name);
    using Fn = std::int32_t(__fastcall *)(void *, int, const core::WStringRep *);
    return Slot<Fn>(gui, 12)(gui, 0, text.Engine());
}

// The GUI system's by-name slots (17 open, 18 close, 19 hide), each
// __thiscall (const std::wstring &name), `ret 4`. They return nothing, and
// do nothing for a name the system does not have, so the name is looked up
// first: false for a screen that is not there (DAOLib #46).
bool ByName(std::size_t slot, std::wstring_view name)
{
    void *gui = GuiSystem();
    if (gui == nullptr || SceneId(gui, name) < 0)
    {
        return false;
    }
    WString text(name);
    using Fn = void(__fastcall *)(void *, int, const core::WStringRep *);
    Slot<Fn>(gui, slot)(gui, 0, text.Engine());
    return true;
}

} // namespace

namespace
{
// Below, with the movie file's detour.
void AttachOpenFile();
} // namespace

// A screen of a plugin's own, as the game has its 61: a GUI::GUIScene the GUI system keeps by
// name, whose movie it opens on a layer when shown (dev/GUI.md). The layer is the stacking order
// the game's scenes use; the flags are theirs too (0x20 modal, 0x10 a loading screen).
//
// The movie is found as the game finds its own: a scene "turnorder" is the resource
// "turnorder.gfx", looked up by the engine's resource system in the places it looks for every
// resource (its override folders and the game's archives). Where the game has none by that name,
// the plugin's own folder is the place: daolib\plugins\<name>\turnorder.gfx (or .swf), read once
// and handed to GFx as the file the game's opener made, so the whole plugin is one folder (DAOLib
// #26; add_daolib_plugin's FILES puts it there). Add a screen once the game is up (the GUI system
// exists from the first frame of the main menu: a frame callback will do); show, hide and close
// it from the scripts' thread. Read 2026-09-30, not yet seen in play.
bool AddScreen(std::wstring_view name, std::wstring_view movie, std::int32_t layer, std::uint32_t flags)
{
    void *gui = GuiSystem();
    if (gui == nullptr || SceneId(gui, name) != -1)
    {
        return false;
    }
    // As the GUI system makes its own: 0xC0 bytes from the engine's
    // allocator, GUIScene(name, filename, layer, b, flags), AddScene. The
    // GUI system owns it from there.
    using NewFn = void *(__cdecl *)(std::size_t);
    void *scene = reinterpret_cast<NewFn>(Module::Address(addr::kOperatorNew))(0xC0);
    if (scene == nullptr)
    {
        return false;
    }
    WString sceneName(name);
    WString filename(movie);
    using SceneFn = void *(__fastcall *)(void *, int, const core::WStringRep *, const core::WStringRep *, std::int32_t,
                                         std::int32_t, std::uint32_t);
    reinterpret_cast<SceneFn>(Module::Address(addr::kGUIScene_Constructor))(scene, 0, sceneName.Engine(),
                                                                            filename.Engine(), layer, 0, flags);
    // A movie in this plugin's own folder is served from there, as the
    // screen opens it (DAOLib #26).
    std::error_code ec;
    const std::filesystem::path own = Module::OwnFolder() / movie;
    if (std::filesystem::is_regular_file(own.wstring() + L".gfx", ec) ||
        std::filesystem::is_regular_file(own.wstring() + L".swf", ec))
    {
        AttachOpenFile();
    }
    const std::int32_t id = FreeScreenId(gui);
    using AddFn = void(__fastcall *)(void *, int, std::int32_t, void *);
    reinterpret_cast<AddFn>(Module::Address(addr::kGUISystem_AddScene))(gui, 0, id, scene);
    return SceneId(gui, name) == id;
}

bool ShowScreen(std::wstring_view name)
{
    return ByName(17, name);
}

bool HideScreen(std::wstring_view name)
{
    return ByName(19, name);
}

bool CloseScreen(std::wstring_view name)
{
    return ByName(18, name);
}

namespace
{

// A GUI::GUIScene's fields (Screens, in dao/Gui.h).
constexpr std::size_t kSceneName = 0x58;
constexpr std::size_t kSceneMovie = 0x74 + 0xC; // the Filename attribute's text
constexpr std::size_t kSceneLayer = 0x9C;
constexpr std::size_t kSceneFlags = 0xA4;
// A movie record's active byte.
constexpr std::size_t kMovieActive = 0x28;
// The GUI system's std::vector<int> of full-screen scenes shown.
constexpr std::size_t kFullScreens = 0x1E4;

template <class T> T Field(const std::byte *object, std::size_t offset)
{
    return *reinterpret_cast<const T *>(object + offset);
}

std::vector<Screen> ScreensOf(const void *gui)
{
    std::vector<Screen> screens;
    const std::vector<void *> records = Owners();
    Scenes(gui).ForEach([&](std::int32_t id, const std::byte *scene) {
        if (scene == nullptr)
        {
            return;
        }
        Screen screen;
        screen.id = id;
        screen.name = reinterpret_cast<const core::WStringRep *>(scene + kSceneName)->View();
        screen.movie = reinterpret_cast<const core::WStringRep *>(scene + kSceneMovie)->View();
        screen.layer = Field<std::int32_t>(scene, kSceneLayer);
        screen.flags = Field<std::uint32_t>(scene, kSceneFlags);
        for (void *record : records)
        {
            if (NameOf(record) == screen.movie)
            {
                screen.shown = Field<std::uint8_t>(static_cast<const std::byte *>(record), kMovieActive) != 0;
                break;
            }
        }
        screens.push_back(std::move(screen));
    });
    return screens;
}

} // namespace

// Read 2026-10-02. The GUI system keeps its scenes in a std::map at +0x58, id to GUI::GUIScene
// (dao/core/Msvc8.h, TreeRep): the game's 60 have ids 0-0x3F. A scene's name is the std::wstring
// at +0x58 (what SceneIdByName compares), its movie's name the text of its Filename attribute
// (+0x74, the text at +0xC in it), its layer +0x9C and flags +0xA4. The game's scenes are each
// on a movie of their own ("Inventory" on inventory.gfx, "Options" on OptionsMenu, "Main" on
// MainUI, the loot panel being the inventory's). Showing a scene opens its movie by that name,
// and the manager's record of the movie has an active byte at +0x28: set when it is shown
// (OpenMovie, through 0x00518CB0), cleared when it is hidden (the GUI system's slot 19, through
// the manager's 0x00511990, which takes the movie off the renderer's list too, 0x005187F0), and
// cleared for every movie when the interface is put away whole (0x005118F0's loop). So shown
// here is the record's active byte: what is drawn, not what is logically open.
//
// On the game's thread, as CurrentPopup. Seen in play 2026-10-02, GOG, through the Eluvian
// server, every call answered at once:
// - Exploring with nothing open, nine shown: Tooltips (layer 8), PopupLayer and Notifications
//   (7), NavBar and Quickbar (4), ArmyControl, Minimap and Portraits (1), FloatyLayer (0); no
//   full-screen GUI. "Main" is not among them. PopupLayer reads shown with no box up: its movie
//   stays active, so CurrentPopup is what says a box is up.
// - The loot panel: the full-screen GUI Inventory, in the GUI mode.
// - The level-up screen (GUI.GoToLevelUp): the full-screen GUI LevelUp, in CharGen mode, LevelUp
//   and BookBack among the shown and Notifications gone.
// - After GUI.HideFullScreenGUI, and after the loot panel's Close: the nine again and no
//   full-screen GUI.
// Not yet seen: a store, the map, the inventory opened by its key, and two plugins' screens
// taking ids of their own (AddScreen).
std::vector<Screen> Screens()
{
    const void *gui = GuiSystem();
    return gui != nullptr ? ScreensOf(gui) : std::vector<Screen>{};
}

std::vector<Screen> ShownScreens()
{
    std::vector<Screen> shown = Screens();
    std::erase_if(shown, [](const Screen &screen) { return !screen.shown; });
    std::ranges::stable_sort(shown, std::ranges::greater{}, &Screen::layer);
    return shown;
}

// The screen as Screens() finds it, and its movie through its record (FindOwner, MovieOf). Call
// the movie on the thread its own calls come on; in the GOG game that is the frame's
// (dao/Frame.h), the two seen as one thread, so a frame callback will do there. Read 2026-10-03.
// Seen in play the same day through the Eluvian server (GOG), from the frame's thread: the
// Options screen found, SceneManager.PlayScene("Controls") showed its Controls page and
// _root.KeyBindings_mc.SetScrollPercentage(100) scrolled the key list to its end, for a picture
// taken with no mouse; a root-level OptionTabAction, which the movie does not have, answered
// false.
std::optional<Movie> ScreenMovie(std::wstring_view screen)
{
    for (const Screen &shown : ShownScreens())
    {
        if (shown.name == screen)
        {
            void *view = MovieOf(FindOwner(shown.movie));
            return view != nullptr ? std::optional<Movie>(Movie{view}) : std::nullopt;
        }
    }
    return std::nullopt;
}

// A full-screen GUI is what ToggleFullScreenGUIMessage <name> opens and GUI.HideFullScreenGUI
// closes: the inventory, the character record, the map, the journal, mostly in the GUI game mode
// (dao::GameMode::Gui), the level-up screen in character creation's (CharGen). The GUI system
// shows its scene by id (its slot 13, which also signals the scripts) and pushes the id on a
// std::vector<int> at +0x1E4 (0x00610CE0); hiding pops it (0x0060F1E0, through slot 14), and
// closing all of them pops them all (slot 16). Toggling a name hides it when its id is the
// vector's last, so the last is the one up; opening one first closes the rest, so there is one at
// most. GameModeGUI keeps the id at +0x98 as well. Read 2026-10-02; seen in play with Screens.
std::optional<Screen> CurrentFullScreen()
{
    const void *gui = GuiSystem();
    if (gui == nullptr)
    {
        return std::nullopt;
    }
    const auto ids =
        reinterpret_cast<const core::VectorRep<std::int32_t> *>(static_cast<const std::byte *>(gui) + kFullScreens)
            ->View();
    if (ids.empty())
    {
        return std::nullopt;
    }
    for (Screen &screen : ScreensOf(gui))
    {
        if (screen.id == ids.back())
        {
            return std::move(screen);
        }
    }
    return std::nullopt;
}

namespace
{

// GUI::ScaleformResourceFile (dao/Addresses.h): where its bytes begin and
// where it has read to, and the slot that answers its length.
constexpr std::size_t kFileBegin = 0x30;
constexpr std::size_t kFileAt = 0x34;
constexpr std::size_t kFileSlots = 19;
constexpr std::size_t kFileLengthSlot = 6;
// Its error code, slot 8: 0x1001 for a file with no resource or no bytes,
// so one the library serves from a plugin's folder answers for itself.
constexpr std::size_t kFileErrorSlot = 8;
constexpr std::int32_t kFileNotFound = 0x1001;

using Patched = std::shared_ptr<const std::vector<std::byte>>;

std::mutex g_movieMutex;
std::vector<std::pair<std::string, MoviePatch>> g_moviePatches; // by name, lower case
std::map<std::string, Patched, std::less<>> g_patchedMovies;    // null for a movie no patch changed; kept for good
std::map<const std::byte *, std::size_t> g_patchedLengths;
std::map<std::string, Patched, std::less<>> g_ownMovies; // this plugin's folder's, by name; null for none
// The file's vtable with the length's and the error's slots ours, its RTTI
// word before it.
std::array<void *, kFileSlots + 1> g_patchedFileVtable{};
bool g_patchedFileVtableMade = false;

// The movie's name as a patch names it: no folder, no extension, lower case.
std::string MovieName(std::string_view url)
{
    if (const auto slash = url.find_last_of("/\\:"); slash != std::string_view::npos)
    {
        url.remove_prefix(slash + 1);
    }
    if (const auto dot = url.find_last_of('.'); dot != std::string_view::npos)
    {
        url = url.substr(0, dot);
    }
    std::string name(url);
    for (char &c : name)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return name;
}

// Where the file's bytes begin, null for a file that found no resource.
const std::byte *FileBegin(void *file)
{
    const auto begin = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(file) + kFileBegin);
    return begin ? reinterpret_cast<const std::byte *>(*begin) : nullptr;
}

// The file's slot 6, __thiscall with no arguments: its length. A patched
// file is known by where its bytes begin.
std::int32_t __fastcall PatchedLength(void *file, int /*edx*/)
{
    const std::byte *begin = FileBegin(file);
    const std::lock_guard lock(g_movieMutex);
    const auto found = g_patchedLengths.find(begin);
    return found != g_patchedLengths.end() ? static_cast<std::int32_t>(found->second) : 0;
}

// The file's slot 8, __thiscall with no arguments: its error code, none
// while it has bytes to read (the game's answers 0x1001 for a file with no
// resource handle, which one served from a plugin's folder has not).
std::int32_t __fastcall PatchedError(void *file, int /*edx*/)
{
    return FileBegin(file) != nullptr ? 0 : kFileNotFound;
}

// A movie of this plugin's own, from its folder (Module::OwnFolder,
// daolib\plugins\<name>, DAOLib #26): <name>.gfx, else <name>.swf, read once.
Patched OwnMovie(const std::string &name)
{
    {
        const std::lock_guard lock(g_movieMutex);
        if (const auto found = g_ownMovies.find(name); found != g_ownMovies.end())
        {
            return found->second;
        }
    }
    Patched movie;
    const std::filesystem::path folder = Module::OwnFolder();
    for (const wchar_t *extension : {L".gfx", L".swf"})
    {
        std::ifstream in(folder / (std::filesystem::path(name).wstring() + extension), std::ios::binary);
        if (!in)
        {
            continue;
        }
        std::vector<char> read((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!read.empty())
        {
            const auto *bytes = reinterpret_cast<const std::byte *>(read.data());
            movie = std::make_shared<const std::vector<std::byte>>(bytes, bytes + read.size());
            break;
        }
    }
    const std::lock_guard lock(g_movieMutex);
    const auto [made, first] = g_ownMovies.try_emplace(name, movie);
    if (first && movie)
    {
        g_patchedLengths[movie->data()] = movie->size();
    }
    return made->second;
}

// The movie as its patches leave it: made the first time, the same after.
Patched PatchedMovie(void *file, const std::string &name)
{
    std::vector<MoviePatch> patches;
    {
        const std::lock_guard lock(g_movieMutex);
        if (const auto made = g_patchedMovies.find(name); made != g_patchedMovies.end())
        {
            return made->second;
        }
        for (const auto &[movie, patch] : g_moviePatches)
        {
            if (movie == name)
            {
                patches.push_back(patch);
            }
        }
    }
    if (patches.empty())
    {
        return nullptr;
    }

    // What the game would read: the resource's bytes, as long as the file
    // says it is.
    void *const *slots = *static_cast<void *const **>(file);
    const std::byte *begin = FileBegin(file);
    using LengthFn = std::int32_t(__fastcall *)(void *, int);
    const std::int32_t length = reinterpret_cast<LengthFn>(slots[kFileLengthSlot])(file, 0);
    Patched patched;
    if (begin != nullptr && length > 0)
    {
        if (auto movie = core::gfx::Unpack({begin, static_cast<std::size_t>(length)}))
        {
            bool changed = false;
            for (const MoviePatch &patch : patches)
            {
                detail::Guard("a movie's patch", [&] {
                    if (auto next = patch(*movie))
                    {
                        *movie = std::move(*next);
                        changed = true;
                    }
                });
            }
            if (changed)
            {
                patched = std::make_shared<const std::vector<std::byte>>(std::move(*movie));
            }
        }
    }

    const std::lock_guard lock(g_movieMutex);
    const auto [made, first] = g_patchedMovies.try_emplace(name, patched);
    if (first && patched)
    {
        g_patchedLengths[patched->data()] = patched->size();
    }
    return made->second;
}

// EclipseGFxFileOpener::OpenFile: __thiscall (const char *url), `ret 4`,
// answering the file GFx reads the movie from.
using OpenFileFn = void *(__fastcall *)(void *opener, int edx, const char *url);
Detour<OpenFileFn> g_openFile;

// Whether a file is the game's own over a resource: its vtable the game's,
// or the game's in every slot but the length's and the error's. The second
// is a file another plugin has patched or served already. Each plugin links its own copy of
// this library, each copy detours OpenFile, and the copies run one inside
// the other; the inner one's file has that copy's vtable. (Seen in play,
// 2026-10-01: with two plugins patching the options movie, the outer copy
// took the inner's file for a stranger and left it alone.)
bool IsMovieFile(void *file)
{
    const auto *const game = reinterpret_cast<void *const *>(Module::Address(addr::kScaleformResourceFile_Vftable));
    const auto slots = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(file));
    if (!slots || *slots == 0)
    {
        return false;
    }
    if (*slots == reinterpret_cast<std::uintptr_t>(game))
    {
        return true;
    }
    for (std::size_t slot = 0; slot < kFileSlots; ++slot)
    {
        const auto function = TryRead<std::uintptr_t>(*slots + (slot * sizeof(void *)));
        if (!function || (slot != kFileLengthSlot && slot != kFileErrorSlot &&
                          *function != reinterpret_cast<std::uintptr_t>(game[slot])))
        {
            return false;
        }
    }
    return true;
}

// Has the file read the bytes given, from their start, and answer their
// length and no error. Its resource, if it found one, stays held and is let
// go as ever; one with none has nothing to let go.
void PointFileAt(void *file, const Patched &bytes)
{
    {
        const std::lock_guard lock(g_movieMutex);
        if (!g_patchedFileVtableMade)
        {
            // The game's vtable, its RTTI word before it, with the length's
            // and the error's slots ours.
            const auto *const game =
                reinterpret_cast<void *const *>(Module::Address(addr::kScaleformResourceFile_Vftable));
            std::copy(game - 1, game + kFileSlots, g_patchedFileVtable.begin());
            g_patchedFileVtable[1 + kFileLengthSlot] = reinterpret_cast<void *>(&PatchedLength);
            g_patchedFileVtable[1 + kFileErrorSlot] = reinterpret_cast<void *>(&PatchedError);
            g_patchedFileVtableMade = true;
        }
    }
    auto *fields = static_cast<std::byte *>(file);
    *reinterpret_cast<const std::byte **>(fields + kFileBegin) = bytes->data();
    *reinterpret_cast<const std::byte **>(fields + kFileAt) = bytes->data();
    *static_cast<void *const **>(file) = &g_patchedFileVtable[1];
}

void *__fastcall OpenFile(void *opener, int edx, const char *url)
{
    void *file = g_openFile.Original()(opener, edx, url);
    if (file == nullptr || url == nullptr || !IsMovieFile(file))
    {
        return file;
    }
    // A movie the game's resource system has none of: this plugin's own,
    // from its folder, if it has one by that name. The game's own come first.
    if (FileBegin(file) == nullptr)
    {
        if (const Patched own = OwnMovie(MovieName(url)))
        {
            PointFileAt(file, own);
        }
        return file;
    }
    // A movie the game found, as the patches for it leave it.
    if (const Patched patched = PatchedMovie(file, MovieName(url)))
    {
        PointFileAt(file, patched);
    }
    return file;
}

// The detour, once: OnMovieLoad's patches and a plugin's own movies both
// need it.
void AttachOpenFile()
{
    const std::lock_guard lock(g_movieMutex);
    if (g_openFile.Original() == nullptr)
    {
        g_openFile.Attach(addr::kEclipseGFxFileOpener_OpenFile, &OpenFile);
    }
}

} // namespace

// Read 2026-10-01 (dao/Addresses.h, EclipseGFxFileOpener::OpenFile): the game's file object reads
// from the resource's bytes in memory, and this points it at the patched bytes and answers their
// length. Seen in play the same day through dao/Input.h's rows in the controls screen: the
// patched options movie opened each time, twice in one session and again after a restart, its
// other tabs as ever. Patches of several plugins, each plugin's on what the others' left, seen in
// play 2026-10-01: one plugin's key rows and another's section on the Game tab, in one opening
// of the screen.
void OnMovieLoad(std::wstring_view name, MoviePatch patch)
{
    if (!patch)
    {
        return;
    }
    std::string narrow;
    for (const wchar_t c : name)
    {
        narrow.push_back(c < 0x80 ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : '?');
    }
    {
        const std::lock_guard lock(g_movieMutex);
        g_moviePatches.emplace_back(std::move(narrow), std::move(patch));
    }
    AttachOpenFile();
}

// Shipped beside the plugin and given to the game only while the plugin is loaded, so that
// removing the plugin brings the game's movie back with nothing left in the game's folders; a
// patch in memory does not ship BioWare's file, a copy does. Not yet seen in play.
bool ReplaceMovie(std::wstring_view name, const std::filesystem::path &file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return false;
    }
    const std::string read((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // Unpacked, as every patch is given its movie, so that a patch added
    // after this one can still read and change it; nothing for bytes that
    // are no movie.
    auto movie = core::gfx::Unpack(std::as_bytes(std::span(read.data(), read.size())));
    if (!movie)
    {
        return false;
    }
    OnMovieLoad(name, [copy = std::move(*movie)](std::span<const std::byte>) { return std::optional(copy); });
    return true;
}

// The engine's own test (dao/Addresses.h), which explore mode asks before it picks what is under
// the cursor. Seen in play, 2026-10-01 and 02, in the GOG, EA and Steam games alike
// (examples/world logs each change): yes at the main menu, no with the cursor over the world, yes
// while the game's menu was open and no when it closed.
bool MouseOverInterface()
{
    void *manager = Manager();
    if (manager == nullptr)
    {
        return false;
    }
    using Fn = bool(__fastcall *)(void *, int);
    return reinterpret_cast<Fn>(Module::Address(addr::kScaleformManager_MouseOverInterface))(manager, 0);
}

bool HasMovie(std::wstring_view file)
{
    // As GUI::ScaleformResourceFile opens a movie (dao/Resources.h), then
    // this plugin's folder, as OpenFile serves its own.
    std::error_code ec;
    return resources::Exists(file) || std::filesystem::is_regular_file(Module::OwnFolder() / file, ec);
}

namespace
{

std::mutex g_fullScreenLock;
std::vector<std::function<void(std::wstring_view, bool)>> g_fullScreenObservers;
std::optional<std::wstring> g_fullScreen; // as the last frame found it

void CompareFullScreen()
{
    const auto now = CurrentFullScreen();
    std::optional<std::wstring> name = now ? std::optional(now->name) : std::nullopt;
    if (name == g_fullScreen)
    {
        return;
    }
    const std::optional<std::wstring> before = std::exchange(g_fullScreen, std::move(name));
    std::vector<std::function<void(std::wstring_view, bool)>> observers;
    {
        const std::lock_guard lock(g_fullScreenLock);
        observers = g_fullScreenObservers;
    }
    for (const auto &observer : observers)
    {
        if (before)
        {
            detail::Guard("a full-screen callback", [&] { observer(*before, false); });
        }
        if (g_fullScreen)
        {
            detail::Guard("a full-screen callback", [&] { observer(*g_fullScreen, true); });
        }
    }
}

} // namespace

// Compared each frame, as the GUI system keeps them (its stack of full-screen ids, the last the
// one up). Read 2026-10-05 (DAOLib #27); not yet seen in play.
void OnFullScreen(std::function<void(std::wstring_view name, bool shown)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] { OnFrame(&CompareFullScreen); });
    const std::lock_guard lock(g_fullScreenLock);
    g_fullScreenObservers.push_back(std::move(observer));
}

// The clip is made with the movie's own createEmptyMovieClip at its next free depth, and the
// movie <ownMovie>.swf is loaded into it with loadMovie, served from the plugin's folder (as
// AddScreen's are). A screen only hidden keeps the clip and its movie; one closed loses both. A
// mark set on the clip before the load, which the load clears, says which clip's load never came.
//
// For the loaded movie's script, from the turn-based mod's tactics box (its issue 46,
// 2026-10-05):
// - The clip and anything the script attaches need names of their own: an element attached
//   inside the clip by the clip's name would shadow it.
// - A game element attached by attachMovie is not usable in the frame it is attached, and an
//   element of the screen's own may be there before it is set up: wait for its m_bLoaded, as the
//   screen's own rows do, not only for it to exist.
// - Anything attached at the root draws over the screen's drop-down lists, and the screen's own
//   swapDepths(1) moves whatever sits at depth 1.
// - The screen's token observers keep ids of their own (the tactics screen's 2303, 2308, 2600 to
//   2609); a script's own observer takes an id of its own (TokenManager.AddTokenObserverWithID,
//   as the screen does).
// Read 2026-10-05 (DAOLib #27); not yet seen in play.
bool LoadInto(Movie movie, std::string_view clip, std::wstring_view ownMovie)
{
    if (!movie || clip.empty() || ownMovie.empty())
    {
        return false;
    }
    const std::string path = "_root." + std::string(clip);
    // Set on the clip before its load: a movie loaded into a clip replaces
    // what the clip held, its variables too, so the mark still there means
    // the load never came (DAOLib #27, found by the turn-based mod).
    const std::string pending = path + ".DAOLib_LoadPending";
    if (movie.IsAvailable(path.c_str()))
    {
        if (!movie.GetBool(pending.c_str()).value_or(false))
        {
            return true;
        }
        movie.Invoke((path + ".removeMovieClip").c_str());
    }
    // The plugin's own movie, served from its folder as GFx opens it.
    std::error_code ec;
    const std::filesystem::path own = Module::OwnFolder() / ownMovie;
    if (!std::filesystem::is_regular_file(own.wstring() + L".gfx", ec) &&
        !std::filesystem::is_regular_file(own.wstring() + L".swf", ec))
    {
        return false;
    }
    AttachOpenFile();
    const auto depth = movie.Call("_root.getNextHighestDepth");
    const std::string name(clip);
    const Value make[] = {Value::String(name.c_str()),
                          Value::Number(depth && depth->type == Value::Type::Number ? depth->number : 0)};
    if (!movie.Call("_root.createEmptyMovieClip", make) || !movie.IsAvailable(path.c_str()))
    {
        return false;
    }
    movie.SetVariable(pending.c_str(), Value::Bool(true), Movie::SetType::Normal);
    const std::string url = Utf8(ownMovie) + ".swf";
    const Value load[] = {Value::String(url.c_str())};
    return movie.Invoke((path + ".loadMovie").c_str(), load);
}

bool ReturnFSCommand(void *owner, std::span<const std::int32_t> values)
{
    return Movie{MovieOf(owner)}.SetArray("_root.fsc_retval", values);
}

// The movie's slot 42, SetExternalInterfaceRetVal, as the game's handlers answer.
void Call::Return(const Value &answer) const
{
    void *movie = MovieOf(owner);
    if (movie == nullptr)
    {
        return;
    }
    using Fn = void(__fastcall *)(void *, int, const Value *);
    const auto *vtable = *static_cast<Fn *const *>(movie);
    vtable[42](movie, 0, &answer);
}

// At plugin load or later; the first call detours the engine's lookup.
void OnFSCommand(std::string name, FSCommandHandler handler)
{
    const std::lock_guard lock(g_mutex);
    g_fsCommands.insert_or_assign(std::move(name), std::move(handler));
    AttachLocked();
}

void OnExternalInterface(std::string name, ExternalInterfaceHandler handler)
{
    const std::lock_guard lock(g_mutex);
    g_externalCalls.insert_or_assign(std::move(name), std::move(handler));
    AttachLocked();
}

void InterceptFSCommand(std::string name, std::function<bool(void *owner, std::string_view args)> filter)
{
    const std::lock_guard lock(g_mutex);
    g_fsCommandFilters[std::move(name)].push_back(std::move(filter));
    AttachLocked();
}

void InterceptExternalInterface(std::string name, std::function<bool(const Call &call)> filter)
{
    const std::lock_guard lock(g_mutex);
    g_externalFilters[std::move(name)].push_back(std::move(filter));
    AttachLocked();
}

void OnAnyFSCommand(std::function<void(void *owner, std::string_view name, std::string_view args)> observer)
{
    const std::lock_guard lock(g_mutex);
    g_anyFSCommand.push_back(std::move(observer));
    AttachLocked();
}

void OnAnyExternalInterface(ExternalInterfaceHandler observer)
{
    const std::lock_guard lock(g_mutex);
    g_anyExternal.push_back(std::move(observer));
    AttachLocked();
}

// Read 2026-10-01. The game's message boxes are one movie, the popup layer (GUI::GUIPopupLayer,
// built by 0x0070A071's function), which keeps what it shows in the data tree under
// GUIPopupLayer: DialogBoxText, NumButtons, and for each button a Label0 to Label3 and a Callback0
// to Callback3, the message the game is sent when it is pressed, as text
// ("CClientExoApp.GUIDialogResult 1 0"); also an input box, a checkbox and a picker, not read
// here. The GUI system keeps whether a box is up at +0x1FA: set when it shows one (0x0060BA90,
// which then has the movie show it, PopupLayer.ShowDialogBoxFromGame) and cleared by
// DialogBoxClosedMessage. Seen in play, 2026-10-01, through the Eluvian server: after the
// scripts' ShowPopup(379897, 1) it gave the text "Shield Pummel", the string's own and not its
// number, and two buttons, Yes with the callback "GUIPopupLayer.ScriptDialogResult 1 1 -1" and No
// with "... 1 2 -1"; nothing before the box was up. A script's box holds nothing: loads, orders
// and a fight went on under it.
std::optional<Popup> CurrentPopup()
{
    constexpr std::size_t kBoxUp = 0x1FA; // in the GUI system
    constexpr int kMostButtons = 4;
    const void *gui = GuiSystem();
    if (gui == nullptr || TryRead<std::uint8_t>(reinterpret_cast<std::uintptr_t>(gui) + kBoxUp).value_or(0) == 0)
    {
        return std::nullopt;
    }
    Popup box;
    box.text = data::Text(L"GUIPopupLayer.DialogBoxText").value_or(L"");
    const int buttons = static_cast<int>(data::Number(L"GUIPopupLayer.NumButtons").value_or(0));
    for (int i = 0; i < buttons && i < kMostButtons; ++i)
    {
        const std::wstring n = std::to_wstring(i);
        box.buttons.push_back({data::Text(L"GUIPopupLayer.Label" + n).value_or(L""),
                               data::Text(L"GUIPopupLayer.Callback" + n).value_or(L"")});
    }
    return box;
}

// The button's callback goes as the movie sends it when the button is clicked (dao/Messages.h,
// SendText), and then, as the movie tells it once the box has gone, DialogBoxClosedMessage. Read
// 2026-10-01; what the movie itself does at a click is ActionScript and not read. Seen in play
// the same day, through the Eluvian server: with a script's Yes and No box up, pressing No
// answered true, CurrentPopup was nothing at once, and the game's own screenshot showed the box
// gone and the interface back. Whether the script heard the answer (EVENT_TYPE_POPUP_RESULT) is
// not yet seen.
bool PressPopupButton(std::size_t index)
{
    const auto box = CurrentPopup();
    void *gui = GuiSystem();
    if (!box || gui == nullptr || index >= box->buttons.size() || !messages::SendText(box->buttons[index].callback))
    {
        return false;
    }
    messages::Send(gui, messages::Make<messages::DialogBoxClosedMessage>());
    HideScreen(L"PopupLayer");
    return true;
}

} // namespace dao::gui
