# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

# isort: off
from support import FakeDBusHandler, app  # noqa: F401
from core import tray_image
from core.catalog import locked_by
from core.settings_model import SettingsModel
from qtpy.QtWidgets import QLabel, QPushButton
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory

# isort: on

SVG = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 8 8"><rect width="8" height="8"/></svg>'


def picked(test, name="flag.svg", content=SVG):
    """A file the user picks, and an empty XDG_DATA_HOME for the copies."""
    tmp = tempfile.TemporaryDirectory()
    test.addCleanup(tmp.cleanup)
    patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": os.path.join(tmp.name, "data")})
    patcher.start()
    test.addCleanup(patcher.stop)
    source = Path(tmp.name) / name
    source.write_text(content)
    return source, Path(tmp.name) / "data"


class TrayLocksTest(unittest.TestCase):
    def test_images_count_only_with_a_custom_image(self):
        self.assertEqual(
            locked_by("TrayImageVietnamese", {"TrayIconKind": "V / E"}.get), "TrayIconKind"
        )
        self.assertIsNone(locked_by("TrayImageEnglish", {"TrayIconKind": "Custom image"}.get))

    def test_letters_take_the_icon_colour(self):
        self.assertIsNone(locked_by("IconTheme", {"TrayIconKind": "vi / en"}.get))
        self.assertEqual(locked_by("TrayIconSize", {"TrayIconKind": "vi / en"}.get), "TrayIconKind")


class TrayImageInstallTest(unittest.TestCase):
    def test_svg_goes_to_the_scalable_icons(self):
        source, data = picked(self)
        copy = Path(tray_image.install(source, "vi"))
        self.assertEqual(copy.parent, data / "icons/hicolor/scalable/apps")
        self.assertTrue(copy.name.startswith("ngosen-tray-vi-"))
        self.assertEqual(copy.read_text(), SVG)

    def test_new_content_gets_a_new_name_and_replaces_the_old_copy(self):
        source, _data = picked(self)
        first = tray_image.install(source, "vi")
        source.write_text(SVG.replace("rect", "circle"))
        second = tray_image.install(source, "vi")
        self.assertNotEqual(first, second)
        self.assertFalse(os.path.exists(first))

    def test_the_saved_copy_stays_until_saving(self):
        source, _data = picked(self)
        saved = tray_image.install(source, "vi")
        source.write_text(SVG.replace("rect", "circle"))
        tray_image.install(source, "vi", keep={saved})
        self.assertTrue(os.path.exists(saved))

    def test_the_other_language_is_left_alone(self):
        source, _data = picked(self)
        english = tray_image.install(source, "en")
        tray_image.install(source, "vi")
        self.assertTrue(os.path.exists(english))

    def test_formats_trays_cannot_show_are_refused(self):
        source, _data = picked(self, "photo.jpg", "jpeg")
        with self.assertRaises(ValueError):
            tray_image.install(source, "vi")


class TrayPreviewTest(unittest.TestCase):
    def setUp(self):
        self.model = SettingsModel(FakeDBusHandler())
        self.assertTrue(self.model.load())

    def test_letters_follow_the_icon_colour(self):
        self.model.set("TrayIconKind", "vi / en")
        self.model.set("IconTheme", "Light")
        self.assertEqual(
            self.model.tray_icons(True, "KDE"),
            (("fcitx-ngosen-text-vi-default-black", "fcitx-ngosen-text-en-default-black"), False),
        )
        self.model.set("TrayIconKind", "V / E")
        self.assertEqual(
            self.model.tray_icons(True, "KDE")[0],
            ("fcitx-ngosen-default-black", "fcitx-ngosen-off-default-black"),
        )

    def test_a_language_without_its_image_keeps_the_icon(self):
        source, _data = picked(self)
        self.model.set("TrayIconKind", "Custom image")
        self.model.set("TrayImageVietnamese", str(source))
        self.model.set("TrayImageEnglish", str(source) + ".gone")
        self.assertEqual(self.model.tray_icons(False, "KDE")[0], (str(source), "fcitx-ngosen-off"))


class QtTrayImageTest(unittest.TestCase):
    def setUp(self):
        self.page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.APPEARANCE)

    def test_picking_an_image_shows_it_in_the_tray_preview(self):
        source, data = picked(self)
        self.page.update_config("TrayIconKind", "Custom image")
        self.page.use_tray_image("TrayImageEnglish", str(source))
        copy = self.page.model.value_of("TrayImageEnglish")
        self.assertTrue(copy.startswith(str(data)))
        self.assertEqual(self.page._tray_icon_names()[0], ("fcitx-ngosen", copy))
        self.assertFalse(self.page.findChildren(QLabel, "TrayImage")[1].pixmap().isNull())

    def test_remove_brings_back_the_icon(self):
        source, _data = picked(self)
        self.page.update_config("TrayIconKind", "Custom image")
        self.page.use_tray_image("TrayImageVietnamese", str(source))
        remove = [b for b in self.page.findChildren(QPushButton) if b.text() == "Remove"][0]
        remove.click()
        self.assertEqual(self.page.model.value_of("TrayImageVietnamese"), "")
        self.assertEqual(self.page._tray_icon_names()[0][0], "fcitx-ngosen")

    def test_screen_readers_tell_the_image_buttons_apart(self):
        names = [b.accessibleName() for b in self.page.findChildren(QPushButton)]
        image_buttons = [n for n in names if "tray image" in n]
        self.assertEqual(len(image_buttons), 4)
        self.assertEqual(len(set(image_buttons)), 4)

    def test_image_buttons_wait_for_a_custom_image(self):
        choose = self.page.control_for("TrayImageVietnamese")
        self.assertFalse(choose.isEnabled())
        self.page.update_config("TrayIconKind", "Custom image")
        self.assertTrue(choose.isEnabled())


if __name__ == "__main__":
    unittest.main()
