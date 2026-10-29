# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import subprocess
import tempfile
import unittest
from unittest import mock
from urllib.parse import parse_qs, urlparse

from qtpy.QtWidgets import QWidget

# support puts the settings window on sys.path, so it has to come first.
# isort: off
from support import FakeDBusHandler, app  # noqa: F401
from core import system_check
from core.system_check import FAIL, WARN, Facts

# isort: on

FCITX = {"GTK_IM_MODULE": "fcitx", "QT_IM_MODULE": "fcitx", "XMODIFIERS": "@im=fcitx"}


def facts(session, desktop, env=None, **kw):
    base = {"XDG_SESSION_TYPE": session, "XDG_CURRENT_DESKTOP": desktop}
    base.update(FCITX if env is None else env)
    kw.setdefault("fcitx_running", True)
    kw.setdefault("ngosen_in_group", True)
    return Facts(env=base, **kw)


def titles(found, level=None):
    return [f.title for f in found if level is None or f.level == level]


class EvaluateTest(unittest.TestCase):
    def test_gnome_with_install_sh_variables_is_clean(self):
        self.assertEqual(system_check.evaluate(facts("wayland", "ubuntu:GNOME")), [])

    def test_fcitx5_not_running_is_the_only_finding(self):
        found = system_check.evaluate(facts("wayland", "GNOME", env={}, fcitx_running=False))
        self.assertEqual(titles(found), ["fcitx5 is not running"])

    def test_ngosen_missing_from_the_group(self):
        found = system_check.evaluate(facts("wayland", "GNOME", ngosen_in_group=False))
        self.assertIn("Ngó Sen is not in the fcitx5 input method list", titles(found, FAIL))

    def test_qt_without_im_module_on_gnome(self):
        env = dict(FCITX, QT_IM_MODULE="")
        found = system_check.evaluate(facts("wayland", "GNOME", env=env))
        self.assertEqual(titles(found, FAIL), ["Qt apps have no input method"])

    def test_plasma_x11_needs_gtk_and_qt_modules(self):
        env = {"XMODIFIERS": "@im=fcitx"}
        found = system_check.evaluate(facts("x11", "KDE", env=env))
        self.assertEqual(
            titles(found, FAIL), ["GTK apps have no input method", "Qt apps have no input method"]
        )

    def test_plasma_wayland_wants_kwin_to_start_fcitx5_and_no_im_modules(self):
        env = {"XMODIFIERS": "@im=fcitx"}
        clean = facts(
            "wayland",
            "KDE",
            env=env,
            kwin_input_method="/usr/share/applications/org.fcitx.Fcitx5.desktop",
        )
        self.assertEqual(system_check.evaluate(clean), [])

        found = system_check.evaluate(facts("wayland", "KDE", env=FCITX))
        self.assertEqual(titles(found, FAIL), ["KDE does not start fcitx5 as its virtual keyboard"])
        self.assertEqual(
            titles(found, WARN), ["GTK_IM_MODULE or QT_IM_MODULE is set on KDE Wayland"]
        )

    def test_other_wayland_compositors_need_qt_module(self):
        env = {"XMODIFIERS": "@im=fcitx"}
        found = system_check.evaluate(facts("wayland", "Hyprland", env=env))
        self.assertEqual(titles(found, WARN), ["Qt 5 apps have no input method"])

    def test_missing_xmodifiers_and_running_ibus_warn(self):
        env = dict(FCITX, XMODIFIERS="@im=ibus")
        found = system_check.evaluate(facts("wayland", "GNOME", env=env, ibus_running=True))
        self.assertEqual(
            titles(found, WARN),
            ["Older X11 apps have no input method", "IBus is running next to fcitx5"],
        )

    def test_gnome_50_without_the_forward_key_fix_warns(self):
        def found(version, state):
            f = facts(
                "wayland", "ubuntu:GNOME", gnome_shell_version=version, forward_keys_state=state
            )
            return system_check.evaluate(f)

        title = "GNOME 50.1 drops the keys Ngó Sen sends to apps"
        self.assertEqual(titles(found("50.1", "INACTIVE"), WARN), [title])
        self.assertIn("gnome-extensions enable", found("50.1", "INACTIVE")[0].fix)
        self.assertIn("install.sh", found("50.1", "")[0].fix)
        self.assertEqual(found("50.1", "ACTIVE"), [])
        self.assertEqual(found("50.4", "INACTIVE"), [])
        self.assertEqual(found("49.2", ""), [])
        self.assertEqual(found("", ""), [])


