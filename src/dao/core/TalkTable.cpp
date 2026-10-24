#include "dao/core/TalkTable.h"

#include "dao/core/Gff4.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <string_view>
#include <utility>

namespace dao::core
{

// An id with no text, its string's offset 0xFFFFFFFF, reads as an empty text (gff4's StringAt),
// as the game finds it, hiding any later table's (CTlkTable's find, 0x004E47F0, takes the text
// through 0x00456140, which gives the constant 0x00ADEF14 for that offset; read 2026-10-24, DAOLib
// #106). An empty slot of the list, of id kEmptySlot, holds no string.
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
        if (id && text && *id != kEmptySlot)
        {
            table.strings_.emplace_back(*id, std::move(*text));
        }
    }
    // By id, the last of any repeated id kept, as a map would keep it.
    std::ranges::stable_sort(table.strings_, {}, &std::pair<std::uint32_t, std::wstring>::first);
    return table;
}

// CTlkTable's find (0x004E47F0, read 2026-10-24 for DAOLib #106): (id << 15) + ~id, then three
// shift-and-xor steps with a multiply by 5 and by 0x809, in 32 bits.
std::uint32_t TalkTableFile::Hash(std::uint32_t id)
{
    std::uint32_t a = (id << 15) + ~id;
    std::uint32_t c = (a >> 12) ^ a;
    a = c * 5;
    c = (a >> 4) ^ a;
    c *= 0x809;
    return (c >> 16) ^ c;
}

// Laid out as every one of the game's 1,501 tables is, each rebuilt byte for byte from its ids and
// texts by these rules (2026-10-24, DAOLib #106; tests/test_talktable_roundtrip.cpp): "GFF V4.0",
// "PC  ", "TLK ", "V0.2", two templates and the data at 96; the templates "TLK " (4 bytes: its
// list of STRN held in place) and "STRN" (8 bytes: the id, and its string by offset). The data is
// the top struct, then the list, an open hash table of a third more slots than strings (the count
// times four thirds, rounded down), the ids put in from the lowest by the game's own search, each
// from its Hash's slot one on until a free one, a free slot's id and offset 0xFFFFFFFF; then the
// texts, in the slots' order, each a count of UTF-16 units and the units, a NUL last, starting on
// four bytes after bytes of 0xFF, a text met before not written again but pointed at, an empty
// text not written and its offset 0xFFFFFFFF. A table of no strings has no list: the top struct's
// offset is 0xFFFFFFFF.
std::vector<std::byte> TalkTableFile::Write(std::span<const std::pair<std::uint32_t, std::wstring>> strings)
{
    using Entry = std::pair<std::uint32_t, const std::wstring *>;
    std::vector<Entry> sorted;
    sorted.reserve(strings.size());
    for (const auto &[id, text] : strings)
    {
        sorted.emplace_back(id, &text);
    }
    std::ranges::sort(sorted, {}, &Entry::first);
    if (std::ranges::adjacent_find(sorted, {}, &Entry::first) != sorted.end() ||
        std::ranges::any_of(sorted, [](const Entry &entry) { return entry.first == kEmptySlot; }))
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
    const auto put = [&](std::size_t at, std::uint32_t value) {
        for (std::size_t i = 0; i < 4; ++i)
        {
            out[at + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFU);
        }
    };
    const auto text = [&](std::string_view letters) {
        for (const char c : letters)
        {
            out.push_back(static_cast<std::byte>(c));
        }
    };
    constexpr std::uint32_t kData = 96; // 28 of header, two templates of 16, three fields of 12
    constexpr std::uint32_t kNone = 0xFFFFFFFF;
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
    if (sorted.empty())
    {
        u32(kNone);
        return out;
    }
    // The top struct, pointing at the list just after it; the list's slots; then the texts.
    u32(4);
    const std::size_t count = sorted.size() * 4 / 3;
    std::vector<Entry> slots(count, Entry{kEmptySlot, nullptr});
    for (const Entry &entry : sorted)
    {
        std::size_t at = Hash(entry.first) % count;
        while (slots[at].first != kEmptySlot)
        {
            at = (at + 1) % count;
        }
        slots[at] = entry;
    }
    u32(static_cast<std::uint32_t>(count));
    const std::size_t list = out.size();
    out.resize(list + (8 * count));
    std::map<std::wstring_view, std::uint32_t> written;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto &[id, string] = slots[i];
        std::uint32_t offset = kNone;
        if (string != nullptr && !string->empty())
        {
            const auto [found, added] = written.try_emplace(*string, 0);
            if (added)
            {
                out.resize((out.size() + 3) & ~std::size_t{3}, std::byte{0xFF});
                found->second = static_cast<std::uint32_t>(out.size() - kData);
                u32(static_cast<std::uint32_t>(string->size() + 1));
                for (const wchar_t c : *string)
                {
                    out.push_back(static_cast<std::byte>(c & 0xFF));
                    out.push_back(static_cast<std::byte>((c >> 8) & 0xFF));
                }
                out.push_back(std::byte{0});
                out.push_back(std::byte{0});
            }
            offset = found->second;
        }
        put(list + (8 * i), id);
        put(list + (8 * i) + 4, offset);
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
