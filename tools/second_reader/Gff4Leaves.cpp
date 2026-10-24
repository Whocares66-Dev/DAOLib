// dao_gff4_leaves: core's GFF4 reader over a list of files, each file's values as the tokens
// tools/second_reader.py's walk of tools/gff4.py gives (its canonical()), hashed: the two readers
// compared over the install, gff4.py the second reader of core's (DAOLib #106, step 6). Its first
// run found both reading a generic string by reference one hop too far (dao/core/Gff4.cpp).
//
//   dao_gff4_leaves <sample.txt> <out.txt> [where]
//
// sample.txt: a file a line, "archive|name" for one in an archive, "|path" for one on its own.
// out.txt: "where<TAB>hash<TAB>tokens" a line. With where, that file's tokens printed as well.
#include "dao/core/Erf.h"
#include "dao/core/Gff4.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gff4 = dao::core::gff4;
namespace archive = dao::core::erf;

namespace
{

using T = gff4::Type;

std::string Text(const std::wstring &s)
{
    std::string out;
    for (const wchar_t c : s)
    {
        if (c == L'\\')
        {
            out += "\\\\";
        }
        else if (c >= 0x20 && c < 0x7F)
        {
            out += static_cast<char>(c);
        }
        else
        {
            char hex[8];
            std::snprintf(hex, sizeof hex, "\\u%04x", static_cast<unsigned>(c) & 0xFFFF);
            out += hex;
        }
    }
    return out;
}

std::string Real(double v)
{
    if (v != v)
    {
        return "fnan"; // Python prints a NaN of either sign so
    }
    char text[64];
    std::snprintf(text, sizeof text, "f%.17g", v);
    return text;
}

std::optional<std::string> Whole(const gff4::Struct &s, std::uint32_t label, std::uint16_t type)
{
    const auto as = [&](auto value) -> std::optional<std::string> {
        return value ? std::optional("i" + std::to_string(*value)) : std::nullopt;
    };
    switch (static_cast<T>(type))
    {
    case T::U8:
        return as(s.Get<std::uint8_t>(label));
    case T::S8:
        return as(s.Get<std::int8_t>(label));
    case T::U16:
        return as(s.Get<std::uint16_t>(label));
    case T::S16:
        return as(s.Get<std::int16_t>(label));
    case T::U32:
        return as(s.Get<std::uint32_t>(label));
    case T::S32:
        return as(s.Get<std::int32_t>(label));
    case T::U64:
        return as(s.Get<std::uint64_t>(label));
    case T::S64:
        return as(s.Get<std::int64_t>(label));
    default:
        return std::nullopt;
    }
}

bool IsFloats(std::uint16_t type)
{
    return type == 10 || type == 12 || type == 13 || type == 15 || type == 16;
}

std::size_t FloatCount(std::uint16_t type)
{
    switch (type)
    {
    case 10:
        return 3;
    case 16:
        return 16;
    default:
        return 4;
    }
}

// A list of numbers' elements as the Python walk gives them: whole numbers or floats, a vector's
// floats a list of its own.
void ListOfNumbers(const gff4::Struct &s, const gff4::Field &f, std::vector<std::string> &out)
{
    const auto type = static_cast<T>(f.type);
    const std::span<const std::byte> bytes = s.ListBytes(f.label, type);
    std::size_t size = 0;
    switch (f.type)
    {
    case 0:
    case 1:
        size = 1;
        break;
    case 2:
    case 3:
        size = 2;
        break;
    case 4:
    case 5:
    case 8:
        size = 4;
        break;
    case 6:
    case 7:
    case 9:
        size = 8;
        break;
    case 10:
    case 12:
    case 13:
    case 15:
        size = 16;
        break;
    case 16:
        size = 64;
        break;
    default:
        out.push_back("L?type" + std::to_string(f.type));
        return;
    }
    const std::size_t count = bytes.size() / size;
    out.push_back("L" + std::to_string(count));
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::byte *at = bytes.data() + (i * size);
        const auto read = [&](auto value) {
            std::memcpy(&value, at, sizeof value);
            return value;
        };
        switch (f.type)
        {
        case 0:
            out.push_back("i" + std::to_string(read(std::uint8_t{})));
            break;
        case 1:
            out.push_back("i" + std::to_string(read(std::int8_t{})));
            break;
        case 2:
            out.push_back("i" + std::to_string(read(std::uint16_t{})));
            break;
        case 3:
            out.push_back("i" + std::to_string(read(std::int16_t{})));
            break;
        case 4:
            out.push_back("i" + std::to_string(read(std::uint32_t{})));
            break;
        case 5:
            out.push_back("i" + std::to_string(read(std::int32_t{})));
            break;
        case 6:
            out.push_back("i" + std::to_string(read(std::uint64_t{})));
            break;
        case 7:
            out.push_back("i" + std::to_string(read(std::int64_t{})));
            break;
        case 8:
            out.push_back(Real(read(float{})));
            break;
        case 9:
            out.push_back(Real(read(double{})));
            break;
        default: {
            const std::size_t n = FloatCount(f.type);
            out.push_back("L" + std::to_string(n));
            for (std::size_t k = 0; k < n; ++k)
            {
                float v = 0;
                std::memcpy(&v, at + (4 * k), 4);
                out.push_back(Real(v));
            }
        }
        }
    }
}

