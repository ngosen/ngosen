# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The GTK 4 and libadwaita settings window."""

import subprocess
import sys

MIN_ADW_MINOR = 5

# In a child process: the Qt window may open here instead, and its GTK 3 theme plugin aborts when
# GTK 4 is already loaded.
PROBE = f"""
import gi
gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
from gi.repository import Adw
raise SystemExit(0 if Adw.get_minor_version() >= {MIN_ADW_MINOR} else 1)
"""


def available():
    """True when GTK 4 and libadwaita 1.5 or newer can be loaded."""
    try:
        result = subprocess.run([sys.executable, "-c", PROBE], capture_output=True, timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        return False
    return result.returncode == 0
