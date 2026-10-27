# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Finds settings by the row titles shown on screen."""

from core.text import fold
from gi.repository import Adw


def find(pages, query):
    """Returns (title, row, page name) for each row whose title holds every word of query.

    pages is a list of (name, widget); a widget with searchable = False is skipped with
    everything inside it, so lists of the user's own entries do not show up as settings.
    """
    words = fold(query).split()
    if not words:
        return []
    found = []
    for name, widget in pages:
        for row in _rows(widget):
            title = row.get_title().strip().rstrip(":")
            if title and len(title) <= 80 and all(w in fold(title) for w in words):
                found.append((title, row, name))
    return found


def _rows(widget):
    child = widget.get_first_child()
    while child is not None:
        if child.get_visible() and getattr(child, "searchable", True):
            if isinstance(child, Adw.PreferencesRow):
                yield child
            yield from _rows(child)
        child = child.get_next_sibling()
