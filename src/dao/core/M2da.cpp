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
// conflicts. Which part's row the game keeps is not known: the order of the
// game's sources is the resource manager's in the exe, unread (#106 step 4).
// Written for Eluvian's ghilana 2026-10-12 and moved here.
#include "dao/core/M2da.h"

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

std::optional<std::size_t> WorksheetOf(std::span<const Worksheet> worksheets, std::wstring_view stem)
{
    const std::wstring lower = Lower(stem);
    for (const bool merged : {true, false})
    {
        std::optional<std::size_t> best;
        for (std::size_t i = 0; i < worksheets.size(); ++i)
        {
            const auto &prefix = worksheets[i].prefix;
            if (worksheets[i].merged == merged && lower.starts_with(prefix) &&
                (!best || prefix.size() > worksheets[*best].prefix.size()))
            {
                best = i;
            }
        }
        if (best)
        {
            return best;
        }
    }
    return std::nullopt;
}

Merged Merge(std::span<const GdaFile *const> parts)
{
    Merged merged;
    const auto columnOf = MergeColumns(parts, merged.columns);
    std::unordered_map<std::int64_t, std::vector<std::size_t>> rowsOf;
    for (std::size_t p = 0; p < parts.size(); ++p)
    {
        const GdaFile &table = *parts[p];
        const auto id = table.ColumnOf("ID");
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
            }
            merged.rows.push_back(std::move(row));
        }
    }
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

} // namespace dao::core::m2da
