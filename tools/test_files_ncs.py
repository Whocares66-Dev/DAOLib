"""dao_files ncs: compiled scripts disassembled from the command line (DAOLib #265)."""

import os
import subprocess
import tempfile
import unittest

import archive


def script(code):
    """A compiled script: "NCS V1.0", "B" and the file's size, big-endian, then its code."""
    size = 13 + len(code)
    return b"NCS V1.0B" + size.to_bytes(4, "big") + bytes(code)


class NcsTest(unittest.TestCase):
    def test_a_script_is_disassembled_under_its_name_and_a_non_script_skipped(self):
        with tempfile.TemporaryDirectory() as folder:
            ncs = os.path.join(folder, "lib_test.ncs")
            with open(ncs, "wb") as out:
                out.write(script([0x20, 0x00]))  # RETN
            other = os.path.join(folder, "other.ncs")
            with open(other, "wb") as out:
                out.write(b"not a script")
            run = subprocess.run(
                [archive.tool(), "ncs", ncs, other],
                capture_output=True,
                text=True,
                check=False,
            )
        self.assertEqual(run.returncode, 0, run.stderr)
        lines = run.stdout.splitlines()
        self.assertEqual(lines[0], "== lib_test.ncs")
        self.assertIn("RETN", lines[1])
        self.assertIn("== other.ncs", lines)
        self.assertEqual(lines[-1], "not a compiled script: skipped")

    def test_an_archive_not_there_fails(self):
        run = subprocess.run(
            [archive.tool(), "ncs", "no_such_archive.erf|*.ncs"],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(run.returncode, 1)
        self.assertIn("no archive", run.stderr)


if __name__ == "__main__":
    unittest.main()
