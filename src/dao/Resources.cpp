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
// Where a file comes from (DAOLib #106, dev/FILES.md): the manager's own list
// of its sources, which AddSource keeps in the order a file is looked for, and
// a resource's source (+0x1C). Seen in play 2026-10-12 (examples/resources with
// a where.txt, GOG's game from game\gogrun): 488 sources at the main menu and
// 859 with a save loaded in Forest Clearing; test files of one name in the
// Documents override, the install's, the plugin's, an add-in's and the core
// patch each read from the first of them the list has, and SourceOf named it.
#include "dao/Resources.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <unordered_map>
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
constexpr std::size_t kSourcePriority = 0x60;   // a source's priority, the lower looked in first
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

// A source name's path, asked of the engine's alias table as its own opener of an archive asks
// (dao/Addresses.h, kFileAliases_Path). The resolver gives an alias it has not back before its
// ':', and a name with no ':' as it is; an alias it has becomes its folder, a full path, so only a
// full path is an answer.
// Seen in play 2026-10-29 (examples/resources, DAOLib #267), at the main menu of a private copy
// of the GOG install: WRITE_OVERRIDE:, WRITE_DATA: and ADDINS: became the instance's Documents
// folders (packages\core\override\, packages\core\data\, AddIns\); OVERRIDE:, DATA:, PATCH:,
// ENV:, TEXTURES: (textures\high\), AUDIO_SOUND:, AUDIO_VO: (audio\vo\en-us\), BUILD_ADDINS: and
// BUILD_OFFERS: the install's, as dev/FILES.md has them, each there on disk but the install's
// override, which that copy has not; "BUILD_ADDINS:DAO_PRC_PROMO_WKD\core\..." came back with the
// rest lower-cased. NO_SUCH_ALIAS:x.rim and a bare rules_core.ncs gave no path.
std::optional<std::filesystem::path> PathOfName(const core::WStringRep *name)
{
    using GetFn = std::uintptr_t(__cdecl *)();
    using PathFn = core::WStringRep *(__fastcall *)(std::uintptr_t, int, core::WStringRep *, const core::WStringRep *);
    if (!Module::IsKnownExecutable())
    {
        return std::nullopt;
    }
    const std::uintptr_t aliases = reinterpret_cast<GetFn>(Module::Address(addr::kFileAliases_Get))();
    if (aliases == 0)
    {
        return std::nullopt;
    }
    WString path;
    reinterpret_cast<PathFn>(Module::Address(addr::kFileAliases_Path))(aliases, 0, path.Engine(), name);
    std::filesystem::path answer(path.View());
    return answer.is_absolute() ? std::optional(std::move(answer)) : std::nullopt;
}

// A RIM's bytes, from its file opened again: a RIM closes its one file once its task has read
// it whole (dao/Addresses.h, "A RIM's file"). Nothing for a packed RIM, whose files' offsets are
// into the unpacked block.
bool ReadRim(std::uintptr_t source, std::uint32_t offset, std::vector<std::byte> &bytes)
{
    if (TryRead<std::uint32_t>(source + kRimUnpacked).value_or(1) != 0)
    {
        return false;
    }
    const auto path = PathOfName(reinterpret_cast<const core::WStringRep *>(source + kSourceName));
    if (!path)
    {
        return false;
    }
    std::ifstream file(*path, std::ios::binary);
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

    // The source the resource came from, or 0.
    [[nodiscard]] std::uintptr_t Source() const
    {
        return handle_ != 0 ? TryRead<std::uintptr_t>(handle_ + kResourceSource).value_or(0) : 0;
    }

  private:
    std::uintptr_t handle_ = 0;
};

// A folder as AddSource takes it.
constexpr std::int32_t kDirectoryKind = 1;  // a folder with its subfolders and any .erf or .rim in it
constexpr std::int32_t kFolderPriority = 2; // OVERRIDE: is 1, PATCH: 100000; a tie is not decided by it
// Read once (0), or listed again every fifth frame (1) as OVERRIDE: is: AddFolder's `watched`.
constexpr std::int32_t kReadOnce = 0;
constexpr std::int32_t kWatched = 1;

