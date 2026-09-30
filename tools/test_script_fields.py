"""Tests for tools/script_fields.py's tracking of fields through a command's
body, over sequences shaped like DAOrigins.exe's own.

    python -m unittest discover -s tools -p "test_*.py"
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from script_fields import field_accesses, path_text  # noqa: E402

CREATURE = 0x005D58C0
PUSH_INT, PUSH_FLOAT = 0x00A2D650, 0x00A2D690
PUSHES = {PUSH_INT: "int", PUSH_FLOAT: "float"}


def run(body):
    return field_accesses(body, {CREATURE}, PUSHES)


class Getters(unittest.TestCase):
    def test_get_combat_state(self):
        # 0x0065DDD0, GetCombatState's own instructions.
        body = [
            ("call", "0xa2f6d0"),
            ("mov", "ecx, dword ptr [esp + 4]"),
            ("push", "ecx"),
            ("mov", "ecx, dword ptr [0xc327ec]"),
            ("call", "0x5d58c0"),
            ("test", "eax, eax"),
            ("je", "0x65de16"),
            ("mov", "edx, dword ptr [eax + 0xc60]"),
            ("mov", "esi, dword ptr [edx]"),
            ("mov", "ecx, dword ptr [0xc1d080]"),
            ("push", "esi"),
            ("call", "0xa2d650"),
        ]
        found = run(body)
        self.assertIn((CREATURE, (0xC60,), 4, "read", ""), found)
        self.assertIn((CREATURE, (0xC60, 0x0), 4, "returned", "int"), found)

    def test_a_float_through_the_x87_stack(self):
        body = [
            ("call", "0x5d58c0"),
            ("mov", "esi, eax"),
            ("fld", "dword ptr [esi + 0x2a4]"),
            ("mov", "ecx, dword ptr [0xc1d080]"),
            ("push", "ecx"),
            ("fstp", "dword ptr [esp]"),
            ("call", "0xa2d690"),
        ]
        self.assertIn((CREATURE, (0x2A4,), 4, "returned", "float"), run(body))

    def test_a_byte_field_compared(self):
        body = [("call", "0x5d58c0"), ("cmp", "byte ptr [eax + 0x14], 6")]
        self.assertEqual(run(body), [(CREATURE, (0x14,), 1, "read", "")])


class Setters(unittest.TestCase):
    def test_a_field_written(self):
        body = [
            ("call", "0x5d58c0"),
            ("mov", "ecx, dword ptr [esp + 8]"),
            ("mov", "dword ptr [eax + 0x31c], ecx"),
        ]
        self.assertEqual(run(body), [(CREATURE, (0x31C,), 4, "write", "")])


class Limits(unittest.TestCase):
    def test_a_call_clobbers_the_object_in_eax(self):
        body = [
            ("call", "0x5d58c0"),
            ("call", "0xa2f3c0"),
            ("mov", "edx, dword ptr [eax + 0x10]"),
        ]
        self.assertEqual(run(body), [])

    def test_the_object_kept_in_esi_survives_a_call(self):
        body = [
            ("call", "0x5d58c0"),
            ("mov", "esi, eax"),
            ("call", "0xa2f3c0"),
            ("mov", "edx, dword ptr [esi + 0x10]"),
        ]
        self.assertEqual(run(body), [(CREATURE, (0x10,), 4, "read", "")])

    def test_a_byte_field_is_not_followed_as_a_pointer(self):
        body = [
            ("call", "0x5d58c0"),
            ("movzx", "ecx, byte ptr [eax + 0x14]"),
            ("mov", "edx, dword ptr [ecx]"),
        ]
        self.assertEqual(run(body), [(CREATURE, (0x14,), 1, "read", "")])

    def test_path_text(self):
        self.assertEqual(path_text((0xC60, 0x0)), "+0xc60 +0x0")


if __name__ == "__main__":
    unittest.main()
