# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

from qtpy.QtCore import QEvent, QEventLoop, QTimer
from qtpy.QtWidgets import QCheckBox, QMessageBox, QPushButton
from support import FakeDBusHandler, app
from ui.main_window import NgoSenSettingsWindow

SPELL_CHECK = "Enable Spell Check Using Dictionary"


class FcitxDown(FakeDBusHandler):
    def __init__(self):
        super().__init__()
        self.iface = None

    def get_config(self):
        return {}


class AutosaveTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.data_home = tmp.name
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)

    def open_window(self, fake, page_title=None):
        window = NgoSenSettingsWindow(dbus_handler=fake)
        if page_title:
            row = next(
                r
                for r in range(window.sidebar.count())
                if window.sidebar.item(r).text() == page_title
            )
            window.sidebar.setCurrentRow(row)
        window.show()
        app.processEvents()
        self.addCleanup(window.close)
        return window

    def checkbox(self, window, text):
        app.sendPostedEvents(None, QEvent.DeferredDelete)
        return next(cb for cb in window.findChildren(QCheckBox) if cb.text() == text)

    def test_change_is_saved_without_a_button(self):
        fake = FakeDBusHandler({"SpellCheck": "True"})
        window = self.open_window(fake, "Typing")
        self.checkbox(window, SPELL_CHECK).click()
        window.save_pending()
        self.assertEqual(fake.values["SpellCheck"], "False")
        self.assertTrue(window.message_bar.isVisible())
        self.assertIn(SPELL_CHECK, window.message_label.text())
        self.assertTrue(window.btn_undo.isVisible())

    def test_change_is_saved_after_a_pause(self):
        fake = FakeDBusHandler({"SpellCheck": "True"})
        window = self.open_window(fake, "Typing")
        self.checkbox(window, SPELL_CHECK).click()
        self.assertEqual(fake.values["SpellCheck"], "True")
        loop = QEventLoop()
        QTimer.singleShot(1000, loop.quit)
        loop.exec()
        self.assertEqual(fake.values["SpellCheck"], "False")

    def test_undo_puts_the_setting_back(self):
        fake = FakeDBusHandler({"SpellCheck": "True"})
        window = self.open_window(fake, "Typing")
        self.checkbox(window, SPELL_CHECK).click()
        window.save_pending()
        window.btn_undo.click()
        self.assertEqual(fake.values["SpellCheck"], "True")
        self.assertTrue(self.checkbox(window, SPELL_CHECK).isChecked())
        self.assertFalse(window.btn_undo.isVisible())

    def test_undo_brings_back_rules_and_dictionary(self):
        dict_path = os.path.join(self.data_home, "fcitx5/ngosen/vietnamese.cm.dict")
        os.makedirs(os.path.dirname(dict_path))
        with open(dict_path, "w", encoding="utf-8") as f:
            f.write("an\nanh\n")
        fake = FakeDBusHandler(sub_configs={"app_rules": [{"App": "firefox", "Mode": "5"}]})
        window = self.open_window(fake)
        with mock.patch.object(QMessageBox, "question", return_value=QMessageBox.Yes):
            window.on_restore_defaults()
        window.save_pending()
        self.assertEqual(fake.sub_configs["app_rules"], [])
        window.btn_undo.click()
        self.assertEqual(fake.sub_configs["app_rules"], [{"App": "firefox", "Mode": "5"}])
        with open(dict_path, encoding="utf-8") as f:
            self.assertEqual(f.read(), "an\nanh\n")
        self.assertIn("ngosen", fake.reloads)

    def test_dictionary_tab_not_opened_is_left_alone(self):
        dict_path = os.path.join(self.data_home, "fcitx5/ngosen/vietnamese.cm.dict")
        os.makedirs(os.path.dirname(dict_path))
        with open(dict_path, "w", encoding="utf-8") as f:
            f.write("an\nanh\n")
        fake = FakeDBusHandler({"SpellCheck": "True", "EnableDictionary": "True"})
        # Its first tab shows; the dictionary tab behind it is built but never shown.
        window = self.open_window(fake, "Macros & Dictionary")
        window.sidebar.setCurrentRow(0)
        self.checkbox(window, SPELL_CHECK).click()
        window.save_pending()
        self.assertIn(SPELL_CHECK, window.message_label.text())
        self.assertEqual(fake.values["EnableDictionary"], "True")
        with open(dict_path, encoding="utf-8") as f:
            self.assertEqual(f.read(), "an\nanh\n")

    def test_closing_saves_a_pending_change(self):
        fake = FakeDBusHandler({"SpellCheck": "True"})
        window = self.open_window(fake, "Typing")
        self.checkbox(window, SPELL_CHECK).click()
        window.close()
        self.assertEqual(fake.values["SpellCheck"], "False")

    def test_no_ok_apply_or_cancel(self):
        window = self.open_window(FakeDBusHandler())
        texts = {b.text().replace("&", "") for b in window.findChildren(QPushButton)}
        self.assertFalse(texts & {"OK", "Apply", "Cancel"})

    def test_fcitx5_not_running_is_shown(self):
        window = self.open_window(FcitxDown())
        self.assertTrue(window.message_bar.isVisible())
        self.assertIn("fcitx5", window.message_label.text())


if __name__ == "__main__":
    unittest.main()
