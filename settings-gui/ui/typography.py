# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Text emphasis built from the system font and palette, so every theme keeps its look."""

from qtpy.QtGui import QFont, QPalette
from qtpy.QtWidgets import QLabel


def emphasize(widget, scale=1.0):
    font = widget.font()
    font.setWeight(QFont.DemiBold)
    if scale != 1.0:
        font.setPointSizeF(font.pointSizeF() * scale)
    widget.setFont(font)
    return widget


def mute(label):
    # Some themes leave the placeholder colour unset, so fade the theme's own text colour.
    palette = label.palette()
    color = palette.color(QPalette.WindowText)
    color.setAlphaF(0.65)
    palette.setColor(QPalette.WindowText, color)
    label.setPalette(palette)
    font = label.font()
    font.setPointSizeF(font.pointSizeF() * 0.9)
    label.setFont(font)
    return label


def page_title(text):
    title = QLabel(text)
    title.setObjectName("CategoryTitle")
    return emphasize(title, 1.3)
