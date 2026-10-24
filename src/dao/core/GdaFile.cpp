// Notes on GDA files:
//
// A GFF4 of type "G2DA" (the game keeps 749 in packages\core\data\2da.erf).
// Read 2026-09-30 by tools/gda.py (retired 2026-10-24, #106), which this follows, as xoreos reads it
// (src/aurora/gdafile.cpp): the top struct holds the column list (10002, each
// column's name hash in 10001) and the row list (10003); a row's template has
// a field per column, 10005 on, of the column's type. Written in C++ for
// Eluvian's ghilana 2026-10-12 and moved here (DAOLib #106).
//
// The layout, read 2026-10-24 over every table of the GOG install (DAOLib
// #106): 1,715 of version 0.2, the data tables, and 1,432 of version 0.1, the
// conversations' (designerdialogs.erf), whose columns hold their name's text
// (10000) as well as its hash and no type byte. Kept as read: the header and
// templates, and where each column's field is in a row. A table's fields are
// mostly the plain layout (those of four bytes and more in column order, then
// the bytes, the row rounded up to four: 1,709 of the 1,715), but six, apr_base
// and bitm_base's kin, hold their editing's history (a column turned into a
// byte moved to the row's end, later ones in the holes), so a layout cannot be
// worked out from the columns alone. The data, made again by Bytes, is the
// same for all: the top struct, the column list with the columns' structs as
// they are (a type byte and three of 0xFF in version 0.2), version 0.1's names'
// texts, then on four bytes after 0xFF the row list, the bytes of a row no
// field takes 0xFF; then the rows' texts, in row order and each row's in column
// order, each on four bytes after 0xFF, a text met before not written again but
// pointed at, an empty text not written (offset 0xFFFFFFFF; no table of the
// game's has an empty text written). A table of no rows has no row list.
// tests/test_gdafile_roundtrip.cpp writes every one of them back to its bytes.
#include "dao/core/GdaFile.h"

#include "dao/core/GdaHash.h"

#include <cstring>
#include <map>
#include <utility>

namespace dao::core
{

namespace
{

constexpr std::uint32_t kNull = 0xFFFFFFFF;

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

std::optional<std::uint32_t> U32At(std::span<const std::byte> bytes, std::size_t at)
{
    if (at > bytes.size() || bytes.size() - at < 4)
    {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + at, sizeof(value));
    return value;
}

// The width in a row of a whole number's field; none for another type.
std::optional<std::size_t> WholeWidth(gff4::Type type)
{
    switch (type)
    {
    case gff4::Type::U8:
    case gff4::Type::S8:
        return 1;
    case gff4::Type::U16:
    case gff4::Type::S16:
        return 2;
    case gff4::Type::U32:
    case gff4::Type::S32:
        return 4;
    case gff4::Type::U64:
    case gff4::Type::S64:
        return 8;
    default:
        return std::nullopt;
    }
}

// Whether a cell is of the kind a column of the type holds; none for a column with no field.
bool Fits(const std::optional<gff4::Type> &type, const GdaFile::Cell &cell)
{
    if (!type)
    {
        return std::holds_alternative<std::monostate>(cell);
    }
    if (WholeWidth(*type))
    {
        return std::holds_alternative<std::int64_t>(cell);
    }
    if (*type == gff4::Type::F32 || *type == gff4::Type::F64)
    {
        return std::holds_alternative<double>(cell);
    }
    return *type == gff4::Type::String &&
           (std::holds_alternative<std::wstring>(cell) || std::holds_alternative<std::monostate>(cell));
}

// A table's file made after its header and templates: its data, each place in it counted from
// the data's start as the file's offsets are, and its texts, each written once.
class DataWriter
{
  public:
    explicit DataWriter(const std::vector<std::byte> &head) : out_(head), data_(head.size())
    {
    }

