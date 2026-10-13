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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
        std::optional<gff4::Type> type; // as the rows hold it; none if no row has the cell
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

    static constexpr std::uint32_t kColumnHash = 10001;
    static constexpr std::uint32_t kColumnList = 10002;
    static constexpr std::uint32_t kRowList = 10003;
    static constexpr std::uint32_t kFirstCell = 10005;

  private:
    std::vector<Column> columns_;
    std::vector<std::vector<Cell>> rows_;
};

} // namespace dao::core
