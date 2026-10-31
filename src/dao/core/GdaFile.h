// A GDA file, one of the game's data tables, over bytes: its columns by
// hash (dao/core/GdaHash.h names them) and its rows, each cell of its
// column's type. What dao/Gda.h reads through the running engine, this reads
// from the file.
//
//   const auto table = dao::core::GdaFile::Parse(bytes);
//   const auto label = table->ColumnOf("Label");
//   const auto *text = std::get_if<std::wstring>(&table->Rows()[0][*label]);
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/SortedIndex.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace dao::core
{

class GdaFile
{
  public:
    // A cell: none, an integer (of any of GFF4's integer types), a float (f32 or f64), or text.
    using Cell = std::variant<std::monostate, std::int64_t, double, std::wstring>;

    struct Column
    {
        std::uint32_t hash = 0;
        std::optional<gff4::Type> type; // as the rows' template holds it; none if it has no such field
    };

    // Returns the table in `bytes`; nothing if they are no GFF4 of type G2DA.
    static std::optional<GdaFile> Parse(std::span<const std::byte> bytes);

    const std::vector<Column> &Columns() const
    {
        return columns_;
    }
    // Returns the rows, each a cell for each column: none where the row has no such field.
    const std::vector<std::vector<Cell>> &Rows() const
    {
        return rows_;
    }
    // Returns the place of the column of a hash, or of a name (hashed as the game does).
    std::optional<std::size_t> ColumnOf(std::uint32_t hash) const;
    std::optional<std::size_t> ColumnOf(std::string_view name) const;

    // Returns the place of the row whose ID column holds `id`, the first if several do.
    // @return none for an ID no row has, or a table with no ID column
    std::optional<std::size_t> RowOf(std::int64_t id) const;
    // Returns the row whose ID column holds `id`, every cell in the columns' order, or null as for
    // RowOf.
    const std::vector<Cell> *Row(std::int64_t id) const;

    // Returns a cell as an ID: an integer, or a float that is a whole number; none for anything
    // else.
    static std::optional<std::int64_t> IdOf(const Cell &cell);

    // Returns the table as a file, laid out byte for byte as the game's own are: its columns and
    // their layout as read, its rows.
    std::vector<std::byte> Bytes() const;

    // Returns a GDA file of the given rows under an original's columns and layout, for a mod's
    // part of a table the game merges (an apr_mymod.gda of new appearances).
    // @param rows each a cell for each column, of the column's kind: an integer for a column
    // of whole numbers, a float for f32 or f64, text or none for a string
    // @return empty for an original that is no GDA file, a row of another length, or a cell of
    // another kind
    static std::vector<std::byte> Write(std::span<const std::byte> original, std::span<const std::vector<Cell>> rows);

    static constexpr std::uint32_t kColumnName = 10000; // version 0.1's
    static constexpr std::uint32_t kColumnHash = 10001;
    static constexpr std::uint32_t kColumnList = 10002;
    static constexpr std::uint32_t kRowList = 10003;
    static constexpr std::uint32_t kFirstCell = 10005;

  private:
    // Makes the lookups by a column's hash and a row's ID, once the columns and rows are read.
    void Index();

    std::vector<Column> columns_;
    std::vector<std::vector<Cell>> rows_;
    SortedIndex<std::uint32_t> columnOfHash_; // the first column of each hash
    SortedIndex<std::int64_t> rowOfId_;       // the first row of each ID
    // The layout as read, for Bytes: the header and templates; the top struct's size and where its
    // two lists' offsets are; the column structs as they are; version 0.1's column names and
    // where a column struct holds its name; each column's field in a row, and a row's size.
    std::vector<std::byte> head_;
    std::uint32_t topSize_ = 8;
    std::uint32_t columnListAt_ = 0;
    std::uint32_t rowListAt_ = 4;
    std::vector<std::byte> columnStructs_;
    std::uint32_t columnSize_ = 0;
    std::optional<std::uint32_t> nameAt_;
    std::vector<std::wstring> names_;
    std::vector<std::optional<std::uint32_t>> cellAt_;
    std::uint32_t rowSize_ = 0;
    bool rowList_ = true; // false where the file has none: a table of no rows
};

} // namespace dao::core
