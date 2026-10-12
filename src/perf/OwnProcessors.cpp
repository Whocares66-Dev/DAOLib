#include "perf/OwnProcessors.h"

#include "dao/Log.h"
#include "dao/core/Processors.h"

#include <Windows.h>

#include <TlHelp32.h>

#include <array>
#include <bit>
#include <cstdint>
#include <cwchar>
#include <vector>

namespace perf
{

namespace
{

// The other copies' processors: every other process of the game's name
// whose affinity can be read. One that cannot be (another user's) is left
// out, and may be shared with.
std::vector<std::uint64_t> OthersProcessors()
{
    std::vector<std::uint64_t> others;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return others;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof entry;
    for (BOOL more = Process32FirstW(snapshot, &entry); more != 0; more = Process32NextW(snapshot, &entry))
    {
        if (entry.th32ProcessID == GetCurrentProcessId() || _wcsicmp(entry.szExeFile, L"DAOrigins.exe") != 0)
        {
            continue;
        }
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (process == nullptr)
        {
            continue;
        }
        DWORD_PTR theirs = 0;
        DWORD_PTR system = 0;
        if (GetProcessAffinityMask(process, &theirs, &system) != 0)
        {
            others.push_back(theirs);
        }
        CloseHandle(process);
    }
    CloseHandle(snapshot);
    return others;
}

bool LeftOut()
{
    std::array<wchar_t, 8> value{};
    const DWORD length = GetEnvironmentVariableW(L"DAOLIB_OWN_PROCESSORS", value.data(), 8);
    return length == 1 && value[0] == L'0';
}

} // namespace

void InstallOwnProcessors()
{
    DWORD_PTR own = 0;
    DWORD_PTR system = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &own, &system) == 0)
    {
        spdlog::warn("processors: the process's own could not be read (error {})", GetLastError());
        return;
    }
    if (LeftOut())
    {
        spdlog::info("processors: {:#x} of the machine's {:#x}, left as started (DAOLIB_OWN_PROCESSORS=0)", own,
                     system);
        return;
    }
    // Two copies started together must not both find the same group free:
    // one looks and moves at a time.
    HANDLE turn = CreateMutexW(nullptr, FALSE, L"Local\\DAOLibOwnProcessors");
    if (turn != nullptr)
    {
        WaitForSingleObject(turn, 5000);
    }
    const std::vector<std::uint64_t> others = OthersProcessors();
    const std::uint64_t chosen = dao::core::ChooseProcessors(system, own, others);
    if (chosen == own)
    {
        spdlog::info("processors: {:#x} of the machine's {:#x}, as started; {} other cop{} of the game", own, system,
                     others.size(), others.size() == 1 ? "y" : "ies");
    }
    else if (SetProcessAffinityMask(GetCurrentProcess(), static_cast<DWORD_PTR>(chosen)) != 0)
    {
        spdlog::info("processors: moved from {:#x} to {:#x} of the machine's {:#x}; {} other cop{} of the game, on "
                     "the first of them at least one",
                     own, chosen, system, others.size(), others.size() == 1 ? "y" : "ies");
    }
    else
    {
        spdlog::warn("processors: could not move from {:#x} to {:#x} (error {})", own, chosen, GetLastError());
    }
    if (turn != nullptr)
    {
        ReleaseMutex(turn);
        CloseHandle(turn);
    }
}

} // namespace perf
