# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Backup files holding every Ngó Sen setting, as JSON."""

import json
import os
from datetime import datetime

from core.settings_snapshot import local_dict_path

COMPONENTS = ("config", "macros", "keymaps", "rules", "dictionary")
SUB_CONFIGS = {
    "macros": ("ngosen-macro", "Macro"),
    "keymaps": ("custom_keymap", "CustomKeymap"),
    "rules": ("app_rules", "Rules"),
}


def export(dbus, path):
    """Writes all settings to path; raises OSError or ValueError on failure."""
    config = dbus.get_config()
    if not config:
        raise ValueError("fcitx5 did not return the settings")
    backup = {"config": config.get("values", {})}
    for name, (sub_path, root_key) in SUB_CONFIGS.items():
        backup[name] = dbus.get_sub_config_list(sub_path, root_key)
    if os.path.exists(local_dict_path()):
        with open(local_dict_path(), encoding="utf-8") as f:
            backup["dictionary"] = f.read()
    backup["meta"] = {
        "version": 1,
        "timestamp": datetime.now().isoformat(),
        "components": [k for k in COMPONENTS if k in backup],
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(backup, f, indent=2, ensure_ascii=False)


def load(path):
    """Reads a backup file; raises OSError or ValueError when it is not one."""
    with open(path, encoding="utf-8") as f:
        backup = json.load(f)
    if not isinstance(backup, dict) or not any(k in backup for k in COMPONENTS):
        raise ValueError("no settings found in the file")
    return backup


def apply(dbus, backup):
    """Writes what the backup holds and leaves the rest alone. Returns False if a part failed."""
    ok = True
    if "config" in backup:
        ok = dbus.set_config(backup["config"]) and ok
    for name, (sub_path, root_key) in SUB_CONFIGS.items():
        if name in backup:
            ok = dbus.set_sub_config_list(sub_path, root_key, backup[name]) and ok
    if "dictionary" in backup:
        try:
            os.makedirs(os.path.dirname(local_dict_path()), exist_ok=True)
            with open(local_dict_path(), "w", encoding="utf-8") as f:
                f.write(backup["dictionary"])
        except OSError:
            ok = False
    return dbus.reload_addon_config() and ok