class ReportTest(unittest.TestCase):
    def test_debug_info_gives_each_app_once(self):
        text = (
            "Group [wayland:] has 3 InputContext(s)\n"
            "  IC [a] program:kitty frontend:wayland cap:72 focus:0\n"
            "  IC [b] program:kitty frontend:wayland cap:72 focus:1\n"
            "  IC [c] program: frontend:wayland cap:72 focus:0\n"
            "  IC [d] program:gtk3-im:zalo frontend:ibus cap:12 focus:0\n"
        )
        self.assertEqual(
            system_check.parse_debug_info(text), [("kitty", "wayland"), ("gtk3-im:zalo", "ibus")]
        )

    def test_report_and_issue_url_carry_the_findings(self):
        f = facts("x11", "KDE", env={}, ngosen_version="1.4.2", distro="Fedora Linux 44")
        f.apps = [("konsole", "dbus")]
        text = system_check.report(f, system_check.evaluate(f))
        self.assertIn("[X] Qt apps have no input method", text)
        self.assertIn("QT_IM_MODULE=(unset)", text)
        self.assertIn("konsole (dbus)", text)

        query = parse_qs(urlparse(system_check.issue_url(f, text)).query)
        self.assertEqual(query["template"], ["bug_report.yml"])
        self.assertEqual(query["version"], ["1.4.2"])
        self.assertEqual(query["distro"], ["Fedora Linux 44"])
        self.assertEqual(query["diagnose"], [text])


