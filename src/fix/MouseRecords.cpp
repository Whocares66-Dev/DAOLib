#include "fix/MouseRecords.h"

#include "dao/Addresses.h"
#include "dao/Log.h"
#include "dao/Patch.h"

#include <array>
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
// and each made: the displacements 0x14 lower, the same fields of the record before.
constexpr std::uint32_t kFirstAt = 0x460;
constexpr std::array<std::uint8_t, 12> kFirst = {0x8D, 0x0C, 0x80, 0x8D, 0x04, 0x8A,
                                                 0x8B, 0x48, 0x0C, 0x8B, 0x50, 0x08};
constexpr std::array<std::uint8_t, 12> kFirstMade = {0x8D, 0x0C, 0x80, 0x8D, 0x04, 0x8A,
                                                     0x8B, 0x48, 0xF8, 0x8B, 0x50, 0xF4}; // [eax-8], [eax-0xC]
constexpr std::uint32_t kSecondAt = 0x4C8;
constexpr std::array<std::uint8_t, 10> kSecond = {0x8B, 0x54, 0x81, 0x0C, 0x8D, 0x04, 0x81, 0x8B, 0x40, 0x08};
constexpr std::array<std::uint8_t, 10> kSecondMade = {0x8B, 0x54, 0x81, 0xF8, 0x8D,
                                                      0x04, 0x81, 0x8B, 0x40, 0xF4}; // [ecx+eax*4-8], [eax-0xC]

dao::Patch g_first;
dao::Patch g_second;

} // namespace

void InstallMouseRecordsFix()
{
    // No thread of the game's runs yet: this is before its own code starts.
    if (!g_first.Apply({dao::addr::kRawInputWin32_ReadMouse, kFirstAt}, kFirst, kFirstMade) ||
        !g_second.Apply({dao::addr::kRawInputWin32_ReadMouse, kSecondAt}, kSecond, kSecondMade))
    {
        g_first.Revert();
        spdlog::warn("fixes: the mouse's movement still takes its time from one past the last record read: the "
                     "code is not as it was read, or could not be written, so it is left as it is (issue #105)");
        return;
    }
    spdlog::info("fixes: the mouse's movement takes its time from the last record read, not from the one after "
                 "it (issue #105)");
}

} // namespace fix
