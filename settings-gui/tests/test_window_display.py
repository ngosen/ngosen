# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""GTK window tests must not open windows on the desktop of whoever runs them."""

import os
import subprocess
import sys
import unittest

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
GUI_DIR = os.path.dirname(TESTS_DIR)


def probe(code, **env):
    """Runs code from the tests folder in a desktop session that did not ask for window tests."""
    base = {k: v for k, v in os.environ.items() if k not in ("NGOSEN_WINDOW_TESTS", "GDK_BACKEND")}
    base.update(DISPLAY=os.environ.get("DISPLAY", ":0"), WAYLAND_DISPLAY="wayland-0")
    base.update(PYTHONPATH=os.pathsep.join((TESTS_DIR, GUI_DIR)), **env)
    result = subprocess.run(
        [sys.executable, "-c", code],
        cwd=TESTS_DIR,
        env=base,
        capture_output=True,
        text=True,
        timeout=60,
    )
    if result.returncode != 0:
        raise AssertionError(result.stderr[-2000:])
    return result.stdout.split()


class WindowDisplayTest(unittest.TestCase):
    def test_no_window_without_opt_in(self):
        self.assertEqual(
            probe(
                "import test_gtk_emoji, test_gtk_window; print(test_gtk_emoji.CAN_RUN, test_gtk_window.CAN_RUN)"
            ),
            ["False", "False"],
        )

    def test_opt_in_draws_on_x11(self):
        # xvfb-run passes WAYLAND_DISPLAY on, and GTK picks Wayland first.
        code = "import os, window_display; print(os.environ.get('GDK_BACKEND'), 'WAYLAND_DISPLAY' in os.environ)"
        self.assertEqual(probe(code, NGOSEN_WINDOW_TESTS="1"), ["x11", "False"])


if __name__ == "__main__":
    unittest.main()
