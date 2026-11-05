# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""A row saying why what a page holds is not in use right now."""

from gi.repository import Adw


def status_row():
    row = Adw.ActionRow(use_markup=False, css_classes=["dim-label"], visible=False)
    # It explains a state; search should not offer it as a setting.
    row.searchable = False
    return row


def show_status(row, text):
    row.set_title(text)
    row.set_visible(bool(text))
