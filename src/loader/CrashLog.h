// The crash logger: on a crash anywhere in the game's process, a log of it
// beside the other logs, crash-<date>-<time>.log, in the shape of Skyrim's
// CrashLoggerSSE: the exception and the instruction, a probable call stack,
// the registers and the stack with every value annotated (an engine object by
// its RTTI class, code by its function), the modules, the plugins, and the
// last lines of every DAOLib log. dev/CRASHES.md says how to read one.
#pragma once

#include <filesystem>

namespace loader::crashlog
{

// Installs the unhandled-exception filter, and a hook on
// SetUnhandledExceptionFilter that keeps it installed: a filter the game or
// its runtime sets later is remembered and called after ours, not put in
// its place. From the loader's DllMain, first. The folder is the loader's
// own: daolib\functions.tsv and the plugins' PDBs are looked for under it.
void Install(const std::filesystem::path &loaderFolder);

} // namespace loader::crashlog
