# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest

from qtpy.QtGui import QColor, QIcon, QImage
from support import FakeDBusHandler, app  # noqa: F401
from ui.main_window import NgoSenSettingsWindow


class SidebarIconTest(unittest.TestCase):
    def setUp(self):
        # A theme like Yaru: it has preferences-system but not preferences-other.
        self.dir = tempfile.TemporaryDirectory()
        theme = os.path.join(self.dir.name, "plain")
        os.makedirs(os.path.join(theme, "16"))
        with open(os.path.join(theme, "index.theme"), "w") as f:
            f.write("[Icon Theme]\nName=plain\nDirectories=16\n\n[16]\nSize=16\nType=Fixed\n")
        # PNG, since not every Qt build has the SVG image plugin.
        image = QImage(16, 16, QImage.Format_ARGB32)
        image.fill(QColor("black"))
        image.save(os.path.join(theme, "16", "preferences-system.png"))
        self.old = (QIcon.themeSearchPaths(), QIcon.themeName())
        QIcon.setThemeSearchPaths([self.dir.name])
        QIcon.setThemeName("plain")
        self.addCleanup(self.restore)

    def restore(self):
        QIcon.setThemeSearchPaths(self.old[0])
        QIcon.setThemeName(self.old[1])
        self.dir.cleanup()

    def test_more_has_an_icon_without_preferences_other(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.addCleanup(window.close)
        more = window.sidebar.item(window.sidebar.count() - 1)
        self.assertFalse(more.icon().isNull())


if __name__ == "__main__":
    unittest.main()
