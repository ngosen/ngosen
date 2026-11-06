# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import subprocess
import tempfile
import unittest
from unittest import mock

# support puts the settings window on sys.path, so it has to come first.
# isort: off
from support import app  # noqa: F401
from core import session_setup

# isort: on

INSTALL_SH = os.path.join(os.path.dirname(__file__), "..", "..", "install.sh")
ENTRY = "[Desktop Entry]\nType=Application\nName=Fcitx 5\nExec=fcitx5\n"


class SessionSetupCase(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.home = tmp.name
        self.env = {"HOME": self.home, "XDG_CONFIG_DIRS": os.path.join(self.home, "system")}
        self.config = os.path.join(self.home, ".config")
        self.user_entry = os.path.join(self.config, "autostart", session_setup.AUTOSTART_NAME)
        self.env_file = os.path.join(self.config, "environment.d", session_setup.ENV_FILE)
        self.system_entry = os.path.join(
            self.home, "system", "autostart", session_setup.AUTOSTART_NAME
        )
        patcher = mock.patch.object(session_setup, "has_im_config", return_value=False)
        self.has_im_config = patcher.start()
        self.addCleanup(patcher.stop)

    def write(self, path, text):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)

    def read(self, path):
        with open(path, encoding="utf-8") as f:
            return f.read()


class StartsAtLoginTest(SessionSetupCase):
    def starts(self):
        return session_setup.starts_at_login(self.env)

    def test_nothing_starts_fcitx5(self):
        self.assertFalse(self.starts())

    def test_system_and_user_entries_start_it(self):
        self.write(self.system_entry, ENTRY)
        self.assertTrue(self.starts())
        os.remove(self.system_entry)
        self.write(self.user_entry, ENTRY)
        self.assertTrue(self.starts())

    def test_user_mask_hides_the_system_entry(self):
        self.write(self.system_entry, ENTRY)
        self.write(self.user_entry, session_setup.AUTOSTART_MASK)
        self.assertFalse(self.starts())

    def test_an_entry_turned_off_in_gnome_does_not_count(self):
        self.write(self.user_entry, ENTRY + "X-GNOME-Autostart-enabled=false\n")
        self.assertFalse(self.starts())

    def test_entry_under_another_name_counts(self):
        path = os.path.join(self.config, "autostart", "my-input.desktop")
        self.write(path, "[Desktop Entry]\nExec=/usr/bin/fcitx5 -d --replace\n")
        self.assertTrue(self.starts())

    def test_other_fcitx5_programs_do_not_count(self):
        path = os.path.join(self.config, "autostart", "tool.desktop")
        self.write(path, "[Desktop Entry]\nExec=fcitx5-configtool\n")
        self.assertFalse(self.starts())

    def test_broken_entry_is_ignored(self):
        self.write(self.user_entry, "not a desktop file\n\xff\n")
        self.assertFalse(self.starts())

    def test_leftover_xinputrc_without_im_config_does_not_count(self):
        self.write(os.path.join(self.home, ".xinputrc"), "run_im fcitx5\n")
        self.assertFalse(self.starts())

    def test_another_im_config_choice_beats_the_default(self):
        self.has_im_config.return_value = True
        self.write(os.path.join(self.home, ".xinputrc"), "run_im ibus\n")
        done = subprocess.CompletedProcess([], 0, stdout="active\nuser\nfcitx5\n")
        with mock.patch.object(session_setup.subprocess, "run", return_value=done):
            self.assertFalse(self.starts())

    def test_non_choice_leaves_the_default_in_charge(self):
        self.has_im_config.return_value = True
        self.write(os.path.join(self.home, ".xinputrc"), "run_im REMOVE\n")
        done = subprocess.CompletedProcess([], 0, stdout="active\nuser\nfcitx5\n")
        with mock.patch.object(session_setup.subprocess, "run", return_value=done):
            self.assertTrue(self.starts())

    def test_im_config_is_read_untranslated(self):
        self.has_im_config.return_value = True
        done = subprocess.CompletedProcess([], 0, stdout="")
        with mock.patch.object(session_setup.subprocess, "run", return_value=done) as ran:
            self.starts()
        self.assertEqual(ran.call_args.kwargs["env"]["LC_ALL"], "C")

    def test_im_config_choice_counts(self):
        self.has_im_config.return_value = True
        self.write(os.path.join(self.home, ".xinputrc"), "# im-config\nrun_im fcitx5\n")
        self.assertTrue(self.starts())
        done = subprocess.CompletedProcess([], 0, stdout="")
        self.write(os.path.join(self.home, ".xinputrc"), "run_im ibus\n")
        with mock.patch.object(session_setup.subprocess, "run", return_value=done):
            self.assertFalse(self.starts())

    def test_im_config_default_counts(self):
        self.has_im_config.return_value = True

        def default(new_flag_output, old_flag_output=""):
            def run(cmd, **kw):
                out = old_flag_output if cmd[1] == "-m" else new_flag_output
                return subprocess.CompletedProcess(cmd, 0, stdout=out)

            with mock.patch.object(session_setup.subprocess, "run", side_effect=run):
                return self.starts()

        self.assertTrue(default("", "active\nuser\nfcitx5\n"))
        self.assertFalse(default("", "active\nuser\nibus\n"))
        self.assertTrue(default("IM framework (auto): fcitx5\n"))
        self.assertFalse(default("IM framework (auto): ibus\n"))


