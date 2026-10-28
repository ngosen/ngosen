#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Application entry point.
"""

import os
import signal
import sys

from core.brand import APP_ID
from core.cli import parse_args
from core.toolkit_install import has_gtk, has_qt, reopen_if_switched
from i18n import saved_setting, setup_i18n

# The desktops whose own apps are GTK and libadwaita, and their session names.
GTK_DESKTOPS = {"GNOME", "X-Cinnamon"}
GTK_SESSIONS = ("gnome", "ubuntu", "cinnamon")


def desktop_wants_gtk(env):
    current = env.get("XDG_CURRENT_DESKTOP")
    if current:
        return bool(set(current.split(":")) & GTK_DESKTOPS)
    # im-config starts fcitx5 from Xsession.d, before the session manager sets XDG_CURRENT_DESKTOP.
    session = env.get("XDG_SESSION_DESKTOP") or env.get("DESKTOP_SESSION") or ""
    return session.lower().startswith(GTK_SESSIONS)


def toolkit(env=os.environ, qt=has_qt, gtk=has_gtk):
    """The window the user picked, or the other one when only that is installed."""
    wanted = env.get("NGOSEN_SETTINGS_TOOLKIT") or saved_setting("SettingsToolkit", "Auto", env)
    wanted = wanted.lower()
    if wanted == "auto":
        wanted = "gtk" if desktop_wants_gtk(env) else "qt"
    order = ("gtk", "qt") if wanted == "gtk" else ("qt", "gtk")
    installed = {"qt": qt, "gtk": gtk}
    for name in order:
        if installed[name]():
            return name
    return "qt"


def run_gtk():
    from ui_gtk.window import run

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    return run(sys.argv)


def run_qt():
    # Picks Qt6 when both PyQt5 and PyQt6 are installed, so it comes before anything else from Qt.
    import qt_api  # noqa: F401
    from qtpy.QtWidgets import QApplication
    from ui import brand, color_scheme, scroll_bars
    from ui.main_window import NgoSenSettingsWindow

    args = parse_args(sys.argv[1:])
    app = QApplication(sys.argv)
    app.setDesktopFileName(APP_ID)
    app.setApplicationName(APP_ID)
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app.setWindowIcon(brand.logo_icon())
    color_scheme.install(app)
    brand.install_accent(app)
    scroll_bars.install(app)

    window = NgoSenSettingsWindow()
    if args.page:
        window.show_page(args.page)
    window.show()

    try:
        return app.exec()
    except KeyboardInterrupt:
        app.quit()
        return 0


def main():
    """Main execution function."""
    setup_i18n()
    code = run_gtk() if toolkit() == "gtk" else run_qt()
    reopen_if_switched()
    sys.exit(code)


if __name__ == "__main__":
    main()
