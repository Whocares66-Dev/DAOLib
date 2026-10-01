"""Name engine functions after the script commands that call them.

A script command's body pops its arguments off the VM's stack, does its work
by calling into the engine, and pushes its answer. The script command's name
and signature are known (dev/script_commands.csv, from script.ldf), so the
engine functions it calls to do the work can be named after it. Three ways
to find them, each read from the exe:

1. Typed object lookups. `GetCombatState(object)` finds its creature with
   0x005D58C0, NWN's CServerExoApp::GetCreatureByGameObjectID in shape: the
   application's object array (+0xCC), CGameObjectArray::GetGameObject, a
   type byte at +0x14 checked against a constant, and a cast through a
   virtual slot. Which class a cast slot is for is read from the vtables:
   the class whose implementation of that slot is `mov eax, ecx; ret`
   (return this) -- +0x10 only in CCCreature, +0x24 only in CCPlaceable,
   and so on, NWN's CGameObject::As* pattern. The lookup is named
   CClientExoApp::Get<Class>ById.
2. Virtual calls on a looked-up object. After a typed lookup, a call
   through the object's vtable lands in a known slot of a known class; that
   slot's function is named <Class>::ForScript_<Command>, in the namespace
   Ghidra's class recovery gave it if it gave one.
3. A sole direct callee. Where a command's body makes exactly one direct
   call that is not the VM's, not a lookup, not shared by five or more
   commands (helpers: strings, allocation), and not a stub of eight bytes
   or fewer (folded by the compiler with every other stub of the same body),
   that function is named ScriptBackend::ForScript_<Command>.

Several commands can lead to one function (IntToString and IntToChar share
one); it is named after the first and the CSV lists all. A function reached
two ways keeps the first way's name, in the order above.

Writes dev/object_lookups.csv and dev/script_backends.csv; tools/names.py
names them in Ghidra. Needs game/DAOrigins.exe and Ghidra's function list,
game/ghidra/functions.tsv (dev/PROBE.md says how to export it).

    python tools/script_backends.py            rewrite the CSVs, report counts
    python tools/script_backends.py --print    print them instead
"""

import collections
import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import daoexe  # noqa: E402
import msvc_names  # noqa: E402
import address_ids  # noqa: E402
import probe_report  # noqa: E402

COMMANDS = os.path.join(HERE, "..", "dev", "script_commands.csv")
OUT_LOOKUPS = os.path.join(HERE, "..", "dev", "object_lookups.csv")
OUT_BACKENDS = os.path.join(HERE, "..", "dev", "script_backends.csv")

GET_GAME_OBJECT = 0x00A58190  # CGameObjectArray::GetGameObject (dao/Addresses.h)
HELPER_CALLERS = 5  # a callee of this many commands or more is a helper
REGISTERS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")

# -- pure logic, over (mnemonic, operands) pairs, tested in tools/test_script_backends.py --


def as_slot(instructions):
    """The cast slot's offset in a typed lookup's body: the object's vtable
    loaded (`mov edx, [ecx]`), then a slot of it read or called (`mov eax,
    [edx + 0x10]` / `call [edx + 0x10]`). None if the body has no such pair."""
    vtables = set()
    for mnemonic, operands in instructions:
        ops = [o.strip() for o in operands.split(",")]
        if (
            mnemonic == "mov"
            and len(ops) == 2
            and re.fullmatch(r"dword ptr \[e\w\w\]", ops[1])
        ):
            vtables.add(ops[0])
            continue
        m = re.fullmatch(r"dword ptr \[(e\w\w) \+ (0x[0-9a-f]+)\]", ops[-1])
        if m and m.group(1) in vtables and mnemonic in ("mov", "call"):
            return int(m.group(2), 16)
    return None


def method_calls(instructions, lookups):
    """[(kind, lookup address, slot offset or callee)] for each method called
    on an object a lookup returned, tracking it through register moves:
    `call lookup` (eax is the object), `mov esi, eax`, then either
    ("virtual", lookup, 0x58) for `mov edx, [esi]; call [edx + 0x58]` (or
    `mov eax, [edx + 0x58]; call eax`), or ("direct", lookup, 0x006AE0A0)
    for `mov ecx, esi; call 0x006AE0A0`: __thiscall, the object as `this`."""
    objects, vtables, slots = {}, {}, {}
    found = []

    def clobber(register):
        for table in (objects, vtables, slots):
            table.pop(register, None)

    for mnemonic, operands in instructions:
        ops = [o.strip() for o in operands.split(",")]
        if mnemonic == "call":
            target = ops[0]
            m = re.fullmatch(r"dword ptr \[(e\w\w) \+ (0x[0-9a-f]+)\]", target)
            if m and m.group(1) in vtables:
                found.append(("virtual", vtables[m.group(1)], int(m.group(2), 16)))
            elif target in slots:
                found.append(("virtual", *slots[target]))
            elif (
                target.startswith("0x")
                and "ecx" in objects
                and int(target, 16) not in lookups
            ):
                found.append(("direct", objects["ecx"], int(target, 16)))
            for register in ("eax", "ecx", "edx"):
                clobber(register)
            if target.startswith("0x") and int(target, 16) in lookups:
                objects["eax"] = int(target, 16)
            continue
        if mnemonic == "mov" and len(ops) == 2 and ops[0] in REGISTERS:
            destination, source = ops
            m = re.fullmatch(r"dword ptr \[(e\w\w)\]", source)
            m2 = re.fullmatch(r"dword ptr \[(e\w\w) \+ (0x[0-9a-f]+)\]", source)
            if source in objects:
                value = ("object", objects[source])
            elif m and m.group(1) in objects:
                value = ("vtable", objects[m.group(1)])
            elif m2 and m2.group(1) in vtables:
                value = ("slot", (vtables[m2.group(1)], int(m2.group(2), 16)))
            elif source in vtables:
                value = ("vtable", vtables[source])
            else:
                value = None
            clobber(destination)
            if value:
                {"object": objects, "vtable": vtables, "slot": slots}[value[0]][
                    destination
                ] = value[1]
            continue
        if ops and ops[0] in REGISTERS and mnemonic not in ("cmp", "test", "push"):
            clobber(ops[0])
    return found


