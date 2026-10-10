// The contract between the loader and a plugin: the function a plugin DLL exports, and what
// the loader passes to it.
//
// A plugin exports one function, which the loader calls once at start-up:
//
//   extern "C" __declspec(dllexport) bool DAOLib_Load(const dao::LoaderInfo* info);

// Notes on the contract: a plugin is a DLL in its own folder,
// bin_ship\daolib\plugins\<name>\<name>.dll, its files beside it (DAOLib
// #26). The contract is plain C in shape so the loader and a plugin need not
// share a compiler version: no std types, no classes, nothing with a
// destructor.

#pragma once

#include <cstdint>

namespace dao
{

// Notes on kPluginApiVersion: 2 since 2026-10-01, when the library came to
// know more than one build of the game; a plugin built against version 1
// would use GOG's addresses in the EA build.

// The version of this contract. A plugin should refuse to load when LoaderInfo::apiVersion
// differs from the version it was built with.
inline constexpr std::uint32_t kPluginApiVersion = 2;

// What the loader passes to DAOLib_Load.
struct LoaderInfo
{
    std::uint32_t apiVersion;      // the loader's kPluginApiVersion
    std::uint32_t structSize;      // sizeof(LoaderInfo), so that a newer loader can add fields
    const wchar_t *loaderPath;     // the full path of the loader DLL
    const wchar_t *pluginPath;     // the full path of this plugin's DLL
    const wchar_t *executablePath; // the full path of DAOrigins.exe
    bool knownExecutable;          // whether the library has addresses for this build of the game
};

// The type of DAOLib_Load, the function a plugin exports.
//
// The loader calls it once, from the executable's entry point, before any game code has run. It
// runs on the process's main thread, which is not the game thread (dao/Frame.h). A detour placed
// here is therefore in place before the game first calls the hooked function. No game object
// exists yet, so place hooks but read nothing from the game. Return false to refuse to load; the
// loader logs the refusal and carries on.
using LoadFn = bool(__cdecl *)(const LoaderInfo *);

inline constexpr const char *kLoadExportName = "DAOLib_Load"; // the exported function's name

} // namespace dao
