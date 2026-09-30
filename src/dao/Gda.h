// The game's data tables (GDA files: abilities, items, packages, ...), as
// the engine has them loaded, read by table, row and column name.
//
//   using dao::script::Table;
//   const auto target = dao::gda::GetInt(Table::AbilitiesTalents, ability, "TargetType");
//   const auto range = dao::gda::GetFloat(Table::Ranges, row, "PrimaryRange");
//   const auto label = dao::gda::GetString(Table::AbilitiesTalents, ability, "Label");
//   const auto rows = dao::gda::Rows(Table::AbilitiesTalents);
//
// A table is one of the scripts' TABLE_* numbers (dao::script::Table, from
// 2da_constants_h.nss): an M2DA, every file of the game's 2da.erf whose
// name starts with the table's prefix (m2da_base.gda: 1 is "ABI_", 6
// "BITM_"). dev/gda_tables.csv lists each file's table and columns with
// their types (tools/gda.py). A row is the row's id, as scripts pass it to
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
// uses, slot 8 for a string, which GetM2DAString uses, and slot 11 for a
// bool. The row and column counts are the table's slots 5 and 4, as
// GetM2DARows and GetM2DAColumns have them. A string or bool asked of a
// column of another type gets what the engine gives: whatever the table's
// conversion makes of it, as GetM2DAString would.
#pragma once

#include "dao/core/GdaHash.h"
#include "dao/generated/ScriptConstants.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dao::gda
{

std::optional<std::int32_t> GetInt(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
std::optional<float> GetFloat(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
std::optional<std::wstring> GetString(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
std::optional<bool> GetBool(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
// How many rows and columns a table has; nothing for a table the engine
// does not have.
std::optional<std::int32_t> Rows(std::int32_t table);
std::optional<std::int32_t> Columns(std::int32_t table);

inline std::optional<std::int32_t> GetInt(script::Table table, std::int32_t row, std::string_view column)
{
    return GetInt(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
inline std::optional<float> GetFloat(script::Table table, std::int32_t row, std::string_view column)
{
    return GetFloat(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
inline std::optional<std::wstring> GetString(script::Table table, std::int32_t row, std::string_view column)
{
    return GetString(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
inline std::optional<bool> GetBool(script::Table table, std::int32_t row, std::string_view column)
{
    return GetBool(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
inline std::optional<std::int32_t> Rows(script::Table table)
{
    return Rows(static_cast<std::int32_t>(table));
}
inline std::optional<std::int32_t> Columns(script::Table table)
{
    return Columns(static_cast<std::int32_t>(table));
}

} // namespace dao::gda
