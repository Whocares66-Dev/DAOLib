#include "fix/MouseRecords.h"

#include "dao/Addresses.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace fix
{

namespace
{

// The two places in ReadMouse, from its start, as the engine has them
// (dao/Addresses.h, "The mouse's records"): each ends in two reads of a
// record's fields, [eax+0xC] and [eax+8], with eax the record at the count.
//   +0x460  lea ecx,[eax+eax*4]; lea eax,[edx+ecx*4]; mov ecx,[eax+0xC]; mov edx,[eax+8]
//   +0x4C8  mov edx,[ecx+eax*4+0xC]; lea eax,[ecx+eax*4]; mov eax,[eax+8]
constexpr std::size_t kFirstAt = 0x460;
constexpr std::array<std::uint8_t, 12> kFirst = {0x8D, 0x0C, 0x80, 0x8D, 0x04, 0x8A,
                                                 0x8B, 0x48, 0x0C, 0x8B, 0x50, 0x08};
constexpr std::size_t kSecondAt = 0x4C8;
constexpr std::array<std::uint8_t, 10> kSecond = {0x8B, 0x54, 0x81, 0x0C, 0x8D, 0x04, 0x81, 0x8B, 0x40, 0x08};
constexpr std::size_t kEnd = kSecondAt + kSecond.size();

// A read's displacement: where its byte is from the function's start, and
// the byte for the same field of the record before, 0x14 lower.
struct Displacement
{
    std::size_t at = 0;
    std::uint8_t made = 0;
};
constexpr std::array<Displacement, 4> kDisplacements = {{
    {kFirstAt + 8, 0xF8},  // [eax+0xC] -> [eax-8]
    {kFirstAt + 11, 0xF4}, // [eax+8]   -> [eax-0xC]
    {kSecondAt + 3, 0xF8}, // [ecx+eax*4+0xC] -> [ecx+eax*4-8]
    {kSecondAt + 9, 0xF4}, // [eax+8]   -> [eax-0xC]
}};

template <std::size_t N> bool Holds(std::uintptr_t at, const std::array<std::uint8_t, N> &bytes)
{
    std::array<std::uint8_t, N> there{};
    return dao::TryCopy(there.data(), at, there.size()) && there == bytes;
}

} // namespace

void InstallMouseRecordsFix()
{
    const std::uintptr_t start = dao::Module::Address(dao::addr::kRawInputWin32_ReadMouse);
    if (start == 0 || !Holds(start + kFirstAt, kFirst) || !Holds(start + kSecondAt, kSecond))
    {
        spdlog::warn("fixes: the mouse's movement still takes its time from one past the last record read: the "
                     "code is not as it was read, so it is left as it is (issue #105)");
        return;
    }
    auto *const code = reinterpret_cast<std::uint8_t *>(start);
    DWORD was = 0;
    if (VirtualProtect(code + kFirstAt, kEnd - kFirstAt, PAGE_EXECUTE_READWRITE, &was) == FALSE)
    {
        spdlog::warn("fixes: the mouse's movement still takes its time from one past the last record read: the "
                     "code could not be written (issue #105)");
        return;
    }
    // No thread of the game's runs yet: this is before its own code starts.
    for (const Displacement &displacement : kDisplacements)
    {
        code[displacement.at] = displacement.made;
    }
    VirtualProtect(code + kFirstAt, kEnd - kFirstAt, was, &was);
    FlushInstructionCache(GetCurrentProcess(), code + kFirstAt, kEnd - kFirstAt);
    spdlog::info("fixes: the mouse's movement takes its time from the last record read, not from the one after "
                 "it (issue #105)");
}

} // namespace fix
