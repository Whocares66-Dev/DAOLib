// The system's version.dll, stood in for. DAOrigins.exe imports three of its
// functions (GetFileVersionInfoSizeW, GetFileVersionInfoW, VerQueryValueW)
// and other code in the process may ask for any of the seventeen, so every
// one is exported here as a jump through a pointer resolved, at attach,
// from the real DLL in the system folder (SysWOW64 for a 32-bit process,
// which GetSystemDirectoryW already redirects to).
//
// Naked functions and inline asm are x86-only, which the game is; a 64-bit
// build fails here on purpose rather than exporting nothing.
#include "loader/Proxy.h"

#include <Windows.h>

#include <array>
#include <string>

#ifndef _M_IX86
#error "The version.dll proxy is written for x86, the game's architecture"
#endif

namespace
{

HMODULE g_real = nullptr;

} // namespace

// One pointer per export, filled by Load and jumped through by the thunk of
// the same name. extern "C" so the thunk's decorated name is predictable
// (_proxy_Name) for the /EXPORT below, and the pointer's for the asm.
extern "C"
{
#define DAO_PROXY(name)                                                                                                \
    FARPROC proxy_ptr_##name = nullptr;                                                                                \
    __declspec(naked) void proxy_##name(){__asm jmp dword ptr[proxy_ptr_##name]} __pragma(                             \
        comment(linker, "/EXPORT:" #name "=_proxy_" #name))

    DAO_PROXY(GetFileVersionInfoA)
    DAO_PROXY(GetFileVersionInfoByHandle)
    DAO_PROXY(GetFileVersionInfoExA)
    DAO_PROXY(GetFileVersionInfoExW)
    DAO_PROXY(GetFileVersionInfoSizeA)
    DAO_PROXY(GetFileVersionInfoSizeExA)
    DAO_PROXY(GetFileVersionInfoSizeExW)
    DAO_PROXY(GetFileVersionInfoSizeW)
    DAO_PROXY(GetFileVersionInfoW)
    DAO_PROXY(VerFindFileA)
    DAO_PROXY(VerFindFileW)
    DAO_PROXY(VerInstallFileA)
    DAO_PROXY(VerInstallFileW)
    DAO_PROXY(VerLanguageNameA)
    DAO_PROXY(VerLanguageNameW)
    DAO_PROXY(VerQueryValueA)
    DAO_PROXY(VerQueryValueW)
#undef DAO_PROXY
}

namespace loader::proxy
{

bool Load()
{
    if (g_real != nullptr)
    {
        return true;
    }
    std::array<wchar_t, MAX_PATH> system{};
    const UINT length = GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
    if (length == 0 || length >= system.size())
    {
        return false;
    }
    std::wstring path(system.data(), length);
    path += L"\\version.dll";
    g_real = LoadLibraryW(path.c_str());
    if (g_real == nullptr)
    {
        return false;
    }

    struct Export
    {
        const char *name;
        FARPROC *slot;
    };
    // Every slot; a name the system DLL lacks (none today) leaves its slot
    // null, and a call to it faults where the real one would have failed
    // to link, which is the same outcome a step later.
    const std::array<Export, 17> exports{{
        {"GetFileVersionInfoA", &proxy_ptr_GetFileVersionInfoA},
        {"GetFileVersionInfoByHandle", &proxy_ptr_GetFileVersionInfoByHandle},
        {"GetFileVersionInfoExA", &proxy_ptr_GetFileVersionInfoExA},
        {"GetFileVersionInfoExW", &proxy_ptr_GetFileVersionInfoExW},
        {"GetFileVersionInfoSizeA", &proxy_ptr_GetFileVersionInfoSizeA},
        {"GetFileVersionInfoSizeExA", &proxy_ptr_GetFileVersionInfoSizeExA},
        {"GetFileVersionInfoSizeExW", &proxy_ptr_GetFileVersionInfoSizeExW},
        {"GetFileVersionInfoSizeW", &proxy_ptr_GetFileVersionInfoSizeW},
        {"GetFileVersionInfoW", &proxy_ptr_GetFileVersionInfoW},
        {"VerFindFileA", &proxy_ptr_VerFindFileA},
        {"VerFindFileW", &proxy_ptr_VerFindFileW},
        {"VerInstallFileA", &proxy_ptr_VerInstallFileA},
        {"VerInstallFileW", &proxy_ptr_VerInstallFileW},
        {"VerLanguageNameA", &proxy_ptr_VerLanguageNameA},
        {"VerLanguageNameW", &proxy_ptr_VerLanguageNameW},
        {"VerQueryValueA", &proxy_ptr_VerQueryValueA},
        {"VerQueryValueW", &proxy_ptr_VerQueryValueW},
    }};
    for (const auto &e : exports)
    {
        *e.slot = GetProcAddress(g_real, e.name);
    }
    return true;
}

} // namespace loader::proxy
