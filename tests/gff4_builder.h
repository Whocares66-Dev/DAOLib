// Building a GFF4 file by hand for the tests: bytes written at offsets,
// and strings and number lists appended to the data block, each answering
// its offset from the data's start. The tests of dao/core/Gff4.h and the
// readers over it lay out their files with this, as the game's are laid out
// (tools/gff4.py --templates on one).
#pragma once

#include "dao/core/Gff4.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
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

// Field and template descriptions, and a file laid out from them as the game's are
// (tools/gff4.py --templates on one): the header and templates by Start, then structs,
// lists and strings appended to the data block.

struct FieldSpec
{
    std::uint32_t label;
    std::uint16_t type;
    std::uint16_t flags;
    std::uint32_t offset;
};

struct TemplateSpec
{
    const char *label;
    std::uint32_t size;
    std::vector<FieldSpec> fields;
};

inline constexpr std::uint16_t kGeneric = 0xFFFF;

// The header and the templates; the data starts where this ends.
inline Builder Start(const char *type, const std::vector<TemplateSpec> &templates)
{
    Builder b;
    b.Text(0, std::string("GFF V4.0PC  ") + type + "V1.0");
    b.U32(20, static_cast<std::uint32_t>(templates.size()));
    std::size_t fields = 28 + (16 * templates.size());
    for (std::size_t i = 0; i < templates.size(); ++i)
    {
        const std::size_t at = 28 + (16 * i);
        b.Text(at, templates[i].label);
        b.U32(at + 4, static_cast<std::uint32_t>(templates[i].fields.size()));
        b.U32(at + 8, static_cast<std::uint32_t>(fields));
        b.U32(at + 12, templates[i].size);
        for (const FieldSpec &f : templates[i].fields)
        {
            b.U32(fields, f.label);
            b.U32(fields + 4, f.type | (std::uint32_t{f.flags} << 16));
            b.U32(fields + 8, f.offset);
            fields += 12;
        }
    }
    b.data = fields;
    b.U32(24, static_cast<std::uint32_t>(b.data));
    return b;
}

inline void Float(Builder &b, std::size_t at, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    b.U32(at, bits);
}

inline void Floats(Builder &b, std::size_t at, const std::vector<float> &values)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        Float(b, at + (4 * i), values[i]);
    }
}

// Room for a struct at the data's end: where it is.
inline std::size_t Struct(Builder &b, std::size_t size)
{
    const std::size_t at = b.bytes.size();
    b.U8(at + size - 1, 0);
    return at;
}

// A list of bytes at the data's end: its data offset.
inline std::uint32_t Bytes(Builder &b, const std::vector<std::uint8_t> &bytes)
{
    const std::size_t at = b.bytes.size();
    b.U32(at, static_cast<std::uint32_t>(bytes.size()));
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        b.U8(at + 4 + i, bytes[i]);
    }
    return static_cast<std::uint32_t>(at - b.data);
}

// A generic list of references to structs, each (template, place): its data offset.
inline std::uint32_t Children(Builder &b, const std::vector<std::pair<std::uint16_t, std::size_t>> &structs)
{
    const std::size_t at = b.bytes.size();
    b.U32(at, static_cast<std::uint32_t>(structs.size()));
    for (std::size_t i = 0; i < structs.size(); ++i)
    {
        b.U32(at + 4 + (8 * i),
              structs[i].first | (std::uint32_t{dao::core::gff4::kStruct | dao::core::gff4::kReference} << 16));
        b.U32(at + 8 + (8 * i), static_cast<std::uint32_t>(structs[i].second - b.data));
    }
    return static_cast<std::uint32_t>(at - b.data);
}

template <class T> void Append(std::vector<std::uint8_t> &to, const T &value)
{
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
    to.insert(to.end(), bytes, bytes + sizeof(T));
}

} // namespace dao::test
