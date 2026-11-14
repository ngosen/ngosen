# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import sys
import unittest
from unittest import mock

from core.settings_model import SettingsModel
from qtpy.QtCore import QEvent
from qtpy.QtTest import QTest
from qtpy.QtWidgets import QApplication, QComboBox, QPushButton
from support import FakeDBusHandler, app  # noqa: F401
from ui import convert_box
from ui.convert_box import ConvertBox
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory

from core import convert


class ExplainTest(unittest.TestCase):
    def setUp(self):
        self.model = SettingsModel(FakeDBusHandler())
        self.model.load()

    def failure(self, name):
        fake = FakeDBusHandler()
        fake.convert_error = name
        return convert.convert(fake, self.model, "abc")

    def test_each_failure_says_what_to_do(self):
        lines = {
            None: "No desktop session bus",
            "org.freedesktop.DBus.Error.ServiceUnknown": "fcitx5 is not running",
            "org.freedesktop.DBus.Error.UnknownObject": "Ngó Sen is not loaded",
            convert.TOO_OLD: "Restart fcitx5",
        }
        for name, line in lines.items():
            with self.subTest(name=name):
                text, error = self.failure(name)
                self.assertIsNone(text)
                self.assertIn(line, error)

    def test_empty_text_needs_no_call(self):
        fake = FakeDBusHandler()
        self.assertEqual(convert.convert(fake, self.model, ""), ("", None))
        self.assertFalse(getattr(fake, "conversions", []))

    def test_settings_go_with_the_text(self):
        fake = FakeDBusHandler()
        self.model.set("ConvertCase", "Title Case")
        self.model.set("ConvertStripDiacritics", "True")
        convert.convert(fake, self.model, "abc")
        self.assertEqual(fake.conversions, [("abc", "TCVN3 (ABC)", "Unicode", 4, True)])


class ConvertPageTest(unittest.TestCase):
    def setUp(self):
        self.dbus = FakeDBusHandler()
        self.page = DynamicSettingsPage(self.dbus, category=SettingsCategory.CONVERT)
        self.page.resize(700, 900)
        self.page.show()
        app.processEvents()
        self.addCleanup(self.page.close)
        self.box = self.page.findChild(ConvertBox)

    def type(self, text):
        self.box.source.setPlainText(text)
        self.box.refresh()

    def test_pasted_text_shows_converted(self):
        self.type("TiÕng ViÖt")
        self.assertEqual(self.box.result.toPlainText(), "Tiếng Việt")
        self.assertTrue(self.box.copy.isEnabled())
        self.box.copy.click()
        self.assertEqual(QApplication.clipboard().text(), "Tiếng Việt")

    def test_page_rebuilt_right_after_copying(self):
        # An Undo rebuilds the page while the button still says Copied.
        self.type("abc")
        errors = []
        hook = mock.patch.object(sys, "excepthook", lambda *exc: errors.append(exc[1]))
        hook.start()
        self.addCleanup(hook.stop)
        with mock.patch.object(convert_box, "COPIED_MS", 10):
            self.box.copy.click()
        self.page.load_config()
        app.sendPostedEvents(None, QEvent.DeferredDelete)
        QTest.qWait(50)
        app.processEvents()
        self.assertEqual(errors, [])

    def test_a_changed_setting_converts_again(self):
        self.type("TiÕng")
        combo = self.page.control_for("ConvertCase")
        combo.setCurrentIndex(combo.findData("UPPER CASE"))
        self.assertEqual(self.box.result.toPlainText(), "TIẾNG")
        self.assertEqual(self.page.model.modified["ConvertCase"], "UPPER CASE")

    def test_swap_changes_both_charsets(self):
        button = next(b for b in self.page.findChildren(QPushButton) if "wap" in b.text())
        button.click()
        self.assertEqual(self.page.model.modified["ConvertFrom"], "Unicode")
        self.assertEqual(self.page.model.modified["ConvertTo"], "TCVN3 (ABC)")

    def test_failure_shows_why(self):
        self.dbus.convert_error = convert.TOO_OLD
        self.type("abc")
        self.assertFalse(self.box.error.isHidden())
        self.assertIn("Restart fcitx5", self.box.error.text())
        self.assertFalse(self.box.copy.isEnabled())

    def test_hotkey_and_charsets_are_on_the_page(self):
        for key in (
            "ConvertKey",
            "ConvertFrom",
            "ConvertTo",
            "ConvertCase",
            "ConvertStripDiacritics",
        ):
            with self.subTest(key=key):
                self.assertIsNotNone(self.page.control_for(key))
        self.assertIsInstance(self.page.control_for("ConvertFrom"), QComboBox)


if __name__ == "__main__":
    unittest.main()
