// Example: a few of the game's bytes changed in place (dao/Patch.h).
//
// Full Detail LOD (a Nexus mod, mods/7058) makes these changes in the exe file; here they are
// made in memory as the plugin loads, so the exe stays vanilla and every build is recognised.
// Each is made only where the bytes are as vanilla has them, and logged.
//
// - Full detail: the order a model's levels of detail are tried in, one row of four for each
//   level asked for, made full detail first in every row. The engine never swaps a model for a
//   lower level, so it never reloads one, which in a crowd it did dozens of times at once.
// - A sharp preview: the inventory and the character record draw their character into a
//   picture of 270 by 459 and stretch it over the frame. Both are made three times as large.
//
// What to look for in play: in the Denerim market no stutter as the crowd moves, and distant
// people keeping their full detail; in the inventory, a character drawn sharp with smooth
// edges. What it does goes to example_patch.log.
#include "dao/Patch.h"
#include "dao/Addresses.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdint>

namespace
{

using Order = std::array<std::int32_t, 16>;
constexpr Order kVanillaOrder = {0, 1, 2, 3, 1, 0, 2, 3, 2, 1, 0, 3, 3, 2, 1, 0};
constexpr Order kFullDetail = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};

// The preview's size as its maker pushes it: `push height; push width`.
using Push = std::array<std::uint8_t, 10>;
constexpr Push PushSize(std::uint32_t width, std::uint32_t height)
{
    return {0x68,
            static_cast<std::uint8_t>(height),
            static_cast<std::uint8_t>(height >> 8),
            static_cast<std::uint8_t>(height >> 16),
            static_cast<std::uint8_t>(height >> 24),
            0x68,
            static_cast<std::uint8_t>(width),
            static_cast<std::uint8_t>(width >> 8),
            static_cast<std::uint8_t>(width >> 16),
            static_cast<std::uint8_t>(width >> 24)};
}
constexpr std::uint32_t kWidth = 270;
constexpr std::uint32_t kHeight = 459;
constexpr std::uint32_t kTimes = 3;
constexpr Push kVanillaSize = PushSize(kWidth, kHeight);
constexpr std::uint32_t kSharpWidth = kWidth * kTimes;
constexpr std::uint32_t kSharpHeight = kHeight * kTimes;
constexpr Push kSharpSize = PushSize(kSharpWidth, kSharpHeight);

dao::Patch g_order;
dao::Patch g_inventory;
dao::Patch g_record;

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_patch");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    // Before the game's code runs, so no thread is reading the bytes as they change.
    if (g_order.Apply({dao::addr::kModelLodOrder, 0}, kVanillaOrder, kFullDetail))
    {
        spdlog::info("models are drawn at full detail at any distance");
    }
    if (g_inventory.Apply({dao::addr::kGUIInventory_OnShow, 0x6E}, kVanillaSize, kSharpSize) &&
        g_record.Apply({dao::addr::kGUICharacterRecord_OnShow, 0x140}, kVanillaSize, kSharpSize))
    {
        spdlog::info("the inventory's and the character record's previews are drawn at {} by {}", kWidth * kTimes,
                     kHeight * kTimes);
    }
    else
    {
        g_inventory.Revert(); // both or neither
    }
    return true;
}
