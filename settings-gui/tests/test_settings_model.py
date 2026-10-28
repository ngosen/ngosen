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
        self.assertIsNone(SettingsModel(self.fake).defaults())
        self.model.set("SpellCheck", "False")
        self.fake.iface = None
        self.assertFalse(self.model.save())


class HotkeyRulesTest(unittest.TestCase):
    def model(self, values=None):
        model = SettingsModel(FakeDBusHandler(values))
        model.load()
        return model

    def test_a_new_first_key_keeps_the_others(self):
        model = self.model({"ModeMenuKey": {"0": "grave", "1": "Control+space"}})
        model.set_first_hotkey("ModeMenuKey", "F12")
        self.assertEqual(model.current["ModeMenuKey"], {"0": "F12", "1": "Control+space"})
        model.set_first_hotkey("ModeMenuKey", "")
        self.assertEqual(model.current["ModeMenuKey"], {"0": "Control+space"})

    def test_ctrl_shift_shows_as_one_key(self):
        model = self.model()
        self.assertTrue(model.toggle_is_ctrl_shift())
        self.assertEqual(model.shown_toggle_key(), "Control+Shift")

    def test_a_new_toggle_key_replaces_every_ctrl_shift(self):
        model = self.model()
        model.set_toggle_key("F12")
        self.assertEqual(model.current["ToggleKey"], {"0": "F12"})
        self.assertEqual(model.shown_toggle_key(), "F12")

    def test_ctrl_shift_recorded_or_cleared_restores_every_side(self):
        for text in ("Shift+Control_R", ""):
            model = self.model({"ToggleKey": {"0": "Control+space"}})
            model.set_toggle_key(text)
            self.assertEqual(model.current["ToggleKey"], DEFAULTS["ToggleKey"], repr(text))

    def test_a_new_toggle_key_keeps_keys_copied_from_fcitx5(self):
        model = self.model({"ToggleKey": {"0": "Control+space", "1": "Zenkaku_Hankaku"}})
        model.set_toggle_key("F12")
        self.assertEqual(model.current["ToggleKey"], {"0": "F12", "1": "Zenkaku_Hankaku"})


if __name__ == "__main__":
    unittest.main()
