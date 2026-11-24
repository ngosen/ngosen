#!/usr/bin/env python3
"""Tests for the settings part of ngosen-check."""

import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core import settings_report  # noqa: E402

METADATA = [
    [
        "ngosenConfig",
        [
            ["Mode", "Enum", "Mode", "Sen", {"Enum": {"0": "OFF", "1": "Sen", "2": "Preedit"}}],
            ["AutoNonVnRestore", "Boolean", "Auto Restore Invalid Words", "True", {}],
            ["ToggleKey", "List|Key", "Switch Between English and Vietnamese", {}, {}],
            ["HiddenWaitMs", "Integer", "Hidden wait", "30", {}],
        ],
    ]
]

VALUES = {
    "Mode": "Preedit",
    "AutoNonVnRestore": "False",
    "ToggleKey": {"0": "Control+Shift_L", "1": "Control+Shift_R"},
    "HiddenWaitMs": "77",
}

LISTS = {
    "app_rules": [
        {"App": "libreoffice-calc", "Mode": "5"},
        {"App": "org.kde.plasmashell", "Mode": "0"},
        {"App": "firefox", "Mode": "2"},
        {"App": "ctx_1234", "Mode": "5"},
    ],
    "app_switches": [
        {"App": "kitty", "Vietnamese": "False"},
        {"App": "zalo", "Vietnamese": "True"},
    ],
    "emoji_apps": [{"App": "zalo"}],
    "teencode_apps": [],
    "ngosen-macro": [{"Key": "ko", "Value": "mật khẩu ngân hàng"}],
    "emoji_words": [],
}


class FakeHandler:
    def __init__(self, config=None):
        self.config = {"values": VALUES, "metadata": METADATA} if config is None else config

    def get_config(self):
        return self.config

    def get_sub_config_list(self, path, root_key):
        return LISTS.get(path, [])


class SettingsReportTest(unittest.TestCase):
    def setUp(self):
        self.config_dir = tempfile.mkdtemp()
        conf = os.path.join(self.config_dir, "fcitx5", "conf")
        os.makedirs(conf)
        with open(os.path.join(conf, "ngosen-vim-apps.conf"), "w", encoding="utf-8") as f:
            f.write("# Apps where Ngó Sen follows Vim's normal-mode keys, one per line\nkitty\n")
        patch = mock.patch.object(settings_report, "local_dict_path", return_value="/nonexistent")
        patch.start()
        self.addCleanup(patch.stop)

    def report(self, handler=None):
        return settings_report.settings_report(handler or FakeHandler(), self.config_dir)

    def test_prints_window_settings_with_label_key_and_value(self):
        text = self.report()
        self.assertIn("(Mode): Preedit", text)
        self.assertIn("(AutoNonVnRestore): off", text)
        self.assertIn("(ToggleKey): Control+Shift_L, Control+Shift_R", text)

    def test_leaves_out_options_the_window_does_not_show(self):
        text = self.report()
        self.assertNotIn("HiddenWaitMs", text)
        self.assertNotIn("77", text)

    def test_lists_each_app_and_skips_unnamed_windows(self):
        text = self.report()
        self.assertIn(
            "Mode: firefox: Sen, libreoffice-calc: Preedit, org.kde.plasmashell: OFF", text
        )
        self.assertNotIn("ctx_", text)
        self.assertIn("English by the switch key: kitty", text)
        self.assertIn("Emoji suggestions on: zalo", text)
        self.assertIn("Teencode on: (none)", text)
        self.assertIn("Vim mode on: kitty", text)

    def test_counts_macros_without_their_text(self):
        text = self.report()
        self.assertIn("Macros: 1", text)
        self.assertNotIn("mật khẩu", text)

    def test_says_so_when_fcitx5_does_not_answer(self):
        self.assertIn("cannot be shown", self.report(FakeHandler(config={})))


if __name__ == "__main__":
    unittest.main()
