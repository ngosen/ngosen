# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Questions whose buttons follow the window's language."""

from i18n import _
from qtpy.QtWidgets import QMessageBox


def confirm(parent, title, text, accept_text):
    """True when the user picks accept_text; Qt would name Yes/No in the desktop's language instead."""
    box = QMessageBox(QMessageBox.Question, title, text, QMessageBox.NoButton, parent)
    accept = box.addButton(accept_text, QMessageBox.AcceptRole)
    box.setDefaultButton(box.addButton(_("&Cancel"), QMessageBox.RejectRole))
    box.exec()
    return box.clickedButton() is accept
