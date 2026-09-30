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
