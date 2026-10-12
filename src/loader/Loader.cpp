// The loader. Windows loads this in place of version.dll because it sits
// beside DAOrigins.exe, which imports version.dll by name and finds the
// game's folder before the system's. DllMain stands the proxy up (any exe
// in bin_ship that imports version.dll gets that much and no more), and in
// DAOrigins.exe detours the exe's entry point: every static import's
// DllMain has run by then, ours included, so a detour placed here catches
// the CRT's start, on the main thread, with the loader lock released.
// There the plugins are loaded and their DAOLib_Load called, then the entry
// point runs and the game starts.
//
// Which build of the game this is, and where its entry point is, comes from
// the address library's file for it (dao/Module.h): with none, nothing is
// loaded.
//
// The EA build (dev/BUILDS.md) is wrapped: the exe's entry point is a jump
// into Core\Activation.dll, and the game's code is encrypted until that has
// run, so there is nothing to detour when this DLL is loaded and a plugin's
// hooks would be written into bytes about to be decrypted over. There the
// plugins load at the first call the game's own code makes instead: the
// CRT's start reads the clock for its stack cookie before anything else
// (__security_init_cookie, the entry point's first call), so a detour on
// GetSystemTimeAsFileTime, watching for a caller inside the exe's .text, is
// the same moment one call later, on the same thread.
//
// Steam's build is wrapped the same way, by SteamStub 2.1 (the address
// library's dev/STEAM.md): its headers' entry point is the stub's, in a
// section of its own, and .text is encrypted until the stub has run. The
// same way in serves it: nothing here knows which wrapper it is, only that
// the headers' entry point is not the game's.
#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Plugin.h"
#include "dao/Resources.h"
#include "fix/ArchiveRead.h"
#include "fix/SavedFileName.h"
#include "loader/CrashLog.h"
#include "loader/Instance.h"
#include "loader/Proxy.h"
#include "perf/EngineHeap.h"
#include "perf/FastLoad.h"
#include "perf/OwnProcessors.h"

#include <Windows.h>

#include <intrin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

using EntryFn = void(__stdcall *)();
dao::Detour<EntryFn> g_entry;
HMODULE g_self = nullptr;

std::filesystem::path SelfPath()
{
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(g_self, buffer.data(), static_cast<DWORD>(buffer.size()));
    return {std::wstring_view(buffer.data(), length)};
}

bool IsGameProcess()
{
    auto name = dao::Module::Path().filename().wstring();
    std::ranges::transform(name, name.begin(), ::towlower);
    return name == L"daorigins.exe";
}

// Each plugin's override\ subfolder (daolib\plugins\<name>\override\) is a
// source of the engine's resource manager, as packages\core\override is
// (DAOLib #43): its files found by name, a texture, a model, a table, and
// its M2DA_ tables merged, by the engine itself. The subfolder, not the
// plugin's own folder, so that a source holds the game's kinds of file
// alone, not the plugin's DLL and its pdb. Asked for before the plugins
// load, so it is added as the engine sets its own sources up, at a priority
// just past the override folder's: the player's override still wins over a
// plugin and a plugin over the game's archives. dao/Resources.cpp has how,
// the same way a plugin adds a folder of its own (DAOLib #85).
void ServePluginFolders(const std::vector<std::filesystem::path> &plugins)
{
    std::error_code ec;
    for (const auto &dll : plugins)
    {
        const auto folder = dll.parent_path() / L"override";
        if (std::filesystem::is_directory(folder, ec) && !dao::resources::AddFolder(folder))
        {
            spdlog::warn("resources: {}\\override is not a source", folder.parent_path().filename().string());
        }
    }
}

// The folder DAOLIB_PLUGINS names, or an empty path without the variable
// (DAOLib #67). Every copy of the game shares the one game folder and holds
// its DLLs open, so a copy run for a test names its own plugins folder, a
// build tree's daolib\plugins as it stands, and loads those in place of the
// game folder's: nothing of the test goes into the game, and no other copy
// loads it. In place of, not as well as, and a folder that is not there
// loads nothing: a test must not run with another session's plugins by
// mistake.
std::filesystem::path PluginsAsked()
{
    std::wstring value(32768, L'\0'); // the longest a variable's value can be
    const DWORD length = GetEnvironmentVariableW(L"DAOLIB_PLUGINS", value.data(), static_cast<DWORD>(value.size()));
    value.resize(length < value.size() ? length : 0);
    return {value};
}

