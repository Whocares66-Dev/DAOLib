"""The rows of the source that name a function or a global of the game hold
its ID in the address library, and nothing else about it.

    python tools/address_ids.py            turn each new row into its ID, and put its name in the table
    python tools/address_ids.py --check    fail if a row is not yet an ID, or its ID is not what the row says it is (a ctest)
    python tools/address_ids.py 0x005D6740      what is at an address of the GOG listing's
    python tools/address_ids.py 10547           an ID
    python tools/address_ids.py SetPaused       a name, the table's or a constant's here

A row is

    DAO_ADDRESS(kCClientExoApp_SetPaused, 10547)
    DAO_DATA(kClientExoApp, 100835)
    DAO_VFTABLE(64938);

The ID is the address library's (extern/DAOAddressLibrary): what it is
called, and where it is in each build of the game, are in that library's
table, data/addresses.csv, and only there. So a thing has one name wherever
it is spoken of, and an address is never written down twice.

A new finding is read in Ghidra, in the GOG build, as an address. Write it
as the address and the engine's name for it,

    DAO_ADDRESS(kCClientExoApp_SetPaused, "CClientExoApp::SetPaused", 0x005D6740)
    DAO_DATA(kClientExoApp, "g_clientExoApp", 0x00C327EC)
    DAO_VFTABLE(0x00B007D0);

and run this. It finds the ID at that address, gives it the name in the
table if it has none there, and rewrites the row to the ID alone. A vftable
has its name from the exe's RTTI already. A name the table already has
differently is left as the table has it, and said: `addrlib.py name` in the
library changes one on purpose.

A name given is a change to the address library, a repository of its own
(the submodule extern/DAOAddressLibrary), and that repository has its own
keeper: nothing is committed or pushed there from a DAOLib checkout (the
user's rule, 2026-10-01: "Communicate changes to addr lib agent & let it
decide to make changes & commit"). So this writes the name into the
checkout's copy of the table, which is enough to build and to pass
--check, and then:

1. Send the address library's session the row: the ID, function or data,
   the name, the GOG address, and what it is. It commits the name to the
   library's master, where a name is safe ahead of the code that uses it,
   since it changes no ID and no address.
2. In the submodule, `git checkout -- data` to drop the local copy of the
   name, then check out the library's commit that has it (`git fetch`,
   `git checkout <commit>`; a submodule has no branch checked out).
3. Here, `git add extern/DAOAddressLibrary` with the row: the commit that
   uses a name holds the library at a commit that has it.

Commit the row here only then, not between running this and step 2: until
the submodule is at the keeper's commit the name is in this checkout
alone, and a commit made meanwhile would hold the library at a commit
without it and fail --check for everyone else. Rows found while waiting
can be sent as they come; the keeper names them in one commit.

A problem with the library, its tools or its docs goes the same way: said
to its session, changed there.

The library's notes are said too, with a lookup and when a new row's ID
has one: a function `changed` in a build is not the same code there as in
the listing, so a detour at its start still catches it but an offset into
it, or a field offset read out of it, may be wrong there; `shared` is one
address for several IDs, so a detour there catches them all.

An address the library has no ID for is said, with the nearest ID below it;
such a place is written in code as that ID's address plus the offset.

The rows are every DAO_ADDRESS, DAO_DATA and DAO_VFTABLE under src/ and
examples/. An address is the one in the build the Ghidra listing is of,
found in the table by its link stamp: a build's key there is only a label.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
LIBRARY = os.path.join(ROOT, "extern", "DAOAddressLibrary")
# The build every address written in the source was read in: GOG's, as
# src/dao/Module.cpp's kListing says for the running game. All four header
# fields, since Steam's build has GOG's link stamp.
LISTING = {
    "link_stamp": 0x4EB19D53,
    "size_of_image": 0x009EC0A2,
    "entry_point_rva": 0x004E3024,
    "image_base": 0x00400000,
}

HEX = r"0[xX][0-9A-Fa-f]+"
# A row as it stays: the constant and the ID.
ROW = re.compile(r"\bDAO_(ADDRESS|DATA)\(\s*(\w+)\s*,\s*(\d+)\s*\)")
VFTABLE = re.compile(r"\bDAO_VFTABLE\(\s*(\d+)\s*\)")
# A row as a finding is first written: the engine's name and the address,
# with an ID between them in the form the rows had before the names moved.
NEW_ROW = re.compile(
    rf'\b(DAO_(ADDRESS|DATA)\(\s*(\w+)\s*,\s*)"([^"]*)"\s*,\s*(?:\d+\s*,\s*)?({HEX})(\s*\))'
)
NEW_VFTABLE = re.compile(
    rf"\bDAO_VFTABLE\(\s*(?:\d+\s*,\s*)?({HEX})\s*\)"
    rf"|\bstatic\s+constexpr\s+std::uintptr_t\s+kVftable\s*=\s*({HEX})(?=\s*;)"
)
KIND = {"ADDRESS": "function", "DATA": "data"}


def database():
    sys.path.insert(0, os.path.join(LIBRARY, "tools"))
    import addrlib

    return addrlib, addrlib.Database.load()


def listing(db):
    """The key of the listing's build in the table."""
    keys = [
        b["key"]
        for b in db.builds
        if all(int(b[field], 16) == value for field, value in LISTING.items())
    ]
    if len(keys) != 1 or keys[0] not in db.columns:
        raise SystemExit(
            "the address library has no column for GOG's build "
            f"(linked at {LISTING['link_stamp']:#010x}, image size {LISTING['size_of_image']:#x})"
        )
    return keys[0]


