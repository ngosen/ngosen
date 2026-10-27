# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

# isort: off
from fake_fcitx import FakeDBusHandler
import ui_gtk

# isort: on

CAN_RUN = ui_gtk.available() and bool(os.environ.get("DISPLAY"))
# The GTK CI job sets this, so a missing package fails the run instead of skipping these tests.
REQUIRED = os.environ.get("NGOSEN_WINDOW_TESTS") == "required"

if CAN_RUN:
    from gi.repository import Adw
    from ui_gtk import brand, window

    from core import dictionary


def isolate(test):
    """Keeps the dictionary files of the machine running the tests out of reach."""
    tmp = tempfile.TemporaryDirectory()
    test.addCleanup(tmp.cleanup)
    for patcher in (
        mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name}),
        mock.patch.object(dictionary, "BUNDLED_PATHS", [os.path.join(tmp.name, "bundled.dict")]),
    ):
        patcher.start()
        test.addCleanup(patcher.stop)
    with open(dictionary.BUNDLED_PATHS[0], "w", encoding="utf-8") as f:
        f.write("hello\nworld\n")
    return os.path.join(tmp.name, "fcitx5/ngosen/vietnamese.cm.dict")


class RequiredTest(unittest.TestCase):
    @unittest.skipUnless(REQUIRED, "not required here")
    def test_window_tests_run(self):
        self.assertTrue(CAN_RUN, "the GTK tests need GTK 4, libadwaita 1.5 and a display")


class NoFcitx(FakeDBusHandler):
    def get_config(self):
        return {}


