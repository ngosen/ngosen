# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The line a page shows when what it holds is not in use right now, and why."""

from i18n import _

from core.settings_model import SettingsModel


def read(dbus):
    """What fcitx5 holds now, or None when it cannot be read."""
    model = SettingsModel(dbus)
    return model if model.load() else None


def is_on(value):
    return str(value).lower() == "true"


def _off(model, key):
    value = model.value_of(key)
    return value is not None and not is_on(value)


def _name(model, key):
    return model.label(key) or key


def _turn_on(model, keys):
    off = [_name(model, key) for key in keys if _off(model, key)]
    return _("Not in use now: turn on {}.").format(", ".join(off)) if off else ""


def dictionary_line(model):
    # The engine reads the words only while restoring a word that fails the spell check.
    return _turn_on(model, ("EnableDictionary", "AutoNonVnRestore", "SpellCheck"))


def macro_line(model):
    return _turn_on(model, ("EnableMacro",))


def keymap_line(model):
    method = model.value_of("InputMethod")
    other_method = method is not None and method != "Custom"
    switch_off = _off(model, "EnableCustomKeymap")
    if other_method and switch_off:
        return _("Not in use now: turn on {} and choose Custom in {}.").format(
            _name(model, "EnableCustomKeymap"), _name(model, "InputMethod")
        )
    if other_method:
        return _("Not in use now: choose Custom in {}.").format(_name(model, "InputMethod"))
    return _turn_on(model, ("EnableCustomKeymap",))


def custom_without_keys_line(model, dbus):
    if model.value_of("InputMethod") != "Custom":
        return ""
    if is_on(model.value_of("EnableCustomKeymap")) and dbus.get_sub_config_list(
        "custom_keymap", "CustomKeymap"
    ):
        return ""
    return _("Custom has no keys yet: turn on Custom Keymap and add keys in Keys › Keymap.")


def retone_line(model):
    charset = model.value_of("OutputCharset")
    if _off(model, "RetoneSelection") or charset is None or charset == "Unicode":
        return ""
    return _("Not in use now: Cứu dấu works only with Unicode.")
