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

// Written as the game's own add-ins' tables are laid out (dao_prc_promo_edg_en-us.tlk, read
// 2026-10-14): "GFF V4.0", "PC  ", "TLK ", "V0.2", two templates and the data at 96; the
// templates "TLK " (4 bytes: its list of STRN held in place) and "STRN" (8 bytes: the id, and
// its string by offset); the data the top struct, the list, then the strings, each a count of
// UTF-16 units and the units, a NUL last, as gff4::Editor writes them.
std::vector<std::byte> TalkTableFile::Write(std::span<const std::pair<std::uint32_t, std::wstring>> strings)
{
    std::vector<std::uint32_t> ids;
    std::ranges::transform(strings, std::back_inserter(ids), &std::pair<std::uint32_t, std::wstring>::first);
    std::ranges::sort(ids);
    if (std::ranges::adjacent_find(ids) != ids.end())
    {
        return {};
    }
    std::vector<std::byte> out;
    const auto u32 = [&](std::uint32_t value) {
        for (int i = 0; i < 4; ++i)
        {
            out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFU));
        }
    };
    const auto text = [&](std::string_view letters) {
        for (const char c : letters)
        {
            out.push_back(static_cast<std::byte>(c));
        }
    };
    constexpr std::uint32_t kData = 96; // 28 of header, two templates of 16, three fields of 12
    text("GFF V4.0PC  TLK V0.2");
    u32(2);
    u32(kData);
    // The templates: label, field count, where their fields are, size.
    text("TLK ");
    u32(1);
    u32(60);
    u32(4);
    text("STRN");
    u32(2);
    u32(72);
    u32(8);
    // The fields: label, type and flags, offset in the struct.
    u32(kStringList);
    u32(1U | (std::uint32_t{gff4::kList | gff4::kStruct} << 16)); // a list of template 1 in place
    u32(0);
    u32(kStringId);
    u32(static_cast<std::uint32_t>(gff4::Type::U32));
    u32(0);
    u32(kString);
    u32(static_cast<std::uint32_t>(gff4::Type::String));
    u32(4);
    // The top struct, pointing at the list just after it; the list; then the strings.
    u32(4);
    u32(static_cast<std::uint32_t>(strings.size()));
    std::uint32_t at = 8 + (static_cast<std::uint32_t>(strings.size()) * 8);
    for (const auto &[id, string] : strings)
    {
        u32(id);
        u32(at);
        at += 4 + (static_cast<std::uint32_t>(string.size() + 1) * 2);
    }
    for (const auto &[id, string] : strings)
    {
        u32(static_cast<std::uint32_t>(string.size() + 1));
        for (const wchar_t c : string)
        {
            out.push_back(static_cast<std::byte>(c & 0xFF));
            out.push_back(static_cast<std::byte>((c >> 8) & 0xFF));
        }
        out.push_back(std::byte{0});
        out.push_back(std::byte{0});
    }
    return out;
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
