#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Application entry point.
"""

import importlib.util
import os
import signal
import sys

import ui_gtk
from core.brand import APP_ID
from core.cli import parse_args
from i18n import saved_setting, setup_i18n

QT_BINDINGS = ("PyQt6", "PySide6", "PyQt5")


def has_qt(find=importlib.util.find_spec):
    return bool(find("qtpy")) and any(find(module) for module in QT_BINDINGS)


def toolkit(env=os.environ, qt=has_qt, gtk=ui_gtk.available):
    """The window the user picked, or the other one when only that is installed."""
    wanted = env.get("NGOSEN_SETTINGS_TOOLKIT") or saved_setting("SettingsToolkit", "Qt", env)
    order = ("gtk", "qt") if wanted.lower() == "gtk" else ("qt", "gtk")
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
    sys.exit(run_gtk() if toolkit() == "gtk" else run_qt())


if __name__ == "__main__":
    main()
