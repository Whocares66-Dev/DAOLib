// A struct of a template (GFF3) read by its fields' labels, each as the type the schema gives it:
// what the generated views of dao/core/generated/Gff3Schema.h are made of (DAOLib #106). A field
// the file lacks, or of another type, reads as none. A view points at its file's bytes, which must
// outlive it, as gff3::Struct does.
//
//   if (const auto creature = dao::core::templates::utc::Creature::Of(*file))
//       if (const auto look = creature->Appearance_Data())
//           const std::optional<std::int32_t> type = look->Appearance_Type();
#pragma once

#include "dao/core/Gff3.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dao::core::templates
{

class StructView
{
  public:
    explicit StructView(gff3::Struct in) : in_(std::move(in))
    {
    }

    const gff3::Struct &Struct() const
    {
        return in_;
    }

  protected:
    template <class T> std::optional<T> Number(std::string_view label) const
    {
        return in_.Get<T>(label);
    }
    std::optional<std::string> Text(std::string_view label) const
    {
        return in_.String(label);
    }
    std::optional<std::string> ResRef(std::string_view label) const
    {
        return in_.ResRef(label);
    }
    std::optional<gff3::LocString> Loc(std::string_view label) const
    {
        return in_.Loc(label);
    }
    std::optional<std::uint32_t> StrRef(std::string_view label) const
    {
        return in_.StrRef(label);
    }
    std::optional<std::vector<float>> Floats(std::string_view label) const
    {
        std::vector<float> floats = in_.Floats(label);
        return floats.empty() ? std::nullopt : std::optional(std::move(floats));
    }
    std::optional<std::span<const std::byte>> Void(std::string_view label) const
    {
        return in_.Void(label);
    }
    template <class View> std::optional<View> Child(std::string_view label) const
    {
        const auto child = in_.Child(label);
        return child ? std::optional<View>(View(*child)) : std::nullopt;
    }
    // Returns a list's entries; none for a file without the list.
    template <class View> std::vector<View> List(std::string_view label) const
    {
        std::vector<View> views;
        for (const gff3::Struct &entry : in_.List(label))
        {
            views.emplace_back(entry);
        }
        return views;
    }

  private:
    gff3::Struct in_;
};

} // namespace dao::core::templates
