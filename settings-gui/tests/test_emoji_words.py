# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import tempfile
import unittest
from unittest import mock

# isort: off
from fake_fcitx import FakeDBusHandler
from core.emoji_words import EmojiWords, export_words, read_shared, toggle_hint

# isort: on


def words(fake=None):
    return EmojiWords(fake or FakeDBusHandler())


def saved(fake):
    return fake.sub_configs.get("emoji_words")


class ChipsTest(unittest.TestCase):
    def test_default_words_are_on_and_name_words_off(self):
        chips = dict(words().chips("😂"))
        self.assertTrue(chips["cười"])
        self.assertTrue(chips["haha"])
        self.assertFalse(chips["nước mắt"])

    def test_turning_a_default_word_off_saves_only_that(self):
        fake = FakeDBusHandler()
        table = words(fake)
        table.toggle("😂", "cười")
        self.assertTrue(table.save())
        self.assertEqual(saved(fake), [{"Keyword": "cười", "Emoji": "😂", "On": "False"}])
        self.assertFalse(dict(table.chips("😂"))["cười"])

    def test_english_words_are_on_after_the_vietnamese(self):
        words_of_cat = [w for w, _on in words().chips("🐈")]
        self.assertIn(("cat", True), words().chips("🐈"))
        self.assertLess(words_of_cat.index("mèo"), words_of_cat.index("cat"))

    def test_english_off_hides_english_words(self):
        table = words()
        table.english = False
        self.assertNotIn("cat", dict(table.chips("🐈")))
        self.assertEqual(table.rows("cat"), [])

    def test_turning_an_english_word_off_saves_it(self):
        fake = FakeDBusHandler()
        table = words(fake)
        table.toggle("🐈", "cat")
        self.assertTrue(table.save())
        self.assertEqual(saved(fake), [{"Keyword": "cat", "Emoji": "🐈", "On": "False"}])

    def test_turning_it_back_on_saves_nothing(self):
        fake = FakeDBusHandler()
        table = words(fake)
        table.toggle("😂", "cười")
        table.toggle("😂", "cười")
        self.assertFalse(table.changed("😂"))
        self.assertTrue(table.save())
        self.assertEqual(saved(fake), [])

    def test_a_word_of_its_name_turns_on(self):
        fake = FakeDBusHandler()
        table = words(fake)
        table.toggle("😂", "nước mắt")
        table.save()
        self.assertEqual(saved(fake), [{"Keyword": "nước mắt", "Emoji": "😂", "On": "True"}])

    def test_an_added_word_is_kept_lower_case_and_stays_when_turned_off(self):
        fake = FakeDBusHandler()
        table = words(fake)
        self.assertTrue(table.add("👔", "  Sếp   lớn "))
        self.assertTrue(dict(table.chips("👔"))["sếp lớn"])
        table.toggle("👔", "sếp lớn")
        self.assertIn(("sếp lớn", False), table.chips("👔"))
        table.save()
        self.assertEqual(saved(fake), [{"Keyword": "sếp lớn", "Emoji": "👔", "On": "False"}])

    def test_an_empty_or_existing_word_is_not_added(self):
        table = words()
        self.assertFalse(table.add("😂", "   "))
        self.assertFalse(table.add("😂", "Cười"))
        self.assertFalse(table.changed("😂"))

    def test_as_before_drops_every_change_to_an_emoji(self):
        table = words()
        table.toggle("😂", "cười")
        table.add("😂", "vui ghê")
        table.toggle("🤣", "cười")
        table.reset("😂")
        self.assertFalse(table.changed("😂"))
        self.assertTrue(table.changed("🤣"))

    def test_saved_edits_are_read_back(self):
        fake = FakeDBusHandler(
            sub_configs={
                "emoji_words": [
                    {"Keyword": "cười", "Emoji": "😂", "On": "False"},
                    {"Keyword": "sếp", "Emoji": "👔", "On": "True"},
                ]
            }
        )
        table = words(fake)
        self.assertFalse(dict(table.chips("😂"))["cười"])
        self.assertTrue(dict(table.chips("👔"))["sếp"])
        self.assertFalse(table.is_modified())

    def test_a_refused_save_keeps_the_change_unsaved(self):
        fake = FakeDBusHandler()
        table = words(fake)
        table.toggle("😂", "cười")
        with mock.patch.object(fake, "set_sub_config_list", return_value=False):
            self.assertFalse(table.save())
        self.assertTrue(table.is_modified())


class FilterTest(unittest.TestCase):
    def test_a_group_shows_its_emoji_only(self):
        shown = [e for e, *_ in words().rows(group="Food & Drink")]
        self.assertIn("🍜", shown)
        self.assertNotIn("😂", shown)

    def test_search_finds_a_word_on_or_off(self):
        shown = [e for e, *_ in words().rows(query="tiền")]
        self.assertIn("💰", shown)
        self.assertNotIn("😂", shown)

    def test_search_ignores_case_and_finds_the_emoji_itself(self):
        self.assertIn("😂", [e for e, *_ in words().rows(query="CƯỜI")])
        self.assertEqual([e for e, *_ in words().rows(query="🐱")], ["🐱"])

    def test_only_emoji_with_a_word_on(self):
        table = words()
        # Almost every emoji has an English word on.
        table.english = False
        self.assertNotIn("📱", [e for e, *_ in table.rows(only_on=True)])
        table.toggle("📱", "điện thoại")
        self.assertIn("📱", [e for e, *_ in table.rows(only_on=True)])

    def test_counts(self):
        table = words()
        self.assertGreater(len(table.rows()), 1800)
        self.assertGreater(len(table.rows(only_on=True)), 900)


