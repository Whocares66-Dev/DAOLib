"""Write src/dao/core/generated/SaveTemplates.h from the struct templates of a real .das.

The 78 templates of a save are the engine's own struct definitions and the same in every save (538 of
a player's checked: one table), so dao::core::savegame::Blank makes a save from this table with no file
to copy. The templates hold no player data. The header is checked in, so the library builds with no
save and without Python.

    python tools/gen_save_templates.py SAVE.das               rewrite SaveTemplates.h from a save
    python tools/gen_save_templates.py SAVE.das SAVE.das.met  also SaveMetaTemplates.h from its metadata

The metadata's one template (type SVMT) is the same in every .das.met and holds no player data either:
dao::core::SaveMeta::Blank makes a metadata file from it.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gff4  # noqa: E402

OUT = os.path.join(HERE, "..", "src", "dao", "core", "generated", "SaveTemplates.h")

HEAD = """// Written by tools/gen_save_templates.py from the templates of a .das; do not edit.
//
// The 78 struct templates of a saved game, in the file's order: the engine's own definitions, the
// same in every save, with each field's label (dev/gff4_fields.csv), type, flags and offset.
// Savegame.h's Blank writes them as the exe does.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dao::core::savegame::detail
{

struct SaveField
{
    std::uint32_t label;
    std::uint16_t type;
    std::uint16_t flags;
    std::uint32_t offset;
};

struct SaveTemplate
{
    std::array<char, 4> label; // NULs for the top struct
    std::uint32_t size;
    std::size_t first; // the index in kSaveFields of its first field
    std::size_t count;
};

"""

META_OUT = os.path.join(
    HERE, "..", "src", "dao", "core", "generated", "SaveMetaTemplates.h"
)

META_HEAD = """// Written by tools/gen_save_templates.py from the template of a .das.met; do not edit.
//
// The struct template of a save's metadata (GFF4 type SVMT): the engine's own definition, the same in every
// .das.met, with each field's label (dev/gff4_fields.csv), type, flags and offset.
// SaveMeta::Blank writes it as the exe does.
#pragma once

#include "dao/core/generated/SaveTemplates.h"

#include <array>

namespace dao::core::detail
{

"""

NUL = chr(0)


def char_literal(c):
    return "'" + chr(92) + "0'" if c == NUL else f"'{c}'"


def write_meta(path):
    with open(path, "rb") as f:
        g = gff4.Gff4(f.read())
    if len(g.templates) != 1:
        raise SystemExit("a .das.met has one template")
    label, flds, size = g.templates[0]
    chars = ", ".join(char_literal(c) for c in label.ljust(4, NUL)[:4])
    out = [META_HEAD]
    out.append(
        f"inline constexpr std::array<savegame::detail::SaveField, {len(flds)}> kMetaFields{{{{\n"
    )
    out.extend(f"    {{{lab}, {t}, 0x{fl:04X}, {off}}},\n" for lab, t, fl, off in flds)
    out.append("}};\n\n")
    out.append(
        "inline constexpr std::array<savegame::detail::SaveTemplate, 1> kMetaTemplates{{\n"
    )
    out.append(f"    {{{{{{{chars}}}}}, {size}, 0, {len(flds)}}},\n")
    out.append("}};\n\n} // namespace dao::core::detail" + "\n")
    with open(META_OUT, "w", newline="\r\n") as f:
        f.write("".join(out))


def main(argv):
    if len(argv) not in (2, 3):
        print(__doc__)
        return 2
    if len(argv) == 3:
        write_meta(argv[2])
    with open(argv[1], "rb") as f:
        g = gff4.Gff4(f.read())
    fields = []
    rows = []
    for label, flds, size in g.templates:
        rows.append((label, size, len(fields), len(flds)))
        fields.extend(flds)
    out = [HEAD]
    out.append(
        f"inline constexpr std::array<SaveField, {len(fields)}> kSaveFields{{{{\n"
    )
    out.extend(
        f"    {{{lab}, {t}, 0x{fl:04X}, {off}}},\n" for lab, t, fl, off in fields
    )
    out.append("}};\n\n")
    out.append(
        f"inline constexpr std::array<SaveTemplate, {len(rows)}> kSaveTemplates{{{{\n"
    )
    for label, size, first, count in rows:
        chars = ", ".join(char_literal(c) for c in label.ljust(4, NUL)[:4])
        out.append(f"    {{{{{{{chars}}}}}, {size}, {first}, {count}}},\n")
    out.append("}};\n\n} // namespace dao::core::savegame::detail\n")
    with open(OUT, "w", newline="\r\n") as f:
        f.write("".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
