# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The apps where the user turned teencode on from the mode menu."""

from i18n import _

from core.hotkeys import pretty_format_hotkey_parts

APPS = ("teencode_apps", "Apps")


def toggle_hint(menu_keys, teencode_key):
    """How to turn teencode on for one app, with the keys the user set (ModeMenuKey, ShortcutTeencode)."""
    keys = list(menu_keys.values()) if isinstance(menu_keys, dict) else list(menu_keys or [])
    menu = "+".join(pretty_format_hotkey_parts(keys[0])) if keys else ""
    if not menu or not teencode_key:
        return _("Set a mode menu key and a teencode key on the Keys page to turn teencode on.")
    return _(
        "Teencode keeps words like zậy, đag and nhữg instead of restoring the keys typed. In the"
        " app you are typing in, press {menu} then {key} to turn it on or off for that app."
    ).format(menu=menu, key=teencode_key)


class TeencodeApps:
    def __init__(self, dbus):
        self.dbus = dbus
        self.apps = self.read()

    def read(self):
        """fcitx5's list, which the keyboard may have changed since the page loaded."""
        apps = [item.get("App", "") for item in self.dbus.get_sub_config_list(*APPS) or []]
        return [app for app in apps if app]

    def turn_off(self, app):
        """Takes app off the list; returns False if fcitx5 did not save it."""
        apps = [a for a in self.read() if a != app]
        if not self.dbus.set_sub_config_list(*APPS, [{"App": a} for a in apps]):
            return False
        self.apps = apps
        return True
