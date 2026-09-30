#include "Census.h"

#include "dao/Module.h"

#include <Windows.h>

#include <detours/detours.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstring>

#ifndef _M_IX86
#error "The census's stubs are x86 machine code, the game's architecture"
#endif

namespace probe
{

namespace
{

constexpr Target kTargets[] = {
#include "ProbeTargets.inc"
};
constexpr std::size_t kTargetCount = std::size(kTargets);
constexpr std::size_t kStubSize = 16;

bool g_attached[kTargetCount] = {};

} // namespace

// What the stubs and CountAndContinue share; extern "C" and at namespace
// scope so the inline assembly can name them.
extern "C"
{
    std::uint32_t g_probeCounts[kTargetCount] = {};
    void *g_probeOriginals[kTargetCount] = {};

    __declspec(naked) void ProbeCountAndContinue()
    {
        __asm {
            lock inc dword ptr g_probeCounts[eax*4]
            jmp dword ptr g_probeOriginals[eax*4]
        }
    }
}

std::span<const Target> Targets()
{
    return kTargets;
}

CensusResult AttachCensus(std::span<const std::uint32_t> except)
{
    CensusResult result;
    auto *stubs = static_cast<std::uint8_t *>(
        VirtualAlloc(nullptr, kTargetCount * kStubSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (stubs == nullptr)
    {
        spdlog::error("census: no memory for stubs, error {}", GetLastError());
        result.failed = kTargetCount;
        return result;
    }
    const auto common = reinterpret_cast<std::uintptr_t>(&ProbeCountAndContinue);
    for (std::size_t i = 0; i < kTargetCount; ++i)
    {
        std::uint8_t *stub = stubs + i * kStubSize;
        // mov eax, imm32 ; jmp rel32 ; int3 padding
        const auto index = static_cast<std::uint32_t>(i);
        const auto rel = static_cast<std::int32_t>(common - (reinterpret_cast<std::uintptr_t>(stub) + 10));
        std::memset(stub, 0xCC, kStubSize);
        stub[0] = 0xB8;
        std::memcpy(stub + 1, &index, 4);
        stub[5] = 0xE9;
        std::memcpy(stub + 6, &rel, 4);
    }
    FlushInstructionCache(GetCurrentProcess(), stubs, kTargetCount * kStubSize);

    for (std::size_t i = 0; i < kTargetCount; ++i)
    {
        const Target &t = kTargets[i];
        if (std::ranges::find(except, t.address) != except.end())
        {
            ++result.skipped;
            continue;
        }
        g_probeOriginals[i] = reinterpret_cast<void *>(dao::Module::Address(t.address));
        // Detours abandons a whole transaction when one attach in it fails,
        // so each target gets a transaction of its own.
        if (DetourTransactionBegin() != NO_ERROR)
        {
            ++result.failed;
            continue;
        }
        DetourUpdateThread(GetCurrentThread());
        const LONG attach = DetourAttach(&g_probeOriginals[i], stubs + i * kStubSize);
        const LONG commit = attach == NO_ERROR ? DetourTransactionCommit() : DetourTransactionAbort();
        if (attach == NO_ERROR && commit == NO_ERROR)
        {
            g_attached[i] = true;
            ++result.attached;
        }
        else
        {
            ++result.failed;
            spdlog::warn("census: not attached at {:#010x} ({}): error {}", t.address, t.label,
                         attach != NO_ERROR ? attach : commit);
        }
    }
    return result;
}

std::uint32_t CallCount(std::size_t index)
{
    return index < kTargetCount ? *static_cast<volatile std::uint32_t *>(&g_probeCounts[index]) : 0;
}

bool IsAttached(std::size_t index)
{
    return index < kTargetCount && g_attached[index];
}

} // namespace probe
