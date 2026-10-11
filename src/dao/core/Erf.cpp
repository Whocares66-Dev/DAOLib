#include "dao/core/Erf.h"

#include "dao/core/Bytes.h"

#include <array>
#include <cstring>

namespace dao::core::erf
{

namespace
{

constexpr std::size_t kNameUnits = 32;
constexpr std::array<char16_t, 8> kMagic{u'E', u'R', u'F', u' ', u'V', u'2', u'.', u'0'};

} // namespace

std::optional<std::uint32_t> EntryCount(std::span<const std::byte> header)
{
    if (header.size() < kHeaderSize || std::memcmp(header.data(), kMagic.data(), sizeof kMagic) != 0)
    {
        return std::nullopt;
    }
    return ReadAt<std::uint32_t>(header, sizeof kMagic);
}

std::vector<Entry> Entries(std::span<const std::byte> start)
{
    std::vector<Entry> entries;
    const std::optional<std::uint32_t> count = EntryCount(start);
    if (!count)
    {
        return entries;
    }
    const std::size_t whole = (start.size() - kHeaderSize) / kEntrySize;
    entries.reserve(std::min<std::size_t>(*count, whole));
    for (std::size_t i = 0; i < *count && i < whole; ++i)
    {
        const std::size_t at = kHeaderSize + (i * kEntrySize);
        Entry entry;
        for (std::size_t unit = 0; unit < kNameUnits; ++unit)
        {
            const char16_t letter = ReadAt<char16_t>(start, at + (unit * sizeof(char16_t))).value_or(0);
            if (letter == 0)
            {
                break;
            }
            entry.name.push_back(static_cast<wchar_t>(letter));
        }
        entry.offset = ReadAt<std::uint32_t>(start, at + (kNameUnits * sizeof(char16_t))).value_or(0);
        entry.size = ReadAt<std::uint32_t>(start, at + (kNameUnits * sizeof(char16_t)) + 4).value_or(0);
        entries.push_back(std::move(entry));
    }
    return entries;
}

} // namespace dao::core::erf
