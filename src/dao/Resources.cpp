// The game's files, through the engine's resource system.
//
// Read 2026-10-02 (dao/Addresses.h, "A resource's bytes"): what the Flash
// interface's movie file does to open a movie, Resources::Find with the
// name and the mode it asks with, then the resource's Data and Size, and
// Release. Seen in play 2026-10-02 by the turn-based mod: rules_core.ncs
// read as 251,960 bytes, the size of the shipped one in
// designerscripts.rim.
//
// A folder as a source (DAOLib #43 for a plugin's override\, DAOLib #85 for
// one a plugin adds itself; dao/Addresses.h, "The resource manager's
// sources"): ECResMan::ResMan::AddSource with the folder's path, as the game
// adds packages\core\override, and its release by that same path. The game
// sets its sources up as the second step of its start (ECWinApp's
// Initialize, 0x0040E850: "Initialize - Resource Manager", before the world
// database, graphics, the GUI and "Initialize - Game"), and a folder asked
// for before that waits for it: SetUpSources answers whether the manager is
// set up, and the game ends when it is not ("Failed to initialize
// ResourceManager", 0x0040E9B6).
//
// SetUpSources is a member function, __thiscall with no stack arguments (a
// plain `ret`): Initialize does `mov ecx, esi` before the call, and it keeps
// `this` with `mov edi, ecx` before it sets ECX itself. The hook has had
// both wrong, in the loader, where it was first: answering nothing, so that
// what was left in al was taken for the answer and the game exited at start
// whenever a source was added (2026-10-09, the turn-based mod's play checks;
// the exit log of DAOLib #44 showed WinMain returning -1); and declared
// __cdecl(), so passing on whatever ECX held, which the release build
// happened to leave as `this` and the debug build did not (DAOLib #46).
//
// Seen in play 2026-10-12 (examples/resources, GOG's game, at the main
// menu): a folder asked for at load and the plugin's override\ were
// sources 0.3 s after the plugins loaded, a texture in the folder read on
// the first frame, not found once the folder was released, and found again
// once it was added in the running game. Not seen: what a release reaches
// of a file the game holds loaded.
#include "dao/Resources.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

namespace dao::resources
{

namespace
{

// The mode the movie file asks Find with, which has the bytes there on
// return (dao/Addresses.h).
constexpr std::int32_t kLoadNow = 3;
// More than any of the game's files: a length past it is taken as a
// misread.
constexpr std::uint32_t kMostBytes = 512U * 1024 * 1024;

// A resource found by name, released when it goes.
class Found
{
  public:
    explicit Found(std::wstring_view file)
    {
        WString name(file);
        // Resources::Find: __cdecl (out handle, const std::wstring &, mode).
        using FindFn = std::uintptr_t *(__cdecl *)(std::uintptr_t *, const core::WStringRep *, std::int32_t);
        reinterpret_cast<FindFn>(Module::Address(addr::kResources_Find))(&handle_, name.Engine(), kLoadNow);
    }
    ~Found()
    {
        // Resources::Release: __thiscall on the handle.
        using ReleaseFn = void(__fastcall *)(std::uintptr_t *, int);
        reinterpret_cast<ReleaseFn>(Module::Address(addr::kResources_Release))(&handle_, 0);
    }
    Found(const Found &) = delete;
    Found &operator=(const Found &) = delete;
    Found(Found &&) = delete;
    Found &operator=(Found &&) = delete;

    [[nodiscard]] bool Exists() const
    {
        return handle_ != 0;
    }

    std::optional<std::vector<std::byte>> Bytes()
    {
        if (handle_ == 0)
        {
            return std::nullopt;
        }
        // Resources::Data and Size: __thiscall on the handle.
        using DataFn = std::uintptr_t(__fastcall *)(std::uintptr_t *, int);
        using SizeFn = std::uint32_t(__fastcall *)(std::uintptr_t *, int);
        const std::uintptr_t data = reinterpret_cast<DataFn>(Module::Address(addr::kResources_Data))(&handle_, 0);
        const std::uint32_t size = reinterpret_cast<SizeFn>(Module::Address(addr::kResources_Size))(&handle_, 0);
        // No bytes where they should be is a resource not loaded or a lock
        // refused, both of which also answer a size of 0 (dao/Addresses.h),
        // so it is nothing read rather than an empty file.
        if (data == 0 || size > kMostBytes)
        {
            return std::nullopt;
        }
        std::vector<std::byte> bytes(size);
        if (size != 0 && !TryCopy(bytes.data(), data, size))
        {
            return std::nullopt;
        }
        return bytes;
    }

