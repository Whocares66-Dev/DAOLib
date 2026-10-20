// Moved from src/fix/ArchiveRead.cpp (DAOLib #210), where it fixed the game's
// reader, which seeked and read a shared handle and lost models (#3). Core's
// archives read the same way, with no lock.
#include "dao/core/ReadAt.h"

namespace dao::core
{

std::uint32_t ReadAt(HANDLE handle, void *buffer, std::uint32_t bytes, std::uint64_t offset)
{
    OVERLAPPED overlapped{};
    overlapped.Offset = static_cast<DWORD>(offset);
    overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
    DWORD read = 0;
    if (ReadFile(handle, buffer, bytes, &read, &overlapped) == FALSE)
    {
        // Pending only on a handle opened for overlapped reads, which the
        // archives' are not: waited for, as ReadFile with no OVERLAPPED would.
        if (GetLastError() != ERROR_IO_PENDING || GetOverlappedResult(handle, &overlapped, &read, TRUE) == FALSE)
        {
            return 0;
        }
    }
    return read;
}

} // namespace dao::core
