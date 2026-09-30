"""The game's data tables (GDA files), read out of its packages.

A GDA file is a GFF4 file of type G2DA (the game keeps 749 in
packages\\core\\data\\2da.erf). The format, as xoreos reads it
(src/aurora/gff4file.cpp, gdafile.cpp) and as checked here on the game's
files 2026-09-30:

- a header: "GFF " "V4.0", the platform "PC  ", the type "G2DA" and its
  version, the number of struct templates, and the offset the data starts
  at (V4.1 puts a shared string table's count and offset before it);
- struct templates, 16 bytes each (a label, the field count, where the
  field declarations are, the struct's size), each field declared in 12
  (its label, its type in the low 16 bits and flags in the high: 0x8000 a
  list, 0x4000 a struct whose template is the type, 0x2000 by reference;
  and its offset in the struct);
- the data: the top struct holds the column list (field 10002, each
  column's name hash in field 10001) and the row list (10003); a row's
  template has one field per column, 10005 on, whose type is the column's.

A column is known by the CRC32 of its lowercased UTF-16 name
(dao/core/GdaHash.h); names come from xoreos's list of known ones
(src/aurora/gdaheaders.cpp, if C:\\Modding\\xoreos is there) or stay hashes.
m2da_base.gda maps the tables' numbers, what dao::gda and the scripts' TABLE_*
constants take, to a worksheet prefix: table 1 is "ABI_", every file whose
name starts with abi_ read as one table (an M2DA, "merged 2DA", the engine's
class for it). 2da_base.gda lists the older single tables the same way.

    python tools/gda.py                write dev/gda_tables.csv: every table's
                                       number, name, file and columns (no data)
    python tools/gda.py --dump NAME    print one table (abi_base) as CSV
"""

import csv
import io
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import script_commands  # noqa: E402

ROOT = os.path.join(HERE, "..")
OUT = os.path.join(ROOT, "dev", "gda_tables.csv")
ARCHIVE = os.path.join(script_commands.GAME, "packages", "core", "data", "2da.erf")
XOREOS_NAMES = r"C:\Modding\xoreos\src\aurora\gdaheaders.cpp"

COLUMN_LIST, ROW_LIST, COLUMN_HASH, COLUMN1 = 10002, 10003, 10001, 10005
TYPES = {
    0: ("u8", "<B"),
    1: ("s8", "<b"),
    2: ("u16", "<H"),
    3: ("s16", "<h"),
    4: ("u32", "<I"),
    5: ("s32", "<i"),
    6: ("u64", "<Q"),
    7: ("s64", "<q"),
    8: ("f32", "<f"),
    9: ("f64", "<d"),
    14: ("string", None),
}


def column_hash(name):
    return zlib.crc32(name.lower().encode("utf-16-le")) & 0xFFFFFFFF


def known_names():
    if not os.path.exists(XOREOS_NAMES):
        return {}
    with open(XOREOS_NAMES, encoding="utf-8", errors="replace") as f:
        return {
            int(h): n for h, n in re.findall(r'\{\s*(\d+)U,\s*"([^"]+)"\s*\}', f.read())
        }


class Gff4:
    def __init__(self, data):
        if data[:4] != b"GFF " or data[4:8] not in (b"V4.0", b"V4.1"):
            raise ValueError("not a GFF4 file")
        if data[8:12] != b"PC  ":
            raise ValueError("not a PC (little-endian) GFF4 file")
        self.d = data
        self.type = data[12:16].decode()
        count = struct.unpack_from("<I", data, 20)[0]
        at = 24
        self.shared = []
        if data[4:8] == b"V4.1":
            scount, soffset = struct.unpack_from("<II", data, 24)
            at = 32
            if scount:
                raise ValueError(
                    "shared strings: not needed for the game's GDAs, not read"
                )
        self.data_offset = struct.unpack_from("<I", data, at)[0]
        at += 4
        self.templates = []
        for i in range(count):
            label, fcount, foffset, size = struct.unpack_from(
                "<4sIII", data, at + 16 * i
            )
            fields = []
            if foffset != 0xFFFFFFFF:
                for j in range(fcount):
                    flabel, tf, off = struct.unpack_from("<III", data, foffset + 12 * j)
                    fields.append((flabel, tf & 0xFFFF, tf >> 16, off))
            self.templates.append((label[::-1].decode(errors="replace"), fields, size))

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
        if not flags & 0x4000:
            raise ValueError("not a struct field")
        template = t
        at = base + off
        count = 1
        if flags & 0x8000:
            loff = self.u32(at)
            if loff == 0xFFFFFFFF:
                return []
            at = self.data_offset + loff
            count = self.u32(at)
            at += 4
        size = 4 if flags & 0x2000 else self.templates[template][2]
        out = []
        for i in range(count):
            p = at + i * size
            if flags & 0x2000:
                ref = self.u32(p)
                if ref == 0xFFFFFFFF:
                    continue
                p = self.data_offset + ref
            out.append((template, p))
        return out

    def value(self, base, field):
        t, flags, off = field
        at = base + off
        if t == 14:
            soff = self.u32(at)
            if soff == 0xFFFFFFFF:
                return ""
            p = self.data_offset + soff
            n = self.u32(p)
            # The game's strings end in a NUL, counted in the length.
            text = self.d[p + 4 : p + 4 + 2 * n].decode("utf-16-le", errors="replace")
            return text.rstrip(chr(0))
        name, fmt = TYPES.get(t, (None, None))
        if fmt is None:
            return None
        return struct.unpack_from(fmt, self.d, at)[0]

    def scalars(self, base, field):
        """The values of a list-of-numbers field (a talk table's Huffman tree
        and bit stream are lists of 32-bit words): the list's offset, from
        the data's start, then its count and the values one after another."""
        t, flags, off = field
        if not flags & 0x8000:
            raise ValueError("not a list field")
        loff = self.u32(base + off)
        if loff == 0xFFFFFFFF:
            return ()
        at = self.data_offset + loff
        count = self.u32(at)
        _, fmt = TYPES[t]
        return struct.unpack_from(f"<{count}{fmt[1]}", self.d, at + 4)


