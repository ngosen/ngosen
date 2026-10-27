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

    def test_six_pages(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        titles = [window.sidebar.item(r).text() for r in range(window.sidebar.count())]
        self.assertEqual(
            titles,
            ["Typing", "Macros & Dictionary", "Keys", "Appearance", "What's new & tips", "About"],
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

    def test_about_page_says_when_fcitx5_runs_the_old_addon(self):
        for replaced in (False, True):
            with mock.patch("core.system_check.runs_replaced_addon", return_value=replaced):
                page = AboutPage()
            self.addCleanup(page.close)
            notice = page.findChild(QLabel, "AddonReplaced")
            self.assertEqual(notice is not None, replaced)

    def test_backup_buttons_sit_in_the_bottom_bar(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        texts = {b.text().replace("&", "") for b in window.findChildren(QPushButton)}
        self.assertTrue({"Defaults", "Back Up…", "Restore…"} <= texts)

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
