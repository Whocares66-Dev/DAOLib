#include "loader/Instance.h"

#include "dao/Log.h"
#include "dao/String.h"
#include "dao/core/Instance.h"

#include <Windows.h>

#include <ShlObj.h>
#include <detours/detours.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace loader::instance
{

namespace
{

std::string g_name;       // the instance's name; empty for the game as ever
std::string g_seeded;     // what a new instance's profile was given of the default one's
std::string g_problem;    // why a name asked for was not taken
std::wstring g_mutex;     // the instance's mutex
std::wstring g_documents; // the instance's Documents folder

// The game's WinMain (0x00416AD0 in GOG's build) makes the mutex
// L"dragonage" at 0x00416BD6 and waits on it with no timeout, returning -1
// if another copy holds it: read 2026-10-10, dev/BUILDS.md. Its other named
// mutex, L"DragonAgeAddInsXML" (0x005F9044), guards a file the copies share
// and is left as it is.
using CreateMutexFn = HANDLE(WINAPI *)(LPSECURITY_ATTRIBUTES, BOOL, LPCWSTR);
CreateMutexFn g_createMutex = &CreateMutexW;

HANDLE WINAPI CreateMutexHook(LPSECURITY_ATTRIBUTES security, BOOL owned, LPCWSTR name)
{
    if (name != nullptr && dao::core::kGameMutex == name)
    {
        name = g_mutex.c_str();
    }
    return g_createMutex(security, owned, name);
}

// The game asks for Documents in four places (0x0040F0AB, 0x004A58F1,
// 0x004BFE29, 0x005EC68F), each SHGetFolderPathW with CSIDL_PERSONAL and no
// flags, into a MAX_PATH buffer; shell32's other import is ShellExecuteW.
using FolderPathFn = HRESULT(WINAPI *)(HWND, int, HANDLE, DWORD, LPWSTR);
FolderPathFn g_folderPath = &SHGetFolderPathW;

HRESULT WINAPI FolderPathHook(HWND window, int folder, HANDLE token, DWORD flags, LPWSTR path)
{
    const HRESULT result = g_folderPath(window, folder, token, flags, path);
    constexpr int kFolderMask = 0xFF; // the rest are CSIDL_FLAG_*
    if (SUCCEEDED(result) && path != nullptr && (folder & kFolderMask) == CSIDL_PERSONAL)
    {
        wcscpy_s(path, MAX_PATH, g_documents.c_str());
    }
    return result;
}

// DAOLib's own logs and crash logs ask this way (dao::LogDirectory), in the
// loader and in every plugin.
using KnownFolderFn = HRESULT(WINAPI *)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR *);
KnownFolderFn g_knownFolder = &SHGetKnownFolderPath;

HRESULT WINAPI KnownFolderHook(REFKNOWNFOLDERID folder, DWORD flags, HANDLE token, PWSTR *path)
{
    const HRESULT result = g_knownFolder(folder, flags, token, path);
    if (FAILED(result) || path == nullptr || IsEqualGUID(folder, FOLDERID_Documents) == 0)
    {
        return result;
    }
    const std::size_t bytes = (g_documents.size() + 1) * sizeof(wchar_t);
    auto *ours = static_cast<PWSTR>(CoTaskMemAlloc(bytes));
    if (ours == nullptr)
    {
        return result;
    }
    memcpy(ours, g_documents.c_str(), bytes);
    CoTaskMemFree(*path);
    *path = ours;
    return result;
}

constexpr const char *kVariable = "DAOLIB_INSTANCE";

std::string NameAsked()
{
    std::array<char, dao::core::kLongestInstanceName + 2> value{};
    const DWORD length = GetEnvironmentVariableA(kVariable, value.data(), static_cast<DWORD>(value.size()));
    // Longer than the buffer is longer than a name may be: kept as it came, to be refused by name.
    return length < value.size() ? std::string(value.data(), length) : std::string(value.size(), '?');
}

// One of the user's folders as the system gives it: asked before the detours are placed.
std::filesystem::path KnownFolder(REFKNOWNFOLDERID folder)
{
    PWSTR found = nullptr;
    std::filesystem::path path;
    if (SUCCEEDED(SHGetKnownFolderPath(folder, KF_FLAG_DEFAULT, nullptr, &found)))
    {
        path = found;
    }
    CoTaskMemFree(found);
    return path;
}

// A new instance's profile is given the default profile's: the settings and
// key bindings, what says which add-ins are installed and allowed
// (Settings\AddIns.xml, Settings\offers.xml, addins, Offers), without which
// the instance has no downloadable content (the user, 2026-10-11: "no dlc in
// new session"), the profile's own override packages, and the saves ("maybe
// copy the saves too. the saves thereafter will be separate"). Not the
// screenshots or the logs. Once: a file named seeded in the instance's
// folder says it is done, and after that the instance's are its own. A copy
// cut short is taken up again at the next start, what is there left alone.
constexpr const wchar_t *kSeededMarker = L"seeded";

std::filesystem::path g_seedFrom; // the default profile, while this instance's is still to be given its saves
std::filesystem::path g_seedTo;

// Copies one folder of the default profile; false if not all of it came.
bool CopyPart(const char *part)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(g_seedFrom / part, ec))
    {
        return true;
    }
    std::filesystem::create_directories(g_seedTo / part, ec);
    std::filesystem::copy(g_seedFrom / part, g_seedTo / part,
                          std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_existing, ec);
    g_seeded += g_seeded.empty() ? "" : ", ";
    g_seeded += part;
    g_seeded += ec ? " (not all of it)" : "";
    return !ec;
}

