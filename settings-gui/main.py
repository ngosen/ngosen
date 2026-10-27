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
from i18n import setup_i18n


def toolkit(env=os.environ):
    """qt, or gtk when the developer asks for it and GTK 4 with libadwaita 1.5 is there."""
    if env.get("NGOSEN_SETTINGS_TOOLKIT") != "gtk":
        return "qt"
    import ui_gtk

    if ui_gtk.available():
        return "gtk"
    print("GTK 4 or libadwaita 1.5 is missing; opening the Qt window.", file=sys.stderr)
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
