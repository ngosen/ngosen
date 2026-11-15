# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
from unittest import mock

# isort: off
from support import FakeDBusHandler, app
from qtpy.QtWidgets import QLabel, QPushButton
from ui.main_window import NgoSenSettingsWindow
from ui.pages.emoji_page import EmojiPage

from core.emoji_table import EMOJI, GROUPS

# isort: on

SETTINGS = ("EmojiSuggestions", "EmojiPlacement", "YahooEmoticons")


class EmojiPageTest(unittest.TestCase):
    def open(self, fake=None):
        self.fake = fake or FakeDBusHandler()
        self.window = NgoSenSettingsWindow(dbus_handler=self.fake)
        self.addCleanup(self.window.close)
        self.window.resize(1000, 800)
        self.window.show()
        self.page = self.window.show_page("emoji")
        app.processEvents()
        return self.page

    def find(self, query):
        self.page.search.setText(query)
        self.page.search.returnPressed.emit()
        self.page.scroll.ensureWidgetVisible(self.page.view)
        app.processEvents()
        self.page.view.fill()

    def row(self, emoji):
        self.find(emoji)
        return next(r for r in self.page.view.live.values() if r.emoji == emoji)

    @staticmethod
    def chip(row, word):
        return next(c for c in row.chips if c.text() == word)

    def saved_words(self):
        self.window.save_pending()
        return self.fake.sub_configs.get("emoji_words")

    def test_the_page_opens_by_name_after_macros(self):
        self.assertIsInstance(self.open(), EmojiPage)
        titles = [self.window.sidebar.item(r).text() for r in range(self.window.sidebar.count())]
        self.assertEqual(titles.index("Emoji"), titles.index("Macros & Dictionary") + 1)

    def test_clicking_a_word_saves_it_off(self):
        self.open()
        chip = self.chip(self.row("😂"), "cười")
        self.assertEqual(chip.accessibleName(), "Turn off cười for 😂")
        chip.click()
        self.assertEqual(chip.accessibleName(), "Turn on cười for 😂")
        self.assertEqual(self.saved_words(), [{"Keyword": "cười", "Emoji": "😂", "On": "False"}])
        self.assertEqual(self.window.message_label.text(), "Saved “😂”.")

    def test_a_word_typed_in_the_box_is_added_on(self):
        self.open()
        row = self.row("😂")
        self.assertTrue(row.add_button.isHidden())
        row.entry.setText("Hài Quá")
        self.assertFalse(row.add_button.isHidden())
        row.entry.returnPressed.emit()
        self.assertTrue(self.chip(row, "hài quá").isChecked())
        self.assertEqual(row.entry.text(), "")
        self.assertEqual(self.saved_words(), [{"Keyword": "hài quá", "Emoji": "😂", "On": "True"}])

    def test_as_before_puts_back_the_default_words(self):
        self.open()
        row = self.row("😂")
        self.assertTrue(row.reset_button.isHidden())
        self.chip(row, "cười").click()
        self.assertFalse(row.reset_button.isHidden())
        self.saved_words()
        row.reset_button.click()
        self.assertTrue(self.chip(row, "cười").isChecked())
        self.assertTrue(row.reset_button.isHidden())
        self.assertEqual(self.saved_words(), [])

    def test_a_failed_save_shows_the_word_as_it_was(self):
        self.open()
        chip = self.chip(self.row("😂"), "cười")
        with mock.patch.object(self.fake, "set_sub_config_list", return_value=False):
            chip.click()
            self.window.save_pending()
        chip = self.chip(self.row("😂"), "cười")
        self.assertTrue(chip.isChecked())
        self.assertFalse(self.page.is_modified())

    def test_turn_back_on_and_undo(self):
        self.open(FakeDBusHandler(sub_configs={"emoji_off_apps": [{"App": "firefox"}]}))
        buttons = [b for b in self.page.findChildren(QPushButton) if b.text() == "Turn back on"]
        self.assertEqual(len(buttons), 1)
        buttons[0].click()
        app.processEvents()
        self.assertEqual(self.fake.sub_configs["emoji_off_apps"], [])
        self.assertEqual(self.window.message_label.text(), "Emoji are back on in firefox.")
        self.assertIn("Not turned off in any app.", self.shown_texts())
        self.window.undo()
        app.processEvents()
        self.assertEqual(self.fake.sub_configs["emoji_off_apps"], [{"App": "firefox"}])
        self.assertIn("firefox", self.shown_texts())
        self.assertFalse(self.page.is_modified())

    def shown_texts(self):
        return [label.text() for label in self.page._apps.findChildren(QLabel) if label.isVisible()]

    def test_the_filters_change_the_count(self):
        self.open()
        count = self.page.count_line
        self.assertEqual(count.text(), f"{len(EMOJI)} emoji")
        flags = next(b for b in self.page.group_buttons.buttons() if b.text() == "Flags")
        flags.click()
        in_flags = sum(1 for row in EMOJI if GROUPS[row[1]] == "Flags")
        self.assertEqual(count.text(), f"{in_flags} emoji")
        self.page.group_buttons.buttons()[0].click()
        self.page.only_on.click()
        with_words = int(count.text().split()[0])
        self.assertLess(with_words, len(EMOJI))
        self.find("cười")
        self.assertLess(int(count.text().split()[0]), with_words)
        self.find("no such word")
        self.assertEqual(count.text(), "No emoji match")

    def test_only_rows_near_the_view_are_built(self):
        self.open()
        self.find("")
        self.assertTrue(self.page.view.live)
        self.assertLess(len(self.page.view.live), 100)
        bar = self.page.scroll.verticalScrollBar()
        bar.setValue(bar.maximum())
        app.processEvents()
        self.assertIn(len(self.page._shown) - 1, self.page.view.live)
        self.assertLess(len(self.page.view.live), 100)

    def test_the_settings_open_from_the_window(self):
        self.open()
        for key in SETTINGS:
            self.assertTrue(self.window.open_setting(key), key)
            self.assertIs(self.window.focusWidget(), self.page.control_for(key), key)

    def test_defaults_bring_back_words_and_apps(self):
        self.open(FakeDBusHandler(sub_configs={"emoji_off_apps": [{"App": "firefox"}]}))
        self.chip(self.row("😂"), "cười").click()
        self.window.save_pending()
        self.assertTrue(self.page.is_modified_from_default())
        with mock.patch("ui.main_window.confirm", return_value=True):
            self.window.on_restore_defaults()
        self.window.save_pending()
        self.assertEqual(self.fake.sub_configs["emoji_words"], [])
        self.assertEqual(self.fake.sub_configs["emoji_off_apps"], [])

    def test_defaults_turn_on_an_app_turned_off_after_opening(self):
        self.open()
        self.fake.sub_configs["emoji_off_apps"] = [{"App": "kate"}]
        with mock.patch("ui.main_window.confirm", return_value=True):
            self.window.on_restore_defaults()
        self.window.save_pending()
        self.assertEqual(self.fake.sub_configs["emoji_off_apps"], [])

    def test_the_hint_names_keys_saved_on_another_page(self):
        self.open()
        self.assertIn("press ` then w", self.page._apps_hint.text())
        self.fake.values.update(ModeMenuKey={"0": "Control+e"}, ShortcutEmoji="x")
        self.window.show_page("typing")
        self.window.show_page("emoji")
        app.processEvents()
        self.assertIn("press Ctrl+E then x", self.page._apps_hint.text())


if __name__ == "__main__":
    unittest.main()
