"""A plugin's own talk table, written as the game's are.

A talk table is a GFF4 file of type "TLK " version 0.2: the top struct's string list (field
19001), each string's id (19002) and its text (19003), laid out byte for byte as every one of the
game's 1,501 (dao/core/TalkTable.cpp has the rules, DAOLib #106). The game's tables are read
through dao/core/TalkTable.h, and from Python through tools/gff4.py; this writes one.

    python tools/tlk.py --write IN.csv OUT.tlk     a plugin's own talk table (dao/Strings.h)

IN.csv is id,text rows (UTF-8, a header row of "id,text" allowed).
"""

import csv
import struct
import sys

STRING_LIST, STRING_ID, STRING_TEXT = 19001, 19002, 19003


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
    if "--write" not in argv or len(argv) < argv.index("--write") + 3:
        print("python tools/tlk.py --write IN.csv OUT.tlk", file=sys.stderr)
        return 2
    i = argv.index("--write")
    source, target = argv[i + 1], argv[i + 2]
    strings = read_csv(source)
    with open(target, "wb") as f:
        f.write(write(strings))
    print(f"{len(strings)} strings written to {target}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
