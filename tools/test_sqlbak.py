"""tools/sqlbak.py on pages built here, in the layout it reads."""

import struct
import unittest

import sqlbak
from sqlbak import PAGE


def page(number, kind, object_id, records):
    """A page: the 96-byte header, the records one after another, the slot
    array at the end."""
    p = bytearray(PAGE)
    p[0], p[1] = 1, kind
    struct.pack_into("<H", p, 22, len(records))
    struct.pack_into("<I", p, 24, object_id)
    struct.pack_into("<I", p, 32, number)
    at = 96
    for i, r in enumerate(records):
        p[at : at + len(r)] = r
        struct.pack_into("<H", p, PAGE - 2 * (i + 1), at)
        at += len(r)
    return bytes(p)


def row(fixed, variable):
    """A data record: status (variable columns present), the fixed part,
    the column count, a null bitmap, then the variable columns. A
    ('pointer', bytes) column gets the high bit on its end offset."""
    columns = 1 + len(variable)
    head = bytes([0x30, 0]) + struct.pack("<H", 4 + len(fixed)) + fixed
    head += struct.pack("<H", columns) + bytes((columns + 7) // 8)
    head += struct.pack("<H", len(variable))
    start = len(head) + 2 * len(variable)
    ends, body = [], b""
    for v in variable:
        raw = v[1] if isinstance(v, tuple) else v
        body += raw
        end = start + len(body)
        ends.append(end | 0x8000 if isinstance(v, tuple) else end)
    return head + struct.pack(f"<{len(ends)}H", *ends) + body


def lob(kind, payload, length=None):
    """A large value's record: status, length, blob id, type, payload."""
    body = struct.pack("<Q", 0x1234) + struct.pack("<H", kind) + payload
    return bytes([0x08, 0]) + struct.pack("<H", length or 4 + len(body)) + body


class Records(unittest.TestCase):
    def test_a_row_gives_its_fixed_part_and_variable_columns(self):
        p = page(
            3,
            1,
            238,
            [
                row(
                    struct.pack("<I", 75),
                    [b"rules_core", b"\\_Core Scripts\\", ("pointer", b"P" * 16)],
                )
            ],
        )
        fixed, columns, variable = sqlbak.record(p, sqlbak.slots(p)[0])
        self.assertEqual(struct.unpack("<I", fixed)[0], 75)
        self.assertEqual(columns, 4)
        self.assertEqual(variable[0], b"rules_core")
        self.assertEqual(variable[1], b"\\_Core Scripts\\")
        self.assertEqual(variable[2], ("pointer", b"P" * 16))

    def test_the_header_names_the_page_and_its_owner(self):
        h = sqlbak.header(page(7, 3, 127, []))
        self.assertEqual((h["page"], h["type"], h["object"]), (7, 3, 127))


class LargeValues(unittest.TestCase):
    def backup(self, pages):
        data = b"".join(pages)
        return sqlbak.Backup(data, 0)

    def test_a_small_root_holds_the_bytes(self):
        text = b"void main () { }"
        small = lob(sqlbak.SMALL_ROOT, struct.pack("<H", len(text)) + bytes(4) + text)
        b = self.backup([page(0, 3, 127, [small])])
        self.assertEqual(sqlbak.large_value(b, 0, 0), text)

    def test_a_large_root_joins_its_fragments_through_an_internal_node(self):
        first, second = b'#include "core_h"\n', b"void main () { }"
        data1 = lob(sqlbak.DATA, first)
        data2 = lob(sqlbak.DATA, second)
        # internal node: max links, links, level 0, then (offset, page, file, slot) of 16 bytes each
        internal = lob(
            sqlbak.INTERNAL,
            struct.pack("<HHH", 501, 2, 0)
            + struct.pack("<QIHH", len(first), 1, 1, 0)
            + struct.pack("<QIHH", len(first) + len(second), 1, 1, 1),
        )
        # large root: max links, links, level 1, 4 unused bytes, then (size, page, file, slot) of 12 bytes
        root = lob(
            sqlbak.LARGE_ROOT,
            struct.pack("<HHHI", 5, 1, 1, 1)
            + struct.pack("<IIHH", len(first) + len(second), 2, 1, 0),
        )
        b = self.backup(
            [
                page(0, 3, 127, [root]),
                page(1, 3, 127, [data1, data2]),
                page(2, 3, 127, [internal]),
            ]
        )
        self.assertEqual(sqlbak.large_value(b, 0, 0), first + second)

    def test_a_row_pointer_reaches_the_value(self):
        text = b"void main () { }"
        small = lob(sqlbak.SMALL_ROOT, struct.pack("<H", len(text)) + bytes(4) + text)
        b = self.backup([page(0, 3, 127, [small])])
        pointer = bytes(8) + struct.pack("<IHH", 0, 1, 0)
        self.assertEqual(b.value(pointer), text)


if __name__ == "__main__":
    unittest.main()
