"""The game's GFF version 4 files, read whole: talk tables, data tables,
save games (.das, type "SAVE"), their metadata (.das.met, "SVMT"), and the
toolset's templates.

    python tools/gff4.py --dump FILE [--depth N] [--items N]
    python tools/gff4.py --templates FILE
    python tools/gff4.py --import-names [XOREOS]   rewrite dev/gff4_fields.csv

A GFF4 file is a header, a table of struct templates (each a four-letter
label, a size, and its fields: a numeric label, a type, flags and an
offset in the struct) and a data block the structs sit in. A field's
flags say whether it is a list (0x8000), a struct (0x4000, its type then
being the template's index) and a reference (0x2000, the slot holding an
offset into the data instead of the value). Strings are always reached
through an offset; a "generic" field holds 8-byte elements, each a type
and flags and a value or an offset. Read 2026-09-30 against the game's
files, with xoreos's reader (src/aurora/gff4file.cpp) as the account of
the format.

Field labels are numbers. dev/gff4_fields.csv names them: xoreos's table
(src/aurora/gff4fields.h, GPL version 3 or later, as DAOLib is), taken
from a clone by --import-names with xoreos's notice kept at the file's
head.
"""

import csv
import os
import re
import struct
import subprocess
import sys

LIST, STRUCT, REFERENCE = 0x8000, 0x4000, 0x2000
NULL = 0xFFFFFFFF

# type: (name, struct format of one value, size)
TYPES = {
    0: ("u8", "<B", 1),
    1: ("s8", "<b", 1),
    2: ("u16", "<H", 2),
    3: ("s16", "<h", 2),
    4: ("u32", "<I", 4),
    5: ("s32", "<i", 4),
    6: ("u64", "<Q", 8),
    7: ("s64", "<q", 8),
    8: ("f32", "<f", 4),
    9: ("f64", "<d", 8),
    10: ("vector3", "<3f", 12),
    12: ("vector4", "<4f", 16),
    13: ("quaternion", "<4f", 16),
    14: ("string", None, 4),
    15: ("color", "<4f", 16),
    16: ("matrix", "<16f", 64),
    17: ("tlkstring", "<2I", 8),
}
GENERIC = 0xFFFF
GENERIC_SIZE = 8

HERE = os.path.dirname(os.path.abspath(__file__))
NAMES = os.path.join(HERE, "..", "dev", "gff4_fields.csv")
XOREOS = os.environ.get("DAO_XOREOS", r"C:\Modding\xoreos")


def field_names(path=NAMES):
    """{label: name} from dev/gff4_fields.csv, skipping its notice (# lines)."""
    try:
        with open(path, encoding="utf-8", newline="") as f:
            rows = csv.reader(line for line in f if not line.startswith("#"))
            next(rows, None)
            return {int(label): name for label, name in rows}
    except OSError:
        return {}


def parse_xoreos(text):
    """[(label, name)] from xoreos's gff4fields.h."""
    return [
        (int(n), name) for name, n in re.findall(r"kGFF4(\w+)\s*=\s*(\d+)\s*,", text)
    ]


