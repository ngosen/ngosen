# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

# fake_fcitx puts the settings window on sys.path, so it has to come first.
# isort: off
from fake_fcitx import DEFAULTS, FakeDBusHandler
from core import defaults, dictionary

# isort: on


class RestoreTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        bundled = os.path.join(tmp.name, "bundled.dict")
        with open(bundled, "w", encoding="utf-8") as f:
            f.write("hello\n")
        for patcher in (
            mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name}),
            mock.patch.object(dictionary, "BUNDLED_PATHS", [bundled]),
        ):
            patcher.start()
            self.addCleanup(patcher.stop)
        self.dbus = FakeDBusHandler({"SpellCheck": "False", "SettingsToolkit": "GTK"})

    def test_settings_go_back_to_their_defaults(self):
        self.assertTrue(defaults.restore(self.dbus))
        self.assertEqual(self.dbus.values["SpellCheck"], DEFAULTS["SpellCheck"])

    def test_the_chosen_settings_window_stays(self):
        # Going back to Qt could open a window whose libraries were never installed.
        defaults.restore(self.dbus)
        self.assertEqual(self.dbus.values["SettingsToolkit"], "GTK")


if __name__ == "__main__":
    unittest.main()
