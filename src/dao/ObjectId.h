// An engine object's id: what a script's `object` is, and what the
// application's lookups take (dao/ClientExoApp.h). A 32-bit number, as in
// Neverwinter Nights.
//
// OBJECT_INVALID is 0xFFFFFFFF (-1), not NWN's 0x7F000000, read
// 2026-09-30. The VM passes an object through as it is (StackPopObject,
// 0x00A2F6D0, translates nothing), and the engine means "none" by -1 both
// ways: GetArea pushes -1 for no area (0x00655000), explore mode keeps -1
// for no object under the cursor, and RemoveEffectsByParameters (0x00673940)
// filters by creator only when it is not -1. The game's own scripts call
// that one 38 times with the creator left at its default, OBJECT_INVALID,
// to mean any creator, so their OBJECT_INVALID is -1. 0x7F000000 had been
// taken from NWN, and the generated calls passed it as their default: to
// the engine a real, nonexistent object, so a removal by parameters
// matched nothing (the turn-based mod's pin that would not end, 2026-09-30).
#pragma once

#include <cstdint>

namespace dao
{

using ObjectId = std::uint32_t;

inline constexpr ObjectId kInvalidObject = 0xFFFFFFFF;

} // namespace dao
