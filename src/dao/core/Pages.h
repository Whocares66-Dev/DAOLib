// Whether memory can be read without a fault, from what Windows' VirtualQuery
// says of its page: for the crash and hang logs (src/loader/CrashLog.cpp),
// which follow every stack word that looks like a pointer.
//
// Written 2026-10-03 (DAOLib issue #9): a crash log ended mid-way, the
// likeliest cause a read of a thread's stack guard page. That raises
// STATUS_GUARD_PAGE_VIOLATION, not an access violation, so dao::TryCopy did
// not catch it, and it ended the process. A page is asked about first
// instead; the values are Windows' (winnt.h), passed in so this stays apart
// from <Windows.h>.
#pragma once

#include <cstdint>

namespace dao::core
{

inline constexpr std::uint32_t kMemCommit = 0x1000;
inline constexpr std::uint32_t kPageNoAccess = 0x01;
inline constexpr std::uint32_t kPageReadOnly = 0x02;
inline constexpr std::uint32_t kPageReadWrite = 0x04;
inline constexpr std::uint32_t kPageWriteCopy = 0x08;
inline constexpr std::uint32_t kPageExecute = 0x10;
inline constexpr std::uint32_t kPageExecuteRead = 0x20;
inline constexpr std::uint32_t kPageExecuteReadWrite = 0x40;
inline constexpr std::uint32_t kPageExecuteWriteCopy = 0x80;
inline constexpr std::uint32_t kPageGuard = 0x100;

// A page that is committed, readable and not a guard page. Execute-only and
// no-access pages fault; a guard page raises its own exception once.
constexpr bool IsReadablePage(std::uint32_t state, std::uint32_t protect)
{
    constexpr std::uint32_t kReadable = kPageReadOnly | kPageReadWrite | kPageWriteCopy | kPageExecuteRead |
                                        kPageExecuteReadWrite | kPageExecuteWriteCopy;
    return state == kMemCommit && (protect & kPageGuard) == 0 && (protect & kPageNoAccess) == 0 &&
           (protect & kReadable) != 0;
}

} // namespace dao::core
