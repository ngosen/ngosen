# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

# isort: off
from support import app
from qtpy.QtGui import QColor, QPalette
from ui import brand

# isort: on


def highlight():
    return app.palette().color(QPalette.Active, QPalette.Highlight).name()


class AccentTest(unittest.TestCase):
    def setUp(self):
        original = app.palette()
        self.addCleanup(app.setPalette, original)
        follower = brand.install_accent(app)
        self.addCleanup(app.removeEventFilter, follower)

    def test_the_accent_is_enamel(self):
        self.assertEqual(highlight(), brand.ENAMEL[0])
        self.assertEqual(
            app.palette().color(QPalette.Active, QPalette.HighlightedText).name(),
            brand.ON_ENAMEL,
        )

    def test_a_switch_to_dark_keeps_enamel(self):
        # The desktop sends a whole new palette, with its own accent, when it turns dark.
        dark = QPalette(QColor("#292c30"), QColor("#202326"))
        dark.setColor(QPalette.Highlight, QColor("#3daee9"))
        app.setPalette(dark)
        app.processEvents()
        self.assertEqual(highlight(), brand.ENAMEL[1])


if __name__ == "__main__":
    unittest.main()
