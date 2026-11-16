# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import subprocess
import tempfile
import unittest
from unittest import mock

# isort: off
import fake_fcitx  # noqa: F401  (puts the settings window on sys.path)
from core import check_reminder, login_waits, system_check
from core.system_check import FAIL, WARN

# isort: on

NOTICE = "Log out and back in to finish the setup"
LEFT = "Logged back in, but the settings still do not apply"


def facts(login, **changes):
    """XFCE with im-config chosen: its variables wait for the next login."""
    values = dict(env={"XDG_SESSION_TYPE": "x11", "XDG_CURRENT_DESKTOP": "XFCE"})
    values.update(fcitx_running=True, ngosen_in_group=True, has_im_config=True)
    values.update(im_config_chosen=True, login=login)
    values.update(changes)
    return system_check.Facts(**values)


def done(login):
    env = {"XDG_SESSION_TYPE": "x11", "XMODIFIERS": "@im=fcitx"}
    env.update(GTK_IM_MODULE="fcitx", QT_IM_MODULE="fcitx")
    return facts(login, env=env)


class StepsTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.path = os.path.join(folder.name, "ngosen", "login-waits.json")

    def notices(self, *runs):
        """The level of the login notice after each run, or None when there is none."""
        levels = []
        for run in runs:
            found = login_waits.steps(run, self.path)
            notice = [f for f in found if f.waiting]
            levels.append(notice[0].level if notice else None)
        return levels

    def write(self, text):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "w", encoding="utf-8") as f:
            f.write(text)

    def test_within_one_login_the_notice_stays_a_warning(self):
        self.assertEqual(self.notices(facts("b/1"), facts("b/1")), [WARN, WARN])

    def test_a_step_still_waiting_after_a_new_login_is_a_failure(self):
        self.assertEqual(self.notices(facts("b/1"), facts("b/2"), facts("b/2")), [WARN, FAIL, FAIL])
        found = login_waits.steps(facts("b/2"), self.path)
        self.assertEqual([f.title for f in found if f.level == FAIL], [LEFT])

    def test_a_restart_counts_as_a_new_login(self):
        self.assertEqual(self.notices(facts("a/1"), facts("b/1")), [WARN, FAIL])

    def test_without_a_login_id_nothing_is_remembered(self):
        self.assertEqual(self.notices(facts("")), [WARN])
        self.assertFalse(os.path.exists(self.path))
        self.assertEqual(self.notices(facts("b/2")), [WARN])
        self.assertEqual(self.notices(facts("")), [WARN])

    def test_a_finished_step_is_forgotten(self):
        self.assertEqual(self.notices(facts("b/1"), done("b/2"), facts("b/3")), [WARN, None, WARN])

    def test_with_fcitx5_off_the_waiting_steps_are_kept(self):
        off = facts("b/2", fcitx_running=False)
        self.assertEqual(self.notices(facts("b/1"), off, facts("b/2")), [WARN, None, FAIL])

    def test_a_step_that_starts_waiting_in_this_login_is_a_warning(self):
        env = {
            "XDG_SESSION_TYPE": "wayland",
            "XDG_CURRENT_DESKTOP": "KDE",
            "XMODIFIERS": "@im=fcitx",
        }
        kde = dict(env=env)
        kde.update(kwin_input_method="fcitx", started_by_kwin=False)
        self.notices(facts("b/1"))
        self.assertEqual(self.notices(facts("b/2", **kde)), [WARN])

    def test_an_unwritable_folder_remembers_nothing_and_never_turns_red(self):
        # A file where the folder goes stops root too, as CI runs; a read-only folder does not.
        with open(os.path.dirname(self.path), "w", encoding="utf-8"):
            pass
        self.assertEqual(self.notices(facts("b/1"), facts("b/2")), [WARN, WARN])
        self.assertFalse(os.path.exists(self.path))

    def test_a_broken_file_starts_over(self):
        self.write("{not json")
        self.assertEqual(self.notices(facts("b/1"), facts("b/2")), [WARN, FAIL])

    def test_a_file_from_a_newer_version_is_read_but_not_written(self):
        newer = {"version": login_waits.VERSION + 1, "waits": {}, "more": 1}
        self.write(json.dumps(newer))
        self.notices(facts("b/1"))
        with open(self.path, encoding="utf-8") as f:
            self.assertEqual(json.load(f), newer)

    def test_the_failure_opens_the_window_on_the_check(self):
        reminders = os.path.join(os.path.dirname(self.path), "check-reminders.json")
        first = login_waits.steps(facts("b/1"), self.path)
        later = login_waits.steps(facts("b/2"), self.path)
        opened = [check_reminder.should_open(found, reminders) for found in (first, first, later)]
        self.assertEqual(opened, [True, False, True])


class LoginIdTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.boot = os.path.join(folder.name, "boot_id")
        with open(self.boot, "w", encoding="ascii") as f:
            f.write("5e1f\n")

    def test_it_joins_the_boot_and_the_session(self):
        self.assertEqual(login_waits.login_id({"XDG_SESSION_ID": "3"}, self.boot), "5e1f/3")

    def test_it_is_empty_when_either_is_missing(self):
        with mock.patch.object(login_waits, "_display_session", return_value=""):
            self.assertEqual(login_waits.login_id({}, self.boot), "")
        missing = os.path.join(os.path.dirname(self.boot), "none")
        self.assertEqual(login_waits.login_id({"XDG_SESSION_ID": "3"}, missing), "")

    def test_without_the_variable_it_asks_logind_for_the_desktop_session(self):
        # GNOME and Plasma start apps from systemd, which leaves XDG_SESSION_ID out.
        with mock.patch.object(login_waits, "_display_session", return_value="7"):
            self.assertEqual(login_waits.login_id({}, self.boot), "5e1f/7")
            self.assertEqual(login_waits.login_id({"XDG_SESSION_ID": "3"}, self.boot), "5e1f/3")

    def test_the_desktop_session_is_empty_when_logind_cannot_tell(self):
        answers = [
            subprocess.CompletedProcess([], 0, stdout="12\n"),
            subprocess.CompletedProcess([], 1, stdout="", stderr="no such user"),
            FileNotFoundError("loginctl"),
            subprocess.TimeoutExpired("loginctl", 5),
        ]
        found = []
        for answer in answers:
            with mock.patch.object(subprocess, "run", side_effect=[answer]):
                found.append(login_waits._display_session())
        self.assertEqual(found, ["12", "", "", ""])

    def test_run_tells_the_check_this_login(self):
        env = {"XDG_SESSION_ID": "3", "XDG_STATE_HOME": os.path.dirname(self.boot)}
        with mock.patch.object(system_check, "collect", return_value=facts("")):
            result, found = login_waits.run(None, env=env, boot_id_path=self.boot)
        self.assertEqual(result.login, "5e1f/3")
        self.assertEqual([f.level for f in found if f.waiting], [WARN])
        self.assertTrue(os.path.exists(login_waits.state_file(env)))


if __name__ == "__main__":
    unittest.main()
