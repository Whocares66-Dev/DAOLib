// dao_gff3_schema: every field of a kind of template, as the install's templates of the kind have
// them, read with no game (DAOLib #106, step 5), written as dev/gff3_schema_<type>.csv, from which
// tools/gen_gff3_schema.py writes src/dao/core/generated/Gff3Schema.h.
//
//   dao_gff3_schema <game> <documents> <type> <references.csv> <out.csv>
//
// Each copy the game takes of every file of the type (utc, uti) is walked by path: the top struct
// "", a struct field's struct "Appearance Data", a list's entries "ItemList[]". Of each path's
// fields: the type the files give it, how many files have it, and what its values are, said only
// where nearly all of at least ten distinct values say it:
//   resource   a ResRef whose values are the names of the install's files of one extension
//   strref     a whole number whose non-zero values resolve in the talk tables, none below 1,000
//   row        a whole number references.csv names a worksheet of, kept by hand with its evidence,
//              whose values are that worksheet's rows' IDs
// The counts go beside it so a reader can judge.
#include "dao/core/Gff3.h"
#include "dao/core/Install.h"
#include "dao/core/M2da.h"
#include "dao/core/TalkTables.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace gff3 = dao::core::gff3;
namespace install = dao::core::install;
namespace m2da = dao::core::m2da;