class AutostartFixTest(SessionSetupCase):
    def setUp(self):
        super().setUp()
        self.source = os.path.join(self.home, "applications", session_setup.AUTOSTART_NAME)
        self.write(self.source, ENTRY)
        patcher = mock.patch.object(session_setup, "AUTOSTART_SOURCE", self.source)
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_copies_the_entry_and_undo_removes_it(self):
        done, undo = session_setup.enable_autostart(self.env)
        self.assertTrue(done)
        self.assertEqual(self.read(self.user_entry), ENTRY)
        self.assertTrue(session_setup.starts_at_login(self.env))
        self.assertTrue(undo())
        self.assertFalse(os.path.exists(self.user_entry))

    def test_removes_the_mask_so_the_system_entry_runs(self):
        self.write(self.system_entry, ENTRY)
        self.write(self.user_entry, session_setup.AUTOSTART_MASK)
        done, undo = session_setup.enable_autostart(self.env)
        self.assertTrue(done)
        self.assertFalse(os.path.exists(self.user_entry))
        self.assertTrue(session_setup.starts_at_login(self.env))
        self.assertTrue(undo())
        self.assertEqual(self.read(self.user_entry), session_setup.AUTOSTART_MASK)

    def test_replaces_the_mask_when_there_is_no_system_entry(self):
        self.write(self.user_entry, session_setup.AUTOSTART_MASK)
        done, undo = session_setup.enable_autostart(self.env)
        self.assertTrue(done)
        self.assertEqual(self.read(self.user_entry), ENTRY)
        self.assertTrue(undo())
        self.assertEqual(self.read(self.user_entry), session_setup.AUTOSTART_MASK)

    def test_leaves_a_users_own_entry_alone(self):
        mine = "[Desktop Entry]\nHidden=true\nComment=mine\n"
        self.write(self.user_entry, mine)
        self.assertEqual(session_setup.enable_autostart(self.env), (False, None))
        self.assertEqual(self.read(self.user_entry), mine)

    def test_keeps_the_mask_when_the_package_has_no_entry(self):
        os.remove(self.source)
        self.write(self.user_entry, session_setup.AUTOSTART_MASK)
        self.assertEqual(session_setup.enable_autostart(self.env), (False, None))
        self.assertEqual(self.read(self.user_entry), session_setup.AUTOSTART_MASK)

    def test_undo_keeps_an_entry_edited_since(self):
        _done, undo = session_setup.enable_autostart(self.env)
        self.write(self.user_entry, ENTRY + "Comment=edited\n")
        self.assertFalse(undo())
        self.assertIn("edited", self.read(self.user_entry))