def read_gda(data):
    """(columns [(hash, type name)], rows [[values]])."""
    g = Gff4(data)
    if g.type != "G2DA":
        raise ValueError(f"a {g.type}, not a G2DA")
    top = g.fields(0)
    base = g.data_offset
    columns = []
    for template, p in g.list_of_structs(base, top[COLUMN_LIST]):
        cf = g.fields(template)
        columns.append(g.value(p, cf[COLUMN_HASH]) if COLUMN_HASH in cf else None)
    rows = []
    types = [None] * len(columns)
    for template, p in g.list_of_structs(base, top[ROW_LIST]):
        rf = g.fields(template)
        row = []
        for i in range(len(columns)):
            field = rf.get(COLUMN1 + i)
            if field is None:
                row.append(None)
                continue
            types[i] = TYPES.get(field[0], (f"type{field[0]}", None))[0]
            row.append(g.value(p, field))
        rows.append(row)
    return list(zip(columns, types, strict=True)), rows


def column_name(h, names):
    return names.get(h) or f"#{h:08x}"


def main(argv):
    if not os.path.exists(ARCHIVE):
        print(f"no {os.path.normpath(ARCHIVE)}", file=sys.stderr)
        return 2
    entries = script_commands.erf_entries(ARCHIVE)
    names = known_names()
    sys.stdout.reconfigure(encoding="utf-8")
    if "--dump" in argv:
        want = argv[argv.index("--dump") + 1].lower()
        want = want if want.endswith(".gda") else want + ".gda"
        columns, rows = read_gda(entries[want])
        heads = [column_name(h, names) for h, _ in columns]
        text = []
        if "--names" in argv:
            import tlk  # noqa: PLC0415

            talk = tlk.TalkTable.open()
            text = [i for i, h in enumerate(heads) if h.lower().endswith("strref")]
        w = csv.writer(sys.stdout, lineterminator="\n")
        w.writerow(heads + [heads[i][: -len("StrRef")] + "Text" for i in text])
        for row in rows:
            w.writerow(list(row) + [talk.get(row[i]) or "" for i in text])
        return 0
    # m2da_base: the tables by number, each a worksheet prefix.
    base_cols, base_rows = read_gda(entries["m2da_base.gda"])
    heads = [column_name(h, names).lower() for h, _ in base_cols]
    prefixes = {}
    for r in base_rows:
        rec = dict(zip(heads, r, strict=True))
        prefix = str(rec.get("worksheet") or "").strip().lower()
        if prefix:
            prefixes[prefix] = (rec.get("id"), str(rec.get("label") or "").strip())
    out = io.StringIO()
    w = csv.writer(out, lineterminator="\n")
    w.writerow(["file", "table", "table_name", "rows", "columns"])
    unnamed = total = matched = 0
    for file in sorted(entries):
        if not file.endswith(".gda"):
            continue
        try:
            columns, rows = read_gda(entries[file])
        except ValueError as e:
            print(f"{file}: {e}", file=sys.stderr)
            continue
        total += len(columns)
        unnamed += sum(1 for h, _ in columns if h not in names)
        stem = file[: -len(".gda")]
        match = max((p for p in prefixes if stem.startswith(p)), key=len, default=None)
        table, table_name = prefixes[match] if match else ("", "")
        matched += 1 if match else 0
        w.writerow(
            [
                stem,
                table,
                table_name,
                len(rows),
                " ".join(f"{column_name(h, names)}:{t}" for h, t in columns),
            ]
        )
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(out.getvalue())
    print(
        f"{len([e for e in entries if e.endswith('.gda')])} tables, {total} columns, "
        f"{total - unnamed} named; {matched} files in one of the {len(prefixes)} numbered tables"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
