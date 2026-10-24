"""tools/gff4.py as a second reader of core's GFF4 reader, over an install or a profile's saves.

Each file is walked by both readers alike (canonical() here over gff4.py; tools/second_reader's
dao_gff4_leaves over core's gff4::Struct): every field in template order, structs depth first, a
TlkString its strref, a generic list its struct elements; the values as tokens, hashed, and the
two readers' hashes compared file by file. The first run (DAOLib #106, step 6, 2026-10-24) found
both reading a generic string by reference one hop too far (dao/core/Gff4.cpp's GenericElement);
since then 6,288 sampled files of 31 kinds of the GOG install, and 120 of a profile's saves, agree.

    python tools/second_reader.py sample [GAME]          up to 300 files of each kind, under 1 MB
    python tools/second_reader.py folder DIR [MOST]       a folder's GFF4 files (saves), read only
    build\core-x64\dao_gff4_leaves.exe build\second_reader\sample.txt build\second_reader\cpp.txt
    python tools/second_reader.py compare                 fails if any file differs
    python tools/second_reader.py show WHERE              one file's tokens, as gff4.py gives them
"""

import collections
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gff4  # noqa: E402
import script_commands  # noqa: E402

GAME = "C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins"
OUT = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "build", "second_reader"
)
PER_KIND = 300
LARGEST = 1 << 20


def text(s):
    out = []
    for c in s:
        o = ord(c)
        if c == "\\":
            out.append("\\\\")
        elif 0x20 <= o < 0x7F:
            out.append(c)
        else:
            out.extend(
                "\\u" + unit[2:] + unit[:2]
                for unit in c.encode("utf-16-le").hex(" ", 2).split()
            )
    return "".join(out)


def number(v):
    if isinstance(v, bool):
        return "i" + str(int(v))
    if isinstance(v, int):
        return "i" + str(v)
    return "f" + format(v, ".17g")


def leaves(value, out, field_type=None):
    """Tokens of a value of walk()'s: a dict a struct, a list its elements, else a leaf."""
    if value == "...":
        out.append("...")
    elif value is None:
        out.append("none")
    elif isinstance(value, dict):
        out.append("T" + value["@"])
        for label, v in value.items():
            if label != "@":
                out.append("#" + str(label))
                leaves(v, out)
    elif isinstance(value, list):
        out.append("L" + str(len(value)))
        for v in value:
            leaves(v, out)
    elif isinstance(value, str):
        out.append("s" + text(value))
    else:
        out.append(number(value))


def canonical(g):
    """gff4.py's reading of a file, walked as core's is (build/lib-agent-3-leaves.cpp): its
    templates, fields and values as gff4.py reads them (g._one, g.string, TYPES), a TlkString its
    strref, a list of structs its structs that are there, a generic list its elements that are
    structs, a generic value by whatever type it holds."""
    out = []
    d, u32 = g.d, g.u32

    def struct_tokens(template, base, depth):
        if depth < 0:
            out.append("...")
            return
        out.append("T" + g.templates[template][0])
        for label, t, flags, off in g.templates[template][1]:
            out.append("#" + str(label))
            if depth <= 0:
                out.append("...")
                continue
            value(t, flags, base + off, depth)

    def one(t, is_struct, is_ref, at, depth):
        if is_ref:
            off = u32(at)
            if off == gff4.NULL:
                out.append("none")
                return
            at = g.data_offset + off
        if is_struct:
            struct_tokens(t, at, depth - 1)
        elif t == 14:
            out.append("s" + text(g.string(at)))
        elif t == 17:
            out.append("t" + str(u32(at)))
        else:
            name, fmt, _ = gff4.TYPES[t]
            v = struct.unpack_from(fmt, d, at)
            if len(v) == 1:
                out.append(number(v[0]))
            else:
                out.append("L" + str(len(v)))
                out.extend(number(x) for x in v)

    def elements(t, flags, at):
        loff = u32(at)
        if loff == gff4.NULL:
            return 0, at
        p = g.data_offset + loff
        return u32(p), p + 4

    def value(t, flags, at, depth):
        is_struct = bool(flags & gff4.STRUCT)
        is_ref = bool(flags & gff4.REFERENCE) and not (t == 14 and not is_struct)
        if flags & gff4.LIST:
            count, p = elements(t, flags, at)
            if t == gff4.GENERIC and not is_struct:
                kept = []
                for i in range(count):
                    e = p + i * gff4.GENERIC_SIZE
                    tf = u32(e)
                    et, ef = tf & 0xFFFF, tf >> 16
                    slot = e + 4
                    if is_ref:
                        off = u32(slot)
                        if off == gff4.NULL:
                            continue
                        slot = g.data_offset + off
                    if ef & gff4.STRUCT:
                        kept.append((et, ef, slot))
                out.append("L" + str(len(kept)))
                for et, ef, slot in kept:
                    one(et, True, bool(ef & gff4.REFERENCE), slot, depth)
                return
            size = 4 if is_ref else g._size(t, is_struct, listed=True)
            if is_struct:
                there = [
                    p + i * size
                    for i in range(count)
                    if not (is_ref and u32(p + i * size) == gff4.NULL)
                ]
                out.append("L" + str(len(there)))
                for e in there:
                    one(t, True, is_ref, e, depth)
                return
            out.append("L" + str(count))
            for i in range(count):
                one(t, False, is_ref, p + i * size, depth)
            return
        if t == gff4.GENERIC and not is_struct:
            tf = u32(at)
            et, ef = tf & 0xFFFF, tf >> 16
            slot = at + 4
            # As gff4.py's _generic: a string element is not followed through the reference.
            if is_ref and not (et == 14 and not ef & (gff4.STRUCT | gff4.LIST)):
                off = u32(slot)
                if off == gff4.NULL:
                    out.append("none")
                    return
                slot = g.data_offset + off
            value(et, ef & ~gff4.REFERENCE, slot, depth)
            return
        one(t, is_struct, is_ref, at, depth)

    struct_tokens(0, g.data_offset, 64)
    return out


