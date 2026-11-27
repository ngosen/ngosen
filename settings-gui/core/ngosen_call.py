# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Why a call to the Ngó Sen that fcitx5 runs, at /ngosen, did not get an answer."""

NOT_RUNNING = (
    "org.freedesktop.DBus.Error.ServiceUnknown",
    "org.freedesktop.DBus.Error.NameHasNoOwner",
)
# The addon loads only once Ngó Sen is used.
NOT_LOADED = (
    "org.freedesktop.DBus.Error.UnknownObject",
    "org.freedesktop.DBus.Error.UnknownInterface",
)
# A Ngó Sen from before the method: the object is there, the method is not.
TOO_OLD = "org.freedesktop.DBus.Error.UnknownMethod"
# What dbus-python raises when the answer does not come in time.
NO_REPLY = "org.freedesktop.DBus.Error.NoReply"


class CallError(Exception):
    """name is the D-Bus error name, or None when there is no session bus."""

    def __init__(self, name, message=""):
        super().__init__(message or name or "")
        self.name = name
        self.message = message
