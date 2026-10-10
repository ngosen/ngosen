# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

from qtpy.QtWidgets import QLabel, QWidget
from support import FakeDBusHandler, app
from ui.main_window import NgoSenSettingsWindow
from ui.pages.about import AboutPage
from ui.pages.dynamic_settings import DynamicSettingsPage

# The only widgets DESIGN.md lets carry a stylesheet.
BRAND_WIDGETS = {"ModeButtons", "SaveMessage", "KeyCap"}


class NativeLookTest(unittest.TestCase):
    def setUp(self):
        self.window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.window._build_all_pages()
        self.window.show()
        app.processEvents()
        self.addCleanup(self.window.close)

    def test_only_brand_widgets_have_a_stylesheet(self):
        styled = {
            w.objectName() or type(w).__name__
            for w in [self.window, *self.window.findChildren(QWidget)]
            if w.styleSheet()
        }
        self.assertLessEqual(styled, BRAND_WIDGETS)

    def test_about_has_no_lotus_flower(self):
        about = self.window.findChild(AboutPage)
        self.assertFalse(any("🪷" in label.text() for label in about.findChildren(QLabel)))

    def test_a_page_in_a_tab_has_no_second_title(self):
        def titles(page):
            return [
                label
                for label in page.findChildren(QLabel, "CategoryTitle")
                if label.isVisibleTo(page)
            ]

        pages = self.window.findChildren(DynamicSettingsPage)
        typing = next(p for p in pages if p.category.name == "TYPING")
        shortcuts = next(p for p in pages if p.category.name == "SHORTCUTS")
        self.assertEqual(len(titles(typing)), 1)
        self.assertEqual(titles(shortcuts), [])


if __name__ == "__main__":
    unittest.main()
