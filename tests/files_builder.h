// The game's files made in a test, as the game lays them out: an archive
// (ERF V2.0, or V2.2 with its flags), a data table (a GFF4 of type G2DA), a
// talk table (GFF4 TLK V0.2), a GFF3 template, a plain DDS, and a folder of
// them. What the readers' own notes say of each is what is written here.
// Written for Eluvian's ghilana (DAOLib #106), whose tests include this file
// too, so its files are made as DAOLib's tests make them.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace dao::test::files
{

using Bytes = std::vector<std::byte>;

inline void Put32(Bytes &out, std::size_t at, std::uint32_t value)
{
    if (out.size() < at + 4)
    {
        out.resize(at + 4);
    }
    std::memcpy(out.data() + at, &value, 4);
}

inline void Append32(Bytes &out, std::uint32_t value)
{
    Put32(out, out.size(), value);
}

inline void AppendText(Bytes &out, std::string_view text)
{
    for (const char c : text)
    {
        out.push_back(static_cast<std::byte>(c));
    }
}

inline Bytes FromText(std::string_view text)
{
    Bytes out;
    AppendText(out, text);
    return out;
}

// An archive of named files.
inline Bytes Erf(const std::vector<std::pair<std::string, Bytes>> &files, bool v22 = false, std::uint32_t flags = 0)
{
    Bytes out;
    for (const char c : std::string(v22 ? "ERF V2.2" : "ERF V2.0"))
    {
        out.push_back(static_cast<std::byte>(c));
        out.push_back(std::byte{0});
    }
    Append32(out, static_cast<std::uint32_t>(files.size()));
    Append32(out, 109);
    Append32(out, 1);
    Append32(out, 0xFFFFFFFF);
    if (v22)
    {
        Append32(out, flags);
        Append32(out, 0);
        out.resize(out.size() + 16);
    }
    const std::size_t entry = v22 ? 76 : 72;
    auto offset = static_cast<std::uint32_t>(out.size() + (files.size() * entry));
    for (const auto &[name, bytes] : files)
    {
        for (std::size_t i = 0; i < 32; ++i)
        {
            const char c = i < name.size() ? name[i] : '\0';
            out.push_back(static_cast<std::byte>(c));
            out.push_back(std::byte{0});
        }
        Append32(out, offset);
        Append32(out, static_cast<std::uint32_t>(bytes.size()));
        if (v22)
        {
            Append32(out, static_cast<std::uint32_t>(bytes.size()));
        }
        offset += static_cast<std::uint32_t>(bytes.size());
    }
    for (const auto &[name, bytes] : files)
    {
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    return out;
}

// A GFF4 file of struct templates and data, laid out as DAOLib's reader
// reads one: the header, the templates (16 bytes each), their fields (12
// each), then the data from DataStart().
class Gff4
{
  public:
    struct Field
    {
        std::uint32_t label;
        std::uint16_t type;
        std::uint16_t flags;
        std::uint32_t offset;
    };
    struct Template
    {
        std::string label;
        std::uint32_t size;
        std::vector<Field> fields;
    };

    Gff4(std::string_view type, std::string_view version, const std::vector<Template> &templates)
    {
        AppendText(bytes, "GFF V4.0PC  ");
        AppendText(bytes, type);
        AppendText(bytes, version);
        Append32(bytes, static_cast<std::uint32_t>(templates.size()));
        Append32(bytes, 0); // the data's offset, below
        std::size_t fieldAt = 28 + (16 * templates.size());
        for (const auto &t : templates)
        {
            std::string label = t.label;
            label.resize(4, ' ');
            AppendText(bytes, label);
            Append32(bytes, static_cast<std::uint32_t>(t.fields.size()));
            Append32(bytes, static_cast<std::uint32_t>(fieldAt));
            Append32(bytes, t.size);
            fieldAt += 12 * t.fields.size();
        }
        for (const auto &t : templates)
        {
            for (const auto &f : t.fields)
            {
                Append32(bytes, f.label);
                Append32(bytes, static_cast<std::uint32_t>(f.type) | (static_cast<std::uint32_t>(f.flags) << 16));
                Append32(bytes, f.offset);
            }
        }
        data = bytes.size();
        Put32(bytes, 24, static_cast<std::uint32_t>(data));
    }

    // Room for `size` bytes at the data's end; their offset from the data's start.
    std::uint32_t Reserve(std::size_t size)
    {
        const std::size_t at = bytes.size();
        bytes.resize(at + size);
        return static_cast<std::uint32_t>(at - data);
    }
    void Set32(std::uint32_t dataOffset, std::uint32_t value)
    {
        Put32(bytes, data + dataOffset, value);
    }
    // A string at the data's end: a count of UTF-16 units with its NUL, then the units.
    std::uint32_t String(std::string_view text)
    {
        const std::uint32_t at = Reserve(4 + (2 * (text.size() + 1)));
        Set32(at, static_cast<std::uint32_t>(text.size() + 1));
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            bytes[data + at + 4 + (2 * i)] = static_cast<std::byte>(text[i]);
        }
        return at;
    }

    Bytes bytes;
    std::size_t data = 0;
};

inline constexpr std::uint16_t kListOfStructs = 0xC000;

using Cell = std::variant<std::int32_t, float, std::string>;

// A data table: the columns by name, the rows by cell; a column's type is its
// first row's cell's.
inline Bytes Gda(const std::vector<std::string> &columns, const std::vector<std::vector<Cell>> &rows,
                 const std::vector<std::uint32_t> &hashes)
{
    std::vector<Gff4::Field> rowFields;
    for (std::size_t c = 0; c < columns.size(); ++c)
    {
        std::uint16_t type = 5; // s32
        if (!rows.empty() && std::holds_alternative<float>(rows[0][c]))
        {
            type = 8;
        }
        else if (!rows.empty() && std::holds_alternative<std::string>(rows[0][c]))
        {
            type = 14;
        }
        rowFields.push_back({static_cast<std::uint32_t>(10005 + c), type, 0, static_cast<std::uint32_t>(4 * c)});
    }
    Gff4 file("G2DA", "V0.2",
              {{"G2DA", 8, {{10002, 1, kListOfStructs, 0}, {10003, 2, kListOfStructs, 4}}},
               {"COLM", 8, {{10001, 4, 0, 0}, {10999, 0, 0, 4}}},
               {"ROW", static_cast<std::uint32_t>(4 * columns.size()), rowFields}});
    const std::uint32_t top = file.Reserve(8);
    const std::uint32_t columnList = file.Reserve(4 + (8 * columns.size()));
    file.Set32(top, columnList);
    file.Set32(columnList, static_cast<std::uint32_t>(columns.size()));
    for (std::size_t c = 0; c < columns.size(); ++c)
    {
        file.Set32(columnList + 4 + static_cast<std::uint32_t>(8 * c), hashes[c]);
    }
    const std::uint32_t rowList = file.Reserve(4 + (4 * columns.size() * rows.size()));
    file.Set32(top + 4, rowList);
    file.Set32(rowList, static_cast<std::uint32_t>(rows.size()));
    for (std::size_t r = 0; r < rows.size(); ++r)
    {
        for (std::size_t c = 0; c < columns.size(); ++c)
        {
            const auto slot = rowList + 4 + static_cast<std::uint32_t>(4 * ((r * columns.size()) + c));
            if (const auto *i = std::get_if<std::int32_t>(&rows[r][c]))
            {
                file.Set32(slot, static_cast<std::uint32_t>(*i));
            }
            else if (const auto *f = std::get_if<float>(&rows[r][c]))
            {
                std::uint32_t bits = 0;
                std::memcpy(&bits, f, 4);
                file.Set32(slot, bits);
            }
            else
            {
                file.Set32(slot, file.String(std::get<std::string>(rows[r][c])));
            }
        }
    }
    return file.bytes;
}

// A talk table of version 0.2.
inline Bytes Tlk(const std::vector<std::pair<std::uint32_t, std::string>> &strings)
{
    Gff4 file("TLK ", "V0.2",
              {{"TLK", 4, {{19001, 1, kListOfStructs, 0}}}, {"STRN", 8, {{19002, 4, 0, 0}, {19003, 14, 0, 4}}}});
    const std::uint32_t top = file.Reserve(4);
    const std::uint32_t list = file.Reserve(4 + (8 * strings.size()));
    file.Set32(top, list);
    file.Set32(list, static_cast<std::uint32_t>(strings.size()));
    for (std::size_t i = 0; i < strings.size(); ++i)
    {
        file.Set32(list + 4 + static_cast<std::uint32_t>(8 * i), strings[i].first);
        file.Set32(list + 8 + static_cast<std::uint32_t>(8 * i), file.String(strings[i].second));
    }
    return file.bytes;
}

// A GFF3 file: structs of fields, the fields' labels and values, as
// xoreos's gff3file.cpp reads them.
class Gff3
{
  public:
    struct Value
    {
        std::string label;
        std::uint32_t type;
        std::uint32_t data; // the value, or an offset into the field data, a struct's index, a list's offset
    };

    explicit Gff3(std::string type) : type_(std::move(type))
    {
    }

    // A struct of fields; its index.
    std::uint32_t Struct(std::uint32_t id, const std::vector<Value> &values)
    {
        structs_.emplace_back(id, values);
        return static_cast<std::uint32_t>(structs_.size() - 1);
    }
    // Gives a struct made earlier its fields: the top struct, made first, once what it holds is.
    void Set(std::uint32_t index, const std::vector<Value> &values)
    {
        structs_.at(index).second = values;
    }
    std::uint32_t ExoString(std::string_view text)
    {
        const auto at = static_cast<std::uint32_t>(fieldData_.size());
        Append32(fieldData_, static_cast<std::uint32_t>(text.size()));
        AppendText(fieldData_, text);
        return at;
    }
    std::uint32_t ResRef(std::string_view text)
    {
        const auto at = static_cast<std::uint32_t>(fieldData_.size());
        fieldData_.push_back(static_cast<std::byte>(text.size()));
        AppendText(fieldData_, text);
        return at;
    }
    std::uint32_t LocString(std::uint32_t strref)
    {
        const auto at = static_cast<std::uint32_t>(fieldData_.size());
        Append32(fieldData_, 8);
        Append32(fieldData_, strref);
        Append32(fieldData_, 0);
        return at;
    }
    // Bytes of field data as they are; their offset.
    std::uint32_t Raw(const Bytes &bytes)
    {
        const auto at = static_cast<std::uint32_t>(fieldData_.size());
        fieldData_.insert(fieldData_.end(), bytes.begin(), bytes.end());
        return at;
    }
    // A list of structs; its offset in the list indices.
    std::uint32_t List(const std::vector<std::uint32_t> &structs)
    {
        const auto at = static_cast<std::uint32_t>(lists_.size());
        Append32(lists_, static_cast<std::uint32_t>(structs.size()));
        for (const auto s : structs)
        {
            Append32(lists_, s);
        }
        return at;
    }

    // The file, its top struct the first made.
    Bytes Done() const
    {
        std::vector<std::string> labels;
        Bytes structs;
        Bytes fields;
        Bytes indices;
        for (const auto &[id, values] : structs_)
        {
            const auto first = static_cast<std::uint32_t>(fields.size() / 12);
            for (const auto &v : values)
            {
                std::uint32_t label = 0;
                while (label < labels.size() && labels[label] != v.label)
                {
                    ++label;
                }
                if (label == labels.size())
                {
                    labels.push_back(v.label);
                }
                Append32(fields, v.type);
                Append32(fields, label);
                Append32(fields, v.data);
            }
            Append32(structs, id);
            if (values.size() == 1)
            {
                Append32(structs, first);
            }
            else
            {
                Append32(structs, static_cast<std::uint32_t>(indices.size()));
                for (std::uint32_t i = 0; i < values.size(); ++i)
                {
                    Append32(indices, first + i);
                }
            }
            Append32(structs, static_cast<std::uint32_t>(values.size()));
        }
        Bytes labelBytes;
        for (auto label : labels)
        {
            label.resize(16, '\0');
            AppendText(labelBytes, label);
        }
        Bytes out;
        std::string head = type_;
        head.resize(4, ' ');
        AppendText(out, head + "V3.2");
        std::uint32_t at = 56;
        const auto section = [&](const Bytes &bytes, std::uint32_t count) {
            Append32(out, at);
            Append32(out, count);
            at += static_cast<std::uint32_t>(bytes.size());
        };
        section(structs, static_cast<std::uint32_t>(structs_.size()));
        section(fields, static_cast<std::uint32_t>(fields.size() / 12));
        section(labelBytes, static_cast<std::uint32_t>(labels.size()));
        section(fieldData_, static_cast<std::uint32_t>(fieldData_.size()));
        section(indices, static_cast<std::uint32_t>(indices.size()));
        section(lists_, static_cast<std::uint32_t>(lists_.size()));
        for (const Bytes *part :
             std::array<const Bytes *, 6>{&structs, &fields, &labelBytes, &fieldData_, &indices, &lists_})
        {
            out.insert(out.end(), part->begin(), part->end());
        }
        return out;
    }

  private:
    std::string type_;
    std::vector<std::pair<std::uint32_t, std::vector<Value>>> structs_;
    Bytes fieldData_;
    Bytes lists_;
};

// A DDS of 32 bits a pixel, not packed: its largest picture of one colour,
// then a half-size picture of another.
inline Bytes Dds(std::uint32_t width, std::uint32_t height, std::array<std::uint8_t, 4> rgba,
                 std::array<std::uint8_t, 4> smaller)
{
    Bytes out(128);
    std::memcpy(out.data(), "DDS ", 4);
    Put32(out, 4, 124);
    Put32(out, 12, height);
    Put32(out, 16, width);
    Put32(out, 28, 2);
    Put32(out, 76, 32);
    Put32(out, 80, 0x41);
    Put32(out, 88, 32);
    const auto picture = [&out](std::uint32_t w, std::uint32_t h, std::array<std::uint8_t, 4> c) {
        for (std::size_t i = 0; i < std::size_t{w} * h; ++i)
        {
            out.push_back(static_cast<std::byte>(c[2]));
            out.push_back(static_cast<std::byte>(c[1]));
            out.push_back(static_cast<std::byte>(c[0]));
            out.push_back(static_cast<std::byte>(c[3]));
        }
    };
    picture(width, height, rgba);
    picture(std::max(width / 2, 1U), std::max(height / 2, 1U), smaller);
    return out;
}

// A folder of the test's own under the temporary folder, gone with it:
// named at random, as many copies of the tests run at once.
struct TempFolder
{
    std::filesystem::path path;

    TempFolder()
    {
        std::random_device random;
        path = std::filesystem::temp_directory_path() /
               ("dao_files_test_" + std::to_string(random()) + "_" + std::to_string(random()));
        std::filesystem::create_directories(path);
    }
    ~TempFolder()
    {
        try
        {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
        catch (...) // NOLINT(bugprone-empty-catch): a folder left in the temporary folder is no test's failure
        {
        }
    }
    TempFolder(const TempFolder &) = delete;
    TempFolder &operator=(const TempFolder &) = delete;

    void Write(const std::filesystem::path &relative, const Bytes &bytes) const
    {
        const auto full = path / relative;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream out(full, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
};

} // namespace dao::test::files
