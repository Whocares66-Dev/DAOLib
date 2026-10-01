#include "dao/Gui.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Data.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/core/GfxMovie.h"
#include "dao/core/Msvc8.h"

#include <array>
#include <cctype>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
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
    return TypeName(gui).rfind("GUI::", 0) == 0 ? gui : nullptr;
}

// The ids a plugin's screens take, past the game's 0-0x3F.
std::uint32_t g_nextScreenId = 0x100;

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
    const std::uint32_t id = g_nextScreenId++;
    using AddFn = void(__fastcall *)(void *, int, std::uint32_t, void *);
    reinterpret_cast<AddFn>(Module::Address(addr::kGUISystem_AddScene))(gui, 0, id, scene);
    return SceneId(gui, name) == static_cast<std::int32_t>(id);
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

// GUI::ScaleformResourceFile (dao/Addresses.h): where its bytes begin and
// where it has read to, and the slot that answers its length.
constexpr std::size_t kFileBegin = 0x30;
constexpr std::size_t kFileAt = 0x34;
constexpr std::size_t kFileSlots = 19;
constexpr std::size_t kFileLengthSlot = 6;

using Patched = std::shared_ptr<const std::vector<std::byte>>;

std::mutex g_movieMutex;
std::vector<std::pair<std::string, MoviePatch>> g_moviePatches; // by name, lower case
std::map<std::string, Patched, std::less<>> g_patchedMovies;    // null for a movie no patch changed; kept for good
std::map<const std::byte *, std::size_t> g_patchedLengths;
// The file's vtable with the length's slot ours, its RTTI word before it.
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

void *__fastcall OpenFile(void *opener, int edx, const char *url)
{
    void *file = g_openFile.Original()(opener, edx, url);
    // Only the game's own file over a resource it found is one to point
    // elsewhere.
    if (file == nullptr || url == nullptr ||
        *static_cast<void *const **>(file) !=
            reinterpret_cast<void *const *>(Module::Address(addr::kScaleformResourceFile_Vftable)) ||
        FileBegin(file) == nullptr)
    {
        return file;
    }
    const Patched patched = PatchedMovie(file, MovieName(url));
    if (!patched)
    {
        return file;
    }
    {
        const std::lock_guard lock(g_movieMutex);
        if (!g_patchedFileVtableMade)
        {
            void *const *slots = *static_cast<void *const **>(file);
            std::copy(slots - 1, slots + kFileSlots, g_patchedFileVtable.begin());
            g_patchedFileVtable[1 + kFileLengthSlot] = reinterpret_cast<void *>(&PatchedLength);
            g_patchedFileVtableMade = true;
        }
    }
    // The file reads from the patched bytes, from their start, and answers
    // their length. Its resource stays held and is let go as ever.
    auto *fields = static_cast<std::byte *>(file);
    *reinterpret_cast<const std::byte **>(fields + kFileBegin) = patched->data();
    *reinterpret_cast<const std::byte **>(fields + kFileAt) = patched->data();
    *static_cast<void *const **>(file) = &g_patchedFileVtable[1];
    return file;
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
    const std::lock_guard lock(g_movieMutex);
    g_moviePatches.emplace_back(std::move(narrow), std::move(patch));
    if (g_openFile.Original() == nullptr)
    {
        g_openFile.Attach(addr::kEclipseGFxFileOpener_OpenFile, &OpenFile);
    }
}

bool HasMovie(std::wstring_view file)
{
    // As GUI::ScaleformResourceFile opens a movie: a resource of type 3.
    constexpr std::int32_t kMovieResource = 3;
    WString name(file);
    std::uintptr_t handle = 0;
    using FindFn = std::uintptr_t *(__cdecl *)(std::uintptr_t *, const core::WStringRep *, std::int32_t);
    reinterpret_cast<FindFn>(Module::Address(addr::kResources_Find))(&handle, name.Engine(), kMovieResource);
    const bool found = handle != 0;
    using ReleaseFn = void(__fastcall *)(std::uintptr_t *, int);
    reinterpret_cast<ReleaseFn>(Module::Address(addr::kResources_Release))(&handle, 0);
    return found;
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
