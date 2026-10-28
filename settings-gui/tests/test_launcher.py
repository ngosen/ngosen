# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import ast
import pathlib
import subprocess
import sys
import tempfile
import unittest

import fake_fcitx  # noqa: F401  (puts settings-gui on the path)
import main
from test_core_toolkit_free import GUI_DIR, import_without_toolkits


class LauncherTest(unittest.TestCase):
    def test_the_launcher_loads_no_toolkit_before_picking_one(self):
        result = import_without_toolkits(["main"], [GUI_DIR])
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_qt_api_comes_before_anything_from_qt(self):
        tree = ast.parse((GUI_DIR / "main.py").read_text(encoding="utf-8"))
        run_qt = next(n for n in tree.body if getattr(n, "name", "") == "run_qt")
        first = run_qt.body[0]
        self.assertIsInstance(first, ast.Import)
        self.assertEqual(first.names[0].name, "qt_api")

    def test_probing_for_gtk_leaves_it_out_of_this_process(self):
        # Qt may still open here, and its GTK 3 theme plugin aborts next to a loaded GTK 4.
        probe = "import sys, ui_gtk; ui_gtk.available(); print('gi.repository.Gtk' in sys.modules)"
        result = subprocess.run(
            [sys.executable, "-c", probe], cwd=GUI_DIR, capture_output=True, text=True
        )
        self.assertEqual(result.stdout.strip(), "False", result.stderr)

    def test_qt_unless_the_developer_asks_for_gtk(self):
        self.assertEqual(main.toolkit({}, qt=yes), "qt")
        self.assertEqual(main.toolkit({"NGOSEN_SETTINGS_TOOLKIT": "qt"}, qt=yes), "qt")

    def test_the_saved_choice_opens_when_it_is_installed(self):
        env = saved_toolkit(self, "GTK")
        self.assertEqual(main.toolkit(env, qt=yes, gtk=yes), "gtk")
        self.assertEqual(main.toolkit(env, qt=yes, gtk=no), "qt")

    def test_the_developer_variable_beats_the_saved_choice(self):
        env = saved_toolkit(self, "GTK")
        env["NGOSEN_SETTINGS_TOOLKIT"] = "qt"
        self.assertEqual(main.toolkit(env, qt=yes, gtk=yes), "qt")

    def test_with_no_choice_gnome_and_cinnamon_open_gtk(self):
        for desktop in ("ubuntu:GNOME", "X-Cinnamon"):
            env = {"XDG_CURRENT_DESKTOP": desktop}
            self.assertEqual(main.toolkit(env, qt=yes, gtk=yes), "gtk", desktop)
        self.assertEqual(main.toolkit({"XDG_CURRENT_DESKTOP": "KDE"}, qt=yes, gtk=yes), "qt")

    def test_a_session_started_before_its_desktop_still_opens_gtk(self):
        for key, session in (
            ("DESKTOP_SESSION", "cinnamon"),
            ("XDG_SESSION_DESKTOP", "ubuntu-xorg"),
        ):
            self.assertEqual(main.toolkit({key: session}, qt=yes, gtk=yes), "gtk", session)
        self.assertEqual(main.toolkit({"DESKTOP_SESSION": "plasma"}, qt=yes, gtk=yes), "qt")
        env = {"XDG_CURRENT_DESKTOP": "KDE", "DESKTOP_SESSION": "cinnamon"}
        self.assertEqual(main.toolkit(env, qt=yes, gtk=yes), "qt")

    def test_a_picked_qt_stays_on_gnome(self):
        env = saved_toolkit(self, "Qt")
        env["XDG_CURRENT_DESKTOP"] = "GNOME"
        self.assertEqual(main.toolkit(env, qt=yes, gtk=yes), "qt")

    def test_a_machine_with_only_gtk_opens_gtk(self):
        self.assertEqual(main.toolkit({}, qt=no, gtk=yes), "gtk")

    def test_qt_needs_qtpy_and_a_binding(self):
        def found(*names):
            return lambda module: module in names

        self.assertTrue(main.has_qt(found("qtpy", "PySide6")))
        self.assertFalse(main.has_qt(found("PyQt6")))
        self.assertFalse(main.has_qt(found("qtpy")))


def yes():
    return True


def no():
    return False


def saved_toolkit(test, value):
    home = tempfile.TemporaryDirectory()
    test.addCleanup(home.cleanup)
    conf = pathlib.Path(home.name, "fcitx5", "conf")
    conf.mkdir(parents=True)
    (conf / "ngosen.conf").write_text(f"UiLanguage=English\nSettingsToolkit={value}\n")
    return {"XDG_CONFIG_HOME": home.name}


if __name__ == "__main__":
    unittest.main()
