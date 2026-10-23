"""tools/gff4.py on a GFF4 file built here: every kind of field the game's
save games use, read back whole by walk()."""

import struct
import unittest

import gff4


def build():
    # Templates: the top struct, and an element struct of one s32.
    top_fields = [
        (1, 4, 0, 0),  # u32
        (2, 14, 0, 4),  # string
        (3, 10, 0, 8),  # vector3
        (4, 1, gff4.LIST | gff4.STRUCT, 20),  # a list of ELEM structs
        (5, 1, gff4.STRUCT | gff4.REFERENCE, 24),  # one ELEM by reference
        (6, gff4.GENERIC, gff4.LIST | gff4.REFERENCE, 28),  # a list of generics
        (7, 10, gff4.LIST, 32),  # a list of vector3, 16 bytes each
    ]
    elem_fields = [(10, 5, 0, 0)]
    templates = [(b"TOP ", top_fields, 36), (b"ELEM", elem_fields, 4)]

    # The data block, offsets from its start.
    s_off, l_off, r_off, g_off = 36, 48, 60, 64
    x_off, y_off, v_off = 84, 88, 92
    top = struct.pack("<II3fIIII", 7, s_off, 1.0, 2.0, 3.0, l_off, r_off, g_off, v_off)
    string = (
        struct.pack("<I", 3) + "hi\0".encode("utf-16-le") + b"\0\0"
    )  # 3 chars, padded to 12
    elements = struct.pack("<Iii", 2, -1, 5)
    referenced = struct.pack("<i", 42)
    generics = struct.pack("<IIIII", 2, 4, x_off, 14, y_off)
    x = struct.pack("<I", 99)
    y = struct.pack("<I", s_off)  # a string element's slot: the string's offset
    vectors = struct.pack("<I8f", 2, 1.0, 2.0, 3.0, 9.0, 4.0, 5.0, 6.0, 9.0)
    data = top + string + elements + referenced + generics + x + y + vectors
    assert len(top + string) == l_off and len(top + string + elements) == r_off
    assert len(top + string + elements + referenced) == g_off
    assert len(top + string + elements + referenced + generics) == x_off
    assert len(top + string + elements + referenced + generics + x + y) == v_off

    header = b"GFF V4.0PC  TESTV1.0" + struct.pack("<I", len(templates))
    table_at = len(header) + 4 + 16 * len(templates)
    field_tables, template_rows = b"", b""
    for label, fields, size in templates:
        offset = table_at + len(field_tables)
        template_rows += struct.pack("<4sIII", label, len(fields), offset, size)
        for flabel, t, flags, off in fields:
            field_tables += struct.pack("<III", flabel, t | (flags << 16), off)
    data_offset = table_at + len(field_tables)
    return header + struct.pack("<I", data_offset) + template_rows + field_tables + data


class Gff4Test(unittest.TestCase):
    def test_walk_reads_every_kind_of_field(self):
        g = gff4.Gff4(build())
        self.assertEqual(g.type, "TEST")
        self.assertEqual([t[0] for t in g.templates], ["TOP", "ELEM"])
        top = g.walk()
        self.assertEqual(top[1], 7)
        self.assertEqual(top[2], "hi")
        self.assertEqual(top[3], [1.0, 2.0, 3.0])
        self.assertEqual(top[4], [{"@": "ELEM", 10: -1}, {"@": "ELEM", 10: 5}])
        self.assertEqual(top[5], {"@": "ELEM", 10: 42})
        self.assertEqual(top[6], [99, "hi"])
        self.assertEqual(top[7], [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]])

    def test_depth_limits_nesting(self):
        top = gff4.Gff4(build()).walk(depth=0)
        self.assertEqual(top[5], "...")

    def test_names_parse_from_xoreos_and_read_back(self):
        import os
        import tempfile

        text = "/* notice */\n\tkGFF4Tag = 1,\n\tkGFF4SavegameCampaign = 16000,\n"
        self.assertEqual(
            gff4.parse_xoreos(text), [(1, "Tag"), (16000, "SavegameCampaign")]
        )
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "src", "aurora"))
            with open(
                os.path.join(d, "src", "aurora", "gff4fields.h"), "w", encoding="utf-8"
            ) as f:
                f.write(text)
            out = os.path.join(d, "names.csv")
            self.assertEqual(gff4.import_names(d, out), 2)
            self.assertEqual(
                gff4.field_names(out), {1: "Tag", 16000: "SavegameCampaign"}
            )
        self.assertEqual(gff4.field_names(os.path.join("no", "such", "file.csv")), {})

    def test_the_committed_table_names_the_save_fields(self):
        names = gff4.field_names()
        self.assertEqual(names.get(16000), "SavegameCampaign")
        self.assertGreater(len(names), 2000)


if __name__ == "__main__":
    unittest.main()
