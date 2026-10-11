#include "fix/EngineHeap.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace fix
{

namespace
{

constexpr std::uint32_t kLive = 0xDA0A110C;      // the mark of a block of ours in use
constexpr std::uint32_t kFreed = 0xDA0AF4EE;     // and of one freed
constexpr std::int32_t kDefaultShift = 4;        // the Main pool's alignment: 16 bytes
constexpr std::int32_t kLargestShift = 16;       // an alignment past 64 KiB is not an alignment
constexpr std::uint32_t kFirstOddOnesLogged = 8; // of each kind
constexpr std::uint64_t kReportEvery = 1U << 23; // allocations between two lines in the log

// Before each block, ending where the block begins.
struct Header
{
    std::uint32_t mark = 0;
    std::uint32_t size = 0; // as asked for
    void *raw = nullptr;    // what the heap gave
    std::int32_t shift = 0; // the alignment it was given
};
static_assert(sizeof(Header) == 16);

HANDLE g_heap = nullptr;
std::atomic<std::uint64_t> g_allocations{0};
std::atomic<std::uint64_t> g_failed{0};
std::atomic<std::uint64_t> g_liveBytes{0};
std::atomic<std::uint64_t> g_liveBlocks{0};
std::atomic<std::uint32_t> g_notOurs{0};
std::atomic<std::uint32_t> g_freedTwice{0};

Header *HeaderOf(void *block)
{
    return reinterpret_cast<Header *>(static_cast<std::byte *>(block) - sizeof(Header));
}

// Now and then, what the heap holds: the one sign in loader.log of how it fares over a session.
void Report()
{
    constexpr double kMiB = 1048576.0;
    spdlog::info("engine heap: {} allocations so far; {} blocks live, {:.1f} MiB asked for in them; {} failed; {} "
                 "frees of what is not its block, {} of what was freed",
                 g_allocations.load(), g_liveBlocks.load(), static_cast<double>(g_liveBytes.load()) / kMiB,
                 g_failed.load(), g_notOurs.load(), g_freedTwice.load());
}

void *Take(std::uint32_t size, std::int32_t shift)
{
    if (size == 0)
    {
        return nullptr;
    }
    shift = std::clamp(shift < 0 ? kDefaultShift : shift, kDefaultShift, kLargestShift);
    const std::uintptr_t align = std::uintptr_t{1} << shift;
    void *raw = HeapAlloc(g_heap, 0, static_cast<SIZE_T>(size) + sizeof(Header) + align - 1);
    if (raw == nullptr)
    {
        g_failed.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    const std::uintptr_t block = (reinterpret_cast<std::uintptr_t>(raw) + sizeof(Header) + align - 1) & ~(align - 1);
    auto *made = reinterpret_cast<void *>(block);
    *HeaderOf(made) = Header{kLive, size, raw, shift};
    g_liveBlocks.fetch_add(1, std::memory_order_relaxed);
    g_liveBytes.fetch_add(size, std::memory_order_relaxed);
    if ((g_allocations.fetch_add(1, std::memory_order_relaxed) + 1) % kReportEvery == 0)
    {
        Report();
    }
    return made;
}

// False for a pointer that is not a block of ours in use: the engine's own free and
// reallocation leave such a pointer alone and say nothing (0x004B90A2, 0x004B93B3, and the
// used flag in 0x004B9D90). The mark is read so that a pointer into memory that is not there
// answers nothing instead of faulting.
bool Ours(void *block, const char *what)
{
    const std::uint32_t mark =
        dao::TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(HeaderOf(block))).value_or(0);
    if (mark == kLive)
    {
        return true;
    }
    std::atomic<std::uint32_t> &count = mark == kFreed ? g_freedTwice : g_notOurs;
    if (count.fetch_add(1, std::memory_order_relaxed) < kFirstOddOnesLogged)
    {
        spdlog::warn("engine heap: {} of {}, which is {}; left alone, as the engine's own did", what, block,
                     mark == kFreed ? "a block already freed" : "no block of this heap's");
    }
    return false;
}

void Give(void *block)
{
    Header *header = HeaderOf(block);
    header->mark = kFreed;
    g_liveBlocks.fetch_sub(1, std::memory_order_relaxed);
    g_liveBytes.fetch_sub(header->size, std::memory_order_relaxed);
    HeapFree(g_heap, 0, header->raw);
}

// ECMemory::Allocate: __cdecl (size, alignment's shift or -1 for the pool's own); null for a
// size of 0.
using AllocateFn = void *(__cdecl *)(std::uint32_t size, std::int32_t alignShift);
dao::Detour<AllocateFn> g_allocate;

void *__cdecl Allocate(std::uint32_t size, std::int32_t alignShift)
{
    return Take(size, alignShift);
}

// ECMemory::Free: __cdecl (pointer).
using FreeFn = void(__cdecl *)(void *block);
dao::Detour<FreeFn> g_free;

void __cdecl Free(void *block)
{
    if (block != nullptr && Ours(block, "a free"))
    {
        Give(block);
    }
}

// ECMemory::Reallocate: __cdecl (pointer, size). As the engine's: null for a pointer no pool
// owns, the pointer itself for the size it has (0x004B9370), null with nothing freed for a
// size of nothing (the pool's slot 5).
using ReallocateFn = void *(__cdecl *)(void *block, std::uint32_t size);
dao::Detour<ReallocateFn> g_reallocate;

void *__cdecl Reallocate(void *block, std::uint32_t size)
{
    if (block == nullptr)
    {
        return Take(size, -1);
    }
    if (!Ours(block, "a reallocation") || size == 0)
    {
        return nullptr;
    }
    const Header was = *HeaderOf(block);
    if (was.size == size)
    {
        return block;
    }
    void *made = Take(size, was.shift);
    if (made == nullptr)
    {
        return nullptr;
    }
    std::memcpy(made, block, std::min(was.size, size));
    Give(block);
    return made;
}

// ECMemory::FreeSubPool: __cdecl (code). It locks through the manager, which is not there
// now, and frees a sub-pool nothing in this build makes; the codes its callers hold are -1.
using FreeSubPoolFn = std::uintptr_t(__cdecl *)(std::int32_t code);
dao::Detour<FreeSubPoolFn> g_freeSubPool;

std::uintptr_t __cdecl FreeSubPool(std::int32_t code)
{
    if (code != -1)
    {
        spdlog::warn("engine heap: the sub-pool of code {} was to be freed whole; there is none", code);
    }
    return 0;
}

// ECMemory::Statistics: __cdecl (out, out, pool index or -1 for all), answering whether
// there was such a pool. Its one caller asks for a pool that was never there.
using StatisticsFn = bool(__cdecl *)(std::uint32_t *used, std::uint32_t *size, std::int32_t pool);
dao::Detour<StatisticsFn> g_statistics;

bool __cdecl Statistics(std::uint32_t *used, std::uint32_t *size, std::int32_t /*pool*/)
{
    *used = 0;
    *size = 0;
    return false;
}

bool TurnedOff()
{
    std::array<char, 4> value{};
    const DWORD length = GetEnvironmentVariableA("DAOLIB_ENGINE_HEAP", value.data(), static_cast<DWORD>(value.size()));
    return length == 1 && value[0] == '0';
}

} // namespace

void InstallEngineHeapFix()
{
    if (TurnedOff())
    {
        spdlog::info("fixes: DAOLIB_ENGINE_HEAP=0, so the engine's memory is its own pools (issue #69)");
        return;
    }
    // Before the first allocation or not at all: a block of the engine's own freed here, or
    // one of these freed there, is the mix this must never be.
    const auto manager = dao::TryRead<std::uintptr_t>(dao::Module::Address(dao::addr::kEngineMemoryManager));
    if (!manager || *manager != 0)
    {
        spdlog::warn("fixes: the engine's memory manager is already there, so its memory stays its own pools "
                     "(issue #69)");
        return;
    }
    g_heap = HeapCreate(0, 0, 0);
    // All of them or none: the two calls that reach through the manager first, the allocation last.
    if (g_heap == nullptr || !g_statistics.Attach(dao::addr::kEngineMemory_Statistics, &Statistics) ||
        !g_freeSubPool.Attach(dao::addr::kEngineMemory_FreeSubPool, &FreeSubPool) ||
        !g_reallocate.Attach(dao::addr::kEngineMemory_Reallocate, &Reallocate) ||
        !g_free.Attach(dao::addr::kEngineMemory_Free, &Free) ||
        !g_allocate.Attach(dao::addr::kEngineMemory_Allocate, &Allocate))
    {
        g_allocate.Detach();
        g_free.Detach();
        g_reallocate.Detach();
        g_freeSubPool.Detach();
        g_statistics.Detach();
        spdlog::warn("fixes: the engine's allocator could not be detoured, so its memory stays its own pools "
                     "(issue #69)");
        return;
    }
    spdlog::info("fixes: the engine's memory is the system's heap, not its own 850 MiB of pools (issue #69)");
}

} // namespace fix