class OffAppsTest(unittest.TestCase):
    def test_turning_emoji_back_on_and_undo(self):
        fake = FakeDBusHandler(
            sub_configs={"emoji_off_apps": [{"App": "firefox"}, {"App": "kate"}]}
        )
        table = words(fake)
        self.assertEqual(table.off_apps, ["firefox", "kate"])
        undo = table.enable_app("firefox")
        self.assertEqual(fake.sub_configs["emoji_off_apps"], [{"App": "kate"}])
        self.assertTrue(undo())
        self.assertEqual(fake.sub_configs["emoji_off_apps"], [{"App": "firefox"}, {"App": "kate"}])

    def test_turning_back_on_keeps_an_app_turned_off_since(self):
        fake = FakeDBusHandler(sub_configs={"emoji_off_apps": [{"App": "firefox"}]})
        table = words(fake)
        fake.sub_configs["emoji_off_apps"].append({"App": "kate"})
        table.enable_app("firefox")
        self.assertEqual(fake.sub_configs["emoji_off_apps"], [{"App": "kate"}])
        self.assertEqual(table.off_apps, ["kate"])

    def test_undo_keeps_an_app_turned_off_since(self):
        fake = FakeDBusHandler(sub_configs={"emoji_off_apps": [{"App": "firefox"}]})
        undo = words(fake).enable_app("firefox")
        fake.sub_configs["emoji_off_apps"].append({"App": "kate"})
        self.assertTrue(undo())
        self.assertEqual(fake.sub_configs["emoji_off_apps"], [{"App": "firefox"}, {"App": "kate"}])


class ToggleHintTest(unittest.TestCase):
    def test_names_the_keys_the_user_set(self):
        self.assertIn("press ` then w", toggle_hint({"0": "grave"}, "w"))
        self.assertIn("press Ctrl+E then e", toggle_hint({"0": "Control+e"}, "e"))

    def test_without_a_menu_key_points_to_where_to_set_one(self):
        self.assertNotIn("press", toggle_hint({}, "w"))
        self.assertNotIn("press", toggle_hint({"0": "grave"}, ""))


class ShareTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.path = os.path.join(tmp.name, "words.json")

    def write(self, data):
        with open(self.path, "w", encoding="utf-8") as f:
            json.dump(data, f)

    def test_export_then_import_adds_the_words_turned_on(self):
        mine = words()
        mine.add("👔", "sếp lớn")
        mine.toggle("😂", "cười")
        self.assertEqual(export_words(mine, self.path), 1)
        theirs = words()
        self.assertEqual(theirs.merge(read_shared(self.path)), 1)
        self.assertTrue(theirs.is_on("👔", "sếp lớn"))
        self.assertTrue(theirs.is_on("😂", "cười"))

    def test_import_keeps_a_word_the_user_turned_off(self):
        self.write(
            {
                "meta": {"kind": "ngosen-emoji-words", "version": 1},
                "emoji_words": [{"Keyword": "cười", "Emoji": "😂"}],
            }
        )
        table = words()
        table.toggle("😂", "cười")
        self.assertEqual(table.merge(read_shared(self.path)), 0)
        self.assertFalse(table.is_on("😂", "cười"))

    def test_unknown_emoji_and_long_words_are_skipped(self):
        self.write(
            {
                "meta": {"kind": "ngosen-emoji-words", "version": 1},
                "emoji_words": [
                    {"Keyword": "Sếp  Lớn", "Emoji": "👔"},
                    {"Keyword": "x", "Emoji": "not an emoji"},
                    {"Keyword": "y" * 41, "Emoji": "👔"},
                ],
            }
        )
        self.assertEqual(read_shared(self.path), [("👔", "sếp lớn")])

    def test_other_files_are_refused(self):
        for data in (
            {"config": {}, "emoji_words": []},
            {"meta": {"kind": "ngosen-emoji-words"}, "emoji_words": "sếp"},
            {"meta": {"kind": "ngosen-emoji-words"}, "emoji_words": [5]},
            [1, 2],
        ):
            self.write(data)
            with self.assertRaises(ValueError, msg=data):
                read_shared(self.path)


class DefaultsTest(unittest.TestCase):
    def test_defaults_clear_words_and_apps(self):
        fake = FakeDBusHandler(
            sub_configs={
                "emoji_words": [{"Keyword": "sếp", "Emoji": "👔", "On": "True"}],
                "emoji_off_apps": [{"App": "kate"}],
            }
        )
        table = words(fake)
        self.assertTrue(table.is_modified_from_default())
        table.restore_defaults()
        self.assertTrue(table.save() and table.save_apps())
        self.assertEqual(saved(fake), [])
        self.assertEqual(fake.sub_configs["emoji_off_apps"], [])


if __name__ == "__main__":
    unittest.main()
