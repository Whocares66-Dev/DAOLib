// Notes on the merge:
//
// m2da_base.gda (426 rows on the GOG install) and 2da_base.gda (378) have the
// columns ID, Label, Worksheet and PackageIDForAI; a worksheet is a prefix,
// "ABI_" for table 1, Abilities, or a whole name, "portraits" for table 7,
// which DAOTurnBased's portraits_turnbased.gda extends (tools/gda.py,
// 2026-09-30). 2da_base lists "ABI_base" as an old single table of its own,
// which the longest prefix of all would take abi_base.gda from the abilities'
// M2DA for, so m2da_base's prefixes are tried first (seen 2026-10-12).
//
// The same file is often in several archives: abi_base.gda is in the core's
// 2da.erf and again in two of its patches, the same rows but fifteen. So a
// row the parts give alike is one row, and only rows that differ are
// conflicts. Written for Eluvian's ghilana 2026-10-12 and moved here.
//
// How the game merges, read 2026-10-13 (#106; dev/FILES.md): at start it
// lists every GDA file it has (ListNames: its sources in their order, each
// name once, from the source that wins it) and fills an M2DA for the prefix
// "M2DA_" from the list, which is m2da_base (0x004E0BF0); then for each of
// that table's rows, an M2DA of the row's Worksheet from the same list, or for
// an ID of 10001 to 10999 the one file of the worksheet's name. An M2DA takes,
// in the list's order, each name whose first letters, as many as the prefix
// has, are the prefix in any case (0x004ECC90, 0x004C0B00), loads it as a part
// (0x004ECBB0), and puts its rows into one array sorted by ID (0x004ECA10):
// a part whose first column is not ID (hash 0x66FBC936) gives none, and a row
// whose ID the array has already is dropped (0x004EC910 answers -1), so the
// first part in the list to give an ID keeps it. A script's lookup then finds
// the row by ID in that array (0x004EC130). A source lists its own files by
// their names (seen in play, below; its index, 0x004E6A90, not read).
//
// Seen in play 2026-10-13 (GOG's game, through Eluvian's script tool):
// GetM2DAInt(268, "TitleStrRef", 25) is 445854, Awakening's
// achievements_ep1.gda's row, where the core's patch003.erf has -1 in
// achievements.gda; GetM2DARows(10076), aip_archer, is 12, its own file's
// rows, not the 14 with aip_archer_dwarf, _elf and _human; GetM2DARows(4),
// prj_, is 76, prj_base.gda's 67 and more, where the longest prefix would
// give the worksheet prj_base's file to prj_base alone.
//
// One source's own order, seen in play 2026-10-13: test tables of
// achievements (table 268) giving row 25 each a value of its own, in one ERF
// in a plugin's override\ folder (ahead of every add-in), and the game asked
// for the row. Of achievements_zz1 and _zz2 the first, the entries in either
// order; of _b and _a, _a; of _q and _c, _c; of _m9 and _m10, _m10; of five
// files in a shuffled order, _b2, the first by name; and loose in the folder,
// of _y, _e and _s, written in that order, _e. Each time the first by name.
#include "dao/core/M2da.h"

#include "dao/core/GdaHash.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

