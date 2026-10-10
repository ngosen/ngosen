# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs the settings window against a fake fcitx5, with no display and no D-Bus."""

import copy
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from qtpy.QtWidgets import QApplication  # noqa: E402

app = QApplication.instance() or QApplication([])

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
