# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The macro table: short keys the engine expands, and the switches and formats around it."""

DEFAULTS = {
    "EnableMacro": True,
    "CapitalizeMacro": True,
    "EnableMacroInOffMode": False,
    "TimeFormat": "%H:%M",
    "DateFormat": "%d/%m/%Y",
}
# The formats the settings window offers; an empty one leaves the time or date out.
FORMATS = {
    "TimeFormat": ("%H:%M", "%H:%M:%S", "%I:%M %p", "%I:%M:%S %p", ""),
    "DateFormat": ("%d/%m/%Y", "%d/%m/%y", "%m/%d/%Y", "%Y-%m-%d", "%y-%m-%d", ""),
}


def is_invalid_key(key):
    # The engine ends a macro key at the first character that is not a letter or digit.
    return bool(key) and not key.isalnum()


def merge_keys(entries):
    """Keeps one entry per key, ignoring case as the engine does; the last value wins."""
    merged = {}
    for entry in entries:
        key = entry["Key"].lower()
        if key in merged:
            merged[key]["Value"] = entry["Value"]
        else:
            merged[key] = dict(entry)
    return list(merged.values())


def state(options, entries):
    return {"data": [dict(entry) for entry in entries], **options}


def is_default(options, entries):
    return not entries and options == DEFAULTS


class MacroTable:
    def __init__(self, dbus):
        self.dbus = dbus
        self.options = dict(DEFAULTS)
        self.entries = []
        self.saved_state = self.state()

    def load(self):
        config_data = self.dbus.get_config()
        if config_data:
            values = config_data.get("values", {})
            for key, default in DEFAULTS.items():
                value = values.get(key, default)
                if isinstance(default, bool):
                    value = str(value).lower() == "true"
                elif value not in FORMATS[key]:
                    value = default
                self.options[key] = value
        self.entries = merge_keys(
            {"Key": item.get("Key", ""), "Value": item.get("Value", "")}
            for item in self.dbus.get_sub_config_list("ngosen-macro", "Macro")
            if item.get("Key")
        )
        self.saved_state = self.state()

    def reset(self):
        self.options = dict(DEFAULTS)
        self.entries = []

    def is_modified_from_default(self):
        return not is_default(self.options, self.entries)

    def state(self):
        return state(self.options, self.entries)

    def is_modified(self):
        return self.state() != self.saved_state

    def save(self):
        """Writes the options and the table. False when fcitx5 cannot be reached or refuses."""
        config_data = self.dbus.get_config()
        if config_data:
            values = config_data.get("values", {})
            for key, value in self.options.items():
                if isinstance(value, bool):
                    value = "True" if value else "False"
                values[key] = value
            if not self.dbus.set_config(values):
                return False
        elif not self.dbus.iface:
            return False

        if not self.dbus.set_sub_config_list("ngosen-macro", "Macro", self.entries):
            return False
        self.saved_state = self.state()
        return True
