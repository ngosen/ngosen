# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The whole path a user takes: open the window, change a setting, close it, fcitx5 has it.

The window tests elsewhere use an in-process fake that skips D-Bus, so a wrong wire format (#303)
or a crash on exit (#729) passes them.
"""

import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest

from fcitx5_service import AVAILABLE, GUI_DIR, PrivateFcitx5

ADDON = "fcitx://config/addon/ngosen"
REQUIRED = os.environ.get("NGOSEN_WINDOW_TESTS") == "required"

# An exception in a toolkit callback only prints, and the window would then stay open.
QT_FLIP_AND_CLOSE = """
import os
import traceback

import main
import qt_api
from qtpy.QtCore import QTimer
import ui.main_window


class Window(ui.main_window.NgoSenSettingsWindow):
    def __init__(self):
        super().__init__()
        QTimer.singleShot(0, self.flip_and_close)

    def flip_and_close(self):
        try:
            self.open_setting("SpellCheck")
            pages = (p for p in self._pages if hasattr(p, "control_for"))
            next(c for c in (p.control_for("SpellCheck") for p in pages) if c).click()
            self.close()
        except Exception:
            traceback.print_exc()
            os._exit(3)


ui.main_window.NgoSenSettingsWindow = Window
main.run_qt()
"""

GTK_FLIP_AND_CLOSE = """
import os
import traceback

import main
from gi.repository import GLib
from ui_gtk import window


class Window(window.Window):
    def __init__(self, *args):
        super().__init__(*args)
        GLib.idle_add(self.flip_and_close)

    def flip_and_close(self):
        try:
            row = self.typing.rows["SpellCheck"]
            row.set_active(not row.get_active())
            self.close()
        except Exception:
            traceback.print_exc()
            os._exit(3)
        return False


window.Window = Window
raise SystemExit(main.run_gtk())
"""


def has_qt():
    return bool(importlib.util.find_spec("PyQt6") or importlib.util.find_spec("PyQt5"))


def has_gtk():
    sys.path.insert(0, GUI_DIR)
    try:
        import ui_gtk

        return ui_gtk.available() and bool(os.environ.get("DISPLAY"))
    finally:
        sys.path.remove(GUI_DIR)


class SaveJourney:
    script = ""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.fcitx5 = PrivateFcitx5(tmp.name)
        self.addCleanup(self.fcitx5.close)
        self.fcitx5.start()
        self.env = dict(
            self.fcitx5.env,
            PYTHONPATH=GUI_DIR,
            XDG_CONFIG_HOME=os.path.join(tmp.name, "config"),
            XDG_DATA_HOME=os.path.join(tmp.name, "data"),
        )

    def test_a_changed_setting_reaches_fcitx5(self):
        result = subprocess.run(
            [sys.executable, "-c", self.script],
            env=self.env,
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(result.returncode, 0, result.stderr[-2000:])
        saved = [c["values"] for c in self.fcitx5.set_config_calls() if c["uri"] == ADDON]
        self.assertTrue(saved, "no SetConfig reached fcitx5: " + result.stdout[-2000:])
        self.assertEqual(saved[-1].get("SpellCheck"), "False", saved[-1])


@unittest.skipUnless(AVAILABLE and has_qt(), "needs dbus-daemon, gi and PyQt")
class QtSaveJourneyTest(SaveJourney, unittest.TestCase):
    script = QT_FLIP_AND_CLOSE

    def setUp(self):
        super().setUp()
        self.env.update(NGOSEN_SETTINGS_TOOLKIT="qt", QT_QPA_PLATFORM="offscreen")


@unittest.skipUnless(REQUIRED or (AVAILABLE and has_gtk()), "needs dbus-daemon, GTK and a display")
class GtkSaveJourneyTest(SaveJourney, unittest.TestCase):
    script = GTK_FLIP_AND_CLOSE

    def setUp(self):
        super().setUp()
        self.env.update(NGOSEN_SETTINGS_TOOLKIT="gtk")


if __name__ == "__main__":
    unittest.main()
