# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Two text boxes on the Convert page: text pasted in the first shows converted in the second."""

from i18n import _
from qtpy.QtCore import QTimer
from qtpy.QtGui import QIcon
from qtpy.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QStyle,
    QVBoxLayout,
    QWidget,
)

from core import convert
from ui import brand

# Long enough to skip the calls in the middle of a fast paste or typing burst.
DELAY_MS = 150
LINES = 4
COPIED_MS = 1500


def _text_box(placeholder, read_only=False):
    box = QPlainTextEdit()
    box.setPlaceholderText(placeholder)
    box.setFont(brand.sized_font(17))
    box.setTabChangesFocus(True)
    box.setReadOnly(read_only)
    margins = box.contentsMargins()
    box.setFixedHeight(
        box.fontMetrics().lineSpacing() * LINES
        + round(2 * box.document().documentMargin())
        + margins.top()
        + margins.bottom()
        + 2 * box.frameWidth()
    )
    return box


class ConvertBox(QWidget):
    """Converts with the settings the page shows, saved or not, through the running Ngó Sen."""

    def __init__(self, model, dbus_handler):
        super().__init__()
        self.model = model
        self.dbus = dbus_handler
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(8)

        self.source = _text_box(_("Paste the text to convert"))
        self.result = _text_box(_("The converted text shows here"), read_only=True)
        layout.addWidget(self.source)
        layout.addWidget(self.result)

        bottom = QHBoxLayout()
        self.error_icon = QLabel()
        size = self.style().pixelMetric(QStyle.PM_SmallIconSize)
        self.error_icon.setPixmap(QIcon.fromTheme("dialog-warning").pixmap(size, size))
        self.error = QLabel()
        self.error.setWordWrap(True)
        self.copy = QPushButton(QIcon.fromTheme("edit-copy"), _("&Copy result"))
        self.copy.clicked.connect(self._copy)
        bottom.addWidget(self.error_icon)
        bottom.addWidget(self.error, 1)
        bottom.addStretch()
        bottom.addWidget(self.copy)
        layout.addLayout(bottom)

        self.timer = QTimer(self, singleShot=True, interval=DELAY_MS)
        self.timer.timeout.connect(self.refresh)
        self.source.textChanged.connect(self.timer.start)
        self.copied = None
        self._show("", None)

    def refresh(self):
        self.timer.stop()
        self._show(*convert.convert(self.dbus, self.model, self.source.toPlainText()))

    def _show(self, text, error):
        self.result.setPlainText(text or "")
        self.error.setText(error or "")
        self.error_icon.setVisible(bool(error))
        self.error.setVisible(bool(error))
        self.copy.setEnabled(bool(text))

    def _copy(self):
        QApplication.clipboard().setText(self.result.toPlainText())
        self.copy.setText(_("Copied"))
        # A child timer goes with the page, which an Undo may rebuild before it fires.
        if self.copied is None:
            self.copied = QTimer(self, singleShot=True)
            self.copied.timeout.connect(lambda: self.copy.setText(_("&Copy result")))
        self.copied.start(COPIED_MS)
