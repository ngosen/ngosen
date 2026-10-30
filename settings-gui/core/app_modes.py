# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""What fcitx5 remembers per app: the mode picked from the mode menu and where the toggle key left it."""

LISTS = (("app_rules", "Rules"), ("app_switches", "Switches"))


class RememberedAppModes:
    """Has no page of its own; it lets Defaults forget the remembered modes like any other setting."""

    def __init__(self, dbus_handler):
        self.dbus = dbus_handler
        self.load_data()

    def load_data(self):
        self.lists = {path: self.dbus.get_sub_config_list(path, key) or [] for path, key in LISTS}
        self._saved = {path: list(items) for path, items in self.lists.items()}

    def is_modified(self):
        return self.lists != self._saved

    def is_modified_from_default(self):
        return any(self.lists.values())

    def restore_defaults(self):
        self.lists = {path: [] for path, _ in LISTS}

    def save_data(self):
        ok = True
        for path, key in LISTS:
            if self.dbus.set_sub_config_list(path, key, self.lists[path]):
                self._saved[path] = list(self.lists[path])
            else:
                ok = False
        return ok
