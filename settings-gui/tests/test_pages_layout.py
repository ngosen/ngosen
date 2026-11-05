# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import tempfile
import unittest
from unittest import mock

from qtpy.QtWidgets import QLabel, QPushButton
from support import FakeDBusHandler, app  # noqa: F401
from ui.main_window import NgoSenSettingsWindow
from ui.pages.about import AboutPage
from ui.pages.keymap_editor import KeymapEditorPage


class LayoutTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = tmp.name
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_seven_pages(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        titles = [window.sidebar.item(r).text() for r in range(window.sidebar.count())]
        self.assertEqual(
            titles,
            [
                "Typing",
                "Macros & Dictionary",
                "Keys",
                "Appearance",
                "What's new & tips",
                "Check my machine",
                "About",
            ],
        )

    def test_about_description_takes_the_page_width(self):
        page = AboutPage()
        page.resize(700, 700)
        page.show()
        app.processEvents()
        self.addCleanup(page.close)
        desc = page.findChild(QLabel, "AboutDescription")
        # The page margins are 40px a side; a squeezed label gets only its text hint width.
        self.assertGreater(desc.width(), page.width() - 120)

    def test_backup_buttons_sit_in_the_bottom_bar(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        texts = {b.text().replace("&", "") for b in window.findChildren(QPushButton)}
        self.assertTrue({"Restore defaults…", "Back Up…", "Restore…"} <= texts)

    def test_the_bottom_bar_shows_only_on_settings_pages(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.addCleanup(window.close)
        window.show()
        for name, shown in (
            ("typing", True),
            ("macros", True),
            ("whats-new", False),
            ("check", False),
            ("about", False),
        ):
            window.show_page(name)
            self.assertEqual(window.bottom_bar.isVisible(), shown, name)

    def test_opening_straight_on_a_page_hides_the_bottom_bar(self):
        for name in ("check", "whats-new"):
            window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
            self.addCleanup(window.close)
            window.show_page(name)
            window.show()
            app.processEvents()
            self.assertFalse(window.bottom_bar.isVisible(), name)

    def test_a_message_follows_the_page_bottom_when_the_bar_comes_back(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.addCleanup(window.close)
        window.resize(750, 500)
        window.show()
        window.show_page("check")
        app.processEvents()
        window._show_message("Saved.", keep=True)
        app.processEvents()
        window.show_page("typing")
        app.processEvents()
        message = window.message_bar.geometry()
        self.assertLessEqual(message.bottom(), window.content_stack.geometry().bottom())
        self.assertLess(window.content_stack.geometry().bottom() - message.bottom(), 20)
        self.assertFalse(message.intersects(window.bottom_bar.geometry()))

    def test_the_backup_tip_does_not_depend_on_the_page_it_shows_on(self):
        po = os.path.join(os.path.dirname(__file__), "..", "..", "po", "vi.po")
        with open(po, encoding="utf-8") as f:
            text = f.read()
        self.assertIn('msgstr "“Sao lưu…” và “Khôi phục…” ở đáy các trang cài đặt', text)
        self.assertIn('msgid "Restore &defaults…"\nmsgstr "Đưa về &mặc định…"', text)
        self.assertNotIn("and “Restore…” below", text)

    def test_change_on_a_second_tab_is_saved(self):
        fake = FakeDBusHandler({"EnableCustomKeymap": "False"})
        window = NgoSenSettingsWindow(dbus_handler=fake)
        window.sidebar.setCurrentRow(
            next(
                r for r in range(window.sidebar.count()) if window.sidebar.item(r).text() == "Keys"
            )
        )
        keymap = window.findChild(KeymapEditorPage)
        keymap.cb_enable.click()
        window.save_pending()
        self.assertEqual(fake.values["EnableCustomKeymap"], "True")

    def test_restore_can_be_undone(self):
        fake = FakeDBusHandler(
            {"SpellCheck": "True"}, {"ngosen-macro": [{"Key": "kg", "Value": "khô gà"}]}
        )
        window = NgoSenSettingsWindow(dbus_handler=fake)
        path = os.path.join(self.tmp, "backup.json")
        window.export_backup(path)
        with open(path, encoding="utf-8") as f:
            self.assertEqual(json.load(f)["macros"], [{"Key": "kg", "Value": "khô gà"}])

        fake.values["SpellCheck"] = "False"
        fake.sub_configs["ngosen-macro"] = []
        window.reload_pages()
        window.restore_backup(path)
        self.assertEqual(fake.values["SpellCheck"], "True")
        self.assertEqual(fake.sub_configs["ngosen-macro"], [{"Key": "kg", "Value": "khô gà"}])
        self.assertIn("ngosen", fake.reloads)

        window.btn_undo.click()
        self.assertEqual(fake.values["SpellCheck"], "False")
        self.assertEqual(fake.sub_configs["ngosen-macro"], [])


if __name__ == "__main__":
    unittest.main()
