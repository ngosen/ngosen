# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

from qtpy.QtCore import QEvent
from qtpy.QtWidgets import QCheckBox, QLabel, QMessageBox, QPushButton
from support import DEFAULTS, FakeDBusHandler, app
from ui.components import HotkeyEditorWidget
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dict_editor import DictEditorPage
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory
from ui.pages.keymap_editor import KeymapEditorPage
from ui.pages.macro_editor import MacroEditorPage


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

    def asked(self, window):
        with mock.patch("ui.main_window.confirm", return_value=False) as question:
            window.on_restore_defaults()
        return question.call_args.args[2]

    def test_reset_says_the_toggle_key_goes_back_to_ctrl_shift(self):
        window = NgoSenSettingsWindow(
            dbus_handler=FakeDBusHandler({"ToggleKey": {"0": "Control+space"}})
        )
        self.assertIn("goes back to Ctrl+Shift", self.asked(window))
        self.assertNotIn(
            "Ctrl+Shift", self.asked(NgoSenSettingsWindow(dbus_handler=FakeDBusHandler()))
        )

    def test_reset_sees_a_toggle_key_not_saved_yet(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        window._build_all_pages()
        page = next(
            p
            for p in window._pages
            if isinstance(p, DynamicSettingsPage) and p.findChildren(HotkeyEditorWidget)
        )
        page.findChildren(HotkeyEditorWidget)[0].hotkey_capture.textChanged.emit("F12")
        self.assertIn("goes back to Ctrl+Shift", self.asked(window))

    def test_reset_covers_pages_not_opened(self):
        fake = FakeDBusHandler(
            {"SpellCheck": "False"},
            {
                "app_rules": [{"App": "firefox", "Mode": "5"}],
                "ngosen-macro": [{"Key": "kg", "Value": "khô gà"}],
            },
        )
        window = NgoSenSettingsWindow(dbus_handler=fake)
        with mock.patch("ui.main_window.confirm", return_value=True):
            window.on_restore_defaults()
        window.save_pending()
        self.assertEqual(fake.values["SpellCheck"], "True")
        self.assertEqual(fake.sub_configs["app_rules"], [])
        self.assertEqual(fake.sub_configs["ngosen-macro"], [])

    def test_reset_button_sees_pages_not_opened(self):
        cases = {
            "macros": FakeDBusHandler(
                sub_configs={"ngosen-macro": [{"Key": "kg", "Value": "khô gà"}]}
            ),
            "keymap": FakeDBusHandler(
                sub_configs={"custom_keymap": [{"Key": "s", "Value": "DauSac"}]}
            ),
        }
        for name, fake in cases.items():
            with self.subTest(name):
                window = NgoSenSettingsWindow(dbus_handler=fake)
                app.processEvents()
                self.assertTrue(window.btn_reset.isEnabled())

    def test_reset_button_off_at_defaults(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        app.processEvents()
        self.assertFalse(window.btn_reset.isEnabled())

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


class ConfirmButtonsTest(unittest.TestCase):
    """Qt names Yes/No in the desktop's language, which can differ from the window's."""

    def buttons_shown(self, ask):
        shown = []

        def record(box):
            shown.extend(b.text() for b in box.buttons())
            return 0

        refuse = mock.Mock(side_effect=AssertionError("Qt's own Yes/No buttons"))
        with (
            mock.patch.object(QMessageBox, "exec", record),
            mock.patch.object(QMessageBox, "question", refuse),
        ):
            ask()
        return sorted(shown)

    def test_reset_names_its_buttons(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        shown = self.buttons_shown(window.on_restore_defaults)
        self.assertEqual(shown, ["&Cancel", "&Restore defaults"])

    def test_keymap_preset_names_its_buttons(self):
        page = KeymapEditorPage(FakeDBusHandler())
        shown = self.buttons_shown(page.on_load_preset)
        self.assertEqual(shown, ["&Cancel", "&Replace"])


class MacroTest(unittest.TestCase):
    def test_same_key_in_other_case_updates_the_row(self):
        # The typing core matches macro keys in any case, so "VN" would never be used.
        fake = FakeDBusHandler({}, {"ngosen-macro": [{"Key": "vn", "Value": "Việt Nam"}]})
        page = MacroEditorPage(fake)
        page.input_key.setText("VN")
        page.input_val.setText("Vietnam")
        self.assertEqual(page.btn_add.text(), "Update")
        page.upsert_row("VN", "Vietnam")
        self.assertTrue(page.save_data())
        self.assertEqual(fake.sub_configs["ngosen-macro"], [{"Key": "vn", "Value": "Vietnam"}])

    def test_a_freshly_opened_table_is_not_modified(self):
        fake = FakeDBusHandler(
            {"TimeFormat": "%k:%M"},
            {"ngosen-macro": [{"Key": "vn", "Value": "Việt Nam"}, {"Key": "VN", "Value": "VN"}]},
        )
        page = MacroEditorPage(fake)
        self.assertFalse(page.is_modified())

    def test_reload_shows_what_fcitx5_has(self):
        fake = FakeDBusHandler({}, {"ngosen-macro": [{"Key": "kg", "Value": "khô gà"}]})
        page = MacroEditorPage(fake)
        # The main window asks every page whether it changed on each edit.
        window = mock.Mock(on_changed=page.is_modified_from_default)
        page.window = lambda: window
        page.upsert_row("unsaved", "never saved")
        fake.values["EnableMacro"] = "False"
        page.load_data()
        self.assertEqual(page.macros.entries, [{"Key": "kg", "Value": "khô gà"}])
        self.assertEqual(page.table.rowCount(), 1)


class KeymapTest(unittest.TestCase):
    def test_a_freshly_opened_table_is_not_modified(self):
        fake = FakeDBusHandler({}, {"custom_keymap": [{"Key": "s", "Value": "Missing"}]})
        page = KeymapEditorPage(fake)
        self.assertFalse(page.is_modified())

    def test_reload_shows_what_fcitx5_has(self):
        fake = FakeDBusHandler({}, {"custom_keymap": [{"Key": "s", "Value": "DauSac"}]})
        page = KeymapEditorPage(fake)
        # The main window asks every page whether it changed on each edit.
        window = mock.Mock(on_changed=lambda: (page.is_modified(), page.is_modified_from_default()))
        page.window = lambda: window
        page.upsert_row("f", "DauHuyen")
        fake.values["EnableCustomKeymap"] = "True"
        page.load_data()
        self.assertEqual(page.keymap.entries, [{"Key": "s", "Value": "DauSac"}])
        self.assertEqual(page.table.rowCount(), 1)


class HotkeyTest(unittest.TestCase):
    def test_editing_first_hotkey_keeps_the_others(self):
        fake = FakeDBusHandler({"ModeMenuKey": {"0": "grave", "1": "Control+space"}})
        page = DynamicSettingsPage(fake, category=SettingsCategory.SHORTCUTS)
        editor = page.findChildren(HotkeyEditorWidget)[1]
        editor.hotkey_capture.textChanged.emit("F12")
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["ModeMenuKey"], {"0": "F12", "1": "Control+space"})


class ToggleKeyTest(unittest.TestCase):
    def page(self, fake):
        page = DynamicSettingsPage(fake, category=SettingsCategory.SHORTCUTS)
        page.show()
        return page

    def offer(self, page):
        return next(b for b in page.findChildren(QPushButton) if b.text() == "Use Ctrl+Shift")

    def test_toggle_key_comes_first(self):
        page = self.page(FakeDBusHandler())
        self.assertEqual(
            page.findChildren(HotkeyEditorWidget)[0].hotkey_capture.current_key, "Control+Shift"
        )

    def test_ctrl_shift_users_get_no_offer(self):
        page = self.page(FakeDBusHandler())
        self.assertFalse(self.offer(page).isVisible())

    def test_ctrl_space_users_can_switch_to_ctrl_shift(self):
        fake = FakeDBusHandler({"ToggleKey": {"0": "Control+space"}})
        page = self.page(fake)
        button = self.offer(page)
        self.assertTrue(button.isVisible())
        button.click()
        self.assertTrue(page.save_data())
        self.assertEqual(
            fake.values["ToggleKey"],
            {
                "0": "Control+Shift_L",
                "1": "Control+Shift_R",
                "2": "Shift+Control_L",
                "3": "Shift+Control_R",
            },
        )
        self.assertFalse(button.isVisible())

    def test_a_new_key_replaces_every_ctrl_shift(self):
        fake = FakeDBusHandler()
        page = self.page(fake)
        page.findChildren(HotkeyEditorWidget)[0].hotkey_capture.textChanged.emit("F12")
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["ToggleKey"], {"0": "F12"})
        self.assertTrue(self.offer(page).isVisible())

    def test_clearing_the_key_restores_ctrl_shift(self):
        fake = FakeDBusHandler({"ToggleKey": {"0": "Control+space"}})
        page = self.page(fake)
        page.findChildren(HotkeyEditorWidget)[0].btn_clear.click()
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["ToggleKey"], DEFAULTS["ToggleKey"])

    def test_a_new_key_keeps_keys_copied_from_fcitx5(self):
        fake = FakeDBusHandler({"ToggleKey": {"0": "Control+space", "1": "Zenkaku_Hankaku"}})
        page = self.page(fake)
        page.findChildren(HotkeyEditorWidget)[0].hotkey_capture.textChanged.emit("F12")
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["ToggleKey"], {"0": "F12", "1": "Zenkaku_Hankaku"})

    def test_recorded_ctrl_shift_keeps_every_side(self):
        fake = FakeDBusHandler({"ToggleKey": {"0": "Control+space"}})
        page = self.page(fake)
        page.findChildren(HotkeyEditorWidget)[0].hotkey_capture.textChanged.emit("Shift+Control_R")
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["ToggleKey"], DEFAULTS["ToggleKey"])

    def test_ctrl_shift_ends_a_recording(self):
        page = self.page(FakeDBusHandler({"ToggleKey": {"0": "Control+space"}}))
        capture = page.findChildren(HotkeyEditorWidget)[0].hotkey_capture
        capture.setChecked(True)
        self.offer(page).click()
        self.assertFalse(capture.isChecked())

    def test_other_input_methods_get_a_hint(self):
        hint = "fcitx5 has other input methods too; its own key still switches to them."
        page = self.page(FakeDBusHandler())
        self.assertIn(hint, [label.text() for label in page.findChildren(QLabel)])
        alone = FakeDBusHandler()
        alone.input_method_names = lambda: ["ngosen"]
        page = self.page(alone)
        self.assertNotIn(hint, [label.text() for label in page.findChildren(QLabel)])


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

    def test_reset_button_sees_added_words(self):
        local = os.path.join(self.tmp.name, "fcitx5/ngosen/vietnamese.cm.dict")
        os.makedirs(os.path.dirname(local))
        with open(local, "w", encoding="utf-8") as f:
            f.write("an\nba\nxyz\n")
        bundled = self.bundled
        with mock.patch.object(DictEditorPage, "_get_global_dict_path", lambda self: bundled):
            window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
            app.processEvents()
        self.assertTrue(window.btn_reset.isEnabled())


if __name__ == "__main__":
    unittest.main()