class EnvironmentFixTest(SessionSetupCase):
    def setUp(self):
        super().setUp()
        patcher = mock.patch.object(session_setup, "_succeeds", return_value=True)
        self.ran = patcher.start()
        self.addCleanup(patcher.stop)

    def test_gnome_gets_all_variables_and_undo_removes_them(self):
        self.env["XDG_CURRENT_DESKTOP"] = "ubuntu:GNOME"
        done, undo = session_setup.set_environment(self.env)
        self.assertTrue(done)
        self.assertEqual(self.read(self.env_file), session_setup.GNOME_ENV)
        self.ran.assert_called_with(["systemctl", "--user", "daemon-reload"])
        self.assertEqual(session_setup.saved_environment(self.env)["QT_IM_MODULE"], "fcitx")
        self.assertTrue(undo())
        self.assertFalse(os.path.exists(self.env_file))

    def test_kde_gets_only_xmodifiers(self):
        self.env["XDG_CURRENT_DESKTOP"] = "KDE"
        self.assertTrue(session_setup.set_environment(self.env)[0])
        self.assertEqual(self.read(self.env_file), session_setup.KDE_ENV)

    def test_an_existing_file_is_not_overwritten(self):
        self.write(self.env_file, "FOO=bar\n")
        self.assertEqual(session_setup.set_environment(self.env), (False, None))
        self.assertEqual(self.read(self.env_file), "FOO=bar\n")

    def test_a_value_the_user_chose_elsewhere_stays(self):
        mine = os.path.join(self.config, "environment.d", "10-mine.conf")
        self.write(mine, "QT_IM_MODULE=ibus\n")
        self.assertEqual(session_setup.set_environment(self.env), (False, None))
        self.assertFalse(os.path.exists(self.env_file))

    def test_saved_values_lose_quotes_and_the_last_file_wins(self):
        folder = os.path.join(self.config, "environment.d")
        self.write(os.path.join(folder, "10-a.conf"), "# note\nQT_IM_MODULE=ibus\n")
        self.write(os.path.join(folder, "20-b.conf"), "QT_IM_MODULE=\"fcitx\"\nX='y'\n")
        self.assertEqual(
            session_setup.saved_environment(self.env), {"QT_IM_MODULE": "fcitx", "X": "y"}
        )

    def test_gnome_on_debian_also_sets_im_config_and_undo_restores_it(self):
        self.has_im_config.return_value = True
        self.env["XDG_CURRENT_DESKTOP"] = "GNOME"
        xinputrc = os.path.join(self.home, ".xinputrc")
        self.write(xinputrc, "# mine\n")
        _done, undo = session_setup.set_environment(self.env)
        self.ran.assert_any_call(["im-config", "-n", "fcitx5"])
        self.write(xinputrc, "run_im fcitx5\n")
        self.assertTrue(undo())
        self.assertEqual(self.read(xinputrc), "# mine\n")

    def test_gnome_keeps_another_im_config_choice(self):
        self.has_im_config.return_value = True
        self.env["XDG_CURRENT_DESKTOP"] = "GNOME"
        self.write(os.path.join(self.home, ".xinputrc"), "run_im ibus\n")
        self.assertTrue(session_setup.set_environment(self.env)[0])
        self.assertNotIn(mock.call(["im-config", "-n", "fcitx5"]), self.ran.call_args_list)