void Walk(const gff4::Struct &s, int depth, std::vector<std::string> &out);

// A generic field holding one value: a struct walked, else the value by whichever type it is.
void Generic(const gff4::Struct &s, std::uint32_t label, int depth, std::vector<std::string> &out)
{
    if (const auto child = s.Child(label))
    {
        Walk(*child, depth - 1, out);
        return;
    }
    if (const auto text = s.String(label))
    {
        out.push_back("s" + Text(*text));
        return;
    }
    if (const auto tlk = s.Tlk(label))
    {
        out.push_back("t" + std::to_string(tlk->strref));
        return;
    }
    for (const std::uint16_t type : {0, 1, 2, 3, 4, 5, 6, 7})
    {
        if (auto value = Whole(s, label, type))
        {
            out.push_back(*value);
            return;
        }
    }
    if (const auto f = s.Get<float>(label))
    {
        out.push_back(Real(*f));
        return;
    }
    if (const auto d = s.Get<double>(label))
    {
        out.push_back(Real(*d));
        return;
    }
    if (const auto floats = s.Floats(label); !floats.empty())
    {
        out.push_back("L" + std::to_string(floats.size()));
        for (const float f : floats)
        {
            out.push_back(Real(f));
        }
        return;
    }
    out.push_back("none");
}

void Field(const gff4::Struct &s, const gff4::Field &f, int depth, std::vector<std::string> &out)
{
    const bool list = (f.flags & gff4::kList) != 0;
    const bool structure = (f.flags & gff4::kStruct) != 0;
    if (list && (structure || f.type == static_cast<std::uint16_t>(T::Generic)))
    {
        const std::vector<gff4::Struct> items = s.Structs(f.label);
        out.push_back("L" + std::to_string(items.size()));
        for (const gff4::Struct &item : items)
        {
            Walk(item, depth - 1, out);
        }
    }
    else if (list && f.type == static_cast<std::uint16_t>(T::String))
    {
        const std::vector<std::wstring> strings = s.Strings(f.label);
        out.push_back("L" + std::to_string(strings.size()));
        for (const std::wstring &text : strings)
        {
            out.push_back("s" + Text(text));
        }
    }
    else if (list)
    {
        ListOfNumbers(s, f, out);
    }
    else if (structure)
    {
        if (const auto child = s.Child(f.label))
        {
            Walk(*child, depth - 1, out);
        }
        else
        {
            out.push_back("none");
        }
    }
    else if (f.type == static_cast<std::uint16_t>(T::Generic))
    {
        Generic(s, f.label, depth, out);
    }
    else if (f.type == static_cast<std::uint16_t>(T::TlkString))
    {
        const auto tlk = s.Tlk(f.label);
        out.push_back(tlk ? "t" + std::to_string(tlk->strref) : "none");
    }
    else if (f.type == static_cast<std::uint16_t>(T::String))
    {
        out.push_back("s" + Text(s.String(f.label).value_or(L"")));
    }
    else if (f.type == static_cast<std::uint16_t>(T::F32))
    {
        const auto v = s.Get<float>(f.label);
        out.push_back(v ? Real(*v) : "none");
    }
    else if (f.type == static_cast<std::uint16_t>(T::F64))
    {
        const auto v = s.Get<double>(f.label);
        out.push_back(v ? Real(*v) : "none");
    }
    else if (IsFloats(f.type))
    {
        const std::vector<float> floats = s.Floats(f.label);
        out.push_back("L" + std::to_string(floats.size()));
        for (const float v : floats)
        {
            out.push_back(Real(v));
        }
    }
    else
    {
        out.push_back(Whole(s, f.label, f.type).value_or("none"));
    }
}

