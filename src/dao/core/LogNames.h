// Which files of the log folder are a game's own logs: the ones a report of
// that game's quotes the last lines of (DAOLib issue #57).
//
// The folder is shared. The first game running has the plain names,
// <name>.log; one started while it runs writes <name>.<process id>.log
// (dao/Log.h). Beside them lie the reports, crash-, hang- and exit-, the
// session before's <name>.previous.log, and whatever earlier games left.
#pragma once

#include <cstdint>
#include <string_view>

namespace dao::core
{

// Whether a file of the log folder is a log the process `pid` writes:
// <name>.log if that process has the plain names (`ownsLogs`), and
// <name>.<pid>.log if it has not. Never a report or a session before's.
// Whether the file was written in this run is for the caller to ask.
bool IsOwnLog(std::string_view file, bool ownsLogs, std::uint32_t pid);

} // namespace dao::core
