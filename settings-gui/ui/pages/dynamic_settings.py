# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Dynamic Settings Page with Card-based Layout matching modern guidelines.
"""

from enum import Enum

from core.dbus_handler import NgoSenDBusHandler
from i18n import N_, _
from qtpy.QtCore import QEvent, QSize, Qt, QTimer
from qtpy.QtGui import QColor, QIcon, QPalette, QPixmap
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
TRAY_ICON = N_("Tray icon")

# Under "Show in the tray menu" each checkbox names the menu entry and shows its icon.
MENU_ENTRIES = {
    "ShowMenuInputMethod": (N_("Input Method"), "input-keyboard"),
    "ShowMenuCharset": (N_("Charset"), "character-set"),
    "ShowMenuSpellCheck": (N_("Spell Check"), "tools-check-spelling"),
    "ShowMenuMacro": (N_("Macro"), "document-edit"),
    "ShowMenuCapitalizeMacro": (N_("Capitalize Macro"), "format-text-uppercase"),
    "ShowMenuAutoCapitalize": (N_("Auto Capitalize"), "format-text-capitalize"),
    "ShowMenuAutoRestore": (N_("Auto Restore Invalid Words"), "edit-undo"),
    "ShowMenuDictionary": (N_("Custom Dictionary"), "accessories-dictionary"),
}

# Groups show in this order; the first checkbox of a group carries the group name.
SETTINGS_MAP = {
    SettingsCategory.TYPING: {
        N_("Typing"): ["Mode", "InputMethod", "OutputCharset"],
        N_("Tone marks"): [
            "SpellCheck",
            "AutoNonVnRestore",
            "LockWordModifier",
            "DdFreeStyle",
            "ModernStyle",
            "FreeMarking",
            "BackspaceRemovesTone",
            "W2U",
            "BracketTransform",
        ],
        N_("Quick typing"): [
            "DoubleSpaceToPeriod",
            "DoubleHyphenToEmDash",
            "AutoCapitalizeAfterPunctuation",
            "CapitalizeStartOfText",
            "RetypeAfterSwitch",
            "RetoneSelection",
        ],
    },
    SettingsCategory.SHORTCUTS: {
        N_("Hotkeys"): ["ToggleKey", "ModeMenuKey", "CycleModeKey"],
        MODE_SWITCHING: [
            "ShortcutSen",
            "ShortcutPreedit",
            "ShortcutEmoji",
            "ShortcutOff",
            "ShortcutDefault",
        ],
    },
    SettingsCategory.APPEARANCE: {
        N_("Language"): ["UiLanguage"],
        TRAY_ICON: ["UseLotusIcons", "IconTheme"],
        N_("Show in the tray menu"): list(MENU_ENTRIES),
    },
}

# One line under a setting, saying what the user sees.
HINTS = {
    "ToggleKey": N_("Tap it alone to switch. The tray icon shows V while typing Vietnamese."),
    "SpellCheck": N_("Words that are not Vietnamese get no tone marks."),
    "AutoNonVnRestore": N_("Typing windows still gives windows."),
    "LockWordModifier": N_(
        "Tap the key alone to keep the word as shown: tunx, Ctrl → tũn, with no restore or macro. Alt also shows the menu bar in some apps."
    ),
    "FreeMarking": N_("Tone marks can also go at the end of the word."),
    "UiLanguage": N_("The tray menu changes at once, this window the next time it opens."),
    "BackspaceRemovesTone": N_(
        "z in Telex (0 in VNI) removes the tone mark even with this off. On a word without one, Backspace deletes as usual."
    ),
    "Mode": N_(
        "Sen types straight into the app. Preedit underlines the word until it is done; use it in an app that Sen does not work with."
    ),
    "OutputCharset": N_(
        "Keep Unicode unless an old document or app needs another one, such as TCVN3 (ABC). Cứu dấu works only with Unicode."
    ),
    "DdFreeStyle": N_("dd and Space stays đ instead of going back to dd."),
    "ModernStyle": N_("hoà, khoẻ, thuý, tuỳ. Off: hòa, khỏe, thúy, tùy."),
    "W2U": N_(
        "Non-Start: nhw → như, but w alone stays w. Everywhere: w alone gives ư too. uw always gives ư."
    ),
    "BracketTransform": N_("ch]a → chưa, m[f → mờ. Type a bracket twice to get it back: [[ → [."),
    "DoubleSpaceToPeriod": N_("ban and Space twice gives “ban. ”."),
    "DoubleHyphenToEmDash": N_("a--b → a—b."),
    "AutoCapitalizeAfterPunctuation": N_("ban. tooi → ban. Tôi. The same after Enter."),
    "CapitalizeStartOfText": N_(
        "xin → Xin in an empty field or on a new line. Not in address bars or some apps; on GNOME 47 and later, Chrome and similar apps skip an empty field."
    ),
    "RetypeAfterSwitch": N_(
        "Typed tieengs vieetj before switching on? Switch on and it becomes tiếng việt. Tab or Esc gives back what you typed."
    ),
    "RetoneSelection": N_("Select nhanh and type s: nhánh. Tab, Esc or s again gives back s."),
    "ModeMenuKey": N_(
        "Opens a menu to pick a mode for the app in use. Only while typing Vietnamese; in English the key types as usual."
    ),
    "CycleModeKey": N_(
        "Moves the app in use to the next mode in the list below, and remembers it for that app."
    ),
    "ModeOrder": N_(
        "With the mode menu open, press a mode's key to pick it for the app in use. Pick Default Typing to make the app follow the switch key again."
    ),
    "UseLotusIcons": N_("Off: plain one-colour icons, in the colour picked below."),
    "IconTheme": N_(
        "Only for one-colour icons. Light suits a light panel, Dark a dark one, and Auto follows the light or dark setting of the desktop."
    ),
}

MODE_BUTTON_ORDER = ["Sen", "Preedit", "Emoji Picker", "OFF"]

# Each language is named in itself, so it can be found whichever one the window is in.
LANGUAGE_NAMES = {"Vietnamese": "Tiếng Việt", "English": "English"}

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


class HintLabel(QLabel):
    """A hint that wraps when the page is too narrow for it, and is as tall as its lines."""

    def __init__(self, text):
        super().__init__(text)
        self.setWordWrap(True)

    # QFormLayout sizes a wrapping label for a guessed width, which left a blank line under short
    # hints; pin the height to the width the label really got.
    def resizeEvent(self, event):
        super().resizeEvent(event)
        height = self.heightForWidth(self.width())
        if height > 0 and height != self.minimumHeight():
            self.setFixedHeight(height)


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

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("SettingCard")

        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(16, 16, 16, 16)
        self.main_layout.setSpacing(12)

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
            self._tray_preview = None

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
                    if k == "ToggleKey":
                        self._render_toggle_key(item)
                    elif k in ["ModeMenuKey", "CycleModeKey"] or item[1] == "Hotkey":
                        self._render_hotkey(item)
                    elif k == "Mode":
                        self._render_mode_buttons(item)
                    elif "Enum" in item[4]:
                        self._render_combobox(item)
                    elif item[1] == "Boolean":
                        self._render_checkbox(item, group_label)
                        group_label = ""
                if group == TRAY_ICON:
                    self._render_tray_preview()

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

    def eventFilter(self, watched, event):
        # The list keeps each row's width from when the row was laid out, so a resized window cuts off the key buttons.
        if event.type() == QEvent.Resize:
            for list_widget in self.list_widgets:
                if list_widget.viewport() is watched:
                    self._fit_mode_rows(list_widget)
        return super().eventFilter(watched, event)

    def _update_wrap_policy(self):
        if getattr(self, "form", None) is None:
            return
        # Measured side by side only, since labels above fields need less width.
        if self.form.rowWrapPolicy() == QFormLayout.DontWrapRows:
            self._side_by_side_width = self.container.minimumSizeHint().width()
        too_wide = self.scroll.viewport().width() < getattr(self, "_side_by_side_width", 0)
        narrow = self.width() < NARROW_WIDTH or too_wide
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
        hint = HintLabel(_(hint_text))
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

    def _render_toggle_key(self, item):
        key, type_str, label, default, annotations = item
        # The default is Ctrl+Shift: either Ctrl with either Shift, in either order, as in UniKey.
        ctrl_shift = default if isinstance(default, dict) else {}

        def is_ctrl_shift():
            val = self.current_values.get(key, default)
            return isinstance(val, dict) and sorted(val.values()) == sorted(ctrl_shift.values())

        def shown_key():
            val = self.current_values.get(key, default)
            first = val.get("0", "") if isinstance(val, dict) else ""
            # Without a side, since either Ctrl and either Shift work.
            return "Control+Shift" if is_ctrl_shift() else first

        editor = HotkeyEditorWidget(shown_key())
        editor.setFixedWidth(235)
        field = QWidget()
        layout = QVBoxLayout(field)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(editor)
        hint = HintLabel(_(HINTS[key]))
        mute(hint)
        layout.addWidget(hint)

        # Upgrades kept Ctrl+Space; UniKey's key is one click away.
        offer = QWidget()
        offer_layout = QHBoxLayout(offer)
        offer_layout.setContentsMargins(0, 4, 0, 0)
        offer_text = QLabel(_("Switch with Ctrl+Shift, as in UniKey?"))
        use_ctrl_shift = QPushButton(_("Use Ctrl+Shift"))
        offer_layout.addWidget(offer_text)
        offer_layout.addWidget(use_ctrl_shift)
        offer_layout.addStretch(1)
        offer.setVisible(not is_ctrl_shift())
        layout.addWidget(offer)

        def choose_ctrl_shift():
            self.update_config(key, dict(ctrl_shift))
            editor.set_key(shown_key())
            offer.setVisible(False)

        def changed(text):
            # No key would leave no way back to Vietnamese; Ctrl+Shift recorded with either side or
            # in either order is still all of UniKey's key.
            if not text or text in ctrl_shift.values():
                choose_ctrl_shift()
                return
            # The Ctrl+Shift entries are one key to the user; other extra keys still work.
            new = {"0": text} if is_ctrl_shift() else self._with_first_key(key, text)
            self.update_config(key, new)
            offer.setVisible(not is_ctrl_shift())

        editor.textChanged.connect(changed)
        use_ctrl_shift.clicked.connect(choose_ctrl_shift)

        others = [n for n in (self.dbus.input_method_names() or []) if n != "ngosen"]
        if others:
            hint = HintLabel(
                _("fcitx5 has other input methods too; its own key still switches to them.")
            )
            mute(hint)
            layout.addWidget(hint)
        self.form.addRow(_(label) + ":", field)

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
            shown = LANGUAGE_NAMES.get(rb_text, rb_text) if key == "UiLanguage" else _(rb_text)
            combo.addItem(shown, rb_text)

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
        # Custom takes its keys only from the Keymap tab, so without one it types no marks.
        custom_hint = HintLabel(
            _("Custom has no keys yet: turn on Custom Keymap and add keys in Keys › Keymap.")
        )
        custom_hint.setObjectName("CustomKeymapHint")
        mute(custom_hint)
        layout.addWidget(custom_hint)
        has_keymap = str(
            self.current_values.get("EnableCustomKeymap", "False")
        ).lower() == "true" and bool(self.dbus.get_sub_config_list("custom_keymap", "CustomKeymap"))

        def show_example():
            text = TYPING_EXAMPLES.get(combo.currentData(), "")
            example.setText(text)
            example.setVisible(bool(text))
            custom_hint.setVisible(combo.currentData() == "Custom" and not has_keymap)

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
        self.form.addRow(_(label) + ":", self._with_hint(row, key))

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
        if key in MENU_ENTRIES:
            text, icon_name = MENU_ENTRIES[key]
            cb.setText(_(text))
            icon = brand.theme_icon(icon_name)
            if icon.isNull():
                # Keep the text in line with the entries that have an icon.
                blank = QPixmap(cb.iconSize())
                blank.fill(Qt.transparent)
                icon = QIcon(blank)
            cb.setIcon(icon)
        cb.setChecked(str(val).lower() == "true")
        cb.toggled.connect(
            lambda checked, k=key: self.update_config(k, "True" if checked else "False")
        )
        self.form.addRow(group_label, self._with_hint(cb, key))

    def _tray_icon_names(self):
        """The icons the tray shows for Vietnamese and English, and whether they suit a dark panel."""
        if str(self.current_values.get("UseLotusIcons", "True")).lower() == "true":
            return ("fcitx-ngosen", "fcitx-ngosen-off"), brand.is_dark(self)
        color = str(self.current_values.get("IconTheme", "Auto"))
        dark = {"Light": False, "Dark": True}.get(color, brand.is_dark(self))
        suffix = "-default" if dark else "-default-black"
        return (f"fcitx-ngosen{suffix}", f"fcitx-ngosen-off{suffix}"), dark

    def _render_tray_preview(self):
        preview = QLabel()
        preview.setObjectName("TrayIconPreview")
        preview.setAccessibleName(_("Tray icons for Vietnamese and English"))
        caption = HintLabel(_("Vietnamese, English"))
        mute(caption)
        row = QWidget()
        layout = QHBoxLayout(row)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(8)
        layout.addWidget(preview)
        layout.addWidget(caption)
        layout.addStretch()
        self._tray_preview = preview
        brand.follow_theme(preview, lambda _w: self._update_tray_preview())
        self.form.addRow(_("In the tray:"), row)

    def _update_tray_preview(self):
        preview = getattr(self, "_tray_preview", None)
        if preview is None:
            return
        names, dark = self._tray_icon_names()
        preview.setPixmap(brand.tray_preview(names, dark))

    def _render_string(self, item, layout):
        key, type_str, label, default, annotations = item
        val = str(self.current_values.get(key, default))

        wrapper = QVBoxLayout()
        wrapper.setSpacing(2)
        wrapper.setContentsMargins(0, 0, 0, 0)

        row_layout = QHBoxLayout()
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.setSpacing(8)

        visibility_key = MODE_SHORTCUT_TO_VISIBILITY[key]
        visibility_val = self.current_values.get(visibility_key, "True")
        cb = QCheckBox()
        cb.setChecked(str(visibility_val).lower() == "true")
        cb.toggled.connect(
            lambda checked, k=visibility_key: self.update_config(k, "True" if checked else "False")
        )
        row_layout.addWidget(cb)

        label_widget = QLabel(_(label))
        row_layout.addWidget(label_widget)
        row_layout.addStretch()

        capture_btn = SingleKeyCaptureWidget(val)
        capture_btn.setFixedWidth(100)
        capture_btn.textChanged.connect(lambda text, k=key: self.update_config(k, text))
        row_layout.addWidget(capture_btn)

        wrapper.addLayout(row_layout)

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

        card = CardWidget()
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
        # The addon renames old mode names before it hands the config out.
        order = list(dict.fromkeys(order_str.split(",")))

        # Ensure all modes are present
        all_internal_names = list(MODE_KEY_TO_INTERNAL_NAME.values())
        for name in all_internal_names:
            if name not in order:
                order.append(name)

        # Map internal name back to shortcut key
        internal_to_key = {v: k for k, v in MODE_KEY_TO_INTERNAL_NAME.items()}

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

            list_widget.addItem(list_item)
            list_widget.setItemWidget(list_item, container)

            # Store internal name in the item's data for reordering
            list_item.setData(Qt.UserRole, name)

        self._fit_mode_rows(list_widget)
        list_widget.viewport().installEventFilter(self)
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
        keys_hint = HintLabel(_(HINTS["ModeOrder"]))
        mute(keys_hint)
        card.content_layout.addWidget(keys_hint)
        self.form.addRow(label, card)

    @staticmethod
    def _fit_mode_rows(list_widget):
        # Item heights are fixed, so a row only grows for its warning when told to.
        total_height = 0
        for i in range(list_widget.count()):
            item = list_widget.item(i)
            row = list_widget.itemWidget(item)
            # A warning hidden just now still counts until the layout recomputes.
            row.layout().activate()
            height = row.sizeHint().height()
            item.setSizeHint(QSize(list_widget.viewport().width(), height))
            total_height += height
        list_widget.setFixedHeight(total_height + 2 * list_widget.frameWidth())

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
        for list_widget in self.list_widgets:
            self._fit_mode_rows(list_widget)

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
        if key in ("UseLotusIcons", "IconTheme"):
            self._update_tray_preview()

        # Real-time validation if it's a shortcut
        if key in MODE_SHORTCUT_KEYS or key in MODE_SHORTCUT_TO_VISIBILITY.values():
            self._validate_mode_shortcuts()

        # Notify the parent window (NgoSenSettingsWindow) if it exists
        main_win = self.window()
        if hasattr(main_win, "on_changed"):
            main_win.on_changed()