void Walk(const gff4::Struct &s, int depth, std::vector<std::string> &out)
{
    if (depth < 0)
    {
        out.push_back("...");
        return;
    }
    out.push_back("T" + s.Label());
    for (const gff4::Field &f : s.Shape().fields)
    {
        out.push_back("#" + std::to_string(f.label));
        if (depth <= 0)
        {
            out.push_back("...");
            continue;
        }
        Field(s, f, depth, out);
    }
}

std::uint64_t Fnv(const std::vector<std::string> &tokens)
{
    std::uint64_t h = 0xCBF29CE484222325ULL;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (i > 0)
        {
            h = (h ^ static_cast<unsigned char>('\n')) * 0x100000001B3ULL;
        }
        for (const char c : tokens[i])
        {
            h = (h ^ static_cast<unsigned char>(c)) * 0x100000001B3ULL;
        }
    }
    return h;
}

std::optional<std::vector<std::byte>> Bytes(const std::string &where, std::map<std::string, archive::Archive> &open)
{
    const auto bar = where.find('|');
    const std::string container = where.substr(0, bar);
    const std::string name = where.substr(bar + 1);
    if (container.empty())
    {
        std::ifstream in(std::filesystem::u8path(name), std::ios::binary);
        const std::vector<char> chars(std::istreambuf_iterator<char>(in), {});
        std::vector<std::byte> bytes(chars.size());
        std::memcpy(bytes.data(), chars.data(), chars.size());
        return bytes;
    }
    auto found = open.find(container);
    if (found == open.end())
    {
        open.clear(); // one archive open at a time: the sample is in archive order
        auto opened = archive::Archive::Open(std::filesystem::u8path(container));
        if (!opened)
        {
            return std::nullopt;
        }
        found = open.emplace(container, std::move(*opened)).first;
    }
    return found->second.Read(std::wstring(name.begin(), name.end()));
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "dao_gff4_leaves <sample.txt> <out.txt> [where]\n";
        return 2;
    }
    std::ifstream sample(argv[1]);
    std::ofstream out(argv[2], std::ios::binary);
    std::map<std::string, archive::Archive> open;
    for (std::string where; std::getline(sample, where);)
    {
        if (argc > 3 && where != argv[3])
        {
            continue;
        }
        const auto bytes = Bytes(where, open);
        const auto file = bytes ? gff4::File::Parse(*bytes) : std::nullopt;
        const auto root = file ? file->Root() : std::nullopt;
        if (!root)
        {
            out << where << "\terror\tcore does not read it\n";
            continue;
        }
        std::vector<std::string> tokens;
        Walk(*root, 64, tokens);
        if (argc > 3)
        {
            for (const std::string &token : tokens)
            {
                std::cout << token << "\n";
            }
        }
        char hash[32];
        std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(Fnv(tokens)));
        out << where << '\t' << hash << '\t' << tokens.size() << '\n';
    }
    return 0;
}
