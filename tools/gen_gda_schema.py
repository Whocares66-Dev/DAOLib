"""Write src/dao/core/generated/GdaSchema.h and .cpp from the scanned worksheets.

Every worksheet the game merges (dev/gda_schema.csv, which tools/gda_schema writes from an
install) becomes an entry of the catalogue, its columns each with its name, hash, type and kind
(a strref, a resource with its extension, a worksheet's label, a row of another table: what
dev/gda_references.csv, kept by hand, adds to the scan or says in its place). A worksheet is
named once: by the scripts' TABLE_* constant where it has one (dao::script::Table, as the scripts
and the callers already name it), by m2da_base's label where it has none (tables::Sheet). The
worksheets the callers read by column (TYPED) get a row class, each column a typed accessor that
gives none where a table lacks the column.

The files are checked in, so the library builds without Python or an install; ctest runs this
with --check, which fails if they are not what the tables give.

    python tools/gen_gda_schema.py            rewrite the files
    python tools/gen_gda_schema.py --check    fail if they would change
"""

import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
SCHEMA = os.path.join(ROOT, "dev", "gda_schema.csv")
REFERENCES = os.path.join(ROOT, "dev", "gda_references.csv")
CONSTANTS = os.path.join(ROOT, "src", "dao", "generated", "ScriptConstants.h")
OUT_H = os.path.join(ROOT, "src", "dao", "core", "generated", "GdaSchema.h")
OUT_CPP = os.path.join(ROOT, "src", "dao", "core", "generated", "GdaSchema.cpp")

# The worksheets callers read by column (DAOLib #106's survey of DAOTurnBased, Eluvian, Paragon,
# ElgaraShaders and DAOLib, 2026-10-24): abilities, appearances, classes, items, portraits, visual
# effects, placeables, properties, item stat defines, item stats, effects, projectiles.
TYPED = [1, 2, 3, 6, 7, 8, 93, 98, 109, 110, 136, 1006]

TYPES = {
    "u8": ("U8", "std::uint8_t", "Whole"),
    "s8": ("S8", "std::int8_t", "Whole"),
    "u16": ("U16", "std::uint16_t", "Whole"),
    "s16": ("S16", "std::int16_t", "Whole"),
    "u32": ("U32", "std::uint32_t", "Whole"),
    "s32": ("S32", "std::int32_t", "Whole"),
    "u64": ("U64", "std::uint64_t", "Whole"),
    "s64": ("S64", "std::int64_t", "Whole"),
    "f32": ("F32", "float", "Real"),
    "f64": ("F64", "double", "Real"),
    "string": ("String", "std::wstring_view", "Text"),
}
KINDS = {
    "plain": "Plain",
    "strref": "StrRef",
    "resource": "Resource",
    "worksheet": "Worksheet",
    "row": "Row",
}
KEYWORDS = {
    "default",
    "delete",
    "new",
    "class",
    "int",
    "float",
    "double",
    "char",
    "short",
    "long",
    "bool",
    "auto",
}
RESERVED = {"Table", "Row", "Of", "RowView", "Whole", "Real", "Text", "Cell"}


def table_names():
    text = open(CONSTANTS, encoding="utf-8").read()
    block = text[text.index("enum class Table") :]
    block = block[: block.index("};")]
    names = {}
    for name, value in re.findall(r"(\w+) = (\d+),", block):
        names.setdefault(
            int(value), name
        )  # the first of an id's names, in the enum's order
    return names


def identifier(text):
    words = re.split(r"[^0-9A-Za-z]+", text)
    name = "".join(w[:1].upper() + w[1:] for w in words if w)
    return name if name and not name[0].isdigit() else "Sheet" + name


def accessor(column):
    name = re.sub(r"[^0-9A-Za-z_]", "_", column)
    if not name or name[0].isdigit() or name in KEYWORDS or name in RESERVED:
        name = "Column_" + name
    return name