void LoadPlugins()
{
    const auto exePath = dao::Module::Path();
    const auto loaderPath = SelfPath();
    // Beside the loader, which deployed is beside the exe. A loader put into
    // the game from the build tree (tools\run_from_build.py) finds the build's
    // plugins the same way, with nothing copied into the game.
    const auto asked = PluginsAsked();
    const auto folder = asked.empty() ? loaderPath.parent_path() / L"daolib" / L"plugins" : asked;
    if (!asked.empty())
    {
        spdlog::info("DAOLIB_PLUGINS is set: plugins are loaded from {} and not from the game's folder",
                     asked.string());
    }
    const bool known = dao::Module::IsKnownExecutable();

    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec))
    {
        spdlog::info("no plugin folder at {}", folder.string());
        return;
    }
    // Each plugin is a folder of its own, daolib\plugins\<name>, holding
    // <name>.dll and the plugin's files (DAOLib #26): its movies, talk tables
    // and data, so that installing or removing a plugin is one folder. A DLL
    // loose in the plugins folder is not loaded, and the log says where it
    // goes.
    std::vector<std::filesystem::path> plugins;
    for (const auto &entry : std::filesystem::directory_iterator(folder, ec))
    {
        if (entry.is_directory(ec))
        {
            const auto dll = entry.path() / (entry.path().filename().wstring() + L".dll");
            if (std::filesystem::is_regular_file(dll, ec))
            {
                plugins.push_back(dll);
            }
        }
        else if (entry.is_regular_file(ec) && entry.path().extension() == L".dll")
        {
            const auto stem = entry.path().stem().string();
            spdlog::warn(R"({}: not loaded; a plugin goes in a folder of its own, daolib\plugins\{}\{}.dll)",
                         entry.path().filename().string(), stem, stem);
        }
    }
    std::ranges::sort(plugins);
    ServePluginFolders(plugins);

    for (const auto &path : plugins)
    {
        const auto name = path.filename().string();
        HMODULE module = LoadLibraryW(path.c_str());
        if (module == nullptr)
        {
            spdlog::error("{}: LoadLibrary failed, error {}", name, GetLastError());
            continue;
        }
        const auto load = reinterpret_cast<dao::LoadFn>(GetProcAddress(module, dao::kLoadExportName));
        if (load == nullptr)
        {
            spdlog::warn("{}: no {} export; not a DAOLib plugin, left loaded", name, dao::kLoadExportName);
            continue;
        }
        const dao::LoaderInfo info{
            .apiVersion = dao::kPluginApiVersion,
            .structSize = sizeof(dao::LoaderInfo),
            .loaderPath = loaderPath.c_str(),
            .pluginPath = path.c_str(),
            .executablePath = exePath.c_str(),
            .knownExecutable = known,
        };
        const bool ok = load(&info);
        spdlog::info("{}: {}", name, ok ? "loaded" : "refused to load");
    }
    spdlog::info("{} plugin(s) in {}", plugins.size(), folder.string());
}

void __stdcall EntryHook()
{
    spdlog::info("entry point reached; loading plugins");
    perf::InstallOwnProcessors();
    loader::instance::SeedSaves();
    loader::crashlog::StartWatchdog();
    perf::InstallEngineHeap();
    fix::InstallArchiveReadFix();
    fix::InstallSavedFileNameFix();
    perf::InstallFastLoad();
    LoadPlugins();
    spdlog::debug("starting the game");
    g_entry.Original()();
}

// The wrapped build's way in: the clock, as the game's code first reads it.
using SystemTimeFn = void(WINAPI *)(LPFILETIME);
SystemTimeFn g_systemTime = nullptr;
dao::Module::CodeRange g_code;
std::atomic<bool> g_gameStarted = false;

