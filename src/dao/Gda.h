// Reads the game's data tables (GDA files: abilities, items, ranges and so on) by table, row and
// column name. It reads the tables the game has already loaded, without going through a script
// command, and returns the same values as the scripts' GetM2DAInt, GetM2DAFloat and
// GetM2DAString.
//
//   using dao::script::Table;
//   const auto target = dao::gda::GetInt(Table::AbilitiesTalents, ability, "TargetType");
//   const auto range = dao::gda::GetFloat(Table::Ranges, row, "PrimaryRange");
//   const auto label = dao::gda::GetString(Table::AbilitiesTalents, ability, "Label");
//   const auto rows = dao::gda::Rows(Table::AbilitiesTalents);
//
// dev/gda_schema.csv lists each worksheet's columns with their types.
#pragma once

#include "dao/core/GdaHash.h"
#include "dao/generated/ScriptConstants.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dao::gda
{

// Reads an int cell of a data table.
// @param table one of the scripts' TABLE_* numbers (dao::script::Table)
// @param row the row's ID, as scripts pass it to GetM2DAInt
// @param columnHash the column's name, hashed with core::GdaColumnHash
// @return nullopt if the game has no such table, row or column, or has not loaded its tables yet
std::optional<std::int32_t> GetInt(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
// Reads a float cell. Works like GetInt.
std::optional<float> GetFloat(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
// Reads a string cell. Works like GetInt. For a column of another type it returns the table's
// own conversion of the value, as GetM2DAString does.
std::optional<std::wstring> GetString(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
// Reads a bool cell. Works like GetInt. For a column of another type it returns the table's own
// conversion of the value.
std::optional<bool> GetBool(std::int32_t table, std::int32_t row, std::uint32_t columnHash);
// Returns the number of rows in a table.
// @return nullopt if the game has no such table
std::optional<std::int32_t> Rows(std::int32_t table);
// Returns the number of columns in a table.
// @return nullopt if the game has no such table
std::optional<std::int32_t> Columns(std::int32_t table);

// Reads an int cell by the column's name. Otherwise it works like GetInt above.
inline std::optional<std::int32_t> GetInt(script::Table table, std::int32_t row, std::string_view column)
{
    return GetInt(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
// Reads a float cell by the column's name.
inline std::optional<float> GetFloat(script::Table table, std::int32_t row, std::string_view column)
{
    return GetFloat(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
// Reads a string cell by the column's name.
inline std::optional<std::wstring> GetString(script::Table table, std::int32_t row, std::string_view column)
{
    return GetString(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
// Reads a bool cell by the column's name.
inline std::optional<bool> GetBool(script::Table table, std::int32_t row, std::string_view column)
{
    return GetBool(static_cast<std::int32_t>(table), row, core::GdaColumnHash(column));
}
// Returns the number of rows in a table, or nullopt if the game has no such table.
inline std::optional<std::int32_t> Rows(script::Table table)
{
    return Rows(static_cast<std::int32_t>(table));
}
// Returns the number of columns in a table, or nullopt if the game has no such table.
inline std::optional<std::int32_t> Columns(script::Table table)
{
    return Columns(static_cast<std::int32_t>(table));
}

} // namespace dao::gda
