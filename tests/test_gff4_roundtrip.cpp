// The GFF4 Editor over the game's own files (DAOLib #106's third step): each file read, edited
// (a whole number, a string, a list of numbers lengthened), and read again, every field compared
// before and after by a walk of the whole file through the reader's own calls. Only the edited
// fields may differ. The Editor writes over a copy of the bytes and appends what does not fit, so a
// file nothing was set in is the same bytes by its construction; what this holds is that an edit
// changes its field alone, and that the reader reads what the Editor wrote, in every kind of file
// the game has.
//
// Two passes: a sample of the game/ folders' files in the debug run ([folder], some 150 files,
// 80 s), and every GFF4 file the GOG install's game takes, hidden as Install's other real-install
// tests are, run by its tag: dao_core_tests "[install-real]" (55,000 files of 22 kinds, about three
// hours, most of it the animations' 173 million fields).
//
// Lines are matched to the edits by their slot, not their path: a struct reached by two paths (ten
// cutscenes' first transition) changes at both. Every element of the game's generic lists is a
// struct, which the walk follows; one that is not would fail a pass, as no call of the reader lists
// such elements (DAOLib #250), and the null or empty generic lists are counted. The first run found
// the reader's TlkText reading a text word of 0 as an offset (dao/core/Gff4.cpp, 2026-10-23).
#include "dao/core/Gff4.h"
#include "dao/core/Install.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace gff4 = dao::core::gff4;
namespace install = dao::core::install;

