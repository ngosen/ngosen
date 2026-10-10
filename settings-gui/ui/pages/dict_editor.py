# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Dictionary Editor Page. Edits the user dictionary.
Implements UI with row reordering and TSV import/export.
"""

import os
import tempfile

from core.dbus_handler import NgoSenDBusHandler
from i18n import _
from qtpy.QtCore import QItemSelectionModel, QSize, QStringListModel, Qt
from qtpy.QtGui import QColor, QIcon
from qtpy.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListView,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
)

from ui.pages.base_editor import BaseEditorPage
from ui.pages.dynamic_settings import CardWidget
from ui.typography import mute, page_title


class WordListModel(QStringListModel):
    """The dictionary words, with words the engine cannot use flagged."""

    item_size = QSize()

    def data(self, index, role=Qt.DisplayRole):
        if role == Qt.SizeHintRole:
            return self.item_size
        if role in (Qt.BackgroundRole, Qt.ToolTipRole, Qt.DecorationRole):
            if " " not in (super().data(index, Qt.DisplayRole) or ""):
                return None
            if role == Qt.BackgroundRole:
                color = QColor(Qt.red)
                color.setAlpha(60)
                return color
            if role == Qt.ToolTipRole:
                return _("Warning: Dictionary words should not contain spaces.")
            return QIcon.fromTheme("dialog-warning")
        return super().data(index, role)


class DictEditorPage(BaseEditorPage):
    """UI for editing the Ngó Sen dictionary."""

    def __init__(
        self,
        dbus_handler: NgoSenDBusHandler,
        parent=None,
    ):
        super().__init__(parent)
        self.dbus = dbus_handler
        self.words = []  # List of all words
        self.initial_state = {}
        self._is_loaded = False
        self._load_failed = False
        self._setup_ui()

    def _get_local_dict_path(self) -> str:
        xdg_data_home = os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share"))
        return os.path.join(xdg_data_home, "fcitx5/ngosen/vietnamese.cm.dict")

    def _get_global_dict_path(self) -> str:
        # Common locations for fcitx5 pkgdata
        paths = [
            "/usr/share/fcitx5/ngosen/vietnamese.cm.dict",
            "/usr/local/share/fcitx5/ngosen/vietnamese.cm.dict",
        ]
        for p in paths:
            if os.path.exists(p):
                return p
        return paths[0]

    def _setup_ui(self):
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(30, 20, 30, 20)
        main_layout.setSpacing(15)

        title = page_title(_("Custom Dictionary"))
        main_layout.addWidget(title)

        explanation = QLabel(
            _(
                "Words in the custom dictionary will be excluded from 'Auto Restore Invalid Words'. Use this for names, technical terms, or words not yet in the built-in dictionary."
            )
        )
        explanation.setWordWrap(True)
        mute(explanation)
        main_layout.addWidget(explanation)

        # Dictionary behavior toggles
        toggles_card = CardWidget("")
        toggles_layout = QHBoxLayout()
        self.cb_enable = QCheckBox(_("Custom Dictionary"))
        self.cb_enable.toggled.connect(self._on_item_changed)
        toggles_layout.addWidget(self.cb_enable)
        toggles_layout.addStretch()

        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText(_("Search words..."))
        self.search_input.setClearButtonEnabled(True)
        self.search_input.setFixedWidth(200)
        self.search_input.textChanged.connect(self.on_search_changed)
        toggles_layout.addWidget(QLabel(_("Search:")))
        toggles_layout.addWidget(self.search_input)

        toggles_card.content_layout.addLayout(toggles_layout)
        main_layout.addWidget(toggles_card)

        # Main content area
        editor_card = CardWidget("")
        content_layout = QVBoxLayout()
        editor_card.content_layout.addLayout(content_layout)
        main_layout.addWidget(editor_card)

        # 1. Input Row (Top)
        input_layout = QHBoxLayout()
        self.input_word = QLineEdit()
        self.input_word.setPlaceholderText(_("Word (e.g. khongdau)"))
        self.input_word.setClearButtonEnabled(True)
        self.input_word.returnPressed.connect(self.on_add)

        self.btn_add = QPushButton(QIcon.fromTheme("list-add"), _("Add"))
        self.btn_add.clicked.connect(self.on_add)
        self.input_word.textChanged.connect(self._update_add_button_icon)

        input_layout.addWidget(QLabel(_("Word:")))
        input_layout.addWidget(self.input_word, 1)
        input_layout.addWidget(self.btn_add)
        content_layout.addLayout(input_layout)

        # The view holds only the matching words; filtering in Qt would call the Python
        # data() of every word on each key press.
        self.model = WordListModel(self)
        self.list_view = QListView()
        self.list_view.setModel(self.model)
        self.list_view.setFlow(QListView.LeftToRight)
        self.list_view.setWrapping(True)
        self.list_view.setResizeMode(QListView.Adjust)
        self.list_view.setUniformItemSizes(True)
        # Uniform sizes ask only the first word, so every cell gets the full column width.
        self.model.item_size = QSize(144, self.fontMetrics().height() + 6)
        self.list_view.setGridSize(QSize(150, self.fontMetrics().height() + 10))
        self.list_view.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.list_view.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.list_view.clicked.connect(self.on_word_clicked)
        self.list_view.selectionModel().selectionChanged.connect(self.update_button_states)
        content_layout.addWidget(self.list_view)

        # 3. Bottom Toolbar
        toolbar_layout = QHBoxLayout()
        toolbar_layout.setContentsMargins(0, 5, 0, 0)

        self.btn_remove = QPushButton(QIcon.fromTheme("list-remove"), _("Remove"))
        self.btn_remove.clicked.connect(self.on_remove)

        toolbar_layout.addWidget(self.btn_remove)
        toolbar_layout.addStretch()

        content_layout.addLayout(toolbar_layout)
        self.update_button_states()

    def showEvent(self, event):
        super().showEvent(event)
        if not self._is_loaded:
            self.load_data()
            self._is_loaded = True

    def load_data(self):
        self.blockSignals(True)
        self._load_failed = False
        try:
            # Load global dictionary settings via DBus
            config_data = self.dbus.get_config()
            if config_data:
                values = config_data.get("values", {})
                self.cb_enable.setChecked(
                    str(values.get("EnableDictionary", "False")).lower() == "true"
                )

            self.words = []
            local_path = self._get_local_dict_path()
            path_to_read = (
                local_path if os.path.exists(local_path) else self._get_global_dict_path()
            )

            if os.path.exists(path_to_read):
                try:
                    self.words = self._read_words(path_to_read)
                except Exception as e:
                    self._load_failed = True
                    print(f"Failed to read dictionary {path_to_read}: {e}")
                    QMessageBox.warning(
                        self,
                        _("Warning"),
                        _(
                            "Failed to load dictionary completely: {}\nSaving is disabled to prevent data loss."
                        ).format(e),
                    )

            self._show_words()
            self.initial_state = self._get_current_state()
        finally:
            self.blockSignals(False)
            self.on_search_changed()
            self.update_button_states()

    @staticmethod
    def _read_words(path):
        with open(path, "r", encoding="utf-8") as f:
            return [w for w in (line.strip() for line in f) if w and not w.startswith("#")]

    def _bundled_words(self):
        try:
            return self._read_words(self._get_global_dict_path())
        except OSError:
            return []

    def _show_words(self):
        needle = self.search_input.text().strip().casefold()
        self.model.setStringList([w for w in self.words if needle in w.casefold()])
        self.update_button_states()

    def shown_words(self):
        return self.model.stringList()

    def flagged_words(self):
        rows = (self.model.index(r, 0) for r in range(self.model.rowCount()))
        return [i.data() for i in rows if i.data(Qt.BackgroundRole) is not None]

    def select_words(self, words):
        selection = self.list_view.selectionModel()
        selection.clearSelection()
        for r in range(self.model.rowCount()):
            index = self.model.index(r, 0)
            if index.data() in words:
                selection.select(index, QItemSelectionModel.Select)

    def update_button_states(self, *_args):
        self.btn_remove.setEnabled(self.list_view.selectionModel().hasSelection())

    def restore_defaults(self):
        """Turns the custom dictionary off and goes back to the bundled words."""
        self.blockSignals(True)
        try:
            self.cb_enable.setChecked(False)
            self.words = self._bundled_words()
            self._show_words()
            self._on_item_changed()
        finally:
            self.blockSignals(False)

    def is_modified_from_default(self):
        """Returns True if the dictionary is on or its words differ from the bundled ones."""
        return self.cb_enable.isChecked() or sorted(self.words) != sorted(self._bundled_words())

    def is_modified(self):
        """Returns True if the current state differs from the initial loaded state."""
        return self._get_current_state() != self.initial_state

    def _get_current_state(self):
        """Captures the current UI state for comparison."""
        return {
            "words": sorted(self.words),
            "EnableDictionary": self.cb_enable.isChecked(),
        }

    def save_data(self) -> bool:
        if self._load_failed:
            QMessageBox.warning(
                self,
                _("Error"),
                _(
                    "Cannot save dictionary because loading failed earlier. Please fix the file format first."
                ),
            )
            return False

        # Save global dictionary settings via DBus
        config_data = self.dbus.get_config()
        if config_data:
            values = config_data.get("values", {})
            values["EnableDictionary"] = "True" if self.cb_enable.isChecked() else "False"
            if not self.dbus.set_config(values):
                return False
        elif not self.dbus.iface:
            return False

        local_path = self._get_local_dict_path()
        try:
            if sorted(self.words) == sorted(self._bundled_words()):
                # A copy of the bundled words would hide later updates to them.
                if os.path.exists(local_path):
                    os.remove(local_path)
            else:
                target_dir = os.path.dirname(local_path)
                os.makedirs(target_dir, exist_ok=True)

                with tempfile.NamedTemporaryFile(
                    "w", dir=target_dir, encoding="utf-8", delete=False
                ) as tf:
                    for word in self.words:
                        tf.write(f"{word}\n")
                    temp_name = tf.name

                os.replace(temp_name, local_path)

            self.dbus.reload_addon_config()
            self.initial_state = self._get_current_state()
            return True
        except Exception as e:
            if "temp_name" in locals() and os.path.exists(temp_name):
                try:
                    os.remove(temp_name)
                except OSError:
                    pass
            QMessageBox.warning(self, _("Error"), _("Failed to save dictionary: {}").format(e))
            return False

    def upsert_row(self, word: str, sort: bool = True):
        if word in self.words:
            return
        self.words.append(word)
        if sort:
            self.words.sort()
        self._show_words()
        self._on_item_changed()

    def _is_invalid_word(self, word: str) -> bool:
        """Checks if word contains spaces."""
        if not word:
            return False
        return " " in word

    def on_search_changed(self):
        self._show_words()

    def on_add(self):
        word = self.input_word.text().strip()
        if not word:
            return

        self.upsert_row(word)
        self.input_word.clear()
        self.input_word.setFocus()

    def _update_add_button_icon(self):
        """Handles validation and Add button state."""
        word = self.input_word.text().strip()
        is_invalid = self._is_invalid_word(word)

        if is_invalid:
            self.input_word.setStyleSheet("color: red;")
            self.input_word.setToolTip(_("Warning: Dictionary words should not contain spaces."))
        else:
            self.input_word.setStyleSheet("")
            self.input_word.setToolTip("")

        self.btn_add.setEnabled(not is_invalid and bool(word))

        if word in self.words:
            self.btn_add.setIcon(QIcon.fromTheme("document-save"))
            self.btn_add.setText(_("Exists"))
            self.btn_add.setEnabled(False)
        else:
            self.btn_add.setIcon(QIcon.fromTheme("list-add"))
            self.btn_add.setText(_("Add"))

    def on_word_clicked(self, index):
        self.input_word.setText(index.data())

    def on_remove(self):
        selected = {i.data() for i in self.list_view.selectionModel().selectedIndexes()}
        if not selected:
            return
        self.words = [w for w in self.words if w not in selected]
        self._show_words()
        self.update_button_states()
        self._on_item_changed()
        self._update_add_button_icon()
