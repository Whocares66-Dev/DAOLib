// The contract between the loader (version.dll) and a plugin DLL in
// bin_ship\daolib\plugins\. A plugin exports one function:
//
//     extern "C" __declspec(dllexport) bool DAOLib_Load(const dao::LoaderInfo* info);
//
// The loader calls it once, on the game's main thread, from the exe's
// entry point before any game code has run: the CRT is up, the game's
// globals are not yet constructed, and nothing of the game's is running,
// so a detour placed here is in place for the first call. Return false to
// say the plugin refused to load (the loader logs it and carries on).
//
// Plain C in shape so the loader and a plugin need not share a compiler
// version: no std types, no classes, nothing with a destructor.
#pragma once

#include <cstdint>

namespace dao
{

inline constexpr std::uint32_t kPluginApiVersion = 1;

struct LoaderInfo
{
    std::uint32_t apiVersion;      // kPluginApiVersion of the loader that calls
    std::uint32_t structSize;      // sizeof(LoaderInfo), so a newer loader can grow it
    const wchar_t *loaderPath;     // the loader DLL's own path
    const wchar_t *pluginPath;     // this plugin's path
    const wchar_t *executablePath; // DAOrigins.exe's
    bool expectedExecutable;       // Module::IsExpectedExecutable() at load
};

using LoadFn = bool(__cdecl *)(const LoaderInfo *);
inline constexpr const char *kLoadExportName = "DAOLib_Load";

} // namespace dao
