#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Switches between English and Vietnamese from a desktop shortcut, also with no field focused."""

import sys

import dbus
from i18n import _, setup_i18n
from save_log import INTERFACE, NOT_RUNNING, PATH, SERVICE

NO_OBJECT = "org.freedesktop.DBus.Error.UnknownObject"
# fcitx5 still runs a Ngó Sen from before the package was upgraded.
OLD_ENGINE = (
    "org.freedesktop.DBus.Error.UnknownMethod",
    "org.freedesktop.DBus.Error.UnknownInterface",
)


def toggle(bus):
    """Returns (exit code, message); the message is empty when it worked."""
    try:
        # By its unique name, so a shortcut pressed while fcitx5 is down never starts one.
        owner = bus.get_name_owner(SERVICE)
        proxy = bus.get_object(owner, PATH, introspect=False)
        dbus.Interface(proxy, INTERFACE).ToggleVietnamese(timeout=5)
    except dbus.DBusException as e:
        name = e.get_dbus_name()
        if name in NOT_RUNNING:
            return 1, _("fcitx5 is not running.")
        if name == NO_OBJECT:
            return 1, _("Ngó Sen is not loaded. Switch to Ngó Sen once, then try again.")
        if name in OLD_ENGINE:
            return 1, _("Ngó Sen was updated. Log out and back in, then try again.")
        if name == "org.freedesktop.DBus.Error.NoReply":
            return 1, _("fcitx5 did not answer.")
        return 1, e.get_dbus_message() or str(e)
    return 0, ""


def notify(bus, message):
    # A shortcut has no terminal to print to.
    try:
        proxy = bus.get_object("org.freedesktop.Notifications", "/org/freedesktop/Notifications")
        dbus.Interface(proxy, "org.freedesktop.Notifications").Notify(
            "Ngó Sen",
            0,
            "fcitx-ngosen",
            _("Could not switch English/Vietnamese"),
            message,
            [],
            {},
            6000,
        )
    except dbus.DBusException:
        pass


def main():
    setup_i18n()
    try:
        bus = dbus.SessionBus()
    except dbus.DBusException:
        print(
            _("No desktop session bus. Run this as your own user in a desktop terminal."),
            file=sys.stderr,
        )
        return 1
    code, message = toggle(bus)
    if code != 0:
        print(message, file=sys.stderr)
        notify(bus, message)
    return code


if __name__ == "__main__":
    sys.exit(main())
