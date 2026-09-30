"""The game's script sources, out of the toolset's content database.

The Dragon Age toolset ships its content database as a SQL Server 2005
backup (tools\\dbbak\\bw_dragonage_content.bak in the installer,
dev/TOOLSET.md). It holds the source of every script the game runs --
rules_core.nss, "the main entry point into combat AI", among them -- which
the game itself ships only compiled. tools/sqlbak.py reads it without SQL
Server.

Two tables matter, found 2026-09-30 by where the text of the scripts is:

    object 242   (id int, text): 4,209 script texts, each a large value
    object 238   the toolset's resources, one row per version: the id of its
                 content first, then, among the variable columns, the name
                 (rules_core), the folder (\\_Core Scripts\\), the owner, and
                 a state letter, C for the current version and H for history

Joined on the id, the rows in state C give 2,101 scripts with their names
and folders. This writes each to toolset/extract/scripts/<folder>/<name>.nss
(toolset/ is ignored by git: the scripts are BioWare's) and an index,
scripts.csv, beside them.

    python tools/toolset_scripts.py            extract, report counts
    python tools/toolset_scripts.py --check    report only, write nothing

The backup is extracted from the installer with
    7z e -otoolset\\extract\\tools\\dbbak toolset\\extract\\DragonAgeToolset1.01Setup.exe
       tools\\dbbak\\bw_dragonage_content.bak
"""

import csv
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sqlbak  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
BACKUP = os.path.join(
    ROOT, "toolset", "extract", "tools", "dbbak", "bw_dragonage_content.bak"
)
OUT = os.path.join(ROOT, "toolset", "extract", "scripts")

TEXTS = 242
RESOURCES = 238
RESOURCE_COLUMNS = 12
NAME, FOLDER, OWNER, STATE = 0, 1, 2, 3  # among the variable columns


def texts(backup):
    """{id: pointer to the text}."""
    out = {}
    for fixed, _, variable in backup.rows(TEXTS):
        if variable and isinstance(variable[0], tuple):
            out[struct.unpack_from("<I", fixed, 0)[0]] = variable[0][1]
    return out


def current_scripts(backup, pointers):
    """[(id, name, folder, owner)] for the resources in state C with a text."""
    out = []
    for fixed, columns, variable in backup.rows(RESOURCES):
        if columns != RESOURCE_COLUMNS or len(variable) <= STATE or len(fixed) < 4:
            continue
        if any(isinstance(v, tuple) for v in variable[: STATE + 1]):
            continue
        rid = struct.unpack_from("<I", fixed, 0)[0]
        if variable[STATE] != b"C" or rid not in pointers:
            continue
        name, folder, owner = (
            variable[i].decode("latin-1") for i in (NAME, FOLDER, OWNER)
        )
        out.append((rid, name, folder, owner))
    return out


def safe(part):
    return re.sub(r'[<>:"|?*]', "_", part).strip(" .") or "_"


def main(argv):
    if not os.path.exists(BACKUP):
        print(
            f"no {os.path.normpath(BACKUP)}; see this tool's docstring for extracting it",
            file=sys.stderr,
        )
        return 2
    backup = sqlbak.Backup.open(BACKUP)
    pointers = texts(backup)
    scripts = current_scripts(backup, pointers)
    names = {}
    for rid, name, _, _ in scripts:
        names.setdefault(name, []).append(rid)
    duplicates = {n: ids for n, ids in names.items() if len(ids) > 1}
    print(
        f"{len(pointers)} texts, {len(scripts)} current scripts, {len(duplicates)} names current more than once"
    )
    if "--check" in argv:
        return 0
    rows = []
    total = 0
    for rid, name, folder, owner in sorted(
        scripts, key=lambda s: (s[2].lower(), s[1].lower())
    ):
        text = backup.value(pointers[rid])
        parts = [safe(p) for p in folder.replace("/", "\\").split("\\") if p]
        filename = safe(name) + (f".{rid}" if name in duplicates else "") + ".nss"
        path = os.path.join(OUT, *parts, filename)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(text)
        total += len(text)
        rows.append([rid, name, folder, owner, len(text), os.path.relpath(path, OUT)])
    with open(os.path.join(OUT, "scripts.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["id", "name", "folder", "owner", "bytes", "path"])
        w.writerows(rows)
    print(f"wrote {len(rows)} scripts, {total:,} bytes, to {os.path.normpath(OUT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
