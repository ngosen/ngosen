# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The GTK 4 and libadwaita settings window."""

MIN_ADW_MINOR = 5


def available():
    """True when GTK 4 and libadwaita 1.5 or newer can be loaded."""
    try:
        import gi

        gi.require_version("Gtk", "4.0")
        gi.require_version("Adw", "1")
        from gi.repository import Adw
    except (ImportError, ValueError):
        return False
    return Adw.get_minor_version() >= MIN_ADW_MINOR
