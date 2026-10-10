#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Application entry point.
"""

import signal
import sys

from i18n import setup_i18n
from qtpy.QtWidgets import QApplication
from ui import brand
from ui.main_window import NgoSenSettingsWindow


def main():
    """Main execution function."""
    setup_i18n()
    app = QApplication(sys.argv)
    app.setDesktopFileName("io.github.ngosen.NgoSen.Settings")
    app.setApplicationName("io.github.ngosen.NgoSen.Settings")
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app.setWindowIcon(brand.logo_icon())

    window = NgoSenSettingsWindow()
    window.show()

    try:
        sys.exit(app.exec())
    except KeyboardInterrupt:
        app.quit()


if __name__ == "__main__":
    main()
