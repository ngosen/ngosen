# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

# isort: off
from fake_fcitx import DEFAULTS, FakeDBusHandler
from core.settings_model import SettingsModel

# isort: on


class SettingsModelTest(unittest.TestCase):
    def setUp(self):
        self.fake = FakeDBusHandler()
        self.model = SettingsModel(self.fake)
        self.assertTrue(self.model.load())

    def test_save_keeps_settings_changed_elsewhere(self):
        self.model.set("SpellCheck", "False")
        self.fake.values["InputMethod"] = "VNI"
        self.assertTrue(self.model.save())
        self.assertEqual(self.fake.values["SpellCheck"], "False")
        self.assertEqual(self.fake.values["InputMethod"], "VNI")
        self.assertFalse(self.model.is_modified())

    def test_unsaved_values_count_as_changes(self):
        self.model.load({**DEFAULTS, "InputMethod": "VNI"})
        self.assertEqual(self.model.modified, {"InputMethod": "VNI"})
        self.assertTrue(self.model.is_modified())

    def test_defaults_come_from_the_engine(self):
        self.fake.values["InputMethod"] = "VNI"
        self.model.load()
        self.assertTrue(self.model.is_modified_from_default())
        self.assertEqual(self.model.defaults()["InputMethod"], "Telex")

    def test_duplicate_shortcut_among_shown_modes(self):
        self.model.set("ShortcutEmoji", "q")
        (message, keys), *rest = self.model.shortcut_conflicts()
        self.assertEqual(rest, [])
        self.assertEqual(sorted(keys), ["ShortcutEmoji", "ShortcutPreedit"])
        self.assertIn("'q'", message)

    def test_hidden_mode_shortcut_does_not_conflict(self):
        self.model.set("ShortcutEmoji", "q")
        self.model.set("ShowModeEmoji", "False")
        self.assertEqual(self.model.shortcut_conflicts(), [])

    def test_without_fcitx5(self):
        self.fake.get_config = lambda: {}
        self.assertFalse(self.model.load())
        self.assertIsNone(self.model.defaults())
        self.model.set("SpellCheck", "False")
        self.fake.iface = None
        self.assertFalse(self.model.save())


if __name__ == "__main__":
    unittest.main()
