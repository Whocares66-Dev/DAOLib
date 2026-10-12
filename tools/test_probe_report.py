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


class LoadingScreens(unittest.TestCase):
    """probe-loading.tsv, as the recorder writes a loading screen."""

    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        with open(
            os.path.join(self.dir.name, "functions.tsv"), "w", encoding="utf-8"
        ) as f:
            f.write("# entry\tend\tname\n")
            f.write("006c8420\t006c871f\tGameModeLoading::AreaTransition\n")
            f.write("006c5c50\t006c5e3f\tGameModeLoading::EnterArea\n")
            f.write("005ca590\t005ca62f\tAreaLayout::LoadAround\n")
            f.write("004e6120\t004e613f\tEvent::Wait\n")
        rows = [
            "# probe-loading",
            "1\tscreen\t1000\t1200\t100",
            "1\tcpu\t11\t180\t1",
            "1\tcpu\t12\t40\t0\tDragonAge::Render",
            "1\tstep\t5\t10\t1\t90\t30",
            "1\tleaf\t5\t10\t1\t0x5ca5a0\t60",
            "1\tinner\t5\t10\t1\t0x6c5c60\t30",
            "1\tstack\t5\t10\t1\t0x6c8530\t90",
            "1\tstack\t5\t10\t1\t0x6c5d00\t90",
            "1\tstep\t5\t10\t0\t10\t10",
            "1\thealth\t50\t12\t0\t1\t0",
            "1\tmodule\t5\t10\t1\td3d9.dll\t30",
            "1\tvia\t5\t10\t1\tkernelbase.dll!WaitForSingleObjectEx < d3d9.dll < 0x6c5d00\t30",
            "1\tchain\t5\t10\t1\t0x6c5d00,0x6c8530,0x0,0x0\t90",
            "1\tDragonAge::Render.step\t5\t10\t1\t20\t15",
            "1\tDragonAge::Render.leaf\t5\t10\t1\t0x5ca5a0\t5",
            "1\tDragonAge::Render.inner\t5\t10\t1\t0x4e6130\t15",
            "2\tscreen\t5000\t5100\t1",
            "2\tstep\t7\t-1\t1\t1\t0",
        ]
        with open(
            os.path.join(self.dir.name, "probe-loading.tsv"), "w", encoding="utf-8"
        ) as f:
            f.write("\n".join(rows) + "\n")
        self.functions = probe_report.Functions(
            os.path.join(self.dir.name, "functions.tsv")
        )

    def tearDown(self):
        self.dir.cleanup()

    def test_a_screen_is_read_with_its_steps_and_threads(self):
        screens = probe_report.read_loading(self.dir.name)
        self.assertEqual([s["n"] for s in screens], [1, 2])
        first = screens[0]
        self.assertEqual(
            (first["from"], first["to"], first["samples"]), (1000, 1200, 100)
        )
        self.assertEqual(
            first["cpu"], {11: (180, True, "?"), 12: (40, False, "DragonAge::Render")}
        )
        inside = first["steps"][(5, 10, True)]
        self.assertEqual((inside["samples"], inside["outside"]), (90, 30))
        self.assertEqual(inside["leaf"][0x5CA5A0], 60)
        self.assertEqual(first["steps"][(5, 10, False)]["samples"], 10)
        self.assertEqual(first["health"], (50, 12, 0, 1, 0))
        self.assertEqual(inside["module"]["d3d9.dll"], 30)
        self.assertEqual(inside["chain"][(0x6C5D00, 0x6C8530, 0, 0)], 90)
        render = first["threads"]["DragonAge::Render"][(5, 10, True)]
        self.assertEqual((render["samples"], render["leaf"][0x5CA5A0]), (20, 5))
        # A step of -1 is a state without steps.
        self.assertIn((7, -1, True), screens[1]["steps"])

    def test_a_step_is_ranked_by_function_under_and_in(self):
        out = io.StringIO()
        with redirect_stdout(out):
            code = probe_report.loading(self.dir.name, self.functions, 5, only=[1])
        text = out.getvalue()
        self.assertEqual(code, 0)
        self.assertIn("1 loading screen(s), 200 ms, 100 samples", text)
        self.assertIn("processor time, ms: its thread 180, DragonAge::Render 40", text)
        self.assertIn(
            "state 5 step 10: 100 samples, about 200 ms (100% of all); 90 inside the update, 10 between",
            text,
        )
        # Under: both callers in every inside sample. In: the leaf's function and the one that called out.
        self.assertIn(" 90.0%  GameModeLoading::AreaTransition", text)
        self.assertIn(" 60.0%  AreaLayout::LoadAround", text)
        self.assertIn(" 30.0%  GameModeLoading::EnterArea", text)
        self.assertIn("the longest gap 12 ms", text)
        self.assertIn("outside the game's code, in: d3d9.dll 30%", text)
        self.assertIn(
            " 30.0%  kernelbase.dll!WaitForSingleObjectEx < d3d9.dll < GameModeLoading::EnterArea",
            text,
        )
        self.assertIn(
            " 90.0%  GameModeLoading::EnterArea < GameModeLoading::AreaTransition", text
        )
        # A watched thread: fifteen of its twenty samples left the exe from a wait.
        self.assertIn("DragonAge::Render, sampled with the loading thread", text)
        self.assertIn("while state 5 step 10: 20 samples, 75% in a wait", text)

    def test_two_returns_into_one_function_are_one_sample_under_it(self):
        counts = {0x6C8430: 40, 0x6C8530: 90, 0x6C5D00: 10}
        once = probe_report.by_function(self.functions, counts, once=True)
        self.assertEqual(once["GameModeLoading::AreaTransition"], 90)
        summed = probe_report.by_function(self.functions, counts)
        self.assertEqual(summed["GameModeLoading::AreaTransition"], 130)

    def test_no_file_is_no_screens(self):
        empty = tempfile.TemporaryDirectory()
        self.addCleanup(empty.cleanup)
        self.assertEqual(probe_report.read_loading(empty.name), [])


if __name__ == "__main__":
    unittest.main()
