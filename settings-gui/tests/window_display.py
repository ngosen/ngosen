# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""GTK window tests run only when asked, on the X server xvfb-run starts, never on the desktop.

Run them with: xvfb-run -a env NGOSEN_WINDOW_TESTS=1 python3 -m unittest test_gtk_window
"""

import os

ENABLED = bool(os.environ.get("NGOSEN_WINDOW_TESTS"))
if ENABLED:
    # xvfb-run passes WAYLAND_DISPLAY on, and GTK picks Wayland first.
    os.environ["GDK_BACKEND"] = "x11"
    os.environ.pop("WAYLAND_DISPLAY", None)


def can_show():
    return ENABLED and bool(os.environ.get("DISPLAY"))
