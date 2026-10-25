// dao_files: the game's files out of their archives, and GFF4 files as JSON, many in one call, over
// the core alone (DAOLib #106, step 6): what ElgaraShaders' build_shaders.py and effect_list.py
// read through tools/script_commands.py's erf_entries and tools/gff4.py before.
//
//   dao_files extract ARCHIVE FOLDER [PATTERN ...]
//       every entry of the archive matching any pattern (* and ?, in any case; none, all of them)
//       written as FOLDER\<its name>, byte for byte; a line a file in the archive's order, then the
//       count. Exits 1 where a pattern with no wildcard names no entry.
//   dao_files list ARCHIVE
//       each entry of the archive in its order, a line each: its name, its size, and whether it
//       reads ("read") or not ("enciphered" or "compressed"); then the count.
//   dao_files json [--names CSV] FOLDER INPUT ...
//       each GFF4 file given, a path or ARCHIVE|PATTERN, written as FOLDER\<its name>.json: a struct
//       {"@": template, "Name": value, ...} in its template's order, a list an array, a vector's
//       floats an array, a talk-table string {"strref": id}, nothing null, a NaN null. The names
//       are dev/gff4_fields.csv's (xoreos'), by default the one two folders above this tool; a
//       field with no name keeps its number.
#include "dao/core/Erf.h"
#include "dao/core/Gff4.h"
#include "dao/core/Gff4Walk.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace archive = dao::core::erf;
namespace gff4 = dao::core::gff4;
namespace fs = std::filesystem;

namespace
{

wchar_t Lower(wchar_t c)
{
    return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c - L'A' + L'a') : c;
}

