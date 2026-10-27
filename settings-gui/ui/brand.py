# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The few marks taken from the Ngó Sen website: enamel green and the Xanh Mono typeface."""

import os

from core.catalog import TRAY_PANEL
from qtpy.QtCore import QEvent, QObject, QStandardPaths, Qt
from qtpy.QtGui import QColor, QFont, QFontDatabase, QIcon, QPainter, QPalette, QPixmap

FONT_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "fonts", "XanhMono-Regular.ttf"
)
TYPED_FAMILY = "Xanh Mono"
LOGO = "io.github.ngosen.NgoSen.Settings"

# (light, dark) pairs; dark is picked from the window background, not the desktop name.
# The dark shade keeps light text readable on it and still stands out on a dark window.
ENAMEL = ("#1c6b5f", "#247b6c")
ON_ENAMEL = "#f0f5ee"
SAVED_FILL = ("#e1efe9", "#183a33")
SAVED_BORDER = ("#1c6b5f", "#3f9c8b")
ERROR_FILL = ("#f8e3df", "#3d1d18")
ERROR_BORDER = ("#c2301c", "#d0533f")
# A neutral light or dark desktop panel, behind the tray icon preview.
PANEL = TRAY_PANEL

_font_loaded = False


def is_dark_palette(palette):
    return palette.color(QPalette.Window).lightness() < 128


def is_dark(widget):
    return is_dark_palette(widget.palette())


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


class _AccentFollower(QObject):
    def __init__(self, app):
        super().__init__(app)
        self._app = app

    def eventFilter(self, obj, event):
        # An application's filter sees every object's events; only the app's own change counts.
        if obj is self._app and event.type() == QEvent.ApplicationPaletteChange:
            _apply_accent(self._app)
        return False


def _apply_accent(app):
    palette = app.palette()
    enamel = QColor(ENAMEL[1] if is_dark_palette(palette) else ENAMEL[0])
    # Half see-through, so a disabled selection stays enamel but reads as off.
    faded = QColor(enamel)
    faded.setAlpha(128)
    roles = [QPalette.Highlight] + ([QPalette.Accent] if hasattr(QPalette, "Accent") else [])
    accented = QPalette(palette)
    for group, color in (
        (QPalette.Active, enamel),
        (QPalette.Inactive, enamel),
        (QPalette.Disabled, faded),
    ):
        for role in roles:
            accented.setColor(group, role, color)
    for group in (QPalette.Active, QPalette.Inactive):
        accented.setColor(group, QPalette.HighlightedText, QColor(ON_ENAMEL))
    # Setting the palette sends another change; stop once it already holds enamel.
    if accented != palette:
        app.setPalette(accented)


def install_accent(app):
    """Makes enamel the accent, now and whenever the desktop sends a new palette."""
    _apply_accent(app)
    follower = _AccentFollower(app)
    app.installEventFilter(follower)
    return follower


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


def theme_icon(name):
    """The theme's icon, or its symbolic variant for themes like Adwaita that ship only that."""
    for candidate in (name, f"{name}-symbolic"):
        if QIcon.hasThemeIcon(candidate):
            return QIcon.fromTheme(candidate)
    return QIcon()


def installed_icon(name):
    """The theme's icon, or the installed hicolor file if the theme lacks it."""
    # Icon loaders shorten a missing name, so themes with an "fcitx" icon (Papirus, Colloid) would
    # show that instead.
    icon = QIcon.fromTheme(name)
    if icon.name() == name:
        return icon
    path = QStandardPaths.locate(
        QStandardPaths.GenericDataLocation, f"icons/hicolor/scalable/apps/{name}.svg"
    )
    return QIcon(path) if path else QIcon()


def logo_icon():
    return installed_icon(LOGO)


def tray_preview(names, dark, size=22):
    """The given tray icons side by side on a strip shaded like a light or dark panel."""
    pad, gap = 6, 8
    strip = QPixmap(pad * 2 + size * len(names) + gap * (len(names) - 1), size + pad * 2)
    strip.fill(Qt.transparent)
    painter = QPainter(strip)
    painter.setRenderHint(QPainter.Antialiasing)
    painter.setPen(Qt.NoPen)
    painter.setBrush(QColor(PANEL[1] if dark else PANEL[0]))
    painter.drawRoundedRect(strip.rect(), 4, 4)
    for i, name in enumerate(names):
        painter.drawPixmap(pad + i * (size + gap), pad, installed_icon(name).pixmap(size, size))
    painter.end()
    return strip
