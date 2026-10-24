// The GFF3 Editor over the game's own files (DAOLib #106's third step): each GFF V3.2 file of the GOG
// install, loose or in an archive of version 2.0, given to an Editor and written back with nothing
// set, which must be its own bytes; then edited (a whole number, a float, a string or resref, a
// localised string, a number in a list's first entry), read again, and every field compared before
// and after by a walk of the whole file through the reader's own calls: only the edited fields may
// differ. The toolset's own order of writing a file from nothing is not reproduced (dev/FILES.md,
// "Templates"): a mod edits a template, which is what this holds. Hidden, as it reads every archive
// of the install: run it by its tag, dao_core_tests "[install-real]".
#include "dao/core/Erf.h"
#include "dao/core/Gff3.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace archive = dao::core::erf;
namespace gff3 = dao::core::gff3;

namespace
{

// A file as lines: each field's path and its value as the reader gives it.
using Lines = std::map<std::string, std::string>;

std::vector<std::byte> ReadFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(std::filesystem::file_size(path)));
    in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

std::wstring Lower(std::wstring text)
{
    std::ranges::transform(text, text.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return text;
}

template <class T> std::string Number(const gff3::Struct &in, std::string_view label)
{
    const auto value = in.Get<T>(label);
    return value ? std::to_string(*value) : "(none)";
}

// A field's value as the reader gives it, by the field's type; a struct or a list is walked.
std::string Value(const gff3::Struct &in, const gff3::Field &field)
{
    using T = gff3::Type;
    std::ostringstream text;
    switch (field.type)
    {
    case T::Byte:
        return Number<std::uint8_t>(in, field.label);
    case T::Char:
        return Number<std::int8_t>(in, field.label);
    case T::Word:
        return Number<std::uint16_t>(in, field.label);
    case T::Short:
        return Number<std::int16_t>(in, field.label);
    case T::Dword:
        return Number<std::uint32_t>(in, field.label);
    case T::Int:
        return Number<std::int32_t>(in, field.label);
    case T::Dword64:
        return Number<std::uint64_t>(in, field.label);
    case T::Int64:
        return Number<std::int64_t>(in, field.label);
    case T::Float:
        return Number<float>(in, field.label);
    case T::Double:
        return Number<double>(in, field.label);
    case T::String:
        return "\"" + in.String(field.label).value_or("(none)") + "\"";
    case T::ResRef:
        return "resref " + in.ResRef(field.label).value_or("(none)");
    case T::StrRef:
        return "strref " + std::to_string(in.StrRef(field.label).value_or(0));
    case T::LocString: {
        const auto loc = in.Loc(field.label);
        if (!loc)
        {
            return "(none)";
        }
        text << "loc " << loc->strref;
        for (const auto &[language, string] : loc->strings)
        {
            text << " " << language << ":\"" << string << "\"";
        }
        return text.str();
    }
    case T::Void: {
        const auto bytes = in.Void(field.label);
        std::uint64_t hash = 0xCBF29CE484222325ULL;
        for (const std::byte b : bytes.value_or(std::span<const std::byte>{}))
        {
            hash = (hash ^ std::to_integer<std::uint64_t>(b)) * 0x100000001B3ULL;
        }
        return "void " + std::to_string(bytes ? bytes->size() : 0) + " " + std::to_string(hash);
    }
    case T::Orientation:
    case T::Vector:
        for (const float f : in.Floats(field.label))
        {
            text << f << ' ';
        }
        return text.str();
    default:
        return "type " + std::to_string(static_cast<std::uint32_t>(field.type));
    }
}

void Walk(const gff3::Struct &in, const std::string &path, Lines &lines, int depth = 0)
{
    REQUIRE(depth < 64);
    lines[path + "#id"] = std::to_string(in.Id());
    for (const gff3::Field &field : in.Fields())
    {
        const std::string at = path + "/" + std::string(field.label);
        if (field.type == gff3::Type::Struct)
        {
            const auto child = in.Child(field.label);
            REQUIRE(child);
            Walk(*child, at, lines, depth + 1);
        }
        else if (field.type == gff3::Type::List)
        {
            const std::vector<gff3::Struct> items = in.List(field.label);
            lines[at + "#count"] = std::to_string(items.size());
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                Walk(items[i], at + "[" + std::to_string(i) + "]", lines, depth + 1);
            }
        }
        else
        {
            lines[at] = Value(in, field);
        }
    }
}

Lines WalkOf(std::span<const std::byte> bytes)
{
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto root = file->Root();
    REQUIRE(root);
    Lines lines;
    Walk(*root, "", lines);
    return lines;
}

bool Whole(gff3::Type type)
{
    return type <= gff3::Type::Int64;
}

// A whole number one more than a field's, wrapped at the field's width, as SetNumber writes it.
std::int64_t Next(const gff3::Struct &in, const gff3::Field &field)
{
    using T = gff3::Type;
    switch (field.type)
    {
    case T::Byte:
        return static_cast<std::uint8_t>(*in.Get<std::uint8_t>(field.label) + 1);
    case T::Char:
        return static_cast<std::int8_t>(*in.Get<std::int8_t>(field.label) + 1);
    case T::Word:
        return static_cast<std::uint16_t>(*in.Get<std::uint16_t>(field.label) + 1);
    case T::Short:
        return static_cast<std::int16_t>(*in.Get<std::int16_t>(field.label) + 1);
    case T::Dword:
        return static_cast<std::uint32_t>(*in.Get<std::uint32_t>(field.label) + 1);
    case T::Int:
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(*in.Get<std::int32_t>(field.label)) + 1U);
    case T::Dword64:
        return static_cast<std::int64_t>(*in.Get<std::uint64_t>(field.label) + 1);
    default:
        return static_cast<std::int64_t>(static_cast<std::uint64_t>(*in.Get<std::int64_t>(field.label)) + 1U);
    }
}

