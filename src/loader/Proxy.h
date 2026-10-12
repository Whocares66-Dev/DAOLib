#pragma once

#include <Windows.h>

#include <string>

namespace loader::proxy
{

// The name another mod's dinput8.dll is given, beside the loader, for the
// loader's exports to pass to it and not straight to the system's.
inline constexpr const wchar_t *kChainedName = L"dinput8.chain.dll";

// What Load found of a chained DLL: its path, empty with none beside the
// loader, whether it loaded, and the system's error if it did not.
struct Chain
{
    std::wstring file;
    bool loaded = false;
    DWORD error = 0;
};

// Load the system's dinput8.dll, and a chained one if there is one, and
// point every exported thunk at it. Called from DllMain before anything
// else, with the loader's own module: the thunks jump through null until it
// has run. False if the system DLL could not be loaded, which is a process
// that cannot go on either way.
bool Load(HMODULE self);

// What Load found, for the log, which is not open when Load runs.
const Chain &Chained();

} // namespace loader::proxy