namespace dao::core::m2da
{

namespace
{

std::wstring Lower(std::wstring_view text)
{
    std::wstring lower(text);
    for (wchar_t &letter : lower)
    {
        if (letter >= L'A' && letter <= L'Z')
        {
            letter = static_cast<wchar_t>(letter - L'A' + L'a');
        }
    }
    return lower;
}

std::optional<std::int64_t> IdOf(const GdaFile::Cell &cell)
{
    if (const auto *integer = std::get_if<std::int64_t>(&cell))
    {
        return *integer;
    }
    if (const auto *real = std::get_if<double>(&cell); real != nullptr && std::floor(*real) == *real)
    {
        return static_cast<std::int64_t>(*real);
    }
    return std::nullopt;
}

// Every part's columns, by hash, in the order first met; each hash's place.
std::unordered_map<std::uint32_t, std::size_t> MergeColumns(std::span<const GdaFile *const> parts,
                                                            std::vector<GdaFile::Column> &columns)
{
    std::unordered_map<std::uint32_t, std::size_t> columnOf;
    for (const GdaFile *part : parts)
    {
        for (const auto &column : part->Columns())
        {
            const auto [it, added] = columnOf.emplace(column.hash, columns.size());
            if (added)
            {
                columns.push_back(column);
            }
            else if (!columns[it->second].type)
            {
                columns[it->second].type = column.type;
            }
        }
    }
    return columnOf;
}

// One ID's rows: one kept of each version, the other parts that give it alike
// said with it, the rest dropped. True if several parts give it different rows.
bool Collapse(std::vector<Row> &rows, const std::vector<std::size_t> &ofId, std::vector<bool> &dropped)
{
    std::vector<std::size_t> versions;
    for (const std::size_t r : ofId)
    {
        const auto same = std::ranges::find_if(
            versions, [&](std::size_t v) { return rows[v].part != rows[r].part && rows[v].cells == rows[r].cells; });
        if (same != versions.end())
        {
            rows[*same].alike.push_back(rows[r].part);
            dropped[r] = true;
        }
        else
        {
            versions.push_back(r);
        }
    }
    std::set<std::size_t> partsOf;
    for (const std::size_t v : versions)
    {
        partsOf.insert(rows[v].part);
    }
    if (versions.size() < 2 || partsOf.size() < 2)
    {
        return false;
    }
    for (const std::size_t v : versions)
    {
        rows[v].conflict = true;
    }
    return true;
}

// Each ID's first row in the parts' order marked as the one the game keeps.
void MarkTaken(std::vector<Row> &rows, const std::unordered_map<std::int64_t, std::vector<std::size_t>> &takenOf)
{
    for (const auto &[id, ofId] : takenOf)
    {
        Row &first = rows[ofId.front()];
        first.taken = true;
    }
}

} // namespace

std::vector<Worksheet> Worksheets(const GdaFile &table, bool merged)
{
    std::vector<Worksheet> out;
    const auto id = table.ColumnOf("ID");
    const auto label = table.ColumnOf("Label");
    const auto worksheet = table.ColumnOf("Worksheet");
    if (!id || !worksheet)
    {
        return out;
    }
    for (const auto &row : table.Rows())
    {
        const auto *prefix = std::get_if<std::wstring>(&row[*worksheet]);
        if (prefix == nullptr || prefix->empty())
        {
            continue;
        }
        Worksheet sheet;
        sheet.id = IdOf(row[*id]).value_or(-1);
        if (label)
        {
            if (const auto *text = std::get_if<std::wstring>(&row[*label]))
            {
                sheet.label = *text;
            }
        }
        sheet.prefix = Lower(*prefix);
        sheet.merged = merged;
        out.push_back(std::move(sheet));
    }
    return out;
}

bool Takes(const Worksheet &worksheet, std::wstring_view stem)
{
    const std::wstring lower = Lower(stem);
    return worksheet.Single() ? lower == worksheet.prefix : lower.starts_with(worksheet.prefix);
}

std::vector<std::size_t> WorksheetsOf(std::span<const Worksheet> worksheets, std::wstring_view stem)
{
    std::vector<std::size_t> found;
    for (std::size_t i = 0; i < worksheets.size(); ++i)
    {
        if (worksheets[i].merged && Takes(worksheets[i], stem))
        {
            found.push_back(i);
        }
    }
    if (!found.empty())
    {
        return found;
    }
    // 2da_base's older single tables: which file the game gives one is not read; the longest
    // prefix, as before the merge was read.
    const std::wstring lower = Lower(stem);
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < worksheets.size(); ++i)
    {
        const auto &prefix = worksheets[i].prefix;
        if (!worksheets[i].merged && lower.starts_with(prefix) &&
            (!best || prefix.size() > worksheets[*best].prefix.size()))
        {
            best = i;
        }
    }
    if (best)
    {
        found.push_back(*best);
    }
    return found;
}

Merged Merge(std::span<const GdaFile *const> parts)
{
    Merged merged;
    const auto columnOf = MergeColumns(parts, merged.columns);
    std::unordered_map<std::int64_t, std::vector<std::size_t>> rowsOf;
    // Each ID's rows in the order the game meets them, from the parts it takes rows from.
    std::unordered_map<std::int64_t, std::vector<std::size_t>> takenOf;
    for (std::size_t p = 0; p < parts.size(); ++p)
    {
        const GdaFile &table = *parts[p];
        const auto id = table.ColumnOf("ID");
        const bool gives = id && *id == 0;
        if (!gives)
        {
            merged.skipped.push_back(p);
        }
        for (const auto &cells : table.Rows())
        {
            Row row;
            row.part = p;
            row.cells.resize(merged.columns.size());
            for (std::size_t c = 0; c < table.Columns().size(); ++c)
            {
                row.cells[columnOf.at(table.Columns()[c].hash)] = cells[c];
            }
            row.id = id ? IdOf(cells[*id]) : std::nullopt;
            if (row.id)
            {
                rowsOf[*row.id].push_back(merged.rows.size());
                if (gives)
                {
                    takenOf[*row.id].push_back(merged.rows.size());
                }
            }
            merged.rows.push_back(std::move(row));
        }
    }
    MarkTaken(merged.rows, takenOf);
    std::vector<bool> dropped(merged.rows.size(), false);
    for (const auto &[id, ofId] : rowsOf)
    {
        merged.conflicts += Collapse(merged.rows, ofId, dropped) ? 1 : 0;
    }
    std::vector<Row> kept;
    kept.reserve(merged.rows.size());
    for (std::size_t r = 0; r < merged.rows.size(); ++r)
    {
        if (!dropped[r])
        {
            kept.push_back(std::move(merged.rows[r]));
        }
    }
    merged.rows = std::move(kept);
    return merged;
}

const Row *Taken(const Merged &table, std::int64_t id)
{
    const auto found = std::ranges::find_if(table.rows, [&](const Row &row) { return row.taken && row.id == id; });
    return found != table.rows.end() ? &*found : nullptr;
}

const GdaFile::Cell *CellOf(const Merged &table, const Row &row, std::string_view column)
{
    return CellOf(table, row, GdaColumnHash(column));
}

const GdaFile::Cell *CellOf(const Merged &table, const Row &row, std::uint32_t hash)
{
    for (std::size_t i = 0; i < table.columns.size() && i < row.cells.size(); ++i)
    {
        if (table.columns[i].hash == hash)
        {
            return &row.cells[i];
        }
    }
    return nullptr;
}

} // namespace dao::core::m2da
