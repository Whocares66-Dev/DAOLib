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

// Opens <name>.log in LogDirectory() and makes it this DLL's default spdlog logger. The previous
// run's file is kept as <name>.previous.log, one generation. The first line says which log level is
// in effect.
// @param name the start of the file name, usually the plugin's name
//
// Call it once per DLL; later calls do nothing. Lines are written by a background thread, so
// logging never waits for the disk: warnings and errors are written at once, other lines within
// kLogLatency. If a second copy of the game is started while one is running, it writes
// <name>.<process id>.log instead (OwnsLogs).
void OpenLog(std::string_view name);

// Returns whether this game has the plain file names in LogDirectory(): <name>.log, and with it
// any file a plugin keeps beside the logs under a name of its own.
// @return true for the first game running in its log folder; false for one started while that
//  one runs, which OpenLog gives <name>.<process id>.log, and which should name its own files by
//  its process id too
//
// Every DLL of a game gets the same answer, at any time for as long as the game runs, before or
// after OpenLog. A named instance has a log folder of its own and is the first game in it.
bool OwnsLogs();

} // namespace dao
