# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The typing mode fcitx5 remembers per app when one is picked from the mode menu."""

from qtpy.QtWidgets import QWidget


class RememberedAppModes(QWidget):
    """Never shown; it lets Defaults forget the remembered modes like any other setting."""

    def __init__(self, dbus_handler):
        super().__init__()
        self.dbus = dbus_handler
        self.load_data()

    def load_data(self):
        self.rules = self.dbus.get_sub_config_list("app_rules", "Rules") or []
        self._saved = list(self.rules)

    def is_modified(self):
        return self.rules != self._saved

    def is_modified_from_default(self):
        return bool(self.rules)

    def restore_defaults(self):
        self.rules = []

    def save_data(self):
        if not self.dbus.set_sub_config_list("app_rules", "Rules", self.rules):
            return False
        self._saved = list(self.rules)
        return True
