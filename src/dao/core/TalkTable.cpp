#include "dao/core/TalkTable.h"

#include "dao/core/Gff4.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace dao::core
{

std::optional<TalkTableFile> TalkTableFile::Parse(std::span<const std::byte> bytes)
{
    const auto file = gff4::File::Parse(bytes);
    if (!file || file->Type() != "TLK" || file->TypeVersion() != "V0.2")
    {
        return std::nullopt;
    }
    const auto root = file->Root();
    if (!root || root->Shape().Find(kStringList) == nullptr)
    {
        return std::nullopt;
    }
    TalkTableFile table;
    for (const gff4::Struct &entry : root->Structs(kStringList))
    {
        const auto id = entry.Get<std::uint32_t>(kStringId);
        auto text = entry.String(kString);
        if (id && text)
        {
            table.strings_.emplace_back(*id, std::move(*text));
        }
    }
    // By id, the last of any repeated id kept, as a map would keep it.
    std::ranges::stable_sort(table.strings_, {}, &std::pair<std::uint32_t, std::wstring>::first);
    return table;
}

std::optional<std::wstring> TalkTableFile::String(std::uint32_t id) const
{
    auto it = std::ranges::upper_bound(strings_, id, {}, &std::pair<std::uint32_t, std::wstring>::first);
    if (it == strings_.begin() || std::prev(it)->first != id)
    {
        return std::nullopt;
    }
    return std::prev(it)->second;
}

} // namespace dao::core
