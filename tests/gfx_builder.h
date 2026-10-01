// Building a Flash movie by hand for the tests: actions as a compiler
// writes them, tags, and a movie of tags. The tests of dao/core/GfxMovie.h
// and dao/core/OptionsMovie.h lay out their movies with this, as the
// game's optionsmenu.gfx is laid out.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace dao::test::gfx
{

using Bytes = std::vector<std::byte>;

inline void Put(Bytes &out, std::uint32_t value, std::size_t width)
{
    for (std::size_t i = 0; i < width; ++i)
    {
        out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFU));
    }
}

inline void PutText(Bytes &out, std::string_view text)
{
    for (const char c : text)
    {
        out.push_back(static_cast<std::byte>(c));
    }
    out.push_back(std::byte{0});
}

inline void Add(Bytes &out, const Bytes &more)
{
    out.insert(out.end(), more.begin(), more.end());
}

inline std::uint32_t Get(const Bytes &bytes, std::size_t at, std::size_t width)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
    {
        value |= std::to_integer<std::uint32_t>(bytes[at + i]) << (8 * i);
    }
    return value;
}

// Actions, as a compiler writes them.
inline Bytes Simple(int code)
{
    return {static_cast<std::byte>(code)};
}

inline Bytes Record(int code, const Bytes &body)
{
    Bytes out{static_cast<std::byte>(code)};
    Put(out, static_cast<std::uint32_t>(body.size()), 2);
    Add(out, body);
    return out;
}

inline Bytes Pool(std::initializer_list<std::string_view> strings)
{
    Bytes body;
    Put(body, static_cast<std::uint32_t>(strings.size()), 2);
    for (const std::string_view s : strings)
    {
        PutText(body, s);
    }
    return Record(0x88, body);
}

// function(argument); with both strings from the pool, as the game's are.
inline Bytes PooledCall(int argument, int function)
{
    Bytes body;
    body.push_back(std::byte{8});
    body.push_back(static_cast<std::byte>(argument));
    body.push_back(std::byte{7});
    Put(body, 1, 4);
    body.push_back(std::byte{8});
    body.push_back(static_cast<std::byte>(function));
    Bytes out = Record(0x96, body);
    Add(out, Simple(0x3D));
    Add(out, Simple(0x17));
    return out;
}

// An anonymous function2 with no parameters around `body`.
inline Bytes Function2(const Bytes &body)
{
    Bytes record;
    PutText(record, "");
    Put(record, 0, 2); // parameters
    record.push_back(std::byte{1});
    Put(record, 0, 2); // flags
    Put(record, static_cast<std::uint32_t>(body.size()), 2);
    Bytes out = Record(0x8E, record);
    Add(out, body);
    return out;
}

// A function with a name and one parameter around `body`.
inline Bytes Function(std::string_view name, const Bytes &body)
{
    Bytes record;
    PutText(record, name);
    Put(record, 1, 2);
    PutText(record, "a_sText");
    Put(record, static_cast<std::uint32_t>(body.size()), 2);
    Bytes out = Record(0x9B, record);
    Add(out, body);
    return out;
}

inline Bytes Branch(int code, int offset)
{
    Bytes body;
    Put(body, static_cast<std::uint16_t>(static_cast<std::int16_t>(offset)), 2);
    return Record(code, body);
}

inline Bytes LongTag(unsigned code, const Bytes &body)
{
    Bytes out;
    Put(out, code << 6U | 0x3FU, 2);
    Put(out, static_cast<std::uint32_t>(body.size()), 4);
    Add(out, body);
    return out;
}

inline Bytes ShortTag(unsigned code, const Bytes &body)
{
    Bytes out;
    Put(out, code << 6U | static_cast<std::uint32_t>(body.size()), 2);
    Add(out, body);
    return out;
}

// A movie of these tags and an End tag: "GFX", version 8, a rectangle of
// 15 bits a number (9 bytes), a rate and a count.
inline Bytes Movie(const Bytes &tags)
{
    Bytes out;
    PutText(out, "GFX");
    out.back() = std::byte{8};
    Put(out, 0, 4);
    out.push_back(static_cast<std::byte>(15 << 3));
    out.insert(out.end(), 8, std::byte{0});
    Put(out, 0x1E00, 2);
    Put(out, 1, 2);
    Add(out, tags);
    Put(out, 0, 2);
    const auto length = static_cast<std::uint32_t>(out.size());
    for (std::size_t i = 0; i < 4; ++i)
    {
        out[4 + i] = static_cast<std::byte>((length >> (8 * i)) & 0xFFU);
    }
    return out;
}

// Where a movie made by Movie has its first tag: after the header and a
// rectangle of 15 bits a number.
inline constexpr std::size_t kFirstTag = 8 + 9 + 4;

} // namespace dao::test::gfx
