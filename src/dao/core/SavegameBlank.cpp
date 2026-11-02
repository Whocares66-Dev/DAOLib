#include "dao/core/Gff4.h"
#include "dao/core/Savegame.h"
#include "dao/core/generated/SaveTemplates.h"

#include <cstring>

// Notes on Blank, 2026-11-02 (DAOLib #302).
//
// Write (SavegameWrite.cpp) edits a copy of the file it is given and takes everything from it: the templates
// (a new struct is a blank of a template, its fields found by the template's table), the top struct, and the
// structs a list's elements are paired with. So the writer cannot make a file from none; it needs a GFF4 "SAVE"
// with the 78 templates and a top struct, and nothing else of the file: with every child null and every list
// empty it writes a campaign, area and party afresh from the Save, as it does for a file that lacks them.
//
// The file is laid out as the engine writes a save (read in 538 saves of one player with tools/gff4.py; the
// exe's writer is dev/SAVE_WRITER.md): "GFF V4.0" "PC  " "SAVE" "V1.1", the template count, the offset of the
// data, the template table (label, field count, offset of its fields, size; 16 bytes each, the top struct's
// label four NULs), every template's fields in order (label, type and flags, offset; 12 bytes each), a word of
// 0xFFFFFFFF, and the data, which starts with the top struct. The 78 templates are the same in every save seen,
// so they are a table in code (generated/SaveTemplates.h, tools/gen_save_templates.py). The templates hold no
// value of a save; the top struct here is made by Gff4's Editor::New, which nulls every list, string and
// reference and blanks the structs held in place in turn (the campaign, the hero, the version and the game
// state are), leaving a scalar 0 where the engine's own fresh struct is 0xFF throughout (dev/SAVE_WRITER.md): a
// caller sets every scalar the engine writes, as Write's header says.
//
// [I] Whether the game loads a file so made has not been tried; the file is the shape of the engine's, and
// what a loader reads of an empty top struct is as it reads one with no campaign (dev/SAVES.md).

namespace dao::core::savegame
{

namespace
{

constexpr std::uint32_t kNull = 0xFFFFFFFF;
constexpr std::size_t kHeader = 0x1C; // the signature, version, platform, type, type version, count, data offset
constexpr std::size_t kTemplateBytes = 16;
constexpr std::size_t kFieldBytes = 12;

void Put32(std::vector<std::byte> &out, std::uint32_t value)
{
    const auto *bytes = reinterpret_cast<const std::byte *>(&value);
    out.insert(out.end(), bytes, bytes + 4);
}

void PutText(std::vector<std::byte> &out, const char *text)
{
    const auto *bytes = reinterpret_cast<const std::byte *>(text);
    out.insert(out.end(), bytes, bytes + std::strlen(text));
}

// Everything before the data: the header, the template table, the fields and the word the engine leaves.
std::vector<std::byte> Directory()
{
    using namespace detail;
    const std::size_t fieldsAt = kHeader + (kSaveTemplates.size() * kTemplateBytes);
    const std::size_t dataAt = fieldsAt + (kSaveFields.size() * kFieldBytes) + 4;
    std::vector<std::byte> out;
    out.reserve(dataAt);
    PutText(out, "GFF V4.0PC  SAVEV1.1");
    Put32(out, static_cast<std::uint32_t>(kSaveTemplates.size()));
    Put32(out, static_cast<std::uint32_t>(dataAt));
    for (const SaveTemplate &shape : kSaveTemplates)
    {
        const auto *label = reinterpret_cast<const std::byte *>(shape.label.data());
        out.insert(out.end(), label, label + 4);
        Put32(out, static_cast<std::uint32_t>(shape.count));
        Put32(out, shape.count == 0 ? kNull : static_cast<std::uint32_t>(fieldsAt + (shape.first * kFieldBytes)));
        Put32(out, shape.size);
    }
    for (const SaveField &field : kSaveFields)
    {
        Put32(out, field.label);
        Put32(out, field.type | (static_cast<std::uint32_t>(field.flags) << 16));
        Put32(out, field.offset);
    }
    Put32(out, kNull);
    return out;
}

} // namespace

std::vector<std::byte> Blank()
{
    const std::vector<std::byte> directory = Directory();
    // The file with no data yet, only to give the Editor the templates; its New puts the top struct at the
    // data's start, nulled as every blank struct is.
    const auto bare = gff4::File::Parse(directory);
    if (!bare)
    {
        return {};
    }
    gff4::Editor editor(*bare);
    if (!editor.New(0))
    {
        return {};
    }
    return editor.Bytes();
}

} // namespace dao::core::savegame
