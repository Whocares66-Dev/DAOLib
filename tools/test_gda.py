"""tools/gda.py on a GDA file built here, in the layout the game's have."""

import struct
import unittest

import gda


def build_gda(columns, rows):
    """A GFF V4.0 PC G2DA: three struct templates (top, column, row), the
    top struct's column and row lists inline, strings after them. columns
    is [(name, 'i' or 's')], rows [[value]]."""
    ncol = len(columns)
    # field types: 5 s32, 14 string; the row template has one field per column
    row_fields = [
        (gda.COLUMN1 + i, 5 if t == "i" else 14, 0, 4 * i)
        for i, (_, t) in enumerate(columns)
    ]
    templates = [
        (
            b"gtop",
            [
                (gda.COLUMN_LIST, 1, 0x8000 | 0x4000, 0),
                (gda.ROW_LIST, 2, 0x8000 | 0x4000, 4),
            ],
            8,
        ),
        (b"colm", [(gda.COLUMN_HASH, 4, 0, 0)], 4),
        (b"row ", row_fields, 4 * ncol),
    ]
    header_size = 28
    fields_at = header_size + 16 * len(templates)
    decls = b""
    tmpl = b""
    for label, fields, size in templates:
        tmpl += struct.pack(
            "<4sIII", label[::-1], len(fields), fields_at + len(decls), size
        )
        for flabel, t, flags, off in fields:
            decls += struct.pack("<III", flabel, (flags << 16) | t, off)
    data_offset = fields_at + len(decls)
    # data: top (8 bytes), column list, row list, strings
    body = bytearray(8)
    col_list = len(body)
    body += struct.pack("<I", ncol)
    for name, _ in columns:
        body += struct.pack("<I", gda.column_hash(name))
    row_list = len(body)
    body += struct.pack("<I", len(rows))
    row_at = len(body)
    body += bytes(4 * ncol * len(rows))
    struct.pack_into("<II", body, 0, col_list, row_list)
    for r, row in enumerate(rows):
        for c, value in enumerate(row):
            at = row_at + 4 * (ncol * r + c)
            if columns[c][1] == "i":
                struct.pack_into("<i", body, at, value)
            else:
                text = (value + "\0").encode("utf-16-le")
                struct.pack_into("<I", body, at, len(body))
                body += struct.pack("<I", len(text) // 2) + text
    head = b"GFF V4.0PC  G2DAV0.2" + struct.pack("<II", len(templates), data_offset)
    return head + tmpl + decls + bytes(body)


class Gda(unittest.TestCase):
    def test_a_column_is_known_by_its_hash(self):
        self.assertEqual(gda.column_hash("TargetType"), 0x0F642429)

    def test_a_table_reads_back(self):
        data = build_gda(
            [("ID", "i"), ("Label", "s"), ("TargetType", "i")],
            [[1, "Attack", 2], [5, "Shield", 0]],
        )
        columns, rows = gda.read_gda(data)
        self.assertEqual(
            [h for h, _ in columns],
            [gda.column_hash(n) for n in ("ID", "Label", "TargetType")],
        )
        self.assertEqual([t for _, t in columns], ["s32", "string", "s32"])
        self.assertEqual(rows, [[1, "Attack", 2], [5, "Shield", 0]])

    def test_not_a_gff4_file_is_refused(self):
        with self.assertRaises(ValueError):
            gda.read_gda(b"ERF V2.0" + bytes(40))


if __name__ == "__main__":
    unittest.main()
