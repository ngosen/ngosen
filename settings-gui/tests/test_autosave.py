# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

from qtpy.QtCore import QEvent, QEventLoop, QTimer
from qtpy.QtWidgets import QCheckBox, QLabel, QMessageBox, QPushButton
from support import FakeDBusHandler, app
from ui.main_window import NgoSenSettingsWindow

DD_FREE_STYLE = "Allow dd To Produce đ When Auto Restore Invalid Words Is On"


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
        fake = FakeDBusHandler({"DdFreeStyle": "True"})
        window = self.open_window(fake, "Typing")
        self.checkbox(window, DD_FREE_STYLE).click()
        window.save_pending()
        self.assertEqual(fake.values["DdFreeStyle"], "False")
        self.assertTrue(window.message_bar.isVisible())
        self.assertIn(DD_FREE_STYLE, window.message_label.text())
        self.assertTrue(window.btn_undo.isVisible())

    def test_change_is_saved_after_a_pause(self):
        fake = FakeDBusHandler({"DdFreeStyle": "True"})
        window = self.open_window(fake, "Typing")
        self.checkbox(window, DD_FREE_STYLE).click()
        self.assertEqual(fake.values["DdFreeStyle"], "True")
        loop = QEventLoop()
        QTimer.singleShot(1000, loop.quit)
        loop.exec()
        self.assertEqual(fake.values["DdFreeStyle"], "False")

    def test_undo_puts_the_setting_back(self):
        fake = FakeDBusHandler({"DdFreeStyle": "True"})
        window = self.open_window(fake, "Typing")
        self.checkbox(window, DD_FREE_STYLE).click()
        window.save_pending()
        window.btn_undo.click()
        self.assertEqual(fake.values["DdFreeStyle"], "True")
        self.assertTrue(self.checkbox(window, DD_FREE_STYLE).isChecked())
        self.assertFalse(window.btn_undo.isVisible())

    def test_undo_keeps_what_fcitx5_remembered_since_the_window_opened(self):
        fake = FakeDBusHandler({"DdFreeStyle": "True"})
        window = self.open_window(fake, "Typing")
        fake.sub_configs["app_rules"] = [{"App": "kate", "Mode": "3"}]
        self.checkbox(window, DD_FREE_STYLE).click()
        window.save_pending()
        window.btn_undo.click()
        self.assertEqual(fake.values["DdFreeStyle"], "True")
        self.assertEqual(fake.sub_configs["app_rules"], [{"App": "kate", "Mode": "3"}])

    def test_undo_after_restoring_a_backup_keeps_what_fcitx5_remembered(self):
        path = os.path.join(self.data_home, "b.json")
        fake = FakeDBusHandler()
        window = self.open_window(fake)
        window.export_backup(path)
        fake.sub_configs["app_rules"] = [{"App": "kate", "Mode": "3"}]
        window.restore_backup(path)
        self.assertEqual(fake.sub_configs["app_rules"], [])
        window.btn_undo.click()
        self.assertEqual(fake.sub_configs["app_rules"], [{"App": "kate", "Mode": "3"}])

    def test_undo_brings_back_rules_and_dictionary(self):
        dict_path = os.path.join(self.data_home, "fcitx5/ngosen/vietnamese.cm.dict")
        os.makedirs(os.path.dirname(dict_path))
        with open(dict_path, "w", encoding="utf-8") as f:
            f.write("an\nanh\n")
        switches = [{"App": "kate", "Vietnamese": "False"}]
        fake = FakeDBusHandler(
            sub_configs={
                "app_rules": [{"App": "firefox", "Mode": "5"}],
                "app_switches": switches,
            }
        )
        window = self.open_window(fake)
        with mock.patch("ui.main_window.confirm", return_value=True):
            window.on_restore_defaults()
        window.save_pending()
        self.assertEqual(fake.sub_configs["app_rules"], [])
        self.assertEqual(fake.sub_configs["app_switches"], [])
        window.btn_undo.click()
        self.assertEqual(fake.sub_configs["app_rules"], [{"App": "firefox", "Mode": "5"}])
        self.assertEqual(fake.sub_configs["app_switches"], switches)
        with open(dict_path, encoding="utf-8") as f:
            self.assertEqual(f.read(), "an\nanh\n")
        self.assertIn("ngosen", fake.reloads)

    def test_a_dictionary_that_is_not_utf8_opens_and_undo_keeps_it(self):
        dict_path = os.path.join(self.data_home, "fcitx5/ngosen/vietnamese.cm.dict")
        os.makedirs(os.path.dirname(dict_path))
        with open(dict_path, "wb") as f:
            f.write(b"an\n\xff\n")
        fake = FakeDBusHandler({"DdFreeStyle": "True"})
        with mock.patch.object(QMessageBox, "warning") as warning:
            window = self.open_window(fake, "Typing")
            app.processEvents()
            self.checkbox(window, DD_FREE_STYLE).click()
            window.save_pending()
            window.btn_undo.click()
        warning.assert_called()
        self.assertEqual(fake.values["DdFreeStyle"], "True")
        with open(dict_path, "rb") as f:
            self.assertEqual(f.read(), b"an\n\xff\n")

    def test_dictionary_tab_not_opened_is_left_alone(self):
        dict_path = os.path.join(self.data_home, "fcitx5/ngosen/vietnamese.cm.dict")
        os.makedirs(os.path.dirname(dict_path))
        with open(dict_path, "w", encoding="utf-8") as f:
            f.write("an\nanh\n")
        fake = FakeDBusHandler({"DdFreeStyle": "True", "EnableDictionary": "True"})
        # Its first tab shows; the dictionary tab behind it is built but never shown.
        window = self.open_window(fake, "Macros & Dictionary")
        window.sidebar.setCurrentRow(0)
        self.checkbox(window, DD_FREE_STYLE).click()
        window.save_pending()
        self.assertIn(DD_FREE_STYLE, window.message_label.text())
        self.assertEqual(fake.values["EnableDictionary"], "True")
        with open(dict_path, encoding="utf-8") as f:
            self.assertEqual(f.read(), "an\nanh\n")

    def test_failed_save_shows_what_fcitx5_kept(self):
        fake = FakeDBusHandler({"DdFreeStyle": "True"})
        fake.set_config = lambda values: False
        window = self.open_window(fake, "Typing")
        self.checkbox(window, DD_FREE_STYLE).click()
        window.save_pending()
        self.assertIn("fcitx5", window.message_label.text())
        self.assertTrue(self.checkbox(window, DD_FREE_STYLE).isChecked())

    def test_closing_saves_a_pending_change(self):
        fake = FakeDBusHandler({"DdFreeStyle": "True"})
        window = self.open_window(fake, "Typing")
        self.checkbox(window, DD_FREE_STYLE).click()
        window.close()
        self.assertEqual(fake.values["DdFreeStyle"], "False")

    def test_no_ok_apply_or_cancel(self):
        window = self.open_window(FakeDBusHandler())
        texts = {b.text().replace("&", "") for b in window.findChildren(QPushButton)}
        self.assertFalse(texts & {"OK", "Apply", "Cancel"})

    def test_fcitx5_not_running_is_shown(self):
        window = self.open_window(FcitxDown())
        self.assertTrue(window.message_bar.isVisible())
        self.assertIn("fcitx5", window.message_label.text())

    def test_fcitx5_not_running_is_said_once(self):
        window = self.open_window(FcitxDown())
        app.sendPostedEvents(None, QEvent.DeferredDelete)
        texts = [label.text() for label in window.findChildren(QLabel) if label.isVisible()]
        self.assertEqual(
            [t for t in texts if "fcitx5" in t or "configuration" in t],
            [window.message_label.text()],
        )


if __name__ == "__main__":
    unittest.main()
