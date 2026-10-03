// The crash logger: on a crash anywhere in the game's process, a log of it
// beside the other logs, crash-<date>-<time>.log, in the shape of Skyrim's
// CrashLoggerSSE: the exception and the instruction, a probable call stack,
// the registers and the stack with every value annotated (an engine object by
// its RTTI class, code by its function), the modules, the plugins, and the
// last lines of every DAOLib log. dev/CRASHES.md says how to read one. And
// the hang logger, for a game that stops making frames without crashing.
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

// The hang watchdog: when no frame has come for DAOLIB_HANG_SECONDS (5; 0 for
// none), the frame thread is suspended for a moment, its registers and stack
// copied, and hang-<date>-<time>.log written from the copy with what a crash
// log has, again every as many seconds while it lasts (twelve at most), and
// how long it lasted. Not while loading, at the main menu's start-up, or
// with the window minimised or gone. From the loader's way
// into the game, before the plugins load: it counts frames with
// dao::OnFrame.
void StartWatchdog();

} // namespace loader::crashlog
