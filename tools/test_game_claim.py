"""The claim file's rules, over text: what stands and what does not."""

import json
import unittest

from game_claim import describe, standing

NOW = 1_800_000_000.0


def text(**fields):
    return json.dumps(fields)


class Standing(unittest.TestCase):
    def test_a_claim_until_later_stands(self):
        claim = standing(text(owner="tester", note="steam", until=NOW + 600), NOW)
        self.assertEqual(claim["owner"], "tester")

    def test_one_run_out_does_not(self):
        self.assertIsNone(standing(text(owner="tester", until=NOW - 1), NOW))
        self.assertIsNone(standing(text(owner="tester", until=NOW), NOW))

    def test_the_owners_own_does_not_stop_it(self):
        self.assertIsNone(
            standing(text(owner="lib-agent", until=NOW + 600), NOW, "lib-agent")
        )
        self.assertIsNotNone(
            standing(text(owner="tester", until=NOW + 600), NOW, "lib-agent")
        )

    def test_an_unreadable_file_is_no_claim(self):
        for bad in (
            "",
            "{",
            "[]",
            "null",
            text(owner="tester"),
            text(owner="tester", until="soon"),
        ):
            self.assertIsNone(standing(bad, NOW), bad)

    def test_the_line_says_who_and_how_long(self):
        line = describe({"owner": "tester", "note": "steam", "until": NOW + 90}, NOW)
        self.assertIn("tester", line)
        self.assertIn("1.5 minutes", line)
        self.assertIn("steam", line)


if __name__ == "__main__":
    unittest.main()
