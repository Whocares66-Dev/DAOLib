"""Compile the options screen's recorder into src/dao/generated/OptionsRecord.h (DAOLib #273).

The recorder is ActionScript 2 (tools/options_record/frame_1/DoAction.as): appended to the game's
optionsmenu.gfx, it records each of the screen's own controls on its clip as the movie makes it
(dao/Options.h, Entries and Set). The game's movies are SWF 8, so the block is compiled the way the
turn-based mod builds its movies (DAOTurnBased tools/movie.py): a one-frame shell with an empty
frame script, JPEXS FFDec's -importScript to compile the source into it, the result decompiled
again and checked for the source's functions, then the frame's DoAction body written out as bytes
for GfxMovie's AppendDoAction.

    python tools\\options_record.py                      write the header
    python tools\\options_record.py --ffdec C:\\...\\ffdec  FFDec elsewhere than C:\\Modding

FFDec is https://github.com/jindrapetrik/jpexs-decompiler, run on Java.
"""

from __future__ import annotations

import argparse
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCE = REPO / "tools" / "options_record"
HEADER = REPO / "src" / "dao" / "generated" / "OptionsRecord.h"
FFDEC = Path(r"C:\Modding\ffdec_26.3.0")
SWF_VERSION = 8  # the game's movies are SWF 8
DO_ACTION = 12


def tag(code: int, body: bytes) -> bytes:
    if len(body) < 0x3F:
        return struct.pack("<H", (code << 6) | len(body)) + body
    return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body


def shell() -> bytes:
    """A one-frame SWF 8 with an empty frame script, for FFDec to fill. Its stage is a 1 by 1
    RECT of 2-bit fields (all zero), which no one draws."""
    tags = (
        tag(69, struct.pack("<I", 0))
        + tag(DO_ACTION, b"\0")
        + tag(1, b"")
        + tag(0, b"")
    )
    rest = bytes([0x10, 0]) + struct.pack("<HH", 30 << 8, 1) + tags
    return b"FWS" + bytes([SWF_VERSION]) + struct.pack("<I", 8 + len(rest)) + rest


def rect_size(data: bytes, at: int) -> int:
    """The bytes a SWF RECT at `at` takes."""
    bits = data[at] >> 3
    return (5 + 4 * bits + 7) // 8


def do_action_body(movie: bytes) -> bytes | None:
    """The first DoAction tag's body of an uncompressed SWF."""
    if movie[:3] != b"FWS":
        return None
    at = 8 + rect_size(movie, 8) + 4
    while at + 2 <= len(movie):
        code_and_length = struct.unpack_from("<H", movie, at)[0]
        code, length = code_and_length >> 6, code_and_length & 0x3F
        at += 2
        if length == 0x3F:
            length = struct.unpack_from("<I", movie, at)[0]
            at += 4
        if code == DO_ACTION:
            return movie[at : at + length]
        if code == 0:
            return None
        at += length
    return None


def ffdec(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["java", "-jar", str(root / "ffdec-cli.jar"), *args],
        capture_output=True,
        text=True,
        check=False,
    )


def compile_source(root: Path) -> bytes | None:
    source = (SOURCE / "frame_1" / "DoAction.as").read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory() as work:
        work_dir = Path(work)
        base = work_dir / "shell.swf"
        base.write_bytes(shell())
        scripts = work_dir / "scripts"
        shutil.copytree(SOURCE, scripts)
        made = work_dir / "made.swf"
        result = ffdec(root, "-importScript", str(base), str(made), str(scripts))
        if result.returncode != 0 or not made.exists():
            print(result.stdout, result.stderr)
            return None
        back = work_dir / "back"
        ffdec(root, "-export", "script", str(back), str(made))
        exported = list(back.rglob("DoAction.as"))
        text = (
            exported[0].read_text(encoding="utf-8", errors="replace")
            if exported
            else ""
        )
        wanted = set(re.findall(r"_global\.(DAOLib\w+) = function", source))
        lost = sorted(name for name in wanted if name not in text)
        if not text or lost:
            print(result.stdout, result.stderr)
            print(
                "the compiled script is not the source's: "
                + (", ".join(lost) if text else "it has none")
            )
            return None
        return do_action_body(made.read_bytes())


def header(body: bytes) -> str:
    rows = [
        ", ".join(f"0x{b:02X}" for b in body[i : i + 16])
        for i in range(0, len(body), 16)
    ]
    lines = ",\n".join(f"    {row}" for row in rows)
    return (
        "// The options screen's recorder, compiled by tools/options_record.py from\n"
        "// tools/options_record/frame_1/DoAction.as: a DoAction tag's body for\n"
        "// core::gfx_movie::AppendDoAction (dao/Options.cpp, DAOLib #273). Do not edit.\n"
        "#pragma once\n\n"
        "#include <array>\n\n"
        "namespace dao::generated\n{\n\n"
        "// clang-format off\n"
        f"inline constexpr std::array<unsigned char, {len(body)}> kOptionsRecord{{\n{lines}}};\n"
        "// clang-format on\n\n"
        "} // namespace dao::generated\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--ffdec", type=Path, default=FFDEC, help="FFDec's folder")
    args = parser.parse_args()
    if not (args.ffdec / "ffdec-cli.jar").exists():
        print(f"no FFDec at {args.ffdec}; see this script's head")
        return 1
    body = compile_source(args.ffdec)
    if body is None:
        print("FFDec could not compile the recorder")
        return 1
    HEADER.write_text(header(body), encoding="utf-8", newline="\n")
    print(f"wrote {HEADER} ({len(body)} bytes of script)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
