# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Emoji page: the emoji settings, the apps that offer none, and the words of each emoji."""

import os
from bisect import bisect_left, bisect_right
from itertools import accumulate

from core.catalog import SettingsCategory
from core.emoji_table import GROUPS
from core.emoji_words import EmojiWords, export_words, read_shared, toggle_hint
from i18n import N_, _
from qtpy.QtCore import QEvent, QPoint, QRect, QRectF, QSignalBlocker, QSize, Qt, QTimer
from qtpy.QtGui import QColor, QFontMetrics, QPainter, QPalette, QPen
from qtpy.QtWidgets import (
    QAbstractButton,
    QApplication,
    QButtonGroup,
    QFileDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLayout,
    QLineEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
    QWidgetItem,
)

from ui import brand
from ui.components import KeyCap
from ui.pages.dynamic_settings import DynamicSettingsPage, HintLabel
from ui.typography import mute

# Shorter than the Unicode names, in the order of GROUPS.
GROUP_LABELS = (
    N_("Smileys"),
    N_("People & gestures"),
    N_("Animals & plants"),
    N_("Food & drink"),
    N_("Travel & weather"),
    N_("Activities & events"),
    N_("Objects"),
    N_("Symbols"),
    N_("Flags"),
)
FILTER_DELAY_MS = 150
ROW_MARGIN = 10
ROW_SPACING = 10
FACE_WIDTH = 44
ENTRY_WIDTH = 110


class FlowLayout(QLayout):
    """Puts its widgets side by side and wraps them onto new lines at the width it gets."""

    def __init__(self, parent=None, spacing=6):
        super().__init__(parent)
        self._items = []
        self._gap = spacing
        # Qt asks for the height of the same width many times per layout pass.
        self._heights = {}
        self.setContentsMargins(0, 0, 0, 0)

    def addItem(self, item):
        self._items.append(item)

    def insertWidget(self, index, widget):
        self.addChildWidget(widget)
        self._items.insert(index, QWidgetItem(widget))
        self.invalidate()

    def drop(self, widget):
        # Not removeWidget(): Qt would delete an item Python still owns.
        self._items = [item for item in self._items if item.widget() is not widget]
        self.invalidate()

    def invalidate(self):
        self._heights = {}
        super().invalidate()

    def count(self):
        return len(self._items)

    def itemAt(self, index):
        return self._items[index] if 0 <= index < len(self._items) else None

    def takeAt(self, index):
        return self._items.pop(index) if 0 <= index < len(self._items) else None

    def expandingDirections(self):
        return Qt.Orientation(0)

    def hasHeightForWidth(self):
        return True

    def heightForWidth(self, width):
        if width not in self._heights:
            self._heights[width] = self._place(QRect(0, 0, width, 0), False)
        return self._heights[width]

    def setGeometry(self, rect):
        super().setGeometry(rect)
        self._place(rect, True)

    def sizeHint(self):
        return self.minimumSize()

    def minimumSize(self):
        size = QSize()
        for item in self._items:
            size = size.expandedTo(item.minimumSize())
        return size

    def _place(self, rect, apply):
        x, y, line = rect.x(), rect.y(), 0
        for item in self._items:
            if item.isEmpty():
                continue
            hint = item.sizeHint()
            if x > rect.x() and x + hint.width() > rect.right() + 1:
                x, y, line = rect.x(), y + line + self._gap, 0
            if apply:
                item.setGeometry(QRect(QPoint(x, y), hint))
            x += hint.width() + self._gap
            line = max(line, hint.height())
        return y + line - rect.y()


