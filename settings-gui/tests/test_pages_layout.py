# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import tempfile
import unittest
from unittest import mock

from qtpy.QtWidgets import QPushButton
from support import FakeDBusHandler, app  # noqa: F401
from ui.main_window import NgoSenSettingsWindow
from ui.pages.keymap_editor import KeymapEditorPage


class LayoutTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = tmp.name
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_five_pages(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        titles = [window.sidebar.item(r).text() for r in range(window.sidebar.count())]
        self.assertEqual(titles, ["Typing", "Applications", "Macros & Dictionary", "Keys", "More"])

    def test_backup_buttons_sit_in_the_bottom_bar(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        texts = {b.text().replace("&", "") for b in window.findChildren(QPushButton)}
        self.assertTrue({"Defaults", "Back Up…", "Restore…"} <= texts)

    def test_change_on_a_second_tab_is_saved(self):
        fake = FakeDBusHandler({"EnableCustomKeymap": "False"})
        window = NgoSenSettingsWindow(dbus_handler=fake)
        window.sidebar.setCurrentRow(3)
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
