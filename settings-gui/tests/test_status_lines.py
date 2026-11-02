# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

# isort: off
from support import FakeDBusHandler, app
from qtpy.QtCore import QEvent
from qtpy.QtWidgets import QLabel
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dict_editor import DictEditorPage
from ui.pages.dynamic_settings import DynamicSettingsPage, HintLabel, SettingsCategory
from ui.pages.keymap_editor import KeymapEditorPage
from ui.pages.macro_editor import MacroEditorPage

from core import page_status

# isort: on


class NoFcitx(FakeDBusHandler):
    down = False

    def get_config(self):
        return {} if self.down else super().get_config()


def model_of(values):
    return page_status.read(FakeDBusHandler(values))


def hint_shown(page, name):
    label = page.findChild(QLabel, name)
    return label is not None and not label.isHidden()


class LinesTest(unittest.TestCase):
    ALL_ON = {"EnableDictionary": "True", "AutoNonVnRestore": "True", "SpellCheck": "True"}

    def test_the_dictionary_line_names_each_switch_in_the_way(self):
        self.assertEqual(page_status.dictionary_line(model_of(self.ALL_ON)), "")
        for key in self.ALL_ON:
            model = model_of({**self.ALL_ON, key: "False"})
            line = page_status.dictionary_line(model)
            self.assertIn(model.label(key), line)
            for other in set(self.ALL_ON) - {key}:
                self.assertNotIn(model.label(other), line)

    def test_the_dictionary_line_names_all_the_switches_in_the_way(self):
        model = model_of(dict.fromkeys(self.ALL_ON, "False"))
        line = page_status.dictionary_line(model)
        for key in self.ALL_ON:
            self.assertIn(model.label(key), line)

    def test_the_keymap_line_covers_the_switch_and_the_input_method(self):
        on = {"EnableCustomKeymap": "True", "InputMethod": "Custom"}
        self.assertEqual(page_status.keymap_line(model_of(on)), "")
        model = model_of({**on, "EnableCustomKeymap": "False"})
        self.assertIn(model.label("EnableCustomKeymap"), page_status.keymap_line(model))
        self.assertNotIn(model.label("InputMethod"), page_status.keymap_line(model))
        model = model_of({**on, "InputMethod": "Telex"})
        self.assertIn(model.label("InputMethod"), page_status.keymap_line(model))
        self.assertNotIn(model.label("EnableCustomKeymap"), page_status.keymap_line(model))
        model = model_of({"EnableCustomKeymap": "False", "InputMethod": "Telex"})
        self.assertIn(model.label("InputMethod"), page_status.keymap_line(model))
        self.assertIn(model.label("EnableCustomKeymap"), page_status.keymap_line(model))

    def test_the_macro_line_follows_its_switch(self):
        self.assertEqual(page_status.macro_line(model_of({"EnableMacro": "True"})), "")
        self.assertNotEqual(page_status.macro_line(model_of({"EnableMacro": "False"})), "")

    def test_the_retone_line_shows_only_when_it_is_on_with_another_charset(self):
        line = page_status.retone_line
        self.assertEqual(
            line(model_of({"RetoneSelection": "True", "OutputCharset": "Unicode"})), ""
        )
        self.assertEqual(line(model_of({"RetoneSelection": "False", "OutputCharset": "VIQR"})), "")
        self.assertNotEqual(
            line(model_of({"RetoneSelection": "True", "OutputCharset": "VIQR"})), ""
        )

    def test_a_key_fcitx5_does_not_report_blocks_nothing(self):
        model = model_of({})
        model.metadata = {}
        model.current = {}
        self.assertEqual(page_status.dictionary_line(model), "")
        self.assertEqual(page_status.keymap_line(model), "")
        self.assertEqual(page_status.retone_line(model), "")


