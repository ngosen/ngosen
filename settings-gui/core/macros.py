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


def is_invalid_key(key):
    # The engine ends a macro key at the first character that is not a letter or digit.
    return bool(key) and not key.isalnum()


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
                self.options[key] = value
        self.entries = [
            {"Key": item.get("Key", ""), "Value": item.get("Value", "")}
            for item in self.dbus.get_sub_config_list("ngosen-macro", "Macro")
        ]
        self.saved_state = self.state()

    def reset(self):
        self.options = dict(DEFAULTS)
        self.entries = []

    def is_modified_from_default(self):
        return bool(self.entries) or self.options != DEFAULTS

    def state(self):
        return {"data": [dict(entry) for entry in self.entries], **self.options}

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
