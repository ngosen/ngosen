# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

from qtpy.QtGui import QColor, QPalette
from qtpy.QtWidgets import QLabel, QWidget
from support import FakeDBusHandler, app
from ui import brand
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory


def dark_palette():
    palette = QPalette()
    for role, color in (
        (QPalette.Window, "#202326"),
        (QPalette.WindowText, "#fcfcfc"),
        (QPalette.Base, "#141618"),
        (QPalette.Text, "#fcfcfc"),
        (QPalette.Button, "#292c30"),
        (QPalette.ButtonText, "#fcfcfc"),
    ):
        palette.setColor(role, QColor(color))
    return palette


class ThemeSwitchTest(unittest.TestCase):
    """The desktop can switch between light and dark while the window is open."""

    def setUp(self):
        old = app.palette()
        self.addCleanup(lambda: app.setPalette(old))
        self.page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.TYPING)
        self.addCleanup(self.page.close)
        self.page.show()
        app.processEvents()
        app.setPalette(dark_palette())
        app.processEvents()

    def test_hints_stay_readable(self):
        hint = next(
            label
            for label in self.page.findChildren(QLabel)
            if label.text() == "Typing windows still gives windows."
        )
        self.assertGreater(hint.palette().color(QPalette.WindowText).lightness(), 128)

    def test_mode_buttons_take_the_dark_green(self):
        row = self.page.findChild(QWidget, "ModeButtons")
        self.assertIn(brand.ENAMEL[1], row.styleSheet())


if __name__ == "__main__":
    unittest.main()
