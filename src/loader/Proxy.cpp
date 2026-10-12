// The system's dinput8.dll, stood in for. DAOrigins.exe imports one of its
// functions (DirectInput8Create), and other code in the process may ask for
// any of the six, so every one is exported here as a jump through a pointer
// resolved, at attach, from the real DLL in the system folder (SysWOW64 for
// a 32-bit process, which GetSystemDirectoryW already redirects to).
//
// dinput8.dll and not version.dll, which the loader was until DAOLib #93: a
// compatibility setting on the exe (Disable fullscreen optimizations among
// them) has Windows load its shim DLLs before the exe's imports, from the
// system folder with all they import, and AcGenral.dll imports VERSION.dll
// and WINMM.dll. The exe's import is then answered by the system's, already
// loaded, and a version.dll beside the exe is never opened: the game runs
// and the loader does not. No shim DLL imports dinput8.dll (dev/BUILDS.md).
//
// The name is one other mods take too (a controller mod for this game does,
// a player's report on DAOLib #93), and two files cannot have it. The
// loader keeps it, since it is the one that loads plugins, and the other
// mod's DLL goes beside it renamed dinput8.chain.dll: every export here then
// passes to that DLL, which passes to the system's as it always did. A DLL
// that finds the system's by its bare name, not its path, would find this
// one again; none is known to.
//
// Naked functions and inline asm are x86-only, which the game is; a 64-bit
// build fails here on purpose rather than exporting nothing.
#include "loader/Proxy.h"

#include <Windows.h>

#include <array>
#include <string>

#ifndef _M_IX86
#error "The dinput8.dll proxy is written for x86, the game's architecture"
#endif

namespace
{

HMODULE g_real = nullptr;
loader::proxy::Chain g_chain;

} // namespace

// One pointer per export, filled by Load and jumped through by the thunk of
// the same name. extern "C" so the thunk's decorated name is predictable
// (_proxy_Name) for the /EXPORT below, and the pointer's for the asm.
// PRIVATE: out of the import library, which is where the linker wants the
// four Dll* names a COM server exports, and nothing links against this DLL.
extern "C"
{
#define DAO_PROXY(name)                                                                                                \
    FARPROC proxy_ptr_##name = nullptr;                                                                                \
    __declspec(naked) void proxy_##name(){__asm jmp dword ptr[proxy_ptr_##name]} __pragma(                             \
        comment(linker, "/EXPORT:" #name "=_proxy_" #name ",PRIVATE"))

    DAO_PROXY(DirectInput8Create)
    DAO_PROXY(DllCanUnloadNow)
    DAO_PROXY(DllGetClassObject)
    DAO_PROXY(DllRegisterServer)
    DAO_PROXY(DllUnregisterServer)
    DAO_PROXY(GetdfDIJoystick)
#undef DAO_PROXY
}

namespace loader::proxy
{

const Chain &Chained()
{
    return g_chain;
}

bool Load(HMODULE self)
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
    path += L"\\dinput8.dll";
    g_real = LoadLibraryW(path.c_str());
    if (g_real == nullptr)
    {
        return false;
    }

    // Another mod's dinput8.dll, where the player has put one beside the
    // loader under the chained name. One that will not load leaves the
    // system's in its place, and the game starts as it would have without.
    HMODULE chained = nullptr;
    std::array<wchar_t, MAX_PATH> own{};
    const DWORD ownLength = GetModuleFileNameW(self, own.data(), static_cast<DWORD>(own.size()));
    if (ownLength != 0 && ownLength < own.size())
    {
        std::wstring beside(own.data(), ownLength);
        beside.resize(beside.find_last_of(L'\\') + 1);
        beside += kChainedName;
        if (GetFileAttributesW(beside.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            chained = LoadLibraryW(beside.c_str());
            g_chain = {.file = beside, .loaded = chained != nullptr, .error = chained != nullptr ? 0 : GetLastError()};
        }
    }

    struct Export
    {
        const char *name;
        FARPROC *slot;
    };
    // Every slot, from the chained DLL where it has the name and from the
    // system's where it has not; a name the system DLL lacks (none today)
    // leaves its slot null, and a call to it faults where the real one would
    // have failed to link, which is the same outcome a step later.
    const std::array<Export, 6> exports{{
        {"DirectInput8Create", &proxy_ptr_DirectInput8Create},
        {"DllCanUnloadNow", &proxy_ptr_DllCanUnloadNow},
        {"DllGetClassObject", &proxy_ptr_DllGetClassObject},
        {"DllRegisterServer", &proxy_ptr_DllRegisterServer},
        {"DllUnregisterServer", &proxy_ptr_DllUnregisterServer},
        {"GetdfDIJoystick", &proxy_ptr_GetdfDIJoystick},
    }};
    for (const auto &e : exports)
    {
        *e.slot = chained != nullptr ? GetProcAddress(chained, e.name) : nullptr;
        if (*e.slot == nullptr)
        {
            *e.slot = GetProcAddress(g_real, e.name);
        }
    }
    return true;
}

} // namespace loader::proxy