def table():
    """{GOG address: ID}, and a function giving the ID at or below one."""
    addrlib, db = database()
    key = listing(db)
    by_address = {address: ids[0] for address, ids in db.by_address(key).items()}
    return by_address, lambda address: addrlib.nearest(db, key, address)


def sources():
    for folder in ("src", "examples"):
        for directory, _, files in os.walk(os.path.join(ROOT, folder)):
            if os.path.basename(directory) == "generated":
                continue
            for name in files:
                if name.endswith((".h", ".cpp")):
                    yield os.path.join(directory, name)


def code(text):
    """The text with its // comments and its #define lines blanked, place for
    place, so a row told of in a comment, or the macro's own definition, is
    not taken for one."""

    def blank(m):
        return " " * len(m.group(0))

    return re.sub(r"//[^\n]*|^[ \t]*#[ \t]*define[^\n]*", blank, text, flags=re.M)


def rows_in(text):
    """(constant or None for a vftable, kind, ID) for each row as it stays."""
    bare = code(text)
    out = [(m.group(2), KIND[m.group(1)], int(m.group(3))) for m in ROW.finditer(bare)]
    out += [(None, "data", int(m.group(1))) for m in VFTABLE.finditer(bare)]
    return out


def addresses_in_source():
    """The listing's address of everything a row of the source is."""
    _, db = database()
    key = listing(db)
    out = set()
    for path in sources():
        with open(path, encoding="utf-8") as f:
            for _, _, number in rows_in(f.read()):
                if number in db.rows and key in db.rows[number].addresses:
                    out.add(db.rows[number].addresses[key])
    return out


def notes_of(db, number):
    """What the library notes about an ID, a line per build and flag."""
    return [
        f"{key}: {flag}, {detail}"
        for noted, key, flag, detail in db.notes
        if noted == number
    ]


def convert(text, db, by_address, nearest, said, noted):
    """The text with each newly written row turned into its ID, its name
    given to the table; (text, rows converted, names given). What the
    library notes about a new row's ID is added to `noted`."""
    bare = code(text)
    named = 0
    edits = []

    def lookup(address, what):
        number = by_address.get(address)
        if number is None:
            near = nearest(address)
            said.append(
                f"no ID at {address:#010x} ({what})"
                + (f": it is ID {near[0]} + {near[1]:#x}" if near else "")
            )
        return number

    for m in NEW_ROW.finditer(bare):
        constant, name, address = m.group(3), m.group(4), int(m.group(5), 16)
        number = lookup(address, constant)
        if number is None:
            continue
        row = db.rows[number]
        if row.kind != KIND[m.group(2)]:
            said.append(
                f"{constant}: ID {number} is a {row.kind} in the table, not a {KIND[m.group(2)]}"
            )
            continue
        if not row.name:
            row.name = name
            named += 1
            noted.append(
                f"to send to the address library's session: ID {number}, {row.kind}, "
                f"{name}, GOG {address:#010x}"
            )
        elif row.name != name:
            said.append(
                f"{constant}: the table calls ID {number} {row.name}, not {name}; left as it is there"
            )
        edits.append((m.start(), m.end(), f"{m.group(1)}{number}{m.group(6)}"))
        noted += [
            f"{constant}, ID {number}, in {note}" for note in notes_of(db, number)
        ]
    for m in NEW_VFTABLE.finditer(bare):
        number = lookup(int(m.group(1) or m.group(2), 16), "a vftable")
        if number is not None:
            edits.append((m.start(), m.end(), f"DAO_VFTABLE({number})"))
    for start, end, new in sorted(edits, reverse=True):
        text = text[:start] + new + text[end:]
    return text, len(edits), named


def faults_of(db, key, path, text):
    """What is wrong with a file's rows, a line each."""
    bare = code(text)
    where = os.path.relpath(path, ROOT)
    out = [
        f"{where}: {m.group(3)} is not yet an ID: run tools/address_ids.py"
        for m in NEW_ROW.finditer(bare)
    ]
    out += [
        f"{where}: a vftable is not yet an ID: run tools/address_ids.py"
        for _ in NEW_VFTABLE.finditer(bare)
    ]
    for constant, kind, number in rows_in(text):
        what = constant or f"DAO_VFTABLE({number})"
        row = db.rows.get(number)
        if row is None:
            out.append(f"{where}: {what}: the address library has no ID {number}")
        elif row.kind != kind:
            out.append(
                f"{where}: {what}: ID {number} is a {row.kind}, and the row says {kind}"
            )
        elif key not in row.addresses:
            out.append(
                f"{where}: {what}: ID {number} is not in the build the listing is of"
            )
        elif not row.name:
            out.append(
                f"{where}: {what}: ID {number} has no name in the address library"
            )
    return out


