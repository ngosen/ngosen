# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Recording a hotkey from keysym names, as both settings windows do."""

import unittest

import fake_fcitx  # noqa: F401  (puts settings-gui on the path)
from core.hotkeys import HotkeyRecorder, hotkey_string


class HotkeyStringTest(unittest.TestCase):
    def test_a_letter_with_control_is_upper_case(self):
        self.assertEqual(hotkey_string("a", {"Control"}), "Control+A")
        self.assertEqual(hotkey_string("A", {"Control", "Shift"}), "Control+Shift+A")

    def test_shift_tab_is_written_as_fcitx5_reads_it(self):
        self.assertEqual(hotkey_string("ISO_Left_Tab", {"Shift"}), "Shift+Tab")

    def test_a_shifted_symbol_does_not_repeat_shift(self):
        self.assertEqual(hotkey_string("exclam", {"Shift"}), "exclam")
        self.assertEqual(hotkey_string("space", {"Super"}), "Super+space")


class HotkeyRecorderTest(unittest.TestCase):
    def setUp(self):
        self.recorder = HotkeyRecorder()

    def test_ctrl_shift_alone_is_recorded_on_release(self):
        self.recorder.press_modifier("Control", "Control_L")
        self.recorder.press_modifier("Shift", "Shift_L")
        self.assertEqual(self.recorder.release_modifier("Shift"), "Control+Shift_L")

    def test_a_repeating_modifier_changes_nothing(self):
        self.assertTrue(self.recorder.press_modifier("Control", "Control_R"))
        self.assertFalse(self.recorder.press_modifier("Control", "Control_R"))
        self.assertEqual(self.recorder.release_modifier("Control"), "Control_R")

    def test_a_key_ends_the_modifier_only_key(self):
        self.recorder.press_modifier("Control", "Control_L")
        self.assertEqual(self.recorder.press_key("space", {"Control"}), "Control+space")
        self.assertEqual(self.recorder.release_modifier("Control"), "")

    def test_a_key_with_no_name_records_nothing(self):
        self.recorder.press_modifier("Control", "Control_L")
        self.assertEqual(self.recorder.press_key("", {"Control"}), "")
        self.assertEqual(self.recorder.release_modifier("Control"), "")
        self.assertEqual(self.recorder.held, set())


if __name__ == "__main__":
    unittest.main()
