# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The settings as ngosen-check --settings prints them: only what the settings window shows."""

import os

from i18n import _

from core import dictionary, macros
from core.catalog import MODE_SHORTCUT_TO_VISIBILITY, SETTINGS_MAP, mode_order, shown_mode
from core.settings_snapshot import local_dict_path

# Settings that live on the editor pages rather than in the catalog.
EDITOR_KEYS = {
    "Macros": [
        "EnableMacro",
        "CapitalizeMacro",
        "EnableMacroInOffMode",
        "TimeFormat",
        "DateFormat",
    ],
    "Mode menu": ["ModeOrder", *MODE_SHORTCUT_TO_VISIBILITY.values()],
    "Custom Dictionary": ["EnableDictionary"],
    "Keymap": ["EnableCustomKeymap"],
}

# The engine reads every other number as Sen.
APP_MODES = {"0": "OFF", "5": "Preedit"}


def _entries(metadata):
    """Maps each key to (label, type, enum labels), from fcitx5's description of the config."""
    entries = {}
    for _name, options in metadata or []:
        for option in options:
            key, kind, label = option[0], option[1], option[2]
            extra = option[4] if len(option) > 4 and isinstance(option[4], dict) else {}
            enum = extra.get("Enum", {})
            shown = extra.get("EnumI18n", enum)
            entries[key] = (_(label), kind, {enum[i]: _(shown.get(i, enum[i])) for i in enum})
    return entries


def _show(value, kind, enum_labels):
    if kind == "Boolean":
        return "on" if str(value).lower() == "true" else "off"
    if isinstance(value, dict):
        value = [value[k] for k in sorted(value, key=lambda k: int(k) if k.isdigit() else 0)]
    if isinstance(value, list):
        return ", ".join(str(v) for v in value) or "(none)"
    if value in ("", None):
        return "(none)"
    return enum_labels.get(value, str(value))


def _groups():
    for groups in SETTINGS_MAP.values():
        yield from groups.items()
    yield from EDITOR_KEYS.items()


def _as_window_shows(key, value):
    if key == "Mode":
        return shown_mode(value)
    if key == "ModeOrder":
        return ",".join(mode_order(value))
    if key in macros.FORMATS and value not in macros.FORMATS[key]:
        return macros.DEFAULTS[key]
    return value


def global_lines(config):
    values = config.get("values", {})
    entries = _entries(config.get("metadata"))
    lines = []
    for group, keys in _groups():
        shown = [k for k in keys if k in values and k in entries]
        if not shown:
            continue
        lines.append(f"[{_(group)}]")
        for key in shown:
            label, kind, enum_labels = entries[key]
            value = _as_window_shows(key, values[key])
            lines.append(f"{label} ({key}): {_show(value, kind, enum_labels)}")
    return lines


def _named(apps):
    # ctx_ entries stand for one window of an unnamed app, not for an app.
    return sorted(a for a in apps if a and not a.startswith("ctx_"))


def _app_list(handler, path):
    return _named(a.get("App", "") for a in handler.get_sub_config_list(path, "Apps"))


def read_vim_apps(config_dir):
    path = os.path.join(config_dir, "fcitx5", "conf", "ngosen-vim-apps.conf")
    try:
        # The engine reads the file as bytes, so one bad line does not hide the others.
        with open(path, encoding="utf-8", errors="replace") as f:
            return [line.rstrip("\n") for line in f if line.strip() and not line.startswith("#")]
    except OSError:
        return []


def app_lines(handler, config_dir):
    rules = handler.get_sub_config_list("app_rules", "Rules")
    modes = {r.get("App", ""): APP_MODES.get(str(r.get("Mode")), "Sen") for r in rules}
    switches = handler.get_sub_config_list("app_switches", "Switches")
    languages = {}
    for s in switches:
        # The engine takes the first entry for an app.
        on = str(s.get("Vietnamese")).lower() == "true"
        languages.setdefault(s.get("App", ""), "Vietnamese" if on else "English")
    lists = [
        ("Mode", [f"{a}: {modes[a]}" for a in _named(modes)]),
        ("Language by the switch key", [f"{a}: {languages[a]}" for a in _named(languages)]),
        ("Emoji suggestions on", _app_list(handler, "emoji_apps")),
        ("Teencode on", _app_list(handler, "teencode_apps")),
        ("Vim mode on", _named(read_vim_apps(config_dir))),
    ]
    return ["[Per app]"] + [f"{name}: {', '.join(apps) or '(none)'}" for name, apps in lists]


def count_lines(handler):
    """Counts only: macros and words can hold things the user keeps private."""
    # Counted the way the window loads them, so dropped and merged entries are left out.
    macro_entries = macros.merge_keys(
        m for m in handler.get_sub_config_list("ngosen-macro", "Macro") if m.get("Key")
    )
    words = {
        (w.get("Emoji", ""), w.get("Keyword", "").strip())
        for w in handler.get_sub_config_list("emoji_words", "Words")
    }
    words = [w for w in words if all(w)]
    own = os.path.exists(local_dict_path())
    path = local_dict_path() if own else dictionary.bundled_path()
    try:
        count = len(dictionary.read_words(path))
        dict_line = f"Custom dictionary: {'own' if own else 'bundled'} list, {count} words"
    except (OSError, UnicodeDecodeError):
        dict_line = "Custom dictionary: cannot be read"
    return [
        "[Counts]",
        f"Macros: {len(macro_entries)}",
        dict_line,
        f"Emoji keyword changes: {len(words)}",
    ]


def settings_report(handler, config_dir=None):
    config = handler.get_config()
    if not config:
        return "Settings: fcitx5 did not answer, so they cannot be shown."
    config_dir = config_dir or os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    lines = ["# Settings, as in the settings window"]
    lines += (
        global_lines(config) + [""] + app_lines(handler, config_dir) + [""] + count_lines(handler)
    )
    return "\n".join(lines)
