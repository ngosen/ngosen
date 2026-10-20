#!/usr/bin/env python3
"""Tests for the ngosen-save-log command's messages."""

import os
import sys
import unittest

import dbus

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import save_log  # noqa: E402


class FakeProxy:
    def __init__(self, result):
        self.result = result

    def get_dbus_method(self, member, dbus_interface=None):
        def call():
            if isinstance(self.result, Exception):
                raise self.result
            return dbus.String(self.result)

        return call


class FakeBus:
    def __init__(self, result=None, running=True):
        self.result = result
        self.running = running
        self.called = False

    def name_has_owner(self, name):
        return self.running

    def get_object(self, service, path, introspect=True):
        self.called = True
        return FakeProxy(self.result)


class SaveLogTest(unittest.TestCase):
    def test_prints_the_saved_path(self):
        code, message = save_log.save(FakeBus("/home/u/.local/state/ngosen/typing-1.log"))
        self.assertEqual(code, 0)
        self.assertTrue(message.endswith("/home/u/.local/state/ngosen/typing-1.log"))

    def test_does_not_start_fcitx5(self):
        bus = FakeBus(running=False)
        code, message = save_log.save(bus)
        self.assertEqual((code, message), (1, "fcitx5 is not running."))
        self.assertFalse(bus.called)

    def test_engine_not_loaded(self):
        error = dbus.DBusException("no object", name="org.freedesktop.DBus.Error.UnknownObject")
        code, message = save_log.save(FakeBus(error))
        self.assertEqual(code, 1)
        self.assertIn("not loaded", message)

    def test_passes_on_the_engine_error(self):
        error = dbus.DBusException(
            "Could not save the typing log to /x", name="org.freedesktop.DBus.Error.Failed"
        )
        self.assertEqual(save_log.save(FakeBus(error)), (1, "Could not save the typing log to /x"))


if __name__ == "__main__":
    unittest.main()
