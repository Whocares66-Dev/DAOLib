"""tools/tlk.py on a version 0.2 talk table built here."""

import struct
import unittest

import tlk


def build_tlk(strings):
    """GFF V4.0 PC TLK V0.2: the top struct's list of (id, text) structs."""
    templates = [
        (b"TLK ", [(tlk.STRING_LIST, 1, 0x8000 | 0x4000, 0)], 4),
        (b"STRN", [(tlk.STRING_ID, 4, 0, 0), (tlk.STRING_TEXT, 14, 0, 4)], 8),
    ]
    fields_at = 28 + 16 * len(templates)
    decls = tmpl = b""
    for label, fields, size in templates:
        tmpl += struct.pack(
            "<4sIII", label[::-1], len(fields), fields_at + len(decls), size
        )
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


class TalkTableTest(unittest.TestCase):
    def test_strings_by_their_reference(self):
        table = tlk.TalkTable(
            build_tlk([(379897, "Shield Pummel"), (379898, "Shield Mastery")])
        )
        self.assertEqual(len(table), 2)
        self.assertEqual(table.get(379898), "Shield Mastery")
        self.assertIsNone(table.get(1))

    def test_a_table_of_another_type_is_refused(self):
        data = bytearray(build_tlk([]))
        data[12:16] = b"G2DA"
        with self.assertRaises(ValueError):
            tlk.TalkTable(bytes(data))


if __name__ == "__main__":
    unittest.main()