class CollectTest(unittest.TestCase):
    def test_reads_kwin_setting_and_fcitx_state(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        with open(os.path.join(tmp.name, "kwinrc"), "w", encoding="utf-8") as f:
            f.write("[Wayland]\nInputMethod=/usr/share/applications/org.fcitx.Fcitx5.desktop\n")
        env = {"XDG_CONFIG_HOME": tmp.name, "XDG_SESSION_TYPE": "wayland", "HOME": tmp.name}
        f = system_check.collect(FakeDBusHandler(), "1.4.2", env=env)
        self.assertTrue(f.fcitx_running)
        self.assertTrue(f.ngosen_in_group)
        self.assertIn("Fcitx5", f.kwin_input_method)
        self.assertEqual(f.apps, [("kate", "wayland")])
        self.assertNotIn("HOME", f.env)

    def test_reads_gnome_shell_and_forward_key_fix_on_gnome(self):
        info = "forward-keys@ngosen.github.io\n  Enabled: No\n  State: INACTIVE\n"
        outputs = {"gnome-shell": "GNOME Shell 50.1\n", "gnome-extensions": info}

        def run(cmd, **kw):
            return subprocess.CompletedProcess(cmd, 0, stdout=outputs.get(cmd[0], ""))

        env = {"XDG_CURRENT_DESKTOP": "ubuntu:GNOME"}
        with mock.patch.object(system_check.subprocess, "run", side_effect=run):
            f = system_check.collect(FakeDBusHandler(), env=env)
        self.assertEqual((f.gnome_shell_version, f.forward_keys_state), ("50.1", "INACTIVE"))
        self.assertIn("GNOME Shell 50.1, forward-keys: INACTIVE", system_check.report(f, []))

        outputs["gnome-extensions"] = ""
        with mock.patch.object(system_check.subprocess, "run", side_effect=run):
            self.assertEqual(
                system_check.collect(FakeDBusHandler(), env=env).forward_keys_state, ""
            )
        with mock.patch.object(system_check.subprocess, "run", side_effect=run) as ran:
            f = system_check.collect(FakeDBusHandler(), env={"XDG_CURRENT_DESKTOP": "KDE"})
        self.assertEqual(f.gnome_shell_version, "")
        self.assertNotIn("gnome-shell", [c.args[0][0] for c in ran.call_args_list])

    def test_fcitx5_not_answering(self):
        handler = FakeDBusHandler()
        handler.input_method_names = lambda: None
        f = system_check.collect(handler, env={})
        self.assertFalse(f.fcitx_running)

    def test_collect_notes_a_replaced_addon(self):
        with mock.patch.object(system_check, "runs_replaced_addon", return_value=True):
            f = system_check.collect(FakeDBusHandler(), env={})
        self.assertTrue(f.addon_replaced)


class ReplacedAddonTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.proc = tmp.name

    def process(self, pid, comm, maps):
        os.mkdir(os.path.join(self.proc, pid))
        files = {"comm": comm + ("\n" if isinstance(comm, str) else b"\n"), "maps": maps}
        for name, content in files.items():
            if content is None:
                continue
            data = content.encode() if isinstance(content, str) else content
            with open(os.path.join(self.proc, pid, name), "wb") as f:
                f.write(data)

    def test_fcitx5_holding_a_deleted_addon(self):
        self.process(
            "10", "fcitx5", "7f00-7f10 r-xp 0 00:1f 42 /usr/lib64/fcitx5/libngosen.so (deleted)\n"
        )
        self.assertTrue(system_check.runs_replaced_addon(self.proc))

    def test_fcitx5_with_the_installed_addon(self):
        self.process("10", "fcitx5", "7f00-7f10 r-xp 0 00:1f 42 /usr/lib/fcitx5/libngosen.so\n")
        self.assertFalse(system_check.runs_replaced_addon(self.proc))

    def test_other_deleted_libraries_and_processes_do_not_count(self):
        self.process("10", "fcitx5", "7f00-7f10 r-xp 0 00:1f 42 /usr/lib64/libfoo.so (deleted)\n")
        self.process(
            "11", "kate", "7f00-7f10 r-xp 0 00:1f 42 /usr/lib64/fcitx5/libngosen.so (deleted)\n"
        )
        self.process("12", "fcitx5", None)
        self.assertFalse(system_check.runs_replaced_addon(self.proc))

    def test_names_and_paths_that_are_not_utf8(self):
        self.process("9", b"\xffname", None)
        maps = (
            b"7f00-7f10 r--p 0 00:1f 41 /usr/share/fonts/caf\xe9.ttf\n"
            b"7f10-7f20 r-xp 0 00:1f 42 /usr/lib64/fcitx5/libngosen.so (deleted)\n"
        )
        self.process("10", "fcitx5", maps)
        self.assertTrue(system_check.runs_replaced_addon(self.proc))

    def test_another_users_fcitx5_does_not_count(self):
        self.process(
            "10", "fcitx5", "7f00-7f10 r-xp 0 00:1f 42 /usr/lib64/fcitx5/libngosen.so (deleted)\n"
        )
        with mock.patch("os.getuid", return_value=os.getuid() + 1):
            self.assertFalse(system_check.runs_replaced_addon(self.proc))

    def test_evaluate_warns_about_a_replaced_addon(self):
        found = system_check.evaluate(facts("wayland", "ubuntu:GNOME", addon_replaced=True))
        self.assertEqual(titles(found, WARN), ["fcitx5 still runs Ngó Sen from before the update"])


class DialogTest(unittest.TestCase):
    def setUp(self):
        # The fcitx5 of the machine running the tests must not add a row.
        patcher = mock.patch.object(system_check, "runs_replaced_addon", return_value=False)
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_dialog_lists_findings_and_copies_the_report(self):
        from qtpy.QtGui import QGuiApplication
        from ui.system_check_dialog import SystemCheckDialog

        env = {"XDG_SESSION_TYPE": "x11", "XDG_CURRENT_DESKTOP": "XFCE"}
        with mock.patch.dict(os.environ, env, clear=True):
            dialog = SystemCheckDialog(FakeDBusHandler(), "1.4.2")
        rows = [
            w.objectName()
            for w in dialog.findChildren(object)
            if w.objectName().startswith("CheckRow-")
        ]
        self.assertIn("CheckRow-fail", rows)
        self.assertIn("kate (wayland)", dialog.apps.text())

        fixed = dict(env, GTK_IM_MODULE="fcitx", QT_IM_MODULE="fcitx", XMODIFIERS="@im=fcitx")
        with mock.patch.dict(os.environ, fixed, clear=True):
            dialog.run()
        rows = [
            w.objectName()
            for w in dialog.findChildren(QWidget)
            if w.objectName().startswith("CheckRow-")
        ]
        self.assertEqual(rows, ["CheckRow-ok"])

        with mock.patch.dict(os.environ, env, clear=True):
            dialog.run()
        dialog.copy_report()
        self.assertIn("[X] GTK apps have no input method", QGuiApplication.clipboard().text())

    def test_messaging_copies_the_report_and_opens_the_chat(self):
        from core.brand import MESSENGER_URL
        from qtpy.QtGui import QGuiApplication
        from ui.system_check_dialog import SystemCheckDialog

        env = {"XDG_SESSION_TYPE": "x11", "XDG_CURRENT_DESKTOP": "XFCE"}
        with mock.patch.dict(os.environ, env, clear=True):
            dialog = SystemCheckDialog(FakeDBusHandler(), "1.4.2")
        QGuiApplication.clipboard().setText("")
        with mock.patch("ui.system_check_dialog.QDesktopServices.openUrl") as open_url:
            dialog.message_btn.click()
        self.assertEqual(open_url.call_args[0][0].toString(), MESSENGER_URL)
        self.assertIn("Ngó Sen 1.4.2", QGuiApplication.clipboard().text())
        self.assertIn("Paste", dialog.status.text())

    def test_copying_says_it_is_done(self):
        from ui.system_check_dialog import SystemCheckDialog

        dialog = SystemCheckDialog(FakeDBusHandler(), "1.4.2")
        self.assertEqual(dialog.status.text(), "")
        dialog.copy_btn.click()
        self.assertTrue(dialog.status.text())

    def test_dialog_grows_to_fit_more_rows(self):
        from qtpy.QtWidgets import QApplication, QLabel
        from ui.system_check_dialog import SystemCheckDialog

        env = {"XDG_SESSION_TYPE": "x11", "XDG_CURRENT_DESKTOP": "XFCE"}
        fixed = dict(env, GTK_IM_MODULE="fcitx", QT_IM_MODULE="fcitx", XMODIFIERS="@im=fcitx")
        with mock.patch.dict(os.environ, fixed, clear=True):
            dialog = SystemCheckDialog(FakeDBusHandler(), "1.4.2")
        dialog.show()
        QApplication.processEvents()
        with mock.patch.dict(os.environ, env, clear=True):
            dialog.run()
        QApplication.processEvents()
        for label in dialog.findChildren(QLabel):
            if label.parent().objectName().startswith("CheckRow-") and label.text():
                self.assertGreaterEqual(label.height(), label.sizeHint().height(), label.text())
        dialog.close()


if __name__ == "__main__":
    unittest.main()