class Chip(QAbstractButton):
    """A word of an emoji: solid in the accent when on, dashed and struck through when off."""

    PAD_X = 10
    PAD_Y = 4

    def __init__(self, word, emoji, parent=None):
        super().__init__(parent)
        self.setText(word)
        self.emoji = emoji
        self.setCheckable(True)
        self.setCursor(Qt.PointingHandCursor)
        self.setAttribute(Qt.WA_Hover)

    def setText(self, text):
        super().setText(text)
        self._hint = None

    def changeEvent(self, event):
        if event.type() == QEvent.FontChange:
            self._hint = None
        super().changeEvent(event)

    def show_state(self, on):
        with QSignalBlocker(self):
            self.setChecked(on)
        text = _("Turn off {word} for {emoji}") if on else _("Turn on {word} for {emoji}")
        self.setAccessibleName(text.format(word=self.text(), emoji=self.emoji))
        self.update()

    def sizeHint(self):
        if self._hint is None:
            metrics = self.fontMetrics()
            self._hint = QSize(
                metrics.horizontalAdvance(self.text()) + 2 * self.PAD_X + 2,
                metrics.height() + 2 * self.PAD_Y + 2,
            )
        return self._hint

    def minimumSizeHint(self):
        return self.sizeHint()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        palette = self.palette()
        accent = palette.color(QPalette.Highlight)
        text = palette.color(QPalette.WindowText)
        rect = QRectF(self.rect()).adjusted(1, 1, -1, -1)
        font = self.font()
        hovered = self.underMouse()
        if self.isChecked():
            fill = QColor(accent)
            fill.setAlphaF(0.28 if hovered else 0.16)
            painter.setBrush(fill)
            pen = QPen(accent, 2 if self.hasFocus() else 1)
        else:
            text.setAlphaF(0.6)
            painter.setBrush(QColor(0, 0, 0, 0) if not hovered else _faint(text))
            pen = QPen(accent if self.hasFocus() else text, 2 if self.hasFocus() else 1)
            pen.setDashPattern([3, 2])
            font.setStrikeOut(self.isCheckable())
        painter.setPen(pen)
        radius = rect.height() / 2
        painter.drawRoundedRect(rect, radius, radius)
        painter.setFont(font)
        painter.setPen(text)
        painter.drawText(self.rect(), Qt.AlignCenter, self.text())


class NewWord(Chip):
    """The "+ word" button, which turns into a box to type in when clicked."""

    def __init__(self, emoji, parent=None):
        super().__init__("", emoji, parent)
        self.setCheckable(False)
        self.setAccessibleName(_("Add a word for {}").format(emoji))


def _faint(color):
    faint = QColor(color)
    faint.setAlphaF(0.08)
    return faint


class Lines(QFrame):
    """A framed list on the base colour, its rows parted by thin lines."""

    def __init__(self):
        super().__init__()
        self.setFrameShape(QFrame.StyledPanel)
        self.setBackgroundRole(QPalette.Base)
        self.setAutoFillBackground(True)
        self.rows = QVBoxLayout(self)
        self.rows.setContentsMargins(0, 0, 0, 0)
        self.rows.setSpacing(0)


class Row(QWidget):
    def paintEvent(self, event):
        if self.y() == 0:
            return
        color = self.palette().color(QPalette.WindowText)
        color.setAlphaF(0.12)
        painter = QPainter(self)
        painter.setPen(color)
        painter.drawLine(0, 0, self.width(), 0)


