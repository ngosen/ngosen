# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The "What's new & tips" page: recent features and typing tips, each with a way to its setting."""

from i18n import _
from qtpy.QtCore import Qt
from qtpy.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QScrollArea,
    QVBoxLayout,
    QWidget,
)

from ui import brand
from ui.pages.dynamic_settings import CardWidget
from ui.typography import emphasize, page_title
from ui.whats_new_content import FEATURES, TIPS

# Sidebar pages the tips point at, named as they are in the window.
PAGE_TITLES = {"macros": _("Macros"), "about": _("About")}


class WhatsNewPage(QWidget):
    """Read-only, so the window neither saves it nor searches it for settings."""

    searchable = False

    def __init__(self, open_setting, show_page, parent=None):
        super().__init__(parent)
        self._open_setting = open_setting
        self._show_page = show_page
        self._setup_ui()

    def _setup_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setContentsMargins(30, 20, 30, 20)
        layout.setSpacing(16)
        layout.addWidget(page_title(_("What's new & tips")))

        shown = []
        for feature in FEATURES:
            if feature["version"] not in shown:
                shown.append(feature["version"])
                layout.addWidget(self._heading(_("New in {}").format(feature["version"])))
            layout.addWidget(self._card(feature))

        self.tips_heading = self._heading(_("Tips"))
        layout.addWidget(self.tips_heading)
        for tip in TIPS:
            layout.addWidget(self._tip_row(tip))
        layout.addStretch()
        scroll.setWidget(content)
        root.addWidget(scroll)
        self.scroll = scroll

    def show_tips(self):
        self.scroll.ensureWidgetVisible(self.tips_heading)

    @staticmethod
    def _heading(text):
        return emphasize(QLabel(text), 1.15)

    def _card(self, feature):
        card = CardWidget()
        card.content_layout.addWidget(emphasize(QLabel(_(feature["title"]))))
        how = QLabel(_(feature["how"]))
        how.setWordWrap(True)
        card.content_layout.addWidget(how)
        if feature["example"]:
            example = QLabel(feature["example"])
            example.setObjectName("TypingExample")
            example.setFont(brand.typed_font(16))
            card.content_layout.addWidget(example)
        button = self._open_button(feature["target"])
        if button:
            card.content_layout.addWidget(button, alignment=Qt.AlignRight)
        return card

    def _tip_row(self, tip):
        row = QWidget()
        layout = QHBoxLayout(row)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        text = QLabel(_(tip["text"]))
        text.setWordWrap(True)
        layout.addWidget(text, 1)
        button = self._open_button(tip["target"])
        if button:
            layout.addWidget(button, alignment=Qt.AlignTop)
        return row

    def _open_button(self, target):
        if target is None:
            return None
        kind, name = target
        if kind == "setting":
            button = QPushButton(_("Open setting"))
            button.clicked.connect(lambda: self._open_setting(name))
        else:
            button = QPushButton(_("Open the {} page").format(PAGE_TITLES[name]))
            button.clicked.connect(lambda: self._show_page(name))
        button.setProperty("target", target)
        return button