    std::uint32_t Here() const
    {
        return static_cast<std::uint32_t>(out_.size() - data_);
    }
    void Put(std::size_t at, const void *value, std::size_t size)
    {
        std::memcpy(out_.data() + data_ + at, value, size);
    }
    void Put32At(std::size_t at, std::uint32_t value)
    {
        Put(at, &value, sizeof(value));
    }
    void Append32(std::uint32_t value)
    {
        out_.resize(out_.size() + 4);
        Put32At(Here() - 4, value);
    }
    void Append(const std::vector<std::byte> &bytes)
    {
        out_.insert(out_.end(), bytes.begin(), bytes.end());
    }
    // Adds bytes of 0xFF, which no field takes.
    void Grow(std::size_t size)
    {
        out_.resize(out_.size() + size, std::byte{0xFF});
    }
    void Align()
    {
        out_.resize((out_.size() + 3) & ~std::size_t{3}, std::byte{0xFF});
    }
    // Returns where a text is, written now if not before; 0xFFFFFFFF for an empty one.
    std::uint32_t Text(const std::wstring &text)
    {
        if (text.empty())
        {
            return kNull;
        }
        const auto [found, added] = written_.try_emplace(text, 0);
        if (added)
        {
            Align();
            found->second = Here();
            Append32(static_cast<std::uint32_t>(text.size() + 1));
            for (const wchar_t c : text)
            {
                const auto unit = static_cast<char16_t>(c);
                out_.resize(out_.size() + 2);
                Put(Here() - 2, &unit, sizeof(unit));
            }
            out_.resize(out_.size() + 2, std::byte{0});
        }
        return found->second;
    }
    std::vector<std::byte> Bytes()
    {
        return std::move(out_);
    }