def describe(db, number, constants):
    row = db.rows[number]
    places = ", ".join(
        f"{key} {address:#010x}" for key, address in row.addresses.items()
    )
    used = f"; here {', '.join(constants)}" if constants else ""
    lines = [f"{number:>6}  {row.kind:<8}  {row.name or '(no name)'}  [{places}]{used}"]
    lines += [f"        in {note}" for note in notes_of(db, number)]
    return "\n".join(lines)


def ask(db, by_address, nearest, what):
    """An address, an ID or a name: what the table has, and the constants
    here that are it."""
    constants = {}
    for path in sources():
        with open(path, encoding="utf-8") as f:
            for constant, _, number in rows_in(f.read()):
                constants.setdefault(number, []).append(constant or "a kVftable")
    if what.lower().startswith("0x"):
        address = int(what, 16)
        if address in by_address:
            print(describe(db, by_address[address], constants.get(by_address[address])))
            return 0
        near = nearest(address)
        if not near:
            print(f"no ID at {address:#010x}")
            return 1
        print(
            f"no ID at {address:#010x}; it is ID {near[0]} + {near[1]:#x}: "
            "use that ID's address plus the offset"
        )
        print(describe(db, near[0], constants.get(near[0])))
        return 1
    if what.isdigit():
        if int(what) not in db.rows:
            print(f"no ID {what}")
            return 1
        print(describe(db, int(what), constants.get(int(what))))
        return 0
    shown = 0
    for number, row in db.rows.items():
        names = [row.name, *constants.get(number, ())]
        if any(what.lower() in name.lower() for name in names):
            print(describe(db, number, constants.get(number)))
            shown += 1
            if shown == 50:
                print("... and more; narrow the name")
                break
    return 0 if shown else 1


def main(argv):
    addrlib, db = database()
    key = listing(db)
    by_address = {address: ids[0] for address, ids in db.by_address(key).items()}

    def nearest(address):
        return addrlib.nearest(db, key, address)

    if argv and not argv[0].startswith("--"):
        return ask(db, by_address, nearest, argv[0])
    check = "--check" in argv
    total = converted = named = 0
    said = []
    noted = []
    used = {}  # ID -> the constants that are it
    for path in sources():
        with open(path, encoding="utf-8", newline="") as f:
            text = f.read()
        if check:
            said += faults_of(db, key, path, text)
        else:
            new, done, given = convert(text, db, by_address, nearest, said, noted)
            named += given
            if done:
                converted += done
                print(
                    f"{os.path.relpath(path, ROOT)}: {done} rows turned into their IDs"
                )
                with open(path, "w", encoding="utf-8", newline="") as f:
                    f.write(new)
                text = new
        rows = rows_in(text)
        total += len(rows)
        for constant, _, number in rows:
            if number in db.rows:
                used.setdefault(number, []).append(constant or "a kVftable")
    for line in said:
        print(line, file=sys.stderr)
    for line in noted:
        print(f"note: {line}")
    if named:
        db.save()
        print(
            f"{named} names written into this checkout's copy of the address library's table. "
            "Commit nothing there: send its session the lines above, and take the commit it "
            "makes. This file's head has the steps."
        )
    if check:
        print(f"{total} rows, {len(said)} faults")
    else:
        print(f"{total} rows, {converted} converted, {len(said)} to look at")
    # Said, not failed on: a build may truly lack a function, and what
    # reaches for it there gets a null it must be ready for.
    for other in db.columns:
        lacking = sorted(n for n in used if other not in db.rows[n].addresses)
        if lacking:
            print(
                f"the {other} build has no address for {len(lacking)} of them: "
                f"IDs {', '.join(map(str, lacking[:12]))}"
            )
    # Nor these: what the library notes about the IDs in use, so that it is
    # seen each time and not only when a row is new.
    flagged = {}
    for number, other, flag, _ in db.notes:
        if number in used:
            flagged.setdefault((other, flag), []).append(number)
    for (other, flag), numbers in sorted(flagged.items()):
        print(
            f"in the {other} build {len(numbers)} of them are {flag}: "
            f"IDs {', '.join(map(str, sorted(numbers)[:12]))}"
        )
    # An alias is allowed, but two rows that are one thing are more often
    # one address read twice under two names.
    for number, constants in sorted(used.items()):
        if len(constants) > 1:
            print(
                f"ID {number} ({db.rows[number].name}) is {len(constants)} rows: {', '.join(constants)}"
            )
    return 1 if said else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
