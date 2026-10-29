# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""A button that records a hotkey as fcitx5 names it, with key caps for what is held."""

from core.hotkeys import (
    HOTKEY_SYM_MAP,
    MODIFIER_ORDER,
    HotkeyRecorder,
    pretty_format_hotkey_parts,
)
from gi.repository import Gdk, GObject, Gtk
from i18n import _

MODIFIERS = {
    "Control_L": "Control",
    "Control_R": "Control",
    "Shift_L": "Shift",
    "Shift_R": "Shift",
    "Alt_L": "Alt",
    "Alt_R": "Alt",
    # Shift+Alt reads as Meta on most layouts.
    "Meta_L": "Alt",
    "Meta_R": "Alt",
    "Super_L": "Super",
    "Super_R": "Super",
}
MASKS = (
    ("Control", Gdk.ModifierType.CONTROL_MASK),
    ("Alt", Gdk.ModifierType.ALT_MASK),
    ("Super", Gdk.ModifierType.SUPER_MASK),
    ("Shift", Gdk.ModifierType.SHIFT_MASK),
)


class HotkeyButton(Gtk.ToggleButton):
    """Click, then press the keys; Escape cancels. "recorded" carries the new key."""

    __gsignals__ = {"recorded": (GObject.SignalFlags.RUN_FIRST, None, (str,))}

    def __init__(self, key="", single=False):
        super().__init__(valign=Gtk.Align.CENTER)
        self.key = key
        self.single = single
        self.recorder = HotkeyRecorder()
        self.caps = Gtk.Box(spacing=4, halign=Gtk.Align.CENTER)
        self.set_child(self.caps)
        keys = Gtk.EventControllerKey(propagation_phase=Gtk.PropagationPhase.CAPTURE)
        keys.connect("key-pressed", self._on_press)
        keys.connect("key-released", self._on_release)
        self.add_controller(keys)
        self.connect("toggled", self._on_toggled)
        self._show()

    def set_key(self, key):
        self.key = key
        self._show()

    def _on_toggled(self, _button):
        self.recorder = HotkeyRecorder()
        self._show()

    def _show(self):
        while child := self.caps.get_first_child():
            self.caps.remove(child)
        if self.get_active():
            held = [m for m in MODIFIER_ORDER if m in self.recorder.held]
            waiting = _("[ Press Key... ]") if self.single else _("[ Recording... ]")
            parts = [waiting.strip("[] ")] if not held else held + ["…"]
        elif self.key and self.single:
            # A lone key is shown by what it types, as the Qt window does: plus is "+".
            parts = [HOTKEY_SYM_MAP.get(self.key, self.key.capitalize())]
        elif self.key:
            parts = pretty_format_hotkey_parts(self.key)
        else:
            parts = [_("None")]
        for part in parts:
            label = Gtk.Label(label=part)
            if self.key and not self.get_active() or part in MODIFIER_ORDER:
                label.add_css_class("keycap")
            self.caps.append(label)

    def _finish(self, key):
        self.key = key
        self.set_active(False)
        self.emit("recorded", key)

    def _on_press(self, _controller, keyval, _code, state):
        if not self.get_active():
            return False
        name = Gdk.keyval_name(keyval) or ""
        if name == "Escape":
            self.set_active(False)
        elif name in MODIFIERS:
            if not self.single and self.recorder.press_modifier(MODIFIERS[name], name):
                self._show()
        elif self.single:
            if name:
                self._finish(name)
        else:
            held = {m for m, mask in MASKS if state & mask}
            key = self.recorder.press_key(name, held)
            if key:
                self._finish(key)
        # Tab, Escape and Space would otherwise move focus or press the button.
        return True

    def _on_release(self, _controller, keyval, _code, _state):
        name = Gdk.keyval_name(keyval) or ""
        if not self.get_active() or self.single or name not in MODIFIERS:
            return
        key = self.recorder.release_modifier(MODIFIERS[name])
        if key:
            self._finish(key)
        else:
            self._show()
