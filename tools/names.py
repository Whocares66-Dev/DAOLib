"""Names for what this project knows about the exe, for Ghidra.

Six sources, in the order a name wins when two name one address:

1. src/dao/Addresses.h: each `DAO_ADDRESS(kName, "Engine::Name", 0xVA)` row
   names a function and each `DAO_DATA(...)` row a global, by what the engine
   calls it. Curated by hand, so it outranks the tables.
2. dev/message_handlers.csv (tools/message_handlers.py): each message
   handler's method, as Owner::Method_Message, since the methods overload on
   the message type.
3. dev/object_lookups.csv (tools/script_backends.py): the application's
   typed lookups by object id, as CClientExoApp::Get<Class>ById.
4. dev/script_commands.csv (tools/script_commands.py): the function behind
   each script command, as ScriptCommand::NAME.
5. dev/script_backends.csv (tools/script_backends.py): the engine function
   a script command does its work with, as <Class>::ForScript_<Command>,
   ScriptBackend where no class is known. Only the rows it names.
6. The RTTI of game/DAOrigins.exe (tools/daoexe.py): every vftable, as
   Class::vftable, or Class::vftable_at_<offset> for a secondary one.

tools/ghidra-mcp.ps1 feeds the `--tsv` output (rva, name, kind per line,
tab-separated since template names hold commas; kind is function or data)
to tools/ghidra/ImportNames.java on every launch, so a finding written into
any source is read back beside the code in Ghidra, and a name set by hand in
Ghidra outranks them all. Inside template brackets `::` is written `.`, so
Ghidra's namespace split falls only between real scopes.

    python tools/names.py          list them
    python tools/names.py --tsv    rva<TAB>name<TAB>kind lines
"""

import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import msvc_names  # noqa: E402

ADDRESSES = os.path.join(HERE, "..", "src", "dao", "Addresses.h")
COMMANDS = os.path.join(HERE, "..", "dev", "script_commands.csv")
HANDLERS = os.path.join(HERE, "..", "dev", "message_handlers.csv")
LOOKUPS = os.path.join(HERE, "..", "dev", "object_lookups.csv")
BACKENDS = os.path.join(HERE, "..", "dev", "script_backends.csv")
PREFERRED_BASE = 0x00400000

ROW = re.compile(
    r'DAO_(ADDRESS|DATA)\(\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*(0[xX][0-9A-Fa-f_]+)\s*\)'
)


def ghidra_path(name):
    """A qualified name whose '::' are only the real scope separators, with no
    spaces (Ghidra refuses them in a symbol: "unsigned int" -> unsigned_int)."""
    return "::".join(
        p.replace("::", ".").replace(" ", "_") for p in msvc_names.split_top(name)
    )


def header_rows():
    """(our name, engine name, va, kind) for every row of the header, in order."""
    with open(ADDRESSES, encoding="utf-8") as f:
        text = f.read()
    return [
        (
            m.group(2),
            m.group(3),
            int(m.group(4).replace("_", ""), 16),
            "data" if m.group(1) == "DATA" else "function",
        )
        for m in ROW.finditer(text)
    ]


def handler_rows():
    if not os.path.exists(HANDLERS):
        return []
    out = []
    with open(HANDLERS, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            va = int(row["function_va"], 16)
            if va == 0 or row.get("trivial") == "1":
                continue
            message = msvc_names.split_top(row["message"])[-1]
            name = f"{row['owner']}::{row['method']}_{message}"
            out.append((name, name, va, "function"))
    return out


def named_rows(path):
    """Rows of a CSV with function_va and name columns, the named ones."""
    if not os.path.exists(path):
        return []
    out = []
    with open(path, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            if row["name"]:
                out.append(
                    (row["name"], row["name"], int(row["function_va"], 16), "function")
                )
    return out


def lookup_rows():
    return named_rows(LOOKUPS)


def backend_rows():
    return named_rows(BACKENDS)


def command_rows():
    """Every script command with an implementation; the 135 registered with
    none are left out."""
    if not os.path.exists(COMMANDS):
        return []
    out = []
    with open(COMMANDS, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            va = int(row["function_va"], 16)
            if va == 0:
                continue
            short = row["name"].removeprefix("COMMAND_")
            out.append((f"COMMAND_{short}", f"ScriptCommand::{short}", va, "function"))
    return out


def vtable_rows():
    """Every vftable the RTTI names; nothing if game/DAOrigins.exe is absent."""
    import daoexe

    if not os.path.exists(daoexe.EXE):
        return []
    out = []
    for va, decorated, offset, _ in daoexe.Image().vtables:
        cls = msvc_names.undecorate(decorated)
        leaf = "vftable" if offset == 0 else f"vftable_at_{offset:x}"
        out.append((f"{cls}::{leaf}", f"{cls}::{leaf}", va, "data"))
    return out


def rows():
    """Every name, one per address, the earlier source winning."""
    seen = set()
    out = []
    for source in (
        header_rows,
        handler_rows,
        lookup_rows,
        command_rows,
        backend_rows,
        vtable_rows,
    ):
        for ours, engine, va, kind in source():
            if va in seen:
                continue
            seen.add(va)
            out.append((ours, engine, va, kind))
    return out


def main(argv):
    as_tsv = "--tsv" in argv
    for ours, engine, va, kind in rows():
        name = ghidra_path(engine or ours)
        if as_tsv:
            print(f"{va - PREFERRED_BASE:x}\t{name}\t{kind}")
        else:
            print(f"{va:#010x}  {kind:<8} {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