void WINAPI SystemTimeHook(LPFILETIME time)
{
    // The wrapper, the system and this DLL's own log read the clock too, and
    // only a caller in the exe's code is the game. Once is all it is for;
    // the detour stays, since another thread may be inside it.
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (!g_gameStarted.load(std::memory_order_relaxed) && g_code.Contains(caller) && !g_gameStarted.exchange(true))
    {
        spdlog::info("the game's code is running, decrypted (the clock read from {:#x}); loading plugins",
                     dao::Module::ImageAddress(caller));
        perf::InstallOwnProcessors();
        loader::instance::SeedSaves();
        loader::crashlog::StartWatchdog();
        perf::InstallEngineHeap();
        fix::InstallArchiveReadFix();
        fix::InstallSavedFileNameFix();
        perf::InstallFastLoad();
        LoadPlugins();
        spdlog::debug("starting the game");
    }
    g_systemTime(time);
}

bool WaitForGameCode()
{
    const auto code = dao::Module::Code();
    if (!code)
    {
        return false;
    }
    g_code = *code;
    g_systemTime = &GetSystemTimeAsFileTime;
    if (DetourTransactionBegin() != NO_ERROR)
    {
        return false;
    }
    DetourUpdateThread(GetCurrentThread());
    if (DetourAttach(reinterpret_cast<PVOID *>(&g_systemTime), reinterpret_cast<PVOID>(&SystemTimeHook)) != NO_ERROR ||
        DetourTransactionCommit() != NO_ERROR)
    {
        DetourTransactionAbort();
        return false;
    }
    return true;
}

void Attach()
{
    // Before the log is opened: an instance's log is in the instance's own folder.
    loader::instance::Install();
    dao::OpenLog("loader");
    loader::instance::Report();
    // First, so a crash anywhere after this is logged, the loader's own included.
    loader::crashlog::Install(SelfPath().parent_path());
    const auto fingerprint = dao::Module::Fingerprint();
    const daoaddr::Library *library = dao::Module::Library();
    if (fingerprint)
    {
        spdlog::info("DAOrigins.exe at {:#x}: link stamp {:#x}, image size {:#x}, entry rva {:#x}", dao::Module::Base(),
                     fingerprint->timeDateStamp, fingerprint->sizeOfImage, fingerprint->entryPointRva);
    }
    const auto build = dao::Module::Build();
    if (!build || library == nullptr)
    {
        spdlog::error("the address library has no file for this build of the game ({} beside the loader, under "
                      "daolib\\addresses); plugins are not loaded",
                      build ? daoaddr::FileName(*build) : std::string("an exe whose headers do not read"));
        return;
    }
    // The file by name, not its folder alone: which build's file was read is
    // the first thing to check when an ID is not where it should be.
    spdlog::info("address library: {} IDs for this build, from {}", library->Count(),
                 (dao::Module::AddressFolder() / daoaddr::FileName(*build)).string());
    // A wrapped build's headers give the wrapper's entry point, not the
    // game's, which the address library knows by its ID like any function.
    if (library->Rva(dao::addr::kEntryPoint) != build->entryPointRva)
    {
        if (!WaitForGameCode())
        {
            spdlog::error("could not detour GetSystemTimeAsFileTime; plugins are not loaded");
            return;
        }
        spdlog::info("the game's code is wrapped; plugins load when it first runs");
        return;
    }
    if (!g_entry.Attach(dao::addr::kEntryPoint, &EntryHook))
    {
        spdlog::error("could not detour the entry point at {:#x}", dao::Module::Address(dao::addr::kEntryPoint));
        return;
    }
    spdlog::info("entry point detoured; plugins load when the game starts");
}

} // namespace

// The DLL's entry point: external by contract, whatever the linter would
// prefer.
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID /*reserved*/) // NOLINT(misc-use-internal-linkage)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = instance;
        DisableThreadLibraryCalls(instance);
        if (DetourIsHelperProcess() != 0)
        {
            return TRUE;
        }
        if (!loader::proxy::Load())
        {
            return FALSE;
        }
        if (IsGameProcess())
        {
            Attach();
        }
    }
    return TRUE;
}
