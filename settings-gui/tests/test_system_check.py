# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import subprocess
import tempfile
import unittest
from unittest import mock
from urllib.parse import parse_qs, urlparse

# support puts the settings window on sys.path, so it has to come first.
# isort: off
from support import FakeDBusHandler, app  # noqa: F401
from core import system_check
from core.system_check import FAIL, OK, WARN, Facts

# isort: on

FCITX = {"GTK_IM_MODULE": "fcitx", "QT_IM_MODULE": "fcitx", "XMODIFIERS": "@im=fcitx"}


def facts(session, desktop, env=None, **kw):
    base = {"XDG_SESSION_TYPE": session, "XDG_CURRENT_DESKTOP": desktop}
    base.update(FCITX if env is None else env)
    kw.setdefault("fcitx_running", True)
    kw.setdefault("ngosen_in_group", True)
    kw.setdefault("input_panel_state", "ACTIVE")
    return Facts(env=base, **kw)


def titles(found, level=None):
    return [f.title for f in found if level is None or f.level == level]


class InputPanelTest(unittest.TestCase):
    TITLE = "Menu and hints do not show in GNOME's search box"

    def found(self, state, desktop="ubuntu:GNOME"):
        return system_check.evaluate(facts("wayland", desktop, input_panel_state=state))

    def test_gnome_without_the_panel_offers_to_install_it(self):
        finding = self.found("")[0]
        self.assertEqual((finding.level, finding.title), (WARN, self.TITLE))
        self.assertEqual(finding.action, system_check.INSTALL_INPUT_PANEL)
        self.assertIn(system_check.INPUT_PANEL_PAGE, finding.command)

    def test_installed_panel_has_no_install_button(self):
        self.assertIn("gnome-extensions enable", self.found("INACTIVE")[0].fix)
        for state in ("INACTIVE", "ERROR", "OUT OF DATE"):
            self.assertEqual(self.found(state)[0].action, "")
        self.assertIn(system_check.INPUT_PANEL_PAGE, self.found("ERROR")[0].fix)

    def test_running_panel_and_other_desktops_are_clean(self):
        self.assertEqual(self.found("ACTIVE"), [])
        self.assertNotIn(self.TITLE, titles(self.found("", desktop="sway")))

    def test_install_counts_only_when_the_user_accepts(self):
        def fix(stdout, returncode=0):
            done = subprocess.CompletedProcess([], returncode, stdout=stdout)
            with mock.patch.object(system_check.subprocess, "run", return_value=done) as ran:
                result = system_check.run_fix(system_check.INSTALL_INPUT_PANEL, FakeDBusHandler())
            cmd = ran.call_args.args[0]
            self.assertEqual(cmd[cmd.index("--dest") + 1], "org.gnome.Shell")
            return result

        self.assertEqual(fix("('successful',)\n"), (True, None))
        self.assertEqual(fix("('cancelled',)\n"), (False, None))
        self.assertEqual(fix("", returncode=1), (False, None))


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
        found = system_check.evaluate(facts("wayland", "GNOME", env=env, ibus_owns_session=True))
        self.assertEqual(
            titles(found, WARN),
            ["Older X11 apps have no input method", "IBus is running next to fcitx5"],
        )

    def test_gnome_without_app_names_offers_the_extension(self):
        def found(version, state, fcitx="5.1.19", **more):
            f = facts(
                "wayland",
                "ubuntu:GNOME",
                gnome_shell_version=version,
                forward_keys_state=state,
                fcitx_version=fcitx,
                **more,
            )
            return system_check.evaluate(f)

        title = "Some apps cannot keep their own typing mode"
        for version in ("46.0", "48.2", "49.1", "50.4"):
            self.assertEqual(titles(found(version, "INACTIVE"), WARN), [title], version)
        inactive = found("46.0", "INACTIVE", "5.1.7")[0]
        self.assertEqual(inactive.action, system_check.ENABLE_FORWARD_KEYS)
        self.assertIn("log out", inactive.fix)
        # GNOME reads an extension's files only at login, so turning it on now cannot work yet.
        for state in ("", "OUT OF DATE"):
            stale = found("46.0", state)[0]
            self.assertEqual((stale.action, stale.command), ("", ""))
            self.assertIn("Log out", stale.fix)
        running = {"app_names_running": True}
        self.assertEqual(found("46.0", "ACTIVE", **running), [])
        # A package upgrade replaces the files, but the old extension runs until the next login.
        for version in ("46.0", "50.1"):
            old = found(version, "ACTIVE")
            self.assertEqual(titles(old, WARN), [title], version)
            self.assertIn("Log out", old[0].fix)
        self.assertEqual(found("50.1", "ACTIVE", **running), [])
        ok = system_check.steps(
            facts(
                "wayland",
                "GNOME",
                gnome_shell_version="46.0",
                forward_keys_state="ACTIVE",
                fcitx_version="5.1.7",
                app_names_running=True,
            )
        )
        self.assertIn("Apps keep their own typing mode", titles(ok, OK))
        # fcitx5 names apps itself from 5.1.22, and the extension loads only on these GNOME versions.
        self.assertEqual(found("50.4", "INACTIVE", "5.1.22"), [])
        self.assertEqual(found("50.4", "INACTIVE", "5.10.0"), [])
        self.assertEqual(found("45.0", "INACTIVE"), [])
        self.assertEqual(found("51.0", "INACTIVE"), [])
        self.assertEqual(found("50.4", "INACTIVE", ""), [])
        # GNOME 50.0 to 50.3 say the bigger problem, once.
        self.assertEqual(
            titles(found("50.1", "INACTIVE"), WARN),
            ["GNOME 50.1 drops the keys Ngó Sen sends to apps"],
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
        ok = system_check.steps(
            facts("wayland", "GNOME", gnome_shell_version="50.1", forward_keys_state="ACTIVE")
        )
        self.assertIn("The fix for GNOME 50.1 is on", titles(ok, OK))
        self.assertEqual(len(found("50.0", "INACTIVE")), 1)
        self.assertEqual(len(found("50.3", "INACTIVE")), 1)
        self.assertEqual(found("50.4", "INACTIVE"), [])
        self.assertEqual(found("49.2", ""), [])
        self.assertEqual(found("", ""), [])


class StepsTest(unittest.TestCase):
    def test_passed_checks_are_listed_too(self):
        found = system_check.steps(facts("wayland", "GNOME"))
        self.assertEqual(
            titles(found, OK),
            [
                "fcitx5 is running",
                "Ngó Sen is in the fcitx5 input method list",
                "Older X11 apps use fcitx5",
                "Qt apps use fcitx5",
                "Chrome and older GTK apps use fcitx5",
                "Menu and hints show in GNOME's search box",
            ],
        )
        self.assertEqual(titles(found, FAIL) + titles(found, WARN), [])

    def test_steps_the_window_can_fix(self):
        def action(**kw):
            found = system_check.evaluate(facts("wayland", "GNOME", **kw))
            return [(f.action, f.command) for f in found]

        self.assertEqual(action(fcitx_running=False), [("start-fcitx", "fcitx5 -d")])
        self.assertEqual(action(ngosen_in_group=False), [("add-ngosen", "fcitx5-configtool")])
        self.assertEqual(action(addon_replaced=True), [("restart-fcitx", "fcitx5 -rd")])
        self.assertEqual(
            action(gnome_shell_version="50.1", forward_keys_state="INACTIVE"),
            [("enable-forward-keys", f"gnome-extensions enable {system_check.FORWARD_KEYS}")],
        )
        # Not installed: the window has nothing to turn on.
        self.assertEqual(action(gnome_shell_version="50.1", forward_keys_state=""), [("", "")])


class FixTest(unittest.TestCase):
    def test_ngosen_takes_the_place_of_the_only_keyboard(self):
        self.assertEqual(
            system_check.with_ngosen("us", [("keyboard-fr", "")]), ("fr", [("ngosen", "")])
        )

    def test_ngosen_goes_after_other_input_methods(self):
        items = [("keyboard-us", ""), ("pinyin", "")]
        self.assertEqual(system_check.with_ngosen("us", items), ("us", items + [("ngosen", "")]))

    def test_ngosen_is_added_once(self):
        items = [("keyboard-us", ""), ("ngosen", "")]
        self.assertEqual(system_check.with_ngosen("us", items), ("us", items))

    def test_adding_ngosen_can_be_undone(self):
        dbus = FakeDBusHandler()
        dbus.group = ("Default", "us", [("keyboard-us", "")])
        done, undo = system_check.run_fix(system_check.ADD_NGOSEN, dbus)
        self.assertTrue(done)
        self.assertEqual(dbus.input_method_names(), ["ngosen"])
        undo()
        self.assertEqual(dbus.group, ("Default", "us", [("keyboard-us", "")]))

    def test_failed_fix_offers_no_undo(self):
        dbus = FakeDBusHandler()
        dbus.set_input_method_group = lambda *args: False
        self.assertEqual(system_check.run_fix(system_check.ADD_NGOSEN, dbus), (False, None))
        with mock.patch.object(subprocess, "run", side_effect=OSError):
            self.assertEqual(system_check.run_fix(system_check.START_FCITX, dbus), (False, None))

    def test_restart_goes_through_fcitx5(self):
        dbus = FakeDBusHandler()
        self.assertEqual(system_check.run_fix(system_check.RESTART_FCITX, dbus), (True, None))
        self.assertEqual(dbus.restarts, 1)


class FixReviewTest(unittest.TestCase):
    def test_ngosen_dropped_by_fcitx5_is_not_reported_added(self):
        class Drops(FakeDBusHandler):
            def set_input_method_group(self, name, layout, items):
                kept = [item for item in items if item[0] != "ngosen"]
                return super().set_input_method_group(name, layout, kept)

        dbus = Drops()
        dbus.group = ("Default", "us", [("keyboard-us", "")])
        self.assertEqual(system_check.run_fix(system_check.ADD_NGOSEN, dbus), (False, None))
        self.assertEqual(dbus.group, ("Default", "us", [("keyboard-us", "")]))

    def test_started_fcitx5_does_not_hold_the_output_pipes(self):
        with mock.patch.object(subprocess, "run") as run:
            run.return_value.returncode = 0
            system_check.run_fix(system_check.START_FCITX, FakeDBusHandler())
        self.assertEqual(run.call_args.kwargs["stdout"], subprocess.DEVNULL)
        self.assertEqual(run.call_args.kwargs["stderr"], subprocess.DEVNULL)

    def test_kwin_fcitx5_is_not_offered_a_restart(self):
        found = system_check.evaluate(
            facts("wayland", "KDE", env={}, kwin_input_method="fcitx5", addon_replaced=True)
        )
        replaced = next(f for f in found if "before the update" in f.title)
        self.assertEqual(replaced.action, "")
        self.assertIn("Log out", replaced.fix)

    def test_ngosen_is_added_before_fcitx5_restarts(self):
        fixes = system_check.Fixes(FakeDBusHandler())
        ran = []
        with mock.patch.object(
            system_check, "run_fix", side_effect=lambda a, d: ran.append(a) or (True, None)
        ):
            fixes.run([system_check.RESTART_FCITX, system_check.ADD_NGOSEN])
        self.assertEqual(ran, [system_check.ADD_NGOSEN, system_check.RESTART_FCITX])

    def test_a_fix_that_left_its_step_counts_as_failed(self):
        fixes = system_check.Fixes(FakeDBusHandler())
        fixes.busy = {system_check.RESTART_FCITX}
        still = facts("wayland", "GNOME", addon_replaced=True)
        message, undo = fixes.finish([system_check.RESTART_FCITX], [], system_check.evaluate(still))
        self.assertEqual((message, undo), ("", None))
        self.assertEqual(fixes.failed, {system_check.RESTART_FCITX})
        self.assertEqual(fixes.busy, set())

    def test_one_fixable_step_does_not_mention_fixing_all(self):
        found = [
            system_check.Finding(FAIL, "Ngó Sen is missing", action=system_check.ADD_NGOSEN),
            system_check.Finding(WARN, "X11 apps have no input method"),
        ]
        self.assertNotIn("all at once", system_check.summary(found)[1])
        found.append(system_check.Finding(FAIL, "fcitx5 is off", action=system_check.START_FCITX))
        self.assertIn("all at once", system_check.summary(found)[1])

    def test_failed_fixes_do_not_count_as_buttons(self):
        found = [
            system_check.Finding(FAIL, "Ngó Sen is missing", action=system_check.ADD_NGOSEN),
            system_check.Finding(FAIL, "fcitx5 is off", action=system_check.START_FCITX),
        ]
        one_failed = system_check.summary(found, {system_check.ADD_NGOSEN})[1]
        self.assertNotIn("all at once", one_failed)
        all_failed = system_check.summary(
            found, {system_check.ADD_NGOSEN, system_check.START_FCITX}
        )
        self.assertEqual(all_failed[1], "Each step says what to do.")

    def test_a_failed_fix_keeps_the_instructions(self):
        found = system_check.steps(facts("wayland", "KDE", ngosen_in_group=False))
        add = next(f for f in found if f.action == system_check.ADD_NGOSEN)
        self.assertIn(add.fix, system_check.failed_text(add))

    def test_undo_skips_fixes_that_did_not_take(self):
        fixes = system_check.Fixes(FakeDBusHandler())
        undone = []

        def run_fix(action, _dbus):
            return True, lambda: undone.append(action) or True

        with mock.patch.object(system_check, "run_fix", side_effect=run_fix):
            done, undos, _settle = fixes.run(
                [system_check.ADD_NGOSEN, system_check.ENABLE_FORWARD_KEYS]
            )
        left = [system_check.Finding(FAIL, "off", action=system_check.ENABLE_FORWARD_KEYS)]
        _message, undo = fixes.finish(done, undos, left)
        undo()
        self.assertEqual(undone, [system_check.ADD_NGOSEN])

    def test_a_slow_fix_is_checked_again_before_it_fails(self):
        fixes = system_check.Fixes(FakeDBusHandler())
        not_running = system_check.steps(facts("wayland", "GNOME", fcitx_running=False))
        restart = [system_check.RESTART_FCITX]
        self.assertGreater(fixes.settle_again(restart, not_running), 0)
        self.assertEqual(fixes.settle_again([system_check.ADD_NGOSEN], not_running), 0)
        extension = [system_check.ENABLE_FORWARD_KEYS]
        still_off = [system_check.Finding(FAIL, "off", action=system_check.ENABLE_FORWARD_KEYS)]
        self.assertGreater(fixes.settle_again(extension, still_off), 0)
        while fixes.settle_again(extension, still_off):
            pass
        self.assertEqual(fixes.settle_again(extension, []), 0)

    def test_ngosen_installed_after_fcitx5_asks_for_a_restart(self):
        found = system_check.steps(
            facts("wayland", "GNOME", ngosen_in_group=False, ngosen_loaded=False)
        )
        actions = [f.action for f in found]
        self.assertNotIn(system_check.ADD_NGOSEN, actions)
        self.assertIn(system_check.RESTART_FCITX, actions)
        kde = system_check.steps(
            facts("wayland", "KDE", ngosen_in_group=False, ngosen_loaded=False)
        )
        self.assertNotIn(system_check.RESTART_FCITX, [f.action for f in kde])
        self.assertTrue(any("Log out" in f.fix for f in kde))


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

        def run(cmd, env=None, **kw):
            out = outputs.get(cmd[0], "")
            # gnome-extensions translates its labels, but not the state itself.
            if (env or {}).get("LC_ALL") != "C":
                out = out.replace("State:", "Trạng thái:")
            return subprocess.CompletedProcess(cmd, 0, stdout=out)

        env = {"XDG_CURRENT_DESKTOP": "ubuntu:GNOME"}
        with mock.patch.object(system_check.subprocess, "run", side_effect=run):
            f = system_check.collect(FakeDBusHandler(), env=env)
        self.assertEqual((f.gnome_shell_version, f.forward_keys_state), ("50.1", "INACTIVE"))
        self.assertFalse(f.app_names_running)
        outputs["gdbus"] = "()\n"
        with mock.patch.object(system_check.subprocess, "run", side_effect=run):
            self.assertTrue(system_check.collect(FakeDBusHandler(), env=env).app_names_running)
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

    def test_ibus_counts_only_when_it_holds_this_session(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        proc = os.path.join(tmp.name, "proc")
        for pid, comm in (("100", "ibus-daemon"), ("200", "fcitx5")):
            os.makedirs(os.path.join(proc, pid))
            with open(os.path.join(proc, pid, "comm"), "w", encoding="utf-8") as f:
                f.write(comm + "\n")
        bus = os.path.join(tmp.name, "ibus", "bus")
        os.makedirs(bus)

        def address(name, pid):
            with open(os.path.join(bus, "m1-" + name), "w", encoding="utf-8") as f:
                f.write(f"# IBus address file\nIBUS_ADDRESS=unix:path=/x\nIBUS_DAEMON_PID={pid}\n")

        def owns(**env):
            env["XDG_CONFIG_HOME"] = tmp.name
            return system_check.ibus_owns_session(env, proc=proc, machine_id="m1")

        address("unix-wayland-0", 200)
        address("unix-100", 100)
        # Another program's IBus on its own display, as xpra starts one.
        self.assertFalse(owns(WAYLAND_DISPLAY="wayland-0"))
        address("unix-wayland-0", 100)
        self.assertTrue(owns(WAYLAND_DISPLAY="wayland-0"))
        address("unix-0", 100)
        self.assertTrue(owns(DISPLAY=":0.0"))
        address("unix-0", 999)
        self.assertFalse(owns(DISPLAY=":0"))
        self.assertFalse(owns(DISPLAY=":5"))
        # IBus reads IBUS_ADDRESS_FILE instead of the usual name when it is set.
        self.assertTrue(
            owns(DISPLAY=":5", IBUS_ADDRESS_FILE=os.path.join(bus, "m1-unix-wayland-0"))
        )
        with open(os.path.join(bus, "m1-unix-1"), "w", encoding="utf-8") as f:
            f.write("IBUS_ADDRESS=unix:path=/x\n")
        self.assertFalse(owns(DISPLAY=":1"))
        home = {"HOME": os.path.join(tmp.name, "home"), "WAYLAND_DISPLAY": "wayland-0"}
        os.makedirs(os.path.join(home["HOME"], ".config"))
        os.symlink(os.path.join(tmp.name, "ibus"), os.path.join(home["HOME"], ".config", "ibus"))
        self.assertTrue(system_check.ibus_owns_session(home, proc=proc, machine_id="m1"))

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


class CheckPageTest(unittest.TestCase):
    NO_IM = {"XDG_SESSION_TYPE": "x11", "XDG_CURRENT_DESKTOP": "XFCE"}
    TITLE = "Ngó Sen is not in the fcitx5 input method list"

    def setUp(self):
        from ui.pages.check import CheckPage

        # The fcitx5 and IBus of the machine running the tests must not add a row.
        for name in ("runs_replaced_addon", "ibus_owns_session"):
            patcher = mock.patch.object(system_check, name, return_value=False)
            patcher.start()
            self.addCleanup(patcher.stop)
        env = mock.patch.dict(os.environ, self.NO_IM, clear=True)
        env.start()
        self.addCleanup(env.stop)
        self.dbus = FakeDBusHandler()
        self.messages = []
        self.counts = []
        self.page = CheckPage(
            self.dbus,
            "1.4.2",
            lambda text, undo: self.messages.append((text, undo)),
            self.counts.append,
        )

    def row(self, title):
        return next(row for row in self.page.rows if row.title == title)

    def settle(self):
        from qtpy.QtWidgets import QApplication

        while self.page.fixes.busy:
            QApplication.processEvents()

    def test_lists_every_step_and_copies_the_report(self):
        from qtpy.QtGui import QGuiApplication

        levels = [row.level for row in self.page.rows]
        self.assertEqual(levels[:2], [OK, OK])
        self.assertIn(FAIL, levels)
        self.assertEqual(self.counts[-1], 3)
        self.assertIn("kate (wayland)", self.page.apps.text())
        fixed = dict(self.NO_IM, GTK_IM_MODULE="fcitx", QT_IM_MODULE="fcitx")
        with mock.patch.dict(os.environ, dict(fixed, XMODIFIERS="@im=fcitx"), clear=True):
            self.page.run()
        self.assertEqual({row.level for row in self.page.rows}, {OK})
        self.assertEqual(self.counts[-1], 0)
        self.assertEqual(self.page.heading.text(), "This machine is ready")
        self.page.run()
        self.page.copy_btn.click()
        self.assertIn("[X] GTK apps have no input method", QGuiApplication.clipboard().text())
        self.assertEqual(self.messages[-1][0], "Copied the result.")

    def test_adding_ngosen_fixes_the_step_and_can_be_undone(self):
        self.dbus.group = ("Default", "us", [("keyboard-us", "")])
        self.page.run()
        self.row(self.TITLE).button.click()
        self.assertEqual(self.page.heading.text(), "Fixing…")
        self.settle()
        self.assertEqual(self.dbus.input_method_names(), ["ngosen"])
        self.assertNotIn(self.TITLE, [row.title for row in self.page.rows])
        text, undo = self.messages[-1]
        self.assertEqual(text, "Added Ngó Sen to the input method list.")
        undo()
        self.assertEqual(self.dbus.input_method_names(), ["keyboard-us"])
        self.assertIn(self.TITLE, [row.title for row in self.page.rows])

    def test_a_failed_fix_offers_its_command(self):
        from qtpy.QtGui import QGuiApplication

        self.dbus.group = ("Default", "us", [("keyboard-us", "")])
        self.dbus.set_input_method_group = lambda *args: False
        self.page.run()
        self.row(self.TITLE).button.click()
        self.settle()
        button = self.row(self.TITLE).button
        self.assertEqual(button.text(), "Copy command")
        self.assertEqual(self.messages, [])
        button.click()
        self.assertEqual(QGuiApplication.clipboard().text(), "fcitx5-configtool")

    def test_messaging_copies_the_report_and_opens_the_chat(self):
        from core.brand import MESSENGER_URL
        from qtpy.QtGui import QGuiApplication

        QGuiApplication.clipboard().setText("")
        with mock.patch("ui.pages.check.QDesktopServices.openUrl") as open_url:
            self.page.message_btn.click()
        self.assertEqual(open_url.call_args[0][0].toString(), MESSENGER_URL)
        self.assertIn("Ngó Sen 1.4.2", QGuiApplication.clipboard().text())
        self.assertIn("Paste", self.messages[-1][0])


class CheckPageWindowTest(unittest.TestCase):
    def setUp(self):
        for name in ("runs_replaced_addon", "ibus_owns_session"):
            patcher = mock.patch.object(system_check, name, return_value=False)
            patcher.start()
            self.addCleanup(patcher.stop)
        env = mock.patch.dict(os.environ, CheckPageTest.NO_IM, clear=True)
        env.start()
        self.addCleanup(env.stop)

    def window(self, dbus):
        from ui.main_window import NgoSenSettingsWindow

        window = NgoSenSettingsWindow(dbus_handler=dbus)
        self.addCleanup(window.close)
        return window

    def test_sits_above_about_with_the_steps_left(self):
        window = self.window(FakeDBusHandler())
        titles = [window.sidebar.item(r).text() for r in range(window.sidebar.count())]
        self.assertEqual(titles[-2:], ["Check my machine", "About"])
        page = window.show_page("check")
        self.assertEqual(window.check_badge.text(), "3")
        self.assertFalse(window.check_badge.isHidden())
        with mock.patch.object(page, "run") as run:
            window.show_page("about")
            window.show_page("check")
        run.assert_called_once()

    def test_badge_shows_before_the_page_is_opened(self):
        window = self.window(FakeDBusHandler())
        app.processEvents()
        self.assertEqual(window.check_badge.text(), "3")
        self.assertFalse(window.check_badge.isHidden())

    def test_coming_back_after_a_message_checks_again(self):
        window = self.window(FakeDBusHandler())
        page = window.show_page("check")
        window.show_page("about")
        window._show_message("Saved.")
        with mock.patch.object(page, "run") as run:
            window.show_page("check")
        run.assert_called_once()

    def test_a_failed_undo_says_so(self):
        window = self.window(FakeDBusHandler())
        window.show_check_message("Added.", lambda: False)
        window.btn_undo.click()
        self.assertIn("Could not undo", window.message_label.text())

    def test_undo_takes_back_the_fix(self):
        dbus = FakeDBusHandler()
        dbus.group = ("Default", "us", [("keyboard-us", "")])
        window = self.window(dbus)
        page = window.show_page("check")
        page.fix([system_check.ADD_NGOSEN])
        while page.fixes.busy:
            app.processEvents()
        self.assertFalse(window.btn_undo.isHidden())
        window.btn_undo.click()
        self.assertEqual(dbus.input_method_names(), ["keyboard-us"])
        self.assertEqual(window.message_label.text(), "Change undone.")
        self.assertTrue(window.btn_undo.isHidden())


if __name__ == "__main__":
    unittest.main()
