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
        # Calling the running fcitx5 by its unique name never starts a new one, whose log is empty.
        owner = bus.get_name_owner(SERVICE)
        proxy = bus.get_object(owner, PATH, introspect=False)
        path = dbus.Interface(proxy, INTERFACE).SaveTypingLog(timeout=5)
    except dbus.DBusException as e:
        name = e.get_dbus_name()
        if name in NOT_RUNNING:
            return 1, _("fcitx5 is not running.")
        if name in NOT_LOADED:
            return 1, _("Ngó Sen is not loaded. Switch to Ngó Sen once, then run this again.")
        if name == "org.freedesktop.DBus.Error.NoReply":
            return 1, _("fcitx5 did not answer.")
        return 1, e.get_dbus_message() or str(e)
    return 0, _("Typing log saved: ") + str(path)


def main():
    setup_i18n()
    try:
        bus = dbus.SessionBus()
    except dbus.DBusException:
        # Typical under sudo or ssh, which do not pass on the session's bus address.
        code = 1
        message = _("No desktop session bus. Run this as your own user in a desktop terminal.")
    else:
        code, message = save(bus)
    print(message, file=sys.stdout if code == 0 else sys.stderr)
    return code


if __name__ == "__main__":
    sys.exit(main())
