# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest

from qtpy.QtGui import QColor, QIcon, QImage, QImageReader
from qtpy.QtWidgets import QCheckBox
from support import FakeDBusHandler, app  # noqa: F401
from ui import brand
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory


class FakeThemeTest(unittest.TestCase):
    """Switches Qt to a one-off icon theme holding only the given icons."""

    icons = ()

    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        theme = os.path.join(self.dir.name, "plain")
        os.makedirs(os.path.join(theme, "16"))
        with open(os.path.join(theme, "index.theme"), "w") as f:
            f.write("[Icon Theme]\nName=plain\nDirectories=16\n\n[16]\nSize=16\nType=Fixed\n")
        # PNG, since not every Qt build has the SVG image plugin.
        image = QImage(16, 16, QImage.Format_ARGB32)
        image.fill(QColor("black"))
        for name in self.icons:
            image.save(os.path.join(theme, "16", name + ".png"))
        self.old = (QIcon.themeSearchPaths(), QIcon.themeName())
        QIcon.setThemeSearchPaths([self.dir.name])
        QIcon.setThemeName("plain")
        self.addCleanup(self.restore)

    def restore(self):
        QIcon.setThemeSearchPaths(self.old[0])
        QIcon.setThemeName(self.old[1])
        self.dir.cleanup()


class SidebarIconTest(FakeThemeTest):
    # Like Adwaita, which ships only symbolic icons, and Yaru, which has preferences-system.
    icons = ("preferences-system", "help-about-symbolic")

    def test_appearance_and_about_have_icons(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.addCleanup(window.close)
        items = {
            window.sidebar.item(r).text(): window.sidebar.item(r)
            for r in range(window.sidebar.count())
        }
        self.assertFalse(items["Appearance"].icon().isNull())
        self.assertFalse(items["About"].icon().isNull())


class MenuEntryIconTest(FakeThemeTest):
    # Adwaita has only the symbolic variant of most menu icons.
    icons = ("character-set-symbolic",)

    def test_menu_entry_shows_its_icon(self):
        page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.APPEARANCE)
        self.addCleanup(page.close)
        charset = next(c for c in page.findChildren(QCheckBox) if c.text() == "Charset")
        # A missing icon leaves a blank placeholder, so check the name rather than isNull().
        self.assertEqual(charset.icon().name(), "character-set-symbolic")


class MissingMenuIconTest(FakeThemeTest):
    # Adwaita and Yaru have no character-set icon; hicolor holds the one Ngó Sen installs.
    icons = ("ngosen-charset",)

    def test_menu_entry_falls_back_to_own_icon(self):
        page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.APPEARANCE)
        self.addCleanup(page.close)
        charset = next(c for c in page.findChildren(QCheckBox) if c.text() == "Charset")
        self.assertEqual(charset.icon().name(), "ngosen-charset")


class LogoTest(FakeThemeTest):
    # A theme like Papirus or Colloid: it has an fcitx icon but no fcitx-ngosen.
    icons = ("fcitx",)

    def setUp(self):
        super().setUp()
        apps = os.path.join(self.dir.name, "data", "icons", "hicolor", "scalable", "apps")
        os.makedirs(apps)
        with open(os.path.join(apps, f"{brand.LOGO}.svg"), "w") as f:
            f.write(
                '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16"><rect width="16" height="16"/></svg>'
            )
        old = os.environ.get("XDG_DATA_DIRS")
        os.environ["XDG_DATA_DIRS"] = os.path.join(self.dir.name, "data")
        self.addCleanup(
            lambda: (
                os.environ.pop("XDG_DATA_DIRS")
                if old is None
                else os.environ.__setitem__("XDG_DATA_DIRS", old)
            )
        )

    @unittest.skipUnless(b"svg" in QImageReader.supportedImageFormats(), "no SVG image plugin")
    def test_does_not_borrow_the_fcitx_icon(self):
        self.assertEqual(QIcon.fromTheme("fcitx-ngosen").name(), "fcitx")
        logo = brand.logo_icon()
        self.assertFalse(logo.isNull())
        self.assertNotEqual(logo.name(), "fcitx")


if __name__ == "__main__":
    unittest.main()
