"""Every engine message class's size and fields, from its clone.

A message class (dao/Messages.h) derives from Messages::Message, and slot 1
of its vtable clones it: operator new with the class's size, the base
constructor, the vtable, then the fields copied from the original one by
one, `mov reg, [src + off]; mov [copy + off], reg` (PerformAbilityMessage,
0x0077B210, copies +4, +8 and +0xC of a 0x10-byte message). A field that
is itself an object, a string say, is copied by a call instead; the
table marks a clone with calls after the base constructor so its fields
are not taken as the whole story.

This reads every such clone and writes dev/messages.csv:

    class, vftable, size, fields (offset:width, space-separated),
    calls (the clone calls more than new and the base constructor),
    handled_by (the classes with a handler for it, dev/message_handlers.csv)

    python tools/messages.py            rewrite the table, report counts
    python tools/messages.py --print    print it instead
"""

import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import daoexe  # noqa: E402
import msvc_names  # noqa: E402

ROOT = os.path.join(HERE, "..")
OUT = os.path.join(ROOT, "dev", "messages.csv")
HANDLERS = os.path.join(ROOT, "dev", "message_handlers.csv")

MESSAGE = ".?AVMessage@Messages@@"
OPERATOR_NEW = 0x00413660
# Messages::Message::Message, which every clone calls on its copy first.
MESSAGE_CONSTRUCTOR = 0x004D2080
WIDTH = {"byte": 1, "word": 2, "dword": 4, "qword": 8}


def message_classes(img):
    """[(decorated, vftable)] of the primary vftables of classes with
    Messages::Message at +0."""
    out = []
    for va, decorated, offset, slots in img.vtables:
        if offset != 0 or len(slots) < 2 or decorated == MESSAGE:
            continue
        bases = img.bases(decorated) or []
        if any(b == MESSAGE and at == 0 for b, at, _ in bases):
            out.append((decorated, va, slots))
    return out


def read_clone(img, clone):
    """(size, [(offset, width)], other calls) of a clone function."""
    ins = img.disasm(clone, None, limit=0x400)
    size = None
    pushed = None
    fields = {}
    calls = 0
    seen_new = False
    loads = {}  # register -> (offset, width) loaded from the source
    for i in ins:
        m, op = i.mnemonic, i.op_str
        if m == "push" and re.fullmatch(r"0x[0-9a-f]+|\d+", op):
            pushed = int(op, 0)
        elif m == "call":
            target = int(op, 16) if re.fullmatch(r"0x[0-9a-f]+", op) else None
            if target == OPERATOR_NEW and not seen_new:
                size, seen_new = pushed, True
            elif seen_new and target != MESSAGE_CONSTRUCTOR:
                calls += 1
        # a load from the source: mov reg, [r + off]
        lm = re.fullmatch(r"(\w+), (\w+) ptr \[(\w+) \+ (0x[0-9a-f]+|\d+)\]", op)
        if m in ("mov", "movzx") and lm:
            loads[lm.group(1)] = (int(lm.group(4), 0), WIDTH.get(lm.group(2), 4))
        # a store into the copy: mov [r + off], reg
        sm = re.fullmatch(r"(\w+) ptr \[(\w+) \+ (0x[0-9a-f]+|\d+)\], (\w+)", op)
        if m == "mov" and sm and seen_new:
            off = int(sm.group(3), 0)
            src = loads.get(sm.group(4))
            if src and src[0] == off:
                fields[off] = WIDTH.get(sm.group(1), src[1])
    return size, sorted(fields.items()), calls


def main(argv):
    img = daoexe.Image()
    handled = {}
    with open(HANDLERS, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            handled.setdefault(r["message"], set()).add(r["owner"])
    rows = []
    for decorated, va, slots in message_classes(img):
        name = msvc_names.undecorate(decorated)
        size, fields, calls = read_clone(img, slots[1])
        rows.append(
            [
                name,
                f"{va:#010x}",
                f"{size:#x}" if size else "",
                " ".join(f"{o:#x}:{w}" for o, w in fields),
                calls,
                " | ".join(sorted(handled.get(name, ()))),
            ]
        )
    rows.sort(key=lambda r: r[0].lower())
    if "--print" in argv:
        for r in rows:
            print("  ".join(str(c) for c in r))
    else:
        with open(OUT, "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["class", "vftable", "size", "fields", "calls", "handled_by"])
            w.writerows(rows)
    sized = sum(1 for r in rows if r[2])
    plain = sum(1 for r in rows if r[2] and r[4] == 0)
    print(
        f"{len(rows)} message classes; {sized} sized from their clone, {plain} of those copied field by field"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
