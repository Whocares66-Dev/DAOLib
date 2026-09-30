// An engine object's id: what a script's `object` is, and what the
// application's lookups take (dao/ClientExoApp.h). A 32-bit number, as in
// Neverwinter Nights, whose invalid id DAO keeps: 0x7F000000 appears 67 times
// in the exe's code (dev/LINEAGE.md). That value is taken from NWN and the
// count; it is not yet seen in play.
#pragma once

#include <cstdint>

namespace dao
{

using ObjectId = std::uint32_t;

inline constexpr ObjectId kInvalidObject = 0x7F000000;

} // namespace dao
