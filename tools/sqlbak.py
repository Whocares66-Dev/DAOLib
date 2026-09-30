"""Read rows out of a SQL Server 2005 backup file without SQL Server.

The toolset's content database (dev/TOOLSET.md) is a SQL Server 2005
backup (database format 611), which current SQL Server versions will not
restore. It does not need restoring to be read: a full backup is a Microsoft
Tape Format wrapper around the database's own 8 KB pages, stored in order,
so page n is at a fixed offset from the first, and each page is the
on-disk format SQL Server documents and tools like OrcaMDF read.

This reads just what the toolset's scripts need:

- a page's header: its type (1 data, 3 text mix, ...), the allocation unit
  it belongs to (object id and index id in 2005's header), its own number;
- a record's columns: the fixed part, then the variable-length columns,
  a set high bit on a column's end offset marking a pointer to a large
  value stored elsewhere;
- a large value (text, varchar(max)) from its pointer: a small root holds
  the bytes itself; a large root and its internal nodes hold links to data
  fragments, 12 bytes each in the root and 16 in an internal node.

Worked out on 2026-09-30 against the toolset's backup, where it reads all
4,209 script texts whole (rules_core.nss is 62,713 bytes over eight
fragments).
"""

import struct

PAGE = 8192

# Record types of a large value's records (the 2 bytes after the blob id).
SMALL_ROOT = 0
INTERNAL = 2
DATA = 3
LARGE_ROOT = 5


class Backup:
    def __init__(self, data, base):
        self.data = data
        self.base = base
        self.count = (len(data) - base) // PAGE

    @classmethod
    def open(cls, path):
        with open(path, "rb") as f:
            data = f.read()
        return cls(data, find_base(data))

    def page(self, n):
        off = self.base + n * PAGE
        return self.data[off : off + PAGE]

    def pages(self, object_id, page_type=1):
        """The pages of an allocation unit, by the 2005 header's object id,
        that are where their own number says."""
        for n in range(self.count):
            p = self.page(n)
            h = header(p)
            if h["type"] == page_type and h["object"] == object_id and h["page"] == n:
                yield p

    def rows(self, object_id):
        for p in self.pages(object_id):
            for off in slots(p):
                yield record(p, off)

    def value(self, pointer):
        """A large value from the 16-byte pointer a row holds for it:
        8 bytes of timestamp, then page, file and slot."""
        page, _, slot = struct.unpack_from("<IHH", pointer, 8)
        return large_value(self, page, slot)


def find_base(data):
    """Where page 0 is: the boot page (type 13) is always page 9."""
    for off in range(0, min(len(data), 1 << 24), 512):
        if (
            data[off] == 1
            and data[off + 1] == 13
            and struct.unpack_from("<I", data, off + 32)[0] == 9
        ):
            return off - 9 * PAGE
    raise ValueError("no boot page in the first 16 MB: not a SQL Server backup?")


def header(p):
    return {
        "type": p[1],
        "index": struct.unpack_from("<H", p, 6)[0],
        "slots": struct.unpack_from("<H", p, 22)[0],
        "object": struct.unpack_from("<I", p, 24)[0],
        "page": struct.unpack_from("<I", p, 32)[0],
    }


def slots(p):
    """The records' offsets, from the slot array at the page's end."""
    count = struct.unpack_from("<H", p, 22)[0]
    return [struct.unpack_from("<H", p, PAGE - 2 * (i + 1))[0] for i in range(count)]


def record(p, off):
    """(fixed bytes, column count, variable columns). A variable column is
    bytes, or ('pointer', bytes) for a large value stored elsewhere."""
    status = p[off]
    fixed_end = struct.unpack_from("<H", p, off + 2)[0]
    columns = struct.unpack_from("<H", p, off + fixed_end)[0]
    at = off + fixed_end + 2 + (columns + 7) // 8
    variable = []
    if status & 0x20:
        count = struct.unpack_from("<H", p, at)[0]
        ends = struct.unpack_from(f"<{count}H", p, at + 2)
        start = at + 2 + 2 * count - off
        for end in ends:
            raw = p[off + start : off + (end & 0x7FFF)]
            variable.append(("pointer", raw) if end & 0x8000 else raw)
            start = end & 0x7FFF
    return p[off + 4 : off + fixed_end], columns, variable


def _record_at(backup, page, slot):
    p = backup.page(page)
    off = struct.unpack_from("<H", p, PAGE - 2 * (slot + 1))[0]
    return p, off, struct.unpack_from("<H", p, off + 12)[0]


def large_value(backup, page, slot):
    p, off, kind = _record_at(backup, page, slot)
    if kind == SMALL_ROOT:
        length = struct.unpack_from("<H", p, off + 14)[0]
        return p[off + 20 : off + 20 + length]
    if kind == DATA:
        length = struct.unpack_from("<H", p, off + 2)[0]
        return p[off + 14 : off + length]
    if kind in (LARGE_ROOT, INTERNAL):
        _, links, level = struct.unpack_from("<HHH", p, off + 14)
        out = bytearray()
        for i in range(links):
            if kind == LARGE_ROOT:
                _, lpage, _, lslot = struct.unpack_from("<IIHH", p, off + 24 + 12 * i)
            else:
                _, lpage, _, lslot = struct.unpack_from("<QIHH", p, off + 20 + 16 * i)
            out += large_value(backup, lpage, lslot)
        return bytes(out)
    raise ValueError(
        f"record type {kind} at page {page} slot {slot} is not part of a large value"
    )
