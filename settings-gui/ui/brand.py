# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The few marks taken from the Ngó Sen website: enamel green and the Xanh Mono typeface."""

import os

from qtpy.QtCore import QEvent, QObject, QStandardPaths
from qtpy.QtGui import QFont, QFontDatabase, QIcon, QPalette

FONT_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "fonts", "XanhMono-Regular.ttf"
)
TYPED_FAMILY = "Xanh Mono"
LOGO = "fcitx-ngosen"

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


class _ThemeFollower(QObject):
    def __init__(self, widget, apply, source):
        super().__init__(widget)
        self._widget = widget
        self._apply = apply
        self._applying = False
        source.installEventFilter(self)

    def eventFilter(self, obj, event):
        # Child widgets get only PaletteChange, which apply() itself may also send.
        if event.type() == QEvent.PaletteChange and not self._applying:
            self._applying = True
            try:
                self._apply(self._widget)
            finally:
                self._applying = False
        return False


def follow_theme(widget, apply, source=None):
    """Runs apply(widget) now and again when the desktop switches between light and dark.

    A widget with its own style sheet keeps its palette, so it needs a plain ancestor as source.
    """
    apply(widget)
    _ThemeFollower(widget, apply, source or widget)
    return widget


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


def wordmark_font(pixel_size):
    # A monospace space is a full cell wide; the logo sets the two words closer.
    font = typed_font(pixel_size)
    font.setWordSpacing(-pixel_size * 0.27)
    return font


def logo_icon():
    # Icon loaders shorten a missing name, so themes with an "fcitx" icon (Papirus, Colloid) would
    # show that instead; take the installed hicolor file then.
    icon = QIcon.fromTheme(LOGO)
    if icon.name() == LOGO:
        return icon
    path = QStandardPaths.locate(
        QStandardPaths.GenericDataLocation, f"icons/hicolor/scalable/apps/{LOGO}.svg"
    )
    return QIcon(path) if path else QIcon()
