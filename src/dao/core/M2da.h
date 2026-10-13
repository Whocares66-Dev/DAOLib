// The game's data tables as it merges them (an M2DA, "merged 2DA"): every
// GDA file whose name starts with a table's worksheet prefix is one table,
// the core's abi_base.gda and an add-in's or a mod's abi_*.gda together.
// m2da_base.gda numbers the merged tables and names each prefix, what
// dao::gda and the scripts' TABLE_* constants take; 2da_base.gda the older
// single ones.
//
//   const auto sheets = dao::core::m2da::Worksheets(m2daBase, true);
//   const auto merged = dao::core::m2da::Merge(parts);
#pragma once

#include "dao/core/GdaFile.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::m2da
{

struct Worksheet
{
    std::int64_t id = -1;
    std::wstring label;  // "Abilities"
    std::wstring prefix; // in lower case: "abi_"
    bool merged = true;  // m2da_base's; false for 2da_base's
};

// Returns the tables m2da_base.gda or 2da_base.gda names, by their ID, Label and Worksheet
// columns; a row with no worksheet is left out.
// @param merged true for m2da_base.gda's rows, false for 2da_base.gda's
std::vector<Worksheet> Worksheets(const GdaFile &table, bool merged);

// Returns the worksheet a GDA file is part of: the longest prefix of m2da_base's its stem
// starts with, or of 2da_base's if none of those.
// @param stem the file's name without ".gda", any case
std::optional<std::size_t> WorksheetOf(std::span<const Worksheet> worksheets, std::wstring_view stem);

// A row of a merged table.
struct Row
{
    std::size_t part = 0;             // the file it is from, its place among the parts
    std::vector<GdaFile::Cell> cells; // a cell for each of Merged::columns
    std::optional<std::int64_t> id;   // its ID column's value
    bool conflict = false;            // another part gives its ID a different row
    std::vector<std::size_t> alike;   // the other parts that give this same row
};

struct Merged
{
    std::vector<GdaFile::Column> columns; // every part's, by hash, in the order first met
    std::vector<Row> rows;                // part by part, each in its file's order; a row others give alike once
    std::size_t conflicts = 0;            // IDs parts give different rows
};

// Returns the parts merged: rows keyed by their ID column, a row parts give alike given once,
// and different rows for one ID each given and marked a conflict, since which the game keeps
// is the engine's to say.
// @param parts the table's files, each a copy of one; none may be null
Merged Merge(std::span<const GdaFile *const> parts);

} // namespace dao::core::m2da