// Whether a name matches a pattern of * (any run) and ? (any one), in any case.
bool Matches(std::wstring_view name, std::wstring_view pattern)
{
    std::size_t n = 0;
    std::size_t p = 0;
    std::size_t star = std::wstring_view::npos;
    std::size_t resume = 0;
    while (n < name.size())
    {
        if (p < pattern.size() && (pattern[p] == L'?' || Lower(pattern[p]) == Lower(name[n])))
        {
            ++n;
            ++p;
        }
        else if (p < pattern.size() && pattern[p] == L'*')
        {
            star = p++;
            resume = n;
        }
        else if (star != std::wstring_view::npos)
        {
            p = star + 1;
            n = ++resume;
        }
        else
        {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == L'*')
    {
        ++p;
    }
    return p == pattern.size();
}

bool AnyMatch(std::wstring_view name, const std::vector<std::wstring> &patterns)
{
    if (patterns.empty())
    {
        return true;
    }
    for (const std::wstring &pattern : patterns)
    {
        if (Matches(name, pattern))
        {
            return true;
        }
    }
    return false;
}

std::wstring Wide(std::string_view text)
{
    return fs::path(std::u8string(text.begin(), text.end())).wstring();
}

std::string Narrow(const std::wstring &text)
{
    const std::u8string utf8 = fs::path(text).u8string();
    return std::string(utf8.begin(), utf8.end());
}

bool WriteFile(const fs::path &path, const std::vector<std::byte> &bytes)
{
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

int Extract(const fs::path &file, const fs::path &folder, const std::vector<std::wstring> &patterns)
{
    const auto opened = archive::Archive::Open(file);
    if (!opened)
    {
        std::cerr << "no archive at " << file.string() << "\n";
        return 1;
    }
    fs::create_directories(folder);
    std::map<std::wstring, bool> named; // a pattern with no wildcard, and whether an entry had it
    for (const std::wstring &pattern : patterns)
    {
        if (pattern.find_first_of(L"*?") == std::wstring::npos)
        {
            named.emplace(pattern, false);
        }
    }
    std::size_t written = 0;
    for (const archive::Entry &entry : opened->Entries())
    {
        if (!AnyMatch(entry.name, patterns))
        {
            continue;
        }
        for (auto &[pattern, met] : named)
        {
            met = met || Matches(entry.name, pattern);
        }
        const auto bytes = opened->Read(entry);
        if (!bytes)
        {
            std::cout << Narrow(entry.name) << "\tnot read: the archive's files are enciphered or compressed\n";
            continue;
        }
        if (!WriteFile(folder / entry.name, *bytes))
        {
            std::cerr << "could not write " << (folder / entry.name).string() << "\n";
            return 1;
        }
        std::cout << Narrow(entry.name) << "\n";
        ++written;
    }
    std::cout << written << " files written\n";
    int result = 0;
    for (const auto &[pattern, met] : named)
    {
        if (!met)
        {
            std::cerr << "no entry " << Narrow(pattern) << "\n";
            result = 1;
        }
    }
    return result;
}

int List(const fs::path &file)
{
    const auto opened = archive::Archive::Open(file);
    if (!opened)
    {
        std::cerr << "no archive at " << file.string() << "\n";
        return 1;
    }
    const char *state = opened->Readable() ? "read" : opened->Head().Encryption() != 0 ? "enciphered" : "compressed";
    for (const archive::Entry &entry : opened->Entries())
    {
        std::cout << Narrow(entry.name) << "\t" << entry.size << "\t" << state << "\n";
    }
    std::cout << opened->Entries().size() << " entries\n";
    return 0;
}

std::map<std::uint32_t, std::string> ReadNames(const fs::path &path)
{
    std::map<std::uint32_t, std::string> names;
    std::ifstream in(path);
    for (std::string line; std::getline(in, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const auto comma = line.find(',');
        if (line.empty() || line.front() == '#' || comma == std::string::npos)
        {
            continue;
        }
        try
        {
            names[static_cast<std::uint32_t>(std::stoul(line.substr(0, comma)))] = line.substr(comma + 1);
        }
        catch (const std::exception &)
        {
            // a header row, or a line of no label
        }
    }
    return names;
}

void Quote(std::string &out, std::string_view text)
{
    out += '"';
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if (static_cast<unsigned char>(c) < 0x20)
        {
            char escape[8];
            std::snprintf(escape, sizeof escape, "\\u%04x", static_cast<unsigned>(c));
            out += escape;
        }
        else
        {
            out += c;
        }
    }
    out += '"';
}

// A GFF4 file as JSON, from the walk's events.
class Json : public gff4::Visitor
{
  public:
    explicit Json(const std::map<std::uint32_t, std::string> &names) : names_(names)
    {
    }

    std::string out;

    void BeginStruct(std::string_view label) override
    {
        Before();
        out += "{\"@\": ";
        Quote(out, label);
        open_.push_back(false);
    }
    void EndStruct() override
    {
        out += '}';
        open_.pop_back();
    }
    void Field(std::uint32_t label) override
    {
        out += ", ";
        const auto name = names_.find(label);
        Quote(out, name != names_.end() ? name->second : std::to_string(label));
        out += ": ";
    }
    void BeginList(std::size_t /*count*/) override
    {
        Before();
        out += '[';
        open_.push_back(true);
        first_.push_back(true);
    }
    void EndList() override
    {
        out += ']';
        open_.pop_back();
        first_.pop_back();
    }
    void Value(const gff4::Leaf &leaf) override
    {
        Before();
        if (const auto *whole = std::get_if<std::int64_t>(&leaf))
        {
            out += std::to_string(*whole);
        }
        else if (const auto *large = std::get_if<std::uint64_t>(&leaf))
        {
            out += std::to_string(*large);
        }
        else if (const auto *real = std::get_if<double>(&leaf))
        {
            Real(*real);
        }
        else if (const auto *text = std::get_if<std::wstring>(&leaf))
        {
            Quote(out, Narrow(*text));
        }
        else if (const auto *strref = std::get_if<gff4::StrRef>(&leaf))
        {
            out += "{\"strref\": " + std::to_string(strref->id) + "}";
        }
        else
        {
            out += "null";
        }
    }
    void TooDeep() override
    {
        Before();
        out += "\"...\"";
    }

  private:
    // A comma before a list's every element but its first; a struct's fields put their own.
    void Before()
    {
        if (!open_.empty() && open_.back())
        {
            if (!first_.back())
            {
                out += ", ";
            }
            first_.back() = false;
        }
    }
    void Real(double value)
    {
        if (!std::isfinite(value))
        {
            out += "null";
            return;
        }
        char text[32];
        std::snprintf(text, sizeof text, "%.17g", value);
        out += text;
    }

    const std::map<std::uint32_t, std::string> &names_;
    std::vector<bool> open_;  // the containers open: a list (true) or a struct (false)
    std::vector<bool> first_; // of each list open, whether no element is written yet
};

// One GFF4 file as JSON into the folder; false for bytes that are no GFF4.
bool JsonOf(const std::vector<std::byte> &bytes, const fs::path &out,
            const std::map<std::uint32_t, std::string> &names)
{
    const auto file = gff4::File::Parse(bytes);
    const auto root = file ? file->Root() : std::nullopt;
    if (!root)
    {
        return false;
    }
    Json json(names);
    gff4::Walk(*root, json);
    std::ofstream(out, std::ios::binary) << json.out << "\n";
    return true;
}

std::vector<std::byte> ReadFile(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(fs::file_size(path)));
    in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

int ToJson(const fs::path &folder, const std::vector<std::string> &inputs, const fs::path &namesFile)
{
    const auto names = ReadNames(namesFile);
    if (names.empty())
    {
        std::cerr << "no field names at " << namesFile.string() << " (--names CSV)\n";
    }
    fs::create_directories(folder);
    std::size_t written = 0;
    const auto one = [&](const std::wstring &name, const std::vector<std::byte> &bytes) {
        if (JsonOf(bytes, folder / (name + L".json"), names))
        {
            std::cout << Narrow(name) << "\n";
            ++written;
        }
        else
        {
            std::cout << Narrow(name) << "\tnot GFF4: skipped\n";
        }
    };
    for (const std::string &input : inputs)
    {
        const auto bar = input.find('|');
        if (bar == std::string::npos)
        {
            const fs::path path(Wide(input));
            one(path.filename().wstring(), ReadFile(path));
            continue;
        }
        const auto opened = archive::Archive::Open(Wide(input.substr(0, bar)));
        if (!opened)
        {
            std::cerr << "no archive at " << input.substr(0, bar) << "\n";
            return 1;
        }
        const std::vector<std::wstring> patterns{Wide(input.substr(bar + 1))};
        for (const archive::Entry &entry : opened->Entries())
        {
            if (AnyMatch(entry.name, patterns))
            {
                if (const auto bytes = opened->Read(entry))
                {
                    one(entry.name, *bytes);
                }
            }
        }
    }
    std::cout << written << " files written\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() >= 3 && args[0] == "extract")
    {
        std::vector<std::wstring> patterns;
        for (std::size_t i = 3; i < args.size(); ++i)
        {
            patterns.push_back(Wide(args[i]));
        }
        return Extract(Wide(args[1]), Wide(args[2]), patterns);
    }
    if (args.size() == 2 && args[0] == "list")
    {
        return List(Wide(args[1]));
    }
    if (args.size() >= 3 && args[0] == "json")
    {
        std::size_t at = 1;
        fs::path names = fs::path(argv[0]).parent_path() / ".." / ".." / "dev" / "gff4_fields.csv";
        if (args[at] == "--names" && args.size() >= 5)
        {
            names = Wide(args[at + 1]);
            at += 2;
        }
        return ToJson(Wide(args[at]), std::vector<std::string>(args.begin() + static_cast<std::ptrdiff_t>(at) + 1, args.end()),
                      names);
    }
    std::cerr << "dao_files extract ARCHIVE FOLDER [PATTERN ...]\n"
                 "dao_files list ARCHIVE\n"
                 "dao_files json [--names CSV] FOLDER INPUT ...    INPUT a file, or ARCHIVE|PATTERN\n";
    return 2;
}
