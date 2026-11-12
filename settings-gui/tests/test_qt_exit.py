# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import subprocess
import sys
import unittest

from test_core_toolkit_free import GUI_DIR

TESTS_DIR = GUI_DIR / "tests"

# A child process, since the crash comes only when the interpreter exits.
OPEN_AND_CLOSE = """
import sys
import fake_fcitx
import main
import qt_api
from qtpy.QtCore import QTimer
import ui.main_window

destroyed = []


class Window(ui.main_window.NgoSenSettingsWindow):
    def __init__(self):
        super().__init__(fake_fcitx.FakeDBusHandler())
        self.destroyed.connect(lambda: destroyed.append(True))
        QTimer.singleShot(0, self.close)


ui.main_window.NgoSenSettingsWindow = Window
sys.argv = ["main.py"] + sys.argv[1:]
main.run_qt()
print("destroyed" if destroyed else "alive")
"""


@unittest.skipUnless(os.environ.get("QT_QPA_PLATFORM") == "offscreen", "needs offscreen Qt")
class QtExitTest(unittest.TestCase):
    def close_window(self, *args):
        env = dict(os.environ, PYTHONPATH=os.pathsep.join(map(str, (GUI_DIR, TESTS_DIR))))
        env["NGOSEN_SETTINGS_TOOLKIT"] = "qt"
        return subprocess.run(
            [sys.executable, "-c", OPEN_AND_CLOSE, *args],
            env=env,
            capture_output=True,
            text=True,
            timeout=60,
        )

    def test_closing_the_window_deletes_it_before_python_exits(self):
        for args in ([], ["--page", "whats-new"]):
            with self.subTest(args=args):
                result = self.close_window(*args)
                self.assertEqual(result.returncode, 0, result.stderr[-2000:])
                self.assertEqual(result.stdout.strip(), "destroyed", result.stderr[-2000:])


if __name__ == "__main__":
    unittest.main()
