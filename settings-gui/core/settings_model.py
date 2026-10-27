# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The values a settings page edits, kept apart from the widgets that show them."""

from i18n import _

from core.catalog import MODE_SHORTCUT_TO_VISIBILITY


class SettingsModel:
    def __init__(self, dbus):
        self.dbus = dbus
        self.saved = {}
        self.current = {}
        self.modified = {}
        self.metadata = {}

    def load(self, values=None):
        """Reads the saved settings, with `values` as unsaved changes on top. False without fcitx5."""
        self.modified.clear()
        config_data = self.dbus.get_config()
        if not config_data:
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
        """What the engine uses for each setting the user never changed, or None without fcitx5."""
        config_data = self.dbus.get_config()
        if not config_data:
            return None
        return {item[0]: item[3] for group in config_data.get("metadata", []) for item in group[1]}

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
            if not meta:
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
