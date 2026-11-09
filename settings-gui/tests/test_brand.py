# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

from qtpy.QtCore import QEvent
from qtpy.QtWidgets import QApplication, QComboBox, QLabel, QLineEdit, QPushButton
from support import FakeDBusHandler, app
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory


def uses_system_font(widget):
    return widget.font().family() == QApplication.font().family()


class TypingMarksTest(unittest.TestCase):
    def setUp(self):
        self.page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.TYPING)
        app.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_mode_is_a_row_of_buttons(self):
        buttons = {b.text(): b for b in self.page.findChildren(QPushButton) if b.isCheckable()}
        self.assertEqual(list(buttons), ["Sen", "Preedit", "Emoji Picker"])
        self.assertTrue(buttons["Sen"].isChecked())
        buttons["Preedit"].click()
        self.assertEqual(self.page.current_values["Mode"], "Preedit")
        self.assertFalse(buttons["Sen"].isChecked())

    def test_example_follows_the_input_method(self):
        example = self.page.findChild(QLabel, "TypingExample")
        self.assertEqual(example.text(), "vieetj → việt")
        combo = next(c for c in self.page.findChildren(QComboBox) if c.findData("VNI") >= 0)
        combo.setCurrentIndex(combo.findData("VNI"))
        self.assertEqual(example.text(), "vie65t → việt")

    def custom_hint(self, page):
        hint = page.findChild(QLabel, "CustomKeymapHint")
        return hint is not None and not hint.isHidden()

    def test_custom_without_keymap_says_where_to_set_keys(self):
        page = DynamicSettingsPage(
            FakeDBusHandler({"InputMethod": "Custom"}), category=SettingsCategory.TYPING
        )
        self.assertTrue(self.custom_hint(page))

    def test_custom_hint_follows_the_input_method(self):
        self.assertFalse(self.custom_hint(self.page))
        combo = next(c for c in self.page.findChildren(QComboBox) if c.findData("Custom") >= 0)
        combo.setCurrentIndex(combo.findData("Custom"))
        self.assertTrue(self.custom_hint(self.page))

    def test_custom_with_a_keymap_has_no_hint(self):
        dbus = FakeDBusHandler(
            {"InputMethod": "Custom", "EnableCustomKeymap": "True"},
            {"custom_keymap": [{"Key": "q", "Value": "XoaDauThanh"}]},
        )
        page = DynamicSettingsPage(dbus, category=SettingsCategory.TYPING)
        self.assertFalse(self.custom_hint(page))

    def test_typed_text_uses_the_system_font(self):
        example = self.page.findChild(QLabel, "TypingExample")
        try_it = self.page.findChildren(QLineEdit)[-1]
        self.assertTrue(uses_system_font(example))
        self.assertTrue(uses_system_font(try_it))
        self.assertEqual(example.font().pixelSize(), 16)
        self.assertEqual(try_it.font().pixelSize(), 17)


class SidebarMarksTest(unittest.TestCase):
    def test_wordmark_in_the_system_font(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        wordmark = window.findChild(QLabel, "Wordmark")
        self.assertEqual(wordmark.text(), "Ngó Sen")
        self.assertTrue(uses_system_font(wordmark))
        self.assertEqual(wordmark.font().pixelSize(), 22)
        self.assertFalse(wordmark.font().bold())

    def test_the_title_bar_carries_the_only_logo(self):
        # Window managers draw the app icon in the title bar right above the wordmark.
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        column = window.findChild(QLabel, "Wordmark").parentWidget()
        logos = [label for label in column.findChildren(QLabel) if not label.pixmap().isNull()]
        self.assertEqual(logos, [])


if __name__ == "__main__":
    unittest.main()