def sole_callee(calls, excluded, callers_of, helper_callers=HELPER_CALLERS):
    """The one direct callee left once the excluded and the helpers are
    dropped, or None if none or several are."""
    left = [
        c
        for c in dict.fromkeys(calls)
        if c not in excluded and callers_of[c] < helper_callers
    ]
    return left[0] if len(left) == 1 else None


# -- the exe --


def instructions_of(img, entry, end):
    """Every instruction from entry to end, the function's whole body. A
    count is given because without one the disassembler stops at the first
    `ret`, which in a script command is usually the early return for a bad
    argument, before any of the work: until 2026-09-30 that cut most bodies
    short, and this tool and script_fields.py saw a fraction of them."""
    return [
        (i.mnemonic, i.op_str)
        for i in img.disasm(entry, count=1 << 20, limit=end - entry + 1)
        if i.address <= end
    ]


def cast_classes(img):
    """{slot offset: class} for the cast slots only one class answers with
    `this` (among the client's game-object classes)."""
    classes = {}
    for va, decorated, offset, slots in img.vtables:
        name = msvc_names.undecorate(decorated)
        if offset == 0 and (
            name.startswith("CC") or name in ("CGameObject", "CItem", "CProjectile")
        ):
            classes[name] = (va, slots)
    out = {}
    for offset in range(0x10, 0x60, 4):
        k = offset // 4
        hits = []
        for name, (_, slots) in classes.items():
            if k < len(slots):
                ops = [
                    f"{i.mnemonic} {i.op_str}".strip()
                    for i in img.disasm(slots[k], count=2)
                ]
                if ops == ["mov eax, ecx", "ret"]:
                    hits.append(name)
        if len(hits) == 1:
            out[offset] = hits[0]
        elif "CCObject" in hits:
            out[offset] = (
                "CCObject"  # every game object answers: the cast to their base
            )
        elif hits and all(h.startswith(hits[0]) or hits[0].startswith(h) for h in hits):
            out[offset] = min(hits, key=len)  # a class and its subclasses: the base
    return out


def find_lookups(img, functions, candidates):
    """{address: (class, slot)} for the typed lookups among the candidates."""
    casts = cast_classes(img)
    out = {}
    for fn in candidates:
        hit = functions.containing(fn)
        if hit[0] != fn:
            continue
        end = functions.ends[functions.entries.index(fn)]
        body = instructions_of(img, fn, min(end, fn + 0x80))
        texts = [f"{m} {o}" for m, o in body]
        if not any("+ 0xcc]" in t for t in texts) or not any(
            f"{GET_GAME_OBJECT:#x}" in t for t in texts
        ):
            continue
        slot = as_slot(body)
        if slot in casts:
            out[fn] = (casts[slot], slot)
        elif slot is None and any(m == "ret" for m, _ in body):
            out[fn] = ("CGameObject", None)  # no cast: the object as the array holds it
    return out


class Commands:
    """The script commands' bodies and what they call, and the object
    lookups among their callees: what this tool and tools/script_fields.py
    both read."""

    def __init__(self):
        self.img = daoexe.Image()
        self.functions = probe_report.Functions()
        if not self.functions.entries:
            sys.exit(
                f"no {os.path.normpath(probe_report.FUNCTIONS)}: export it from Ghidra first (dev/PROBE.md)"
            )
        ends = dict(zip(self.functions.entries, self.functions.ends, strict=True))
        # implementation address -> the commands implemented there, by script name
        self.names = collections.defaultdict(list)
        self.signatures = {}
        with open(COMMANDS, encoding="utf-8", newline="") as f:
            for row in csv.DictReader(f):
                va = int(row["function_va"], 16)
                if va:
                    self.names[va].append(
                        row["script_name"] or row["name"].removeprefix("COMMAND_")
                    )
                    self.signatures.setdefault(va, row["signature"])
        self.bodies = {
            fn: instructions_of(self.img, fn, ends.get(fn, fn + 0x400))
            for fn in self.names
        }
        self.calls = {
            fn: [int(o, 16) for m, o in body if m == "call" and o.startswith("0x")]
            for fn, body in self.bodies.items()
        }
        self.callers_of = collections.Counter(
            c for cs in self.calls.values() for c in set(cs)
        )
        self.known = address_ids.addresses_in_source()
        self.vm = {va for va in self.known if 0x00A2D000 <= va < 0x00A30000}
        self.lookups = find_lookups(self.img, self.functions, self.callers_of)


