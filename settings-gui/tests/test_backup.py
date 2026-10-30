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


if __name__ == "__main__":
    unittest.main()