namespace
{

constexpr int kDepth = 48;
constexpr auto kGeneric = static_cast<std::uint16_t>(gff4::Type::Generic);

// A file as lines: each field's path and its value as the reader gives it.
using Lines = std::map<std::string, std::string>;

std::string Narrow(const std::wstring &text)
{
    std::string out;
    out.reserve(text.size());
    for (const wchar_t c : text)
    {
        out += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return out;
}

// A list's bytes as their count and a hash (FNV-1a): a mesh's lists run to megabytes.
std::string Hex(std::span<const std::byte> bytes)
{
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (const std::byte b : bytes)
    {
        hash = (hash ^ std::to_integer<std::uint64_t>(b)) * 0x100000001B3ULL;
    }
    return std::to_string(bytes.size()) + " bytes " + std::to_string(hash);
}

template <class T> std::optional<std::string> Number(const gff4::Struct &in, std::uint32_t label)
{
    const auto value = in.Get<T>(label);
    return value ? std::optional<std::string>(std::to_string(*value)) : std::nullopt;
}

// A scalar's value by the field's type, or for a generic field by whichever type it holds.
std::optional<std::string> Scalar(const gff4::Struct &in, const gff4::Field &field)
{
    using T = gff4::Type;
    const bool generic = field.type == kGeneric;
    const auto is = [&](T type) { return generic || field.type == static_cast<std::uint16_t>(type); };
    if (is(T::String))
    {
        if (const auto text = in.String(field.label))
        {
            return "\"" + Narrow(*text) + "\"";
        }
    }
    if (is(T::TlkString))
    {
        if (const auto tlk = in.Tlk(field.label))
        {
            return "tlk " + std::to_string(tlk->strref) + " \"" + Narrow(in.TlkText(field.label).value_or(L"")) + "\"";
        }
    }
    for (const auto &each :
         {Number<std::uint8_t>, Number<std::int8_t>, Number<std::uint16_t>, Number<std::int16_t>, Number<std::uint32_t>,
          Number<std::int32_t>, Number<std::uint64_t>, Number<std::int64_t>, Number<double>})
    {
        if (auto value = each(in, field.label))
        {
            return value;
        }
    }
    if (const auto floats = in.Floats(field.label); !floats.empty())
    {
        std::ostringstream text;
        for (const float f : floats)
        {
            text << f << ' ';
        }
        return text.str();
    }
    return std::nullopt;
}

// A list that holds no structs: its elements' bytes, or its strings.
std::string List(const gff4::Struct &in, const gff4::Field &field)
{
    if (field.type == static_cast<std::uint16_t>(gff4::Type::String))
    {
        std::string out;
        for (const auto &text : in.Strings(field.label))
        {
            out += "\"" + Narrow(text) + "\" ";
        }
        return out;
    }
    return Hex(in.ListBytes(field.label, static_cast<gff4::Type>(field.type)));
}

struct Walk
{
    Lines lines;
    // Each field's line's slot, its place in the file: a struct reached by two paths has one slot
    // for both, and an edit there is seen at both.
    std::map<std::string, std::size_t> slots;
    const gff4::File *file = nullptr;
    std::size_t nullLists = 0; // generic lists null or of no elements, at each path that reaches them
    // Elements of generic lists that are not structs, which no call of the reader lists: none in the
    // game's files (DAOLib #250: every element of the conversations', the models' and 400 physics
    // files' generic lists is a struct), so a pass fails on one, and the reader would want a call.
    std::size_t nonStructs = 0;
    std::set<std::size_t> path; // the structs above, by offset: a reference back up is not followed

    void Struct(const gff4::Struct &in, const std::string &at, int depth)
    {
        if (depth > kDepth || !path.insert(in.Offset()).second)
        {
            lines[at] = "(again)";
            return;
        }
        lines[at] = in.Label();
        for (const gff4::Field &field : in.Shape().fields)
        {
            FieldOf(in, field, at + "/" + std::to_string(field.label), depth);
        }
        path.erase(in.Offset());
    }

    // A generic list's elements read as their words: each one's flags say whether it is a struct.
    void CountGeneric(const gff4::Struct &in, const gff4::Field &field)
    {
        constexpr std::uint32_t kNull = 0xFFFFFFFF;
        constexpr std::size_t kElement = 8;
        const auto list = file->U32(in.Offset() + field.offset);
        const auto count = list && *list != kNull ? file->U32(file->DataOffset() + *list) : std::nullopt;
        if (!count || *count == 0)
        {
            ++nullLists;
            return;
        }
        for (std::uint32_t i = 0; i < *count; ++i)
        {
            const auto word = file->U32(file->DataOffset() + *list + 4 + (kElement * i));
            if (!word || ((*word >> 16) & gff4::kStruct) == 0)
            {
                ++nonStructs;
            }
        }
    }

    void FieldOf(const gff4::Struct &in, const gff4::Field &field, const std::string &at, int depth)
    {
        slots[at] = in.Offset() + field.offset;
        const bool list = (field.flags & gff4::kList) != 0;
        const bool structs = (field.flags & gff4::kStruct) != 0;
        if (list && (structs || field.type == kGeneric))
        {
            const auto children = in.Structs(field.label);
            lines[at] = "list of " + std::to_string(children.size());
            if (!structs)
            {
                CountGeneric(in, field);
            }
            for (std::size_t i = 0; i < children.size(); ++i)
            {
                Struct(children[i], at + "[" + std::to_string(i) + "]", depth + 1);
            }
            return;
        }
        if (list)
        {
            lines[at] = List(in, field);
            return;
        }
        if (structs || field.type == kGeneric)
        {
            if (const auto child = in.Child(field.label))
            {
                Struct(*child, at, depth + 1);
                return;
            }
        }
        lines[at] = Scalar(in, field).value_or(structs ? "(none)" : "(unread)");
    }
};

// Fills `walk` rather than returning one: its maps' move may allocate.
void WalkOf(const gff4::File &file, Walk &walk)
{
    walk.file = &file;
    if (const auto root = file.Root())
    {
        walk.Struct(*root, "", 0);
    }
}

// The fields an edit is made in: the first of each kind the walk meets, by its struct and label.
struct Target
{
    std::optional<gff4::Struct> in;
    std::uint32_t label = 0;
    std::string at;
};

struct Targets
{
    Target number;  // a whole number of its own, held in place
    Target text;    // a string of its own
    Target numbers; // a list of U32s
    std::set<std::size_t> seen;

    void Find(const gff4::Struct &in, const std::string &at, int depth)
    {
        if (depth > kDepth || !seen.insert(in.Offset()).second)
        {
            return;
        }
        for (const gff4::Field &field : in.Shape().fields)
        {
            const std::string path = at + "/" + std::to_string(field.label);
            const bool list = (field.flags & gff4::kList) != 0;
            const bool structs = (field.flags & gff4::kStruct) != 0;
            const bool reference = (field.flags & gff4::kReference) != 0;
            if (!number.in && !list && !structs && !reference && field.type <= 7)
            {
                number = {in, field.label, path};
            }
            if (!text.in && !list && !structs && field.type == static_cast<std::uint16_t>(gff4::Type::String))
            {
                text = {in, field.label, path};
            }
            if (!numbers.in && list && !structs && !reference &&
                field.type == static_cast<std::uint16_t>(gff4::Type::U32))
            {
                numbers = {in, field.label, path};
            }
            if (structs && !list)
            {
                if (const auto child = in.Child(field.label))
                {
                    Find(*child, path, depth + 1);
                }
            }
            if (structs && list)
            {
                const auto children = in.Structs(field.label);
                for (std::size_t i = 0; i < children.size(); ++i)
                {
                    Find(children[i], path + "[" + std::to_string(i) + "]", depth + 1);
                }
            }
        }
    }
};

// One file's check: what the edits were, and what differed besides them.
struct Checked
{
    bool parsed = false;
    int edits = 0;
    std::size_t lines = 0;
    std::size_t nullLists = 0;
    std::size_t nonStructs = 0;
    std::vector<std::string> wrong; // lines that differ and should not, or should and do not
};

// Within the field's width either way: 0 becomes 1, anything else 0.
std::int64_t Bumped(const gff4::Struct &in, const gff4::Field &field)
{
    return Scalar(in, field) == std::optional<std::string>("0") ? 1 : 0;
}

Checked Check(std::span<const std::byte> bytes)
{
    Checked checked;
    const auto file = gff4::File::Parse(bytes);
    if (!file || !file->Root())
    {
        return checked;
    }
    checked.parsed = true;
    Walk before;
    WalkOf(*file, before);
    Targets targets;
    targets.Find(*file->Root(), "", 0);
    gff4::Editor editor(*file);
    std::set<std::size_t> edited; // the edited fields' slots
    const auto slotOf = [](const Target &target) {
        return target.in->Offset() + target.in->Shape().Find(target.label)->offset;
    };
    if (targets.number.in)
    {
        const gff4::Field *field = targets.number.in->Shape().Find(targets.number.label);
        if (editor.SetNumber(*targets.number.in, targets.number.label, Bumped(*targets.number.in, *field)))
        {
            edited.insert(slotOf(targets.number));
        }
    }
    if (targets.text.in && editor.SetString(*targets.text.in, targets.text.label,
                                            targets.text.in->String(targets.text.label).value_or(L"") + L" (edited)"))
    {
        edited.insert(slotOf(targets.text));
    }
    if (targets.numbers.in)
    {
        const std::array<std::byte, 4> one{std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01}};
        if (editor.Extend(*targets.numbers.in, targets.numbers.label, one, 1))
        {
            edited.insert(slotOf(targets.numbers));
        }
    }
    checked.edits = static_cast<int>(edited.size());
    const auto again = gff4::File::Parse(editor.Bytes());
    if (!again)
    {
        checked.wrong.emplace_back("(the edited file does not parse)");
        return checked;
    }
    Walk after;
    WalkOf(*again, after);
    checked.lines = before.lines.size();
    checked.nullLists = before.nullLists;
    checked.nonStructs = before.nonStructs;
    if (before.nonStructs != 0)
    {
        checked.wrong.push_back(std::to_string(before.nonStructs) + " elements of generic lists are no structs");
    }
    std::set<std::string> keys;
    for (const auto &[key, value] : before.lines)
    {
        keys.insert(key);
    }
    for (const auto &[key, value] : after.lines)
    {
        keys.insert(key);
    }
    for (const std::string &key : keys)
    {
        const auto was = before.lines.find(key);
        const auto now = after.lines.find(key);
        const bool same = was != before.lines.end() && now != after.lines.end() && was->second == now->second;
        const auto slot = before.slots.find(key);
        const bool wasEdited = slot != before.slots.end() && edited.contains(slot->second);
        if (same == wasEdited)
        {
            checked.wrong.push_back(key + ": " + (was != before.lines.end() ? was->second : "(none)") + " -> " +
                                    (now != after.lines.end() ? now->second : "(none)"));
        }
    }
    return checked;
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

bool IsGff4(std::span<const std::byte> bytes)
{
    return bytes.size() >= 8 && std::memcmp(bytes.data(), "GFF V4.", 7) == 0;
}

// What a pass found, by the file's type (its four letters).
struct Tally
{
    std::size_t files = 0;
    std::size_t edits = 0;
    std::size_t lines = 0;
    std::size_t nullLists = 0;
};

struct Pass
{
    std::map<std::string, Tally> byType;
    std::vector<std::string> wrong;

    void Add(const std::string &name, std::span<const std::byte> bytes)
    {
        if (!IsGff4(bytes))
        {
            return;
        }
        const Checked checked = Check(bytes);
        const std::string type =
            checked.parsed ? std::string(gff4::File::Parse(bytes)->Type()) : std::string("(unparsed)");
        Tally &tally = byType[type];
        ++tally.files;
        tally.edits += static_cast<std::size_t>(checked.edits);
        tally.lines += checked.lines;
        tally.nullLists += checked.nullLists;
        if (!checked.parsed)
        {
            wrong.push_back(name + ": does not parse");
        }
        for (const std::string &line : checked.wrong)
        {
            if (wrong.size() < 40)
            {
                std::string text = name;
                text += ' ';
                text += line;
                wrong.push_back(std::move(text));
            }
        }
    }

    std::string Summary() const
    {
        std::ostringstream out;
        for (const auto &[type, tally] : byType)
        {
            out << type << ": " << tally.files << " files, " << tally.edits << " edits, " << tally.lines << " fields, "
                << tally.nullLists << " null or empty generic lists\n";
        }
        return out.str();
    }
};

} // namespace

TEST_CASE("an edit made by the Editor in the game's files changes its field alone, in a sample of game/",
          "[gff4][folder]")
{
    const auto game = std::filesystem::path(__FILE__).parent_path().parent_path() / "game";
    if (!std::filesystem::is_directory(game))
    {
        SKIP("no game/ folders (ghilana's extracts write them there)");
    }
    Pass pass;
    // How many files of each folder: conversations few, the largest by far (21 of them were 1.5
    // million fields of the walk).
    for (const auto &[folder, each] : std::initializer_list<std::pair<const char *, std::size_t>>{{"areas", 20},
                                                                                                  {"conversations", 4},
                                                                                                  {"cutscenes", 20},
                                                                                                  {"models", 20},
                                                                                                  {"plots", 20},
                                                                                                  {"layouts", 20},
                                                                                                  {"levels", 20},
                                                                                                  {"campaigns", 20},
                                                                                                  {"patches", 10},
                                                                                                  {"materials", 20}})
    {
        const auto at = game / folder;
        if (!std::filesystem::is_directory(at))
        {
            continue;
        }
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(at))
        {
            if (entry.is_regular_file())
            {
                files.push_back(entry.path());
            }
        }
        std::ranges::sort(files);
        // Spread over the folder, the same files each run.
        const std::size_t step = std::max<std::size_t>(1, files.size() / each);
        for (std::size_t i = 0; i < files.size(); i += step)
        {
            pass.Add(files[i].filename().string(), FromDisk(files[i]));
        }
    }
    WARN(pass.Summary());
    for (const std::string &line : pass.wrong)
    {
        UNSCOPED_INFO(line);
    }
    CHECK(pass.wrong.empty());
    CHECK_FALSE(pass.byType.empty());
}

TEST_CASE("an edit made by the Editor changes its field alone, in every GFF4 file the GOG install's game takes",
          "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    const auto index = install::Index::Build(game, {});
    Pass pass;
    for (const auto &[name, copies] : index->Resources())
    {
        if (copies.empty() || copies.front().unused != install::Unused::No || !index->Readable(copies.front()))
        {
            continue;
        }
        if (const auto bytes = index->Read(copies.front()))
        {
            pass.Add(name, *bytes);
        }
    }
    WARN(pass.Summary());
    for (const std::string &line : pass.wrong)
    {
        UNSCOPED_INFO(line);
    }
    CHECK(pass.wrong.empty());
}
