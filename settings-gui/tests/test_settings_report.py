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
            ["TimeFormat", "String", "Time Format", "%H:%M", {}],
            ["ModeOrder", "String", "Mode Order", "Sen,Preedit", {}],
            ["ShowModeEmoji", "Boolean", "Show Emoji", "True", {}],
        ],
    ]
]

VALUES = {
    "Mode": "Preedit",
    "AutoNonVnRestore": "False",
    "ToggleKey": {"0": "Control+Shift_L", "1": "Control+Shift_R"},
    "HiddenWaitMs": "77",
    "TimeFormat": "%H:%M:%S",
    "ModeOrder": "Preedit,Sen",
    "ShowModeEmoji": "False",
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
        {"App": "kitty", "Vietnamese": "True"},
    ],
    "emoji_apps": [{"App": "zalo"}],
    "teencode_apps": [],
    "ngosen-macro": [
        {"Key": "ko", "Value": "mật khẩu ngân hàng"},
        {"Key": "KO", "Value": "không"},
        {"Key": "", "Value": "trống"},
    ],
    "emoji_words": [
        {"Emoji": "😂", "Keyword": "cười"},
        {"Emoji": "😂", "Keyword": "cười"},
        {"Emoji": "😂", "Keyword": ""},
    ],
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
        self.vim_conf = os.path.join(conf, "ngosen-vim-apps.conf")
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
        self.assertIn("Language by the switch key: kitty: English, zalo: Vietnamese", text)
        self.assertIn("Emoji suggestions on: zalo", text)
        self.assertIn("Teencode on: (none)", text)
        self.assertIn("Chat abbreviations off: (none)", text)
        self.assertIn("Vim mode on: kitty", text)

    def test_counts_macros_without_their_text(self):
        text = self.report()
        self.assertIn("Macros: 1", text)
        self.assertIn("Emoji keyword changes: 1", text)
        self.assertNotIn("mật khẩu", text)

    def test_prints_mode_switching_and_macro_format_settings(self):
        text = self.report()
        self.assertIn("(TimeFormat): %H:%M:%S", text)
        self.assertIn("(ModeOrder): Preedit,Sen,Emoji,Teencode,ChatShorthand,Off,Vim", text)
        self.assertIn("(ShowModeEmoji): off", text)

    def test_shows_a_saved_global_off_as_sen_like_the_window(self):
        handler = FakeHandler({"values": {**VALUES, "Mode": "OFF"}, "metadata": METADATA})
        self.assertIn("(Mode): Sen", self.report(handler))

    def test_shows_what_the_window_shows_for_odd_saved_values(self):
        values = {**VALUES, "TimeFormat": "%k:%M", "ModeOrder": "Vim,Bogus,Sen"}
        text = self.report(FakeHandler({"values": values, "metadata": METADATA}))
        self.assertIn("(TimeFormat): %H:%M", text)
        self.assertIn("(ModeOrder): Vim,Sen,Preedit,Emoji,Teencode,ChatShorthand,Off", text)

    def test_lists_the_apps_with_chat_abbreviations_off(self):
        conf = os.path.join(os.path.dirname(self.vim_conf), "ngosen-chat-shorthand-off-apps.conf")
        with open(conf, "w", encoding="utf-8") as f:
            f.write("# Apps where Ngó Sen leaves chat abbreviations as typed, one per line\ncode\n")
        self.assertIn("Chat abbreviations off: code", self.report())

    def test_keeps_the_good_lines_of_a_vim_list_that_is_not_utf8(self):
        with open(self.vim_conf, "wb") as f:
            f.write(b"kitty\n\xff\n")
        self.assertIn("Vim mode on: kitty", self.report())

    def test_says_so_when_fcitx5_does_not_answer(self):
        self.assertIn("cannot be shown", self.report(FakeHandler(config={})))


if __name__ == "__main__":
    unittest.main()
