# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The chat abbreviations Ngó Sen expands, and the ones the user keeps as typed."""

from collections import namedtuple

from i18n import _

from core.hotkeys import pretty_format_hotkey_parts
from core.ngosen_call import NO_REPLY, NOT_LOADED, NOT_RUNNING, TOO_OLD, CallError

Entry = namedtuple("Entry", "key text kept")


def parse(listing):
    """The entries of a listing as the engine gives it, those kept as typed first."""
    found = []
    for line in str(listing).splitlines():
        fields = line.split("\t")
        if len(fields) >= 2 and fields[0]:
            found.append(Entry(fields[0], fields[1], len(fields) > 2 and fields[2] == "kept"))
    found.sort(key=lambda entry: (not entry.kept, entry.key))
    return found


def kept_line(entries):
    """What goes beside the list: the abbreviations kept as typed, or that there are none."""
    kept = [entry.key for entry in entries if entry.kept]
    if not kept:
        return _("None is kept as typed.")
    return _("Kept as typed: {}.").format(", ".join(kept))


def app_hint(value_of):
    """How to turn the abbreviations off in one app, with the keys the user set."""
    # Off everywhere, the menu has no such item; an fcitx5 without the key has no row for it.
    if (
        str(value_of("ChatShorthand")).lower() == "false"
        or value_of("ShortcutChatShorthand") is None
    ):
        return ""
    menu_keys = value_of("ModeMenuKey") or []
    keys = list(menu_keys.values()) if isinstance(menu_keys, dict) else list(menu_keys)
    menu = "+".join(pretty_format_hotkey_parts(keys[0])) if keys else ""
    key = value_of("ShortcutChatShorthand")
    if not menu or not key or str(value_of("ShowModeChatShorthand")).lower() == "false":
        return _(
            "Set a mode menu key and a chat abbreviations key on the Keys page to turn them off"
            " in one app."
        )
    return _(
        "In the app you are typing in, press {menu} then {key} to turn them off or on for that"
        " app alone."
    ).format(menu=menu, key=key)


class ChatShorthand:
    """Asks the Ngó Sen that fcitx5 runs, which holds the list and what the user took back."""

    def __init__(self, dbus):
        self.dbus = dbus

    def entries(self):
        """(the entries, None), or (None, a line saying why they cannot be shown)."""
        try:
            return parse(self.dbus.call_ngosen("ChatShorthandList")), None
        except CallError as e:
            return None, _why(e, _("Could not read the abbreviations: {}"))

    def expand_again(self, key):
        """(whether it expands again, a line saying so or why not)."""
        try:
            done, error = self.dbus.call_ngosen("ExpandShorthandAgain", key)
        except CallError as e:
            return False, _why(e, _("Could not expand it again: {}"))
        if not done:
            return False, _("Could not expand it again: {}").format(error)
        return True, _("{} expands again.").format(key)


def _why(error, other):
    if error.name in NOT_RUNNING or error.name in NOT_LOADED:
        return _("Ngó Sen is not running. Type something with it, then try again.")
    if error.name == TOO_OLD:
        return _("The running Ngó Sen is older than this window. Restart fcitx5, then try again.")
    if error.name == NO_REPLY:
        return _("Ngó Sen did not answer. Try again.")
    if error.name is None:
        return _("No desktop session bus, so Ngó Sen cannot be reached.")
    return other.format(error.message or error.name)