class EmojiRow(Row):
    """One emoji with its words, a box to add one, and a way back to the defaults."""

    def __init__(self, page, row, parent):
        super().__init__(parent)
        self.page = page
        self.emoji, _group, name = row[:3]
        layout = QHBoxLayout(self)
        layout.setContentsMargins(ROW_MARGIN, 6, ROW_MARGIN, 6)
        layout.setSpacing(ROW_SPACING)

        face = QLabel(self.emoji)
        face.searchable = False
        face.setFont(page.face_font)
        face.setFixedWidth(FACE_WIDTH)
        face.setAlignment(Qt.AlignCenter)
        face.setAccessibleName(name)
        face.setToolTip(name)
        layout.addWidget(face, 0, Qt.AlignTop)

        self.word_box = QWidget()
        self.flow = FlowLayout(self.word_box)
        # A button until clicked: an entry in every row built would double their cost.
        self.new = NewWord(self.emoji, self.word_box)
        self.new.clicked.connect(self.open_entry)
        self.flow.addWidget(self.new)
        self.entry = None
        self.add_button = None
        self._label_new()
        layout.addWidget(self.word_box, 1)

        self.reset_button = QPushButton(_("As before"))
        self.reset_button.setFlat(True)
        self.reset_button.setToolTip(_("Put back the words {} came with").format(self.emoji))
        self.reset_button.clicked.connect(self._reset)
        layout.addWidget(self.reset_button, 0, Qt.AlignTop)
        self.chips = []
        self.show_words()

    def show_words(self):
        for chip in self.chips:
            self.flow.drop(chip)
            chip.hide()
            chip.deleteLater()
        self.chips = []
        for index, (word, on) in enumerate(self.page.words.chips(self.emoji)):
            chip = Chip(word, self.emoji, self.word_box)
            chip.show_state(on)
            chip.clicked.connect(lambda _checked=False, c=chip: self._toggle(c))
            self.flow.insertWidget(index, chip)
            self.chips.append(chip)
        self._show_reset()

    def _show_reset(self):
        self.reset_button.setVisible(self.page.words.changed(self.emoji))
        self.page.view.fit(self)

    def _label_new(self):
        self.new.setText(self.page.drafts.get(self.emoji) or _("+ word"))

    def open_entry(self):
        # Copied: closing a box refits its row, which can build or drop rows.
        for row in list(self.page.view.live.values()):
            if row is not self:
                row.close_entry()
        if self.entry is None:
            self.entry = QLineEdit(self.word_box)
            self.entry.setPlaceholderText(_("+ word"))
            self.entry.setAccessibleName(_("Add a word for {}").format(self.emoji))
            self.entry.setFixedWidth(ENTRY_WIDTH)
            self.entry.setText(self.page.drafts.get(self.emoji, ""))
            self.add_button = QPushButton(_("Add"), self.word_box)
            self.add_button.setVisible(bool(self.entry.text().strip()))
            self.entry.textChanged.connect(self._on_draft)
            self.entry.returnPressed.connect(self._add)
            self.entry.editingFinished.connect(lambda: QTimer.singleShot(0, self._close_if_empty))
            self.add_button.clicked.connect(self._add)
            self.flow.insertWidget(len(self.chips), self.entry)
            self.flow.insertWidget(len(self.chips) + 1, self.add_button)
            # Shown now, not on the queued show, so the row is measured with it.
            self.entry.show()
            self.new.hide()
            self.flow.drop(self.new)
            self.page.view.fit(self)
        self.entry.setFocus()

    def close_entry(self):
        if self.entry is None:
            return
        for widget in (self.entry, self.add_button):
            self.flow.drop(widget)
            widget.hide()
            widget.deleteLater()
        self.entry = self.add_button = None
        self._label_new()
        self.flow.insertWidget(len(self.chips), self.new)
        self.new.show()
        self.page.view.fit(self)

    def _close_if_empty(self):
        if self.entry is None or self.entry.text():
            return
        if not (self.entry.hasFocus() or self.add_button.hasFocus()):
            self.close_entry()

    def _on_draft(self, text):
        # Kept by the page, since the row goes away when it scrolls out of sight.
        if text:
            self.page.drafts[self.emoji] = text
        else:
            self.page.drafts.pop(self.emoji, None)
        self.add_button.setVisible(bool(text.strip()))
        self.page.view.fit(self)

    def _toggle(self, chip):
        self.page.words.toggle(self.emoji, chip.text())
        chip.show_state(self.page.words.is_on(self.emoji, chip.text()))
        self._show_reset()
        self.page.words_changed(self.emoji)

    def _add(self):
        if not self.page.words.add(self.emoji, self.entry.text()):
            self.entry.selectAll()
            return
        self.entry.clear()
        self.show_words()
        self.page.words_changed(self.emoji)

    def _reset(self):
        self.page.words.reset(self.emoji)
        self.show_words()
        (self.entry or self.new).setFocus()
        self.page.words_changed(self.emoji)


