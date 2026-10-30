# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""A fake fcitx5 for tests that need no toolkit."""

import copy
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

with open(os.path.join(HERE, "config-metadata.json"), encoding="utf-8") as f:
    METADATA = json.load(f)

DEFAULTS = {item[0]: item[3] for group in METADATA for item in group[1]}


class FakeDBusHandler:
    """Behaves like fcitx5's Controller1 for the ngosen addon: partial SetConfig, sub configs."""

    addon_name = "fcitx://config/addon/ngosen"

    def __init__(self, values=None, sub_configs=None):
        self.iface = object()
        self.values = copy.deepcopy(DEFAULTS)
        self.values.update(values or {})
        self.sub_configs = copy.deepcopy(sub_configs or {})
        self.reloads = []
        self.group = ("Default", "us", [("keyboard-us", ""), ("ngosen", "")])
        self.restarts = 0
        self.available = {"keyboard-us", "ngosen"}

    def get_config(self):
        return {"values": copy.deepcopy(self.values), "metadata": copy.deepcopy(METADATA)}

    def set_config(self, values):
        self.values.update(copy.deepcopy(values))
        return True

    def get_sub_config_list(self, path, root_key):
        return copy.deepcopy(self.sub_configs.get(path, []))

    def set_sub_config_list(self, path, root_key, data_list):
        self.sub_configs[path] = copy.deepcopy(data_list)
        return True

    def reload_addon_config(self):
        self.reloads.append("ngosen")
        return True

    def input_method_names(self):
        return [im for im, _im_layout in self.group[2]]

    def has_input_method(self, name):
        return name in self.available

    def input_method_group(self):
        return copy.deepcopy(self.group)

    def set_input_method_group(self, name, layout, items):
        self.group = (name, layout, copy.deepcopy(items))
        return True

    def restart(self):
        self.restarts += 1
        return True

    def debug_info(self):
        return "Group [wayland:] has 1 InputContext(s)\n  IC [1] program:kate frontend:wayland cap:72 focus:1\n"
