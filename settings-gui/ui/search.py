# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Finds settings by the words shown on screen."""

from core.text import fold
from qtpy.QtWidgets import QCheckBox, QLabel, QRadioButton


def find(pages, query):
    """Returns (text, widget to focus, page, setting key or None) for each matching label."""
    words = fold(query).split()
    if not words:
        return []
    found = {}
    for page in pages:
        widgets = []
        for kind in (QCheckBox, QRadioButton, QLabel):
            widgets += page.findChildren(kind)
        for widget in widgets:
            # A line about a state, not a setting to find.
            if not getattr(widget, "searchable", True):
                continue
            text = widget.text().replace("&", "").strip().rstrip(":")
            if not text or len(text) > 80 or not all(w in fold(text) for w in words):
                continue
            buddy = widget.buddy() if isinstance(widget, QLabel) else None
            # A page rebuilt in place still holds its old widgets until the event loop
            # deletes them; the newer one comes later and wins.
            target = buddy or widget
            found[(text, page)] = (text, target, page, _setting_of(target, page))
    return list(found.values())


def _setting_of(widget, page):
    """The only setting inside the closest parent holding any; a label can target a wrapper."""
    controls = getattr(page, "controls", {})
    while widget is not None:
        inside = [
            key
            for key, control in controls.items()
            if control is widget or widget.isAncestorOf(control)
        ]
        if inside:
            return inside[0] if len(inside) == 1 else None
        widget = widget.parentWidget()
    return None