class RowsView(QWidget):
    """Holds widgets only for the rows near the visible part of the page.

    The other rows get an estimated height, so the page scrolls over all of them.
    """

    def __init__(self, page):
        super().__init__()
        self.page = page
        self.rows = []
        self.live = {}
        self._width = None
        self._heights = []
        self._tops = [0]
        self._parts = []
        self._totals = {}
        sizes = page.sizes
        self._line = sizes["chip"]
        self._face = QFontMetrics(page.face_font).height()

    def set_rows(self, rows):
        for widget in self.live.values():
            widget.hide()
            widget.deleteLater()
        self.live = {}
        self.rows = rows
        self.remeasure()

    def remeasure(self):
        self._parts = [self._part(row[0]) for row in self.rows]
        self._totals = {}
        self._width = None
        self._measure(self.width())
        self.updateGeometry()
        QTimer.singleShot(0, self.fill)

    def hasHeightForWidth(self):
        return True

    def heightForWidth(self, width):
        # Layouts ask for widths the view never gets; those need no row positions.
        if width == self._width:
            return self._tops[-1]
        if width not in self._totals:
            self._totals[width] = sum(self._estimate(part, width) for part in self._parts)
        return self._totals[width]

    def sizeHint(self):
        return QSize(400, self.heightForWidth(max(self.width(), 400)))

    def _measure(self, width):
        if width == self._width:
            return
        self._width = width
        self._heights = [self._estimate(part, width) for part in self._parts]
        self._sum()

    def _sum(self):
        self._tops = [0, *accumulate(self._heights)]

    def _part(self, emoji):
        sizes = self.page.sizes
        widths = [sizes["word"](w) for w, _on in self.page.words.chips(emoji)]
        widths.append(sizes["word"](self.page.drafts.get(emoji) or _("+ word")))
        return widths, self.page.words.changed(emoji)

    def _estimate(self, part, width):
        widths, changed = part
        sizes = self.page.sizes
        room = width - sizes["fixed"] - (sizes["reset"] if changed else 0)
        x, lines = 0, 1
        for w in widths:
            if x and x + w > room:
                x, lines = 0, lines + 1
            x += w + 6
        flow = lines * self._line + (lines - 1) * 6
        return max(self._face, flow) + 12

    def fit(self, row):
        """Takes the real height of a shown row, which can differ from the estimate."""
        index = next((i for i, w in self.live.items() if w is row), None)
        if index is None or self._width is None:
            return
        self._parts[index] = self._part(row.emoji)
        self._totals = {}
        height = row.heightForWidth(self._width)
        if height != self._heights[index]:
            self._heights[index] = height
            self._sum()
            self.updateGeometry()
            self._place()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._measure(self.width())
        self.fill()

    def fill(self):
        """Builds the rows in or near sight, and drops those far from it."""
        seen = self.visibleRegion().boundingRect()
        if seen.isEmpty() or not self.rows:
            return
        margin = max(seen.height(), 200)
        first = max(bisect_right(self._tops, seen.top() - margin) - 1, 0)
        last = min(bisect_left(self._tops, seen.bottom() + margin), len(self.rows))
        focus = QApplication.focusWidget()
        for index in list(self.live):
            widget = self.live[index]
            if not first <= index < last and not widget.isAncestorOf(focus):
                del self.live[index]
                widget.hide()
                widget.deleteLater()
        for index in range(first, last):
            if index not in self.live:
                widget = EmojiRow(self.page, self.rows[index], self)
                self.live[index] = widget
                widget.show()
        self._place()
        for index in range(first, last):
            self.fit(self.live[index])

    def _place(self):
        for index, widget in self.live.items():
            widget.setGeometry(0, self._tops[index], self.width(), self._heights[index])

    def refresh(self):
        for widget in self.live.values():
            widget.show_words()
        self.remeasure()