namespace
{

constexpr double kNearlyAll = 0.98;
constexpr std::size_t kEnough = 10;
// Small whole numbers are ids of the talk tables too, and resolve by chance (dev/FILES.md).
constexpr std::int64_t kLeastStrRef = 1000;

bool Enough(std::size_t hits, std::size_t values)
{
    return values >= kEnough && static_cast<double>(hits) >= kNearlyAll * static_cast<double>(values);
}

const char *TypeName(gff3::Type type)
{
    using T = gff3::Type;
    switch (type)
    {
    case T::Byte:
        return "byte";
    case T::Char:
        return "char";
    case T::Word:
        return "word";
    case T::Short:
        return "short";
    case T::Dword:
        return "dword";
    case T::Int:
        return "int";
    case T::Dword64:
        return "dword64";
    case T::Int64:
        return "int64";
    case T::Float:
        return "float";
    case T::Double:
        return "double";
    case T::String:
        return "string";
    case T::ResRef:
        return "resref";
    case T::LocString:
        return "locstring";
    case T::Void:
        return "void";
    case T::Struct:
        return "struct";
    case T::List:
        return "list";
    case T::Orientation:
        return "orientation";
    case T::Vector:
        return "vector";
    case T::StrRef:
        return "strref";
    }
    return "unknown";
}

std::optional<std::int64_t> Whole(const gff3::Struct &in, const gff3::Field &field)
{
    using T = gff3::Type;
    switch (field.type)
    {
    case T::Byte:
        return in.Get<std::uint8_t>(field.label);
    case T::Char:
        return in.Get<std::int8_t>(field.label);
    case T::Word:
        return in.Get<std::uint16_t>(field.label);
    case T::Short:
        return in.Get<std::int16_t>(field.label);
    case T::Dword:
        return in.Get<std::uint32_t>(field.label);
    case T::Int:
        return in.Get<std::int32_t>(field.label);
    case T::Int64:
        return in.Get<std::int64_t>(field.label);
    default:
        return std::nullopt;
    }
}

// A field of a path, as the files have it.
struct Seen
{
    std::map<gff3::Type, std::size_t> types; // how many times each type is given it
    std::size_t files = 0;
    std::set<std::int64_t> numbers; // distinct whole values, 0 and -1 aside
    std::set<std::string> texts;    // distinct resrefs, in lower case, empty aside
};

// A path's fields by label, in the order first met, and its structs' ids.
struct Path
{
    std::vector<std::string> order;
    std::map<std::string, Seen> fields;
    std::map<std::uint32_t, std::size_t> ids;
};

using Paths = std::map<std::string, Path>;

std::string Below(const std::string &path, const std::string &label)
{
    return path.empty() ? label : path + "/" + label;
}

// One file's tree into the paths; `here` holds the fields this file has given already, counted once.
void Walk(const gff3::Struct &in, const std::string &path, Paths &paths, std::set<std::string> &here, int depth)
{
    if (depth > 32)
    {
        return;
    }
    Path &at = paths[path];
    ++at.ids[in.Id()];
    for (const gff3::Field &field : in.Fields())
    {
        const std::string label(field.label);
        auto [seen, added] = at.fields.try_emplace(label);
        if (added)
        {
            at.order.push_back(label);
        }
        ++seen->second.types[field.type];
        if (here.insert(path + "\n" + label).second)
        {
            ++seen->second.files;
        }
        if (const auto number = Whole(in, field); number && *number != 0 && *number != -1)
        {
            seen->second.numbers.insert(*number);
        }
        if (field.type == gff3::Type::ResRef)
        {
            if (const auto text = in.ResRef(field.label); text && !text->empty())
            {
                seen->second.texts.insert(install::Lower(*text));
            }
        }
        if (field.type == gff3::Type::Struct)
        {
            if (const auto child = in.Child(field.label))
            {
                Walk(*child, Below(path, label), paths, here, depth + 1);
            }
        }
        else if (field.type == gff3::Type::List)
        {
            for (const gff3::Struct &entry : in.List(field.label))
            {
                Walk(entry, Below(path, label) + "[]", paths, here, depth + 1);
            }
        }
    }
}

// What a field's values are, with the counts behind it.
struct Kind
{
    std::string kind = "plain";
    std::string extension;
    std::int64_t target = -1;
    std::size_t hits = 0;
    std::size_t values = 0;
    std::int64_t least = 0;
};

using Extensions = std::map<std::string, std::set<std::string>, std::less<>>;

Kind ResRefKind(const Seen &seen, const Extensions &extensions)
{
    Kind kind;
    kind.values = seen.texts.size();
    std::map<std::string, std::size_t> byExtension;
    for (const std::string &text : seen.texts)
    {
        if (const auto found = extensions.find(text); found != extensions.end())
        {
            for (const std::string &one : found->second)
            {
                ++byExtension[one];
            }
        }
    }
    for (const auto &[one, count] : byExtension)
    {
        if (count > kind.hits)
        {
            kind.hits = count;
            kind.extension = one;
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

// A whole number's: a row of the worksheet references.csv names, if its values are that
// worksheet's IDs; else a strref, if they resolve.
Kind WholeKind(const Seen &seen, std::optional<std::int64_t> target, const install::Index &index,
               const install::Strings &strings)
{
    Kind kind;
    kind.values = seen.numbers.size();
    kind.least = seen.numbers.empty() ? 0 : *seen.numbers.begin();
    if (target)
    {
        std::set<std::int64_t> ids;
        if (const m2da::Merged *table = index.Worksheet(*target))
        {
            for (const m2da::Row &row : table->rows)
            {
                if (row.id)
                {
                    ids.insert(*row.id);
                }
            }
        }
        for (const std::int64_t value : seen.numbers)
        {
            kind.hits += ids.contains(value) ? 1 : 0;
        }
        if (Enough(kind.hits, kind.values))
        {
            kind.kind = "row";
            kind.target = *target;
        }
        return kind;
    }
    for (const std::int64_t value : seen.numbers)
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

std::vector<std::string> Cells(const std::string &line)
{
    std::vector<std::string> cells(1);
    bool quoted = false;
    for (const char c : line)
    {
        if (c == '"')
        {
            quoted = !quoted;
        }
        else if (c == ',' && !quoted)
        {
            cells.emplace_back();
        }
        else if (c != '\r')
        {
            cells.back() += c;
        }
    }
    return cells;
}

// references.csv: type,path,field,target,evidence; the worksheet a field's values are rows of.
std::map<std::tuple<std::string, std::string, std::string>, std::int64_t> ReadReferences(
    const std::filesystem::path &file)
{
    std::map<std::tuple<std::string, std::string, std::string>, std::int64_t> out;
    std::ifstream in(file);
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line))
    {
        const std::vector<std::string> cells = Cells(line);
        if (cells.size() >= 4 && !cells[3].empty())
        {
            out[{cells[0], cells[1], cells[2]}] = std::stoll(cells[3]);
        }
    }
    return out;
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

template <class K> K MostOf(const std::map<K, std::size_t> &counts)
{
    K most = counts.begin()->first;
    for (const auto &[key, count] : counts)
    {
        most = count > counts.at(most) ? key : most;
    }
    return most;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 6)
    {
        std::cerr << "dao_gff3_schema <game> <documents> <type> <references.csv> <out.csv>\n";
        return 2;
    }
    const auto index = install::Index::Build(argv[1], argv[2]);
    if (!index)
    {
        std::cerr << "no install at " << argv[1] << "\n";
        return 1;
    }
    const std::string type = install::Lower(argv[3]);
    const auto references = ReadReferences(argv[4]);
    Paths paths;
    std::size_t files = 0;
    for (const install::NamedCopy &file : index->Listed("." + type))
    {
        const auto bytes = index->Readable(file.copy) ? index->Read(file.copy) : std::nullopt;
        const auto parsed = bytes ? gff3::File::Parse(*bytes) : std::nullopt;
        const auto root = parsed ? parsed->Root() : std::nullopt;
        if (root)
        {
            std::set<std::string> here;
            Walk(*root, "", paths, here, 0);
            ++files;
        }
    }
    const install::Strings strings(*index, install::TalkTables(*index, false));
    Extensions extensions;
    for (const auto &[name, copies] : index->Resources())
    {
        if (const auto dot = name.rfind('.'); dot != std::string::npos && dot > 0)
        {
            extensions[name.substr(0, dot)].insert(name.substr(dot + 1));
        }
    }
    std::ofstream out(argv[5], std::ios::binary);
    out << "type,path,struct,field,gff3type,kind,extension,target,files,hits,values,least\n";
    std::size_t fields = 0;
    for (const auto &[path, at] : paths)
    {
        const std::uint32_t id = MostOf(at.ids);
        for (const std::string &label : at.order)
        {
            const Seen &seen = at.fields.at(label);
            const gff3::Type fieldType = MostOf(seen.types);
            Kind kind;
            if (fieldType == gff3::Type::ResRef)
            {
                kind = ResRefKind(seen, extensions);
            }
            else if (fieldType <= gff3::Type::Int64 && fieldType != gff3::Type::Dword64)
            {
                const auto reference = references.find({type, path, label});
                kind = WholeKind(seen, reference != references.end() ? std::optional(reference->second) : std::nullopt,
                                 *index, strings);
            }
            out << type << ',' << Quoted(path) << ',' << id << ',' << Quoted(label) << ',' << TypeName(fieldType) << ','
                << kind.kind << ',' << kind.extension << ',' << kind.target << ',' << seen.files << ',' << kind.hits
                << ',' << kind.values << ',' << kind.least << '\n';
            ++fields;
        }
    }
    std::cout << files << " files, " << paths.size() << " paths, " << fields << " fields\n";
    return 0;
}
