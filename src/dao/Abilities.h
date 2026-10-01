// An ability's name, for a person to read: its label in the ability table
// and its name in the player's language. For logs and tools that would
// otherwise show an ability's number.
//
//   dao::abilities::Name(10003)   // L"Fireball"
//   dao::abilities::Label(10003)  // L"FIREBALL"
//
// Read 2026-09-30 from the scripts: an ability's row in TABLE_ABILITIES_SPELLS
// (1, ABI_base, every ability, by its id) has Label and NameStrRef, and the
// ability bar names an ability by NameStrRef through the talk tables
// (ui_h.nss). Both go through dao/Gda.h and dao/Tlk.h; nothing comes back
// for an id with no row, or before the tables are loaded.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace dao::abilities
{

std::optional<std::wstring> Label(std::int32_t ability);
// The label when the row names no string.
std::optional<std::wstring> Name(std::int32_t ability);

} // namespace dao::abilities
