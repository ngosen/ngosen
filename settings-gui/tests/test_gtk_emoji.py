# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import time
import unittest

# isort: off
import window_display
from fake_fcitx import FakeDBusHandler
import ui_gtk

# isort: on

CAN_RUN = ui_gtk.available() and window_display.can_show()

if CAN_RUN:
    from core.catalog import SETTINGS_MAP, SettingsCategory
    from core.emoji_table import EMOJI, GROUPS
    from gi.repository import Adw, GLib, Gtk
    from test_gtk_window import Restarting, isolate
    from ui_gtk import window

OFF_APPS = {"emoji_off_apps": [{"App": "firefox"}]}


def _settle(until=lambda: True, seconds=5):
    """Runs the main loop until it is idle and until() holds, so the list view lays out its rows."""
    context = GLib.MainContext.default()
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        while context.pending():
            context.iteration(False)
        if until():
            return True
        context.iteration(False)
        time.sleep(0.005)
    return False


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and NGOSEN_WINDOW_TESTS")
class GtkEmojiPageTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.dbus = self.fake()
        self.window = self.open(self.dbus)
        self.page = self.window.emoji

    def fake(self, values=None):
        return FakeDBusHandler(values, sub_configs=OFF_APPS)

    def open(self, dbus):
        w = window.Window(None, dbus)
        self.addCleanup(w.destroy)
        w.set_default_size(1000, 800)
        w.present()
        w.show_page("emoji")
        self.assertTrue(_settle(lambda: w.emoji._bound))
        return w

    def row(self, emoji, page=None):
        page = page or self.page
        self.assertTrue(_settle(lambda: any(r.emoji == emoji for r in page._bound.values())))
        return next(r for r in page._bound.values() if r.emoji == emoji)

    @staticmethod
    def chip(row, word):
        return next(c for c in row.chips if c.get_visible() and c.word == word)

    def words(self):
        return self.dbus.sub_configs.get("emoji_words")

    def test_clicking_a_word_turns_it_off_and_saves_at_once(self):
        row = self.row("😂")
        self.chip(row, "cười").emit("clicked")
        self.assertEqual(self.words(), [{"Keyword": "cười", "Emoji": "😂", "On": "False"}])
        self.assertFalse(self.chip(row, "cười").get_active())
        self.assertTrue(row.reset.get_visible())
        self.assertIsNotNone(self.window.toast)

    def test_a_word_that_could_not_be_saved_turns_back(self):
        dbus = Restarting(sub_configs=OFF_APPS)
        page = self.open(dbus).emoji
        row = self.row("😂", page)
        dbus.down = True
        self.chip(row, "cười").emit("clicked")
        self.assertTrue(self.chip(row, "cười").get_active())
        self.assertFalse(page.words.changed("😂"))

    def test_adding_a_word_saves_it_and_shows_it_on(self):
        row = self.row("😂")
        row.new.emit("clicked")
        editor = self.page.editor
        self.assertIs(editor.row, row)
        self.assertFalse(editor.add_button.get_visible())
        editor.entry.set_text("Hài  Quá")
        self.assertTrue(editor.add_button.get_visible())
        editor.entry.emit("activate")
        self.assertEqual(self.words(), [{"Keyword": "hài quá", "Emoji": "😂", "On": "True"}])
        self.assertTrue(self.chip(row, "hài quá").get_active())
        self.assertEqual(editor.entry.get_text(), "")

    def test_a_word_already_there_is_not_added_again(self):
        row = self.row("😂")
        row.new.emit("clicked")
        self.page.editor.entry.set_text("haha")
        self.page.editor.add_button.emit("clicked")
        self.assertIsNone(self.words())
        self.assertTrue(self.page.editor.entry.has_css_class("error"))

    def test_as_before_puts_back_the_default_words(self):
        row = self.row("😂")
        self.chip(row, "cười").emit("clicked")
        row.reset.emit("clicked")
        self.assertEqual(self.words(), [])
        self.assertTrue(self.chip(row, "cười").get_active())
        self.assertFalse(row.reset.get_visible())

    def test_turn_back_on_saves_and_undo_turns_it_off_again(self):
        rows = list(_rows(self.page.apps))
        self.assertEqual([r.get_title() for r in rows], ["firefox"])
        button = next(c for c in _children(rows[0]) if isinstance(c, Gtk.Button))
        button.emit("clicked")
        self.assertEqual(self.dbus.sub_configs["emoji_off_apps"], [])
        self.assertEqual(
            [r.get_title() for r in _rows(self.page.apps)], [window._("Not turned off in any app.")]
        )
        toast = self.window.toast
        self.assertEqual(toast.get_title(), window._("Emoji are back on in {}.").format("firefox"))
        toast.emit("button-clicked")
        self.assertEqual(self.dbus.sub_configs["emoji_off_apps"], [{"App": "firefox"}])
        self.assertEqual([r.get_title() for r in _rows(self.page.apps)], ["firefox"])

    def test_the_filters_change_the_count(self):
        def count():
            return self.page.count.get_text()

        self.assertEqual(count(), window._("{} emoji").format(len(EMOJI)))
        self.page.group_buttons[GROUPS.index("Flags") + 1].set_active(True)
        flags = sum(1 for row in EMOJI if GROUPS[row[1]] == "Flags")
        self.assertEqual(count(), window._("{} emoji").format(flags))
        self.page.group_buttons[0].set_active(True)
        self.page.search.set_text("tiền")
        self.page.refilter()
        found = self.page.words.rows("tiền")
        self.assertEqual(count(), window._("{} emoji").format(len(found)))
        self.assertLess(len(found), flags)
        self.page.search.set_text("")
        self.page.only_on.set_active(True)
        on = self.page.words.rows(only_on=True)
        self.assertEqual(count(), window._("{} emoji").format(len(on)))
        self.assertLess(len(on), len(EMOJI))
        self.page.search.set_text("no such word")
        self.page.refilter()
        self.assertEqual(count(), window._("No emoji match"))

    def test_the_list_makes_rows_only_for_what_can_be_shown(self):
        self.assertLess(len(self.page._bound), len(EMOJI) / 4)

    def test_the_settings_are_found_and_opened_from_the_window(self):
        for key in (k for keys in SETTINGS_MAP[SettingsCategory.EMOJI].values() for k in keys):
            with self.subTest(key=key):
                self.window.show_page("typing")
                self.assertTrue(self.window.open_setting(key))
                self.assertIs(self.window.get_focus(), self.page.control_for(key))
                self.window.search_entry.set_text(self.page.control_for(key).get_title())
                self.window._on_search(self.window.search_entry)
                results = self.window.results
                found = [r.found[2] for r in _children(results) if hasattr(r, "found")]
                self.assertIn(key, found)

    def test_a_locked_setting_opens_the_switch_that_unlocks_it(self):
        page = self.open(self.fake({"EmojiSuggestions": "False"})).emoji
        self.assertFalse(page.control_for("YahooEmoticons").get_sensitive())
        w = page.page.get_root()
        self.assertTrue(w.open_setting("YahooEmoticons"))
        self.assertIs(w.get_focus(), page.control_for("EmojiSuggestions"))

    def test_the_example_follows_the_settings(self):
        self.assertTrue(self.page.example.get_visible())
        self.assertEqual(self.page.picked.get_text(), "Hôm nay cười 🤣")
        placement = self.page.rows["EmojiPlacement"]
        placement.set_selected(placement.values.index("Instead of the word"))
        self.assertEqual(self.page.picked.get_text(), "Hôm nay 🤣")
        self.page.rows["EmojiSuggestions"].set_active(False)
        self.assertFalse(self.page.example.get_visible())


def _children(widget):
    child = widget.get_first_child()
    while child is not None:
        yield child
        yield from _children(child)
        child = child.get_next_sibling()


def _rows(listbox):
    return (c for c in _children(listbox) if isinstance(c, Adw.ActionRow))


if __name__ == "__main__":
    unittest.main()
