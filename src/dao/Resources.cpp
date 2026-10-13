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
//
// A texture's file (DAOLib #100). What Data answers for a .dds the game has
// used is not the file: the engine hands a texture's bytes to its one
// processor at first use, frees them and keeps the processor's buffer in
// their place (dao/Addresses.h, "A file's bytes from its source"), and a
// plugin reading 22 add-in textures got bytes of the files' lengths that
// began with pointers. So a .dds is not taken from the resource: its bytes
// are fetched from its source here, as the engine fetches them. The
// engine's own fetch cannot be asked again, since it stores its buffer in
// the resource, over what the game draws from. A loose file is read by the
// path the engine makes for it; an archive's from the one file the archive
// holds open, at the resource's offset; and both are given the source's
// slot 6 under the source's lock, which deciphers an enciphered archive's.
// Seen in play 2026-10-12 (examples/resources, GOG's game, at the main
// menu): a loose texture and three from texturepack.erf and guiexport.erf
// read whole, each the same bytes as its file read outside the game
// (132, 87,536, 212 and 2,097,280 bytes, by a sum over them).
//
// A level's archive, a RIM, holds no open file once it has loaded (DAOLib
// #102; dao/Addresses.h, "A RIM's file"): a plugin in Redcliffe found a
// level's textures and read nothing of them. A RIM's texture is read from
// the RIM's file opened again, by the path the engine's alias table gives
// for the source's name, as the engine's own opener of an archive asks it.
// Seen in play 2026-10-12 (examples/resources with its read.txt, GOG's game,
// a save in Redcliffe at night, level lak100n): three textures of
// lak100n.gpu.rim read whole (16,512, 174,904 and 131,200 bytes), each the
// same bytes as in the RIM's file by a sum over them, and a room's
// definition, a mesh and a material of lak100n.rim as before, the same as
// theirs. Not seen: a packed RIM, which answers nothing. A tile's own
// textures (lak100n_0_1_0.gpu.rim's) were not found by name there at all,
// which is a matter of finding and not of reading.
//
// The names of a type (DAOLib #96): the manager's own ListNames, which the
// game uses to gather its merged tables (dao/Addresses.h, "The names the
// manager has"), into a list of the engine's made and freed by the engine's
// own two functions. Seen in play 2026-10-12 (examples/resources, GOG's game,
// at the main menu): 7,769 names for dds, with its dot or without, a
// plugin's own override file and the game's among them; 4,089 for mao, an
// add-in's among them; none for a type the game has not; each name once.
#include "dao/Resources.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <fstream>
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

// A resource's fields and its source's (dao/Addresses.h, "A file's bytes from
// its source").
constexpr std::size_t kResourceName = 0x00;     // the name it is found by, the engine's std::wstring
constexpr std::size_t kResourceSource = 0x1C;   // the source it came from
constexpr std::size_t kResourceOffset = 0x28;   // where its bytes begin in the source's file
constexpr std::size_t kResourceFileSize = 0x2C; // and how many they are
constexpr std::size_t kSourceName = 0x04;       // a source's name: an alias and a file, for an archive
constexpr std::size_t kSourceClosed = 0x68;     // a source: closing, or closed
constexpr std::size_t kSourceOpen = 0x69;       // and opened
constexpr std::size_t kSourceFolder = 0xA4;     // a DirectorySource's folder, ending in a separator
constexpr std::size_t kArchiveFile = 0xB8;      // an ErfSource's one open file, a FileAccess
constexpr std::size_t kFileHandle = 0x04;       // a FileAccess's handle
constexpr std::size_t kRimUnpacked = 0x108;     // a RIMSource: its block's size unpacked, 0 if it is not packed
constexpr std::size_t kDecipherSlot = 6;
// Longer than any path the game makes; a length past it is taken as a misread.
constexpr std::uint32_t kMostCharacters = 2048;

// The text of one of the engine's strings, or nothing if it does not read as one.
std::optional<std::wstring> EngineText(std::uintptr_t at)
{
    const auto rep = TryRead<core::WStringRep>(at);
    if (!rep || rep->size > kMostCharacters)
    {
        return std::nullopt;
    }
    std::wstring text(rep->size, L'\0');
    const std::uintptr_t from = rep->capacity > core::WStringRep::kInlineCapacity
                                    ? reinterpret_cast<std::uintptr_t>(rep->data.pointer)
                                    : at + offsetof(core::WStringRep, data);
    if (rep->size != 0 && !TryCopy(text.data(), from, rep->size * sizeof(wchar_t)))
    {
        return std::nullopt;
    }
    return text;
}

