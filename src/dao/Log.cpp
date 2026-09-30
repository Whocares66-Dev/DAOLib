#include "dao/Log.h"

#include <Windows.h>

#include <ShlObj.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <cstdlib>
#include <memory>
#include <string>

namespace dao
{

std::filesystem::path LogDirectory()
{
    PWSTR documents = nullptr;
    std::filesystem::path root;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)))
    {
        root = documents;
    }
    CoTaskMemFree(documents);
    if (root.empty())
    {
        root = std::filesystem::temp_directory_path();
    }
    return root / L"BioWare" / L"Dragon Age" / L"daolib";
}

namespace
{

spdlog::level::level_enum LevelFromEnvironment()
{
    std::string value;
    // getenv_s: the checked form MSVC wants, sized in two calls.
    std::size_t needed = 0;
    if (getenv_s(&needed, nullptr, 0, "DAOLIB_LOG_LEVEL") != 0 || needed == 0)
    {
        return spdlog::level::info;
    }
    value.resize(needed);
    if (getenv_s(&needed, value.data(), value.size(), "DAOLIB_LOG_LEVEL") != 0)
    {
        return spdlog::level::info;
    }
    value.resize(needed - 1); // the terminator getenv_s counted
    const auto level = spdlog::level::from_str(value);
    // from_str answers "off" for a word it does not know, which would hide
    // a typo along with every message; a typo reads as the default instead.
    if (level == spdlog::level::off && value != "off")
    {
        return spdlog::level::info;
    }
    return level;
}

// Whether this game writes the plain <name>.log. The first game running
// with DAOLib does; one started while it runs writes <name>.<pid>.log, so
// as not to truncate the running game's logs (a second DAOrigins.exe did,
// in a playtest, 2026-09-30). The owner's process id sits in a small named
// block of shared memory: every DLL of the owning game finds its own id
// there, and a game that finds another's takes over only if that one has
// exited. Each DLL has the library's statics of its own, so each asks.
bool OwnsLogs()
{
    HANDLE mapping =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(LONG), L"Local\\DAOLib.LogOwner");
    if (mapping == nullptr)
    {
        return true;
    }
    auto *owner = static_cast<LONG volatile *>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LONG)));
    if (owner == nullptr)
    {
        CloseHandle(mapping);
        return true;
    }
    // The mapping and its view stay open for the game's life: the block
    // lasts as long as a game holds it.
    const auto self = static_cast<LONG>(GetCurrentProcessId());
    for (;;)
    {
        const LONG current = InterlockedCompareExchange(owner, self, 0);
        if (current == 0 || current == self)
        {
            return true;
        }
        DWORD code = 0;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(current));
        const bool alive = process != nullptr && GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
        if (process != nullptr)
        {
            CloseHandle(process);
        }
        if (alive)
        {
            return false;
        }
        if (InterlockedCompareExchange(owner, self, current) == current)
        {
            return true;
        }
    }
}

} // namespace

void OpenLog(std::string_view name)
{
    static bool opened = false;
    if (opened)
    {
        return;
    }
    opened = true;
    try
    {
        const auto directory = LogDirectory();
        std::filesystem::create_directories(directory);
        const std::string stem =
            OwnsLogs() ? std::string(name) : std::string(name) + "." + std::to_string(GetCurrentProcessId());
        const auto file = directory / (stem + ".log");
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file.string(), true);
        auto logger = std::make_shared<spdlog::logger>(std::string(name), std::move(sink));
        const auto level = LevelFromEnvironment();
        logger->set_level(level);
        logger->flush_on(spdlog::level::trace);
        logger->set_pattern("[%H:%M:%S.%e] [%t] [%l] %v");
        spdlog::set_default_logger(std::move(logger));
        spdlog::info("{} {} (daolib {}), level {}", name, DAO_VERSION_STRING, DAO_VERSION_STRING,
                     spdlog::level::to_string_view(level));
    }
    catch (const std::exception &e)
    {
        // No folder to write to is not a reason to stop the game; the
        // default logger stays spdlog's, which goes nowhere in a DLL, and
        // the reason goes where a debugger attached to the game can see it.
        OutputDebugStringA((std::string("daolib: could not open the log: ") + e.what()).c_str());
    }
}

} // namespace dao
