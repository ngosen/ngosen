# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Reusable UI components.
"""

from core.hotkeys import (
    HOTKEY_SYM_MAP,
    MODIFIER_ORDER,
    HotkeyRecorder,
    keysym_name,
    pretty_format_hotkey_parts,
)
from i18n import _
from qtpy.QtCore import QEvent, Qt, Signal
from qtpy.QtGui import QIcon, QKeySequence
from qtpy.QtWidgets import QHBoxLayout, QLabel, QPushButton, QWidget

from ui.typography import emphasize, mute

MODIFIER_KEYS = {
    Qt.Key_Control: "Control",
    Qt.Key_Shift: "Shift",
    Qt.Key_Alt: "Alt",
    Qt.Key_Meta: "Super",
}
MODIFIER_FLAGS = [
    (Qt.ControlModifier, "Control"),
    (Qt.AltModifier, "Alt"),
    (Qt.MetaModifier, "Super"),
    (Qt.ShiftModifier, "Shift"),
]


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

        self.recorder = HotkeyRecorder()

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
            self.recorder = HotkeyRecorder()
        self._update_display()

    def _clear_layout(self):
        while self.main_layout.count():
            item = self.main_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

    def _update_display(self):
        self._clear_layout()

        if self.isChecked():
            if not self.recorder.held:
                lbl = QLabel(_("[ Recording... ]"))
                emphasize(lbl)
                self.main_layout.addWidget(lbl)
            else:
                # Show current active modifiers
                for mod in MODIFIER_ORDER:
                    if mod in self.recorder.held:
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

        if key_code in MODIFIER_KEYS:
            if self.recorder.press_modifier(MODIFIER_KEYS[key_code], self._keysym_name(event)):
                self._update_display()
            return

        if key_code == Qt.Key_unknown:
            self.recorder.press_key("", set())
            return

        base_key = self._keysym_name(event)
        if not base_key:
            base_key = (
                event.text()
                if event.text() and event.text().isprintable()
                else QKeySequence(key_code).toString()
            )
        held = {name for flag, name in MODIFIER_FLAGS if modifiers & flag}
        key = self.recorder.press_key(base_key, held)
        if key:
            self._finish(key)

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
        if key_code in MODIFIER_KEYS:
            # Modifiers pressed and let go with no other key make a modifier-only key, such as Ctrl+Shift.
            key = self.recorder.release_modifier(MODIFIER_KEYS[key_code])
            if key:
                self._finish(key)
            else:
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
