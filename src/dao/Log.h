// One log file per DLL, under the folder the game itself writes to:
// <Documents>\BioWare\Dragon Age\daolib\<name>.log. Each DLL links spdlog
// statically and so has a default logger of its own; OpenLog points it at
// that file. The level comes from the DAOLIB_LOG_LEVEL environment variable
// (trace, debug, info, warn, error, off), info when unset, and the file's
// first line names the level it took, so a log says what it can show.
//
// A line costs its caller no disk write: it is appended to a buffer that a
// writer thread of the log's own writes out every quarter of a second, and
// at once for a warning or an error (DAOLib #31), so logging from the
// game's frame thread does not stall it. A line can reach the file up to a
// quarter of a second late; the crash logger's own log is written apart.
#pragma once

#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <string_view>

namespace dao
{

std::filesystem::path LogDirectory();

// The longest a line waits in memory before its writer writes it out: what
// a reader of the log files (the crash logger copying their last lines)
// waits for first.
inline constexpr std::chrono::milliseconds kLogLatency{250};

// Opens <name>.log, truncating the last run's, and makes it the default
// logger. A game started while another runs with DAOLib writes
// <name>.<process id>.log instead, and leaves the running game's logs be.
// Safe to call once per DLL; a second call is ignored.
void OpenLog(std::string_view name);

} // namespace dao
