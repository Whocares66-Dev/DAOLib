// dao_gda_schema: every worksheet the game merges, its columns and what each holds, read from an
// install with no game (DAOLib #106, step 5), written as dev/gda_schema.csv, from which
// tools/gen_gda_schema.py writes src/dao/core/generated/GdaSchema.h.
//
//   dao_gda_schema <game> <documents> <column names> <out.csv>
//
// A worksheet is one of m2da_base's rows, merged from its files as the game merges them
// (install::Index::Worksheet). A column's name is the game's hash looked up in the names list
// (dev/gda_column_names.txt, xoreos'); its kind is what its values are, only where nearly all of
// them say so, the numbers kept beside it so a reader can judge:
//   strref     a whole number column whose non-zero values resolve in the talk tables
//   resource   a text column whose values are the names of the install's files of one extension
//   worksheet  a text column, *_Worksheet, whose values are worksheets' labels
// and plain otherwise. A row of another table is no kind of this tool's: those are kept by hand
// in dev/gda_references.csv.
#include "dao/core/GdaFile.h"
#include "dao/core/GdaHash.h"
#include "dao/core/Install.h"
#include "dao/core/M2da.h"
#include "dao/core/TalkTables.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace install = dao::core::install;
namespace m2da = dao::core::m2da;
using dao::core::GdaFile;