// Whether the game replaces this file's bytes with something of its own when it first uses
// the file, so that the resource no longer holds the file: textures, and nothing else.
bool IsProcessed(std::wstring_view file)
{
    constexpr std::wstring_view kTexture = L".dds";
    return file.size() >= kTexture.size() &&
           std::ranges::equal(file.substr(file.size() - kTexture.size()), kTexture,
                              [](wchar_t a, wchar_t b) { return std::towlower(a) == b; });
}

// A loose file's bytes: the folder's path and the resource's name after it, as the engine
// joins them to open the file itself.
bool ReadLoose(std::uintptr_t resource, std::uintptr_t source, std::uint32_t offset, std::vector<std::byte> &bytes)
{
    const auto folder = EngineText(source + kSourceFolder);
    const auto name = EngineText(resource + kResourceName);
    if (!folder || !name)
    {
        return false;
    }
    std::ifstream file(std::filesystem::path(*folder + *name), std::ios::binary);
    return file && file.seekg(offset) &&
           file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// An archive's bytes, from the one file the archive holds open: read at their place in it, a
// read that carries its own position and so shares nothing with a read the engine is making
// of the same file on another thread (src/fix/ArchiveRead.cpp has the same read, and why).
bool ReadArchive(std::uintptr_t source, std::uint32_t offset, std::vector<std::byte> &bytes)
{
    const std::uintptr_t file = TryRead<std::uintptr_t>(source + kArchiveFile).value_or(0);
    if (file == 0 || TryRead<std::uintptr_t>(file).value_or(0) != Module::Address(addr::kFileAccess_Vftable))
    {
        return false;
    }
    auto *const handle = reinterpret_cast<HANDLE>(TryRead<std::uintptr_t>(file + kFileHandle).value_or(0));
    OVERLAPPED at{};
    at.Offset = offset;
    DWORD read = 0;
    return handle != INVALID_HANDLE_VALUE && handle != nullptr &&
           ReadFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &read, &at) != 0 && read == bytes.size();
}

