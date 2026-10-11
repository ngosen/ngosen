# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

from qtpy.QtWidgets import QTabWidget
from support import FakeDBusHandler, app
from ui.main_window import NgoSenSettingsWindow
from ui.search import fold


class FoldTest(unittest.TestCase):
    def test_drops_vietnamese_marks(self):
        self.assertEqual(fold("Kiểm tra CHÍNH TẢ"), "kiem tra chinh ta")
        self.assertEqual(fold("Đổi chế độ"), "doi che do")


class SearchTest(unittest.TestCase):
    def setUp(self):
        self.window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.window.show()
        app.processEvents()
        self.addCleanup(self.window.close)

    def results(self):
        box = self.window.search_results
        return [box.item(r).text() for r in range(box.count())]

    def open_result(self, starts_with):
        box = self.window.search_results
        row = next(r for r in range(box.count()) if box.item(r).text().startswith(starts_with))
        box.setCurrentRow(row)
        self.window.open_search_result(box.item(row))

    def test_finds_a_setting_and_names_its_page(self):
        self.window.search_field.setText("spell")
        self.assertIn("Enable Spell Check Using Dictionary — Typing", self.results())
        self.assertTrue(self.window.search_results.isVisible())
        self.assertFalse(self.window.sidebar.isVisible())

    def test_opening_a_result_shows_its_tab(self):
        self.window.search_field.setText("custom keymap")
        self.open_result("Custom Keymap")
        self.assertEqual(self.window.sidebar.currentItem().text(), "Keys")
        tabs = self.window.content_stack.currentWidget()
        self.assertIsInstance(tabs, QTabWidget)
        self.assertEqual(tabs.tabText(tabs.currentIndex()), "Keymap")
        self.assertEqual(self.window.search_field.text(), "")
        self.assertTrue(self.window.sidebar.isVisible())

    def test_no_match_says_so(self):
        self.window.search_field.setText("zzzz")
        self.assertEqual(self.results(), ["No settings found"])


if __name__ == "__main__":
    unittest.main()