// The small parts, which the game reads as it starts: from DllMain, before
// anything asks for Documents.
void SeedProfile(const std::filesystem::path &documents, const std::filesystem::path &ours)
{
    std::error_code ec;
    const std::filesystem::path from = documents / L"BioWare" / L"Dragon Age";
    if (std::filesystem::exists(ours / kSeededMarker, ec) || !std::filesystem::is_directory(from, ec))
    {
        return;
    }
    g_seedFrom = from;
    g_seedTo = ours / L"BioWare" / L"Dragon Age";
    for (const char *part : {"Settings", "addins", "Offers", "packages"})
    {
        CopyPart(part);
    }
}

// All three or none: a copy with its own mutex and the shared profile would
// write over the first one's settings and logs.
bool Detour()
{
    if (DetourTransactionBegin() != NO_ERROR)
    {
        return false;
    }
    DetourUpdateThread(GetCurrentThread());
    const bool attached =
        DetourAttach(reinterpret_cast<PVOID *>(&g_createMutex), reinterpret_cast<PVOID>(&CreateMutexHook)) ==
            NO_ERROR &&
        DetourAttach(reinterpret_cast<PVOID *>(&g_folderPath), reinterpret_cast<PVOID>(&FolderPathHook)) == NO_ERROR &&
        DetourAttach(reinterpret_cast<PVOID *>(&g_knownFolder), reinterpret_cast<PVOID>(&KnownFolderHook)) == NO_ERROR;
    if (!attached)
    {
        DetourTransactionAbort();
        return false;
    }
    return DetourTransactionCommit() == NO_ERROR;
}

} // namespace

void Install()
{
    const std::string asked = NameAsked();
    if (asked.empty())
    {
        return;
    }
    // Whatever comes of it, the variable ends as what this process is: the name as taken, or
    // not there. A plugin asks it (dao::Module::Instance), and a child process inherits it.
    SetEnvironmentVariableA(kVariable, nullptr);
    const auto taken = dao::core::InstanceOf(asked);
    if (!taken)
    {
        g_problem = "it is not a name: 1 to 32 letters, digits, '-' or '_'";
        return;
    }
    const std::string &name = *taken;
    const std::filesystem::path documents = KnownFolder(FOLDERID_Documents);
    const std::filesystem::path local = KnownFolder(FOLDERID_LocalAppData);
    if (documents.empty() || local.empty())
    {
        g_problem = "the Documents folder or the local application data folder was not found";
        return;
    }
    const std::filesystem::path ours = dao::core::InstanceDocuments(local, name);
    std::error_code ec;
    std::filesystem::create_directories(ours, ec);
    if (ec || ours.native().size() >= MAX_PATH)
    {
        g_problem = "its folder could not be made, or its path is too long: " + ours.string();
        return;
    }
    SeedProfile(documents, ours);
    g_documents = ours.native();
    g_mutex = dao::core::InstanceMutexName(name);
    if (!Detour())
    {
        g_problem = "the detours could not be placed";
        return;
    }
    g_name = name;
    SetEnvironmentVariableA(kVariable, g_name.c_str());
}

void SeedSaves()
{
    // Not for a name that was refused after its small parts were copied: that process is the game as ever.
    if (g_seedFrom.empty() || g_name.empty())
    {
        return;
    }
    const ULONGLONG began = GetTickCount64();
    spdlog::info("instance \"{}\": its profile is new; copying the default profile's saves, which can take a while",
                 g_name);
    const bool all = CopyPart("Characters");
    if (all)
    {
        // Only now: a copy cut short is taken up again at the next start.
        std::ofstream(g_seedTo.parent_path().parent_path() / kSeededMarker) << "seeded\n";
    }
    spdlog::info("instance \"{}\": its profile was given the default profile's {}, in {} ms{}", g_name, g_seeded,
                 GetTickCount64() - began, all ? "" : "; the copy is taken up again at the next start");
    g_seedFrom.clear();
}

void Report()
{
    if (!g_problem.empty())
    {
        spdlog::warn("instance: DAOLIB_INSTANCE is set, but {}; this is the game as ever, which runs one at a time",
                     g_problem);
    }
    else if (!g_name.empty())
    {
        spdlog::info("instance \"{}\": the game's mutex is {} and its Documents folder {}", g_name, dao::Utf8(g_mutex),
                     dao::Utf8(g_documents));
    }
}

} // namespace loader::instance
