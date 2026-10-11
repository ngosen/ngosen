# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

from qtpy.QtCore import QEvent
from qtpy.QtWidgets import QCheckBox, QComboBox, QMessageBox
from support import FakeDBusHandler, app
from ui.components import HotkeyEditorWidget
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dict_editor import DictEditorPage
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory
from ui.pages.keymap_editor import KeymapEditorPage
from ui.pages.macro_editor import MacroEditorPage
from ui.pages.mode_manager import ModeManagerPage


def checkbox(page, text):
    # A page rebuilt in place deletes its old widgets on the next event loop turn.
    app.sendPostedEvents(None, QEvent.DeferredDelete)
    return next(cb for cb in page.findChildren(QCheckBox) if cb.text() == text)


class ResetTest(unittest.TestCase):
    def test_reset_shows_and_saves_defaults(self):
        fake = FakeDBusHandler({"SpellCheck": "False"})
        page = DynamicSettingsPage(fake, category=SettingsCategory.TYPING)
        page.restore_defaults()
        self.assertTrue(checkbox(page, "Enable Spell Check Using Dictionary").isChecked())
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["SpellCheck"], "True")

    def test_reset_covers_pages_not_opened(self):
        fake = FakeDBusHandler(
            {"SpellCheck": "False"},
            {
                "app_rules": [{"App": "firefox", "Mode": "5"}],
                "ngosen-macro": [{"Key": "kg", "Value": "khô gà"}],
            },
        )
        window = NgoSenSettingsWindow(dbus_handler=fake)
        with mock.patch.object(QMessageBox, "question", return_value=QMessageBox.Yes):
            window.on_restore_defaults()
        window.save_pending()
        self.assertEqual(fake.values["SpellCheck"], "True")
        self.assertEqual(fake.sub_configs["app_rules"], [])
        self.assertEqual(fake.sub_configs["ngosen-macro"], [])

    def test_reset_keymap_turns_it_off(self):
        fake = FakeDBusHandler(
            {"EnableCustomKeymap": "True"},
            {"custom_keymap": [{"Key": "s", "Value": "DauSac"}]},
        )
        page = KeymapEditorPage(fake)
        page.restore_defaults()
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["EnableCustomKeymap"], "False")
        self.assertEqual(fake.sub_configs["custom_keymap"], [])

    def test_reset_macro_formats(self):
        fake = FakeDBusHandler({"TimeFormat": "%I:%M %p", "DateFormat": "%Y-%m-%d"})
        page = MacroEditorPage(fake)
        page.restore_defaults()
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["TimeFormat"], "%H:%M")
        self.assertEqual(fake.values["DateFormat"], "%d/%m/%Y")

    def test_reset_clears_application_rules(self):
        fake = FakeDBusHandler(sub_configs={"app_rules": [{"App": "firefox", "Mode": "5"}]})
        page = ModeManagerPage(fake)
        page.restore_defaults()
        self.assertTrue(page.save_data())
        self.assertEqual(fake.sub_configs["app_rules"], [])


class ModeTest(unittest.TestCase):
    def test_default_mode_has_one_control(self):
        # The general page sets Mode; a second control on the applications page fought with it.
        page = ModeManagerPage(FakeDBusHandler())
        self.assertEqual(page.findChildren(QComboBox), [])


class HotkeyTest(unittest.TestCase):
    def test_editing_first_hotkey_keeps_the_others(self):
        fake = FakeDBusHandler({"ModeMenuKey": {"0": "grave", "1": "Control+space"}})
        page = DynamicSettingsPage(fake, category=SettingsCategory.SHORTCUTS)
        editor = page.findChildren(HotkeyEditorWidget)[0]
        editor.hotkey_capture.textChanged.emit("F12")
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["ModeMenuKey"], {"0": "F12", "1": "Control+space"})


class DictionaryTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.bundled = os.path.join(self.tmp.name, "bundled.dict")
        with open(self.bundled, "w", encoding="utf-8") as f:
            f.write("an\nba\n")
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": self.tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)

    def page(self, fake):
        page = DictEditorPage(fake)
        page._get_global_dict_path = lambda: self.bundled
        page.load_data()
        return page

    def test_saving_reloads_the_dictionary(self):
        fake = FakeDBusHandler({"EnableDictionary": "True"})
        page = self.page(fake)
        page.upsert_row("xyz")
        self.assertTrue(page.save_data())
        self.assertEqual(fake.reloads, ["ngosen"])

    def test_reset_goes_back_to_the_bundled_words(self):
        fake = FakeDBusHandler({"EnableDictionary": "True"})
        local = os.path.join(self.tmp.name, "fcitx5/ngosen/vietnamese.cm.dict")
        os.makedirs(os.path.dirname(local))
        with open(local, "w", encoding="utf-8") as f:
            f.write("an\nba\nxyz\n")
        page = self.page(fake)
        page.restore_defaults()
        self.assertFalse(page.cb_enable.isChecked())
        self.assertEqual(sorted(page.words), ["an", "ba"])
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["EnableDictionary"], "False")
        self.assertFalse(os.path.exists(local))


if __name__ == "__main__":
    unittest.main()
