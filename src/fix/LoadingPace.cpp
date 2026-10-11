#include "fix/LoadingPace.h"

#include "dao/Addresses.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace fix
{

namespace
{

constexpr std::size_t kSleepAt = 2;             // in the task's Execute: `push ebx; push 0x42`, the 0x42
constexpr std::uint16_t kPushSleep = 0x426A;    // `push 0x42` as it stands, at the function's +1
constexpr std::uint32_t kStepRead = 0x3D872B02; // 0.066F
constexpr std::uint32_t kEngineMs = 66;
constexpr std::uint32_t kDefaultMs = 16;
constexpr std::uint32_t kLeastMs = 4;

// Writes bytes over read-only memory, the page made writable for the write.
bool Write(std::uintptr_t at, const void *bytes, std::size_t size)
{
    DWORD was = 0;
    if (VirtualProtect(reinterpret_cast<void *>(at), size, PAGE_EXECUTE_READWRITE, &was) == FALSE)
    {
        return false;
    }
    std::memcpy(reinterpret_cast<void *>(at), bytes, size);
    VirtualProtect(reinterpret_cast<void *>(at), size, was, &was);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(at), size);
    return true;
}

// The turn asked for in ms: 0 for the engine's own.
std::uint32_t MillisecondsAsked()
{
    std::array<char, 8> value{};
    const DWORD length = GetEnvironmentVariableA("DAOLIB_LOADING_PACE", value.data(), static_cast<DWORD>(value.size()));
    if (length == 0 || length >= value.size())
    {
        return kDefaultMs;
    }
    const auto asked = static_cast<std::uint32_t>(std::strtoul(value.data(), nullptr, 10));
    return asked == 0 ? 0 : std::clamp(asked, kLeastMs, kEngineMs);
}

} // namespace

void InstallLoadingPaceFix()
{
    const std::uint32_t ms = MillisecondsAsked();
    if (ms == 0)
    {
        spdlog::info("fixes: DAOLIB_LOADING_PACE=0, so the loading screen's task sleeps its 66 ms a turn as ever "
                     "(issue #71)");
        return;
    }
    const std::uintptr_t execute = dao::Module::Address(dao::addr::kLoadingGUITask_Execute);
    const std::uintptr_t step = dao::Module::Address(dao::addr::kLoadingGUITask_Step);
    const auto sleepNow = execute != 0 ? dao::TryRead<std::uint16_t>(execute + 1) : std::nullopt;
    const auto stepNow = step != 0 ? dao::TryRead<std::uint32_t>(step) : std::nullopt;
    if (!sleepNow || *sleepNow != kPushSleep || !stepNow || *stepNow != kStepRead)
    {
        spdlog::warn("fixes: the loading screen's task is not as read (its sleep {:#x}, its step {:#x}), so it "
                     "sleeps as ever (issue #71)",
                     sleepNow.value_or(0), stepNow.value_or(0));
        return;
    }
    const auto sleep = static_cast<std::uint8_t>(ms);
    const float seconds = static_cast<float>(ms) / 1000.0F;
    if (!Write(execute + kSleepAt, &sleep, sizeof sleep) || !Write(step, &seconds, sizeof seconds))
    {
        spdlog::warn("fixes: the loading screen's task could not be written, so it sleeps as ever (issue #71)");
        return;
    }
    spdlog::info("fixes: the loading screen's task takes a turn every {} ms, not every 66, so a loading's steps do "
                 "not wait on it (issue #71)",
                 ms);
}

} // namespace fix