// A RIM's bytes, from its file opened again: a RIM closes its one file once its task has read
// it whole (dao/Addresses.h, "A RIM's file"). The path is the engine's, asked of its alias table
// as its own opener of an archive asks. Nothing for a packed RIM, whose files' offsets are into
// the unpacked block.
bool ReadRim(std::uintptr_t source, std::uint32_t offset, std::vector<std::byte> &bytes)
{
    using GetFn = std::uintptr_t(__cdecl *)();
    using PathFn = core::WStringRep *(__fastcall *)(std::uintptr_t, int, core::WStringRep *, const core::WStringRep *);
    const std::uintptr_t aliases = reinterpret_cast<GetFn>(Module::Address(addr::kFileAliases_Get))();
    if (aliases == 0 || TryRead<std::uint32_t>(source + kRimUnpacked).value_or(1) != 0)
    {
        return false;
    }
    WString path;
    reinterpret_cast<PathFn>(Module::Address(addr::kFileAliases_Path))(
        aliases, 0, path.Engine(), reinterpret_cast<const core::WStringRep *>(source + kSourceName));
    std::ifstream file(std::filesystem::path(path.View()), std::ios::binary);
    return file && file.seekg(offset) &&
           file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// A resource's file fetched from its source beside the engine, as the engine fetches it and
// under the source's lock as the engine does: the source open, its bytes read from where they
// lie, and the source's own way with bytes fresh from its file run over them (slot 6: an
// enciphered archive deciphers them, every other source leaves them as they are).
bool Fetch(std::uintptr_t resource, std::uintptr_t source, std::uint32_t offset, std::vector<std::byte> &bytes)
{
    using LockFn = std::uint32_t(__fastcall *)(std::uintptr_t, int, std::uint32_t);
    using UnlockFn = std::uint32_t(__fastcall *)(std::uintptr_t, int);
    using DecipherFn = bool(__fastcall *)(std::uintptr_t, int, void *, std::uint32_t);
    const std::uintptr_t kind = TryRead<std::uintptr_t>(source).value_or(0);
    const bool loose = kind == Module::Address(addr::kDirectorySource_Vftable);
    // A RIMSource is an ErfSource (its base at +0) that keeps no file open.
    const bool rim = kind == Module::Address(addr::kRimSource_Vftable);
    const bool archive = rim || kind == Module::Address(addr::kErfSource_Vftable);
    const auto decipher = TryRead<std::uintptr_t>(kind + (kDecipherSlot * sizeof(std::uintptr_t)));
    if ((!loose && !archive) || !decipher || *decipher == 0)
    {
        return false;
    }
    // Lock(0) waits and then holds, and a source's lock nests, so a caller the engine already
    // runs under it gets it too. Unlock checks nothing: only after a Lock that answered true.
    if (reinterpret_cast<LockFn>(Module::Address(addr::kResourceSource_Lock))(source, 0, 0) == 0)
    {
        return false;
    }
    const bool open = TryRead<std::uint8_t>(source + kSourceClosed).value_or(1) == 0 &&
                      TryRead<std::uint8_t>(source + kSourceOpen).value_or(0) != 0;
    bool read = false;
    if (!open)
    {
        // Nothing to read from.
    }
    else if (loose)
    {
        read = ReadLoose(resource, source, offset, bytes);
    }
    else
    {
        read = rim ? ReadRim(source, offset, bytes) : ReadArchive(source, offset, bytes);
    }
    const bool fetched = read && reinterpret_cast<DecipherFn>(*decipher)(source, 0, bytes.data(),
                                                                         static_cast<std::uint32_t>(bytes.size()));
    reinterpret_cast<UnlockFn>(Module::Address(addr::kResourceSource_Unlock))(source, 0);
    return fetched;
}

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

    // The file's bytes fetched from the resource's source, beside the engine: for a type whose
    // resource does not keep its file (dao/Addresses.h). The engine's own fetch would put its
    // buffer in the resource, over what the game is drawing from.
    [[nodiscard]] std::optional<std::vector<std::byte>> FileBytes() const
    {
        if (handle_ == 0)
        {
            return std::nullopt;
        }
        const std::uintptr_t source = TryRead<std::uintptr_t>(handle_ + kResourceSource).value_or(0);
        const auto offset = TryRead<std::uint32_t>(handle_ + kResourceOffset);
        const auto size = TryRead<std::uint32_t>(handle_ + kResourceFileSize);
        if (source == 0 || !offset || !size || *size > kMostBytes)
        {
            return std::nullopt;
        }
        std::vector<std::byte> bytes(*size);
        if (!Fetch(handle_, source, *offset, bytes))
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
    return IsProcessed(file) ? found.FileBytes() : found.Bytes();
}

bool Exists(std::wstring_view file)
{
    return !file.empty() && Found(file).Exists();
}

// The game's own list, as it makes one to gather its merged tables (dao/Addresses.h, "The names
// the manager has"): a list of the engine's, made and freed by the engine's own two functions,
// filled by the manager from every source, and copied out here.
std::vector<std::wstring> Names(std::wstring_view type)
{
    using ConstructFn = void *(__fastcall *)(void *, int, core::WStringRep);
    using DestroyFn = void(__fastcall *)(void *, int);
    using ListFn = bool(__fastcall *)(void *, int, void *, const core::WStringRep *, const core::WStringRep *,
                                      std::uint32_t, const core::WStringRep *);
    constexpr std::size_t kListBytes = 0x28;
    constexpr std::size_t kListHead = 0x04;   // in the list: its head node
    constexpr std::size_t kNodeString = 0x08; // in a node: its string, the next node at +0
    // More than the game's files of every type together; a list longer is taken as a misread.
    constexpr std::size_t kMostNames = 1U << 20U;

    std::vector<std::wstring> names;
    const auto construct = reinterpret_cast<ConstructFn>(Module::Address(addr::kNameList_Construct));
    const auto destroy = reinterpret_cast<DestroyFn>(Module::Address(addr::kNameList_Destroy));
    const auto list = reinterpret_cast<ListFn>(Module::Address(addr::kResMan_ListNames));
    void *manager =
        !type.empty() && construct != nullptr && destroy != nullptr && list != nullptr ? Manager() : nullptr;
    if (manager == nullptr)
    {
        return names;
    }
    alignas(void *) std::array<std::byte, kListBytes> found{};
    // The constructor takes a string by value and frees it: an empty one has nothing to free.
    construct(found.data(), 0, core::WStringRep{});
    const core::WStringRep none{};
    WString wanted(type);
    // Every source, the type, all of its files and not the loaded alone, no source left out.
    list(manager, 0, found.data(), &none, wanted.Engine(), 0, &none);
    const auto head = *reinterpret_cast<const std::uintptr_t *>(found.data() + kListHead);
    for (std::uintptr_t node = TryRead<std::uintptr_t>(head).value_or(head);
         node != head && node != 0 && names.size() < kMostNames; node = TryRead<std::uintptr_t>(node).value_or(0))
    {
        if (auto name = EngineText(node + kNodeString))
        {
            names.push_back(std::move(*name));
        }
    }
    destroy(found.data(), 0);
    return names;
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
