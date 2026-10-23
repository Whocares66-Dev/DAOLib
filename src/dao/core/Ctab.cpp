#include "dao/core/Ctab.h"

#include "dao/core/Bytes.h"

#include <algorithm>
#include <iterator>

namespace dao::core::ctab
{

namespace
{

constexpr std::uint32_t kCommentOpcode = 0xFFFE; // a comment token's low word; its high word is its length in dwords
constexpr std::uint32_t kEndToken = 0x0000FFFF;
constexpr std::uint32_t kFourCc = 0x42415443; // "CTAB"
constexpr std::size_t kHeaderSize = 0x1C;     // D3DXSHADER_CONSTANTTABLE
constexpr std::size_t kInfoSize = 0x14;       // D3DXSHADER_CONSTANTINFO
constexpr std::size_t kTypeSize = 0x10;       // D3DXSHADER_TYPEINFO

// A string in the table at an offset: up to its NUL, none for one that runs past the table.
std::optional<std::string> StringAt(std::span<const std::byte> table, std::uint32_t offset)
{
    if (offset >= table.size())
    {
        return std::nullopt;
    }
    const auto rest = table.subspan(offset);
    const auto end = std::ranges::find(rest, std::byte{0});
    if (end == rest.end())
    {
        return std::nullopt;
    }
    std::string text;
    std::transform(rest.begin(), end, std::back_inserter(text), [](std::byte b) { return static_cast<char>(b); });
    return text;
}

// D3DXSHADER_TYPEINFO's first five words, as the table holds them.
struct TypeInfo
{
    std::uint16_t kind;
    std::uint16_t type;
    std::uint16_t rows;
    std::uint16_t columns;
    std::uint16_t elements;
};

std::optional<Constant> ConstantAt(std::span<const std::byte> table, std::size_t at)
{
    const auto name = ReadAt<std::uint32_t>(table, at);
    const auto set = ReadAt<std::uint16_t>(table, at + 4);
    const auto index = ReadAt<std::uint16_t>(table, at + 6);
    const auto count = ReadAt<std::uint16_t>(table, at + 8);
    const auto type = ReadAt<std::uint32_t>(table, at + 12);
    // The whole of the type's record must be in the table, though only its first five words are read.
    const bool typeFits = type && *type <= table.size() && table.size() - *type >= kTypeSize;
    const auto info = typeFits ? ReadAt<TypeInfo>(table, *type) : std::nullopt;
    if (!name || !set || !index || !count || !info || *set > static_cast<std::uint16_t>(RegisterSet::Sampler))
    {
        return std::nullopt;
    }
    auto text = StringAt(table, *name);
    if (!text)
    {
        return std::nullopt;
    }
    Constant constant;
    constant.name = std::move(*text);
    constant.set = static_cast<RegisterSet>(*set);
    constant.index = *index;
    constant.count = *count;
    constant.kind = static_cast<Class>(info->kind);
    constant.type = info->type;
    constant.rows = info->rows;
    constant.columns = info->columns;
    constant.elements = info->elements;
    return constant;
}

std::optional<Table> TableOf(std::span<const std::byte> table, std::uint32_t version)
{
    const auto size = ReadAt<std::uint32_t>(table, 0);
    const auto creator = ReadAt<std::uint32_t>(table, 4);
    const auto count = ReadAt<std::uint32_t>(table, 12);
    const auto infos = ReadAt<std::uint32_t>(table, 16);
    const auto target = ReadAt<std::uint32_t>(table, 24);
    if (!size || *size != kHeaderSize || !creator || !count || !infos || !target || *infos > table.size() ||
        (table.size() - *infos) / kInfoSize < *count)
    {
        return std::nullopt;
    }
    Table result;
    result.version = version;
    result.creator = StringAt(table, *creator).value_or("");
    result.target = StringAt(table, *target).value_or("");
    result.constants.reserve(*count);
    for (std::uint32_t i = 0; i < *count; ++i)
    {
        auto constant = ConstantAt(table, *infos + (i * kInfoSize));
        if (!constant)
        {
            return std::nullopt;
        }
        result.constants.push_back(std::move(*constant));
    }
    return result;
}

} // namespace

std::optional<Table> Read(std::span<const std::byte> bytecode)
{
    const auto version = ReadAt<std::uint32_t>(bytecode, 0);
    if (!version)
    {
        return std::nullopt;
    }
    // The comments come straight after the version token; the first instruction ends the search.
    std::size_t at = sizeof(std::uint32_t);
    for (auto token = ReadAt<std::uint32_t>(bytecode, at); token && *token != kEndToken;
         token = ReadAt<std::uint32_t>(bytecode, at))
    {
        if ((*token & 0xFFFF) != kCommentOpcode)
        {
            return std::nullopt;
        }
        const std::size_t length = (*token >> 16) * sizeof(std::uint32_t);
        const std::size_t body = at + sizeof(std::uint32_t);
        if (body > bytecode.size() || bytecode.size() - body < length)
        {
            return std::nullopt;
        }
        if (length >= sizeof(std::uint32_t) && ReadAt<std::uint32_t>(bytecode, body) == kFourCc)
        {
            return TableOf(bytecode.subspan(body + sizeof(std::uint32_t), length - sizeof(std::uint32_t)), *version);
        }
        at = body + length;
    }
    return std::nullopt;
}

const Constant *At(const Table &table, RegisterSet set, std::uint16_t reg)
{
    const auto found = std::ranges::find_if(table.constants, [&](const Constant &constant) {
        return constant.set == set && reg >= constant.index && reg - constant.index < constant.count;
    });
    return found != table.constants.end() ? &*found : nullptr;
}

} // namespace dao::core::ctab
