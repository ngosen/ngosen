# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest

# isort: off
import fake_fcitx  # noqa: F401  (puts the settings window on sys.path)
from core import check_reminder
from core.system_check import FAIL, OK, WARN, Finding

# isort: on

FINE = [Finding(OK, "fcitx5 is running")]
QT = Finding(FAIL, "Qt apps have no input method")
GTK = Finding(FAIL, "GTK apps have no input method")


class ShouldOpenTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.path = os.path.join(folder.name, "ngosen", "check-reminders.json")

    def opens(self, *founds):
        return [check_reminder.should_open(found, self.path) for found in founds]

    def test_the_first_open_lands_on_the_page_even_when_all_is_fine(self):
        self.assertEqual(self.opens(FINE, FINE), [True, False])

    def test_warnings_and_steps_waiting_for_a_login_do_not_open_it(self):
        later = Finding(FAIL, "KDE does not start fcitx5", after_login=True)
        self.opens(FINE)
        self.assertEqual(self.opens([Finding(WARN, "IBus is running")], [later]), [False, False])

    def test_an_undone_step_opens_it_now_and_every_third_open(self):
        self.opens(FINE)
        self.assertEqual(self.opens(*[[QT]] * 7), [True, False, False, True, False, False, True])

    def test_a_new_step_opens_it_although_another_waits(self):
        self.opens(FINE, [QT])
        self.assertEqual(self.opens([QT, GTK]), [True])

    def test_a_step_that_comes_back_after_a_fix_opens_it_at_once(self):
        self.opens(FINE, [QT], FINE)
        self.assertEqual(self.opens([QT]), [True])

    def test_a_broken_file_does_not_count_as_a_first_open(self):
        os.makedirs(os.path.dirname(self.path))
        with open(self.path, "w") as f:
            f.write("{not json")
        self.assertEqual(self.opens(FINE), [False])

    def test_it_lives_with_the_typing_logs(self):
        path = check_reminder.state_file({"XDG_STATE_HOME": "/s", "HOME": "/h"})
        self.assertEqual(path, "/s/ngosen/check-reminders.json")


if __name__ == "__main__":
    unittest.main()
