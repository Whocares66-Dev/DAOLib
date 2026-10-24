// A row of a merged data table read by its columns' hashes, each cell as its column's type: what
// the generated row classes of dao/core/generated/GdaSchema.h are made of (DAOLib #106). A column
// the table lacks (a mod's table without it), or a cell of another kind, reads as none.
//
//   if (const auto row = dao::core::tables::AbilitiesTalents::Of(*merged, 10000))
//       const std::optional<std::int32_t> use = row->UseType();
#pragma once

#include "dao/core/GdaFile.h"
#include "dao/core/M2da.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace dao::core::tables
{

class RowView
{
  public:
    RowView(const m2da::Merged &table, const m2da::Row &row) : table_(&table), row_(&row)
    {
    }

    const m2da::Merged &Table() const
    {
        return *table_;
    }
    const m2da::Row &Row() const
    {
        return *row_;
    }

  protected:
    // Returns a whole number cell as T; none where the table has no such column or the cell holds
    // no whole number.
    template <class T> std::optional<T> Whole(std::uint32_t hash) const
    {
        const auto *value = Cell<std::int64_t>(hash);
        return value != nullptr ? std::optional<T>(static_cast<T>(*value)) : std::nullopt;
    }
    template <class T> std::optional<T> Real(std::uint32_t hash) const
    {
        const auto *value = Cell<double>(hash);
        return value != nullptr ? std::optional<T>(static_cast<T>(*value)) : std::nullopt;
    }
    // Returns a text cell, a view into the table.
    std::optional<std::wstring_view> Text(std::uint32_t hash) const
    {
        const auto *value = Cell<std::wstring>(hash);
        return value != nullptr ? std::optional<std::wstring_view>(*value) : std::nullopt;
    }

  private:
    template <class T> const T *Cell(std::uint32_t hash) const
    {
        const GdaFile::Cell *cell = m2da::CellOf(*table_, *row_, hash);
        return cell != nullptr ? std::get_if<T>(cell) : nullptr;
    }

    const m2da::Merged *table_;
    const m2da::Row *row_;
};

} // namespace dao::core::tables
