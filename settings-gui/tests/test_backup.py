# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import tempfile
import unittest

# fake_fcitx puts the settings window on sys.path, so it has to come first.
# isort: off
import fake_fcitx  # noqa: F401
from core import backup

# isort: on


class LoadTest(unittest.TestCase):
    def load(self, data):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "b.json")
            with open(path, "w", encoding="utf-8") as f:
                json.dump(data, f)
            return backup.load(path)

    def test_a_backup_loads(self):
        data = {
            "config": {"SpellCheck": "True"},
            "macros": [{"Key": "kg", "Value": "khô"}],
            "dictionary": "hello\n",
        }
        self.assertEqual(self.load(data), data)

    def test_parts_of_the_wrong_type_are_refused_before_anything_is_written(self):
        for data in (
            {"dictionary": 5},
            {"config": [1]},
            {"macros": 5},
            {"rules": [1]},
            {"config": {}, "switches": "kate"},
            {"config": {}, "keymaps": None},
        ):
            with self.subTest(data):
                with self.assertRaises(ValueError):
                    self.load(data)

    def test_switches_and_numbers_written_by_hand_load_as_fcitx5_text(self):
        data = {
            "config": {
                "SpellCheck": False,
                "WaitSurroundingTimeoutMs": 5,
                "ToggleKey": {"0": "Control+space"},
            },
            "switches": [{"App": "kate", "Vietnamese": True}],
        }
        loaded = self.load(data)
        self.assertEqual(
            loaded["config"],
            {
                "SpellCheck": "False",
                "WaitSurroundingTimeoutMs": "5",
                "ToggleKey": {"0": "Control+space"},
            },
        )
        self.assertEqual(loaded["switches"], [{"App": "kate", "Vietnamese": "True"}])

    def test_null_still_clears_a_value(self):
        loaded = self.load({"macros": [{"Key": "vn", "Value": None}]})
        self.assertEqual(loaded["macros"], [{"Key": "vn", "Value": ""}])

    def test_lists_fcitx5_cannot_hold_are_refused(self):
        for data in (
            {"config": {"ToggleKey": ["Control+space"]}},
            {"macros": [{"Key": "vn", "Value": ["Việt Nam"]}]},
        ):
            with self.subTest(data):
                with self.assertRaises(ValueError):
                    self.load(data)

    def test_a_file_nested_too_deep_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "b.json")
            with open(path, "w", encoding="utf-8") as f:
                f.write("[" * 100000 + "]" * 100000)
            with self.assertRaises(ValueError):
                backup.load(path)

    def test_a_file_of_shared_emoji_words_is_not_restored(self):
        shared = {
            "meta": {"kind": "ngosen-emoji-words", "version": 1},
            "emoji_words": [{"Keyword": "sếp", "Emoji": "👔"}],
        }
        with self.assertRaises(ValueError):
            self.load(shared)


class RoundTripTest(unittest.TestCase):
    def test_the_toggle_key_switches_come_back_with_the_rules(self):
        rules = [{"App": "firefox", "Mode": "5"}]
        switches = [{"App": "kate", "Vietnamese": "False"}]
        source = fake_fcitx.FakeDBusHandler(
            sub_configs={"app_rules": rules, "app_switches": switches}
        )
        target = fake_fcitx.FakeDBusHandler()
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "b.json")
            backup.export(source, path)
            self.assertTrue(backup.apply(target, backup.load(path)))
        self.assertEqual(target.sub_configs["app_rules"], rules)
        self.assertEqual(target.sub_configs["app_switches"], switches)

    def test_teencode_apps_come_back(self):
        apps = [{"App": "discord"}]
        source = fake_fcitx.FakeDBusHandler(sub_configs={"teencode_apps": apps})
        target = fake_fcitx.FakeDBusHandler()
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "b.json")
            backup.export(source, path)
            self.assertTrue(backup.apply(target, backup.load(path)))
        self.assertEqual(target.sub_configs["teencode_apps"], apps)

    def test_an_older_backup_brings_its_rules_back_without_todays_switches(self):
        rules = [{"App": "kate", "Mode": "0"}]
        target = fake_fcitx.FakeDBusHandler(
            sub_configs={"app_switches": [{"App": "kate", "Vietnamese": "True"}]}
        )
        self.assertTrue(backup.apply(target, {"rules": rules}))
        self.assertEqual(target.sub_configs["app_rules"], rules)
        self.assertEqual(target.sub_configs["app_switches"], [])

    def test_a_backup_from_the_ibus_edition_keeps_todays_switches(self):
        rules = [{"App": "kate", "Mode": "5"}]
        switches = [{"App": "kate", "Vietnamese": "True"}]
        target = fake_fcitx.FakeDBusHandler(sub_configs={"app_switches": switches})
        data = {"rules": rules, "meta": {"version": 1, "edition": "ibus"}}
        self.assertTrue(backup.apply(target, data))
        self.assertEqual(target.sub_configs["app_rules"], rules)
        self.assertEqual(target.sub_configs["app_switches"], switches)

    def test_a_damaged_meta_counts_as_an_older_backup(self):
        rules = [{"App": "kate", "Mode": "0"}]
        for meta in ("ibus", ["ibus"], None, {"edition": 5}):
            with self.subTest(meta):
                target = fake_fcitx.FakeDBusHandler(
                    sub_configs={"app_switches": [{"App": "kate", "Vietnamese": "True"}]}
                )
                self.assertTrue(backup.apply(target, {"rules": rules, "meta": meta}))
                self.assertEqual(target.sub_configs["app_switches"], [])


if __name__ == "__main__":
    unittest.main()
