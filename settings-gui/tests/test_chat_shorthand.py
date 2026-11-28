# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import re
import unittest

from chat_sample import ChatEngine
from core.chat_shorthand import ChatShorthand, Entry
from core.ngosen_call import NO_REPLY, NOT_RUNNING, TOO_OLD
from qtpy.QtCore import Qt
from qtpy.QtTest import QTest
from qtpy.QtWidgets import QLabel, QPushButton
from support import FakeDBusHandler, app  # noqa: F401
from ui.pages.macro_editor import MacroEditorPage

from core import chat_shorthand

NOT_RUNNING_LINE = "Ngó Sen is not running. Type something with it, then try again."


class ListingTest(unittest.TestCase):
    def test_kept_keys_come_first_then_by_key(self):
        listing = "ko\tkhông\tkept\nae\tanh em\t\ndc\tđược\tkept\nbgio\tbây giờ"
        self.assertEqual(
            chat_shorthand.parse(listing),
            [
                Entry("dc", "được", True),
                Entry("ko", "không", True),
                Entry("ae", "anh em", False),
                Entry("bgio", "bây giờ", False),
            ],
        )

    def test_lines_that_are_no_entry_are_left_out(self):
        self.assertEqual(chat_shorthand.parse(""), [])
        self.assertEqual(
            chat_shorthand.parse("ko\n\tkhông\t\n\nae\tanh em\t"), [Entry("ae", "anh em", False)]
        )

    def test_the_line_names_the_kept_keys(self):
        entries = chat_shorthand.parse("ko\tkhông\tkept\nae\tanh em\t\ndc\tđược\tkept")
        self.assertEqual(chat_shorthand.kept_line(entries), "Kept as typed: dc, ko.")
        self.assertEqual(chat_shorthand.kept_line(entries[2:]), "None is kept as typed.")


class AppHintTest(unittest.TestCase):
    def hint(self, **values):
        return chat_shorthand.app_hint(
            {"ModeMenuKey": {"0": "grave"}, "ShortcutChatShorthand": "c", **values}.get
        )

    def test_the_hint_names_the_keys(self):
        self.assertIn("press ` then c", self.hint())

    def test_without_a_key_it_points_at_the_keys_page(self):
        for values in (
            {"ModeMenuKey": {}},
            {"ShortcutChatShorthand": ""},
            {"ShowModeChatShorthand": "False"},
        ):
            self.assertIn("Keys page", self.hint(**values), values)

    def test_no_hint_while_they_are_off_or_fcitx5_has_no_key_for_them(self):
        # Off everywhere, the menu has no such item; an older fcitx5 has no key to name.
        for values in (
            {"ChatShorthand": "False"},
            {"ChatShorthand": False},
            {"ShortcutChatShorthand": None},
        ):
            self.assertEqual(self.hint(**values), "", values)


class CallsTest(unittest.TestCase):
    def test_the_entries_come_from_the_running_ngosen(self):
        entries, why = ChatShorthand(ChatEngine(kept=["ko"])).entries()
        self.assertIsNone(why)
        self.assertEqual([entry.key for entry in entries], ["ko", "ae", "dc"])

    def test_a_ngosen_that_cannot_be_asked_says_why(self):
        fake = FakeDBusHandler()
        for answer, line in (
            (NOT_RUNNING[0], NOT_RUNNING_LINE),
            (
                TOO_OLD,
                "The running Ngó Sen is older than this window. Restart fcitx5, then try again.",
            ),
            (NO_REPLY, "Ngó Sen did not answer. Try again."),
            (None, "No desktop session bus, so Ngó Sen cannot be reached."),
            ("org.example.Other", "Could not read the abbreviations: org.example.Other"),
        ):
            fake.ngosen_answers = {"ChatShorthandList": answer}
            self.assertEqual(ChatShorthand(fake).entries(), (None, line), answer)
        # Until it is used, fcitx5 has not loaded Ngó Sen.
        self.assertEqual(ChatShorthand(FakeDBusHandler()).entries(), (None, NOT_RUNNING_LINE))

    def test_expanding_again_names_the_key(self):
        fake = ChatEngine(kept=["ko", "dc"])
        self.assertEqual(ChatShorthand(fake).expand_again("ko"), (True, "ko expands again."))
        self.assertEqual(fake.asked, [("ExpandShorthandAgain", "ko")])
        self.assertEqual(fake.kept, {"dc"})

    def test_expanding_again_says_why_it_could_not(self):
        fake = ChatEngine(kept=["ko"], refuses="Permission denied")
        self.assertEqual(
            ChatShorthand(fake).expand_again("ko"),
            (False, "Could not expand it again: Permission denied"),
        )
        self.assertEqual(
            ChatShorthand(FakeDBusHandler()).expand_again("ko"), (False, NOT_RUNNING_LINE)
        )


