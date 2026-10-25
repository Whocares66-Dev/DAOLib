"""The game's archives' files as the core reads them, through dao_files extract (tools/files).

The tools that read a file out of an archive (script.ldf out of scripts.erf, a data table out of
2da.erf, a sample of an install's GFF4 files) read it through this, so they take the core's reader,
with its archive versions, and not one of their own (DAOLib #106, which retired script_commands'
erf_entries).

The tool is $DAO_FILES, which ctest sets to the build's own, else the first built of
build/debug, build/core-x64 and build/release.

    from archive import read
    ldf = read(scripts_erf, "script.ldf")["script.ldf"]
"""

import os
import subprocess
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
BUILT = [
    os.path.join(ROOT, "build", preset, "dao_files.exe")
    for preset in ("debug", "core-x64", "release")
]


def tool():
    """The dao_files to run: $DAO_FILES, else the first built."""
    given = os.environ.get("DAO_FILES")
    if given:
        return given
    for path in BUILT:
        if os.path.exists(path):
            return path
    raise FileNotFoundError(
        "no dao_files: build DAOLib (tools/build.ps1) or set DAO_FILES"
    )


def read(archive, *names):
    """{name: bytes} of an archive's files: those named (* and ? patterns, any case), or every one.

    A name given without a wildcard that the archive lacks raises KeyError; an entry the archive
    holds enciphered or compressed is left out."""
    with tempfile.TemporaryDirectory() as out:
        run = subprocess.run(
            [tool(), "extract", archive, out, *names],
            capture_output=True,
            text=True,
            check=False,
        )
        if run.returncode != 0:
            raise KeyError(f"{archive}: {run.stderr.strip() or run.stdout.strip()}")
        files = {}
        for name in os.listdir(out):
            with open(os.path.join(out, name), "rb") as f:
                files[name] = f.read()
        return files
