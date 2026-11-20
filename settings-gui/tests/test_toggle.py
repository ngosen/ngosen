#!/usr/bin/env python3
"""Tests for the ngosen-toggle command a desktop shortcut runs."""

import os
import sys
import unittest

import dbus

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import toggle  # noqa: E402


class FakeProxy:
    def __init__(self, bus):
        self.bus = bus

    def get_dbus_method(self, member, dbus_interface=None):
        def call(*args, timeout=None):
            self.bus.calls.append((dbus_interface, member, args))
            if dbus_interface == toggle.INTERFACE and isinstance(self.bus.result, Exception):
                raise self.bus.result
            return dbus.Boolean(True)

        return call


class FakeBus:
    def __init__(self, result=None, running=True):
        self.result = result
        self.running = running
        self.calls = []

    def get_name_owner(self, name):
        if not self.running:
            raise dbus.DBusException("no owner", name="org.freedesktop.DBus.Error.NameHasNoOwner")
        return ":1.42"

    def get_object(self, service, path, introspect=True):
        return FakeProxy(self)


def error(name):
    return dbus.DBusException("failed", name=name)


class ToggleTest(unittest.TestCase):
    def test_switches_quietly(self):
        bus = FakeBus()
        self.assertEqual(toggle.toggle(bus), (0, ""))
        self.assertEqual(bus.calls, [(toggle.INTERFACE, "ToggleVietnamese", ())])

    def test_does_not_start_fcitx5(self):
        bus = FakeBus(running=False)
        self.assertEqual(toggle.toggle(bus), (1, "fcitx5 is not running."))
        self.assertEqual(bus.calls, [])

    def test_engine_not_loaded(self):
        code, message = toggle.toggle(FakeBus(error("org.freedesktop.DBus.Error.UnknownObject")))
        self.assertEqual(code, 1)
        self.assertIn("not loaded", message)

    def test_engine_from_before_the_upgrade(self):
        for name in toggle.OLD_ENGINE:
            code, message = toggle.toggle(FakeBus(error(name)))
            self.assertEqual(code, 1)
            self.assertIn("updated", message)

    def test_a_failure_is_shown_on_the_desktop(self):
        bus = FakeBus()
        toggle.notify(bus, "fcitx5 is not running.")
        interface, member, args = bus.calls[0]
        self.assertEqual((interface, member), ("org.freedesktop.Notifications", "Notify"))
        self.assertIn("fcitx5 is not running.", args)


if __name__ == "__main__":
    unittest.main()