class EmojiPage(DynamicSettingsPage):
    def __init__(self, dbus_handler, parent=None):
        self.words = EmojiWords(dbus_handler)
        self._remember_saved()
        self._query = ""
        self._group = None
        self._only_on = False
        self._shown = []
        self._example = None
        self._last_emoji = None
        self.view = None
        self.drafts = {}
        self.face_font = brand.sized_font(28)
        self.sizes = _row_sizes()
        super().__init__(dbus_handler, category=SettingsCategory.EMOJI, parent=parent)
        self._filter_timer = QTimer(self)
        self._filter_timer.setSingleShot(True)
        self._filter_timer.setInterval(FILTER_DELAY_MS)
        self._filter_timer.timeout.connect(self._apply_filter)
        bar = self.scroll.verticalScrollBar()
        bar.valueChanged.connect(self._fill_view)

    def _remember_saved(self):
        self._saved_edits = dict(self.words.edits)
        self._saved_apps = list(self.words.off_apps)
        # Defaults write the empty list even when it matches: the keyboard may have added apps.
        self._apps_reset = False

    def load_config(self, values=None):
        super().load_config(values)
        if not self.container_layout.count():
            return
        self._example = self._how_it_works()
        self.form.addRow("", self._example)
        self.form.addRow(self._separator())
        self.form.addRow(_("Apps without emoji") + ":", self._apps_section())
        self.form.addRow(self._separator())
        self.form.addRow(_("Emoji and their words") + ":", self._words_section())
        self._apply_locks()
        self._show_apps()
        self._apply_filter()

    def _apply_locks(self):
        super()._apply_locks()
        if self._example is not None:
            on = str(self.model.value_of("EmojiSuggestions")).lower() == "true"
            self._example.setEnabled(on)
        english = str(self.model.value_of("EnglishEmojiKeywords")).lower() == "true"
        if english != self.words.english:
            self.words.english = english
            if self.view is not None:
                self._apply_filter()

    def _how_it_works(self):
        example = QWidget()
        layout = QHBoxLayout(example)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(8)
        steps = [_pill("cười"), _pill("Tab: 😂 …"), _keys_pill(), _pill("cười 🤣")]
        for index, step in enumerate(steps):
            if index:
                arrow = QLabel("→")
                arrow.searchable = False
                layout.addWidget(arrow)
            layout.addWidget(step)
        layout.addStretch()
        return example

    def _apps_section(self):
        section = QWidget()
        layout = QVBoxLayout(section)
        layout.setContentsMargins(0, 0, 0, 0)
        hint = HintLabel(self._toggle_hint())
        self._apps_hint = hint
        mute(hint)
        layout.addWidget(hint)
        self._apps = Lines()
        self._apps.setMaximumWidth(460)
        layout.addWidget(self._apps)
        return section

    def _show_apps(self):
        while self._apps.rows.count():
            widget = self._apps.rows.takeAt(0).widget()
            widget.hide()
            widget.deleteLater()
        if not self.words.off_apps:
            empty = QLabel(_("Not turned off in any app."))
            empty.searchable = False
            empty.setContentsMargins(10, 8, 10, 8)
            mute(empty)
            self._apps.rows.addWidget(empty)
        for app in self.words.off_apps:
            row = Row()
            layout = QHBoxLayout(row)
            layout.setContentsMargins(10, 4, 6, 4)
            name = QLabel(app)
            name.searchable = False
            layout.addWidget(name, 1)
            button = QPushButton(_("Turn back on"))
            button.setAccessibleName(_("Turn emoji back on in {}").format(app))
            button.clicked.connect(lambda _checked=False, a=app: self._enable_app(a))
            layout.addWidget(button)
            self._apps.rows.addWidget(row)

    def _enable_app(self, app):
        undo = self.words.enable_app(app)
        window = self.window()
        if undo is None:
            self._show_message(_("Could not save. Check that fcitx5 is running."), error=True)
            return
        self._saved_apps = list(self.words.off_apps)
        self._show_apps()

        def take_back():
            done = undo()
            if done:
                self._saved_apps = list(self.words.off_apps)
            self._show_apps()
            if hasattr(window, "update_reset_button_state"):
                window.update_reset_button_state()
            return done

        self._show_message(_("Emoji are back on in {}.").format(app), undo=take_back)
        if hasattr(window, "update_reset_button_state"):
            window.update_reset_button_state()

    def _show_message(self, text, undo=None, error=False):
        window = self.window()
        if hasattr(window, "show_page_message"):
            window.show_page_message(text, undo=undo, error=error)

    def _words_section(self):
        section = QWidget()
        layout = QVBoxLayout(section)
        layout.setContentsMargins(0, 0, 0, 0)
        hint = HintLabel(
            _(
                "Click a word to turn it off or on. Dashed words are off; type a new word in the box"
                " next to them to add it. A word on for several emoji offers all of them."
            )
        )
        mute(hint)
        layout.addWidget(hint)

        filters = QWidget()
        filters_layout = QVBoxLayout(filters)
        filters_layout.setContentsMargins(0, 0, 0, 0)
        tools = QHBoxLayout()
        self.search = QLineEdit()
        self.search.setPlaceholderText(_("Find an emoji or a word, e.g. tiền"))
        self.search.setAccessibleName(self.search.placeholderText())
        self.search.setClearButtonEnabled(True)
        self.search.setText(self._query)
        self.search.textChanged.connect(self._on_query)
        self.search.returnPressed.connect(self._apply_filter)
        tools.addWidget(self.search, 1)
        self.only_on = self._toggle_button(_("Only emoji with a word on"))
        self.only_on.setChecked(self._only_on)
        self.only_on.toggled.connect(self._on_only_on)
        tools.addWidget(self.only_on)
        self.export_button = QPushButton(_("Export…"))
        self.export_button.setToolTip(_("Save the words you turned on, to share them"))
        self.export_button.clicked.connect(self._choose_export)
        tools.addWidget(self.export_button)
        self.import_button = QPushButton(_("Import…"))
        self.import_button.setToolTip(_("Add the words from a file someone shared"))
        self.import_button.clicked.connect(self._choose_import)
        tools.addWidget(self.import_button)
        filters_layout.addLayout(tools)

        groups = QWidget()
        flow = FlowLayout(groups)
        self.group_buttons = QButtonGroup(groups)
        for group, label in ((None, N_("All")), *zip(GROUPS, GROUP_LABELS)):
            # A lone & would underline the next letter as a shortcut.
            button = self._toggle_button(_(label).replace("&", "&&"))
            button.setChecked(group == self._group)
            button.clicked.connect(lambda _checked=False, g=group: self._on_group(g))
            self.group_buttons.addButton(button)
            flow.addWidget(button)
        filters_layout.addWidget(groups)
        layout.addWidget(filters)

        self._list = Lines()
        self.view = RowsView(self)
        self._list.rows.addWidget(self.view)
        layout.addWidget(self._list)
        self.count_line = QLabel()
        self.count_line.searchable = False
        mute(self.count_line)
        layout.addWidget(self.count_line)
        return section

    def _choose_export(self):
        path, _filter = QFileDialog.getSaveFileName(
            self,
            _("Export Emoji Words"),
            os.path.join(os.path.expanduser("~"), "ngosen-emoji-words.json"),
            _("Emoji words (*.json);;All Files (*)"),
        )
        if path:
            self.export_to(path)

    def _choose_import(self):
        path, _filter = QFileDialog.getOpenFileName(
            self,
            _("Import Emoji Words"),
            os.path.expanduser("~"),
            _("Emoji words (*.json);;All Files (*)"),
        )
        if path:
            self.import_from(path)

    def export_to(self, path):
        try:
            count = export_words(self.words, path)
        except OSError as e:
            self._show_message(_("Could not export: {}").format(e), error=True)
            return
        self._show_message(_("Saved {} words to {}.").format(count, path))

    def import_from(self, path):
        try:
            pairs = read_shared(path)
        except (OSError, ValueError) as e:
            self._show_message(_("Could not import: {}").format(e), error=True)
            return
        added = self.words.merge(pairs)
        if not added:
            self._show_message(_("You already have every word in that file."))
            return
        self.words_changed(_("{} words from the file").format(added))
        self._redraw_words()
        self._apply_filter()

    def _toggle_button(self, text):
        button = QPushButton(text)
        button.setCheckable(True)
        brand.follow_theme(button, _paint_toggle, source=self)
        button.toggled.connect(lambda _checked: _paint_toggle(button))
        return button

    def _on_query(self, text):
        self._query = text
        self._filter_timer.start()

    def _on_only_on(self, checked):
        self._only_on = checked
        self._apply_filter()

    def _on_group(self, group):
        self._group = group
        self._apply_filter()

    def _apply_filter(self):
        if getattr(self, "_filter_timer", None) is not None:
            self._filter_timer.stop()
        self._shown = self.words.rows(self._query, self._group, self._only_on)
        self.view.set_rows(self._shown)
        self._list.setVisible(bool(self._shown))
        count = len(self._shown)
        self.count_line.setText(_("{} emoji").format(count) if count else _("No emoji match"))

    def _fill_view(self, *_args):
        if self.view is not None:
            self.view.fill()

    def _toggle_hint(self, values=None):
        values = values or {}
        keys = [values.get(k, self.value_of(k)) for k in ("ModeMenuKey", "ShortcutEmoji")]
        return toggle_hint(*keys)

    def showEvent(self, event):
        super().showEvent(event)
        # The Keys page keeps its own copy of the settings, so ask fcitx5.
        self._apps_hint.setText(self._toggle_hint(self.dbus.get_config().get("values")))
        # Built hidden at window start, the form may have chosen its layout for a stale width.
        QTimer.singleShot(0, self._update_wrap_policy)
        QTimer.singleShot(0, self._fill_view)

    def _redraw_words(self):
        if self.view is not None:
            self.view.refresh()

    def words_changed(self, emoji):
        self._last_emoji = emoji
        window = self.window()
        if hasattr(window, "on_changed"):
            window.on_changed()

    def update_config(self, key, new_value):
        self._last_emoji = None
        super().update_config(key, new_value)

    def change_label(self):
        return self._last_emoji or super().change_label()

    def is_modified(self):
        return (
            super().is_modified()
            or self.words.is_modified()
            or self._apps_reset
            or self.words.off_apps != self._saved_apps
        )

    def is_modified_from_default(self):
        return super().is_modified_from_default() or self.words.is_modified_from_default()

    def save_data(self) -> bool:
        if not super().save_data():
            return False
        if self.words.is_modified() and not self.words.save():
            # Show what fcitx5 still holds, not an edit that did not take.
            self.words.edits = dict(self._saved_edits)
            self._redraw_words()
            return False
        apps_changed = self._apps_reset or self.words.off_apps != self._saved_apps
        if apps_changed and not self.words.save_apps():
            self.words.off_apps = list(self._saved_apps)
            self._show_apps()
            return False
        self._remember_saved()
        return True

    def load_data(self):
        self.words.load()
        self._remember_saved()
        super().load_data()

    def restore_defaults(self):
        self.words.restore_defaults()
        self._apps_reset = True
        super().restore_defaults()
        self._show_apps()
        self._redraw_words()


