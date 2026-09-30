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
        const auto file = directory / (std::string(name) + ".log");
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
