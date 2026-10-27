# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Reusable UI components.
"""

from core.hotkeys import (
    HOTKEY_SYM_MAP,
    HOTKEY_UI_UNSHIFT_MAP,
    MODIFIER_ORDER,
    keysym_name,
    pretty_format_hotkey_parts,
)
from i18n import _
from qtpy.QtCore import QEvent, Qt, Signal
from qtpy.QtGui import QIcon, QKeySequence
from qtpy.QtWidgets import QHBoxLayout, QLabel, QPushButton, QWidget

from ui.typography import emphasize, mute


class KeyCap(QLabel):
    """A label styled as a keyboard keycap."""

    def __init__(self, text, parent=None):
        super().__init__(text, parent)
        self.setAlignment(Qt.AlignCenter)
        self.setObjectName("KeyCap")
        self.setStyleSheet(
            "QLabel#KeyCap { background-color: palette(button); color: palette(button-text);"
            " border: 1px solid palette(mid); border-bottom: 2px solid palette(dark);"
            " border-radius: 4px; padding: 2px 6px; font-weight: bold; font-family: monospace; }"
        )


class HelpIcon(QLabel):
    """A reusable help icon with a tooltip."""

    def __init__(self, text="", parent=None):
        super().__init__(parent)
        # Try symbolic icon first for monochrome look, fall back to standard
        icon = QIcon.fromTheme("help-about-symbolic")
        if icon.isNull():
            icon = QIcon.fromTheme("help-about")

        self.setPixmap(icon.pixmap(32, 32))
        self.setScaledContents(True)
        if text:
            self.setToolTip(text)
        self.setFixedSize(16, 16)


class HotkeyEditorWidget(QWidget):
    """A widget containing HotkeyCaptureWidget with a clear button."""

    textChanged = Signal(str)

    def __init__(self, current_key="", parent=None):
        super().__init__(parent)
        self._setup_ui(current_key)

    def _setup_ui(self, current_key):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        self.hotkey_capture = HotkeyCaptureWidget(current_key)
        self.hotkey_capture.textChanged.connect(self._on_hotkey_changed)

        self.btn_clear = QPushButton(QIcon.fromTheme("delete"), "")
        self.btn_clear.setFixedWidth(30)
        self.btn_clear.setToolTip(_("Clear hotkey"))
        self.btn_clear.clicked.connect(self._on_clear)

        layout.addWidget(self.hotkey_capture)
        layout.addWidget(self.btn_clear)

    def _on_hotkey_changed(self, text):
        self.textChanged.emit(text)

    def set_key(self, text):
        """Shows `text` as the key, ending a recording, without emitting textChanged."""
        self.hotkey_capture.current_key = text
        self.hotkey_capture.setChecked(False)
        self.hotkey_capture._update_display()

    def _on_clear(self):
        self.set_key("")
        self.textChanged.emit("")


class HotkeyCaptureWidget(QPushButton):
    """A button that captures keystrokes to set an Fcitx5-compatible hotkey.
    Provides live visual feedback for held modifiers during recording."""

    textChanged = Signal(str)

    def __init__(self, current_key="", parent=None):
        super().__init__(parent)
        self.current_key = current_key
        self.setCheckable(True)
        self.setObjectName("HotkeyButton")

        # Local state to track held modifiers during recording
        self.record_mods = set()
        self.modifier_key = ""

        # Use a layout for visual keycaps
        self.main_layout = QHBoxLayout(self)
        self.main_layout.setContentsMargins(8, 2, 8, 2)
        self.main_layout.setSpacing(4)
        self.main_layout.setAlignment(Qt.AlignCenter)

        self.toggled.connect(self._on_toggled)
        self._update_display()

        # Install event filter to catch KeyPress, KeyRelease and ShortcutOverride
        self.installEventFilter(self)

    def eventFilter(self, obj, event):
        # Prevent mnemonics and navigation from triggering while we are recording a hotkey
        if obj == self and self.isChecked():
            if event.type() == QEvent.ShortcutOverride:
                event.accept()
                return True
            if event.type() == QEvent.KeyPress:
                self._handle_key_event(event)
                return True
            if event.type() == QEvent.KeyRelease:
                self._handle_release_event(event)
                return True
        return super().eventFilter(obj, event)

    def _on_toggled(self, checked):
        if checked:
            self.record_mods.clear()
            self.modifier_key = ""
        self._update_display()

    def _clear_layout(self):
        while self.main_layout.count():
            item = self.main_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

    def _update_display(self):
        self._clear_layout()

        if self.isChecked():
            if not self.record_mods:
                lbl = QLabel(_("[ Recording... ]"))
                emphasize(lbl)
                self.main_layout.addWidget(lbl)
            else:
                # Show current active modifiers
                for mod in MODIFIER_ORDER:
                    if mod in self.record_mods:
                        self.main_layout.addWidget(KeyCap(HOTKEY_SYM_MAP.get(mod, mod)))
                # Add a pulsing placeholder for the next key
                lbl = QLabel("...")
                emphasize(lbl)
                self.main_layout.addWidget(lbl)
        elif not self.current_key:
            lbl = QLabel(_("None"))
            mute(lbl)
            self.main_layout.addWidget(lbl)
        else:
            parts = pretty_format_hotkey_parts(self.current_key)
            for part in parts:
                cap = KeyCap(part)
                self.main_layout.addWidget(cap)

    def keyPressEvent(self, event):
        """Standard key event handler, only active when not recording."""
        if not self.isChecked():
            super().keyPressEvent(event)
            return

    def _handle_key_event(self, event):
        """Internal helper to process captured keys."""
        key_code = event.key()
        modifiers = event.modifiers()

        # Escape cancels the recording
        if key_code == Qt.Key_Escape:
            self.setChecked(False)
            return

        # Track modifiers in real-time
        mod_map = {
            Qt.Key_Control: "Control",
            Qt.Key_Shift: "Shift",
            Qt.Key_Alt: "Alt",
            Qt.Key_Meta: "Super",
        }
        if key_code in mod_map:
            # A held key repeats.
            if mod_map[key_code] in self.record_mods:
                return
            # Held modifiers plus this one's own name (Shift_L), as fcitx5 writes a modifier-only key.
            held = [m for m in MODIFIER_ORDER if m in self.record_mods]
            self.modifier_key = "+".join(
                held + [self._keysym_name(event) or mod_map[key_code] + "_L"]
            )
            self.record_mods.add(mod_map[key_code])
            self._update_display()
            return

        self.modifier_key = ""
        if key_code == Qt.Key_unknown:
            return

        # It's a base key! Capture it and finalize immediately
        base_key = self._keysym_name(event)
        # Normalize BackTab keysym to Tab to ensure Fcitx5 sees 'Shift+Tab' instead of plain 'ISO_Left_Tab'
        if base_key == "ISO_Left_Tab":
            base_key = "Tab"

        if not base_key:
            base_key = (
                event.text()
                if event.text() and event.text().isprintable()
                else QKeySequence(key_code).toString()
            )

        has_non_shift_modifier = bool(
            modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)
        )
        is_ascii_alpha = len(base_key) == 1 and base_key.isascii() and base_key.isalpha()

        if has_non_shift_modifier and is_ascii_alpha:
            base_key = base_key.upper()

        # Build modifier list from current event state
        mods = []
        if modifiers & Qt.ControlModifier:
            mods.append("Control")
        if modifiers & Qt.AltModifier:
            mods.append("Alt")
        if modifiers & Qt.MetaModifier:
            mods.append("Super")

        # Selective Shift suppression for symbols
        if (modifiers & Qt.ShiftModifier) and (
            (has_non_shift_modifier and is_ascii_alpha) or (base_key not in HOTKEY_UI_UNSHIFT_MAP)
        ):
            mods.append("Shift")

        mods.append(base_key)
        self._finish("+".join(mods))

    @staticmethod
    def _keysym_name(event):
        return keysym_name(event.nativeVirtualKey())

    def _finish(self, key):
        self.current_key = key
        self.setChecked(False)
        self.textChanged.emit(key)

    def _handle_release_event(self, event):
        """Processes key release to update live feedback."""
        if not self.isChecked():
            return

        key_code = event.key()
        mod_map = {
            Qt.Key_Control: "Control",
            Qt.Key_Shift: "Shift",
            Qt.Key_Alt: "Alt",
            Qt.Key_Meta: "Super",
        }
        if key_code in mod_map:
            # Modifiers pressed and let go with no other key make a modifier-only key, such as Ctrl+Shift.
            if self.modifier_key:
                self._finish(self.modifier_key)
                return
            val = mod_map[key_code]
            if val in self.record_mods:
                self.record_mods.remove(val)
                self._update_display()


class SingleKeyCaptureWidget(HotkeyCaptureWidget):
    """A version of HotkeyCaptureWidget that only captures a single key (no modifiers)."""

    def _update_display(self):
        self._clear_layout()

        if self.isChecked():
            lbl = QLabel(_("[ Press Key... ]"))
            emphasize(lbl)
            self.main_layout.addWidget(lbl)
        elif not self.current_key:
            lbl = QLabel(_("None"))
            mute(lbl)
            self.main_layout.addWidget(lbl)
        else:
            # For single key, we just show it directly or via map
            display_text = HOTKEY_SYM_MAP.get(self.current_key, self.current_key.capitalize())
            cap = KeyCap(display_text)
            self.main_layout.addWidget(cap)

    def _handle_key_event(self, event):
        key_code = event.key()

        # Escape cancels
        if key_code == Qt.Key_Escape:
            self.setChecked(False)
            return

        # Ignore standalone modifier keys
        if key_code in (Qt.Key_Control, Qt.Key_Shift, Qt.Key_Alt, Qt.Key_Meta):
            return

        if key_code == Qt.Key_unknown:
            return

        # Capture base key only
        base_key = self._keysym_name(event)

        if not base_key:
            base_key = (
                event.text()
                if event.text() and event.text().isprintable()
                else QKeySequence(key_code).toString()
            )

        if base_key:
            self.current_key = base_key
            self.setChecked(False)
            self.textChanged.emit(self.current_key)
