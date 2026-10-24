"""Write src/dao/core/generated/Gff3Schema.h and .cpp from the scanned templates.

Every field of each kind of template (dev/gff3_schema_<type>.csv, which tools/gff3_schema writes
from an install, rows of other tables checked against dev/gff3_references.csv's targets) becomes
an entry of the catalogue: its path, label, GFF3 type, how many of the install's templates have
it, and what its values are (a strref, a resource with its extension, a row of a worksheet). And
each path becomes a view, each field a typed accessor that gives none where a file lacks the
field, a struct field a view of its own, a list a vector of its entries' views.

The files are checked in, so the library builds without Python or an install; ctest runs this
with --check, which fails if they are not what the tables give.

    python tools/gen_gff3_schema.py            rewrite the files
    python tools/gen_gff3_schema.py --check    fail if they would change
"""

import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
OUT_H = os.path.join(ROOT, "src", "dao", "core", "generated", "Gff3Schema.h")
OUT_CPP = os.path.join(ROOT, "src", "dao", "core", "generated", "Gff3Schema.cpp")

# Each kind of template, and the name of its top struct's view.
TYPES = {"utc": "Creature"}

GFF3 = {
    "byte": ("Byte", "Number<std::uint8_t>", "std::uint8_t"),
    "char": ("Char", "Number<std::int8_t>", "std::int8_t"),
    "word": ("Word", "Number<std::uint16_t>", "std::uint16_t"),
    "short": ("Short", "Number<std::int16_t>", "std::int16_t"),
    "dword": ("Dword", "Number<std::uint32_t>", "std::uint32_t"),
    "int": ("Int", "Number<std::int32_t>", "std::int32_t"),
    "dword64": ("Dword64", "Number<std::uint64_t>", "std::uint64_t"),
    "int64": ("Int64", "Number<std::int64_t>", "std::int64_t"),
    "float": ("Float", "Number<float>", "float"),
    "double": ("Double", "Number<double>", "double"),
    "string": ("String", "Text", "std::string"),
    "resref": ("ResRef", "ResRef", "std::string"),
    "locstring": ("LocString", "Loc", "gff3::LocString"),
    "strref": ("StrRef", "StrRef", "std::uint32_t"),
    "void": ("Void", "Void", "std::span<const std::byte>"),
    "orientation": ("Orientation", "Floats", "std::vector<float>"),
    "vector": ("Vector", "Floats", "std::vector<float>"),
    "struct": ("Struct", None, None),
    "list": ("List", None, None),
}
KINDS = {"plain": "Plain", "strref": "StrRef", "resource": "Resource", "row": "Row"}
RESERVED = {
    "Struct",
    "Of",
    "Number",
    "Text",
    "ResRef",
    "Loc",
    "StrRef",
    "Floats",
    "Void",
    "Child",
    "List",
}


def identifier(text):
    return re.sub(r"[^0-9A-Za-z]", "", text.replace("[]", "Entry")) or "Top"


def class_of(top, path):
    if not path:
        return top
    return "".join(identifier(part[:1].upper() + part[1:]) for part in path.split("/"))


def accessor(label):
    name = re.sub(r"[^0-9A-Za-z_]", "_", label)
    if not name or name[0].isdigit() or name in RESERVED:
        name = "Field_" + name
    return name


def read(kind):
    paths = {}
    for r in csv.DictReader(
        open(
            os.path.join(ROOT, "dev", f"gff3_schema_{kind}.csv"),
            encoding="utf-8",
            newline="",
        )
    ):
        paths.setdefault(r["path"], []).append(r)
    return paths