class HintLabelTest(unittest.TestCase):
    def test_new_text_gets_the_height_it_needs(self):
        label = HintLabel("short")
        label.resize(120, 20)
        app.processEvents()
        label.setText("short")
        short = label.height()
        label.setText("a much longer line of text that has to wrap over a few lines here " * 3)
        self.assertGreater(label.height(), short)
        label.setText("short")
        self.assertEqual(label.height(), short)


class EditorPageTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)

    def dict_page(self, fake):
        page = DictEditorPage(fake)
        page.load_data()
        return page

    def test_each_page_says_why_it_is_not_in_use(self):
        off = {"EnableDictionary": "False", "EnableCustomKeymap": "False", "EnableMacro": "False"}
        fake = FakeDBusHandler(off)
        for page in (self.dict_page(fake), KeymapEditorPage(fake), MacroEditorPage(fake)):
            self.assertFalse(page.status_line.isHidden(), page)
            self.assertTrue(page.status_line.text(), page)

    def test_the_line_is_the_one_the_gtk_window_builds(self):
        fake = FakeDBusHandler({"EnableMacro": "False", "EnableCustomKeymap": "False"})
        model = page_status.read(fake)
        self.assertEqual(MacroEditorPage(fake).status_line.text(), page_status.macro_line(model))
        self.assertEqual(KeymapEditorPage(fake).status_line.text(), page_status.keymap_line(model))

    def test_the_page_switch_changes_the_line_at_once(self):
        page = MacroEditorPage(FakeDBusHandler({"EnableMacro": "True"}))
        self.assertTrue(page.status_line.isHidden())
        page.cb_enable.setChecked(False)
        self.assertFalse(page.status_line.isHidden())

    def test_the_switch_follows_a_change_from_the_tray(self):
        fake = FakeDBusHandler({"EnableMacro": "True", "EnableDictionary": "False"})
        macro, dictionary = MacroEditorPage(fake), self.dict_page(fake)
        fake.values.update(EnableMacro="False", CapitalizeMacro="False", EnableDictionary="True")
        macro.refresh_status()
        dictionary.refresh_status()
        self.assertFalse(macro.cb_enable.isChecked())
        self.assertFalse(macro.cb_capitalize.isChecked())
        self.assertFalse(macro.input_time_format.isEnabled())
        self.assertFalse(macro.is_modified())
        self.assertTrue(dictionary.cb_enable.isChecked())
        self.assertFalse(dictionary.is_modified())

    def test_a_click_after_following_writes_the_new_value(self):
        fake = FakeDBusHandler({"EnableCustomKeymap": "False"})
        page = KeymapEditorPage(fake)
        fake.values["EnableCustomKeymap"] = "True"
        page.refresh_status()
        page.cb_enable.setChecked(False)
        self.assertTrue(page.is_modified())
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["EnableCustomKeymap"], "False")

    def test_a_change_waiting_to_be_saved_is_not_overwritten(self):
        fake = FakeDBusHandler({"EnableCustomKeymap": "False"})
        page = KeymapEditorPage(fake)
        page.cb_enable.setChecked(True)
        fake.values["InputMethod"] = "Telex"
        page.refresh_status()
        self.assertTrue(page.cb_enable.isChecked())
        self.assertTrue(page.is_modified())

    def test_following_does_not_ask_the_window_to_save(self):
        fake = FakeDBusHandler({"EnableMacro": "True"})
        page = MacroEditorPage(fake)
        fake.values["EnableMacro"] = "False"
        with mock.patch.object(page, "_on_item_changed") as changed:
            page.refresh_status()
        changed.assert_not_called()

    def test_the_line_hides_without_fcitx5(self):
        fake = NoFcitx({"EnableMacro": "False"})
        page = MacroEditorPage(fake)
        self.assertFalse(page.status_line.isHidden())
        fake.down = True
        page.refresh_status()
        self.assertTrue(page.status_line.isHidden())


