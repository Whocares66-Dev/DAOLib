"""The names of the exe's functions and globals, for Ghidra.

They are the address library's (extern/DAOAddressLibrary, the `name` column
of its data/addresses.csv), the one place a name is written down: the
exe's own, from its RTTI (every vftable, as Class::vftable, or
Class::vftable_at_<offset> for a secondary one), and the ones given by
whoever worked out what a thing is.

tools/ghidra-mcp.ps1 feeds the `--tsv` output (rva, name, kind per line,
tab-separated since template names hold commas; kind is function or data)
to tools/ghidra/ImportNames.java on every launch, so a finding is read back
beside the code in Ghidra, and a name set by hand in Ghidra outranks them
all. Inside template brackets `::` is written `.`, so Ghidra's namespace
split falls only between real scopes. The addresses are the GOG build's,
which the Ghidra project is of.

    python tools/names.py          list them
    python tools/names.py --tsv    rva<TAB>name<TAB>kind lines
    python tools/names.py --give   name in the table what the tables under dev/ name and it does not

A name is given to the table from a row of the source, by
tools/address_ids.py; by hand, with the library's `addrlib.py name`; or by
`--give`, from what this project's own reading of the exe has found. Those
tables say more than a name (which function answers which message, which
runs which script command), and the name each implies is made here, the
earlier winning an address two of them name:

- dev/message_handlers.csv (tools/message_handlers.py): each message
  handler's method, as Owner::Method_Message, since the methods overload on
  the message type;
- dev/object_lookups.csv (tools/script_backends.py): the application's
  typed lookups by object id, as CClientExoApp::Get<Class>ById;
- dev/script_commands.csv (tools/script_commands.py): the function behind
  each script command, as ScriptCommand::NAME;
- dev/script_backends.csv (tools/script_backends.py): the engine function
  a script command does its work with, as <Class>::ForScript_<Command>,
  ScriptBackend where no class is known. Only the rows it names.

`--give` never changes a name the table has: run it after one of those
tables has grown, then commit the table in extern/DAOAddressLibrary.
"""

import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import address_ids  # noqa: E402
import msvc_names  # noqa: E402

COMMANDS = os.path.join(HERE, "..", "dev", "script_commands.csv")
HANDLERS = os.path.join(HERE, "..", "dev", "message_handlers.csv")
LOOKUPS = os.path.join(HERE, "..", "dev", "object_lookups.csv")
BACKENDS = os.path.join(HERE, "..", "dev", "script_backends.csv")
PREFERRED_BASE = 0x00400000


def ghidra_path(name):
    """A qualified name whose '::' are only the real scope separators, with no
    spaces (Ghidra refuses them in a symbol: "unsigned int" -> unsigned_int)."""
    return "::".join(
        p.replace("::", ".").replace(" ", "_") for p in msvc_names.split_top(name)
    )


def for_ghidra(name):
    """Whether a name of the table's is one to give Ghidra. Not the ones
    Ghidra has its own for, and better: an import (`dll!function`), an
    export (still decorated), a class's type descriptor."""
    # A class in an anonymous namespace begins with `?` too (?A0x2df6e4b4::),
    # but only a decorated name has `@@` in it.
    decorated = name.startswith("?") and "@@" in name
    return not ("!" in name or decorated or name.endswith("::typeinfo"))


def rows():
    """(name, va, kind) for every named thing with an address in the build
    the listing is of, in address order."""
    _, db = address_ids.database()
    key = address_ids.listing(db)
    return sorted(
        (
            (row.name, row.addresses[key], row.kind)
            for row in db.rows.values()
            if row.name and key in row.addresses and for_ghidra(row.name)
        ),
        key=lambda r: r[1],
    )


def handler_names():
    if not os.path.exists(HANDLERS):
        return []
    out = []
    with open(HANDLERS, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            va = int(row["function_va"], 16)
            if va == 0 or row.get("trivial") == "1":
                continue
            message = msvc_names.split_top(row["message"])[-1]
            out.append((f"{row['owner']}::{row['method']}_{message}", va))
    return out


def named(path):
    """(name, va) for the named rows of a CSV with function_va and name."""
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8", newline="") as f:
        return [
            (row["name"], int(row["function_va"], 16))
            for row in csv.DictReader(f)
            if row["name"]
        ]


def command_names():
    """Every script command with an implementation; the 135 registered with
    none are left out."""
    if not os.path.exists(COMMANDS):
        return []
    out = []
    with open(COMMANDS, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            va = int(row["function_va"], 16)
            if va != 0:
                out.append(
                    (f"ScriptCommand::{row['name'].removeprefix('COMMAND_')}", va)
                )
    return out


def derived():
    """(name, va) for what the tables under dev/ name, one per address."""
    seen = set()
    out = []
    for name, va in (
        *handler_names(),
        *named(LOOKUPS),
        *command_names(),
        *named(BACKENDS),
    ):
        if va not in seen:
            seen.add(va)
            out.append((name, va))
    return out


def give():
    """Names in the table what the tables under dev/ name and it does not."""
    _, db = address_ids.database()
    key = address_ids.listing(db)
    by_address = db.by_address(key)
    given = already = other = 0
    for name, va in derived():
        ids = by_address.get(va, [])
        if len(ids) != 1:
            print(
                f"{va:#010x} ({name}) is the address of {len(ids)} IDs", file=sys.stderr
            )
            continue
        row = db.rows[ids[0]]
        if not row.name:
            row.name = name
            given += 1
        elif row.name == name:
            already += 1
        else:
            other += 1
    if given:
        db.save()
    print(
        f"{given} names given to the table, {already} it had, "
        f"{other} it has another name for (a row of the source's, or one given by hand)"
    )
    if given:
        print(
            f"commit and push it in {os.path.relpath(address_ids.LIBRARY, address_ids.ROOT)}, "
            "then commit the submodule here"
        )
    return 0


def main(argv):
    if "--give" in argv:
        return give()
    as_tsv = "--tsv" in argv
    for name, va, kind in rows():
        if as_tsv:
            print(f"{va - PREFERRED_BASE:x}\t{ghidra_path(name)}\t{kind}")
        else:
            print(f"{va:#010x}  {kind:<8} {ghidra_path(name)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
