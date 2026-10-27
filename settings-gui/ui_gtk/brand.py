# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Ngó Sen marks for the GTK window: enamel as the accent, Xanh Mono for typed text."""

import ctypes
import ctypes.util
import os

from gi.repository import Adw, Gdk, Gtk, PangoCairo

FONT_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "fonts", "XanhMono-Regular.ttf"
)
LOGO = "io.github.ngosen.NgoSen.Settings"
# GTK has no word-spacing; a thin space sets the two words as close as the logo does.
WORDMARK = "Ngó Sen"

# (light, dark), as in the Qt window.
ENAMEL = ("#1c6b5f", "#247b6c")
ON_ENAMEL = "#f0f5ee"
# Enamel as text or a focus ring on the window; a dark window needs a lighter shade.
ENAMEL_TEXT = ("#1c6b5f", "#5fb8a6")

MARKS = """
.wordmark { font-family: "Xanh Mono", monospace; font-size: 26px; }
.typed, .typed text { font-family: "Xanh Mono", monospace; font-size: 17px; }
.mode-button:checked { background: %(bg)s; color: %(fg)s; }
"""


def load_font(path=FONT_FILE):
    """Makes Xanh Mono available to this process only; the system monospace font stands in if not."""
    if not os.path.exists(path):
        return
    font_map = PangoCairo.FontMap.get_default()
    if hasattr(font_map, "add_font_file"):  # Pango 1.56
        try:
            font_map.add_font_file(path)
            return
        except Exception:
            pass
    library = ctypes.util.find_library("fontconfig")
    if library:
        ctypes.CDLL(library).FcConfigAppFontAddFile(None, path.encode())


def css(dark, adw_minor):
    shade = 1 if dark else 0
    colors = {"bg": ENAMEL[shade], "fg": ON_ENAMEL, "text": ENAMEL_TEXT[shade]}
    # libadwaita 1.6 moved its colours to CSS variables; 1.5 still reads named colours.
    if adw_minor >= 6:
        accent = (
            ":root { --accent-bg-color: %(bg)s; --accent-fg-color: %(fg)s;"
            " --accent-color: %(text)s; }"
        )
    else:
        accent = (
            "@define-color accent_bg_color %(bg)s;\n"
            "@define-color accent_fg_color %(fg)s;\n"
            "@define-color accent_color %(text)s;"
        )
    return (accent + MARKS) % colors


def install():
    """Applies the marks now and again whenever the window turns light or dark."""
    provider = Gtk.CssProvider()
    Gtk.StyleContext.add_provider_for_display(
        Gdk.Display.get_default(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )
    style = Adw.StyleManager.get_default()

    def apply(*_args):
        provider.load_from_string(css(style.get_dark(), Adw.get_minor_version()))

    style.connect("notify::dark", apply)
    apply()
    return provider
