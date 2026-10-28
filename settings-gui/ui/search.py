# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Finds settings by the words shown on screen."""

from core.text import fold
from qtpy.QtWidgets import QCheckBox, QLabel, QRadioButton


def find(pages, query):
    """Returns (text, widget to focus, page) for each label holding every word of query."""
    words = fold(query).split()
    if not words:
        return []
    found = {}
    for page in pages:
        widgets = []
        for kind in (QCheckBox, QRadioButton, QLabel):
            widgets += page.findChildren(kind)
        for widget in widgets:
            text = widget.text().replace("&", "").strip().rstrip(":")
            if not text or len(text) > 80 or not all(w in fold(text) for w in words):
                continue
            buddy = widget.buddy() if isinstance(widget, QLabel) else None
            # A page rebuilt in place still holds its old widgets until the event loop
            # deletes them; the newer one comes later and wins.
            found[(text, page)] = (text, buddy or widget, page)
    return list(found.values())
