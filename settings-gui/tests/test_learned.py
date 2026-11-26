# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

from core.ngosen_call import NO_REPLY, NOT_LOADED, NOT_RUNNING, TOO_OLD
from learned_sample import SAVED, write
from qtpy.QtWidgets import QPlainTextEdit, QPushButton
from support import FakeDBusHandler, app  # noqa: F401
from ui.pages import dynamic_settings
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory

from core import learned


class FileTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.mkdtemp()
        self.file = os.path.join(self.folder, "learned-diacritics")

    def test_path_is_in_ngosen_data(self):
        self.assertEqual(
            learned.path({"XDG_DATA_HOME": "/data", "HOME": "/home/a"}),
            "/data/ngosen/learned-diacritics",
        )
        self.assertEqual(
            learned.path({"HOME": "/home/a"}), "/home/a/.local/share/ngosen/learned-diacritics"
        )

    def test_phrases_skip_the_header_and_run_counts(self):
        found = learned.phrases(SAVED + "2\tx\tbad\tcount\n4\t1\ttoo\tlong\n")
        self.assertEqual(
            found,
            [(5, ["ngó", "sen"]), (3, ["ngó"]), (1, ["<s>", "ngó"]), (1, ["ngó", "sen", "</s>"])],
        )

    def test_a_file_with_only_its_header_holds_nothing(self):
        self.assertEqual(learned.phrases("timsen-personal 1\n"), [])

    def test_remove_files_takes_what_the_engine_leaves(self):
        names = [
            "learned-diacritics",
            ".learned-diacritics.tmp",
            "learned-diacritics.broken",
            "learned-diacritics.broken.2",
        ]
        kept = ["learned-diacritics.broken.x", "diacritics.timsen"]
        for name in names + kept:
            write(os.path.join(self.folder, name), "")
        learned.remove_files(self.file)
        self.assertEqual(sorted(os.listdir(self.folder)), sorted(kept))

    def test_remove_files_with_nothing_there(self):
        learned.remove_files(os.path.join(self.folder, "none", "learned-diacritics"))


class LearnedTest(unittest.TestCase):
    def setUp(self):
        self.dbus = FakeDBusHandler()
        self.file = os.path.join(tempfile.mkdtemp(), "ngosen", "learned-diacritics")
        write(self.file, SAVED)
        self.learned = learned.Learned(self.dbus, self.file)

    def answer(self, **answers):
        self.dbus.ngosen_answers = answers

    def test_forgetting_without_a_running_ngosen_removes_the_file(self):
        for name in NOT_RUNNING + NOT_LOADED:
            with self.subTest(name=name):
                write(self.file, SAVED)
                self.answer(ForgetLearned=name)
                forgot, line = self.learned.forget()
                self.assertTrue(forgot, line)
                self.assertFalse(os.path.exists(self.file))

    def test_forgetting_keeps_the_file_while_ngosen_may_hold_more(self):
        lines = {
            TOO_OLD: "Restart fcitx5",
            NO_REPLY: "may not have forgotten",
            None: "No desktop session bus",
            "org.freedesktop.DBus.Error.AccessDenied": "Could not forget",
        }
        for name, line in lines.items():
            with self.subTest(name=name):
                self.answer(ForgetLearned=name)
                forgot, said = self.learned.forget()
                self.assertFalse(forgot)
                self.assertIn(line, said)
                self.assertTrue(os.path.exists(self.file))

    def test_forgetting_is_left_to_the_running_ngosen(self):
        self.answer(ForgetLearned=(True, ""))
        self.assertEqual(self.learned.forget(), (True, "Forgot everything Đoán dấu learned."))
        # It removes the file itself, after dropping what it holds.
        self.assertTrue(os.path.exists(self.file))
        self.answer(ForgetLearned=(False, "No space left on device"))
        forgot, line = self.learned.forget()
        self.assertFalse(forgot)
        self.assertIn("No space left on device", line)

    def test_status_counts_after_saving(self):
        self.answer(DiacriticsStatus=("loaded", ""), SaveLearned=("saved", ""))
        self.assertEqual(self.learned.status(), ("Phrases learned: 4.", False, False))
        self.assertEqual(self.dbus.ngosen_calls, ["DiacriticsStatus", "SaveLearned"])

    def test_status_without_a_running_ngosen_reads_the_file(self):
        self.assertEqual(self.learned.status().line, "Phrases learned: 4.")
        os.remove(self.file)
        self.assertEqual(self.learned.status().line, "Nothing learned yet.")

    def test_status_says_what_went_wrong(self):
        self.answer(DiacriticsStatus=("loaded", "bad line 3"))
        status = self.learned.status()
        self.assertTrue(status.error)
        self.assertIn("bad line 3", status.line)
        self.assertIn("Forget all", status.line)
        self.answer(DiacriticsStatus=("loaded", ""), SaveLearned=("failed", "Permission denied"))
        status = self.learned.status()
        self.assertTrue(status.error)
        self.assertIn("Permission denied", status.line)

    def test_status_tells_a_missing_table(self):
        self.answer(DiacriticsStatus=("missing", ""))
        self.assertTrue(self.learned.status().table_missing)
        self.answer(DiacriticsStatus=("off", ""))
        self.assertFalse(self.learned.status().table_missing)

    def test_view_shows_phrases_most_seen_first(self):
        text, failed = self.learned.view()
        self.assertIsNone(failed)
        self.assertEqual(
            [line.split() for line in text.splitlines()],
            [
                ["5", "ngó", "sen"],
                ["3", "ngó"],
                ["1", "(start)", "ngó"],
                ["1", "ngó", "sen", "(end)"],
            ],
        )

    def test_only_a_ngosen_not_running_leaves_the_file_as_all_of_it(self):
        lines = {
            TOO_OLD: "Restart fcitx5",
            NO_REPLY: "did not answer",
            None: "No desktop session bus",
        }
        for name, line in lines.items():
            with self.subTest(name=name):
                self.answer(SaveLearned=name)
                status = self.learned.status()
                self.assertTrue(status.error)
                self.assertIn(line, status.line)
                text, failed = self.learned.view()
                self.assertIsNone(text)
                self.assertIn(line, failed)
        for name in NOT_RUNNING + NOT_LOADED:
            with self.subTest(name=name):
                self.answer(SaveLearned=name)
                self.assertEqual(self.learned.status().line, "Phrases learned: 4.")

    def test_view_without_anything_learned(self):
        os.remove(self.file)
        self.assertEqual(self.learned.view(), ("Nothing learned yet.", None))

    def test_view_opens_nothing_when_the_save_failed(self):
        self.answer(SaveLearned=("failed", "Read-only file system"))
        text, failed = self.learned.view()
        self.assertIsNone(text)
        self.assertIn("Read-only file system", failed)


