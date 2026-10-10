// Reading the game's memory from a thread that does not own it: the probe's
// sampler reads engine objects while the game runs on, and a pointer read
// a moment ago can be freed by the time it is followed. These reads catch
// the access violation (SEH) and report failure instead of crashing the
// game. They do nothing about a torn value, only about a fault.
#include "dao/SafeRead.h"

#include <Windows.h>

#include <cstring>

namespace dao
{

bool TryCopy(void *out, std::uintptr_t address, std::size_t size) noexcept
{
    // Structured exception handling: an access violation inside the copy
    // lands in the handler instead of ending the process. No object with a
    // destructor may live in this function, which is why it is this small.
    __try
    {
        std::memcpy(out, reinterpret_cast<const void *>(address), size);
        return true;
    }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return false;
    }
}

} // namespace dao