class TypingPageTest(unittest.TestCase):
    def page(self, fake):
        return DynamicSettingsPage(fake, category=SettingsCategory.TYPING)

    def test_retone_note_follows_the_charset(self):
        page = self.page(FakeDBusHandler({"RetoneSelection": "True", "OutputCharset": "Unicode"}))
        self.assertFalse(hint_shown(page, "RetoneNote"))
        combo = page.control_for("OutputCharset")
        combo.setCurrentIndex(combo.findData("VIQR"))
        self.assertTrue(hint_shown(page, "RetoneNote"))
        page.control_for("RetoneSelection").setChecked(False)
        self.assertFalse(hint_shown(page, "RetoneNote"))

    def test_the_notes_hide_without_fcitx5(self):
        fake = NoFcitx({"InputMethod": "Custom"})
        page = self.page(fake)
        self.assertTrue(hint_shown(page, "CustomKeymapHint"))
        fake.down = True
        page.refresh_status()
        self.assertFalse(hint_shown(page, "CustomKeymapHint"))

    def test_refreshing_keeps_the_page_widgets(self):
        page = self.page(FakeDBusHandler())
        control = page.control_for("SpellCheck")
        page.refresh_status()
        self.assertIs(page.control_for("SpellCheck"), control)


class WindowStatusTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)

    def window(self, fake):
        window = NgoSenSettingsWindow(dbus_handler=fake)
        window.show()
        app.processEvents()
        self.addCleanup(window.close)
        window._build_all_pages()
        return window

    @staticmethod
    def show_another_page_and_back(window):
        row = window.sidebar.currentRow()
        window.sidebar.setCurrentRow(row + 1)
        window.sidebar.setCurrentRow(row)

    def test_the_custom_hint_goes_once_keys_were_added_elsewhere(self):
        fake = FakeDBusHandler({"InputMethod": "Custom"})
        window = self.window(fake)
        page = window.show_page("typing")
        self.assertTrue(hint_shown(page, "CustomKeymapHint"))
        fake.values["EnableCustomKeymap"] = "True"
        fake.sub_configs["custom_keymap"] = [{"Key": "q", "Value": "XoaDauThanh"}]
        self.show_another_page_and_back(window)
        self.assertFalse(hint_shown(page, "CustomKeymapHint"))

    def test_changing_page_saves_first_so_the_next_page_reads_the_change(self):
        fake = FakeDBusHandler(
            {"AutoNonVnRestore": "True", "SpellCheck": "True", "EnableDictionary": "True"}
        )
        window = self.window(fake)
        typing = window.show_page("typing")
        typing.control_for("SpellCheck").setChecked(False)
        self.assertTrue(window._save_timer.isActive())
        window.show_page("macros")
        self.assertEqual(fake.values["SpellCheck"], "False")
        words = next(p for p in window._pages if isinstance(p, DictEditorPage))
        words.load_data()
        self.assertIn(page_status.read(fake).label("SpellCheck"), words.status_line.text())

    def test_coming_back_to_the_window_reads_the_tray_change(self):
        fake = FakeDBusHandler({"EnableMacro": "True"})
        window = self.window(fake)
        page = next(p for p in window._pages if isinstance(p, MacroEditorPage))
        fake.values["EnableMacro"] = "False"
        with mock.patch.object(window, "isActiveWindow", return_value=True):
            app.sendEvent(window, QEvent(QEvent.ActivationChange))
        self.assertFalse(page.cb_enable.isChecked())
        self.assertFalse(page.status_line.isHidden())

    def test_undo_and_the_defaults_refresh_the_lines(self):
        fake = FakeDBusHandler({"EnableMacro": "True"})
        window = self.window(fake)
        page = next(p for p in window._pages if isinstance(p, MacroEditorPage))
        page.cb_enable.setChecked(False)
        window.save_pending()
        self.assertFalse(page.status_line.isHidden())
        window.undo()
        self.assertTrue(page.status_line.isHidden())


if __name__ == "__main__":
    unittest.main()