def import_names(xoreos=XOREOS, out=NAMES):
    """Rewrite dev/gff4_fields.csv from a xoreos clone, its notice first."""
    header = os.path.join(xoreos, "src", "aurora", "gff4fields.h")
    text = open(header, encoding="utf-8").read()
    notice = text[text.index("/*") + 2 : text.index("*/")]
    rows = parse_xoreos(text)
    try:
        commit = subprocess.run(
            ["git", "-C", xoreos, "log", "-1", "--format=%h %cs"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        commit = "unknown commit"
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(
            "# GFF4 field labels and their names, from xoreos's src/aurora/gff4fields.h\n"
        )
        f.write(
            f"# (https://github.com/xoreos/xoreos, {commit}), by tools/gff4.py --import-names. Its notice:\n"
        )
        for line in notice.strip().splitlines():
            f.write("#" + line.lstrip(" *").rstrip().join(["  ", ""]).rstrip() + "\n")
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["label", "name"])
        w.writerows(rows)
    return len(rows)


class Gff4:
    def __init__(self, data):
        if data[:4] != b"GFF " or data[4:8] not in (b"V4.0", b"V4.1"):
            raise ValueError("not a GFF4 file")
        if data[8:12] != b"PC  ":
            raise ValueError("not a PC (little-endian) GFF4 file")
        self.d = data
        self.type = data[12:16].decode()
        self.type_version = data[16:20].decode(errors="replace")
        count = struct.unpack_from("<I", data, 20)[0]
        at = 24
        if data[4:8] == b"V4.1":
            scount = struct.unpack_from("<I", data, 24)[0]
            at = 32
            if scount:
                raise ValueError(
                    "shared strings: none of the game's files use them, not read"
                )
        self.data_offset = struct.unpack_from("<I", data, at)[0]
        at += 4
        self.templates = []
        for i in range(count):
            label, fcount, foffset, size = struct.unpack_from(
                "<4sIII", data, at + 16 * i
            )
            fields = []
            if foffset != NULL:
                for j in range(fcount):
                    flabel, tf, off = struct.unpack_from("<III", data, foffset + 12 * j)
                    fields.append((flabel, tf & 0xFFFF, tf >> 16, off))
            self.templates.append(
                (label.decode(errors="replace").rstrip(chr(0) + " "), fields, size)
            )

    def u32(self, at):
        return struct.unpack_from("<I", self.d, at)[0]

    def fields(self, template):
        return {
            label: (t, flags, off)
            for label, t, flags, off in self.templates[template][1]
        }

    def list_of_structs(self, base, field):
        """[(template, struct offset)] of a struct-list field of the struct at base."""
        t, flags, off = field
        if not flags & STRUCT:
            raise ValueError("not a struct field")
        at = base + off
        count = 1
        if flags & LIST:
            loff = self.u32(at)
            if loff == NULL:
                return []
            at = self.data_offset + loff
            count = self.u32(at)
            at += 4
        size = 4 if flags & REFERENCE else self.templates[t][2]
        out = []
        for i in range(count):
            p = at + i * size
            if flags & REFERENCE:
                ref = self.u32(p)
                if ref == NULL:
                    continue
                p = self.data_offset + ref
            out.append((t, p))
        return out

    def string(self, at):
        """A string whose data offset is at `at`."""
        soff = self.u32(at)
        if soff == NULL:
            return ""
        p = self.data_offset + soff
        n = self.u32(p)
        # The game's strings end in a NUL, counted in the length.
        return (
            self.d[p + 4 : p + 4 + 2 * n]
            .decode("utf-16-le", errors="replace")
            .rstrip(chr(0))
        )

    def value(self, base, field):
        """A scalar or string field of the struct at base."""
        t, flags, off = field
        if t == 14:
            return self.string(base + off)
        name, fmt, _ = TYPES.get(t, (None, None, 0))
        if fmt is None:
            return None
        return struct.unpack_from(fmt, self.d, base + off)[0]

    def scalars(self, base, field):
        """The values of a list-of-numbers field: the list's data offset,
        then its count and the values one after another."""
        t, flags, off = field
        if not flags & LIST:
            raise ValueError("not a list field")
        loff = self.u32(base + off)
        if loff == NULL:
            return ()
        at = self.data_offset + loff
        count = self.u32(at)
        _, fmt, _ = TYPES[t]
        return struct.unpack_from(f"<{count}{fmt[1]}", self.d, at + 4)

    # -- reading everything ------------------------------------------------

    def walk(self, template=0, base=None, depth=64):
        """The struct at base (the top struct by default) as {label: value},
        nested structs as dicts, lists as lists."""
        if base is None:
            base = self.data_offset
        out = {}
        for label, t, flags, off in self.templates[template][1]:
            out[label] = self._field(t, flags, base + off, depth)
        return out

    def _field(self, t, flags, at, depth):
        is_struct = bool(flags & STRUCT)
        is_ref = bool(flags & REFERENCE) and not (t == 14 and not is_struct)
        if flags & LIST:
            loff = self.u32(at)
            if loff == NULL:
                return []
            p = self.data_offset + loff
            count = self.u32(p)
            p += 4
            if t == GENERIC and not is_struct:
                return [
                    self._generic(p + i * GENERIC_SIZE, is_ref, depth)
                    for i in range(count)
                ]
            size = 4 if is_ref else self._size(t, is_struct)
            return [
                self._one(t, is_struct, is_ref, p + i * size, depth)
                for i in range(count)
            ]
        if t == GENERIC and not is_struct:
            return self._generic(at, is_ref, depth)
        return self._one(t, is_struct, is_ref, at, depth)

    def _size(self, t, is_struct):
        if is_struct:
            return self.templates[t][2]
        return GENERIC_SIZE if t == GENERIC else TYPES.get(t, (None, None, 4))[2]

    def _one(self, t, is_struct, is_ref, at, depth):
        if is_ref:
            off = self.u32(at)
            if off == NULL:
                return None
            at = self.data_offset + off
        if is_struct:
            if depth <= 0:
                return "..."
            return {"@": self.templates[t][0], **self.walk(t, at, depth - 1)}
        if t == 14:
            return self.string(at)
        name, fmt, _ = TYPES.get(t, (None, None, 0))
        if fmt is None:
            return f"<type {t}>"
        v = struct.unpack_from(fmt, self.d, at)
        return v[0] if len(v) == 1 else list(v)

    def _generic(self, at, is_ref, depth):
        """One generic element: a type and flags, then the value inline or
        by an offset into the data."""
        tf = self.u32(at)
        t, flags = tf & 0xFFFF, tf >> 16
        slot = at + 4
        if is_ref:
            off = self.u32(slot)
            if off == NULL:
                return None
            slot = self.data_offset + off
        return self._field(t, flags & ~REFERENCE, slot, depth)


def show(value, names, indent="", items=8, out=sys.stdout):
    if isinstance(value, dict):
        for label, v in value.items():
            if label == "@":
                continue
            name = names.get(label, str(label)) if isinstance(label, int) else label
            head = f"{indent}{name}"
            if isinstance(v, dict):
                print(f"{head}: [{v.get('@', '')}]", file=out)
                show(v, names, indent + "  ", items, out)
            elif isinstance(v, list) and v and isinstance(v[0], dict):
                print(f"{head}: {len(v)} x [{v[0].get('@', '')}]", file=out)
                for i, e in enumerate(v[:items]):
                    print(f"{indent}  [{i}]", file=out)
                    show(e, names, indent + "    ", items, out)
                if len(v) > items:
                    print(f"{indent}  ... {len(v) - items} more", file=out)
            else:
                text = repr(
                    v
                    if not isinstance(v, list) or len(v) <= items
                    else v[:items] + ["..."]
                )
                print(f"{head} = {text[:200]}", file=out)


def main(argv):
    if argv[:1] == ["--import-names"]:
        count = import_names(*argv[1:2])
        print(f"wrote {os.path.normpath(NAMES)}: {count} names")
        return 0
    if len(argv) < 2 or argv[0] not in ("--dump", "--templates"):
        print(__doc__)
        return 2
    # The game's text is UTF-16; a Windows console's code page is narrower.
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    g = Gff4(open(argv[1], "rb").read())
    names = field_names()
    print(
        f"GFF4 {g.type} {g.type_version}: {len(g.templates)} templates, field names: {len(names)}"
    )
    if argv[0] == "--templates":
        for i, (label, fields, size) in enumerate(g.templates):
            print(
                f"  {i:3} {label} size {size:#x}: "
                + ", ".join(names.get(f[0], str(f[0])) for f in fields)
            )
        return 0
    depth = int(argv[argv.index("--depth") + 1]) if "--depth" in argv else 64
    items = int(argv[argv.index("--items") + 1]) if "--items" in argv else 8
    show(g.walk(depth=depth), names, items=items)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
