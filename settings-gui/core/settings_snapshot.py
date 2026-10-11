# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Everything a change in the settings window can touch, so one save can be undone."""

import os

SUB_CONFIGS = (
    ("app_rules", "Rules"),
    ("ngosen-macro", "Macro"),
    ("custom_keymap", "CustomKeymap"),
)


def local_dict_path():
    xdg_data_home = os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share"))
    return os.path.join(xdg_data_home, "fcitx5/ngosen/vietnamese.cm.dict")


def take(dbus):
    """Returns the saved settings, or None when fcitx5 cannot be read."""
    config = dbus.get_config()
    if not config:
        return None
    snapshot = {"config": config.get("values", {})}
    for path, root_key in SUB_CONFIGS:
        snapshot[path] = dbus.get_sub_config_list(path, root_key)
    try:
        with open(local_dict_path(), encoding="utf-8") as f:
            snapshot["dictionary"] = f.read()
    except FileNotFoundError:
        snapshot["dictionary"] = None
    except OSError:
        return None
    return snapshot


def restore(dbus, snapshot):
    ok = dbus.set_config(snapshot["config"])
    for path, root_key in SUB_CONFIGS:
        ok = dbus.set_sub_config_list(path, root_key, snapshot[path]) and ok
    dict_path = local_dict_path()
    try:
        if snapshot["dictionary"] is None:
            if os.path.exists(dict_path):
                os.remove(dict_path)
        else:
            os.makedirs(os.path.dirname(dict_path), exist_ok=True)
            with open(dict_path, "w", encoding="utf-8") as f:
                f.write(snapshot["dictionary"])
    except OSError:
        ok = False
    # SetConfig does not make the addon read the dictionary again.
    return dbus.reload_addon_config() and ok
