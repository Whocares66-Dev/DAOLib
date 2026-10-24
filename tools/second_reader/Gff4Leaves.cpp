// dao_gff4_leaves: core's GFF4 reader over a list of files, each file's values as the tokens
// tools/second_reader.py's walk of tools/gff4.py gives (its canonical()), hashed: the two readers
// compared over the install, gff4.py the second reader of core's (DAOLib #106, step 6). Its first
// run found both reading a generic string by reference one hop too far (dao/core/Gff4.cpp).
//
//   dao_gff4_leaves <sample.txt> <out.txt> [where]
//
// sample.txt: a file a line, "archive|name" for one in an archive, "|path" for one on its own.
// out.txt: "where<TAB>hash<TAB>tokens" a line. With where, that file's tokens printed as well.
// The walk is core's (dao/core/Gff4Walk.h); this writes its events as second_reader.py's tokens.
#include "dao/core/Erf.h"
#include "dao/core/Gff4.h"
#include "dao/core/Gff4Walk.h"

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
#include <string_view>
#include <variant>
#include <vector>

namespace gff4 = dao::core::gff4;
namespace archive = dao::core::erf;

namespace
{

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

// The tokens second_reader.py's canonical() gives, of the walk's events.
class Tokens : public gff4::Visitor
{
  public:
    std::vector<std::string> out;

    void BeginStruct(std::string_view label) override
    {
        out.push_back("T" + std::string(label));
    }
    void EndStruct() override
    {
    }
    void Field(std::uint32_t label) override
    {
        out.push_back("#" + std::to_string(label));
    }
    void BeginList(std::size_t count) override
    {
        out.push_back("L" + std::to_string(count));
    }
    void EndList() override
    {
    }
    void TooDeep() override
    {
        out.emplace_back("...");
    }
    void Value(const gff4::Leaf &leaf) override
    {
        if (const auto *whole = std::get_if<std::int64_t>(&leaf))
        {
            out.push_back("i" + std::to_string(*whole));
        }
        else if (const auto *large = std::get_if<std::uint64_t>(&leaf))
        {
            out.push_back("i" + std::to_string(*large));
        }
        else if (const auto *real = std::get_if<double>(&leaf))
        {
            out.push_back(Real(*real));
        }
        else if (const auto *text = std::get_if<std::wstring>(&leaf))
        {
            out.push_back("s" + Text(*text));
        }
        else if (const auto *strref = std::get_if<gff4::StrRef>(&leaf))
        {
            out.push_back("t" + std::to_string(strref->id));
        }
        else
        {
            out.emplace_back("none");
        }
    }
};

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
        Tokens visitor;
        gff4::Walk(*root, visitor, 64);
        const std::vector<std::string> &tokens = visitor.out;
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