def _step_frame(frame):
    frame.setFrameShape(QFrame.StyledPanel)
    frame.setBackgroundRole(QPalette.Base)
    frame.setAutoFillBackground(True)
    return frame


def _pill(text):
    label = _step_frame(QLabel(text))
    label.searchable = False
    label.setMargin(4)
    return label


def _keys_pill():
    pill = _step_frame(QFrame())
    layout = QHBoxLayout(pill)
    layout.setContentsMargins(6, 2, 8, 2)
    layout.setSpacing(6)
    layout.addWidget(KeyCap("Tab"))
    choices = QLabel("1😂 2🤣 3😄 4😆 5😁")
    choices.searchable = False
    layout.addWidget(choices)
    return pill


def _paint_toggle(button):
    # The accent fill, through the palette rather than a style sheet, so the style draws the button.
    palette = QApplication.palette(button)
    if button.isChecked():
        palette.setColor(QPalette.Button, palette.color(QPalette.Highlight))
        palette.setColor(QPalette.ButtonText, palette.color(QPalette.HighlightedText))
    button.setPalette(palette)


def _row_sizes():
    """What a row's parts measure, to estimate the height of rows not built yet."""
    probe = Chip("", "")
    metrics = probe.fontMetrics()
    widths = {}

    def word(text):
        if text not in widths:
            widths[text] = metrics.horizontalAdvance(text) + 2 * Chip.PAD_X + 2
        return widths[text]

    reset = QPushButton(_("As before"))
    reset.setFlat(True)
    return {
        "chip": probe.sizeHint().height(),
        "word": word,
        "reset": reset.sizeHint().width() + ROW_SPACING,
        "fixed": 2 * ROW_MARGIN + FACE_WIDTH + ROW_SPACING,
    }
