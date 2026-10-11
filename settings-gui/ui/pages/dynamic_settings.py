# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Dynamic Settings Page with Card-based Layout matching modern guidelines.
"""

from enum import Enum

from core.dbus_handler import NgoSenDBusHandler
from i18n import N_, _
from qtpy.QtCore import QSize, Qt, QTimer
from qtpy.QtGui import QColor, QIcon, QPalette
from qtpy.QtWidgets import (
    QButtonGroup,
    QCheckBox,
    QComboBox,
    QFormLayout,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QStyle,
    QVBoxLayout,
    QWidget,
)

from ui import brand
from ui.components import (
    HotkeyEditorWidget,
    SingleKeyCaptureWidget,
)
from ui.typography import emphasize, mute, page_title


class SettingsCategory(Enum):
    APPEARANCE = "appearance"
    TYPING = "typing"
    SHORTCUTS = "shortcuts"


MODE_SWITCHING = N_("Mode switching")

# Groups show in this order; the first checkbox of a group carries the group name.
SETTINGS_MAP = {
    SettingsCategory.TYPING: {
        N_("Typing"): ["Mode", "InputMethod", "OutputCharset"],
        N_("Tone marks"): [
            "SpellCheck",
            "AutoNonVnRestore",
            "DdFreeStyle",
            "ModernStyle",
            "FreeMarking",
            "W2U",
            "BracketTransform",
        ],
        N_("Quick typing"): [
            "DoubleSpaceToPeriod",
            "DoubleHyphenToEmDash",
            "AutoCapitalizeAfterPunctuation",
            "useSurroundingTextIfPossible",
        ],
    },
    SettingsCategory.SHORTCUTS: {
        N_("Hotkeys"): ["ModeMenuKey", "CycleModeKey"],
        MODE_SWITCHING: [
            "ShortcutSen",
            "ShortcutPreedit",
            "ShortcutEmoji",
            "ShortcutOff",
            "ShortcutDefault",
        ],
    },
    SettingsCategory.APPEARANCE: {
        N_("Tray icon"): ["UseLotusIcons", "IconTheme"],
    },
}

# One line under a setting, saying what the user sees.
HINTS = {
    "SpellCheck": N_("Words that are not Vietnamese get no tone marks."),
    "AutoNonVnRestore": N_("Typing windows still gives windows."),
    "FreeMarking": N_("Tone marks can also go at the end of the word."),
    "useSurroundingTextIfPossible": N_("Turn this off if an app repeats letters."),
}

MODE_BUTTON_ORDER = ["Sen", "Preedit", "Emoji Picker", "OFF"]

# What a word looks like while typing it, shown under the input method.
TYPING_EXAMPLES = {
    "Telex": "vieetj → việt",
    "VNI": "vie65t → việt",
    "VIQR": "vie^.t → việt",
    "Telex + VNI": "vieetj → việt",
    "Telex + VNI + VIQR": "vieetj → việt",
}

# Below this width the labels go above their fields.
NARROW_WIDTH = 620


MODE_SHORTCUT_TO_VISIBILITY = {
    "ShortcutSen": "ShowModeSen",
    "ShortcutPreedit": "ShowModePreedit",
    "ShortcutEmoji": "ShowModeEmoji",
    "ShortcutOff": "ShowModeOff",
    "ShortcutDefault": "ShowModeDefault",
}

MODE_KEY_TO_INTERNAL_NAME = {
    "ShortcutSen": "Sen",
    "ShortcutPreedit": "Preedit",
    "ShortcutEmoji": "Emoji",
    "ShortcutOff": "Off",
    "ShortcutDefault": "Default",
}

MODE_SHORTCUT_KEYS = list(MODE_SHORTCUT_TO_VISIBILITY.keys())


class CardWidget(QFrame):
    """A visual container (Card) for grouping related settings."""

    def __init__(self, title: str, parent=None):
        super().__init__(parent)
        self.setObjectName("SettingCard")

        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(16, 16, 16, 16)
        self.main_layout.setSpacing(12)

        if title:
            title_label = QLabel(title)
            title_label.setObjectName("CardTitle")
            self.main_layout.addWidget(title_label)

        self.content_layout = QVBoxLayout()
        self.content_layout.setSpacing(10)
        self.main_layout.addLayout(self.content_layout)


class DynamicSettingsPage(QWidget):
    def __init__(
        self,
        dbus_handler: NgoSenDBusHandler,
        category: SettingsCategory = SettingsCategory.TYPING,
        parent=None,
    ):
        super().__init__(parent)
        self.dbus = dbus_handler
        self.category = category
        self.current_values = {}
        self.initial_values = {}
        self.modified_values = {}
        self.button_groups = []
        self.shortcut_labels = {}
        self.shortcut_warning_labels = {}
        self.validation_errors = []
        self.list_widgets = []  # Track list widgets for layout refresh

        self._setup_ui()
        self.load_config()

    def showEvent(self, event):
        """Force layout refresh when page becomes visible to fix initialization overflow."""
        super().showEvent(event)
        QTimer.singleShot(0, self._refresh_list_layouts)

    def _refresh_list_layouts(self):
        """Recalculate heights for all QListWidgets once the window has final dimensions."""
        for lw in self.list_widgets:
            total_h = 0
            for i in range(lw.count()):
                item = lw.item(i)
                widget = lw.itemWidget(item)
                if widget:
                    h = widget.sizeHint().height()
                    item.setSizeHint(QSize(100, h))
                    total_h += lw.visualItemRect(item).height()
            lw.setFixedHeight(total_h + 2 * lw.frameWidth())
            lw.update()

    def _setup_ui(self):
        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)

        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll.setFrameShape(QFrame.NoFrame)

        self.container = QWidget()
        self.container_layout = QVBoxLayout(self.container)
        self.container_layout.setContentsMargins(30, 20, 30, 20)
        self.container_layout.setSpacing(20)

        self.scroll.setWidget(self.container)
        self.layout.addWidget(self.scroll)

    def load_config(self, values=None):
        """Shows the saved settings, or `values` as unsaved changes on top of them."""
        self.blockSignals(True)
        try:
            while self.container_layout.count():
                item = self.container_layout.takeAt(0)
                if item.widget():
                    item.widget().deleteLater()
            self.button_groups.clear()
            self.modified_values.clear()
            self.shortcut_labels.clear()
            self.shortcut_warning_labels.clear()
            self.list_widgets.clear()

            config_data = self.dbus.get_config()
            if not config_data:
                # The window already says fcitx5 cannot be reached.
                return

            saved_values = config_data.get("values", {})
            self.current_values = dict(saved_values if values is None else values)
            self.modified_values = {
                k: v for k, v in self.current_values.items() if saved_values.get(k) != v
            }
            metadata_list = config_data.get("metadata", [])
            if not metadata_list:
                return

            # Flat map all items for easy lookup
            self.all_metadata = {}
            for group in metadata_list:
                for item in group[1]:
                    self.all_metadata[item[0]] = item

            if not self.property("inTab"):
                self.container_layout.addWidget(page_title(_(self.category.name.capitalize())))

            form_widget = QWidget()
            self.form = QFormLayout(form_widget)
            self.form.setLabelAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self.form.setFieldGrowthPolicy(QFormLayout.AllNonFixedFieldsGrow)
            self._update_wrap_policy()
            self.container_layout.addWidget(form_widget)

            for index, (group, keys) in enumerate(SETTINGS_MAP.get(self.category, {}).items()):
                if index:
                    self.form.addRow(self._separator())
                if group == MODE_SWITCHING:
                    self._render_mode_list(_(group) + ":")
                    continue
                group_label = _(group) + ":"
                for k in keys:
                    item = self.all_metadata.get(k)
                    if not item:
                        continue
                    if k in ["ModeMenuKey", "CycleModeKey"] or item[1] == "Hotkey":
                        self._render_hotkey(item)
                    elif k == "Mode":
                        self._render_mode_buttons(item)
                    elif "Enum" in item[4]:
                        self._render_combobox(item)
                    elif item[1] == "Boolean":
                        self._render_checkbox(item, group_label)
                        group_label = ""

            if self.category == SettingsCategory.TYPING:
                self.form.addRow(self._separator())
                try_it = QLineEdit()
                try_it.setPlaceholderText(_("Type here to test"))
                try_it.setFont(brand.typed_font(17))
                self.form.addRow(_("Try it:"), try_it)

            self.initial_values = saved_values.copy()
            self.container_layout.addStretch()
        finally:
            self.blockSignals(False)

    def is_modified_from_default(self):
        if not hasattr(self, "all_metadata"):
            return False
        for key, val in self.current_values.items():
            meta = self.all_metadata.get(key)
            if meta:
                default_val = meta[3]
                # Handle cases where default_val might be a dict (like hotkeys)
                if isinstance(default_val, dict) and isinstance(val, dict):
                    if str(val.get("0")) != str(default_val.get("0")):
                        return True
                elif str(val) != str(default_val):
                    return True
        return False

    def is_modified(self):
        """Returns True if the current values differ from the initial loaded values."""
        return self.current_values != self.initial_values

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._update_wrap_policy()

    def _update_wrap_policy(self):
        if getattr(self, "form", None) is None:
            return
        narrow = self.width() < NARROW_WIDTH
        self.form.setRowWrapPolicy(QFormLayout.WrapAllRows if narrow else QFormLayout.DontWrapRows)

    @staticmethod
    def _separator():
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setFrameShadow(QFrame.Sunken)
        return line

    def _with_hint(self, widget, key):
        hint_text = HINTS.get(key)
        if not hint_text:
            return widget
        wrapper = QWidget()
        layout = QVBoxLayout(wrapper)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(widget)
        # Wrapping makes the form reserve two lines for a one-line hint.
        hint = QLabel(_(hint_text))
        mute(hint)
        if isinstance(widget, QCheckBox):
            # Line the hint up with the checkbox text, not its box.
            style = widget.style()
            indent = style.pixelMetric(QStyle.PM_IndicatorWidth) + style.pixelMetric(
                QStyle.PM_CheckBoxLabelSpacing
            )
            hint.setContentsMargins(indent, 0, 0, 0)
        layout.addWidget(hint)
        return wrapper

    def _render_hotkey(self, item):
        key, type_str, label, default, annotations = item
        val = self.current_values.get(key, default)
        hotkey_str = val.get("0", "") if isinstance(val, dict) else ""

        hk_btn = HotkeyEditorWidget(hotkey_str)
        hk_btn.setFixedWidth(235)
        hk_btn.textChanged.connect(
            lambda text, k=key: self.update_config(k, self._with_first_key(k, text))
        )
        self.form.addRow(_(label) + ":", self._with_hint(hk_btn, key))

    def _with_first_key(self, key, text):
        # Only the first key of a list is shown; the others still work and must survive an edit.
        val = self.current_values.get(key)
        keys = [val[i] for i in sorted(val, key=int)] if isinstance(val, dict) else []
        keys = ([text] if text else []) + keys[1:]
        return {str(i): k for i, k in enumerate(keys)}

    def _render_combobox(self, item):
        key, type_str, label, default, annotations = item
        val = str(self.current_values.get(key, default))

        combo = QComboBox()
        combo.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        enum_dict = annotations.get("Enum", {})
        sorted_keys = sorted(enum_dict.keys(), key=lambda x: int(x) if str(x).isdigit() else x)
        for k in sorted_keys:
            rb_text = str(enum_dict[k])
            combo.addItem(_(rb_text), rb_text)

        idx = combo.findData(val)
        if idx >= 0:
            combo.setCurrentIndex(idx)

        combo.currentIndexChanged.connect(
            lambda _index, k=key: self.update_config(k, combo.currentData())
        )
        field = self._with_example(combo) if key == "InputMethod" else combo
        self.form.addRow(_(label) + ":", self._with_hint(field, key))

    def _with_example(self, combo):
        wrapper = QWidget()
        layout = QVBoxLayout(wrapper)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        layout.addWidget(combo)
        example = QLabel()
        example.setObjectName("TypingExample")
        example.setFont(brand.typed_font(16))
        layout.addWidget(example)

        def show_example():
            text = TYPING_EXAMPLES.get(combo.currentData(), "")
            example.setText(text)
            example.setVisible(bool(text))

        combo.currentIndexChanged.connect(lambda _index: show_example())
        show_example()
        return wrapper

    def _render_mode_buttons(self, item):
        key, type_str, label, default, annotations = item
        val = str(self.current_values.get(key, default))
        row = QWidget()
        row.setObjectName("ModeButtons")
        row.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        layout = QHBoxLayout(row)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        group = QButtonGroup(row)
        modes = set(annotations.get("Enum", {}).values())
        for mode in MODE_BUTTON_ORDER:
            if mode not in modes:
                continue
            button = QPushButton(_(mode))
            button.setCheckable(True)
            button.setChecked(mode == val)
            button.clicked.connect(lambda _checked=False, m=mode: self.update_config(key, m))
            group.addButton(button)
            layout.addWidget(button)
        brand.follow_theme(row, self._style_mode_buttons, source=self)
        self.button_groups.append(group)
        self.form.addRow(_(label) + ":", row)

    def _style_mode_buttons(self, row):
        enamel = brand.pick(self, brand.ENAMEL)
        row.setStyleSheet(
            f"QWidget#ModeButtons QPushButton:checked {{ background: {enamel};"
            f" color: {brand.ON_ENAMEL}; border: 1px solid {enamel}; border-radius: 4px;"
            " padding: 4px 12px; }"
        )

    def _render_checkbox(self, item, group_label):
        key, type_str, label, default, annotations = item
        val = self.current_values.get(key, default)

        cb = QCheckBox(_(label))
        cb.setChecked(str(val).lower() == "true")
        cb.toggled.connect(
            lambda checked, k=key: self.update_config(k, "True" if checked else "False")
        )
        self.form.addRow(group_label, self._with_hint(cb, key))

    def _render_string(self, item, layout):
        key, type_str, label, default, annotations = item
        val = str(self.current_values.get(key, default))

        wrapper = QVBoxLayout()
        wrapper.setSpacing(2)
        wrapper.setContentsMargins(0, 0, 0, 0)

        row_layout = QHBoxLayout()
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.setSpacing(8)

        # If it's a shortcut key, add the corresponding visibility checkbox
        if key in MODE_SHORTCUT_TO_VISIBILITY:
            visibility_key = MODE_SHORTCUT_TO_VISIBILITY[key]
            visibility_val = self.current_values.get(visibility_key, "True")

            cb = QCheckBox()
            cb.setChecked(str(visibility_val).lower() == "true")
            cb.toggled.connect(
                lambda checked, k=visibility_key: self.update_config(
                    k, "True" if checked else "False"
                )
            )
            row_layout.addWidget(cb)

        label_widget = QLabel(_(label))
        row_layout.addWidget(label_widget)
        row_layout.addStretch()

        if key in MODE_SHORTCUT_KEYS:
            # Use single key capture for shortcuts
            capture_btn = SingleKeyCaptureWidget(val)
            capture_btn.setFixedWidth(100)
            capture_btn.textChanged.connect(lambda text, k=key: self.update_config(k, text))
            row_layout.addWidget(capture_btn)
        else:
            edit = QLineEdit(val)
            edit.setFixedWidth(56)
            edit.setMaxLength(1)
            edit.setPlaceholderText("-")
            edit.textChanged.connect(lambda text, k=key: self.update_config(k, text))
            row_layout.addWidget(edit)

        wrapper.addLayout(row_layout)

        if key in MODE_SHORTCUT_KEYS:
            warning = QLabel()
            warning.setObjectName("ShortcutWarning")
            brand.follow_theme(warning, self._color_warning)
            warning.setWordWrap(True)
            warning.hide()
            wrapper.addWidget(warning)
            self.shortcut_labels[key] = label_widget
            self.shortcut_warning_labels[key] = warning

        layout.addLayout(wrapper)

    def _color_warning(self, warning):
        palette = warning.palette()
        palette.setColor(QPalette.WindowText, QColor(brand.pick(warning, brand.ERROR_BORDER)))
        warning.setPalette(palette)

    def _render_mode_list(self, label):
        from qtpy.QtWidgets import QAbstractItemView, QListWidget, QListWidgetItem

        card = CardWidget("")
        card.main_layout.setContentsMargins(0, 0, 0, 0)
        card.content_layout.setSpacing(4)

        list_widget = QListWidget()
        list_widget.setDragDropMode(QAbstractItemView.InternalMove)
        list_widget.setResizeMode(QListWidget.Adjust)
        list_widget.setSelectionMode(QAbstractItemView.SingleSelection)
        list_widget.setFocusPolicy(Qt.NoFocus)
        list_widget.setFrameShape(QFrame.NoFrame)
        list_widget.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        list_widget.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        list_widget.viewport().setAutoFillBackground(False)

        # Get current order from config
        order_str = self.current_values.get(
            "ModeOrder",
            "Sen,Preedit,Emoji,Off,Default",
        )
        # Sen was called Uinput, and before that Smooth, Super Smooth and Minecraft; Surrounding Text was
        # merged into it.
        order = []
        for name in order_str.split(","):
            name = (
                "Sen"
                if name in ("Uinput", "Smooth", "SuperSmooth", "Minecraft", "SurroundingText")
                else name
            )
            if name not in order:
                order.append(name)

        # Ensure all modes are present
        all_internal_names = list(MODE_KEY_TO_INTERNAL_NAME.values())
        for name in all_internal_names:
            if name not in order:
                order.append(name)

        # Map internal name back to shortcut key
        internal_to_key = {v: k for k, v in MODE_KEY_TO_INTERNAL_NAME.items()}

        total_height = 0
        for name in order:
            key = internal_to_key.get(name)
            if not key:
                continue

            item_meta = self.all_metadata.get(key)
            if not item_meta:
                continue

            list_item = QListWidgetItem(list_widget)
            container = QWidget()
            row_layout = QHBoxLayout(container)
            row_layout.setContentsMargins(4, 2, 4, 2)
            row_layout.setSpacing(4)

            # Drag handle icon with fallback
            handle = QLabel()
            icon = QIcon.fromTheme("list-drag-handle")
            if icon.isNull():
                icon = QIcon.fromTheme("view-restore")
            if icon.isNull():
                icon = QIcon.fromTheme("grabber")

            if not icon.isNull():
                handle.setPixmap(icon.pixmap(16, 16))
            else:
                handle.setText("☰")
                mute(handle)

            handle.setFixedSize(24, 24)
            handle.setAlignment(Qt.AlignCenter)
            row_layout.addWidget(handle)

            # Use the existing render logic but into our row
            self._render_string(item_meta, row_layout)

            hint = container.sizeHint()
            # ONLY use the height from hint, use small width to let QListWidget expand it properly.
            # Large width hints from QHBoxLayout+Stretch cause overflow.
            list_item.setSizeHint(QSize(100, hint.height()))
            total_height += hint.height()

            list_widget.addItem(list_item)
            list_widget.setItemWidget(list_item, container)

            # Store internal name in the item's data for reordering
            list_item.setData(Qt.UserRole, name)

        list_widget.setFixedHeight(total_height + 2 * list_widget.frameWidth())
        self.list_widgets.append(list_widget)
        list_widget.model().rowsMoved.connect(lambda *args: self._update_mode_order(list_widget))

        card.content_layout.addWidget(list_widget)
        instructions = QLabel(
            _(
                "Drag the handle on the left to reorder modes in the menu. Use checkboxes to toggle visibility, and click the buttons to reassign shortcuts."
            )
        )
        instructions.setWordWrap(True)
        mute(instructions)
        card.content_layout.addWidget(instructions)
        self.form.addRow(label, card)

    def _update_mode_order(self, list_widget):
        new_order = []
        for i in range(list_widget.count()):
            item = list_widget.item(i)
            new_order.append(item.data(Qt.UserRole))

        self.update_config("ModeOrder", ",".join(new_order))

    def _validate_mode_shortcuts(self):
        """Check for duplicate shortcuts among enabled modes."""
        self.validation_errors.clear()

        # Reset warnings and styles
        for key in MODE_SHORTCUT_KEYS:
            if key in self.shortcut_warning_labels:
                self.shortcut_warning_labels[key].hide()
            if key in self.shortcut_labels:
                self.shortcut_labels[key].setFont(self.font())

        # Only check enabled modes
        enabled_shortcuts = {}
        for shortcut_key, visibility_key in MODE_SHORTCUT_TO_VISIBILITY.items():
            default_visibility = "True"
            vis_meta = (
                self.all_metadata.get(visibility_key) if hasattr(self, "all_metadata") else None
            )
            if vis_meta:
                default_visibility = vis_meta[3]

            is_enabled = (
                str(self.current_values.get(visibility_key, default_visibility)).lower() == "true"
            )
            if is_enabled:
                default_shortcut = ""
                shortcut_meta = (
                    self.all_metadata.get(shortcut_key) if hasattr(self, "all_metadata") else None
                )
                if shortcut_meta:
                    default_shortcut = shortcut_meta[3]
                val = self.current_values.get(shortcut_key, default_shortcut)
                if val:
                    if val in enabled_shortcuts:
                        enabled_shortcuts[val].append(shortcut_key)
                    else:
                        enabled_shortcuts[val] = [shortcut_key]

        # Flag duplicates
        for shortcut, keys in enabled_shortcuts.items():
            if len(keys) > 1:
                error_msg = _("Duplicate shortcut '{}' used for multiple enabled modes.").format(
                    shortcut
                )
                self.validation_errors.append(error_msg)
                for key in keys:
                    if key in self.shortcut_warning_labels:
                        self.shortcut_warning_labels[key].setText(error_msg)
                        self.shortcut_warning_labels[key].show()
                    if key in self.shortcut_labels:
                        emphasize(self.shortcut_labels[key])

    def load_data(self):
        """Standardized reload method (alias for load_config)."""
        self.load_config()

    def restore_defaults(self):
        """Resets current values to engine defaults."""
        self.blockSignals(True)
        try:
            config_data = self.dbus.get_config()
            if not config_data:
                return

            defaults = {
                item[0]: item[3] for group in config_data.get("metadata", []) for item in group[1]
            }
            self.load_config(values=defaults)
        finally:
            self.blockSignals(False)

    def has_validation_errors(self):
        self._validate_mode_shortcuts()
        return bool(self.validation_errors)

    def validation_message(self):
        return "\n".join(self.validation_errors)

    def save_data(self) -> bool:
        """Commits all staged changes to DBus."""
        if self.has_validation_errors():
            return False

        if not self.modified_values:
            return True

        config_data = self.dbus.get_config()
        if config_data:
            latest_values = config_data.get("values", {})
            latest_values.update(self.modified_values)
            if not self.dbus.set_config(latest_values):
                return False
            self.modified_values.clear()
            self.initial_values = self.current_values.copy()
            return True
        elif not self.dbus.iface:
            return False
        return True

    def change_label(self):
        item = getattr(self, "all_metadata", {}).get(getattr(self, "_last_changed_key", None))
        return _(item[2]) if item else None

    def update_config(self, key: str, new_value):
        """Updates internal state and notifies parent window of change."""
        self.modified_values[key] = new_value
        self.current_values[key] = new_value
        self._last_changed_key = key

        # Real-time validation if it's a shortcut
        if key in MODE_SHORTCUT_KEYS or key in MODE_SHORTCUT_TO_VISIBILITY.values():
            self._validate_mode_shortcuts()

        # Notify the parent window (NgoSenSettingsWindow) if it exists
        main_win = self.window()
        if hasattr(main_win, "on_changed"):
            main_win.on_changed()