const gff3::Field *First(const gff3::Struct &in, bool (*wanted)(gff3::Type))
{
    const auto found = std::ranges::find_if(in.Fields(), [&](const gff3::Field &f) { return wanted(f.type); });
    return found == in.Fields().end() ? nullptr : &*found;
}

// The edits made to one file, and the lines they change.
struct Edits
{
    std::vector<std::byte> bytes;
    std::vector<std::string> paths;
};

Edits Edit(const gff3::File &file)
{
    const gff3::Struct root = *file.Root();
    gff3::Editor editor(file);
    std::vector<std::string> paths;
    const auto path = [](const std::string &prefix, const gff3::Field &field) {
        return prefix + "/" + std::string(field.label);
    };
    if (const gff3::Field *number = First(root, Whole))
    {
        CHECK(editor.SetNumber(root, number->label, Next(root, *number)));
        paths.push_back(path("", *number));
    }
    if (const gff3::Field *real =
            First(root, [](gff3::Type t) { return t == gff3::Type::Float || t == gff3::Type::Double; }))
    {
        const double value =
            real->type == gff3::Type::Float ? *root.Get<float>(real->label) : *root.Get<double>(real->label);
        CHECK(editor.SetFloat(root, real->label, value + 1.5));
        paths.push_back(path("", *real));
    }
    if (const gff3::Field *text =
            First(root, [](gff3::Type t) { return t == gff3::Type::String || t == gff3::Type::ResRef; }))
    {
        const std::string now =
            text->type == gff3::Type::String ? *root.String(text->label) : *root.ResRef(text->label);
        if (now.size() < 255)
        {
            CHECK(editor.SetString(root, text->label, now + "x"));
            paths.push_back(path("", *text));
        }
    }
    if (const gff3::Field *loc = First(root, [](gff3::Type t) { return t == gff3::Type::LocString; }))
    {
        gff3::LocString value = *root.Loc(loc->label);
        value.strref += 1;
        value.strings.emplace_back(99, "edited");
        CHECK(editor.SetLoc(root, loc->label, value));
        paths.push_back(path("", *loc));
    }
    // A number in a list's first entry: an edit below the top struct.
    for (const gff3::Field &field : root.Fields())
    {
        const auto item = field.type == gff3::Type::List ? root.ListItem(field.label, 0) : std::nullopt;
        if (const gff3::Field *number = item ? First(*item, Whole) : nullptr)
        {
            CHECK(editor.SetNumber(*item, number->label, Next(*item, *number)));
            paths.push_back(path("/" + std::string(field.label) + "[0]", *number));
            break;
        }
    }
    return {editor.Bytes(), paths};
}

struct Count
{
    std::size_t files = 0;
    std::size_t edits = 0;
};

void Check(const std::wstring &where, const std::vector<std::byte> &bytes, Count &count)
{
    INFO(std::filesystem::path(where).string());
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    // Nothing set: the file's own bytes.
    CHECK(gff3::Editor(*file).Bytes() == bytes);
    const Lines before = WalkOf(bytes);
    const Edits edits = Edit(*file);
    const Lines after = WalkOf(edits.bytes);
    REQUIRE(after.size() == before.size());
    for (const auto &[line, value] : before)
    {
        INFO(line);
        const auto now = after.find(line);
        REQUIRE(now != after.end());
        const bool edited = std::ranges::find(edits.paths, line) != edits.paths.end();
        CHECK((now->second != value) == edited);
    }
    ++count.files;
    count.edits += edits.paths.size();
}

} // namespace

TEST_CASE("on the GOG install, every template is edited field by field and read back", "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    Count count;
    const auto isGff3 = [](std::span<const std::byte> bytes) {
        return bytes.size() >= 8 && static_cast<char>(bytes[4]) == 'V' && static_cast<char>(bytes[5]) == '3' &&
               static_cast<char>(bytes[6]) == '.' && static_cast<char>(bytes[7]) == '2';
    };
    for (auto item = std::filesystem::recursive_directory_iterator(game); item != std::filesystem::end(item); ++item)
    {
        if (item->is_directory() && Lower(item->path().filename().wstring()) == L"bin_ship")
        {
            item.disable_recursion_pending();
            continue;
        }
        if (!item->is_regular_file())
        {
            continue;
        }
        const std::wstring extension = Lower(item->path().extension().wstring());
        if (extension == L".erf" || extension == L".rim")
        {
            const auto opened = archive::Archive::Open(item->path());
            if (!opened || opened->Head().version != archive::Version::V20)
            {
                continue;
            }
            for (const archive::Entry &entry : opened->Entries())
            {
                const auto bytes = opened->Read(entry);
                if (bytes && isGff3(*bytes))
                {
                    Check(item->path().wstring() + L":" + entry.name, *bytes, count);
                }
            }
            continue;
        }
        std::vector<std::byte> start(8);
        std::ifstream(item->path(), std::ios::binary).read(reinterpret_cast<char *>(start.data()), 8);
        if (isGff3(start))
        {
            Check(item->path().wstring(), ReadFile(item->path()), count);
        }
    }
    CHECK(count.files > 0);
    WARN(count.files << " templates written back unchanged and edited, " << count.edits << " edits");
}
