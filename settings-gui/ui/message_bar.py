# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The one message over the bottom of the pages: what was saved, or what went wrong."""

from i18n import _
from qtpy.QtCore import QEvent, QPoint, QSize, Qt, QTimer
from qtpy.QtGui import QIcon, QPainter
from qtpy.QtWidgets import (
    QApplication,
    QFrame,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QScrollArea,
    QToolButton,
)

from ui import brand

SHOWN_MS = 5000
SIDE_MARGIN = 20
GAP = 8


class MessageLabel(QLabel):
    """Cuts a long line short, keeping both ends of a path; a wrapping label shows all of it."""

    def minimumSizeHint(self):
        hint = super().minimumSizeHint()
        return hint if self.wordWrap() else QSize(0, hint.height())

    def paintEvent(self, event):
        if self.wordWrap():
            super().paintEvent(event)
            return
        text = self.fontMetrics().elidedText(self.text(), Qt.ElideMiddle, self.width())
        QPainter(self).drawText(self.rect(), Qt.AlignLeft | Qt.AlignVCenter, text)


class MessageBar(QFrame):
    """Floats over `host` instead of taking room in a layout, so showing it moves no control."""

    def __init__(self, host, on_undo):
        super().__init__(host.parentWidget())
        self.host = host
        self.setObjectName("SaveMessage")
        layout = QHBoxLayout(self)
        layout.setContentsMargins(12, 6, 6, 6)
        self.label = MessageLabel()
        layout.addWidget(self.label, 1)
        self.undo_button = QPushButton(_("&Undo"))
        self.undo_button.clicked.connect(on_undo)
        layout.addWidget(self.undo_button)
        self.close_button = QToolButton()
        self.close_button.setAutoRaise(True)
        self.close_button.setToolTip(_("Close"))
        self.close_button.setAccessibleName(_("Close"))
        icon = QIcon.fromTheme("window-close")
        if icon.isNull():
            self.close_button.setText("×")
        else:
            self.close_button.setIcon(icon)
        self.close_button.clicked.connect(self.dismiss)
        layout.addWidget(self.close_button)
        self.timer = QTimer(self)
        self.timer.setSingleShot(True)
        self.timer.setInterval(SHOWN_MS)
        self.timer.timeout.connect(self._expire)
        self.hide()
        host.installEventFilter(self)
        QApplication.instance().focusChanged.connect(self._keep_focus_clear)

    def show_message(self, text, undo=False, error=False, keep=False):
        """Errors and `keep` messages stay until closed; the rest leave by themselves."""
        if error:
            fill, border = brand.pick(self, brand.ERROR_FILL), brand.pick(self, brand.ERROR_BORDER)
        else:
            fill, border = brand.pick(self, brand.SAVED_FILL), brand.pick(self, brand.SAVED_BORDER)
        self.setStyleSheet(
            f"QFrame#SaveMessage {{ background: {fill}; border: 1px solid {border};"
            " border-radius: 6px; }"
        )
        self.label.setWordWrap(error)
        self.label.setText(text)
        self.label.setToolTip("" if error else text)
        self.undo_button.setVisible(undo)
        self.show()
        self.raise_()
        self.place()
        if error or keep:
            self.timer.stop()
        else:
            self.timer.start()

    def dismiss(self):
        self.timer.stop()
        self.hide()

    def room_needed(self):
        """Height a page must keep free at its end, so its last control can clear one line."""
        margins = self.layout().contentsMargins()
        line = max(self.undo_button.sizeHint().height(), self.close_button.sizeHint().height())
        return line + margins.top() + margins.bottom() + 2 * GAP

    def place(self):
        if self.isHidden():
            return
        corner = self.host.mapTo(self.parentWidget(), QPoint(0, 0))
        width = self.host.width() - 2 * SIDE_MARGIN
        layout = self.layout()
        if layout.hasHeightForWidth():
            height = layout.totalHeightForWidth(width)
        else:
            height = layout.totalSizeHint().height()
        y = corner.y() + self.host.height() - height - GAP
        self.setGeometry(corner.x() + SIDE_MARGIN, y, width, height)
        self._keep_focus_clear()

    def eventFilter(self, watched, event):
        if watched is self.host and event.type() == QEvent.Resize:
            self.place()
        return False

    def _expire(self):
        if self.underMouse() or self.isAncestorOf(QApplication.focusWidget()):
            self.timer.start()
        else:
            self.hide()

    def _keep_focus_clear(self, *_widgets):
        focused = QApplication.focusWidget()
        if self.isHidden() or focused is None or not self.host.isAncestorOf(focused):
            return
        scroll = focused.parentWidget()
        while scroll is not None and not isinstance(scroll, QScrollArea):
            scroll = scroll.parentWidget()
        if scroll is None:
            return
        viewport = scroll.viewport()
        top = focused.mapTo(viewport, QPoint(0, 0)).y()
        bar_top = viewport.mapFromGlobal(self.mapToGlobal(QPoint(0, 0))).y()
        covered = top + focused.height() + GAP - bar_top
        if covered > 0:
            # Never past its own top: a tall table stays readable from its first row.
            bar = scroll.verticalScrollBar()
            bar.setValue(bar.value() + max(0, min(covered, top)))
