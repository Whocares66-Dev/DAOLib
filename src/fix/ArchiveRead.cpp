#include "fix/ArchiveRead.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/SafeRead.h"

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>

namespace fix
{

namespace
{

// ReadRequestTask::Run reads a resource's bytes from its archive's file:
// FileAccess::Seek to the resource's offset, then FileAccess::Read, a
// ReadFile at the file's position. An ErfSource gives every task the one
// file it opened (its +0xB8), and Run holds only its own load's lock; a load
// asked for now runs on the asking thread under the source's lock, a queued
// one on a worker under none of it. So two loads of one archive share the
// file's position: one seeks between the other's seek and its read, and
// the other reads its size from the first's offset. The first's bytes begin
// "GFF V4.0" with the first's data offset at +0x18; past the second's size,
// the parse zeroes its view and reads through it (0x004D53B4), and the
// first, reading on from there, gets bytes that are not a GFF and quietly
// loses its model. Read 2026-10-03 with the address library's session,
// issue #3: two first reads of neighbouring models of one archive, on two
// threads in one millisecond, one of them not a GFF.
//
// Inside Run, the seek now only says where, and the read is at that offset,
// a ReadFile with an OVERLAPPED that carries it: on these handles, opened
// without FILE_FLAG_OVERLAPPED (0x004C9170), that is a read at a position,
// done when it returns, sharing nothing. It leaves the file's position where
// the engine's own read would. The counters the engine's read keeps
// (0x00C2BE50) are not kept for these.

thread_local int t_inRun = 0;
struct PendingSeek
{
    const std::byte *file = nullptr;
    std::uint64_t offset = 0;
};
thread_local PendingSeek t_seek;

// The files a load has sought and not yet read, by which thread: two at
// once on one file is the race, which the fix makes harmless; counted.
std::mutex g_seekingMutex;
std::map<const std::byte *, DWORD> g_seeking;
std::atomic<std::uint32_t> g_races = 0;
std::atomic<bool> g_saidFirst = false;

// ECFileAccess::FileAccess: the handle at +4.
constexpr std::size_t kFileHandle = 0x04;
constexpr std::uint8_t kFromStart = 0; // FILE_BEGIN, the origin Run seeks from

// ECResMan::ReadRequestTask::Run: __thiscall (float, unread), `ret 4`; a
// float on the stack, passed through as its bits.
using RunFn = std::uint32_t(__fastcall *)(void *task, int edx, std::uintptr_t argument);
dao::Detour<RunFn> g_run;

std::uint32_t __fastcall RunReadRequest(void *task, int edx, std::uintptr_t argument)
{
    // Out, by a return or the engine's own exception: a seek with no read
    // after it (its buffer not allocated) leaves its file unmarked.
    struct Inside
    {
        Inside()
        {
            ++t_inRun;
        }
        Inside(const Inside &) = delete;
        Inside &operator=(const Inside &) = delete;
        ~Inside()
        {
            --t_inRun;
            if (t_seek.file != nullptr)
            {
                const std::lock_guard lock(g_seekingMutex);
                if (const auto found = g_seeking.find(t_seek.file);
                    found != g_seeking.end() && found->second == GetCurrentThreadId())
                {
                    g_seeking.erase(found);
                }
            }
            t_seek = {};
        }
    } const inside;
    return g_run.Original()(task, edx, argument);
}

// ECFileAccess::FileAccess::Seek: __thiscall (uint8 origin, int32 low,
// int32 high), `ret 0xC`, the new position's low half or -1.
using SeekFn = std::uint32_t(__fastcall *)(std::byte *file, int edx, std::uint32_t origin, std::uint32_t low,
                                           std::uint32_t high);
dao::Detour<SeekFn> g_seek;

std::uint32_t __fastcall SeekFile(std::byte *file, int edx, std::uint32_t origin, std::uint32_t low, std::uint32_t high)
{
    if (t_inRun == 0 || static_cast<std::uint8_t>(origin) != kFromStart || low == 0xFFFFFFFF)
    {
        return g_seek.Original()(file, edx, origin, low, high);
    }
    t_seek = {file, (static_cast<std::uint64_t>(high) << 32) | low};
    const DWORD me = GetCurrentThreadId();
    DWORD other = 0;
    {
        const std::lock_guard lock(g_seekingMutex);
        if (const auto found = g_seeking.find(file); found != g_seeking.end() && found->second != me)
        {
            other = found->second;
        }
        g_seeking[file] = me;
    }
    if (other != 0)
    {
        // Where the engine's own read would have read another's bytes.
        const std::uint32_t races = ++g_races;
        constexpr std::uint32_t kSaidEach = 10;
        constexpr std::uint32_t kThenEvery = 100;
        if (races <= kSaidEach || races % kThenEvery == 0)
        {
            spdlog::warn("file {}: thread {} seeks it while thread {} is between its seek and its read; each reads "
                         "at its own offset ({} so far, issue #3)",
                         static_cast<const void *>(file), me, other, races);
        }
    }
    return low;
}

// ECFileAccess::FileAccess::Read: __thiscall (void *buffer, uint32 size,
// uint32 count), `ret 0xC`, the bytes read.
using ReadFileFn = std::uint32_t(__fastcall *)(std::byte *file, int edx, void *buffer, std::uint32_t size,
                                               std::uint32_t count);
dao::Detour<ReadFileFn> g_readFile;

// A read at an offset: on a handle not opened for overlapped reads, done
// when ReadFile returns.
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

std::uint32_t __fastcall ReadFromFile(std::byte *file, int edx, void *buffer, std::uint32_t size, std::uint32_t count)
{
    if (t_inRun == 0 || t_seek.file != file)
    {
        return g_readFile.Original()(file, edx, buffer, size, count);
    }
    const auto handle = dao::TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(file) + kFileHandle);
    if (!handle || reinterpret_cast<HANDLE>(*handle) == INVALID_HANDLE_VALUE)
    {
        return g_readFile.Original()(file, edx, buffer, size, count);
    }
    const std::uint32_t read = ReadAt(reinterpret_cast<HANDLE>(*handle), buffer, size * count, t_seek.offset);
    if (!g_saidFirst.exchange(true))
    {
        // Once a session: the fix is in the path the engine takes.
        spdlog::info("fixes: the first archive read at its own offset, {} bytes at {} of file {}, thread {}", read,
                     t_seek.offset, static_cast<const void *>(file), GetCurrentThreadId());
    }
    t_seek.offset += read;
    {
        const std::lock_guard lock(g_seekingMutex);
        if (const auto found = g_seeking.find(file); found != g_seeking.end() && found->second == GetCurrentThreadId())
        {
            g_seeking.erase(found);
        }
    }
    return read;
}

} // namespace

void InstallArchiveReadFix()
{
    g_run.Attach(dao::addr::kReadRequestTask_Run, &RunReadRequest);
    g_seek.Attach(dao::addr::kFileAccess_Seek, &SeekFile);
    g_readFile.Attach(dao::addr::kFileAccess_Read, &ReadFromFile);
    spdlog::info("fixes: a resource is read at its own offset, not at its archive's shared position (issue #3)");
}

} // namespace fix
