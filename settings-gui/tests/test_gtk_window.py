# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import unittest

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


class RequiredTest(unittest.TestCase):
    @unittest.skipUnless(REQUIRED, "not required here")
    def test_window_tests_run(self):
        self.assertTrue(CAN_RUN, "the GTK tests need GTK 4, libadwaita 1.5 and a display")


class NoFcitx(FakeDBusHandler):
    def get_config(self):
        return {}


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkWindowTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
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

    def test_switches_and_formats_save_their_values(self):
        self.page.switches["EnableMacro"].set_active(False)
        self.assertEqual(self.dbus.values["EnableMacro"], "False")
        self.page.formats["TimeFormat"].set_selected(1)
        self.assertEqual(self.dbus.values["TimeFormat"], "%H:%M:%S")


def _children(widget):
    child = widget.get_first_child()
    while child is not None:
        yield child
        yield from _children(child)
        child = child.get_next_sibling()


if __name__ == "__main__":
    unittest.main()