  private:
    std::vector<std::byte> out_;
    std::size_t data_;
    std::map<std::wstring, std::uint32_t, std::less<>> written_;
};

// One cell at its field of a row: a whole number in its field's width, a float, or a text.
void PutCell(DataWriter &out, std::size_t at, gff4::Type type, const GdaFile::Cell &cell)
{
    if (type == gff4::Type::String)
    {
        const auto *text = std::get_if<std::wstring>(&cell);
        out.Put32At(at, out.Text(text != nullptr ? *text : std::wstring{}));
    }
    else if (const auto *number = std::get_if<std::int64_t>(&cell); number != nullptr && WholeWidth(type))
    {
        out.Put(at, number, *WholeWidth(type)); // little-endian: the low bytes first
    }
    else if (const auto *real = std::get_if<double>(&cell); real != nullptr && type == gff4::Type::F32)
    {
        const auto value = static_cast<float>(*real);
        out.Put(at, &value, sizeof(value));
    }
    else if (real != nullptr && type == gff4::Type::F64)
    {
        out.Put(at, real, sizeof(*real));
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
    const std::vector<gff4::Struct> columns = root->Structs(kColumnList);
    for (const auto &column : columns)
    {
        table.columns_.push_back(Column{column.Get<std::uint32_t>(kColumnHash).value_or(0), std::nullopt});
    }
    for (const auto &row : root->Structs(kRowList))
    {
        std::vector<Cell> cells(table.columns_.size());
        for (std::size_t i = 0; i < table.columns_.size(); ++i)
        {
            const gff4::Field *field = row.Shape().Find(kFirstCell + static_cast<std::uint32_t>(i));
            if (field != nullptr)
            {
                cells[i] = CellOf(row, *field);
            }
        }
        table.rows_.push_back(std::move(cells));
    }
    // The layout, for Bytes. A file of another shape keeps none, and Bytes gives nothing for it.
    const auto &templates = file->Templates();
    const gff4::Field *columnList = root->Shape().Find(kColumnList);
    const gff4::Field *rowList = root->Shape().Find(kRowList);
    const std::size_t data = file->DataOffset();
    const auto columnsAt = U32At(bytes, data + (columnList != nullptr ? columnList->offset : 0));
    const auto rowsAt = U32At(bytes, data + (rowList != nullptr ? rowList->offset : 0));
    if (columnList == nullptr || rowList == nullptr || columnList->type >= templates.size() ||
        rowList->type >= templates.size() || !columnsAt || *columnsAt == kNull || !rowsAt)
    {
        return table;
    }
    const gff4::Template &column = templates[columnList->type];
    const gff4::Template &row = templates[rowList->type];
    const std::size_t structsAt = data + *columnsAt + 4;
    const std::size_t structsSize = static_cast<std::size_t>(column.size) * columns.size();
    if (structsAt > bytes.size() || bytes.size() - structsAt < structsSize)
    {
        return table;
    }
    table.head_.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(data));
    table.topSize_ = root->Shape().size;
    table.columnListAt_ = columnList->offset;
    table.rowListAt_ = rowList->offset;
    table.columnStructs_.assign(bytes.begin() + static_cast<std::ptrdiff_t>(structsAt),
                                bytes.begin() + static_cast<std::ptrdiff_t>(structsAt + structsSize));
    table.columnSize_ = column.size;
    if (const gff4::Field *name = column.Find(kColumnName))
    {
        table.nameAt_ = name->offset;
        for (const auto &one : columns)
        {
            table.names_.push_back(one.String(kColumnName).value_or(std::wstring{}));
        }
    }
    for (std::size_t i = 0; i < table.columns_.size(); ++i)
    {
        const gff4::Field *field = row.Find(kFirstCell + static_cast<std::uint32_t>(i));
        table.cellAt_.push_back(field != nullptr ? std::optional(field->offset) : std::nullopt);
        if (field != nullptr)
        {
            table.columns_[i].type = static_cast<gff4::Type>(field->type);
        }
    }
    table.rowSize_ = row.size;
    table.rowList_ = *rowsAt != kNull;
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

std::vector<std::byte> GdaFile::Bytes() const
{
    if (head_.empty())
    {
        return {};
    }
    DataWriter out(head_);
    out.Grow(topSize_);
    out.Put32At(columnListAt_, out.Here());
    out.Append32(static_cast<std::uint32_t>(columns_.size()));
    const std::size_t columnsAt = out.Here();
    out.Append(columnStructs_);
    if (nameAt_)
    {
        for (std::size_t i = 0; i < names_.size(); ++i)
        {
            const std::uint32_t name = out.Text(names_[i]);
            out.Put32At(columnsAt + (i * columnSize_) + *nameAt_, name);
        }
    }
    if (!rowList_)
    {
        out.Put32At(rowListAt_, kNull);
        return out.Bytes();
    }
    out.Align();
    out.Put32At(rowListAt_, out.Here());
    out.Append32(static_cast<std::uint32_t>(rows_.size()));
    const std::size_t rowsAt = out.Here();
    out.Grow(rows_.size() * rowSize_);
    for (std::size_t r = 0; r < rows_.size(); ++r)
    {
        for (std::size_t c = 0; c < columns_.size() && c < rows_[r].size(); ++c)
        {
            if (cellAt_[c] && columns_[c].type)
            {
                const std::size_t at = rowsAt + (r * rowSize_) + *cellAt_[c];
                PutCell(out, at, *columns_[c].type, rows_[r][c]);
            }
        }
    }
    return out.Bytes();
}

std::vector<std::byte> GdaFile::Write(std::span<const std::byte> original, std::span<const std::vector<Cell>> rows)
{
    auto table = Parse(original);
    if (!table || table->head_.empty())
    {
        return {};
    }
    for (const auto &row : rows)
    {
        if (row.size() != table->columns_.size())
        {
            return {};
        }
        for (std::size_t c = 0; c < row.size(); ++c)
        {
            if (!Fits(table->columns_[c].type, row[c]))
            {
                return {};
            }
        }
    }
    table->rows_.assign(rows.begin(), rows.end());
    table->rowList_ = !rows.empty();
    return table->Bytes();
}

} // namespace dao::core
