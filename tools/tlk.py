"""The game's text, out of its talk tables.

Every name and description the game shows is a string reference (a
StrRef, like abi_base's NameStrRef) into a talk table:
packages\\core\\data\\talktables\\core_en-us.tlk, its languages, and one per
add-on. A talk table is a GFF4 file of type "TLK " (tools/gff4.py reads
GFF4), and all 1,501 in the GOG install are version 0.2 (checked
2026-09-30): the top struct's string list (field 19001), each string's id
(19002) and its text (19003), as xoreos reads them
(src/aurora/talktable_gff.cpp). Version 0.5, which Huffman-codes the text
and which xoreos also reads, does not occur here.

    python tools/tlk.py 379897 ...          print those strings
    python tools/tlk.py --lang de-de 379897 in another language
    python tools/tlk.py --count             how many strings there are
    python tools/tlk.py --write IN.csv OUT.tlk
                                            a plugin's own talk table

--write makes a talk table of a plugin's own (dao/Strings.h) from a CSV of
id,text rows (UTF-8, a header row of "id,text" allowed), laid out as the
game's own: the same templates, labels and field types, so the library's
reader and the game's read it alike.
"""

import csv
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gff4  # noqa: E402
import script_commands  # noqa: E402

TABLES = os.path.join(script_commands.GAME, "packages", "core", "data", "talktables")
STRING_LIST, STRING_ID, STRING_TEXT = 19001, 19002, 19003


class TalkTable:
    def __init__(self, data):
        g = gff4.Gff4(data)
        if g.type != "TLK ":
            raise ValueError(f"a {g.type!r}, not a talk table")
        top = g.fields(0)
        if STRING_LIST not in top:
            raise ValueError(
                "no string list (field 19001): not a version 0.2 talk table"
            )
        self.strings = {}
        for template, p in g.list_of_structs(g.data_offset, top[STRING_LIST]):
            f = g.fields(template)
            self.strings[g.value(p, f[STRING_ID])] = g.value(p, f[STRING_TEXT])

    @classmethod
    def open(cls, lang="en-us"):
        with open(os.path.join(TABLES, f"core_{lang}.tlk"), "rb") as f:
            return cls(f.read())

    def __len__(self):
        return len(self.strings)

    def get(self, strref):
        return self.strings.get(strref)


def write(strings):
    """A version 0.2 talk table of [(id, text)], as bytes: GFF V4.0 PC,
    type "TLK " V0.2, a TLK top struct holding the string list, each a STRN
    of the id and an offset to the text, as the game's own are."""
    templates = [
        (b"TLK ", [(STRING_LIST, 1, 0x8000 | 0x4000, 0)], 4),
        (b"STRN", [(STRING_ID, 4, 0, 0), (STRING_TEXT, 14, 0, 4)], 8),
    ]
    fields_at = 28 + 16 * len(templates)
    decls = tmpl = b""
    for label, fields, size in templates:
        tmpl += struct.pack("<4sIII", label, len(fields), fields_at + len(decls), size)
        for flabel, t, flags, off in fields:
            decls += struct.pack("<III", flabel, (flags << 16) | t, off)
    data_offset = fields_at + len(decls)
    body = bytearray(4)
    # The list starts right after the top struct.
    struct.pack_into("<I", body, 0, len(body))
    body += struct.pack("<I", len(strings))
    at = len(body)
    body += bytes(8 * len(strings))
    for i, (strref, text) in enumerate(strings):
        raw = (text + "\0").encode("utf-16-le")
        struct.pack_into("<II", body, at + 8 * i, strref, len(body))
        body += struct.pack("<I", len(raw) // 2) + raw
    head = b"GFF V4.0PC  TLK V0.2" + struct.pack("<II", len(templates), data_offset)
    return head + tmpl + decls + bytes(body)


def read_csv(path):
    """[(id, text)] from a CSV of id,text rows; a header row is skipped."""
    out = []
    with open(path, encoding="utf-8-sig", newline="") as f:
        for row in csv.reader(f):
            if not row or not row[0].strip():
                continue
            if not row[0].strip().lstrip("-").isdigit():
                continue  # the header
            out.append((int(row[0]), row[1] if len(row) > 1 else ""))
    return out


def main(argv):
    if "--write" in argv:
        i = argv.index("--write")
        source, target = argv[i + 1], argv[i + 2]
        strings = read_csv(source)
        with open(target, "wb") as f:
            f.write(write(strings))
        print(f"{len(strings)} strings written to {target}")
        return 0
    lang = "en-us"
    if "--lang" in argv:
        i = argv.index("--lang")
        lang = argv[i + 1]
        argv = argv[:i] + argv[i + 2 :]
    table = TalkTable.open(lang)
    sys.stdout.reconfigure(encoding="utf-8")
    if "--count" in argv:
        print(f"{len(table)} strings in core_{lang}.tlk")
        return 0
    for a in argv:
        print(f"{a}\t{table.get(int(a))}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
