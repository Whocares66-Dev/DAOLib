// Calling a plugin's code from inside the game's. The library's hooks run
// on the game's own stack, and an exception has no safe way back through
// it: the game's frames around a hook were not written to meet one, so
// it ends the process, or unwinds the game half-way through a change
// and leaves its state wrong. So every callback the library calls goes
// through Guard: an exception is logged, where it happened with it, and
// the game goes on as it would have without that callback. Frame, event,
// message, GUI, save and command callbacks all do.
//
// A plugin's own Detours hooks are its own to guard the same way
// (dev/PLUGINS.md): the library never sees those calls. Nor does this
// catch a crash (an access violation is not a C++ exception); the
// loader's crash logger has those.
#pragma once

#include <spdlog/spdlog.h>

#include <cstdio>
#include <exception>
#include <utility>

namespace dao::detail
{

// Logs what a callback threw; `what` is null for something that is not a
// std::exception. Logging can itself fail, and nothing may leave here.
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

// Runs `callback`; true if it returned, false if it threw (and was logged).
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