std::mutex g_foldersMutex;
bool g_sourcesSetUp = false;         // the game has set its sources up: a folder goes straight to it
bool g_watching = false;             // how to learn of that has been seen to
std::vector<std::wstring> g_waiting; // folders asked for before then
std::vector<std::wstring> g_added;   // folders this copy of the library has given the game
std::vector<std::wstring> g_watched; // those of them given or to be given watched

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
    const bool watched = std::ranges::find(g_watched, name) != g_watched.end();
    const bool added = manager != nullptr && add(manager, 0, path.Engine(), kDirectoryKind, kFolderPriority, 0, nullptr,
                                                 watched ? kWatched : kReadOnce, 0);
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

// A source as a plugin is told of it, or nothing if it does not read as one.
std::optional<Source> SourceAt(std::uintptr_t source)
{
    const auto vftable = TryRead<std::uintptr_t>(source);
    const auto priority = TryRead<std::int32_t>(source + kSourcePriority);
    auto name = EngineText(source + kSourceName);
    if (!vftable || !priority || !name)
    {
        return std::nullopt;
    }
    Source found{.name = std::move(*name), .priority = *priority};
    if (*vftable == Module::Address(addr::kDirectorySource_Vftable))
    {
        found.kind = SourceKind::Folder;
    }
    else if (*vftable == Module::Address(addr::kRimSource_Vftable))
    {
        found.kind = SourceKind::Rim;
    }
    else if (*vftable == Module::Address(addr::kErfSource_Vftable))
    {
        found.kind = SourceKind::Archive;
    }
    return found;
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

namespace
{

// ReadAsync's reads: asked for, read on a thread of the library's, handed back on the game thread.
// Read 2026-10-15 for DAOLib #138 (DAOElgaraShaders: a material's three textures read on the game
// thread took up to 156 ms): Resources::Find on a thread other than the game's asks for the
// resource and waits for it (0x004D5A50, then WaitLoaded), as the engine's own streaming threads
// do, where on the game thread exploring it asks and goes on (0x004D5A90); and a texture's bytes
// are fetched from its source under the source's own lock (Fetch, above). Neither needs the game
// thread, so Read is run here as it is, on the library's one reading thread.
struct Asked
{
    std::wstring file;
    ReadDone done;
};
struct Answered
{
    ReadDone done;
    std::optional<std::vector<std::byte>> bytes;
};

std::mutex g_asyncMutex;
std::condition_variable g_asked;
std::deque<Asked> g_toRead;     // under g_asyncMutex
std::vector<Answered> g_toHand; // under g_asyncMutex

void ReadThread()
{
    for (;;)
    {
        Asked asked;
        {
            std::unique_lock lock(g_asyncMutex);
            g_asked.wait(lock, [] { return !g_toRead.empty(); });
            asked = std::move(g_toRead.front());
            g_toRead.pop_front();
        }
        std::optional<std::vector<std::byte>> bytes;
        detail::Guard("a resource's read", [&] { bytes = Read(asked.file); });
        const std::lock_guard lock(g_asyncMutex);
        g_toHand.push_back({std::move(asked.done), std::move(bytes)});
    }
}

void HandBack()
{
    std::vector<Answered> answered;
    {
        const std::lock_guard lock(g_asyncMutex);
        answered = std::exchange(g_toHand, {});
    }
    for (Answered &answer : answered)
    {
        detail::Guard("a resource's read's callback", [&] { answer.done(std::move(answer.bytes)); });
    }
}

} // namespace

void ReadAsync(std::wstring_view file, ReadDone done)
{
    if (!done)
    {
        return;
    }
    static std::once_flag once;
    std::call_once(once, [] {
        // Never joined: the library's DLL stays until the process ends, and the thread with it.
        std::thread(&ReadThread).detach();
        OnFrame(&HandBack);
    });
    {
        const std::lock_guard lock(g_asyncMutex);
        g_toRead.push_back({std::wstring(file), std::move(done)});
    }
    g_asked.notify_one();
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

bool AddFolder(const std::filesystem::path &folder, bool watched)
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
    if (watched)
    {
        g_watched.push_back(name);
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
    std::erase(g_watched, name);
    if (std::erase(g_waiting, name) != 0)
    {
        return true;
    }
    return std::erase(g_added, name) != 0 && Take(name);
}

// The manager's list of its sources, which AddSource keeps in the order a file is looked for:
// by priority, then by the source's kind (its slot 8: a folder 0 or 1, a RIM 2, an .erf 3), then
// by name, the later first (0x004E6660). Each name's sources are kept in the same order in the
// manager's index (0x004C7C50), and a file is taken from the first of them.
std::vector<Source> Sources()
{
    constexpr std::size_t kListHead = 0x08;  // in the manager: its list of sources' head node
    constexpr std::size_t kListSize = 0x0C;  // and how many it holds
    constexpr std::size_t kNodeValue = 0x08; // in a node: the source, the next node at +0
    // Far more than the game makes (some 13,000 archives and folders); a list longer is a misread.
    constexpr std::uint32_t kMostSources = 1U << 16U;

    std::vector<Source> sources;
    const auto manager = reinterpret_cast<std::uintptr_t>(Manager());
    const auto head = manager != 0 ? TryRead<std::uintptr_t>(manager + kListHead) : std::nullopt;
    const std::uint32_t count = manager != 0 ? TryRead<std::uint32_t>(manager + kListSize).value_or(0) : 0;
    if (!head || *head == 0 || count > kMostSources)
    {
        return sources;
    }
    sources.reserve(count);
    std::uintptr_t node = TryRead<std::uintptr_t>(*head).value_or(0);
    for (std::uint32_t i = 0; i < count && node != 0 && node != *head; ++i)
    {
        const std::uintptr_t source = TryRead<std::uintptr_t>(node + kNodeValue).value_or(0);
        if (source != 0 && TryRead<std::uint8_t>(source + kSourceClosed).value_or(1) == 0)
        {
            if (auto found = SourceAt(source))
            {
                sources.push_back(std::move(*found));
            }
        }
        node = TryRead<std::uintptr_t>(node).value_or(0);
    }
    return sources;
}

std::optional<std::filesystem::path> PathOf(std::wstring_view sourceName)
{
    WString name(sourceName);
    return PathOfName(name.Engine());
}

std::optional<Source> SourceOf(std::wstring_view file)
{
    if (file.empty())
    {
        return std::nullopt;
    }
    const Found found(file);
    const std::uintptr_t source = found.Source();
    return source != 0 ? SourceAt(source) : std::nullopt;
}

namespace
{

// A resource's fields as its read leaves them (dao/Addresses.h, kResource_ReadOnFirstUse).
constexpr std::size_t kResourceBytes = 0x20;    // its bytes, the engine's to free unless borrowed
constexpr std::size_t kResourceLength = 0x30;   // their length as Resources::Size answers it
constexpr std::size_t kResourceState = 0x34;    // 2 once read, 3 if the read failed
constexpr std::size_t kResourceBorrowed = 0x3C; // the bytes are not the resource's to free
constexpr std::size_t kResourceRead = 0x3D;     // the read is done
constexpr std::int32_t kRead = 2;

std::mutex g_loadMutex;
std::vector<std::pair<std::wstring, ResourcePatch>> g_loadPatches; // by extension, lower case, no dot
// The length each patched resource had before its patch, by the resource and its name: given back
// before the game reads it again, so that its next read and patch start from the file.
std::vector<std::tuple<std::uintptr_t, std::wstring, std::uint32_t>> g_patchedLengths;

using ReadOnFirstUseFn = void(__fastcall *)(void *resource, int edx);
Detour<ReadOnFirstUseFn> g_readOnFirstUse;

std::wstring Lower(std::wstring_view text)
{
    std::wstring out(text);
    std::ranges::transform(out, out.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return out;
}

std::wstring ExtensionOf(const std::wstring &name)
{
    const std::size_t dot = name.rfind(L'.');
    return dot == std::wstring::npos ? std::wstring{} : name.substr(dot + 1);
}

// Gives a resource the length it had before an earlier patch, if it had one.
void GiveBackLength(std::uintptr_t resource, const std::wstring &name)
{
    const std::lock_guard lock(g_loadMutex);
    const auto it = std::ranges::find_if(g_patchedLengths, [&](const auto &entry) {
        return std::get<0>(entry) == resource && std::get<1>(entry) == name;
    });
    if (it != g_patchedLengths.end())
    {
        *reinterpret_cast<std::uint32_t *>(resource + kResourceLength) = std::get<2>(*it);
        g_patchedLengths.erase(it);
    }
}

// Runs the type's patches over a resource the game has just read, and gives it their result in a
// buffer of the engine's own, which the engine frees as it would have freed the file's.
void PatchRead(std::uintptr_t resource, const std::wstring &name)
{
    std::vector<ResourcePatch> patches;
    {
        const std::wstring extension = ExtensionOf(name);
        const std::lock_guard lock(g_loadMutex);
        for (const auto &[type, patch] : g_loadPatches)
        {
            if (type == extension)
            {
                patches.push_back(patch);
            }
        }
    }
    const auto bytes = TryRead<std::uintptr_t>(resource + kResourceBytes);
    const auto length = TryRead<std::uint32_t>(resource + kResourceLength);
    if (patches.empty() || !bytes || *bytes == 0 || !length || *length > kMostBytes ||
        TryRead<std::int32_t>(resource + kResourceState) != kRead)
    {
        return;
    }
    std::vector<std::byte> current(*length);
    if (!TryCopy(current.data(), *bytes, current.size()))
    {
        return;
    }
    bool changed = false;
    for (const ResourcePatch &patch : patches)
    {
        detail::Guard("a resource's patch", [&] {
            if (auto next = patch(name, current))
            {
                current = std::move(*next);
                changed = true;
            }
        });
    }
    if (!changed || current.size() > kMostBytes)
    {
        return;
    }
    using NewFn = void *(__cdecl *)(std::size_t);
    using DeleteFn = void(__cdecl *)(void *);
    void *fresh =
        reinterpret_cast<NewFn>(Module::Address(addr::kOperatorNew))(std::max<std::size_t>(current.size(), 1));
    if (fresh == nullptr)
    {
        return;
    }
    std::memcpy(fresh, current.data(), current.size());
    if (TryRead<std::uint8_t>(resource + kResourceBorrowed).value_or(1) == 0)
    {
        reinterpret_cast<DeleteFn>(Module::Address(addr::kOperatorDelete))(reinterpret_cast<void *>(*bytes));
    }
    *reinterpret_cast<std::uintptr_t *>(resource + kResourceBytes) = reinterpret_cast<std::uintptr_t>(fresh);
    *reinterpret_cast<std::uint32_t *>(resource + kResourceLength) = static_cast<std::uint32_t>(current.size());
    const std::lock_guard lock(g_loadMutex);
    g_patchedLengths.emplace_back(resource, name, *length);
}

// A texture made from a .dds within its file's first read (dao/Addresses.h, kTexManager_MakeTexture):
// the read's name is the thread's while the read runs, and MakeTexture, inside it, reports what it
// makes by that name.
std::mutex g_textureMutex;
std::vector<std::pair<TextureEvent, TextureEvent>> g_textureEvents;
// The textures reported and not yet released, by the engine's object.
std::unordered_map<std::uintptr_t, std::pair<IDirect3DBaseTexture9 *, std::wstring>> g_textures;
thread_local const std::wstring *t_textureRead = nullptr;

// The engine's three kinds of texture, and where each keeps its Direct3D texture.
struct TextureKind
{
    AddressId vftable;
    std::size_t direct3d;
};
constexpr std::array kTextureKinds{
    TextureKind{addr::kD3DTexture2D_Vftable, 0x4C},
    TextureKind{addr::kD3DTextureCubeMap_Vftable, 0x40},
    TextureKind{addr::kD3DTextureVolume_Vftable, 0x48},
};

std::vector<TextureEvent> TextureEvents(bool made)
{
    const std::lock_guard lock(g_textureMutex);
    std::vector<TextureEvent> events;
    for (const auto &[onMade, onReleased] : g_textureEvents)
    {
        if (const TextureEvent &event = made ? onMade : onReleased)
        {
            events.push_back(event);
        }
    }
    return events;
}

void RunTextureEvents(bool made, const Texture &texture)
{
    for (const TextureEvent &event : TextureEvents(made))
    {
        detail::Guard("a texture's callback", [&] { event(texture); });
    }
}

// Reports a texture MakeTexture answered. A file read again can be answered with the texture the
// engine already has for it (seen 2026-10-21, a level's sky and lightmaps on a second load of the
// level: the same object and Direct3D texture, not freed between), which is not reported twice; an
// object known with another texture is reported released first.
void TextureMade(std::uintptr_t engine, const std::wstring &name)
{
    const std::uintptr_t vftable = TryRead<std::uintptr_t>(engine).value_or(0);
    const auto kind = std::ranges::find_if(
        kTextureKinds, [&](const TextureKind &each) { return Module::Address(each.vftable) == vftable; });
    if (kind == kTextureKinds.end())
    {
        return;
    }
    auto *const direct3d =
        reinterpret_cast<IDirect3DBaseTexture9 *>(TryRead<std::uintptr_t>(engine + kind->direct3d).value_or(0));
    if (direct3d == nullptr)
    {
        return;
    }
    std::optional<std::pair<IDirect3DBaseTexture9 *, std::wstring>> replaced;
    {
        const std::lock_guard lock(g_textureMutex);
        const auto [it, added] = g_textures.try_emplace(engine, direct3d, name);
        if (!added)
        {
            if (it->second.first == direct3d && it->second.second == name)
            {
                return;
            }
            replaced = std::exchange(it->second, std::pair{direct3d, name});
        }
    }
    if (replaced)
    {
        RunTextureEvents(false, Texture{.name = replaced->second, .texture = replaced->first});
    }
    RunTextureEvents(true, Texture{.name = name, .texture = direct3d});
}

void TextureFreed(std::uintptr_t engine)
{
    std::optional<std::pair<IDirect3DBaseTexture9 *, std::wstring>> freed;
    {
        const std::lock_guard lock(g_textureMutex);
        if (const auto it = g_textures.find(engine); it != g_textures.end())
        {
            freed = std::move(it->second);
            g_textures.erase(it);
        }
    }
    if (freed)
    {
        RunTextureEvents(false, Texture{.name = freed->second, .texture = freed->first});
    }
}

// MakeTexture, __cdecl (bytes, size, driver, flag), answering the texture or null.
using MakeTextureFn = void *(__cdecl *)(const void *bytes, std::uint32_t size, void *driver, std::int32_t flag);
Detour<MakeTextureFn> g_makeTexture;

void *__cdecl MakeTexture(const void *bytes, std::uint32_t size, void *driver, std::int32_t flag)
{
    void *const made = g_makeTexture.Original()(bytes, size, driver, flag);
    if (made != nullptr && t_textureRead != nullptr)
    {
        TextureMade(reinterpret_cast<std::uintptr_t>(made), *t_textureRead);
    }
    return made;
}

// The three textures' destructors, __thiscall with no arguments.
using TextureDestructorFn = void(__fastcall *)(void *texture, int edx);
std::array<Detour<TextureDestructorFn>, kTextureKinds.size()> g_textureDestructors;

template <std::size_t Kind> void __fastcall TextureDestructor(void *texture, int edx)
{
    TextureFreed(reinterpret_cast<std::uintptr_t>(texture));
    g_textureDestructors[Kind].Original()(texture, edx);
}

// ReadOnFirstUse, __thiscall on the resource, a plain `ret`. Both its callers, Resources::Data and
// ReplaceBuffer, hold the resource's source's lock around it, so the patch and the swap after it
// are under that lock too.
void __fastcall ReadOnFirstUse(void *resource, int edx)
{
    const auto at = reinterpret_cast<std::uintptr_t>(resource);
    const bool unread = TryRead<std::uint8_t>(at + kResourceRead).value_or(1) == 0;
    std::optional<std::wstring> name = unread ? EngineText(at + kResourceName) : std::nullopt;
    if (name)
    {
        *name = Lower(*name);
        GiveBackLength(at, *name);
    }
    const std::wstring *const outer = t_textureRead;
    t_textureRead = name && IsProcessed(*name) ? &*name : nullptr;
    g_readOnFirstUse.Original()(resource, edx);
    t_textureRead = outer;
    if (name && TryRead<std::uint8_t>(at + kResourceRead) == 1 && !IsProcessed(*name))
    {
        PatchRead(at, *name);
    }
}

bool HasPatch(const std::wstring &name)
{
    const std::wstring extension = ExtensionOf(name);
    const std::lock_guard lock(g_loadMutex);
    return std::ranges::any_of(g_loadPatches, [&](const auto &entry) { return entry.first == extension; });
}

// Resources::Size, __thiscall on a handle (the resource at its +0), a plain `ret`, answering the
// resource's +0x30. Read 2026-10-15 for DAOLib #135: the material loader (0x004F72D0) asks Size
// before Data, and Data makes the first read, so where a material is first read there, as an
// add-in's are (DAOElgaraShaders: the Blood Dragon plate's pn_arm_bdaa.mao), Size answered the
// file's length and the parser cut the patched text there: a comment added inside the root
// broke the material, the same after it did not. The core's materials had mostly been read
// before, and so drew right. So Size of a resource not yet read, of a type a patch is for, has
// the engine read it first, through Data as the loader would next, and answers its length
// patched.
using SizeFn = std::uint32_t(__fastcall *)(void *handle, int edx);
Detour<SizeFn> g_size;

std::uint32_t __fastcall Size(void *handle, int edx)
{
    const std::uintptr_t resource =
        handle != nullptr ? TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(handle)).value_or(0) : 0;
    if (resource != 0 && TryRead<std::uint8_t>(resource + kResourceRead).value_or(1) == 0)
    {
        const std::optional<std::wstring> name = EngineText(resource + kResourceName);
        if (name && HasPatch(Lower(*name)))
        {
            using DataFn = std::uintptr_t(__fastcall *)(void *, int);
            reinterpret_cast<DataFn>(Module::Address(addr::kResources_Data))(handle, 0);
        }
    }
    return g_size.Original()(handle, edx);
}

// Detours a file's first read, once, for patches and textures both; under g_loadMutex.
void AttachReads()
{
    if (g_readOnFirstUse.Original() == nullptr)
    {
        g_readOnFirstUse.Attach(addr::kResource_ReadOnFirstUse, &ReadOnFirstUse);
        g_size.Attach(addr::kResources_Size, &Size);
    }
}

} // namespace

// Read 2026-10-14 for DAOLib #124 (DAOElgaraShaders #19): the material loader (0x004F72D0) finds
// "<name>.mao.xml", else "<name>.mao", and parses what Resources::Data answers, of the length
// Resources::Size answers (the resource's +0x30; its +0x2C is the length in its source, which a
// read fetches by and is left alone here). Data reads through ReadOnFirstUse, so its bytes pass
// this detour first.
void OnLoad(std::wstring_view extension, ResourcePatch patch)
{
    if (!patch)
    {
        return;
    }
    std::wstring type = Lower(extension);
    if (type.starts_with(L'.'))
    {
        type.erase(0, 1);
    }
    const std::lock_guard lock(g_loadMutex);
    g_loadPatches.emplace_back(std::move(type), std::move(patch));
    AttachReads();
}

// Read 2026-10-21 for DAOLib #228 (DAOElgaraShaders: a level's day lightmaps swapped for its
// night's at draw time): a .dds's texture is made inside its file's first read, by MakeTexture
// (dao/Addresses.h, kTexManager_MakeTexture), and freed by its kind's destructor, which releases
// the Direct3D texture.
void OnTexture(TextureEvent made, TextureEvent released)
{
    if (!made && !released)
    {
        return;
    }
    {
        const std::lock_guard lock(g_textureMutex);
        g_textureEvents.emplace_back(std::move(made), std::move(released));
        if (g_makeTexture.Original() != nullptr)
        {
            return;
        }
        g_makeTexture.Attach(addr::kTexManager_MakeTexture, &MakeTexture);
        g_textureDestructors[0].Attach(addr::kD3DTexture2D_Destructor, &TextureDestructor<0>);
        g_textureDestructors[1].Attach(addr::kD3DTextureCubeMap_Destructor, &TextureDestructor<1>);
        g_textureDestructors[2].Attach(addr::kD3DTextureVolume_Destructor, &TextureDestructor<2>);
    }
    const std::lock_guard lock(g_loadMutex);
    AttachReads();
}

} // namespace dao::resources
