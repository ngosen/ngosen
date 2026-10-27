# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The values a settings page edits, kept apart from the widgets that show them."""

from i18n import _

from core.catalog import MODE_SHORTCUT_TO_VISIBILITY

# Which settings window is installed is a fact about the machine, not a preference to reset.
NOT_RESET = {"SettingsToolkit"}


class SettingsModel:
    def __init__(self, dbus):
        self.dbus = dbus
        self.saved = {}
        self.current = {}
        self.modified = {}
        self.metadata = {}

    def load(self, values=None):
        """Reads the saved settings; `values`, if given, replaces them as unsaved. False without fcitx5."""
        config_data = self.dbus.get_config()
        if not config_data:
            self.modified = {}
            return False
        saved = config_data.get("values", {})
        self.current = dict(saved if values is None else values)
        self.modified = {k: v for k, v in self.current.items() if saved.get(k) != v}
        self.saved = saved.copy()
        self.metadata = {
            item[0]: item for group in config_data.get("metadata", []) for item in group[1]
        }
        return True

    def defaults(self):
        """What the engine uses for each setting the user never changed, or None before a load."""
        if not self.metadata:
            return None
        return {key: item[3] for key, item in self.metadata.items() if key not in NOT_RESET}

    def default(self, key, fallback=""):
        item = self.metadata.get(key)
        return item[3] if item else fallback

    def label(self, key):
        item = self.metadata.get(key)
        return _(item[2]) if item else None

    def set(self, key, value):
        self.modified[key] = value
        self.current[key] = value

    def is_modified(self):
        return self.current != self.saved

    def is_modified_from_default(self):
        for key, val in self.current.items():
            meta = self.metadata.get(key)
            if not meta or key in NOT_RESET:
                continue
            default_val = meta[3]
            # A hotkey list arrives as {"0": key}.
            if isinstance(default_val, dict) and isinstance(val, dict):
                if str(val.get("0")) != str(default_val.get("0")):
                    return True
            elif str(val) != str(default_val):
                return True
        return False

    def shortcut_conflicts(self):
        """Returns (message, keys) for each shortcut used by more than one shown mode."""
        by_shortcut = {}
        for shortcut_key, visibility_key in MODE_SHORTCUT_TO_VISIBILITY.items():
            shown = self.current.get(visibility_key, self.default(visibility_key, "True"))
            if str(shown).lower() != "true":
                continue
            shortcut = self.current.get(shortcut_key, self.default(shortcut_key))
            if shortcut:
                by_shortcut.setdefault(shortcut, []).append(shortcut_key)
        return [
            (_("Duplicate shortcut '{}' used for multiple enabled modes.").format(shortcut), keys)
            for shortcut, keys in by_shortcut.items()
            if len(keys) > 1
        ]

    def save(self):
        """Writes the changed settings over the latest saved ones, so other changes stay."""
        if not self.modified:
            return True
        config_data = self.dbus.get_config()
        if not config_data:
            return bool(self.dbus.iface)
        latest = config_data.get("values", {})
        latest.update(self.modified)
        if not self.dbus.set_config(latest):
            return False
        self.modified.clear()
        self.saved = self.current.copy()
        return True

    def hotkeys(self, key, fallback=None):
        """A hotkey setting's keys in order; the window shows only the first."""
        val = self.current.get(key, self.default(key) if fallback is None else fallback)
        return [val[i] for i in sorted(val, key=int)] if isinstance(val, dict) else []

    def set_first_hotkey(self, key, text):
        # The keys after the first still work and must survive an edit.
        keys = ([text] if text else []) + self.hotkeys(key, fallback={})[1:]
        self.set(key, {str(i): k for i, k in enumerate(keys)})

    def toggle_is_ctrl_shift(self, key="ToggleKey"):
        # The default is Ctrl+Shift: either Ctrl with either Shift, in either order, as in UniKey.
        ctrl_shift = self.default(key, {})
        return isinstance(ctrl_shift, dict) and sorted(self.hotkeys(key)) == sorted(
            ctrl_shift.values()
        )

    def shown_toggle_key(self, key="ToggleKey"):
        # Without a side, since either Ctrl and either Shift work.
        if self.toggle_is_ctrl_shift(key):
            return "Control+Shift"
        return next(iter(self.hotkeys(key)), "")

    def use_ctrl_shift(self, key="ToggleKey"):
        self.set(key, dict(self.default(key, {})))

    def set_toggle_key(self, text, key="ToggleKey"):
        # No key would leave no way back to Vietnamese; Ctrl+Shift recorded with either side or in
        # either order is still all of UniKey's key.
        ctrl_shift = self.default(key, {})
        if not text or (isinstance(ctrl_shift, dict) and text in ctrl_shift.values()):
            self.use_ctrl_shift(key)
        elif self.toggle_is_ctrl_shift(key):
            # The Ctrl+Shift entries are one key to the user; other extra keys still work.
            self.set(key, {"0": text})
        else:
            self.set_first_hotkey(key, text)

    def tray_icons(self, desktop_dark):
        """The tray icons for Vietnamese and English, and whether they suit a dark panel."""
        if str(self.current.get("UseLotusIcons", "True")).lower() == "true":
            return ("fcitx-ngosen", "fcitx-ngosen-off"), desktop_dark
        color = str(self.current.get("IconTheme", "Auto"))
        dark = {"Light": False, "Dark": True}.get(color, desktop_dark)
        suffix = "-default" if dark else "-default-black"
        return (f"fcitx-ngosen{suffix}", f"fcitx-ngosen-off{suffix}"), dark
