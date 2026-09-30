// The loader. Windows loads this in place of version.dll because it sits
// beside DAOrigins.exe, which imports version.dll by name and finds the
// game's folder before the system's. DllMain stands the proxy up (any exe
// in bin_ship that imports version.dll gets that much and no more), and in
// DAOrigins.exe detours the exe's entry point: every static import's
// DllMain has run by then, ours included, so a detour placed here catches
// the CRT's start, on the main thread, with the loader lock released.
// There the plugins are loaded and their DAOLib_Load called, then the entry
// point runs and the game starts.
#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Plugin.h"
#include "loader/Proxy.h"

#include <Windows.h>

#include <algorithm>
#include <array>
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
    return std::filesystem::path(std::wstring_view(buffer.data(), length));
}

bool IsGameProcess()
{
    auto name = dao::Module::Path().filename().wstring();
    std::ranges::transform(name, name.begin(), ::towlower);
    return name == L"daorigins.exe";
}

void LoadPlugins()
{
    const auto exePath = dao::Module::Path();
    const auto loaderPath = SelfPath();
    const auto folder = exePath.parent_path() / L"daolib" / L"plugins";
    const bool expected = dao::Module::IsExpectedExecutable();

    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec))
    {
        spdlog::info("no plugin folder at {}", folder.string());
        return;
    }
    std::vector<std::filesystem::path> plugins;
    for (const auto &entry : std::filesystem::directory_iterator(folder, ec))
    {
        if (entry.is_regular_file(ec) && entry.path().extension() == L".dll")
        {
            plugins.push_back(entry.path());
        }
    }
    std::ranges::sort(plugins);

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
            .expectedExecutable = expected,
        };
        const bool ok = load(&info);
        spdlog::info("{}: {}", name, ok ? "loaded" : "refused to load");
    }
    spdlog::info("{} plugin(s) in {}", plugins.size(), folder.string());
}

void __stdcall EntryHook()
{
    spdlog::info("entry point reached; loading plugins");
    LoadPlugins();
    spdlog::info("starting the game");
    g_entry.Original()();
}

void Attach()
{
    dao::OpenLog("loader");
    const auto fingerprint = dao::Module::Fingerprint();
    if (fingerprint)
    {
        spdlog::info("DAOrigins.exe at {:#x}: link stamp {:#x}, image size {:#x}, entry rva {:#x}{}",
                     dao::Module::Base(), fingerprint->timeDateStamp, fingerprint->sizeOfImage,
                     fingerprint->entryPointRva,
                     dao::Module::IsExpectedExecutable() ? "" : " -- NOT the expected build");
    }
    if (!dao::Module::IsExpectedExecutable())
    {
        spdlog::error("this is not the build the addresses were read from (dao/core/Fingerprint.h); "
                      "plugins are not loaded");
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
        if (DetourIsHelperProcess())
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
