# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Macro Editor Page. Edits ngosen-macro-table.conf.
Implements UI with row reordering and TSV import/export.
"""

from core.dbus_handler import NgoSenDBusHandler
from i18n import _
from qtpy.QtCore import Qt
from qtpy.QtGui import QColor, QIcon
from qtpy.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QComboBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from core import macros
from ui.helpers import add_help_icon
from ui.pages.base_editor import BaseEditorPage
from ui.pages.dynamic_settings import CardWidget
from ui.typography import mute, page_title


class MacroEditorPage(BaseEditorPage):
    """UI for editing Ngó Sen macros."""

    def __init__(
        self,
        dbus_handler: NgoSenDBusHandler,
        parent=None,
    ):
        super().__init__(parent)
        self.dbus = dbus_handler
        self.macros = macros.MacroTable(dbus_handler)
        self._setup_ui()
        self.load_data()

    def _setup_ui(self):
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(30, 20, 30, 20)
        main_layout.setSpacing(15)

        title = page_title(_("Macros"))
        main_layout.addWidget(title)

        # Macro behavior toggles
        toggles_card = CardWidget()
        toggles_layout = QHBoxLayout()
        self.cb_enable = QCheckBox(_("Enable Macro"))
        self.cb_capitalize = QCheckBox(_("Capitalize Macro"))
        self.cb_enable_off_mode = QCheckBox(_("Macro in Off Mode"))
        self.cb_enable.toggled.connect(self._on_item_changed)
        self.cb_capitalize.toggled.connect(self._on_item_changed)
        self.cb_enable_off_mode.toggled.connect(self._on_item_changed)
        toggles_layout.addWidget(self.cb_enable)

        cap_layout = QHBoxLayout()
        cap_layout.setSpacing(5)
        cap_layout.addWidget(self.cb_capitalize)
        add_help_icon(cap_layout, "CapitalizeMacro")

        toggles_layout.addLayout(cap_layout)

        off_mode_layout = QHBoxLayout()
        off_mode_layout.setSpacing(5)
        off_mode_layout.addWidget(self.cb_enable_off_mode)
        add_help_icon(off_mode_layout, "EnableMacroInOffMode")
        toggles_layout.addLayout(off_mode_layout)

        toggles_layout.addStretch()

        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText(_("Search macros..."))
        self.search_input.setClearButtonEnabled(True)
        self.search_input.setFixedWidth(200)
        self.search_input.textChanged.connect(self.on_search_changed)

        toggles_card.content_layout.addLayout(toggles_layout)
        main_layout.addWidget(toggles_card)

        # Main content area
        editor_card = CardWidget()
        content_layout = QVBoxLayout()
        editor_card.content_layout.addLayout(content_layout)
        main_layout.addWidget(editor_card)

        # Dynamic Macro Settings (Moved below the macro table)
        dynamic_card = CardWidget()
        dynamic_layout = QVBoxLayout()

        # Hint text
        hint_label = QLabel(_("Macros support dynamic placeholders: $TIME and $DATE."))
        hint_label.setWordWrap(True)
        mute(hint_label)
        dynamic_layout.addWidget(hint_label)

        # Format Inputs
        fmt_container = QWidget()
        fmt_hbox = QHBoxLayout(fmt_container)
        fmt_hbox.setContentsMargins(0, 5, 0, 0)
        fmt_hbox.setSpacing(20)

        # Time Format
        time_layout = QHBoxLayout()
        self.input_time_format = QComboBox()
        self.input_time_format.setEditable(False)
        self.input_time_format.currentIndexChanged.connect(self._on_item_changed)

        time_presets = [
            ("%H:%M", "15:04 (24h)"),
            ("%H:%M:%S", "15:04:05 (24h)"),
            ("%I:%M %p", "03:04 PM"),
            ("%I:%M:%S %p", "03:04:05 PM"),
            ("", _("None")),
        ]
        for fmt, desc in time_presets:
            self.input_time_format.addItem(fmt, fmt)
            self.input_time_format.setItemData(
                self.input_time_format.count() - 1, desc, Qt.ToolTipRole
            )

        time_layout.addWidget(QLabel(_("Time Format:")))
        time_layout.addWidget(self.input_time_format, 1)

        # Date Format
        date_layout = QHBoxLayout()
        self.input_date_format = QComboBox()
        self.input_date_format.setEditable(False)
        self.input_date_format.currentIndexChanged.connect(self._on_item_changed)

        date_presets = [
            ("%d/%m/%Y", "dd/MM/yyyy"),
            ("%d/%m/%y", "dd/MM/yy"),
            ("%m/%d/%Y", "MM/dd/yyyy"),
            ("%Y-%m-%d", "yyyy-MM-dd"),
            ("%y-%m-%d", "yy-MM-dd"),
            ("", _("None")),
        ]
        for fmt, desc in date_presets:
            self.input_date_format.addItem(fmt, fmt)
            self.input_date_format.setItemData(
                self.input_date_format.count() - 1, desc, Qt.ToolTipRole
            )

        date_layout.addWidget(QLabel(_("Date Format:")))
        date_layout.addWidget(self.input_date_format, 1)

        fmt_hbox.addLayout(time_layout)
        fmt_hbox.addLayout(date_layout)

        dynamic_layout.addWidget(fmt_container)

        dynamic_card.content_layout.addLayout(dynamic_layout)
        main_layout.addWidget(dynamic_card)

        # 1. Input Row (Top)
        input_layout = QHBoxLayout()
        self.input_key = QLineEdit()
        self.input_key.setPlaceholderText(_("Abbreviation (e.g. kg)"))
        self.input_key.setClearButtonEnabled(True)

        self.input_val = QLineEdit()
        self.input_val.setPlaceholderText(_("Full text (e.g. khô gà)"))
        self.input_val.setClearButtonEnabled(True)
        self.input_val.returnPressed.connect(self.on_add)

        self.btn_add = QPushButton(QIcon.fromTheme("list-add"), _("Add"))
        self.btn_add.clicked.connect(self.on_add)
        self.input_key.textChanged.connect(self._update_add_button_icon)
        self.input_val.textChanged.connect(self._update_add_button_icon)

        input_layout.addWidget(QLabel(_("Key:")))
        input_layout.addWidget(self.input_key, 1)
        input_layout.addWidget(QLabel(_("Value:")))
        input_layout.addWidget(self.input_val, 2)
        input_layout.addWidget(self.btn_add)
        content_layout.addLayout(input_layout)

        # 2. Table Area
        self.table = QTableWidget(0, 2)
        self.table.setHorizontalHeaderLabels([_("Abbreviation"), _("Expanded Text")])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.table.setAlternatingRowColors(True)
        self.apply_table_style()  # Apply custom table styling
        self.table.cellClicked.connect(self.on_row_selected)
        content_layout.addWidget(self.table)

        # 3. Bottom Toolbar
        toolbar_layout = QHBoxLayout()
        toolbar_layout.setContentsMargins(0, 5, 0, 0)

        self.btn_remove = QPushButton(QIcon.fromTheme("list-remove"), _("Remove"))
        self.btn_up = QPushButton(QIcon.fromTheme("go-up"), "")
        self.btn_up.setToolTip(_("Move Up"))
        self.btn_down = QPushButton(QIcon.fromTheme("go-down"), "")
        self.btn_down.setToolTip(_("Move Down"))

        self.btn_remove.clicked.connect(self.on_remove)
        self.btn_up.clicked.connect(self.on_move_up)
        self.btn_down.clicked.connect(self.on_move_down)

        toolbar_layout.addWidget(self.btn_remove)
        toolbar_layout.addWidget(self.btn_up)
        toolbar_layout.addWidget(self.btn_down)
        toolbar_layout.addStretch()
        toolbar_layout.addWidget(QLabel(_("Search:")))
        toolbar_layout.addWidget(self.search_input)

        content_layout.addLayout(toolbar_layout)
        self.update_button_states()

    def load_data(self):
        self.blockSignals(True)
        try:
            self.macros.load()
            self._show_options()
            self.table.setRowCount(0)
            for item in self.macros.entries:
                self.upsert_row(item["Key"], item["Value"], sort=False)
            self.on_search_changed()
            # The table merges keys that differ only in case and drops unknown formats,
            # so what it shows is the baseline, not what was read.
            self._sync_to_model()
            self.macros.saved_state = self.macros.state()
        finally:
            self.blockSignals(False)

    def _show_options(self):
        options = self.macros.options
        self.cb_enable.setChecked(options["EnableMacro"])
        self.cb_capitalize.setChecked(options["CapitalizeMacro"])
        self.cb_enable_off_mode.setChecked(options["EnableMacroInOffMode"])
        for combo, key in (
            (self.input_time_format, "TimeFormat"),
            (self.input_date_format, "DateFormat"),
        ):
            index = combo.findData(options[key])
            if index < 0:
                # Only the listed formats can be picked, so an unknown one falls back.
                index = combo.findData(macros.DEFAULTS[key])
            combo.setCurrentIndex(index)

    def _sync_to_model(self):
        self.macros.options = {
            "EnableMacro": self.cb_enable.isChecked(),
            "CapitalizeMacro": self.cb_capitalize.isChecked(),
            "EnableMacroInOffMode": self.cb_enable_off_mode.isChecked(),
            "TimeFormat": self.input_time_format.currentText(),
            "DateFormat": self.input_date_format.currentText(),
        }
        entries = []
        for row in range(self.table.rowCount()):
            key_item = self.table.item(row, 0)
            val_item = self.table.item(row, 1)
            if key_item and key_item.text():
                entries.append(
                    {"Key": key_item.text(), "Value": val_item.text() if val_item else ""}
                )
        self.macros.entries = entries

    def restore_defaults(self):
        """Resets macros to default (empty table, enabled checkboxes)."""
        self.blockSignals(True)
        try:
            self.macros.reset()
            self._show_options()
            self.table.setRowCount(0)
            self._on_item_changed()
        finally:
            self.blockSignals(False)

    def is_modified_from_default(self):
        self._sync_to_model()
        return self.macros.is_modified_from_default()

    def is_modified(self):
        self._sync_to_model()
        return self.macros.is_modified()

    def save_data(self) -> bool:
        self._sync_to_model()
        return self.macros.save()

    def _find_row_by_key(self, key: str) -> int | None:
        """Finds row index for a given key. Returns None if not found."""
        # The typing core ignores the case of a macro key.
        key = key.lower()
        for r in range(self.table.rowCount()):
            item = self.table.item(r, 0)
            if item and item.text().lower() == key:
                return r
        return None

    def upsert_row(self, key: str, value: str, sort: bool = True):
        # Update existing
        row = self._find_row_by_key(key)
        if row is not None:
            self.table.item(row, 1).setText(value)
            self._apply_row_highlight(row, key)
            if sort:
                self.on_search_changed()  # Re-apply filter
            self.update_button_states()
            self._on_item_changed()
            return

        # Insert new
        row = self.table.rowCount()
        self.table.insertRow(row)
        self.table.setItem(row, 0, QTableWidgetItem(key))
        self.table.setItem(row, 1, QTableWidgetItem(value))
        self._apply_row_highlight(row, key)
        self.on_search_changed()
        if sort:
            self.on_search_changed()  # Re-apply filter
        self.update_button_states()
        self._on_item_changed()

    def _apply_row_highlight(self, row: int, key: str):
        """Applies red background and warning icon to rows with invalid keys."""
        is_invalid = macros.is_invalid_key(key)
        bg_color = Qt.transparent
        tooltip = ""
        icon = QIcon()
        if is_invalid:
            # Use a soft red for warning background
            bg_color = QColor(Qt.red)
            bg_color.setAlpha(60)
            icon = QIcon.fromTheme("dialog-warning")
            tooltip = _("Macro keys cannot contain spaces or special characters.")

        for col in range(self.table.columnCount()):
            item = self.table.item(row, col)
            if item:
                item.setBackground(bg_color)
                item.setToolTip(tooltip)
                # Show icon in the first column
                if col == 0:
                    item.setIcon(icon)
                else:
                    item.setIcon(QIcon())

    def on_search_changed(self):
        """Filters the table rows based on the search input."""
        search_text = self.search_input.text().lower()
        for row in range(self.table.rowCount()):
            key_item = self.table.item(row, 0)
            val_item = self.table.item(row, 1)
            key = key_item.text().lower() if key_item else ""
            val = val_item.text().lower() if val_item else ""

            # Show row if either key or value matches search text
            self.table.setRowHidden(row, search_text not in key and search_text not in val)

    def on_add(self):
        key = self.input_key.text().strip()
        val = self.input_val.text().strip()
        if not key or not val:
            return

        self.upsert_row(key, val)
        self.input_key.clear()
        self.input_val.clear()
        self.input_key.setFocus()

    def _update_add_button_icon(self):
        """Changes the Add button icon to Update if key exists and handles validation."""
        key = self.input_key.text().strip()
        val = self.input_val.text().strip()
        is_invalid = macros.is_invalid_key(key)

        # Validation feedback for input field
        if is_invalid:
            self.input_key.setStyleSheet("color: red;")
            self.input_key.setToolTip(_("Macro keys cannot contain spaces or special characters."))
        else:
            self.input_key.setStyleSheet("")
            self.input_key.setToolTip("")

        # Disable button if key is invalid, empty, or value is empty
        self.btn_add.setEnabled(not is_invalid and bool(key) and bool(val))

        found = self._find_row_by_key(key) is not None
        if found:
            self.btn_add.setIcon(QIcon.fromTheme("document-save"))
            self.btn_add.setText(_("Update"))
        else:
            self.btn_add.setIcon(QIcon.fromTheme("list-add"))
            self.btn_add.setText(_("Add"))

    def on_row_selected(self, row, column):
        key_item = self.table.item(row, 0)
        if key_item:
            self.input_key.setText(key_item.text())
        val_item = self.table.item(row, 1)
        if val_item:
            self.input_val.setText(val_item.text())
        self.update_button_states()