def read():
    worksheets = {}
    for r in csv.DictReader(open(SCHEMA, encoding="utf-8", newline="")):
        sheet = worksheets.setdefault(
            int(r["id"]), {"label": r["label"], "prefix": r["prefix"], "columns": []}
        )
        sheet["columns"].append(
            {
                "name": r["column"],
                "hash": int(r["hash"], 16),
                "type": r["type"],
                "kind": r["kind"],
                "extension": r["extension"],
                "target": -1,
            }
        )
    if os.path.exists(REFERENCES):
        for r in csv.DictReader(open(REFERENCES, encoding="utf-8", newline="")):
            sheet = worksheets.get(int(r["id"]))
            column = (
                next((c for c in sheet["columns"] if c["name"] == r["column"]), None)
                if sheet
                else None
            )
            if column is None:
                raise SystemExit(
                    f"gda_references.csv: no column {r['column']} in worksheet {r['id']}"
                )
            column["kind"] = r["kind"]
            column["extension"] = r["extension"]
            column["target"] = int(r["target"]) if r["target"] else -1
    return worksheets


def names_of(worksheets, tables):
    names = {}
    taken = set(tables.values())
    plain = {}
    for i, sheet in worksheets.items():
        if i not in tables:
            plain.setdefault(identifier(sheet["label"]), []).append(i)
    for name, ids in plain.items():
        for i in ids:
            names[i] = name if len(ids) == 1 and name not in taken else f"{name}{i}"
    return names