def analyse():
    c = Commands()
    img, functions, commands, bodies = c.img, c.functions, c.names, c.bodies
    calls, callers_of, known, vm, lookups = (
        c.calls,
        c.callers_of,
        c.known,
        c.vm,
        c.lookups,
    )

    def trivial(fn):
        size = 0
        for i in img.disasm(fn, count=4):
            size += i.size
            if i.mnemonic == "ret":
                return size <= 8
        return False

    # address -> [how, class, [commands], named]: named once some command
    # calls it as the only method on its object, or as its sole callee. A
    # command calling several methods leaves each a candidate, unnamed: the
    # command's purpose is shared out among them, not each one's.
    backends = {}
    vtable_of = {
        msvc_names.undecorate(d): slots for va, d, off, slots in img.vtables if off == 0
    }

    def note(target, kind, cls, fn, named):
        entry = backends.setdefault(target, [kind, cls, [], False])
        entry[2].extend(c for c in commands[fn] if c not in entry[2])
        entry[3] = entry[3] or named

    for fn, body in sorted(bodies.items()):
        methods = []
        for kind, lookup, what in dict.fromkeys(method_calls(body, lookups)):
            cls = lookups[lookup][0]
            if kind == "virtual":
                slots = vtable_of.get(cls, [])
                if what // 4 >= len(slots):
                    continue
                target = slots[what // 4]
            else:
                target = what
                if (
                    target in vm
                    or target in known
                    or callers_of[target] >= HELPER_CALLERS
                ):
                    continue
            if not trivial(target):
                methods.append((target, kind, cls))
        for target, kind, cls in methods:
            note(target, kind, cls, fn, named=len(methods) == 1)
        if methods:
            continue
        excluded = vm | set(lookups) | {GET_GAME_OBJECT}
        sole = sole_callee(calls[fn], excluded, callers_of)
        if sole and not trivial(sole) and sole not in known:
            _, ghidra = functions.containing(sole)
            if not ghidra.startswith(("_", "thunk", "KERNEL32", "USER32")):
                note(sole, "sole", "", fn, named=True)
    return functions, lookups, backends


def backend_name(how, cls, ghidra, first):
    """Ghidra's class namespace when class recovery gave one, else the
    lookup's class for a virtual call, else ScriptBackend."""
    namespace = ""
    parts = msvc_names.split_top(ghidra)
    if len(parts) > 1 and not parts[0].startswith(("FUN_", "switch")):
        namespace = "::".join(parts[:-1])
    namespace = namespace or cls or "ScriptBackend"
    return f"{namespace}::ForScript_{first}"


def main(argv):
    functions, lookups, backends = analyse()
    lookup_rows = []
    for fn, (cls, slot) in sorted(lookups.items()):
        # CCCreature -> Creature, CProjectile -> Projectile, CGameObject -> GameObject.
        noun = cls[2:] if cls.startswith("CC") else cls[1:]
        lookup_rows.append(
            (
                f"{fn:#010x}",
                f"CClientExoApp::Get{noun}ById",
                cls,
                f"{slot:#x}" if slot is not None else "",
            )
        )
    backend_rows = []
    for fn, (how, cls, cmds, named) in sorted(backends.items()):
        _, ghidra = functions.containing(fn)
        name = backend_name(how, cls, ghidra, cmds[0]) if named else ""
        backend_rows.append((f"{fn:#010x}", name, how, cls, ghidra, " | ".join(cmds)))
    if "--print" in argv:
        for row in lookup_rows:
            print("lookup  ", *row)
        for row in backend_rows:
            print("backend ", *row)
    else:
        with open(OUT_LOOKUPS, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(["function_va", "name", "class", "cast_slot"])
            w.writerows(lookup_rows)
        with open(OUT_BACKENDS, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(
                ["function_va", "name", "how", "class", "ghidra_name", "commands"]
            )
            w.writerows(backend_rows)
        print(
            f"wrote {os.path.normpath(OUT_LOOKUPS)} and {os.path.normpath(OUT_BACKENDS)}"
        )
    how = collections.Counter(b[0] for b in backends.values() if b[3])
    candidates = sum(1 for b in backends.values() if not b[3])
    print(
        f"{len(lookup_rows)} object lookups; {sum(how.values())} engine functions named after the commands"
        f" that call them ({how['virtual']} through a vtable, {how['direct']} as methods of a looked-up"
        f" object, {how['sole']} as a command's sole callee); {candidates} more listed unnamed, one of"
        f" several methods a command calls"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
