#!/usr/bin/env python3
"""Tests for the ngosen-check command."""

import contextlib
import io
import os
import sys
import unittest
from unittest import mock

import dbus

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import check  # noqa: E402
from core.system_check import FAIL, OK, Facts, Finding  # noqa: E402
from core.typing_log import AppTest  # noqa: E402

from core import system_check  # noqa: E402


class FakeBus:
    def __init__(self, running=True):
        self.running = running
        self.objects = []

    def get_name_owner(self, name):
        if not self.running:
            raise dbus.DBusException("no owner", name="org.freedesktop.DBus.Error.NameHasNoOwner")
        return ":1.42"

    def get_object(self, service, path, introspect=True):
        self.objects.append(service)
        return object()


def sample_facts():
    f = Facts(env={"XDG_SESSION_TYPE": "wayland", "XDG_CURRENT_DESKTOP": "GNOME"})
    f.ngosen_version = "1.13.1"
    f.apps = [("org.telegram.desktop", "wayland")]
    return f


FOUND = [
    Finding(OK, "fcitx5 is running", "unused fix"),
    Finding(
        FAIL, "Qt apps have no input method", "Add QT_IM_MODULE=fcitx.", "env", "printf x >> f"
    ),
]


class TerminalReportTest(unittest.TestCase):
    def test_starts_with_the_guide_and_gives_each_fix(self):
        text = system_check.terminal_report(sample_facts(), FOUND)
        self.assertTrue(text.startswith("# Ngó Sen check. Guide for AI assistants: https://"))
        self.assertIn("[X] Qt apps have no input method\n    Add QT_IM_MODULE=fcitx.", text)
        self.assertNotIn("unused fix", text)

    def test_leaves_out_the_fallback_command(self):
        self.assertNotIn("printf x", system_check.terminal_report(sample_facts(), FOUND))

    def test_names_apps_only_when_asked(self):
        f = sample_facts()
        self.assertNotIn("telegram", system_check.terminal_report(f, FOUND))
        self.assertIn(
            "Apps: org.telegram.desktop (wayland)",
            system_check.terminal_report(f, FOUND, include_apps=True),
        )

    def test_never_holds_typed_text(self):
        f = sample_facts()
        test = AppTest(path="/x.log", app_text="mật khẩu ngân hàng")
        self.assertIn("mật khẩu", system_check.report(f, FOUND, test))
        for include_apps in (False, True):
            text = system_check.terminal_report(f, FOUND, include_apps)
            self.assertNotIn("App test", text)
            self.assertNotIn("mật khẩu", text)


class RunningFcitxHandlerTest(unittest.TestCase):
    def test_calls_the_running_fcitx5_by_its_unique_name(self):
        bus = FakeBus()
        self.assertIsNotNone(check.RunningFcitxHandler(bus).iface)
        self.assertEqual(bus.objects, [":1.42"])

    def test_does_not_start_fcitx5(self):
        bus = FakeBus(running=False)
        handler = check.RunningFcitxHandler(bus)
        self.assertIsNone(handler.iface)
        self.assertIsNone(handler.input_method_names())
        self.assertEqual(bus.objects, [])

    def test_no_session_bus(self):
        self.assertIsNone(check.RunningFcitxHandler(None).input_method_names())


class CheckTest(unittest.TestCase):
    def test_stdout_holds_only_the_report(self):
        def collect(handler, version, env):
            print("Fcitx5 D-Bus Error: noise")
            return sample_facts()

        out, err = io.StringIO(), io.StringIO()
        # The language picked on this machine would leak into later tests.
        with (
            mock.patch.object(check, "setup_i18n"),
            mock.patch.object(check.dbus, "SessionBus", return_value=FakeBus()),
            mock.patch.object(system_check, "collect", side_effect=collect),
        ):
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                self.assertEqual(check.main([]), check.NOT_RUNNING)
        self.assertTrue(out.getvalue().startswith("# Ngó Sen check."))
        self.assertNotIn("noise", out.getvalue())
        self.assertIn("noise", err.getvalue())

    def test_exit_codes(self):
        running = Facts(env={}, fcitx_running=True)
        not_loaded = Facts(env={}, fcitx_running=True, ngosen_loaded=False)
        self.assertEqual(check.exit_code(FakeBus(), running), 0)
        self.assertEqual(check.exit_code(None, Facts(env={})), check.NO_BUS)
        self.assertEqual(check.exit_code(FakeBus(), Facts(env={})), check.NOT_RUNNING)
        self.assertEqual(check.exit_code(FakeBus(), not_loaded), check.NOT_LOADED)

    def test_no_session_bus_still_prints_the_report(self):
        out, err = io.StringIO(), io.StringIO()
        with (
            mock.patch.object(check, "setup_i18n"),
            mock.patch.object(check.dbus, "SessionBus", side_effect=dbus.DBusException("none")),
            mock.patch.object(system_check, "collect", return_value=sample_facts()),
        ):
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                self.assertEqual(check.main([]), check.NO_BUS)
        self.assertTrue(out.getvalue().startswith("# Ngó Sen check."))
        self.assertIn("No desktop session bus", err.getvalue())


if __name__ == "__main__":
    unittest.main()