def quote(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


HEAD = """// Written by tools/gen_gda_schema.py from dev/gda_schema.csv (tools/gda_schema, the GOG
// install's worksheets scanned) and dev/gda_references.csv; do not edit.
"""


def header(worksheets, tables, sheets):
    out = [HEAD]
    out.append(
        """//
// The game's merged data tables, as the scripts' TABLE_* and m2da_base name them: each one's
// columns and what each holds, and a typed row for the tables callers read by column. A mod may
// add tables and columns: read those through dao/core/M2da.h as ever.
//
//   const auto *abilities = dao::core::tables::Find(1);
//   if (const auto row = dao::core::tables::AbilitiesTalents::Of(*merged, 10000))
//       const std::optional<std::int32_t> name = row->NameStrRef();
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/TableRow.h"
#include "dao/generated/ScriptConstants.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace dao::core::tables
{

// What a column's values are.
enum class Kind : std::uint8_t
{
    Plain,
    StrRef,    // a talk table's string id
    Resource,  // a file's name, without its extension
    Worksheet, // a worksheet's label
    Row,       // the ID of a row of another worksheet, Column::target
};

struct Column
{
    std::string_view name; // as the game's files spell it: its hash's name
    std::uint32_t hash = 0;
    gff4::Type type = gff4::Type::S32;
    Kind kind = Kind::Plain;
    std::string_view extension; // a Resource's, without its dot
    std::int64_t target = -1;   // a Row's worksheet
};

struct Worksheet
{
    std::int64_t id = 0;
    std::string_view label;  // m2da_base's
    std::string_view prefix; // its files' names start with it
    std::string_view name;   // the scripts' TABLE_* constant's, or Sheet's
    std::span<const Column> columns;
};

// Returns every worksheet the game merges, by ID.
std::span<const Worksheet> Worksheets();
// Returns a worksheet by its ID; null for one the scan did not see (a mod's).
const Worksheet *Find(std::int64_t id);
// Returns a worksheet's column by its name, in any case; null for none.
const Column *ColumnOf(const Worksheet &worksheet, std::string_view name);

// The worksheets with no TABLE_* constant of the scripts', by m2da_base's label: with
// dao::script::Table, one name for each ID.
enum class Sheet : std::int64_t
{
"""
    )
    out.extend(
        f"    {sheets[i]} = {i}, // {worksheets[i]['label']}\n" for i in sorted(sheets)
    )
    out.append("};\n")
    for i in TYPED:
        sheet = worksheets[i]
        name = tables.get(i) or sheets[i]
        enum = (
            f"dao::script::Table::{tables[i]}" if i in tables else f"Sheet::{sheets[i]}"
        )
        out.append(
            f"\n// A row of {sheet['label']} ({sheet['prefix']}*, worksheet {i}).\n"
        )
        out.append(
            f"class {name} : public RowView\n{{\n  public:\n    using RowView::RowView;\n"
        )
        out.append(
            f"    static constexpr std::int64_t kId = static_cast<std::int64_t>({enum});\n\n"
        )
        out.append(
            "    // Returns the row the game takes for an ID; none for an ID no row has.\n"
        )
        out.append(
            f"    static std::optional<{name}> Of(const m2da::Merged &table, std::int64_t id)\n    {{\n"
        )
        out.append("        const m2da::Row *row = m2da::Taken(table, id);\n")
        out.append(
            f"        return row != nullptr ? std::optional<{name}>({name}(table, *row)) : std::nullopt;\n    }}\n\n"
        )
        for c in sheet["columns"]:
            if c["type"] not in TYPES or c["name"].startswith("0x"):
                continue
            _, cpp, read_as = TYPES[c["type"]]
            template = "" if read_as == "Text" else f"<{cpp}>"
            out.append(
                f"    std::optional<{cpp}> {accessor(c['name'])}() const\n    {{\n"
            )
            out.append(
                f"        return {read_as}{template}(0x{c['hash']:08X}U);\n    }}\n"
            )
        out.append("};\n")
    out.append("\n} // namespace dao::core::tables\n")
    return "".join(out)


def source(worksheets, tables, sheets):
    out = [
        HEAD,
        '#include "dao/core/generated/GdaSchema.h"\n\n#include "dao/core/Install.h"\n\n#include <algorithm>\n#include <array>\n\n',
    ]
    out.append("namespace dao::core::tables\n{\n\nnamespace\n{\n\n")
    for i in sorted(worksheets):
        cols = worksheets[i]["columns"]
        out.append(f"constexpr std::array<Column, {len(cols)}> kColumns{i}{{{{\n")
        for c in cols:
            kind = KINDS[c["kind"]]
            out.append(
                f"    {{{quote(c['name'])}, 0x{c['hash']:08X}U, gff4::Type::{TYPES[c['type']][0]}, Kind::{kind}, "
                f"{quote(c['extension'])}, {c['target']}}},\n"
            )
        out.append("}};\n")
    out.append(
        f"\nconstexpr std::array<Worksheet, {len(worksheets)}> kWorksheets{{{{\n"
    )
    for i in sorted(worksheets):
        sheet = worksheets[i]
        name = tables.get(i) or sheets[i]
        out.append(
            f"    {{{i}, {quote(sheet['label'])}, {quote(sheet['prefix'])}, {quote(name)}, kColumns{i}}},\n"
        )
    out.append("}};\n\n} // namespace\n\n")
    out.append(
        """std::span<const Worksheet> Worksheets()
{
    return kWorksheets;
}

const Worksheet *Find(std::int64_t id)
{
    const auto found = std::ranges::lower_bound(kWorksheets, id, {}, &Worksheet::id);
    return found != kWorksheets.end() && found->id == id ? &*found : nullptr;
}

const Column *ColumnOf(const Worksheet &worksheet, std::string_view name)
{
    const std::string wanted = install::Lower(name);
    const auto found = std::ranges::find_if(
        worksheet.columns, [&](const Column &column) { return install::Lower(column.name) == wanted; });
    return found != worksheet.columns.end() ? &*found : nullptr;
}

} // namespace dao::core::tables
"""
    )
    return "".join(out)


def main(argv):
    worksheets = read()
    tables = table_names()
    sheets = names_of(worksheets, tables)
    files = {
        OUT_H: header(worksheets, tables, sheets),
        OUT_CPP: source(worksheets, tables, sheets),
    }
    if "--check" in argv:
        # A checkout's line ends, and clang-format's layout of the checked-in copy, may differ:
        # compare what matters, the tokens.
        stale = [
            p
            for p, text in files.items()
            if not os.path.exists(p)
            or open(p, encoding="utf-8").read().split() != text.split()
        ]
        for p in stale:
            print(
                f"{os.path.relpath(p, ROOT)} is not what dev/gda_schema.csv gives: run tools/gen_gda_schema.py"
            )
        return 1 if stale else 0
    os.makedirs(os.path.dirname(OUT_H), exist_ok=True)
    for p, text in files.items():
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(
        f"{len(worksheets)} worksheets, {sum(len(s['columns']) for s in worksheets.values())} columns, {len(TYPED)} typed"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
