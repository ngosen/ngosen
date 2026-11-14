# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Converting text on the Convert page, through the Ngó Sen that fcitx5 runs."""

from i18n import _

NOT_RUNNING = (
    "org.freedesktop.DBus.Error.ServiceUnknown",
    "org.freedesktop.DBus.Error.NameHasNoOwner",
)
NOT_LOADED = (
    "org.freedesktop.DBus.Error.UnknownObject",
    "org.freedesktop.DBus.Error.UnknownInterface",
)
# A Ngó Sen from before this page: the object is there, the method is not.
TOO_OLD = "org.freedesktop.DBus.Error.UnknownMethod"

SETTING_KEYS = ("ConvertFrom", "ConvertTo", "ConvertCase", "ConvertStripDiacritics")


class ConvertError(Exception):
    """name is the D-Bus error name, or None when there is no session bus."""

    def __init__(self, name, message=""):
        super().__init__(message or name or "")
        self.name = name
        self.message = message


def case_index(model, value):
    """ConvertText's letter case: the place of value among the ConvertCase choices."""
    item = model.metadata.get("ConvertCase")
    enum = item[4].get("Enum", {}) if item else {}
    for index, name in enum.items():
        if name == value:
            return int(index)
    return 0


def explain(error):
    if error.name is None:
        return _("No desktop session bus, so Ngó Sen cannot be reached.")
    if error.name in NOT_RUNNING:
        return _("fcitx5 is not running.")
    if error.name in NOT_LOADED:
        return _("Ngó Sen is not loaded. Switch to Ngó Sen once, then try again.")
    if error.name == TOO_OLD:
        return _("The running Ngó Sen is older than this window. Restart fcitx5 to convert here.")
    return error.message or error.name


def convert(dbus_handler, model, text):
    """(converted text, None), or (None, a line saying why it could not be done)."""
    if not text:
        return "", None
    try:
        result = dbus_handler.convert_text(
            text,
            str(model.value_of("ConvertFrom")),
            str(model.value_of("ConvertTo")),
            case_index(model, model.value_of("ConvertCase")),
            str(model.value_of("ConvertStripDiacritics")).lower() == "true",
        )
    except ConvertError as e:
        return None, explain(e)
    return result, None
