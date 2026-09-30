"""The game's text, out of its talk tables.

Every name and description the game shows is a string reference (a
StrRef, like abi_base's NameStrRef) into a talk table:
packages\\core\\data\\talktables\\core_en-us.tlk, its languages, and one per
add-on. A talk table is a GFF4 file of type "TLK " (tools/gda.py reads
GFF4), and all 1,501 in the GOG install are version 0.2 (checked
2026-09-30): the top struct's string list (field 19001), each string's id
(19002) and its text (19003), as xoreos reads them
(src/aurora/talktable_gff.cpp). Version 0.5, which Huffman-codes the text
and which xoreos also reads, does not occur here.

    python tools/tlk.py 379897 ...          print those strings
    python tools/tlk.py --lang de-de 379897 in another language
    python tools/tlk.py --count             how many strings there are
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gda  # noqa: E402
import script_commands  # noqa: E402

TABLES = os.path.join(script_commands.GAME, "packages", "core", "data", "talktables")
STRING_LIST, STRING_ID, STRING_TEXT = 19001, 19002, 19003


class TalkTable:
    def __init__(self, data):
        g = gda.Gff4(data)
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


def main(argv):
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
