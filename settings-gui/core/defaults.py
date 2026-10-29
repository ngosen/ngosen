# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Puts every setting, table and remembered app mode back to what a new install has."""

from i18n import _

from core.app_modes import RememberedAppModes
from core.dictionary import Dictionary
from core.keymap import Keymap
from core.macros import MacroTable
from core.settings_model import SettingsModel


def restore(dbus):
    """Saves the defaults. False when a part of them was not saved, so the window can say so."""
    model = SettingsModel(dbus)
    if not model.load():
        return False
    for key, value in model.defaults().items():
        if model.current.get(key) != value:
            model.set(key, value)
    ok = model.save()
    for table in (MacroTable(dbus), Keymap(dbus)):
        table.load()
        table.reset()
        ok = table.save() and ok
    app_modes = RememberedAppModes(dbus)
    app_modes.restore_defaults()
    ok = app_modes.save_data() and ok
    return _restore_dictionary(dbus) and ok


def toggle_key_note(model):
    """What a reset changes about switching languages, or "" when the key is already the default."""
    if not model.metadata or model.toggle_is_ctrl_shift():
        return ""
    return _("The key that switches between Vietnamese and English goes back to Ctrl+Shift.")


def _restore_dictionary(dbus):
    dictionary = Dictionary(dbus)
    dictionary.load()
    # A file read only in part is left alone rather than replaced by the bundled words.
    if dictionary.load_failed:
        return False
    dictionary.reset()
    try:
        return dictionary.save()
    except OSError:
        return False
