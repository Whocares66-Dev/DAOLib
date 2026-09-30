// One log file per DLL, under the folder the game itself writes to:
// <Documents>\BioWare\Dragon Age\daolib\<name>.log. Each DLL links spdlog
// statically and so has a default logger of its own; OpenLog points it at
// that file. The level comes from the DAOLIB_LOG_LEVEL environment variable
// (trace, debug, info, warn, error, off), info when unset, and the file's
// first line names the level it took, so a log says what it can show.
#pragma once

#include <spdlog/spdlog.h>

#include <filesystem>
#include <string_view>

namespace dao
{

std::filesystem::path LogDirectory();

// Opens <name>.log, truncating the last run's, and makes it the default
// logger. A game started while another runs with DAOLib writes
// <name>.<process id>.log instead, and leaves the running game's logs be.
// Safe to call once per DLL; a second call is ignored.
void OpenLog(std::string_view name);

} // namespace dao
