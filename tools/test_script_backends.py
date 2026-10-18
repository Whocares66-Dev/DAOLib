"""Tests for tools/script_backends.py's reading of instruction sequences,
over sequences shaped like DAOrigins.exe's own (the lookup is 0x005D58C0's
body, the command GetCombatState's and its kin).

    python -m unittest discover -s tools -p "test_*.py"
"""

import collections
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from script_backends import (  # noqa: E402
    as_slot,
    chosen_name,
    method_calls,
    sole_callee,
)

LOOKUP = 0x005D58C0


class CastSlot(unittest.TestCase):
    def test_the_slot_read_through_the_vtable(self):
        body = [
            ("push", "ecx"),
            ("mov", "edx, dword ptr [esp + 8]"),
            ("mov", "ecx, dword ptr [ecx + 0xcc]"),  # the array: not a vtable
            ("call", "0xa58190"),
            ("mov", "ecx, dword ptr [esp]"),
            ("mov", "al, byte ptr [ecx + 0x14]"),
            ("mov", "edx, dword ptr [ecx]"),  # the vtable
            ("mov", "eax, dword ptr [edx + 0x10]"),  # its cast slot
            ("call", "eax"),
        ]
        self.assertEqual(as_slot(body), 0x10)

    def test_a_called_slot_counts_too(self):
        body = [("mov", "eax, dword ptr [ecx]"), ("call", "dword ptr [eax + 0x24]")]
        self.assertEqual(as_slot(body), 0x24)

    def test_a_slot_below_ten_is_written_without_0x(self):
        # CClientExoApp::GetAreaById (0x005D5960): the area's cast is slot 2.
        body = [
            ("mov", "ecx, dword ptr [esp]"),
            ("mov", "al, byte ptr [ecx + 0x14]"),
            ("mov", "edx, dword ptr [ecx]"),
            ("mov", "eax, dword ptr [edx + 8]"),
            ("call", "eax"),
        ]
        self.assertEqual(as_slot(body), 8)

    def test_no_vtable_no_slot(self):
        self.assertIsNone(
            as_slot([("mov", "ecx, dword ptr [ecx + 0xcc]"), ("ret", "")])
        )


class VirtualCalls(unittest.TestCase):
    def test_a_call_through_the_found_objects_vtable(self):
        body = [
            ("call", "0xa2f6d0"),  # pop the object id
            ("push", "ecx"),
            ("mov", "ecx, dword ptr [0xc327ec]"),
            ("call", "0x5d58c0"),  # the lookup: eax is the creature
            ("mov", "esi, eax"),
            ("test", "esi, esi"),
            ("mov", "edx, dword ptr [esi]"),  # its vtable
            ("mov", "ecx, esi"),
            ("call", "dword ptr [edx + 0x58]"),
        ]
        self.assertEqual(method_calls(body, {LOOKUP}), [("virtual", LOOKUP, 0x58)])

    def test_a_slot_loaded_then_called(self):
        body = [
            ("call", "0x5d58c0"),
            ("mov", "ecx, eax"),
            ("mov", "edx, dword ptr [eax]"),
            ("mov", "eax, dword ptr [edx + 0x1c4]"),
            ("call", "eax"),
        ]
        self.assertEqual(method_calls(body, {LOOKUP}), [("virtual", LOOKUP, 0x1C4)])

    def test_an_overwritten_register_is_no_longer_the_object(self):
        body = [
            ("call", "0x5d58c0"),
            ("mov", "esi, eax"),
            ("mov", "esi, dword ptr [0xc1d080]"),  # esi now the VM
            ("mov", "edx, dword ptr [esi]"),
            ("call", "dword ptr [edx + 0x58]"),
        ]
        self.assertEqual(method_calls(body, {LOOKUP}), [])

    def test_another_functions_result_is_not_an_object(self):
        body = [
            ("call", "0x5d5840"),  # not a known lookup
            ("mov", "edx, dword ptr [eax]"),
            ("call", "dword ptr [edx + 0x58]"),
        ]
        self.assertEqual(method_calls(body, {LOOKUP}), [])


class DirectMethods(unittest.TestCase):
    def test_a_direct_call_with_the_object_as_this(self):
        # SetCombatState: the creature looked up, then 0x006AE0A0 on it.
        body = [
            ("call", "0x5d58c0"),
            ("mov", "esi, eax"),
            ("call", "0xa2f3c0"),  # pops the int: eax, ecx, edx clobbered
            ("mov", "ecx, esi"),
            ("push", "edi"),
            ("call", "0x6ae0a0"),
        ]
        self.assertEqual(method_calls(body, {LOOKUP}), [("direct", LOOKUP, 0x6AE0A0)])

    def test_a_call_with_something_else_as_this_is_not_a_method(self):
        body = [
            ("call", "0x5d58c0"),
            ("mov", "ecx, dword ptr [0xc1d080]"),
            ("call", "0xa2d650"),
        ]
        self.assertEqual(method_calls(body, {LOOKUP}), [])


class SoleCallee(unittest.TestCase):
    def test_one_callee_left_is_the_backend(self):
        callers = collections.Counter({0xA2F6D0: 300, 0x4C0DB0: 150, 0x6AE0A0: 1})
        calls = [0xA2F6D0, 0x4C0DB0, 0x6AE0A0]
        self.assertEqual(sole_callee(calls, {0xA2F6D0}, callers), 0x6AE0A0)

    def test_two_left_is_none(self):
        callers = collections.Counter({0x111: 1, 0x222: 2})
        self.assertIsNone(sole_callee([0x111, 0x222], set(), callers))

    def test_repeated_calls_to_one_function_are_one(self):
        callers = collections.Counter({0x111: 1})
        self.assertEqual(sole_callee([0x111, 0x111], set(), callers), 0x111)


class ChosenName(unittest.TestCase):
    LIBRARY = {0x006C1400: "CCCreature::SetAppearance"}

    def test_the_librarys_name_wins_over_the_guess(self):
        self.assertEqual(
            chosen_name(
                0x006C1400, self.LIBRARY, "CCCreature::ForScript_SetItemHeraldry"
            ),
            "CCCreature::SetAppearance",
        )

    def test_the_guess_stands_where_the_library_has_no_name(self):
        self.assertEqual(
            chosen_name(0x006B6120, self.LIBRARY, "CCCreature::ForScript_SetClassRank"),
            "CCCreature::ForScript_SetClassRank",
        )

    def test_an_unnamed_candidate_takes_the_librarys_name_or_stays_unnamed(self):
        self.assertEqual(
            chosen_name(0x006C1400, self.LIBRARY, ""), "CCCreature::SetAppearance"
        )
        self.assertEqual(chosen_name(0x006B7690, self.LIBRARY, ""), "")


if __name__ == "__main__":
    unittest.main()
