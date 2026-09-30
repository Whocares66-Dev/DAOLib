"""Harvest engine objects' fields from the script commands that read and write them.

Most script commands call no engine function at all: they look the object up
and read or write its fields inline. GetCombatState is

    call  CClientExoApp::GetCreatureById       ; eax = the creature
    mov   edx, dword ptr [eax + 0xc60]          ; creature->+0xC60 (a pointer)
    mov   esi, dword ptr [edx]                  ; ->+0x0
    push  esi
    call  CVirtualMachine::StackPushInteger     ; returned to the script as an int

so a CCCreature holds, at +0xC60, a pointer to something whose first int is
the creature's combat state. This follows each command's looked-up object
through registers (and x87 loads, for floats) and records every field read
or written through it, and every field whose value the command pushes back
to the script: that last gives the field the command's meaning and the
script's type for it.

A path is the chain of offsets from the object: "+0xc60 +0x0" is the int at
offset 0 of what the pointer at +0xC60 points to. A field a command only
compares or passes on is recorded as read; only a pushed one is `returned`.

Writes dev/script_fields.csv, one row per (class, path):

    class,path,width,returned_as,returned_by,read_by,written_by

Needs what tools/script_backends.py needs.

    python tools/script_fields.py            rewrite the CSV, report counts
    python tools/script_fields.py --print    print it instead
"""

import collections
import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import script_backends  # noqa: E402

OUT = os.path.join(HERE, "..", "dev", "script_fields.csv")

# The VM's pushes (dao/Addresses.h), by the script type each hands back.
PUSH_TYPES = {
    0x00A2D650: "int",
    0x00A2D690: "float",
    0x00A2D6D0: "vector",
    0x00A2D730: "string",
    0x00A2D920: "engine structure",
    0x00A2D980: "object",
}

WIDTHS = {"byte": 1, "word": 2, "dword": 4, "qword": 8}
MEMORY = re.compile(
    r"(byte|word|dword|qword) ptr \[(e\w\w)(?: \+ (0x[0-9a-f]+|\d+))?\]"
)
SUBREGISTERS = {
    "al": "eax", "ah": "eax", "ax": "eax",
    "bl": "ebx", "bh": "ebx", "bx": "ebx",
    "cl": "ecx", "ch": "ecx", "cx": "ecx",
    "dl": "edx", "dh": "edx", "dx": "edx",
    "si": "esi", "di": "edi", "bp": "ebp",
}  # fmt: skip
REGISTERS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")


def register(name):
    return SUBREGISTERS.get(name, name)


# -- pure logic, over (mnemonic, operands) pairs, tested in tools/test_script_fields.py --


def field_accesses(instructions, lookups, push_types=None):
    """[(lookup, path, width, access, returned_as)] for the fields of every
    object a lookup returned. access is "read", "write" or "returned";
    returned_as is the script type for "returned", else ""."""
    push_types = PUSH_TYPES if push_types is None else push_types
    values = {}  # register -> (lookup, path, width): the object, or a field of it
    st0 = None  # the x87 top, when it was loaded from a field
    pushes = []  # values pushed since the last call, None for anything else
    found = []

    def operand(text):
        """(lookup, path, width) for a memory operand through a known register."""
        m = MEMORY.fullmatch(text)
        if not m or m.group(2) not in values:
            return None
        lookup, path, width = values[m.group(2)]
        if path and width != 4:
            return None  # only a pointer-sized field can be followed
        offset = int(m.group(3), 0) if m.group(3) else 0
        return lookup, path + (offset,), WIDTHS[m.group(1)]

    def note(field, access, typed=""):
        found.append((field[0], field[1], field[2], access, typed))

    for mnemonic, operands in instructions:
        ops = [o.strip() for o in operands.split(",")] if operands else []
        if mnemonic == "call":
            target = int(ops[0], 16) if ops and ops[0].startswith("0x") else None
            if target in push_types and pushes and pushes[-1] is not None:
                note(pushes[-1], "returned", push_types[target])
            for r in ("eax", "ecx", "edx"):
                values.pop(r, None)
            if target in lookups:
                values["eax"] = (target, (), 4)
            pushes = []
            st0 = None
            continue
        if mnemonic == "push":
            source = ops[0]
            field = operand(source)
            if field:
                note(field, "read")
            pushes.append(field or values.get(register(source)))
            continue
        if mnemonic in ("fld", "fild"):
            field = operand(ops[0])
            if field:
                note(field, "read")
            st0 = field
            continue
        if mnemonic == "fstp" and ops and ops[0] == "dword ptr [esp]":
            if pushes:
                pushes[-1] = st0
            st0 = None
            continue
        if mnemonic in ("cmp", "test"):
            for text in ops:
                field = operand(text)
                if field:
                    note(field, "read")
            continue
        if mnemonic in ("mov", "movzx", "movsx") and len(ops) == 2:
            destination, source = ops
            written = operand(destination)
            if written:
                note(written, "write")
                continue
            field = operand(source)
            if field:
                note(field, "read")
            d = register(destination)
            if d in REGISTERS:
                if field:
                    values[d] = field
                elif source in values:
                    values[d] = values[source]
                else:
                    values.pop(d, None)
            continue
        if ops:
            d = register(ops[0])
            if d in REGISTERS:
                values.pop(d, None)
    return found


def path_text(path):
    return " ".join(f"+{o:#x}" for o in path)


def harvest():
    c = script_backends.Commands()
    fields = {}  # (class, path) -> {"widths", "returned_as", "returned_by", "read_by", "written_by"}
    for fn, body in sorted(c.bodies.items()):
        for lookup, path, width, access, typed in field_accesses(body, c.lookups):
            if path[0] == 0:
                continue  # the object's first word is its vtable: a virtual call, not a field
            cls = c.lookups[lookup][0]
            entry = fields.setdefault(
                (cls, path),
                {
                    "widths": set(),
                    "returned_as": set(),
                    "returned_by": [],
                    "read_by": [],
                    "written_by": [],
                },
            )
            entry["widths"].add(width)
            if typed:
                entry["returned_as"].add(typed)
            column = {
                "returned": "returned_by",
                "read": "read_by",
                "write": "written_by",
            }[access]
            entry[column].extend(n for n in c.names[fn] if n not in entry[column])
    return fields


def main(argv):
    fields = harvest()
    rows = []
    for (cls, path), e in sorted(fields.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        rows.append(
            (
                cls,
                path_text(path),
                "/".join(str(w) for w in sorted(e["widths"])),
                " | ".join(sorted(e["returned_as"])),
                " | ".join(e["returned_by"]),
                " | ".join(e["read_by"]),
                " | ".join(e["written_by"]),
            )
        )
    if "--print" in argv:
        for row in rows:
            print(*row, sep="\t")
    else:
        with open(OUT, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(
                [
                    "class",
                    "path",
                    "width",
                    "returned_as",
                    "returned_by",
                    "read_by",
                    "written_by",
                ]
            )
            w.writerows(rows)
        print(f"wrote {os.path.normpath(OUT)}")
    per_class = collections.Counter(r[0] for r in rows)
    returned = sum(1 for r in rows if r[4])
    print(
        f"{len(rows)} fields over {len(per_class)} classes, {returned} of them returned to a script:"
        f" {', '.join(f'{k} {v}' for k, v in per_class.most_common())}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