def quote(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


HEAD = """// Written by tools/gen_gff3_schema.py from dev/gff3_schema_*.csv (tools/gff3_schema, the GOG
// install's templates scanned) and dev/gff3_references.csv; do not edit.
"""


def view(top, path, fields, paths):
    name = class_of(top, path)
    out = [
        f"\n// {'The top struct' if not path else repr(path)[1:-1]} of a template.\n"
    ]
    out.append(
        f"class {name} : public StructView\n{{\n  public:\n    using StructView::StructView;\n"
    )
    if not path:
        out.append("\n    // Returns a file's top struct; none for a file with none.\n")
        out.append(
            f"    static std::optional<{name}> Of(const gff3::File &file)\n    {{\n"
        )
        out.append("        const auto root = file.Root();\n")
        out.append(
            f"        return root ? std::optional<{name}>({name}(*root)) : std::nullopt;\n    }}\n"
        )
    for f in fields:
        label = f["field"]
        _, read_as, cpp = GFF3[f["gff3type"]]
        method = accessor(label)
        below = (path + "/" if path else "") + label
        if (
            f["gff3type"] in ("struct", "list")
            and (below if f["gff3type"] == "struct" else below + "[]") not in paths
        ):
            continue  # no template of the install has the struct: no view of it
        if f["gff3type"] == "struct":
            out.append(
                f"    std::optional<{class_of(top, below)}> {method}() const\n    {{\n"
            )
            out.append(
                f"        return Child<{class_of(top, below)}>({quote(label)});\n    }}\n"
            )
        elif f["gff3type"] == "list":
            out.append(
                f"    std::vector<{class_of(top, below + '[]')}> {method}() const\n    {{\n"
            )
            out.append(
                f"        return List<{class_of(top, below + '[]')}>({quote(label)});\n    }}\n"
            )
        else:
            out.append(f"    std::optional<{cpp}> {method}() const\n    {{\n")
            out.append(f"        return {read_as}({quote(label)});\n    }}\n")
    out.append("};\n")
    return "".join(out)


def header(all_paths):
    out = [HEAD]
    out.append(
        """//
// The toolset's templates, each kind's fields as the install's templates have them: a catalogue
// of what each field is and holds, and a typed view of each struct. A mod's template may have other
// fields: read those through dao/core/Gff3.h as ever.
//
//   if (const auto creature = dao::core::templates::utc::Creature::Of(*file))
//       const std::optional<std::string> tag = creature->Tag();
#pragma once

#include "dao/core/Gff3.h"
#include "dao/core/TemplateView.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::templates
{

// What a field's values are.
enum class Kind : std::uint8_t
{
    Plain,
    StrRef,   // a talk table's string id
    Resource, // a file's name, without its extension
    Row,      // the ID of a row of a worksheet, Field::target
};

struct Field
{
    std::string_view path;  // the struct it is in: "" the top, "Appearance Data", "ItemList[]"
    std::string_view label; // as the files spell it
    gff3::Type type = gff3::Type::Byte;
    Kind kind = Kind::Plain;
    std::string_view extension; // a Resource's, without its dot
    std::int64_t target = -1;   // a Row's worksheet
    std::size_t files = 0;      // how many of the install's templates of the kind have it
};

// Returns a kind of template's fields, struct by struct; none for a kind with no schema.
// @param type the files' extension, in lower case: "utc"
std::span<const Field> Fields(std::string_view type);
// Returns a field by its struct's path and its label; null for none.
const Field *FieldOf(std::string_view type, std::string_view path, std::string_view label);
"""
    )
    for kind, top in TYPES.items():
        paths = all_paths[kind]
        out.append(f"\nnamespace {kind}\n{{\n")
        # A struct's view before any view that holds it: the deepest paths first.
        order = sorted(paths, key=lambda p: (-p.count("/") - (1 if p else 0), p))
        out.extend(view(top, path, paths[path], paths) for path in order)
        out.append(f"\n}} // namespace {kind}\n")
    out.append("\n} // namespace dao::core::templates\n")
    return "".join(out)


def source(all_paths):
    out = [
        HEAD,
        '#include "dao/core/generated/Gff3Schema.h"\n\n#include <algorithm>\n#include <array>\n\n',
    ]
    out.append("namespace dao::core::templates\n{\n\nnamespace\n{\n\n")
    for kind, paths in all_paths.items():
        rows = [f for path in sorted(paths) for f in paths[path]]
        out.append(
            f"constexpr std::array<Field, {len(rows)}> k{kind.capitalize()}{{{{\n"
        )
        out.extend(
            f"    {{{quote(f['path'])}, {quote(f['field'])}, gff3::Type::{GFF3[f['gff3type']][0]}, "
            f"Kind::{KINDS[f['kind']]}, {quote(f['extension'])}, {f['target']}, {f['files']}}},\n"
            for f in rows
        )
        out.append("}};\n")
    out.append(
        "\n} // namespace\n\nstd::span<const Field> Fields(std::string_view type)\n{\n"
    )
    out.extend(
        f'    if (type == "{kind}")\n    {{\n        return k{kind.capitalize()};\n    }}\n'
        for kind in all_paths
    )
    out.append("    return {};\n}\n\n")
    out.append(
        """const Field *FieldOf(std::string_view type, std::string_view path, std::string_view label)
{
    const std::span<const Field> fields = Fields(type);
    const auto found =
        std::ranges::find_if(fields, [&](const Field &field) { return field.path == path && field.label == label; });
    return found != fields.end() ? &*found : nullptr;
}

} // namespace dao::core::templates
"""
    )
    return "".join(out)


def main(argv):
    all_paths = {kind: read(kind) for kind in TYPES}
    files = {OUT_H: header(all_paths), OUT_CPP: source(all_paths)}
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
                f"{os.path.relpath(p, ROOT)} is not what dev/gff3_schema_*.csv gives: run tools/gen_gff3_schema.py"
            )
        return 1 if stale else 0
    for p, text in files.items():
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(
        f"{sum(len(f) for paths in all_paths.values() for f in paths.values())} fields of {len(TYPES)} kinds"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
