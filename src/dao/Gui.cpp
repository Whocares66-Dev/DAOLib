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
    return *At<void *>(addr::kGuiManager);
}

} // namespace

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

// The GUI system's by-name slots (17 open, 18 close, 19 hide), each
// __thiscall (const std::wstring &name), `ret 4`.
bool ByName(std::size_t slot, std::wstring_view name)
{
    void *gui = GuiSystem();
    if (gui == nullptr)
    {
        return false;
    }
    WString text(name);
    using Fn = void(__fastcall *)(void *, int, const core::WStringRep *);
    Slot<Fn>(gui, slot)(gui, 0, text.Engine());
    return true;
}

// The id the GUI system knows a scene by, or -1 (its slot 12).
std::int32_t SceneId(void *gui, std::wstring_view name)
{
    WString text(name);
    using Fn = std::int32_t(__fastcall *)(void *, int, const core::WStringRep *);
    return Slot<Fn>(gui, 12)(gui, 0, text.Engine());
}

} // namespace

namespace
{
// Below, with the movie file's detour.
void AttachOpenFile();
} // namespace

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

void OnFullScreen(std::function<void(std::wstring_view name, bool shown)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] { OnFrame(&CompareFullScreen); });
    const std::lock_guard lock(g_fullScreenLock);
    g_fullScreenObservers.push_back(std::move(observer));
}

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
