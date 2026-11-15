# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import tempfile
import unittest
from unittest import mock

# isort: off
import fake_fcitx  # noqa: F401  (puts the settings window on sys.path)
from core import check_reminder, system_check
from core.system_check import FAIL, OK, WARN, Finding

# isort: on

FINE = [Finding(OK, "fcitx5 is running")]
QT = FINE + [Finding(FAIL, "Qt apps have no input method")]
GTK = FINE + [Finding(FAIL, "GTK apps have no input method")]
QT_AND_GTK = QT + GTK[1:]
OFF = [Finding(FAIL, "fcitx5 is not running", action=system_check.START_FCITX)]


def facts(**changes):
    env = {"XDG_SESSION_TYPE": "x11", "GTK_IM_MODULE": "fcitx", "QT_IM_MODULE": "fcitx"}
    return system_check.Facts(env=env, fcitx_running=True, **changes)


class ShouldOpenTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.path = os.path.join(folder.name, "ngosen", "check-reminders.json")

    def opens(self, *founds):
        return [check_reminder.should_open(found, self.path) for found in founds]

    def write(self, text):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "w", encoding="utf-8") as f:
            f.write(text)

    def test_the_first_open_lands_on_the_page_even_when_all_is_fine(self):
        self.assertEqual(self.opens(FINE, FINE), [True, False])

    def test_warnings_and_steps_waiting_for_a_login_do_not_open_it(self):
        later = Finding(FAIL, "KDE does not start fcitx5", after_login=True)
        warning = Finding(WARN, "IBus is running next to fcitx5")
        self.opens(FINE)
        self.assertEqual(self.opens(FINE + [warning], FINE + [later]), [False, False])

    def test_a_login_wait_from_the_real_check_does_not_open_it(self):
        found = system_check._with_one_login_notice(FINE + [Finding(FAIL, "Qt", after_login=True)])
        self.opens(FINE)
        self.assertEqual(self.opens(found), [False])

    def test_reminders_come_3_then_6_then_12_opens_apart_for_all_steps_together(self):
        self.opens(FINE)
        seen = self.opens(QT, GTK[:1] + QT[1:] + GTK[1:], *[QT_AND_GTK] * 20)
        self.assertEqual([i for i, shown in enumerate(seen) if shown], [0, 1, 4, 10])

    def test_a_new_step_opens_it_although_another_waits(self):
        self.opens(FINE, QT)
        self.assertEqual(self.opens(QT_AND_GTK, QT_AND_GTK), [True, False])

    def test_a_step_that_comes_back_after_a_fix_opens_it_at_once(self):
        self.opens(FINE, QT, FINE)
        self.assertEqual(self.opens(QT), [True])

    def test_fcitx5_off_hides_the_other_steps_without_forgetting_them(self):
        seen = self.opens(FINE, QT, QT, OFF, QT, QT, OFF, QT)
        self.assertEqual(seen, [True, True, False, True, False, False, True, False])

    def test_fcitx5_started_before_the_install_opens_it(self):
        found = system_check.steps(facts(ngosen_loaded=False))
        self.opens(FINE)
        self.assertEqual(self.opens(found), [True])

    def test_fcitx5_running_the_old_version_does_not_open_it(self):
        found = system_check.steps(facts(ngosen_in_group=True, addon_replaced=True))
        self.opens(FINE)
        self.assertEqual(self.opens(found), [False])

    def test_a_broken_file_opens_only_for_a_step_that_stops_typing(self):
        for text in ("{not json", '{"waiting": []}', "[]", '{"version": 1, "known": 3}'):
            with self.subTest(text=text):
                self.write(text)
                self.assertEqual(self.opens(FINE), [False])
                self.write(text)
                self.assertEqual(self.opens(QT), [True])

    def test_a_file_from_a_newer_version_is_left_alone(self):
        text = '{"version": 2, "known": [], "opens": 0, "gap": 3, "later": true}'
        self.write(text)
        self.assertEqual(self.opens(FINE, QT), [False, True])
        with open(self.path, encoding="utf-8") as f:
            self.assertEqual(f.read(), text)

    def test_an_unwritable_file_opens_only_for_a_step_that_stops_typing(self):
        with mock.patch.object(check_reminder.os, "replace", side_effect=OSError):
            self.assertEqual(self.opens(FINE, FINE), [False, False])
            self.assertEqual(self.opens(QT, QT), [True, True])

    def test_a_write_cut_short_keeps_the_last_good_file(self):
        self.opens(FINE)
        with open(self.path, encoding="utf-8") as f:
            before = f.read()
        with mock.patch.object(check_reminder.json, "dump", side_effect=OSError):
            self.opens(QT)
        with open(self.path, encoding="utf-8") as f:
            self.assertEqual(f.read(), before)
        self.assertEqual(json.loads(before)["version"], 1)

    def test_it_lives_with_the_typing_logs(self):
        path = check_reminder.state_file({"XDG_STATE_HOME": "/s", "HOME": "/h"})
        self.assertEqual(path, "/s/ngosen/check-reminders.json")


if __name__ == "__main__":
    unittest.main()
