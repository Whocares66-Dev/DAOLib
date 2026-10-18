#include "dao/core/Ncs.h"

#include <array>
#include <bit>
#include <cstring>
#include <format>
#include <utility>

namespace dao::core::ncs
{

namespace
{

// The header (0x00A2CF10): "NCS V", a digit, ".", a digit, and for a
// version other than 1.0 a "B" at 8; the code at 13, where the program
// counter starts. Its big-endian size at 9 is not read. Every operand is
// big-endian (dev/NCS.md, read 2026-10-17 by lib-agent-2).
constexpr std::size_t kCode = 13;

std::optional<std::uint32_t> Big(std::span<const std::byte> file, std::size_t at, std::size_t bytes)
{
    if (at > file.size() || file.size() - at < bytes)
    {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i)
    {
        value = (value << 8U) | std::to_integer<std::uint32_t>(file[at + i]);
    }
    return value;
}

std::int32_t Signed(std::uint32_t value, std::size_t bytes)
{
    const unsigned bits = 32U - (8U * static_cast<unsigned>(bytes));
    return static_cast<std::int32_t>(value << bits) >> bits;
}

constexpr std::array<std::string_view, 0x37> kNames{
    "",
    "CPDOWNSP",
    "RSADD",
    "CPTOPSP",
    "CONST",
    "ACTION",
    "LOGAND",
    "LOGOR",
    "INCOR",
    "EXCOR",
    "BOOLAND",
    "EQUAL",
    "NEQUAL",
    "GEQ",
    "GT",
    "LT",
    "LEQ",
    "SHLEFT",
    "SHRIGHT",
    "USHRIGHT",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "MOD",
    "NEG",
    "COMP",
    "MOVSP",
    "STORE_STATEALL",
    "JMP",
    "JSR",
    "JZ",
    "RETN",
    "DESTRUCT",
    "NOTI",
    "DECISP",
    "INCISP",
    "JNZ",
    "CPDOWNBP",
    "CPTOPBP",
    "DECIBP",
    "INCIBP",
    "SAVEBP",
    "RESTOREBP",
    "STORE_STATE",
    "NOP",
    "WAITFOR",
    "WAITUNTIL",
    "CPSPTOAL",
    "CPBPTOAL",
    "CPALTOPSP",
    "CPALTOPBP",
    "STORE_FULL_STATE",
    "ABORTSCRIPT",
    "STOREWAITUNTILTIMER",
};

// script.ldf's engine structures by number; 6 to 9 it names none of.
constexpr std::array<std::string_view, 10> kStructureNames{
    "event",  "location",    "command",     "effect",      "itemproperty",
    "player", "structure 6", "structure 7", "structure 8", "structure 9",
};
constexpr std::array<std::string_view, 10> kStructurePairs{
    "event, event",
    "location, location",
    "command, command",
    "effect, effect",
    "itemproperty, itemproperty",
    "player, player",
    "structure 6, structure 6",
    "structure 7, structure 7",
    "structure 8, structure 8",
    "structure 9, structure 9",
};
constexpr std::array<std::string_view, 10> kStructureArrays{
    "event[]",  "location[]",    "command[]",     "effect[]",      "itemproperty[]",
    "player[]", "structure 6[]", "structure 7[]", "structure 8[]", "structure 9[]",
};

// The operands' layout of an opcode, after its two bytes.
enum class Layout : std::uint8_t
{
    None,
    OffsetSize, // i32, u16: the copies to and from the stack and the arrays
    Offset,     // i32: MOVSP, the jumps, the increments
    Const,
    Action,     // u16, u8
    Compare,    // none, or a u16 size for two structures
    Destruct,   // u16, i16, u16
    StoreState, // i32, i32
};

Layout LayoutOf(Op op)
{
    switch (op)
    {
    case Op::CpDownSp:
    case Op::CpTopSp:
    case Op::CpDownBp:
    case Op::CpTopBp:
    case Op::CpSpToAl:
    case Op::CpBpToAl:
    case Op::CpAlToPSp:
    case Op::CpAlToPBp:
        return Layout::OffsetSize;
    case Op::MovSp:
    case Op::Jmp:
    case Op::Jsr:
    case Op::Jz:
    case Op::Jnz:
    case Op::DecISp:
    case Op::IncISp:
    case Op::DecIBp:
    case Op::IncIBp:
        return Layout::Offset;
    case Op::Const:
        return Layout::Const;
    case Op::Action:
        return Layout::Action;
    case Op::Equal:
    case Op::NEqual:
        return Layout::Compare;
    case Op::Destruct:
        return Layout::Destruct;
    case Op::StoreState:
        return Layout::StoreState;
    default:
        return Layout::None;
    }
}

// CONST's value by its type (0x00A302EE): an int, a float or an object id
// of four bytes; a string or a resource as a u16 count and its bytes.
bool ReadConst(std::span<const std::byte> file, std::size_t at, Instruction &i)
{
    if (i.type == kInt || i.type == kObject || i.type == kFloat)
    {
        const auto word = Big(file, at, 4);
        if (!word)
        {
            return false;
        }
        i.value = i.type == kFloat ? 0 : static_cast<std::int32_t>(*word);
        i.real = i.type == kFloat ? std::bit_cast<float>(*word) : 0.0F;
        i.length = 6;
        return true;
    }
    if (i.type == kString || i.type == kResource)
    {
        const auto count = Big(file, at, 2);
        if (!count || file.size() - at - 2 < *count)
        {
            return false;
        }
        i.text.resize(*count);
        std::memcpy(i.text.data(), file.data() + at + 2, *count);
        i.length = 4 + *count;
        return true;
    }
    return false;
}

// One instruction at `at`, or nothing where it is unknown or runs past the end.
std::optional<Instruction> Decode(std::span<const std::byte> file, std::size_t at)
{
    if (file.size() - at < 2)
    {
        return std::nullopt;
    }
    const auto code = std::to_integer<std::uint8_t>(file[at]);
    if (code == 0 || code >= kNames.size())
    {
        return std::nullopt;
    }
    Instruction i{.offset = static_cast<std::uint32_t>(at),
                  .opcode = static_cast<Op>(code),
                  .type = std::to_integer<std::uint8_t>(file[at + 1]),
                  .length = 2,
                  .value = 0,
                  .second = 0,
                  .third = 0,
                  .real = 0,
                  .text = {}};
    const std::size_t from = at + 2;
    switch (LayoutOf(i.opcode))
    {
    case Layout::None:
        return i;
    case Layout::OffsetSize: {
        const auto offset = Big(file, from, 4);
        const auto size = Big(file, from + 4, 2);
        if (!offset || !size)
        {
            return std::nullopt;
        }
        i.value = static_cast<std::int32_t>(*offset);
        i.second = static_cast<std::int32_t>(*size);
        i.length = 8;
        return i;
    }
    case Layout::Offset: {
        const auto offset = Big(file, from, 4);
        if (!offset)
        {
            return std::nullopt;
        }
        i.value = static_cast<std::int32_t>(*offset);
        i.length = 6;
        return i;
    }
    case Layout::Const:
        return ReadConst(file, from, i) ? std::optional(std::move(i)) : std::nullopt;
    case Layout::Action: {
        const auto command = Big(file, from, 2);
        const auto count = Big(file, from + 2, 1);
        if (!command || !count)
        {
            return std::nullopt;
        }
        i.value = static_cast<std::int32_t>(*command);
        i.second = static_cast<std::int32_t>(*count);
        i.length = 5;
        return i;
    }
    case Layout::Compare: {
        if (i.type != kStructures)
        {
            return i;
        }
        const auto size = Big(file, from, 2);
        if (!size)
        {
            return std::nullopt;
        }
        i.value = static_cast<std::int32_t>(*size);
        i.length = 4;
        return i;
    }
    case Layout::Destruct: {
        const auto remove = Big(file, from, 2);
        const auto keep = Big(file, from + 2, 2);
        const auto keepSize = Big(file, from + 4, 2);
        if (!remove || !keep || !keepSize)
        {
            return std::nullopt;
        }
        i.value = static_cast<std::int32_t>(*remove);
        i.second = Signed(*keep, 2);
        i.third = static_cast<std::int32_t>(*keepSize);
        i.length = 8;
        return i;
    }
    case Layout::StoreState: {
        const auto first = Big(file, from, 4);
        const auto second = Big(file, from + 4, 4);
        if (!first || !second)
        {
            return std::nullopt;
        }
        i.value = static_cast<std::int32_t>(*first);
        i.second = static_cast<std::int32_t>(*second);
        i.length = 10;
        return i;
    }
    }
    return std::nullopt;
}

bool IsDigit(std::byte b)
{
    const auto c = std::to_integer<char>(b);
    return c >= '0' && c <= '9';
}

} // namespace

std::string_view Name(Op op)
{
    const auto code = static_cast<std::size_t>(op);
    return code < kNames.size() ? kNames[code] : std::string_view();
}

std::string_view TypeName(std::uint8_t type)
{
    switch (type)
    {
    case 0x01:
        return "stack";
    case kInt:
        return "int";
    case kFloat:
        return "float";
    case kString:
        return "string";
    case kObject:
        return "object";
    case 0x20:
        return "int, int";
    case 0x21:
        return "float, float";
    case 0x22:
        return "object, object";
    case 0x23:
        return "string, string";
    case kStructures:
        return "structure, structure";
    case 0x25:
        return "int, float";
    case 0x26:
        return "float, int";
    case 0x3A:
        return "vector, vector";
    case 0x3B:
        return "vector, float";
    case 0x3C:
        return "float, vector";
    case 0x40:
        return "int[]";
    case 0x41:
        return "float[]";
    case 0x42:
        return "string[]";
    case 0x43:
        return "object[]";
    case 0x44:
        return "resource[]";
    case kResource:
        return "resource";
    default:
        break;
    }
    if (type >= 0x10 && type < 0x1A)
    {
        return kStructureNames[type - 0x10U];
    }
    if (type >= 0x30 && type < 0x3A)
    {
        return kStructurePairs[type - 0x30U];
    }
    if (type >= 0x50 && type < 0x5A)
    {
        return kStructureArrays[type - 0x50U];
    }
    return {};
}

std::optional<Script> ScriptOf(std::span<const std::byte> file)
{
    if (file.size() < kCode || std::memcmp(file.data(), "NCS V", 5) != 0 || !IsDigit(file[5]) ||
        std::to_integer<char>(file[6]) != '.' || !IsDigit(file[7]))
    {
        return std::nullopt;
    }
    Script script;
    script.major = static_cast<std::uint8_t>(std::to_integer<char>(file[5]) - '0');
    script.minor = static_cast<std::uint8_t>(std::to_integer<char>(file[7]) - '0');
    if ((script.major != 1 || script.minor != 0) && std::to_integer<char>(file[8]) != 'B')
    {
        return std::nullopt;
    }
    std::size_t at = kCode;
    while (at < file.size())
    {
        auto instruction = Decode(file, at);
        if (!instruction)
        {
            script.whole = false;
            script.stopped = static_cast<std::uint32_t>(at);
            break;
        }
        at += instruction->length;
        script.code.push_back(std::move(*instruction));
    }
    return script;
}

std::string Disassemble(const Script &script)
{
    std::string out;
    for (const Instruction &i : script.code)
    {
        out += std::format("{:08x}  {}", i.offset, Name(i.opcode));
        // STORE_STATE's and STORE_STATEALL's type byte is how far on the resumed code starts.
        if (i.opcode == Op::StoreState || i.opcode == Op::StoreStateAll)
        {
            out += std::format(" resume -> {:08x}", i.offset + i.type);
        }
        else if (const auto type = TypeName(i.type); !type.empty() && i.type != 0x01)
        {
            out += std::format(" {}", type);
        }
        switch (LayoutOf(i.opcode))
        {
        case Layout::OffsetSize:
            out += std::format(" {}, {}", i.value, i.second);
            break;
        case Layout::Offset:
            if (i.opcode == Op::Jmp || i.opcode == Op::Jsr || i.opcode == Op::Jz || i.opcode == Op::Jnz)
            {
                out += std::format(" {:+} -> {:08x}", i.value, i.Target());
            }
            else
            {
                out += std::format(" {}", i.value);
            }
            break;
        case Layout::Const:
            if (i.type == kFloat)
            {
                out += std::format(" {}", i.real);
            }
            else if (i.type == kString || i.type == kResource)
            {
                out += std::format(" \"{}\"", i.text);
            }
            else
            {
                out += std::format(" {}", i.value);
            }
            break;
        case Layout::Action:
            out += std::format(" {} ({} arguments)", i.value, i.second);
            break;
        case Layout::Compare:
            if (i.type == kStructures)
            {
                out += std::format(" {}", i.value);
            }
            break;
        case Layout::Destruct:
            out += std::format(" {}, {}, {}", i.value, i.second, i.third);
            break;
        case Layout::StoreState:
            out += std::format(" {}, {}", i.value, i.second);
            break;
        case Layout::None:
            break;
        }
        out += '\n';
    }
    if (!script.whole)
    {
        out += std::format("{:08x}  (not decoded)\n", script.stopped);
    }
    return out;
}

} // namespace dao::core::ncs
