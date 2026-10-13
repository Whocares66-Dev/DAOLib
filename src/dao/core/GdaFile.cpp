// Notes on GDA files:
//
// A GFF4 of type "G2DA" (the game keeps 749 in packages\core\data\2da.erf).
// Read 2026-09-30 by tools/gda.py, which this follows, as xoreos reads it
// (src/aurora/gdafile.cpp): the top struct holds the column list (10002, each
// column's name hash in 10001) and the row list (10003); a row's template has
// a field per column, 10005 on, of the column's type. Written in C++ for
// Eluvian's ghilana 2026-10-12 and moved here (DAOLib #106).
#include "dao/core/GdaFile.h"

#include "dao/core/GdaHash.h"

namespace dao::core
{

namespace
{

GdaFile::Cell CellOf(const gff4::Struct &row, const gff4::Field &field)
{
    const auto label = field.label;
    const auto integer = [](const auto &value) -> GdaFile::Cell {
        return value ? GdaFile::Cell(static_cast<std::int64_t>(*value)) : GdaFile::Cell();
    };
    switch (static_cast<gff4::Type>(field.type))
    {
    case gff4::Type::U8:
        return integer(row.Get<std::uint8_t>(label));
    case gff4::Type::S8:
        return integer(row.Get<std::int8_t>(label));
    case gff4::Type::U16:
        return integer(row.Get<std::uint16_t>(label));
    case gff4::Type::S16:
        return integer(row.Get<std::int16_t>(label));
    case gff4::Type::U32:
        return integer(row.Get<std::uint32_t>(label));
    case gff4::Type::S32:
        return integer(row.Get<std::int32_t>(label));
    case gff4::Type::S64:
        return integer(row.Get<std::int64_t>(label));
    case gff4::Type::U64:
        return integer(row.Get<std::uint64_t>(label)); // past 2^63 as negative: no table of the game's has a u64
    case gff4::Type::F32: {
        const auto value = row.Get<float>(label);
        return value ? GdaFile::Cell(static_cast<double>(*value)) : GdaFile::Cell();
    }
    case gff4::Type::F64: {
        const auto value = row.Get<double>(label);
        return value ? GdaFile::Cell(*value) : GdaFile::Cell();
    }
    case gff4::Type::String: {
        auto value = row.String(label);
        return value ? GdaFile::Cell(std::move(*value)) : GdaFile::Cell();
    }
    default:
        return {};
    }
}

} // namespace

std::optional<GdaFile> GdaFile::Parse(std::span<const std::byte> bytes)
{
    const auto file = gff4::File::Parse(bytes);
    if (!file || file->Type() != "G2DA")
    {
        return std::nullopt;
    }
    const auto root = file->Root();
    if (!root)
    {
        return std::nullopt;
    }
    GdaFile table;
    for (const auto &column : root->Structs(kColumnList))
    {
        table.columns_.push_back(Column{column.Get<std::uint32_t>(kColumnHash).value_or(0), std::nullopt});
    }
    for (const auto &row : root->Structs(kRowList))
    {
        std::vector<Cell> cells(table.columns_.size());
        for (std::size_t i = 0; i < table.columns_.size(); ++i)
        {
            const gff4::Field *field = row.Shape().Find(kFirstCell + static_cast<std::uint32_t>(i));
            if (field == nullptr)
            {
                continue;
            }
            if (!table.columns_[i].type)
            {
                table.columns_[i].type = static_cast<gff4::Type>(field->type);
            }
            cells[i] = CellOf(row, *field);
        }
        table.rows_.push_back(std::move(cells));
    }
    return table;
}

std::optional<std::size_t> GdaFile::ColumnOf(std::uint32_t hash) const
{
    for (std::size_t i = 0; i < columns_.size(); ++i)
    {
        if (columns_[i].hash == hash)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> GdaFile::ColumnOf(std::string_view name) const
{
    return ColumnOf(GdaColumnHash(name));
}

} // namespace dao::core