class GuessingPageTest(unittest.TestCase):
    def setUp(self):
        self.dbus = FakeDBusHandler(values={"GuessDiacritics": "False"})
        self.file = learned.path()
        write(self.file, SAVED)
        self.addCleanup(learned.remove_files, self.file)
        self.page = DynamicSettingsPage(self.dbus, category=SettingsCategory.GUESSING)
        self.page.resize(700, 900)
        self.page.show()
        app.processEvents()
        self.addCleanup(self.page.close)
        self.messages = []
        self.page._page_message = lambda text, error=False: self.messages.append((text, error))

    def button(self, text):
        return next(b for b in self.page.findChildren(QPushButton) if b.text() == text)

    def test_buttons_work_with_guessing_off(self):
        self.assertFalse(self.page.control_for("LearnDiacritics").isEnabled())
        self.assertTrue(self.button("View").isEnabled())
        self.assertTrue(self.button("Forget all…").isEnabled())
        self.assertEqual(self.page._learned_line.text(), "Phrases learned: 4.")

    def test_view_shows_and_hides(self):
        view = self.page.findChild(QPlainTextEdit)
        self.assertTrue(view.isHidden())
        self.button("View").click()
        self.assertFalse(view.isHidden())
        self.assertTrue(view.isReadOnly())
        self.assertIn("ngó sen", view.toPlainText())
        self.button("Hide").click()
        self.assertTrue(view.isHidden())

    def test_forget_asks_first(self):
        with mock.patch.object(dynamic_settings, "confirm", return_value=False):
            self.button("Forget all…").click()
        self.assertTrue(os.path.exists(self.file))
        self.assertEqual(self.messages, [])
        with mock.patch.object(dynamic_settings, "confirm", return_value=True):
            self.button("Forget all…").click()
        self.assertFalse(os.path.exists(self.file))
        self.assertEqual(self.messages, [("Forgot everything Đoán dấu learned.", False)])
        self.assertEqual(self.page._learned_line.text(), "Nothing learned yet.")

    def test_a_failed_forget_is_an_error(self):
        self.dbus.ngosen_answers = {"ForgetLearned": TOO_OLD}
        with mock.patch.object(dynamic_settings, "confirm", return_value=True):
            self.button("Forget all…").click()
        self.assertTrue(os.path.exists(self.file))
        self.assertTrue(self.messages[0][1])

    def test_missing_table_note(self):
        self.assertTrue(self.page._table_note.isHidden())
        self.dbus.ngosen_answers = {"DiacriticsStatus": ("missing", "")}
        self.page.refresh_status()
        self.assertFalse(self.page._table_note.isHidden())


if __name__ == "__main__":
    unittest.main()