class QtPageTest(unittest.TestCase):
    def page(self, fake):
        page = MacroEditorPage(fake)
        self.messages = []
        page._page_message = lambda text, error=False: self.messages.append((text, error))
        return page

    def rows(self, page):
        table = page.chat_table
        return [
            (
                table.item(row, 0).text(),
                table.item(row, 1).text(),
                table.cellWidget(row, 2) is not None,
            )
            for row in range(table.rowCount())
        ]

    def test_the_switch_is_saved_and_read_back(self):
        fake = FakeDBusHandler()
        page = MacroEditorPage(fake)
        self.assertTrue(page.cb_chat.isChecked())
        self.assertFalse(page.is_modified())
        self.assertFalse(page.is_modified_from_default())
        page.cb_chat.setChecked(False)
        self.assertTrue(page.is_modified())
        self.assertTrue(page.save_data())
        self.assertEqual(fake.values["ChatShorthand"], "False")
        again = MacroEditorPage(fake)
        self.assertFalse(again.cb_chat.isChecked())
        self.assertEqual(again.value_of("ChatShorthand"), False)
        self.assertIs(again.control_for("ChatShorthand"), again.cb_chat)
        again.restore_defaults()
        self.assertTrue(again.cb_chat.isChecked())

    def test_the_switch_does_not_wait_for_the_macro_switch(self):
        page = MacroEditorPage(FakeDBusHandler())
        page.cb_enable.setChecked(False)
        self.assertTrue(page.cb_chat.isEnabled())
        self.assertFalse(page.cb_capitalize.isEnabled())

    def test_the_switch_says_what_it_does(self):
        page = MacroEditorPage(FakeDBusHandler())
        hints = [label.text() for label in page.findChildren(QLabel)]
        self.assertIn(
            "ko gives không, dc gives được. Backspace right after the space gives back what you typed.",
            hints,
        )

    def test_the_page_says_how_to_turn_them_off_in_one_app(self):
        fake = FakeDBusHandler()
        page = MacroEditorPage(fake)
        self.assertIn("press ` then c", page.chat_app_hint.text())
        # Changed on the Keys page while this one stayed open.
        fake.values["ShortcutChatShorthand"] = "x"
        page.refresh_status()
        self.assertIn("press ` then x", page.chat_app_hint.text())

    def test_the_page_hides_the_hint_while_they_are_off(self):
        fake = FakeDBusHandler()
        fake.values["ChatShorthand"] = "False"
        page = MacroEditorPage(fake)
        self.assertTrue(page.chat_app_hint.isHidden())
        fake.values["ChatShorthand"] = "True"
        page.refresh_status()
        self.assertFalse(page.chat_app_hint.isHidden())

    def test_the_line_names_the_kept_keys(self):
        self.assertEqual(self.page(ChatEngine()).chat_line.text(), "None is kept as typed.")
        fake = ChatEngine(kept=["ko"])
        page = self.page(fake)
        self.assertEqual(page.chat_line.text(), "Kept as typed: ko.")
        # Taken back while the window was in the background.
        fake.kept.add("dc")
        page.refresh_status()
        self.assertEqual(page.chat_line.text(), "Kept as typed: dc, ko.")

    def test_the_line_says_why_nothing_can_be_listed(self):
        page = self.page(FakeDBusHandler())
        self.assertEqual(page.chat_line.text(), NOT_RUNNING_LINE)
        page.chat_view_button.click()
        self.assertTrue(page.chat_table.isHidden())
        self.assertEqual(self.messages, [(NOT_RUNNING_LINE, True)])
        self.assertEqual(page.chat_view_button.text(), "View the list")

    def test_view_lists_them_with_the_kept_ones_first(self):
        page = self.page(ChatEngine(kept=["ko"]))
        self.assertTrue(page.chat_table.isHidden())
        page.chat_view_button.click()
        self.assertFalse(page.chat_table.isHidden())
        self.assertEqual(page.chat_view_button.text(), "Hide the list")
        self.assertEqual(
            self.rows(page),
            [("ko", "không", True), ("ae", "anh em", False), ("dc", "được", False)],
        )
        button = page.chat_table.cellWidget(0, 2)
        self.assertEqual(button.text(), "Expand again")
        self.assertEqual(button.accessibleName(), "Expand ko again")
        page.chat_view_button.click()
        self.assertTrue(page.chat_table.isHidden())
        self.assertEqual(page.chat_table.rowCount(), 0)
        self.assertEqual(page.chat_view_button.text(), "View the list")

    def test_view_shows_the_line_of_the_list_it_read(self):
        fake = ChatEngine()
        page = self.page(fake)
        # Taken back since the page last asked.
        fake.kept.add("ko")
        page.chat_view_button.click()
        self.assertEqual(self.rows(page)[0], ("ko", "không", True))
        self.assertEqual(page.chat_line.text(), "Kept as typed: ko.")

    def test_the_macro_switch_does_not_ask_the_engine(self):
        fake = ChatEngine()
        page = self.page(fake)
        asked = len(fake.asked)
        page.cb_enable.setChecked(False)
        self.assertEqual(len(fake.asked), asked)

    def test_tab_goes_through_the_list_and_on(self):
        page = self.page(ChatEngine(kept=["ko", "dc"]))
        page.show()
        self.addCleanup(page.close)
        page.chat_view_button.click()
        page.activateWindow()
        page.chat_table.setFocus()
        app.processEvents()
        self.assertIs(app.focusWidget(), page.chat_table)
        for row in (0, 1):
            QTest.keyClick(app.focusWidget(), Qt.Key_Tab)
            self.assertIs(app.focusWidget(), page.chat_table.cellWidget(row, 2))
        QTest.keyClick(app.focusWidget(), Qt.Key_Tab)
        self.assertFalse(page.chat_table.isAncestorOf(app.focusWidget()))
        self.assertIsNot(app.focusWidget(), page.chat_table)

    def test_a_shown_list_follows_the_engine(self):
        fake = ChatEngine()
        page = self.page(fake)
        page.chat_view_button.click()
        fake.kept.add("dc")
        page.refresh_status()
        self.assertEqual(self.rows(page)[0], ("dc", "được", True))
        page.chat.dbus = FakeDBusHandler()
        page.refresh_status()
        self.assertTrue(page.chat_table.isHidden())
        self.assertEqual(page.chat_line.text(), NOT_RUNNING_LINE)

    def test_expand_again_gives_the_key_back_to_the_engine(self):
        fake = ChatEngine(kept=["ko", "dc"])
        page = self.page(fake)
        page.chat_view_button.click()
        page.chat_table.cellWidget(1, 2).click()
        self.assertEqual(fake.asked[-2:], [("ExpandShorthandAgain", "ko"), ("ChatShorthandList",)])
        self.assertEqual(self.messages, [("ko expands again.", False)])
        self.assertEqual(
            self.rows(page),
            [("dc", "được", True), ("ae", "anh em", False), ("ko", "không", False)],
        )
        self.assertEqual(page.chat_line.text(), "Kept as typed: dc.")

    def test_expand_again_says_why_it_could_not(self):
        page = self.page(ChatEngine(kept=["ko"], refuses="Permission denied"))
        page.chat_view_button.click()
        page.chat_table.cellWidget(0, 2).click()
        self.assertEqual(self.messages, [("Could not expand it again: Permission denied", True)])
        self.assertEqual(self.rows(page)[0], ("ko", "không", True))

    def test_the_list_is_not_among_the_settings_to_find(self):
        page = self.page(ChatEngine(kept=["ko"]))
        self.assertFalse(page.chat_table.searchable)
        self.assertFalse(page.chat_line.searchable)
        self.assertIsInstance(page.chat_view_button, QPushButton)

    def test_new_texts_have_a_vietnamese_text(self):
        po = os.path.join(os.path.dirname(__file__), "..", "..", "po", "vi.po")
        with open(po, encoding="utf-8") as f:
            text = f.read()
        for msgid in (
            "ko gives không, dc gives được. Backspace right after the space gives back what you typed.",
            "View the list",
            "Hide the list",
            "Kept as typed",
            "Kept as typed: {}.",
            "None is kept as typed.",
            "Expand again",
            "Expand {} again",
            "{} expands again.",
            "Could not expand it again: {}",
            "Could not read the abbreviations: {}",
            "Chat abbreviations",
            "The list",
            NOT_RUNNING_LINE,
            "Ngó Sen did not answer. Try again.",
        ):
            self.assertRegex(text, f'msgid "{re.escape(msgid)}"\\nmsgstr "[^"]+"', msgid)


if __name__ == "__main__":
    unittest.main()
