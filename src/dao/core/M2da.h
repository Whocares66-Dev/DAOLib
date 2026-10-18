// The game's data tables as it merges them (an M2DA, "merged 2DA"): every
// GDA file whose name starts with a table's worksheet prefix is one table,
// the core's abi_base.gda and an add-in's or a mod's abi_*.gda together.
// m2da_base.gda numbers the merged tables and names each prefix, what
// dao::gda and the scripts' TABLE_* constants take; 2da_base.gda the older
// single ones.
//
// How the game merges, read in the exe and seen in play (DAOLib #106,
// dev/FILES.md): a file is part of every worksheet whose prefix it starts
// with, but of a single table (its ID 10001 to 10999) only the file of its
// own name; the parts are taken in the order the game lists its files; and
// of the rows of one ID the game keeps the first and drops the rest.
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

    // Returns true for a table the game loads from the one file of its own name, not merged:
    // m2da_base's IDs 10001 to 10999 (the AI's packages, aip_*).
    bool Single() const
    {
        return merged && id >= 10001 && id <= 10999;
    }
};

// Returns the tables m2da_base.gda or 2da_base.gda names, by their ID, Label and Worksheet
// columns; a row with no worksheet is left out.
// @param merged true for m2da_base.gda's rows, false for 2da_base.gda's
std::vector<Worksheet> Worksheets(const GdaFile &table, bool merged);

// Returns true if a GDA file is part of a worksheet's table: its stem starts with the prefix, in
// any case, or for a Single() table is the prefix.
// @param stem the file's name without ".gda", any case
bool Takes(const Worksheet &worksheet, std::wstring_view stem);

// Returns the worksheets a GDA file is part of, in their order: every one of m2da_base's that
// Takes it (a file can be part of several, as aip_archer_dwarf is of aip_archer's and its own),
// or if none, the longest prefix of 2da_base's it starts with.
// @param stem the file's name without ".gda", any case
std::vector<std::size_t> WorksheetsOf(std::span<const Worksheet> worksheets, std::wstring_view stem);

// A row of a merged table.
struct Row
{
    std::size_t part = 0;             // the file it is from, its place among the parts
    std::vector<GdaFile::Cell> cells; // a cell for each of Merged::columns
    std::optional<std::int64_t> id;   // its ID column's value
    bool conflict = false;            // another part gives its ID a different row
    bool taken = false;               // the game's: the first row of its ID in the parts' order
    std::vector<std::size_t> alike;   // the other parts that give this same row
};

struct Merged
{
    std::vector<GdaFile::Column> columns; // every part's, by hash, in the order first met
    std::vector<Row> rows;                // part by part, each in its file's order; a row others give alike once
    std::size_t conflicts = 0;            // IDs parts give different rows
    std::vector<std::size_t> skipped;     // parts the game takes no row from: their first column is
                                          // not ID (0x004ECA10)
};

// Returns the parts merged as the game merges them: rows keyed by their ID column, a row parts
// give alike given once, different rows for one ID each given and marked a conflict, and of each
// ID's rows the first in the parts' order taken.
// @param parts the table's files in the order the game lists them: of each name the copy the game
// takes, sorted by install::Index::SortFiles (their sources' order, then by name); none may be null
Merged Merge(std::span<const GdaFile *const> parts);

// Returns the row the game takes for an ID, or null for an ID no row has.
const Row *Taken(const Merged &table, std::int64_t id);

// Returns a row's cell of a column, by the column's name in any case; null where the table has no
// such column.
const GdaFile::Cell *CellOf(const Merged &table, const Row &row, std::string_view column);

} // namespace dao::core::m2da
