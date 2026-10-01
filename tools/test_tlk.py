"""tools/tlk.py on a version 0.2 talk table built here."""

import os
import tempfile
import unittest

import tlk


def build_tlk(strings):
    """A talk table as the game's are laid out: tlk.write."""
    return tlk.write(strings)


class TalkTableTest(unittest.TestCase):
    def test_strings_by_their_reference(self):
        table = tlk.TalkTable(
            build_tlk([(379897, "Shield Pummel"), (379898, "Shield Mastery")])
        )
        self.assertEqual(len(table), 2)
        self.assertEqual(table.get(379898), "Shield Mastery")
        self.assertIsNone(table.get(1))

    def test_a_table_of_another_type_is_refused(self):
        data = bytearray(build_tlk([]))
        data[12:16] = b"G2DA"
        with self.assertRaises(ValueError):
            tlk.TalkTable(bytes(data))

    def test_a_csv_becomes_a_table(self):
        with tempfile.TemporaryDirectory() as folder:
            source = os.path.join(folder, "strings.csv")
            target = os.path.join(folder, "turnbased_en-us.tlk")
            with open(source, "w", encoding="utf-8", newline="") as f:
                f.write('id,text\n1,End turn\n2,"Too far, 3 m"\n7000001,Fin du tour\n')
            self.assertEqual(tlk.main(["--write", source, target]), 0)
            with open(target, "rb") as f:
                table = tlk.TalkTable(f.read())
        self.assertEqual(len(table), 3)
        self.assertEqual(table.get(2), "Too far, 3 m")
        self.assertEqual(table.get(7000001), "Fin du tour")

    def test_the_labels_are_in_file_order(self):
        data = tlk.write([(1, "x")])
        # The game's own files keep "TLK " and "STRN" as written.
        self.assertEqual(data[28:32], b"TLK ")
        self.assertEqual(data[44:48], b"STRN")


if __name__ == "__main__":
    unittest.main()
