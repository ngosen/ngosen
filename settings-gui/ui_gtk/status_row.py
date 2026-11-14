# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""A row saying why what a page holds is not in use right now."""

from gi.repository import Adw

from core import page_status


def status_row():
    row = Adw.ActionRow(use_markup=False, css_classes=["dim-label"], visible=False)
    # It explains a state; search should not offer it as a setting.
    row.searchable = False
    return row


def show_status(row, text):
    row.set_title(text)
    row.set_visible(bool(text))


def older_fcitx_group(model, category):
    """A group holding the older-fcitx5 row, and the row."""
    row = status_row()
    show_status(row, page_status.older_fcitx_line(model, category))
    group = Adw.PreferencesGroup()
    group.add(row)
    return group, row