class ImConfigFixTest(SessionSetupCase):
    def test_fix_is_noted_for_uninstall(self):
        with mock.patch.object(session_setup, "_succeeds", return_value=True):
            session_setup.use_im_config(self.env)
            session_setup.use_im_config(self.env)
        noted = os.path.join(self.home, ".local", "state", "ngosen", "install-changes")
        self.assertEqual(self.read(noted), "im-config\n")

    def test_uses_the_new_flag_when_the_old_one_fails(self):
        with mock.patch.object(session_setup, "_succeeds", side_effect=[False, True]) as ran:
            self.assertTrue(session_setup.use_im_config(self.env)[0])
        self.assertEqual(ran.call_args.args[0], ["im-config", "-w", "fcitx5"])

    def test_failure_changes_nothing(self):
        with mock.patch.object(session_setup, "_succeeds", return_value=False):
            self.assertEqual(session_setup.use_im_config(self.env), (False, None))

    def test_undo_removes_a_file_that_was_not_there(self):
        with mock.patch.object(session_setup, "_succeeds", return_value=True):
            _done, undo = session_setup.use_im_config(self.env)
        self.write(os.path.join(self.home, ".xinputrc"), "run_im fcitx5\n")
        self.assertTrue(undo())
        self.assertFalse(os.path.exists(os.path.join(self.home, ".xinputrc")))


class KwinFixTest(SessionSetupCase):
    def setUp(self):
        super().setUp()
        launcher = os.path.join(self.home, "launcher.desktop")
        self.write(launcher, ENTRY)
        patcher = mock.patch.object(session_setup, "AUTOSTART_SOURCE", launcher)
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_writes_the_setting_and_undo_empties_it_again(self):
        with mock.patch.object(session_setup, "_succeeds", return_value=True) as ran:
            done, undo = session_setup.set_kwin_input_method("")
            self.assertTrue(done)
            self.assertEqual(ran.call_args.args[0][-1], session_setup.AUTOSTART_SOURCE)
            undo()
            self.assertEqual(ran.call_args.args[0][-1], "")

    def test_keeps_another_input_method(self):
        with mock.patch.object(session_setup, "_succeeds", return_value=True) as ran:
            self.assertEqual(session_setup.set_kwin_input_method("/maliit.desktop"), (False, None))
        ran.assert_not_called()

    def test_no_launcher_is_a_failure(self):
        os.remove(session_setup.AUTOSTART_SOURCE)
        with mock.patch.object(session_setup, "_succeeds", return_value=True) as ran:
            self.assertEqual(session_setup.set_kwin_input_method(""), (False, None))
        ran.assert_not_called()

    def test_no_kwriteconfig_is_a_failure(self):
        with mock.patch.object(session_setup.subprocess, "run", side_effect=OSError):
            self.assertEqual(session_setup.set_kwin_input_method(""), (False, None))


class SameAsInstallerTest(SessionSetupCase):
    """install.sh --uninstall compares files with its own text, so a difference strands them."""

    def installer(self, function):
        cmd = f'eval "$(sed -n "/^{function}() {{/,/^}}/p" "$0")"; {function}'
        return subprocess.run(
            ["bash", "-c", cmd, INSTALL_SH], capture_output=True, text=True, check=True
        ).stdout

    def test_gnome_variables(self):
        self.assertEqual(self.installer("im_environment_entry"), session_setup.GNOME_ENV)

    def test_changes_file(self):
        self.assertEqual(self.installer("changes_file"), session_setup.changes_file(os.environ))

    def test_autostart_mask(self):
        self.assertEqual(self.installer("fcitx5_autostart_mask"), session_setup.AUTOSTART_MASK)

    def test_kde_variable(self):
        with open(INSTALL_SH, encoding="utf-8") as f:
            self.assertIn(f"echo '{session_setup.KDE_ENV.strip()}'", f.read())

    def run_command(self, command):
        env = {"HOME": self.home, "PATH": os.defpath}
        subprocess.run(["bash", "-c", command], env=env, check=True)

    def test_commands_never_replace_a_file(self):
        command = session_setup.set_environment_command(session_setup.GNOME_ENV)
        self.run_command(command)
        self.assertEqual(self.read(self.env_file), session_setup.GNOME_ENV)
        self.write(self.env_file, "FOO=bar\n")
        self.run_command(command)
        self.assertEqual(self.read(self.env_file), "FOO=bar\n" + session_setup.GNOME_ENV)
        self.assertIn("cp -n ", session_setup.ENABLE_AUTOSTART_COMMAND)


if __name__ == "__main__":
    unittest.main()
