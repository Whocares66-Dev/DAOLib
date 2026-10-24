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
            strref = g.value(p, f[STRING_ID])
            if strref != 0xFFFFFFFF:  # an empty slot of the table
                self.strings[strref] = g.value(p, f[STRING_TEXT]) or ""

    @classmethod
    def open(cls, lang="en-us"):
        with open(os.path.join(TABLES, f"core_{lang}.tlk"), "rb") as f:
            return cls(f.read())

    def __len__(self):
        return len(self.strings)

    def get(self, strref):
        return self.strings.get(strref)


NONE = 0xFFFFFFFF  # an empty slot's id, and the offset of no text


def hash_id(strref):
    """The game's hash of a string id (CTlkTable's find, 0x004E47F0): its
    remainder by a table's slots is where the game's search for it starts."""
    a = ((strref << 15) + (~strref & NONE)) & NONE
    c = (a >> 12) ^ a
    a = (c * 5) & NONE
    c = (a >> 4) ^ a
    c = (c * 0x809) & NONE
    return (c >> 16) ^ c


def write(strings):
    """A version 0.2 talk table of [(id, text)], as bytes, laid out byte for
    byte as the game's own are (dao/core/TalkTable.cpp has the rules, read
    2026-10-24 over all 1,501): GFF V4.0 PC, type "TLK " V0.2, a TLK top
    struct holding the string list, an open hash table of STRN, each the id
    and an offset to the text."""
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
    head = b"GFF V4.0PC  TLK V0.2" + struct.pack("<II", len(templates), data_offset)
    texts = dict(strings)
    if len(texts) != len(strings) or NONE in texts:
        raise ValueError("an id given twice, or an id of 0xFFFFFFFF")
    if not texts:
        return head + tmpl + decls + struct.pack("<I", NONE)
    # The list starts right after the top struct: a third more slots than
    # strings, the ids put in from the lowest at their hash's slot or the next
    # free one.
    count = len(texts) * 4 // 3
    slots = [NONE] * count
    for strref in sorted(texts):
        at = hash_id(strref) % count
        while slots[at] != NONE:
            at = (at + 1) % count
        slots[at] = strref
    body = bytearray(struct.pack("<II", 4, count))
    listed = len(body)
    body += bytes(8 * count)
    first = {}
    for i, strref in enumerate(slots):
        offset = NONE
        text = texts.get(strref, "")
        if strref != NONE and text:
            if text not in first:
                body += b"\xff" * (-len(body) % 4)
                first[text] = len(body)
                raw = (text + "\0").encode("utf-16-le")
                body += struct.pack("<I", len(raw) // 2) + raw
            offset = first[text]
        struct.pack_into("<II", body, listed + 8 * i, strref, offset)
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
