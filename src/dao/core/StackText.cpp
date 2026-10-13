#include "dao/core/StackText.h"

#include <array>
#include <cstring>
#include <string_view>

namespace dao::core
{

namespace
{

bool Printable(wchar_t unit)
{
    return unit >= 0x20 && unit < 0x7F;
}

// Past this, a length is not a name's: a word that happens to sit where a
// string's length would.
constexpr std::uint32_t kMostCapacity = 0x10000;

} // namespace

std::optional<std::string> PrintableWide(std::span<const wchar_t> units, std::size_t least)
{
    std::string text;
    for (const wchar_t unit : units)
    {
        if (unit == L'\0')
        {
            return text.size() >= least ? std::optional{text} : std::nullopt;
        }
        if (!Printable(unit))
        {
            return std::nullopt;
        }
        text.push_back(static_cast<char>(unit));
    }
    return std::nullopt;
}

std::optional<StackWString> WStringIn(std::span<const std::uint32_t, 7> words)
{
    const std::uint32_t size = words[5];
    const std::uint32_t capacity = words[6];
    if (capacity < kWStringInlineCapacity || capacity > kMostCapacity || size < 3 || size > capacity)
    {
        return std::nullopt;
    }
    StackWString found;
    found.size = size;
    if (capacity > kWStringInlineCapacity)
    {
        found.pointer = words[1];
        if (found.pointer == 0)
        {
            return std::nullopt;
        }
        return found;
    }
    // Inline: the four words after the first are eight UTF-16 units.
    std::array<wchar_t, kWStringInlineCapacity + 1> units{};
    std::memcpy(units.data(), &words[1], sizeof units);
    const auto text = PrintableWide(units, 3);
    if (!text || text->size() != size)
    {
        return std::nullopt;
    }
    found.text = *text;
    return found;
}

bool IsGff4(std::span<const std::byte> head)
{
    constexpr std::string_view kV40 = "GFF V4.0";
    constexpr std::string_view kV41 = "GFF V4.1";
    if (head.size() < kV40.size())
    {
        return false;
    }
    const std::string_view start(reinterpret_cast<const char *>(head.data()), kV40.size());
    return start == kV40 || start == kV41;
}

} // namespace dao::core
