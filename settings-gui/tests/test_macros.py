# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
from unittest import mock

# isort: off
from fake_fcitx import FakeDBusHandler
from core import macros

# isort: on


class MacroTableTest(unittest.TestCase):
    def test_reads_the_switches_and_the_table(self):
        fake = FakeDBusHandler(
            {"EnableMacro": "False", "TimeFormat": "%H:%M:%S"},
            {"ngosen-macro": [{"Key": "kg", "Value": "khô gà"}]},
        )
        table = macros.MacroTable(fake)
        table.load()
        self.assertFalse(table.options["EnableMacro"])
        self.assertEqual(table.options["TimeFormat"], "%H:%M:%S")
        self.assertEqual(table.entries, [{"Key": "kg", "Value": "khô gà"}])
        self.assertFalse(table.is_modified())

    def test_save_writes_the_switches_and_the_table(self):
        fake = FakeDBusHandler()
        table = macros.MacroTable(fake)
        table.load()
        table.options["CapitalizeMacro"] = False
        table.entries.append({"Key": "vn", "Value": "Việt Nam"})
        self.assertTrue(table.save())
        self.assertEqual(fake.values["CapitalizeMacro"], "False")
        self.assertEqual(fake.sub_configs["ngosen-macro"], [{"Key": "vn", "Value": "Việt Nam"}])
        self.assertFalse(table.is_modified())

    def test_a_refused_table_stays_unsaved(self):
        fake = FakeDBusHandler()
        table = macros.MacroTable(fake)
        table.load()
        table.entries.append({"Key": "vn", "Value": "Việt Nam"})
        with mock.patch.object(fake, "set_sub_config_list", return_value=False):
            self.assertFalse(table.save())
        self.assertTrue(table.is_modified())

    def test_reset_goes_back_to_the_defaults(self):
        fake = FakeDBusHandler(
            {"EnableMacroInOffMode": "True"},
            {"ngosen-macro": [{"Key": "kg", "Value": "khô gà"}]},
        )
        table = macros.MacroTable(fake)
        table.load()
        self.assertTrue(table.is_modified_from_default())
        table.reset()
        self.assertFalse(table.is_modified_from_default())

    def test_keys_must_be_letters_or_digits(self):
        self.assertFalse(macros.is_invalid_key("vn2"))
        self.assertFalse(macros.is_invalid_key(""))
        self.assertTrue(macros.is_invalid_key("v n"))


if __name__ == "__main__":
    unittest.main()
