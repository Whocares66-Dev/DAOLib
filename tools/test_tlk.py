"""tools/tlk.py's talk tables, read back through tools/gff4.py."""

import os
import tempfile
import unittest

import gff4
import tlk


def strings_of(data):
    """{id: text} of a talk table, as tools/gff4.py reads it; its empty slots left out."""
    g = gff4.Gff4(data)
    if g.type != "TLK ":
        raise ValueError(f"a {g.type!r}, not a talk table")
    return {
        s[tlk.STRING_ID]: s[tlk.STRING_TEXT]
        for s in g.walk().get(tlk.STRING_LIST, [])
        if s[tlk.STRING_ID] != tlk.NONE
    }


class TalkTableTest(unittest.TestCase):
    def test_strings_by_their_reference(self):
        table = strings_of(
            tlk.write([(379897, "Shield Pummel"), (379898, "Shield Mastery")])
        )
        self.assertEqual(len(table), 2)
        self.assertEqual(table[379898], "Shield Mastery")
        self.assertNotIn(1, table)

    def test_a_table_of_no_strings_has_none(self):
        self.assertEqual(strings_of(tlk.write([])), {})

    def test_a_csv_becomes_a_table(self):
        with tempfile.TemporaryDirectory() as folder:
            source = os.path.join(folder, "strings.csv")
            target = os.path.join(folder, "turnbased_en-us.tlk")
            with open(source, "w", encoding="utf-8", newline="") as f:
                f.write('id,text\n1,End turn\n2,"Too far, 3 m"\n7000001,Fin du tour\n')
            self.assertEqual(tlk.main(["--write", source, target]), 0)
            with open(target, "rb") as f:
                table = strings_of(f.read())
        self.assertEqual(len(table), 3)
        self.assertEqual(table[2], "Too far, 3 m")
        self.assertEqual(table[7000001], "Fin du tour")

    def test_no_csv_is_told_how(self):
        self.assertEqual(tlk.main([]), 2)

    def test_the_labels_are_in_file_order(self):
        data = tlk.write([(1, "x")])
        # The game's own files keep "TLK " and "STRN" as written.
        self.assertEqual(data[28:32], b"TLK ")
        self.assertEqual(data[44:48], b"STRN")


if __name__ == "__main__":
    unittest.main()
