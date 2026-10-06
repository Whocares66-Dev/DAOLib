// Building a GFF4 file by hand for the tests: bytes written at offsets,
// and strings and number lists appended to the data block, each answering
// its offset from the data's start. The tests of dao/core/Gff4.h and the
// readers over it lay out their files with this, as the game's are laid out
// (tools/gff4.py --templates on one).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace dao::test
{

class Builder
{
  public:
    std::vector<std::byte> bytes;

    void Text(std::size_t at, std::string_view text)
    {
        Grow(at + text.size());
        std::memcpy(bytes.data() + at, text.data(), text.size());
    }
    void U32(std::size_t at, std::uint32_t value)
    {
        Grow(at + 4);
        std::memcpy(bytes.data() + at, &value, 4);
    }
    void U8(std::size_t at, std::uint8_t value)
    {
        Grow(at + 1);
        bytes[at] = static_cast<std::byte>(value);
    }
    // Appends to the data block: a string, returning its data offset.
    std::uint32_t String(std::u16string_view text)
    {
        const std::size_t at = bytes.size();
        U32(at, static_cast<std::uint32_t>(text.size() + 1));
        for (std::size_t i = 0; i <= text.size(); ++i)
        {
            const std::uint16_t unit = i < text.size() ? text[i] : 0;
            Grow(at + 4 + (2 * i) + 2);
            std::memcpy(bytes.data() + at + 4 + (2 * i), &unit, 2);
        }
        return static_cast<std::uint32_t>(at - data);
    }
    std::uint32_t Numbers(const std::vector<std::uint32_t> &values)
    {
        const std::size_t at = bytes.size();
        U32(at, static_cast<std::uint32_t>(values.size()));
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            U32(at + 4 + (4 * i), values[i]);
        }
        return static_cast<std::uint32_t>(at - data);
    }

    std::size_t data = 0;

  private:
    void Grow(std::size_t size)
    {
        if (bytes.size() < size)
        {
            bytes.resize(size);
        }
    }
};

} // namespace dao::test