  private:
    std::uintptr_t handle_ = 0;
};

// A folder as AddSource takes it.
constexpr std::int32_t kDirectoryKind = 1;  // a folder with its subfolders and any .erf or .rim in it
constexpr std::int32_t kFolderPriority = 2; // OVERRIDE: is 1, PATCH: 100000; a tie is not decided by it
constexpr std::int32_t kWatch = 0;          // read once; OVERRIDE:'s 1 re-lists it every fifth frame

std::mutex g_foldersMutex;
bool g_sourcesSetUp = false;         // the game has set its sources up: a folder goes straight to it
bool g_watching = false;             // how to learn of that has been seen to
std::vector<std::wstring> g_waiting; // folders asked for before then
std::vector<std::wstring> g_added;   // folders this copy of the library has given the game

// The name a folder's source has in the manager: its full path, ending in a
// separator as the game's own folders' do. A release finds the source by
// it, so both go through here.
std::wstring SourceName(const std::filesystem::path &folder)
{
    std::error_code ec;
    std::filesystem::path full = std::filesystem::absolute(folder, ec).lexically_normal();
    if (!full.has_filename())
    {
        full = full.parent_path(); // it was given ending in a separator
    }
    return full.wstring() + L'\\';
}

// What a log line calls a folder: its last two names, a plugin's and its
// own, and not the path to the game.
std::string Shown(const std::wstring &name)
{
    const std::filesystem::path folder = std::filesystem::path(name).parent_path();
    return (folder.parent_path().filename() / folder.filename()).string();
}

void *Manager()
{
    using ManagerFn = void *(__cdecl *)();
    const auto manager = reinterpret_cast<ManagerFn>(Module::Address(addr::kResMan_Manager));
    return manager != nullptr ? manager() : nullptr;
}

// The game takes the folder as a source, under its own lock.
bool Give(const std::wstring &name)
{
    using AddSourceFn = bool(__fastcall *)(void *, int, const core::WStringRep *, std::int32_t, std::int32_t,
                                           std::int32_t, void **, std::int32_t, std::int32_t);
    const auto add = reinterpret_cast<AddSourceFn>(Module::Address(addr::kResMan_AddSource));
    void *manager = add != nullptr ? Manager() : nullptr;
    WString path(name);
    const bool added =
        manager != nullptr && add(manager, 0, path.Engine(), kDirectoryKind, kFolderPriority, 0, nullptr, kWatch, 0);
    spdlog::info("resources: {} {}", Shown(name),
                 added ? "is a source of the game's files" : "was not taken as a source");
    return added;
}

// The game lets the folder go. It answers no for a lock it did not get as
// for a name it has not.
bool Take(const std::wstring &name)
{
    using ReleaseSourceFn = bool(__fastcall *)(void *, int, const core::WStringRep *);
    const auto release = reinterpret_cast<ReleaseSourceFn>(Module::Address(addr::kResMan_ReleaseSource));
    void *manager = release != nullptr ? Manager() : nullptr;
    WString path(name);
    const bool released = manager != nullptr && release(manager, 0, path.Engine());
    spdlog::info("resources: {} {}", Shown(name), released ? "is no longer a source" : "was not a source to take away");
    return released;
}

// The game's sources are set up: the folders that waited are given.
void SourcesSetUp()
{
    const std::lock_guard lock(g_foldersMutex);
    g_sourcesSetUp = true;
    for (std::wstring &name : std::exchange(g_waiting, {}))
    {
        if (Give(name))
        {
            g_added.push_back(std::move(name));
        }
    }
}

using SetUpSourcesFn = bool(__fastcall *)(void *self, int edx);
Detour<SetUpSourcesFn> g_setUpSources;

bool __fastcall SetUpSources(void *self, int edx)
{
    const bool setUp = g_setUpSources.Original()(self, edx);
    if (setUp)
    {
        SourcesSetUp();
    }
    return setUp;
}

// Learns whether the game's sources are set up, the first time a folder is
// asked for. A game with its application object is long past it; before
// that, plugins are loading and the set-up is still to come, so a detour of
// it says when.
void Watch()
{
    {
        const std::lock_guard lock(g_foldersMutex);
        if (g_watching || g_sourcesSetUp)
        {
            return;
        }
        g_watching = true;
    }
    if (CClientExoApp::Get() != nullptr)
    {
        SourcesSetUp();
    }
    else if (!g_setUpSources.Attach(addr::kResMan_SetUpSources, &SetUpSources))
    {
        spdlog::warn("resources: could not detour the resource manager's set-up; folders are not sources");
    }
}

} // namespace

std::optional<std::vector<std::byte>> Read(std::wstring_view file)
{
    if (file.empty())
    {
        return std::nullopt;
    }
    Found found(file);
    return found.Bytes();
}

bool Exists(std::wstring_view file)
{
    return !file.empty() && Found(file).Exists();
}

bool AddFolder(const std::filesystem::path &folder)
{
    std::error_code ec;
    if (!Module::IsKnownExecutable() || !std::filesystem::is_directory(folder, ec))
    {
        return false;
    }
    std::wstring name = SourceName(folder);
    Watch();
    const std::lock_guard lock(g_foldersMutex);
    const auto has = [&](const std::vector<std::wstring> &names) {
        return std::ranges::find(names, name) != names.end();
    };
    if (has(g_added) || has(g_waiting))
    {
        return true;
    }
    if (!g_sourcesSetUp)
    {
        g_waiting.push_back(std::move(name));
        return true;
    }
    if (!Give(name))
    {
        return false;
    }
    g_added.push_back(std::move(name));
    return true;
}

bool RemoveFolder(const std::filesystem::path &folder)
{
    const std::wstring name = SourceName(folder);
    const std::lock_guard lock(g_foldersMutex);
    if (std::erase(g_waiting, name) != 0)
    {
        return true;
    }
    return std::erase(g_added, name) != 0 && Take(name);
}

} // namespace dao::resources
