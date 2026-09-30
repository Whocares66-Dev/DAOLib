"""Tests for tools/probe_report.py's reading of a session and of Ghidra's
function list, over small files made here.

    python -m unittest discover -s tools -p "test_*.py"
"""

import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import probe_report  # noqa: E402


class FunctionLookup(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        path = os.path.join(self.dir.name, "functions.tsv")
        with open(path, "w", encoding="utf-8") as f:
            f.write("# entry\tend\tname\n")
            f.write(
                "006403c0\t006403e6\tGameModeController::HandleMessage_UserTogglePauseMessage\n"
            )
            f.write("005d6740\t005d6a43\tCClientExoApp::SetPaused\n")
        self.functions = probe_report.Functions(path)

    def tearDown(self):
        self.dir.cleanup()

    def test_an_address_inside_a_function_finds_it(self):
        self.assertEqual(
            self.functions.containing(0x005D67D4),
            (0x005D6740, "CClientExoApp::SetPaused"),
        )
        self.assertEqual(self.functions.containing(0x006403C0)[0], 0x006403C0)
        self.assertEqual(self.functions.containing(0x006403E6)[0], 0x006403C0)

    def test_an_address_between_functions_finds_none(self):
        self.assertEqual(self.functions.containing(0x006403E7), (None, "?0x006403e7"))
        self.assertEqual(self.functions.containing(0x00401000)[0], None)


class Session(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        logs = self.dir.name
        with open(os.path.join(logs, "probe-calls.tsv"), "w", encoding="utf-8") as f:
            f.write("# probe-calls\n")
            f.write("1000\tstatus\t0\t1\t0\n")
            f.write("1000\tcall\t0x0065ddd0\t12\tCOMMAND_GETCOMBATSTATE\n")
            f.write("2000\tstatus\t1\t1\t1\n")
            f.write("2000\tcall\t0x0065ddd0\t3\tCOMMAND_GETCOMBATSTATE\n")
            f.write(
                "2000\tcall\t0x00640100\t1\tGameModeController::HandleMessage_PauseMessage\n"
            )
        self.logs = logs

    def tearDown(self):
        self.dir.cleanup()

    def test_calls_are_summed_over_a_stretch(self):
        out = io.StringIO()
        with redirect_stdout(out):
            probe_report.calls_in(self.logs, probe_report.parse_range("0-5"), 20)
        lines = out.getvalue().splitlines()
        self.assertEqual(
            lines[0].split(), ["15", "0x0065ddd0", "COMMAND_GETCOMBATSTATE"]
        )
        self.assertEqual(len(lines), 2)

    def test_status_and_calls_are_read_apart(self):
        status, calls = probe_report.read_calls(self.logs)
        self.assertEqual(status, [(1000, 0, 1, 0), (2000, 1, 1, 1)])
        self.assertEqual(len(calls), 3)


if __name__ == "__main__":
    unittest.main()
