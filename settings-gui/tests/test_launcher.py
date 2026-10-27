# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import ast
import subprocess
import sys
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
        self.assertEqual(main.toolkit({}), "qt")
        self.assertEqual(main.toolkit({"NGOSEN_SETTINGS_TOOLKIT": "qt"}), "qt")


if __name__ == "__main__":
    unittest.main()
