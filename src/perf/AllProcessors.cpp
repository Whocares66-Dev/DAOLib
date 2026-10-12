#include "perf/AllProcessors.h"

#include "dao/Log.h"

#include <Windows.h>

#include <array>

namespace perf
{

namespace
{

bool LeftOut()
{
    std::array<wchar_t, 8> value{};
    const DWORD length = GetEnvironmentVariableW(L"DAOLIB_ALL_PROCESSORS", value.data(), 8);
    return length == 1 && value[0] == L'0';
}

} // namespace

void InstallAllProcessors()
{
    DWORD_PTR own = 0;
    DWORD_PTR system = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &own, &system) == 0)
    {
        spdlog::warn("processors: the process's own could not be read (error {})", GetLastError());
        return;
    }
    if (own == system)
    {
        spdlog::info("processors: all of the machine's, {:#x}, as started", system);
    }
    else if (LeftOut())
    {
        spdlog::info("processors: {:#x} of the machine's {:#x}, left as started (DAOLIB_ALL_PROCESSORS=0)", own,
                     system);
    }
    else if (SetProcessAffinityMask(GetCurrentProcess(), system) != 0)
    {
        spdlog::info("processors: all of the machine's, {:#x}, where the game was started on {:#x}", system, own);
    }
    else
    {
        spdlog::warn("processors: could not go from {:#x} to the machine's {:#x} (error {})", own, system,
                     GetLastError());
    }
}

} // namespace perf
