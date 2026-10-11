# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The few marks taken from the Ngó Sen website: enamel green and the Xanh Mono typeface."""

import os

from qtpy.QtGui import QFont, QFontDatabase, QPalette

FONT_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "fonts", "XanhMono-Regular.ttf"
)
TYPED_FAMILY = "Xanh Mono"

# (light, dark) pairs; dark is picked from the window background, not the desktop name.
ENAMEL = ("#1c6b5f", "#2a8574")
ON_ENAMEL = "#f0f5ee"
SAVED_FILL = ("#e1efe9", "#183a33")
SAVED_BORDER = ("#1c6b5f", "#3f9c8b")
ERROR_FILL = ("#f8e3df", "#3d1d18")
ERROR_BORDER = ("#c2301c", "#d0533f")

_font_loaded = False


def is_dark(widget):
    return widget.palette().color(QPalette.Window).lightness() < 128


def pick(widget, pair):
    return pair[1] if is_dark(widget) else pair[0]


def typed_font(pixel_size):
    """Xanh Mono for text that shows typing; the system monospace font if it is missing."""
    global _font_loaded
    if not _font_loaded:
        QFontDatabase.addApplicationFont(FONT_FILE)
        _font_loaded = True
    font = QFont(TYPED_FAMILY)
    font.setStyleHint(QFont.TypeWriter)
    font.setPixelSize(pixel_size)
    return font