namespace
{

// The least share of a column's distinct values that must say a kind, and how many there must be.
constexpr double kNearlyAll = 0.98;
constexpr std::size_t kEnough = 10;
// The least value of a column of strrefs: small whole numbers (levels, races, a table's own IDs)
// are ids of the talk tables too, and resolve by chance (seen 2026-10-24: Abilities.PrereqLevel).
constexpr std::int64_t kLeastStrRef = 1000;

std::string Utf8(const std::wstring &text)
{
    std::string out;
    for (const wchar_t c : text)
    {
        out += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return out;
}

std::string TypeName(const std::optional<dao::core::gff4::Type> &type)
{
    using T = dao::core::gff4::Type;
    if (!type)
    {
        return "none";
    }
    switch (*type)
    {
    case T::U8:
        return "u8";
    case T::S8:
        return "s8";
    case T::U16:
        return "u16";
    case T::S16:
        return "s16";
    case T::U32:
        return "u32";
    case T::S32:
        return "s32";
    case T::U64:
        return "u64";
    case T::S64:
        return "s64";
    case T::F32:
        return "f32";
    case T::F64:
        return "f64";
    case T::String:
        return "string";
    default:
        return "type" + std::to_string(static_cast<int>(*type));
    }
}

std::map<std::uint32_t, std::string> ReadNames(const std::filesystem::path &path)
{
    std::map<std::uint32_t, std::string> names;
    std::ifstream in(path);
    for (std::string line; std::getline(in, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (!line.empty() && line.front() != '#')
        {
            names.emplace(dao::core::GdaColumnHash(line), line);
        }
    }
    return names;
}

// What a column's values say it is: its kind, the extension of a resource's, and of its distinct
// values how many say so.
struct Kind
{
    std::string kind = "plain";
    std::string extension;
    std::size_t hits = 0;
    std::size_t values = 0;
    std::int64_t least = 0; // a whole number column's least value, 0 for none
};

bool Enough(std::size_t hits, std::size_t values)
{
    return values >= kEnough && static_cast<double>(hits) >= kNearlyAll * static_cast<double>(values);
}

// Every file's name the install has, by its name without its extension: the extensions it has.
using Extensions = std::map<std::string, std::set<std::string>, std::less<>>;

Extensions ExtensionsOf(const install::Index &index)
{
    Extensions out;
    for (const auto &[name, copies] : index.Resources())
    {
        const auto dot = name.rfind('.');
        if (dot != std::string::npos && dot > 0)
        {
            out[name.substr(0, dot)].insert(name.substr(dot + 1));
        }
    }
    return out;
}

Kind WholeKind(const std::set<std::int64_t> &values, const install::Strings &strings)
{
    Kind kind;
    kind.values = values.size();
    kind.least = values.empty() ? 0 : *values.begin();
    for (const std::int64_t value : values)
    {
        if (value > 0 && value < 0xFFFFFFFF && strings.Lookup(static_cast<std::uint32_t>(value)))
        {
            ++kind.hits;
        }
    }
    if (Enough(kind.hits, kind.values) && kind.least >= kLeastStrRef)
    {
        kind.kind = "strref";
    }
    return kind;
}

Kind TextKind(const std::string &column, const std::set<std::string> &values, const Extensions &extensions,
              const std::set<std::string> &worksheets)
{
    Kind kind;
    kind.values = values.size();
    const std::string lower = install::Lower(column);
    if (lower.size() > 10 && lower.ends_with("_worksheet"))
    {
        for (const std::string &value : values)
        {
            kind.hits += worksheets.contains(value) ? 1 : 0;
        }
        kind.kind = Enough(kind.hits, kind.values) ? "worksheet" : "plain";
        return kind;
    }
    std::map<std::string, std::size_t> byExtension;
    for (const std::string &value : values)
    {
        if (const auto found = extensions.find(value); found != extensions.end())
        {
            for (const std::string &extension : found->second)
            {
                ++byExtension[extension];
            }
        }
    }
    for (const auto &[extension, hits] : byExtension)
    {
        if (hits > kind.hits)
        {
            kind.hits = hits;
            kind.extension = extension;
        }
    }
    if (Enough(kind.hits, kind.values))
    {
        kind.kind = "resource";
    }
    else
    {
        kind.extension.clear();
    }
    return kind;
}

// The distinct values of a column, as numbers or as text in lower case; "****" and empty text none.
void Values(const m2da::Merged &table, std::size_t column, std::set<std::int64_t> &numbers,
            std::set<std::string> &texts)
{
    for (const m2da::Row &row : table.rows)
    {
        if (column >= row.cells.size())
        {
            continue;
        }
        const GdaFile::Cell &cell = row.cells[column];
        if (const auto *number = std::get_if<std::int64_t>(&cell); number != nullptr && *number != 0 && *number != -1)
        {
            numbers.insert(*number);
        }
        else if (const auto *text = std::get_if<std::wstring>(&cell); text != nullptr && !text->empty() && *text != L"****")
        {
            texts.insert(install::Lower(Utf8(*text)));
        }
    }
}

std::string Quoted(const std::string &text)
{
    std::string out = "\"";
    for (const char c : text)
    {
        out += c == '"' ? "\"\"" : std::string(1, c);
    }
    return out + "\"";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "dao_gda_schema <game> <documents> <column names> <out.csv>\n";
        return 2;
    }
    const auto index = install::Index::Build(argv[1], argv[2]);
    const auto base = index ? index->Table("m2da_base.gda") : nullptr;
    if (!base)
    {
        std::cerr << "no install, or no m2da_base.gda in it, at " << argv[1] << "\n";
        return 1;
    }
    const auto names = ReadNames(argv[3]);
    const install::Strings strings(*index, install::TalkTables(*index, false));
    const Extensions extensions = ExtensionsOf(*index);
    const std::vector<m2da::Worksheet> worksheets = m2da::Worksheets(*base, true);
    std::set<std::string> labels;
    for (const m2da::Worksheet &worksheet : worksheets)
    {
        labels.insert(install::Lower(Utf8(worksheet.label)));
    }
    std::ofstream out(argv[4], std::ios::binary);
    out << "id,label,prefix,column,hash,type,kind,extension,hits,values,least\n";
    std::size_t columns = 0;
    for (const m2da::Worksheet &worksheet : worksheets)
    {
        const m2da::Merged *table = index->Worksheet(worksheet.id);
        if (table == nullptr)
        {
            continue;
        }
        for (std::size_t c = 0; c < table->columns.size(); ++c)
        {
            const GdaFile::Column &column = table->columns[c];
            const auto named = names.find(column.hash);
            char hex[16];
            std::snprintf(hex, sizeof hex, "0x%08X", column.hash);
            const std::string name = named != names.end() ? named->second : hex;
            std::set<std::int64_t> numbers;
            std::set<std::string> texts;
            Values(*table, c, numbers, texts);
            Kind kind;
            if (column.type == dao::core::gff4::Type::String)
            {
                kind = TextKind(name, texts, extensions, labels);
            }
            else if (column.type != dao::core::gff4::Type::F32 && column.type != dao::core::gff4::Type::F64)
            {
                kind = WholeKind(numbers, strings);
            }
            out << worksheet.id << ',' << Quoted(Utf8(worksheet.label)) << ',' << Quoted(Utf8(worksheet.prefix)) << ','
                << Quoted(name) << ',' << hex << ',' << TypeName(column.type) << ',' << kind.kind << ','
                << kind.extension << ',' << kind.hits << ',' << kind.values << ',' << kind.least << '\n';
            ++columns;
        }
    }
    std::cout << worksheets.size() << " worksheets, " << columns << " columns\n";
    return 0;
}
