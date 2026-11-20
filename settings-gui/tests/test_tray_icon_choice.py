# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock
from xml.etree import ElementTree

# isort: off
from support import FakeDBusHandler, app  # noqa: F401
from core import tray_image
from core.catalog import locked_by
from core.settings_model import SettingsModel
from qtpy.QtGui import QColor, QImage
from qtpy.QtWidgets import QLabel, QPushButton
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory

# isort: on

SVG = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 8 8"><rect width="8" height="8"/></svg>'
WIDE_SVG = '<?xml version="1.0"?>\n<svg xmlns="http://www.w3.org/2000/svg" width="20" height="30"><rect/></svg>'


def png_header(width, height):
    """Enough of a PNG for its size to be read: signature and IHDR."""
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + ihdr + b"\0\0\0\0"


def svg_box(path):
    root = ElementTree.parse(path).getroot()
    box = tuple(float(v) for v in root.get("viewBox").split())
    if root.get("width") is not None:
        assert float(root.get("width")) == float(root.get("height")), (
            root.get("width"),
            root.get("height"),
        )
    return box


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

    def test_a_wide_svg_is_made_square(self):
        source, _data = picked(self, "flag.svg", WIDE_SVG)
        copy = Path(tray_image.install(source, "vi"))
        self.assertEqual(svg_box(copy), (-5.0, 0.0, 30.0, 30.0))

    def test_a_tall_png_is_centred_in_a_square_svg(self):
        source, _data = picked(self, "flag.png")
        source.write_bytes(png_header(20, 40))
        copy = Path(tray_image.install(source, "vi"))
        self.assertEqual(copy.suffix, ".svg")
        self.assertEqual(svg_box(copy), (0.0, 0.0, 40.0, 40.0))
        self.assertIn('x="10"', copy.read_text())

    def test_a_square_png_is_copied_as_it_is(self):
        source, data = picked(self, "flag.png")
        source.write_bytes(png_header(64, 64))
        copy = Path(tray_image.install(source, "vi"))
        self.assertEqual(copy.parent, data / "icons/hicolor/256x256/apps")
        self.assertEqual(copy.read_bytes(), png_header(64, 64))

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
        source, data = picked(self, "flag.png")
        # PNG, since not every Qt build has the SVG image plugin.
        image = QImage(8, 8, QImage.Format_ARGB32)
        image.fill(QColor("red"))
        image.save(str(source))
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
