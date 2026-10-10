// Runs a plugin's callback so that an exception it throws is logged instead of unwinding
// through the game's code. The library calls every plugin callback (frame, event, message,
// GUI, save and command callbacks) through Guard.
#pragma once

#include <spdlog/spdlog.h>

#include <cstdio>
#include <exception>
#include <utility>

// Notes on Guard: the library's hooks run on the game's own stack, and an exception
// has no safe way back through it. The game's frames around a hook were not written
// to meet one, so it ends the process, or unwinds the game half-way through a change
// and leaves its state wrong. With Guard, the game goes on as it would have without
// that callback. A plugin's own Detours hooks are its own to guard the same way
// (dev/PLUGINS.md): the library never sees those calls. Nor does this catch a crash
// (an access violation is not a C++ exception); the loader's crash logger has those.

namespace dao::detail
{

// Logs that a callback threw. If logging fails as well, writes a line to stderr instead.
// @param where the callback's name, shown in the log
// @param what the exception's message, or null if what was thrown is not a
// std::exception
inline void ReportThrown(const char *where, const char *what) noexcept
{
    try
    {
        if (what != nullptr)
        {
            spdlog::error("{}: a plugin's callback threw, and the game went on without it: {}", where, what);
        }
        else
        {
            spdlog::error("{}: a plugin's callback threw something that is not a std::exception, and the game went "
                          "on without it",
                          where);
        }
    }
    catch (...)
    {
        // The log failed too: the last place left, where a debugger sees it.
        std::fputs("DAOLib: a plugin's callback threw, and logging it failed\n", stderr);
    }
}

// Runs `callback`, logging any exception it throws instead of letting it propagate.
// @param where the callback's name, shown in the log
// @return true if the callback returned normally, false if it threw
template <class F> bool Guard(const char *where, F &&callback) noexcept
{
    try
    {
        std::forward<F>(callback)();
        return true;
    }
    catch (const std::exception &error)
    {
        ReportThrown(where, error.what());
    }
    catch (...)
    {
        ReportThrown(where, nullptr);
    }
    return false;
}

} // namespace dao::detail
