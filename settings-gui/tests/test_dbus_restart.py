# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import subprocess
import sys
import tempfile
import unittest

from fcitx5_service import AVAILABLE, GUI_DIR, PrivateFcitx5


@unittest.skipUnless(AVAILABLE, "needs dbus-daemon and gi")
class Fcitx5RestartTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.fcitx5 = PrivateFcitx5(self.tmp.name)
        self.addCleanup(self.fcitx5.close)

    def test_saves_to_fcitx5_started_after_the_window(self):
        first = os.path.join(self.tmp.name, "first")
        second = os.path.join(self.tmp.name, "second")
        self.fcitx5.start(first)
        script = (
            "import sys; sys.path.insert(0, sys.argv[1]);"
            "from core.dbus_handler import NgoSenDBusHandler;"
            "h = NgoSenDBusHandler(); print(bool(h.get_config()), flush=True);"
            "sys.stdin.readline();"
            "print(h.set_config({'SpellCheck': 'False'}), flush=True);"
            "print(h.set_sub_config_list('ngosen-macro', 'Macro', [{'Key': 'vn'}]), flush=True)"
        )
        window = subprocess.Popen(
            [sys.executable, "-c", script, GUI_DIR],
            env=self.fcitx5.env,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(window.stdout.readline().strip(), "True")
        self.fcitx5.start(second)
        out, _ = window.communicate("go\n", timeout=10)
        self.assertEqual(out.split(), ["True", "True"])
        keys = [sorted(call["values"]) for call in self.fcitx5.set_config_calls(second)]
        self.assertEqual(keys, [["SpellCheck"], ["Macro"]])
        self.assertEqual(self.fcitx5.set_config_calls(first), [])


if __name__ == "__main__":
    unittest.main()
