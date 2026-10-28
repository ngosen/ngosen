# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
from unittest import mock

# isort: off
from fake_fcitx import FakeDBusHandler
from core import keymap

# isort: on


class KeymapTest(unittest.TestCase):
    def test_reads_the_switch_and_the_table(self):
        fake = FakeDBusHandler(
            {"EnableCustomKeymap": "True"},
            {"custom_keymap": [{"Key": "s", "Value": "DauSac"}]},
        )
        table = keymap.Keymap(fake)
        table.load()
        self.assertTrue(table.enabled)
        self.assertEqual(table.entries, [{"Key": "s", "Value": "DauSac"}])
        self.assertFalse(table.is_modified())

    def test_an_unknown_action_shows_as_the_first_one(self):
        fake = FakeDBusHandler({}, {"custom_keymap": [{"Key": "s", "Value": "Missing"}]})
        table = keymap.Keymap(fake)
        table.load()
        self.assertEqual(table.entries, [{"Key": "s", "Value": keymap.BAMBOO_ACTIONS[0][0]}])

    def test_save_writes_the_switch_and_the_table(self):
        fake = FakeDBusHandler()
        table = keymap.Keymap(fake)
        table.load()
        table.enabled = True
        table.entries.append({"Key": "f", "Value": "DauHuyen"})
        self.assertTrue(table.save())
        self.assertEqual(fake.values["EnableCustomKeymap"], "True")
        self.assertEqual(fake.sub_configs["custom_keymap"], [{"Key": "f", "Value": "DauHuyen"}])
        self.assertFalse(table.is_modified())

    def test_a_refused_table_stays_unsaved(self):
        fake = FakeDBusHandler()
        table = keymap.Keymap(fake)
        table.load()
        table.entries.append({"Key": "f", "Value": "DauHuyen"})
        with mock.patch.object(fake, "set_sub_config_list", return_value=False):
            self.assertFalse(table.save())
        self.assertTrue(table.is_modified())

    def test_a_refused_table_puts_the_switch_back(self):
        fake = FakeDBusHandler()
        table = keymap.Keymap(fake)
        table.load()
        table.enabled = True
        with mock.patch.object(fake, "set_sub_config_list", return_value=False):
            self.assertFalse(table.save())
        self.assertEqual(fake.values["EnableCustomKeymap"], "False")

    def test_every_preset_uses_listed_actions(self):
        for name, entries in keymap.PRESETS.items():
            for key, code in entries:
                self.assertIn(code, keymap.ACTION_CODES, f"{name}: {key}")


if __name__ == "__main__":
    unittest.main()
