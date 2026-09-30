// The game's data tables (GDA files: abilities, items, packages, ...), as
// the engine has them loaded, read by table, row and column name.
//
//   using dao::script::Table;
//   const auto target = dao::gda::GetInt(Table::AbilitiesTalents, ability, "TargetType");
//   const auto range = dao::gda::GetFloat(Table::Ranges, row, "PrimaryRange");
//
// A table is one of the scripts' TABLE_* numbers (dao::script::Table, from
// 2da_constants_h.nss); a row is the row's id, as scripts pass it to
// GetM2DAInt; a column is its name, hashed as the game does
// (dao/core/GdaHash.h). Nothing comes back for a table, row or column the
// engine does not have. The same values are the scripts' GetM2DAInt and
// GetM2DAFloat, which dao::script::api also reaches; these go straight to
// the table, with no script machinery.
//
// Read 2026-09-30: the engine keeps its tables behind the object in the
// global at 0x00C1D090; its getters find a table by number and call the
// table's typed getter (C2DASource's vtable, which M2DA and G2DA fill):
// slot 10 for an int, which GetM2DAInt uses and GameModeExplore's ability
// handler reads TargetType with, slot 9 for a float, which GetM2DAFloat
// uses. Strings and bools are slots 8 and 11 by their neighbours, not yet
// wrapped.
#pragma once

#include "dao/core/GdaHash.h"
#include "dao/generated/ScriptConstants.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace dao::gda
{

std::optional<std::int32_t> GetInt(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
std::optional<float> GetFloat(std::int32_t table, std::int32_t row, std::uint32_t columnHash);

inline std::optional<std::int32_t> GetInt(script::Table table, std::int32_t row, std::string_view column)
{
    return GetInt(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
inline std::optional<float> GetFloat(script::Table table, std::int32_t row, std::string_view column)
{
    return GetFloat(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}

} // namespace dao::gda