def fnv(tokens):
    h = 0xCBF29CE484222325
    for b in "\n".join(tokens).encode("utf-8"):
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def is_gff4(data):
    return (
        data[:4] == b"GFF "
        and data[4:8] in (b"V4.0", b"V4.1")
        and data[8:12] == b"PC  "
    )


def sample(game=GAME):
    by_kind = collections.defaultdict(list)
    for root, _, files in os.walk(game):
        if "bin_ship" in root.lower():
            continue
        for f in sorted(files):
            path = os.path.join(root, f)
            low = f.lower()
            if low.endswith((".erf", ".rim")):
                with open(path, "rb") as fh:
                    if fh.read(16) != "ERF V2.0".encode("utf-16-le"):
                        continue
                for name, data in sorted(script_commands.erf_entries(path).items()):
                    kind = os.path.splitext(name)[1].lower()
                    if (
                        len(by_kind[kind]) < PER_KIND
                        and len(data) < LARGEST
                        and is_gff4(data)
                    ):
                        by_kind[kind].append((path + "|" + name, data))
            elif os.path.getsize(path) < LARGEST:
                kind = os.path.splitext(low)[1]
                if len(by_kind[kind]) < PER_KIND:
                    with open(path, "rb") as fh:
                        data = fh.read()
                    if is_gff4(data):
                        by_kind[kind].append(("|" + path, data))
    os.makedirs(OUT, exist_ok=True)
    count = 0
    with (
        open(os.path.join(OUT, "sample.txt"), "w", encoding="utf-8") as listing,
        open(os.path.join(OUT, "python.txt"), "w", encoding="utf-8") as hashes,
    ):
        for kind in sorted(by_kind):
            for where, data in by_kind[kind]:
                listing.write(where + "\n")
                try:
                    tokens = canonical(gff4.Gff4(data))
                    hashes.write(f"{where}\t{fnv(tokens):016x}\t{len(tokens)}\n")
                except (ValueError, struct.error, IndexError, KeyError) as e:
                    hashes.write(f"{where}\terror\t{type(e).__name__}: {e}\n")
                count += 1
    print(
        count,
        "files of",
        len(by_kind),
        "kinds:",
        {k: len(v) for k, v in sorted(by_kind.items())},
    )


def compare():
    def read(name):
        out = {}
        for line in open(os.path.join(OUT, name), encoding="utf-8"):
            where, h, n = line.rstrip("\n").split("\t", 2)
            out[where] = (h, n)
        return out

    py, cpp = read("python.txt"), read("cpp.txt")
    same = [w for w in py if w in cpp and py[w][0] == cpp[w][0]]
    differ = [w for w in py if w in cpp and py[w][0] != cpp[w][0]]
    print(
        len(py),
        "files;",
        len(same),
        "the same;",
        len(differ),
        "different;",
        len(set(py) - set(cpp)),
        "not in C++'s",
    )
    kinds = collections.Counter(
        os.path.splitext(w.split("|")[-1])[1].lower() for w in differ
    )
    print("different by kind:", dict(kinds))
    for w in differ[:10]:
        print(w, "python", py[w], "c++", cpp[w])
    return 1 if differ or set(py) - set(cpp) else 0


def show(where):
    archive, name = where.split("|", 1)
    data = (
        script_commands.erf_entries(archive)[name]
        if archive
        else open(name, "rb").read()
    )
    for t in canonical(gff4.Gff4(data)):
        print(t)


def folder(path, most):
    """A folder's GFF4 files (a profile's saves), read only: up to `most` of each extension."""
    kinds = collections.defaultdict(list)
    for root, _, files in os.walk(path):
        for f in sorted(files):
            kind = os.path.splitext(f)[1].lower()
            if len(kinds[kind]) < most:
                p = os.path.join(root, f)
                with open(p, "rb") as fh:
                    if is_gff4(fh.read(12)):
                        kinds[kind].append(p)
    os.makedirs(OUT, exist_ok=True)
    with (
        open(os.path.join(OUT, "sample.txt"), "w", encoding="utf-8") as listing,
        open(os.path.join(OUT, "python.txt"), "w", encoding="utf-8") as hashes,
    ):
        for kind in sorted(kinds):
            for p in kinds[kind]:
                listing.write("|" + p + "\n")
                tokens = canonical(gff4.Gff4(open(p, "rb").read()))
                hashes.write(f"|{p}\t{fnv(tokens):016x}\t{len(tokens)}\n")
    print({k: len(v) for k, v in kinds.items()})


def main(argv):
    if argv[0] == "sample":
        sample(*argv[1:2])
    elif argv[0] == "folder":
        folder(argv[1], int(argv[2]) if len(argv) > 2 else 1000000)
    elif argv[0] == "compare":
        return compare()
    else:
        show(argv[1])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
