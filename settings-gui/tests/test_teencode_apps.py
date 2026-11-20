# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

# isort: off
from support import FakeDBusHandler, app
from qtpy.QtWidgets import QLabel, QPushButton
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory

from core.teencode_apps import TeencodeApps, toggle_hint

# isort: on


class TeencodeAppsTest(unittest.TestCase):
    def test_turning_off_keeps_apps_the_keyboard_added_since(self):
        fake = FakeDBusHandler(sub_configs={"teencode_apps": [{"App": "discord"}]})
        apps = TeencodeApps(fake)
        fake.sub_configs["teencode_apps"].append({"App": "zalo"})
        self.assertTrue(apps.turn_off("discord"))
        self.assertEqual(fake.sub_configs["teencode_apps"], [{"App": "zalo"}])
        self.assertEqual(apps.apps, ["zalo"])

    def test_the_hint_names_the_keys(self):
        hint = toggle_hint({"0": "grave"}, "t")
        self.assertIn("then t", hint)
        self.assertIn("Keys page", toggle_hint({}, "t"))


class TypingPageTest(unittest.TestCase):
    def open(self, fake):
        self.fake = fake
        self.page = DynamicSettingsPage(fake, category=SettingsCategory.TYPING)
        self.addCleanup(self.page.close)
        self.page.show()
        app.processEvents()

    def shown_texts(self):
        labels = self.page._teencode_list.findChildren(QLabel)
        return [label.text() for label in labels if label.isVisible()]

    def test_turn_off_from_the_list(self):
        self.open(FakeDBusHandler(sub_configs={"teencode_apps": [{"App": "discord"}]}))
        self.assertIn("discord", self.shown_texts())
        buttons = [
            b
            for b in self.page.findChildren(QPushButton)
            if b.text() == "Turn off" and b.isVisible()
        ]
        self.assertEqual(len(buttons), 1)
        buttons[0].click()
        app.processEvents()
        self.assertEqual(self.fake.sub_configs["teencode_apps"], [])
        self.assertIn("Not on in any app.", self.shown_texts())

    def test_apps_turned_on_from_the_keyboard_show_up(self):
        self.open(FakeDBusHandler())
        self.assertIn("Not on in any app.", self.shown_texts())
        self.fake.sub_configs["teencode_apps"] = [{"App": "zalo"}]
        self.page.refresh_status()
        app.processEvents()
        self.assertIn("zalo", self.shown_texts())


if __name__ == "__main__":
    unittest.main()
