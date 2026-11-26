# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import unittest

# isort: off
import window_display
from fake_fcitx import FakeDBusHandler
import ui_gtk

# isort: on
from core.ngosen_call import TOO_OLD
from learned_sample import SAVED, write

from core import learned

CAN_RUN = ui_gtk.available() and window_display.can_show()

if CAN_RUN:
    from gi.repository import Adw
    from test_gtk_emoji import _settle
    from test_gtk_window import isolate
    from ui_gtk import window


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and NGOSEN_WINDOW_TESTS")
class GtkLearnedTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.file = learned.path()
        write(self.file, SAVED)
        self.addCleanup(learned.remove_files, self.file)
        self.dbus = FakeDBusHandler(values={"GuessDiacritics": "False"})
        self.window = window.Window(None, self.dbus)
        self.addCleanup(self.window.destroy)
        self.window.present()
        _settle()
        self.page = self.window.guessing
        self.messages = []
        self.page.show_message = lambda text, undo: self.messages.append(text)

    def test_buttons_work_with_guessing_off(self):
        self.assertFalse(self.page.rows["LearnDiacritics"].get_sensitive())
        self.assertTrue(self.page.learned_row.get_sensitive())
        self.assertEqual(self.page.learned_row.get_subtitle(), "Phrases learned: 4.")

    def test_view_shows_and_hides(self):
        self.assertFalse(self.page.learned_view.get_visible())
        self.page.view_button.emit("clicked")
        self.assertTrue(self.page.learned_view.get_visible())
        self.assertFalse(self.page.learned_text.get_editable())
        buffer = self.page.learned_text.get_buffer()
        text = buffer.get_text(buffer.get_start_iter(), buffer.get_end_iter(), False)
        self.assertIn("ngó sen", text)
        self.assertEqual(self.page.view_button.get_label(), "Hide")
        self.page.view_button.emit("clicked")
        self.assertFalse(self.page.learned_view.get_visible())

    def test_forget_asks_first(self):
        dialog = self.page.confirm_forget()
        dialog.emit("response", "cancel")
        self.assertTrue(os.path.exists(self.file))
        dialog.force_close()
        dialog = self.page.confirm_forget()
        self.assertEqual(
            dialog.get_response_appearance("forget"), Adw.ResponseAppearance.DESTRUCTIVE
        )
        dialog.emit("response", "forget")
        dialog.force_close()
        self.assertFalse(os.path.exists(self.file))
        self.assertEqual(self.messages, ["Forgot everything Đoán dấu learned."])
        self.assertEqual(self.page.learned_row.get_subtitle(), "Nothing learned yet.")

    def test_a_failed_forget_keeps_the_file(self):
        self.dbus.ngosen_answers = {"ForgetLearned": TOO_OLD}
        self.page.forget()
        self.assertTrue(os.path.exists(self.file))
        self.assertIn("Restart fcitx5", self.messages[0])

    def test_how_it_learns_lists_every_line(self):
        lines = self.page.how_it_learns_lines
        self.assertEqual([label.get_label() for label in lines], learned.how_it_learns())

    def test_missing_table_note(self):
        self.assertFalse(self.page.table_note.get_visible())
        self.dbus.ngosen_answers = {"DiacriticsStatus": ("missing", "")}
        self.page.refresh_status()
        self.assertTrue(self.page.table_note.get_visible())


if __name__ == "__main__":
    unittest.main()