class Restarting(FakeDBusHandler):
    """fcitx5 gone for a moment: nothing is written and every read comes back empty."""

    down = False

    def get_config(self):
        return {} if self.down else super().get_config()

    def set_config(self, values):
        return not self.down and super().set_config(values)

    def get_sub_config_list(self, path, root_key):
        return [] if self.down else super().get_sub_config_list(path, root_key)

    def set_sub_config_list(self, path, root_key, data_list):
        return not self.down and super().set_sub_config_list(path, root_key, data_list)


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkWindowTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.dbus = FakeDBusHandler()
        self.window = window.Window(None, self.dbus)

    def test_a_switch_saves_at_once(self):
        row = self.window.typing.rows["SpellCheck"]
        row.set_active(not row.get_active())
        self.assertEqual(self.dbus.values["SpellCheck"], str(row.get_active()))
        self.assertIsNotNone(self.window.toast)

    def test_a_combo_saves_the_value_not_the_label(self):
        row = self.window.typing.rows["InputMethod"]
        row.set_selected(row.values.index("VNI"))
        self.assertEqual(self.dbus.values["InputMethod"], "VNI")

    def test_the_mode_buttons_save_the_picked_mode(self):
        box = self.window.typing.rows["Mode"]
        buttons = [b for b in _children(box) if isinstance(b, window.Gtk.ToggleButton)]
        buttons[1].set_active(True)
        self.assertEqual(self.dbus.values["Mode"], "Preedit")

    def test_show_page_switches_and_ignores_pages_not_built_yet(self):
        self.window.show_page("about")
        self.assertEqual(self.window.stack.get_visible_child_name(), "about")
        self.window.show_page("whats-new")
        self.assertEqual(self.window.stack.get_visible_child_name(), "about")

    def test_without_fcitx5_the_page_says_so(self):
        other = window.Window(None, NoFcitx())
        self.assertIsNone(other.typing)
        self.assertIsInstance(other.stack.get_child_by_name("typing"), Adw.StatusPage)

    def test_the_accent_follows_the_libadwaita_version(self):
        self.assertIn("@define-color accent_bg_color #1c6b5f", brand.css(False, 5))
        self.assertIn("--accent-bg-color: #247b6c", brand.css(True, 6))


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkMacroPageTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.dbus = FakeDBusHandler(sub_configs={"ngosen-macro": [{"Key": "kg", "Value": "khô"}]})
        self.page = window.Window(None, self.dbus).macros

    def saved(self):
        return self.dbus.sub_configs["ngosen-macro"]

    def test_adding_saves_the_table_at_once(self):
        self.page.key_row.set_text("hn")
        self.page.value_row.set_text("Hà Nội")
        self.page.add_button.emit("clicked")
        self.assertEqual(self.saved()[-1], {"Key": "hn", "Value": "Hà Nội"})
        self.assertEqual(self.page.key_row.get_text(), "")

    def test_a_key_in_another_case_updates_the_entry(self):
        self.page.key_row.set_text("KG")
        self.page.value_row.set_text("khô gà")
        self.assertEqual(self.page.add_button.get_label(), window._("Update"))
        self.page.add_button.emit("clicked")
        self.assertEqual(self.saved(), [{"Key": "KG", "Value": "khô gà"}])

    def test_a_key_the_engine_cannot_match_is_refused(self):
        self.page.key_row.set_text("k g")
        self.page.value_row.set_text("x")
        self.assertFalse(self.page.add_button.get_sensitive())

    def test_removing_saves(self):
        self.page._remove("kg")
        self.assertEqual(self.saved(), [])

    def test_a_failed_save_keeps_the_table_for_the_next_one(self):
        dbus = Restarting(sub_configs={"ngosen-macro": [{"Key": "kg", "Value": "khô"}]})
        page = window.Window(None, dbus).macros
        dbus.down = True
        page.key_row.set_text("hn")
        page.value_row.set_text("Hà Nội")
        page.add_button.emit("clicked")
        dbus.down = False
        page.key_row.set_text("tks")
        page.value_row.set_text("cảm ơn")
        page.add_button.emit("clicked")
        self.assertEqual([e["Key"] for e in dbus.sub_configs["ngosen-macro"]], ["kg", "tks"])

    def test_a_switch_that_could_not_be_saved_turns_back(self):
        dbus = Restarting()
        page = window.Window(None, dbus).macros
        dbus.down = True
        page.switches["EnableMacro"].set_active(False)
        self.assertTrue(page.switches["EnableMacro"].get_active())
        self.assertTrue(page.table.options["EnableMacro"])

    def test_switches_and_formats_save_their_values(self):
        self.page.switches["EnableMacro"].set_active(False)
        self.assertEqual(self.dbus.values["EnableMacro"], "False")
        self.page.formats["TimeFormat"].set_selected(1)
        self.assertEqual(self.dbus.values["TimeFormat"], "%H:%M:%S")


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkDictionaryPageTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        self.local = isolate(self)
        self.dbus = FakeDBusHandler()
        self.window = window.Window(None, self.dbus)
        self.page = self.window.dictionary

    def local_words(self):
        with open(self.local, encoding="utf-8") as f:
            return f.read().split()

    def test_adding_a_word_writes_the_file_and_reloads_fcitx5(self):
        self.page.word_row.set_text("ngosen")
        self.page.add_button.emit("clicked")
        self.assertEqual(sorted(self.local_words()), ["hello", "ngosen", "world"])
        self.assertEqual(self.dbus.reloads, ["ngosen"])

    def test_a_word_with_a_space_or_already_there_is_refused(self):
        self.page.word_row.set_text("hai tu")
        self.assertFalse(self.page.add_button.get_sensitive())
        self.page.word_row.set_text("hello")
        self.assertFalse(self.page.add_button.get_sensitive())

    def test_removing_a_word_saves(self):
        self.page._remove("hello")
        self.assertEqual(self.local_words(), ["world"])

    def test_the_switch_saves(self):
        self.page.switch.set_active(True)
        self.assertEqual(self.dbus.values["EnableDictionary"], "True")

    def test_a_file_read_in_part_cannot_be_edited(self):
        os.makedirs(os.path.dirname(self.local), exist_ok=True)
        with open(self.local, "wb") as f:
            f.write(b"ok\n\xff\n")
        page = window.Window(None, self.dbus).dictionary
        self.assertFalse(page.switch.get_sensitive())
        self.assertFalse(page.editor.get_sensitive())

    def test_show_page_opens_the_dictionary_tab(self):
        self.window.show_page("dictionary")
        self.assertEqual(self.window.stack.get_visible_child_name(), "macros")
        self.assertEqual(self.window.tabs["dictionary"].get_visible_child_name(), "dictionary")


def _children(widget):
    child = widget.get_first_child()
    while child is not None:
        yield child
        yield from _children(child)
        child = child.get_next_sibling()


if __name__ == "__main__":
    unittest.main()
