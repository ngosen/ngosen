# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

# isort: off
from support import app
from qtpy.QtGui import QColor, QPalette
from ui import brand

# isort: on


GROUPS = (QPalette.Active, QPalette.Inactive)


def colors(role):
    return {app.palette().color(group, role).name() for group in GROUPS}


class AccentTest(unittest.TestCase):
    def setUp(self):
        original = app.palette()
        self.addCleanup(app.setPalette, original)
        follower = brand.install_accent(app)
        self.addCleanup(app.removeEventFilter, follower)

    def test_the_accent_is_enamel(self):
        self.assertEqual(colors(QPalette.Highlight), {brand.ENAMEL[0]})
        self.assertEqual(colors(QPalette.HighlightedText), {brand.ON_ENAMEL})

    def test_a_switch_to_dark_keeps_enamel(self):
        # The desktop sends a whole new palette, with its own accent, when it turns dark.
        dark = QPalette(QColor("#292c30"), QColor("#202326"))
        dark.setColor(QPalette.Highlight, QColor("#3daee9"))
        app.setPalette(dark)
        app.processEvents()
        self.assertEqual(colors(QPalette.Highlight), {brand.ENAMEL[1]})
        self.assertEqual(colors(QPalette.HighlightedText), {brand.ON_ENAMEL})

    def test_a_disabled_selection_is_a_paler_enamel(self):
        disabled = app.palette().color(QPalette.Disabled, QPalette.Highlight)
        self.assertEqual(disabled.name(), brand.ENAMEL[0])
        self.assertLess(disabled.alpha(), 255)


if __name__ == "__main__":
    unittest.main()
