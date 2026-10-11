# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

from qtpy.QtCore import QEvent
from qtpy.QtGui import QFontInfo
from qtpy.QtWidgets import QComboBox, QLabel, QLineEdit, QPushButton
from support import FakeDBusHandler, app
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory


def family(widget):
    return QFontInfo(widget.font()).family()


class TypingMarksTest(unittest.TestCase):
    def setUp(self):
        self.page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.TYPING)
        app.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_mode_is_a_row_of_buttons(self):
        buttons = {b.text(): b for b in self.page.findChildren(QPushButton) if b.isCheckable()}
        self.assertEqual(list(buttons), ["Sen", "Preedit", "Emoji Picker", "OFF"])
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

    def test_typed_text_uses_xanh_mono(self):
        example = self.page.findChild(QLabel, "TypingExample")
        try_it = self.page.findChildren(QLineEdit)[-1]
        self.assertEqual(family(example), "Xanh Mono")
        self.assertEqual(family(try_it), "Xanh Mono")


class SidebarMarksTest(unittest.TestCase):
    def test_wordmark_in_xanh_mono(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        wordmark = window.findChild(QLabel, "Wordmark")
        self.assertEqual(wordmark.text(), "Ngó Sen")
        self.assertEqual(family(wordmark), "Xanh Mono")
        self.assertFalse(wordmark.font().bold())


if __name__ == "__main__":
    unittest.main()
