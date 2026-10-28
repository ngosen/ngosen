# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focus that also scrolls on a page the window has not laid out yet."""

from gi.repository import Gtk


def focus_when_laid_out(control):
    """Focuses a row and scrolls to it, waiting for a page that was never shown to get its size."""
    control.grab_focus()
    scrolled = control.get_ancestor(Gtk.ScrolledWindow)
    adjustment = scrolled.get_vadjustment() if scrolled else None
    if adjustment is None or adjustment.get_page_size() > 0:
        return

    def laid_out(adjustment):
        if adjustment.get_page_size() == 0:
            return
        adjustment.disconnect(handler)
        root = control.get_root()
        if root is None:
            return
        # A row already holding the focus is not scrolled to again.
        root.set_focus(None)
        control.grab_focus()

    handler = adjustment.connect("changed", laid_out)
