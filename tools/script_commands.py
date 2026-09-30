"""The engine's script command table, read out of game/DAOrigins.exe.

DAO scripts (the toolset's .nss, compiled to .ncs) call the engine through
numbered commands, and the exe registers each one at start-up with its
number, its name and the function that implements it, in one long run of
calls of the shape

    push  offset "COMMAND_TOGGLEGAMEPAUSE"
    push  offset implementation        ; or push 0: registered, not implemented here
    push  188                          ; the command's number
    mov   ecx, esi
    call  Register

(692 with an implementation, 135 without, on 2026-09-29). This walks the
.text section for that shape and writes dev/script_commands.csv: one row
per command, `id,name,function_va,registered_at_va`, function_va 0 for a
command registered with none. tools/names.py turns the rows into Ghidra
names (ScriptCommand::TOGGLEGAMEPAUSE), and the CSV is the first map of
the engine: every function a script can reach, named.

It also reads script.ldf out of the game's packages\core\data\scripts.erf
(the toolset compiler's declaration of every engine function, each with its
command number) and adds each command's script-side name and signature:
the numbers agree, which is what says a registration number is a script
function number.

    python tools/script_commands.py            rewrite the CSV, report counts
    python tools/script_commands.py --print    print the table instead
"""

import csv
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import daoexe  # noqa: E402

OUT = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "dev", "script_commands.csv"
)

# push imm32 (name); push imm32 (function) | push 0; push imm32 | push imm8 (id);
# mov ecx, r32; call rel32.
REGISTRATION = re.compile(
    rb"\x68(?P<name>....)"
    rb"(?:\x68(?P<fn>....)|\x6a\x00)"
    rb"(?:\x68(?P<id32>....)|\x6a(?P<id8>.))"
    rb"\x8b[\xc8-\xcf]"
    rb"\xe8(?P<call>....)",
    re.S,
)


def registrations(img):
    """(id, name, function va, registered-at va, register-call target) per command."""
    _, va, size, raw = next(s for s in img.sections if s[0] == ".text")
    rows = []
    for m in REGISTRATION.finditer(img.data, raw, raw + size):
        name_va = struct.unpack("<I", m.group("name"))[0]
        name = (
            img.cstring(name_va - img.image_base)
            if img.image_base < name_va < img.image_base + img.size_of_image
            else None
        )
        if not name or not name.startswith("COMMAND_"):
            continue
        fn = struct.unpack("<I", m.group("fn"))[0] if m.group("fn") else 0
        cid = (
            struct.unpack("<I", m.group("id32"))[0]
            if m.group("id32")
            else m.group("id8")[0]
        )
        at = m.start() - raw + va + img.image_base
        call_end = m.end() - raw + va + img.image_base
        target = (call_end + struct.unpack("<i", m.group("call"))[0]) & 0xFFFFFFFF
        rows.append((cid, name, fn, at, target))
    rows.sort()
    return rows


GAME = os.environ.get(
    "DAO_GAME_DIR",
    r"C:\Program Files (x86)\GOG Galaxy\Games\Dragon Age Origins",
)

# A declaration in script.ldf: `void ToggleGamePause(int bPause = TRUE) = 188;`.
DECLARATION = re.compile(
    r"^[ \t]*(\w+(?:\[\])?)[ \t]+(\w+)[ \t]*\(([^;{]*?)\)[ \t]*=[ \t]*(\d+)[ \t]*;",
    re.M | re.S,
)


def erf_entries(path):
    """{name: bytes} of an ERF V2.0 archive: a UTF-16 magic, a count, then per
    entry a 32-character UTF-16 name, an offset and a length."""
    with open(path, "rb") as f:
        d = f.read()
    if d[:16].decode("utf-16le", errors="replace") != "ERF V2.0":
        raise ValueError(f"{path} is not an ERF V2.0 archive")
    count = struct.unpack_from("<I", d, 16)[0]
    out = {}
    for i in range(count):
        o = 32 + 72 * i
        name = d[o : o + 64].decode("utf-16le").split("\0")[0]
        off, length = struct.unpack_from("<II", d, o + 64)
        out[name] = d[off : off + length]
    return out


def declarations(game=GAME):
    """{id: (script name, "ret Name(args)")} from the game's script.ldf, the
    engine functions as the toolset's script compiler knows them, each with
    its command number. Empty if the game is not where GAME says."""
    erf = os.path.join(game, "packages", "core", "data", "scripts.erf")
    if not os.path.exists(erf):
        return {}
    text = erf_entries(erf)["script.ldf"].decode("latin-1")
    out = {}
    for m in DECLARATION.finditer(text):
        ret, name, args, cid = m.groups()
        out[int(cid)] = (name, f"{ret} {name}({' '.join(args.split())})")
    return out


def main(argv):
    img = daoexe.Image()
    rows = registrations(img)
    names = set(img.commands())
    found = {r[1] for r in rows}
    implemented = sum(1 for r in rows if r[2])
    targets = {r[4] for r in rows}
    declared = declarations()
    if "--print" in argv:
        for cid, name, fn, _, _ in rows:
            script = declared.get(cid, ("", ""))[1]
            print(f"{cid:4d}  {name:<44} {fn:#010x}  {script}")
    else:
        with open(OUT, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(
                [
                    "id",
                    "name",
                    "function_va",
                    "registered_at_va",
                    "script_name",
                    "signature",
                ]
            )
            for cid, name, fn, at, _ in rows:
                script, signature = declared.get(cid, ("", ""))
                w.writerow([cid, name, f"{fn:#010x}", f"{at:#010x}", script, signature])
        print(f"wrote {os.path.normpath(OUT)}")
    print(
        f"{len(rows)} commands registered, {implemented} with an implementation; "
        f"{len(names)} COMMAND_ names in the exe, {len(names - found)} of them not registered this way; "
        f"register called at {', '.join(f'{t:#x}' for t in sorted(targets))}; "
        f"registrations from {rows[0][3]:#x} to {rows[-1][3]:#x}"
    )
    if names - found:
        print("not registered this way:", ", ".join(sorted(names - found)[:20]))
    if declared:
        by_id = {r[0]: r[1] for r in rows}
        same = sum(
            1
            for cid, (n, _) in declared.items()
            if by_id.get(cid) == "COMMAND_" + n.upper()
        )
        undeclared = [cid for cid in by_id if cid not in declared]
        unregistered = [cid for cid in declared if cid not in by_id]
        print(
            f"script.ldf: {len(declared)} declarations; {same} carry the registered name "
            f"exactly, {len(declared) - same - len(unregistered)} another name for the same number; "
            f"{len(undeclared)} registered commands not declared; "
            f"declared but not registered: {', '.join(f'{c} {declared[c][0]}' for c in unregistered) or 'none'}"
        )
    else:
        print(f"no scripts.erf under {GAME} (set DAO_GAME_DIR); signatures left empty")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
