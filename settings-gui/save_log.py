#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Saves the typing log from a terminal, for when the tray menu is missing or frozen."""

import sys

import dbus
from i18n import _, setup_i18n

SERVICE = "org.fcitx.Fcitx5"
PATH = "/ngosen"
INTERFACE = "io.github.ngosen.NgoSen1"

NOT_RUNNING = (
    "org.freedesktop.DBus.Error.ServiceUnknown",
    "org.freedesktop.DBus.Error.NameHasNoOwner",
)
# fcitx5 is up but has not loaded Ngó Sen, or runs a version without this object.
NOT_LOADED = (
    "org.freedesktop.DBus.Error.UnknownObject",
    "org.freedesktop.DBus.Error.UnknownMethod",
    "org.freedesktop.DBus.Error.UnknownInterface",
)


def save(bus):
    """Returns (exit code, message)."""
    try:
        # A call would otherwise start a new fcitx5, whose log is empty.
        if not bus.name_has_owner(SERVICE):
            return 1, _("fcitx5 is not running.")
        proxy = bus.get_object(SERVICE, PATH, introspect=False)
        path = dbus.Interface(proxy, INTERFACE).SaveTypingLog()
    except dbus.DBusException as e:
        name = e.get_dbus_name()
        if name in NOT_RUNNING:
            return 1, _("fcitx5 is not running.")
        if name in NOT_LOADED:
            return 1, _("Ngó Sen is not loaded. Switch to Ngó Sen once, then run this again.")
        if name == "org.freedesktop.DBus.Error.NoReply":
            return 1, _("fcitx5 did not answer.")
        return 1, e.get_dbus_message()
    return 0, _("Typing log saved: ") + str(path)


def main():
    setup_i18n()
    try:
        bus = dbus.SessionBus()
    except dbus.DBusException:
        code, message = 1, _("fcitx5 is not running.")
    else:
        code, message = save(bus)
    print(message, file=sys.stdout if code == 0 else sys.stderr)
    return code


if __name__ == "__main__":
    sys.exit(main())
