// Gives each DLL its own log file, under <Documents>\BioWare\Dragon Age\daolib. Call OpenLog once
// when the plugin loads, then log through spdlog as usual. Set the level with the
// DAOLIB_LOG_LEVEL environment variable (trace, debug, info, warn, error or off); it is info if
// the variable is unset or not one of these.
#pragma once

#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <string_view>

namespace dao
{

// Returns the folder logs are written to: <Documents>\BioWare\Dragon Age\daolib, or the same
// path under the temporary folder if the Documents folder cannot be found. This function does
// not create the folder.
std::filesystem::path LogDirectory();

// The longest a log line waits in memory before it is written to its file. Wait this long before
// reading another DLL's log for its latest lines.
inline constexpr std::chrono::milliseconds kLogLatency{250};

// Opens <name>.log in LogDirectory(), replacing the previous run's file, and makes it this DLL's
// default spdlog logger. The first line says which log level is in effect.
// @param name the start of the file name, usually the plugin's name
//
// Call it once per DLL; later calls do nothing. Lines are written by a background thread, so
// logging never waits for the disk: warnings and errors are written at once, other lines within
// kLogLatency. If a second copy of the game is started while one is running, it writes
// <name>.<process id>.log instead.
void OpenLog(std::string_view name);

} // namespace dao
