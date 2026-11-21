# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Each list the window saves, sent over D-Bus to a fake fcitx5 that checks the wire format.

The page tests use an in-process fake that takes any Python value, so a list fcitx5 would
refuse or drop (#303) passes them.
"""

import os
import subprocess
import sys
import tempfile
import unittest

from fcitx5_service import AVAILABLE, GUI_DIR, PrivateFcitx5

ADDON = "fcitx://config/addon/ngosen"

# Each case edits a list the way its page does and saves it, then prints what save returned.
SAVE = """
import sys

sys.path.insert(0, sys.argv[1])
from core.dbus_handler import NgoSenDBusHandler

dbus = NgoSenDBusHandler()
case = sys.argv[2]
if case == "macros":
    from core.macros import MacroTable

    table = MacroTable(dbus)
    table.load()
    table.entries.append({"Key": "vn", "Value": "Việt Nam"})
    print(table.save())
elif case == "keymap":
    from core.keymap import Keymap

    table = Keymap(dbus)
    table.load()
    table.enabled = True
    table.entries.append({"Key": "z", "Value": "D_Đ"})
    print(table.save())
elif case == "app_modes":
    from core.app_modes import RememberedAppModes

    # The fake fcitx5 remembers no app, so give Defaults something to forget.
    modes = RememberedAppModes(dbus)
    modes.lists = {
        "app_rules": [{"App": "kate", "Mode": "Preedit"}],
        "app_switches": [{"App": "kate", "Vietnamese": "True"}],
    }
    modes.restore_defaults()
    print(modes.save_data())
elif case == "emoji_words":
    from core.emoji_words import EmojiWords

    words = EmojiWords(dbus)
    words.add("😀", "vui")
    print(words.save())
elif case == "emoji_off_apps":
    from core.emoji_words import EmojiWords

    words = EmojiWords(dbus)
    words.off_apps = ["firefox"]
    print(words.save_apps())
elif case == "backup":
    from core import backup

    print(backup.apply(dbus, backup.load(sys.argv[3])))
"""

# A backup as the IBus edition writes it, with every list the fcitx5 edition restores.
BACKUP = """{
  "config": {"DdFreeStyle": "False", "ToggleKey": {"0": "Control+space"}},
  "macros": [{"Key": "vn", "Value": "Việt Nam"}],
  "keymaps": [{"Key": "z", "Value": "D_Đ"}],
  "rules": [{"App": "kate", "Mode": "Preedit"}],
  "switches": [{"App": "kate", "Vietnamese": "True"}],
  "emoji_words": [{"Keyword": "vui", "Emoji": "😀", "On": "True"}],
  "emoji_off_apps": [{"App": "firefox"}],
  "teencode_apps": [{"App": "discord"}],
  "dictionary": "ngosen\\n",
  "meta": {"version": 1, "edition": "ibus"}
}
"""


@unittest.skipUnless(AVAILABLE, "needs dbus-daemon and gi")
class ListsReachFcitx5Test(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.fcitx5 = PrivateFcitx5(self.tmp.name)
        self.addCleanup(self.fcitx5.close)
        self.fcitx5.start()

    def save(self, case, *args):
        env = dict(self.fcitx5.env, XDG_DATA_HOME=os.path.join(self.tmp.name, "data"))
        result = subprocess.run(
            [sys.executable, "-c", SAVE, GUI_DIR, case, *args],
            env=env,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stderr[-2000:])
        self.assertEqual(result.stdout.split()[-1:], ["True"], result.stdout[-2000:])
        return {
            c["uri"].removeprefix(ADDON + "/"): c["values"] for c in self.fcitx5.set_config_calls()
        }

    def test_macros(self):
        saved = self.save("macros")
        self.assertEqual(
            saved["ngosen-macro"], {"Macro": {"0": {"Key": "vn", "Value": "Việt Nam"}}}
        )

    def test_keymap(self):
        saved = self.save("keymap")
        self.assertEqual(saved[ADDON]["EnableCustomKeymap"], "True")
        self.assertEqual(
            saved["custom_keymap"], {"CustomKeymap": {"0": {"Key": "z", "Value": "D_Đ"}}}
        )

    def test_forgetting_app_modes(self):
        saved = self.save("app_modes")
        self.assertEqual(saved["app_rules"], {"Rules": {}})
        self.assertEqual(saved["app_switches"], {"Switches": {}})

    def test_emoji_words(self):
        saved = self.save("emoji_words")
        word = {"Keyword": "vui", "Emoji": "😀", "On": "True"}
        self.assertEqual(saved["emoji_words"], {"Words": {"0": word}})

    def test_emoji_off_apps(self):
        saved = self.save("emoji_off_apps")
        self.assertEqual(saved["emoji_off_apps"], {"Apps": {"0": {"App": "firefox"}}})

    def test_restoring_a_backup(self):
        path = os.path.join(self.tmp.name, "backup.json")
        with open(path, "w", encoding="utf-8") as f:
            f.write(BACKUP)
        saved = self.save("backup", path)
        self.assertEqual(saved[ADDON]["DdFreeStyle"], "False")
        self.assertEqual(saved[ADDON]["ToggleKey"], {"0": "Control+space"})
        expected = {
            "ngosen-macro": {"Macro": {"0": {"Key": "vn", "Value": "Việt Nam"}}},
            "custom_keymap": {"CustomKeymap": {"0": {"Key": "z", "Value": "D_Đ"}}},
            "app_rules": {"Rules": {"0": {"App": "kate", "Mode": "Preedit"}}},
            "app_switches": {"Switches": {"0": {"App": "kate", "Vietnamese": "True"}}},
            "emoji_words": {"Words": {"0": {"Keyword": "vui", "Emoji": "😀", "On": "True"}}},
            "emoji_off_apps": {"Apps": {"0": {"App": "firefox"}}},
            "teencode_apps": {"Apps": {"0": {"App": "discord"}}},
        }
        for uri, values in expected.items():
            self.assertEqual(saved[uri], values, uri)


if __name__ == "__main__":
    unittest.main()
