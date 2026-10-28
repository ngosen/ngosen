# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import time
import unittest
from unittest import mock
from urllib.parse import unquote

# isort: off
from fake_fcitx import DEFAULTS, FakeDBusHandler
import ui_gtk

# isort: on

CAN_RUN = ui_gtk.available() and bool(os.environ.get("DISPLAY"))
# The GTK CI job sets this, so a missing package fails the run instead of skipping these tests.
REQUIRED = os.environ.get("NGOSEN_WINDOW_TESTS") == "required"

if CAN_RUN:
    from core.brand import MESSENGER_URL
    from gi.repository import Adw, Gdk, Gio, GLib, Gtk
    from ui_gtk import brand, check_dialog, window
    from ui_gtk.hotkey_button import HotkeyButton

    from core import dictionary, keymap, system_check, typing_log, whats_new_content


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
        self.window.show_page("system")
        self.assertEqual(self.window.stack.get_visible_child_name(), "about")

    def test_without_fcitx5_the_page_says_so(self):
        other = window.Window(None, NoFcitx())
        self.assertIsNone(other.typing)
        self.assertIsInstance(other.stack.get_child_by_name("typing"), Adw.StatusPage)

    def test_the_accent_follows_the_libadwaita_version(self):
        self.assertIn("@define-color accent_bg_color #1c6b5f", brand.css(False, 5))
        self.assertIn("--accent-bg-color: #247b6c", brand.css(True, 6))

    def test_the_accent_beats_the_users_gtk_css(self):
        # Desktops such as Plasma write their accent to ~/.config/gtk-4.0/gtk.css.
        theme = Gtk.CssProvider()
        theme.load_from_string(
            "@define-color accent_color #545a92; :root { --accent-color: #545a92; }"
        )
        display = Gdk.Display.get_default()
        Gtk.StyleContext.add_provider_for_display(display, theme, Gtk.STYLE_PROVIDER_PRIORITY_USER)
        self.addCleanup(Gtk.StyleContext.remove_provider_for_display, display, theme)
        ours = brand.install()
        self.addCleanup(Gtk.StyleContext.remove_provider_for_display, display, ours)
        label = Gtk.Label(label="x", css_classes=["accent"])
        self.addCleanup(Gtk.Window(child=label).destroy)
        color = label.get_color()
        shade = 1 if Adw.StyleManager.get_default().get_dark() else 0
        expected = Gdk.RGBA()
        expected.parse(brand.ENAMEL_TEXT[shade])
        self.assertEqual(
            (round(color.red * 255), round(color.green * 255), round(color.blue * 255)),
            (round(expected.red * 255), round(expected.green * 255), round(expected.blue * 255)),
        )


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

    def test_a_word_with_other_whitespace_is_refused(self):
        for word in ("ngo\u00a0sen", "a\tb"):
            self.page.word_row.set_text(word)
            self.assertFalse(self.page.add_button.get_sensitive(), repr(word))

    def test_removing_a_word_removes_every_copy(self):
        os.makedirs(os.path.dirname(self.local), exist_ok=True)
        with open(self.local, "w", encoding="utf-8") as f:
            f.write("hello\nhello\nworld\n")
        page = window.Window(None, self.dbus).dictionary
        page._remove("hello")
        self.assertEqual(window.Window(None, self.dbus).dictionary.dictionary.words, ["world"])

    def test_removing_a_word_changes_only_its_row(self):
        changes = []
        self.page.words.connect("items-changed", lambda _m, *change: changes.append(change))
        self.page._remove("hello")
        self.assertEqual(changes, [(0, 1, 0)])

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


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkKeymapPageTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.dbus = FakeDBusHandler(
            sub_configs={"custom_keymap": [{"Key": "s", "Value": "DauSac"}]}
        )
        self.page = window.Window(None, self.dbus).keymap

    def saved(self):
        return [(e["Key"], e["Value"]) for e in self.dbus.sub_configs["custom_keymap"]]

    def test_adding_and_updating_a_key_saves(self):
        self.page.key_row.set_text("f")
        self.page.action.set_selected(2)
        self.page.add_button.emit("clicked")
        self.page.key_row.set_text("s")
        self.assertEqual(self.page.add_button.get_label(), "Update")
        self.page.action.set_selected(0)
        self.page.add_button.emit("clicked")
        self.assertEqual(self.saved(), [("s", "XoaDauThanh"), ("f", "DauHuyen")])

    def test_changing_the_action_in_a_row_saves(self):
        row = self.page.list.get_row_at_index(0)
        dropdown = next(w for w in _children(row) if isinstance(w, Gtk.DropDown))
        dropdown.set_selected(3)
        self.assertEqual(self.saved(), [("s", "DauHoi")])

    def test_a_key_longer_than_one_character_is_refused(self):
        self.page.key_row.set_text("sf")
        self.assertFalse(self.page.add_button.get_sensitive())

    def test_changing_the_action_keeps_the_row(self):
        row = self.page.list.get_row_at_index(0)
        dropdown = next(w for w in _children(row) if isinstance(w, Gtk.DropDown))
        dropdown.set_selected(3)
        self.assertIs(self.page.list.get_row_at_index(0), row)

    def test_a_row_loads_into_the_editor_and_the_search_filters(self):
        self.page.apply("VNI")
        row = self.page.list.get_row_at_index(0)
        row.emit("activated")
        self.assertEqual(self.page.key_row.get_text(), row.entry["Key"])
        self.assertEqual(self.page.add_button.get_label(), "Update")
        self.page.search.set_text("Dấu sắc")
        self.page.list.invalidate_filter()
        rows = (r for r in _children(self.page.list) if hasattr(r, "entry"))
        shown = [r.entry["Key"] for r in rows if r.get_child_visible()]
        self.assertEqual(shown, ["1"])

    def test_removing_a_key_saves(self):
        self.page._remove("s")
        self.assertEqual(self.saved(), [])

    def test_a_preset_replaces_every_key(self):
        self.page.apply("VNI")
        self.assertEqual(self.saved(), list(keymap.PRESETS["VNI"]))

    def test_the_switch_saves(self):
        self.page.switch.set_active(True)
        self.assertEqual(self.dbus.values["EnableCustomKeymap"], "True")

    def test_a_failed_save_keeps_the_keys_for_the_next_one(self):
        dbus = Restarting(sub_configs={"custom_keymap": [{"Key": "s", "Value": "DauSac"}]})
        page = window.Window(None, dbus).keymap
        dbus.down = True
        page.apply("VNI")
        dbus.down = False
        page.key_row.set_text("f")
        page.add_button.emit("clicked")
        self.assertEqual([e["Key"] for e in dbus.sub_configs["custom_keymap"]], ["s", "f"])


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkShortcutsPageTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.dbus = FakeDBusHandler({"ToggleKey": {"0": "Control+space"}})
        self.window = window.Window(None, self.dbus)
        self.page = self.window.shortcuts

    def test_a_recorded_toggle_key_saves_and_ctrl_shift_is_offered_back(self):
        self.page.hotkeys["ToggleKey"].emit("recorded", "F12")
        self.assertEqual(self.dbus.values["ToggleKey"], {"0": "F12"})
        self.assertTrue(self.page.offer.get_visible())
        button = next(w for w in _children(self.page.offer) if isinstance(w, Gtk.Button))
        button.emit("clicked")
        self.assertEqual(self.dbus.values["ToggleKey"], DEFAULTS["ToggleKey"])
        self.assertFalse(self.page.offer.get_visible())
        self.assertEqual(self.page.hotkeys["ToggleKey"].key, "Control+Shift")

    def test_a_new_menu_key_keeps_the_keys_after_it(self):
        dbus = FakeDBusHandler({"ModeMenuKey": {"0": "grave", "1": "Control+space"}})
        page = window.Window(None, dbus).shortcuts
        page.hotkeys["ModeMenuKey"].emit("recorded", "F12")
        self.assertEqual(dbus.values["ModeMenuKey"], {"0": "F12", "1": "Control+space"})

    def test_a_shared_mode_key_is_held_back_until_it_is_resolved(self):
        rows = self.page.mode_rows
        key_of = {
            k: next(w for w in _children(r) if isinstance(w, HotkeyButton)) for k, r in rows.items()
        }
        key_of["ShortcutPreedit"].emit("recorded", "w")
        self.assertEqual(self.dbus.values["ShortcutPreedit"], "q")
        self.assertTrue(rows["ShortcutPreedit"].has_css_class("error"))
        self.assertTrue(rows["ShortcutEmoji"].has_css_class("error"))
        key_of["ShortcutEmoji"].emit("recorded", "q")
        self.assertEqual(self.dbus.values["ShortcutPreedit"], "w")
        self.assertEqual(self.dbus.values["ShortcutEmoji"], "q")
        self.assertFalse(rows["ShortcutPreedit"].has_css_class("error"))

    def test_hiding_a_mode_ends_its_conflict(self):
        rows = self.page.mode_rows
        button = next(w for w in _children(rows["ShortcutPreedit"]) if isinstance(w, HotkeyButton))
        button.emit("recorded", "w")
        shown = next(w for w in _children(rows["ShortcutEmoji"]) if isinstance(w, Gtk.CheckButton))
        shown.set_active(False)
        self.assertEqual(self.dbus.values["ShowModeEmoji"], "False")
        self.assertEqual(self.dbus.values["ShortcutPreedit"], "w")

    def test_other_settings_still_save_while_mode_keys_clash(self):
        button = next(
            w
            for w in _children(self.page.mode_rows["ShortcutPreedit"])
            if isinstance(w, HotkeyButton)
        )
        button.emit("recorded", "w")
        self.window.typing.rows["SpellCheck"].set_active(False)
        self.assertEqual(self.dbus.values["SpellCheck"], "False")
        self.assertEqual(self.dbus.values["ShortcutPreedit"], "q")

    def test_dropping_a_mode_saves_the_order(self):
        self.assertTrue(self.page.move("Off", "Sen"))
        self.assertEqual(self.dbus.values["ModeOrder"], "Off,Sen,Preedit,Emoji,Default")
        self.page.move("Sen", "Emoji")
        self.assertEqual(self.dbus.values["ModeOrder"], "Off,Preedit,Emoji,Sen,Default")

    def test_show_page_opens_the_shortcuts_tab(self):
        self.window.show_page("keymap")
        self.window.show_page("shortcuts")
        self.assertEqual(self.window.stack.get_visible_child_name(), "keys")
        self.assertEqual(self.window.tabs["shortcuts"].get_visible_child_name(), "shortcuts")


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkAppearancePageTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.dbus = FakeDBusHandler()
        self.page = window.Window(None, self.dbus).appearance

    def test_switching_reopens_in_qt_when_it_is_installed(self):
        row = self.page.rows["SettingsToolkit"]
        button = next(w for w in _children(row) if isinstance(w, Gtk.Button) and w.get_label())
        self.assertEqual(button.get_label(), "Switch to Qt…")
        app = mock.Mock()
        with (
            mock.patch(
                "ui_gtk.toolkit_switch.Offer", return_value=mock.Mock(ready=True, target="qt")
            ),
            mock.patch("ui_gtk.toolkit_switch.switch", return_value=True) as switch,
            mock.patch.object(Gio.Application, "get_default", return_value=app),
        ):
            button.emit("clicked")
            self.assertFalse(button.get_sensitive())
            context = GLib.MainContext.default()
            for _ in range(500):
                if app.quit.called:
                    break
                context.iteration(False)
                time.sleep(0.01)
        switch.assert_called_once_with(self.page.model, "qt")
        app.quit.assert_called_once()

    def test_each_language_is_named_in_itself(self):
        row = self.page.rows["UiLanguage"]
        names = [row.get_model().get_string(i) for i in range(row.get_model().get_n_items())]
        self.assertIn("Tiếng Việt", names)
        row.set_selected(row.values.index("English"))
        self.assertEqual(self.dbus.values["UiLanguage"], "English")

    def test_menu_switches_name_the_menu_entry(self):
        row = self.page.rows["ShowMenuCharset"]
        self.assertEqual(row.get_title(), "Charset")
        row.set_active(not row.get_active())
        self.assertEqual(self.dbus.values["ShowMenuCharset"], str(row.get_active()))

    def test_tray_preview_follows_the_icon_settings(self):
        self.page.rows["UseLotusIcons"].set_active(False)
        theme = self.page.rows["IconTheme"]
        theme.set_selected(theme.values.index("Dark"))
        self.assertTrue(self.page.preview.has_css_class("tray-dark"))
        theme.set_selected(theme.values.index("Light"))
        self.assertTrue(self.page.preview.has_css_class("tray-light"))
        self.assertFalse(self.page.preview.has_css_class("tray-dark"))


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkUndoTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.dbus = FakeDBusHandler(
            {"SpellCheck": "True"}, sub_configs={"ngosen-macro": [{"Key": "kg", "Value": "khô"}]}
        )
        self.window = window.Window(None, self.dbus)
        self.addCleanup(self.window.destroy)

    def undo(self):
        self.assertTrue(self.window.toast.get_button_label())
        self.window.toast.emit("button-clicked")

    def test_undo_puts_a_setting_back_and_shows_it(self):
        self.window.typing.rows["SpellCheck"].set_active(False)
        self.assertEqual(self.dbus.values["SpellCheck"], "False")
        self.undo()
        self.assertEqual(self.dbus.values["SpellCheck"], "True")
        self.assertTrue(self.window.typing.rows["SpellCheck"].get_active())
        self.assertIsNone(self.window.toast.get_button_label())

    def test_undo_brings_back_a_removed_macro(self):
        self.window.macros._remove("kg")
        self.assertEqual(self.dbus.sub_configs["ngosen-macro"], [])
        self.undo()
        self.assertEqual(self.dbus.sub_configs["ngosen-macro"], [{"Key": "kg", "Value": "khô"}])
        self.assertEqual([e["Key"] for e in self.window.macros.table.entries], ["kg"])

    def test_undo_stays_on_the_tab_shown(self):
        self.window.show_page("dictionary")
        self.window.macros._remove("kg")
        self.undo()
        self.assertEqual(self.window.stack.get_visible_child_name(), "macros")
        self.assertEqual(self.window.tabs["dictionary"].get_visible_child_name(), "dictionary")
        self.assertIn("typing", [name for name, _w in self.window.search_pages])

    def test_undo_keeps_what_fcitx5_remembered_since_the_window_opened(self):
        kate = [{"App": "kate", "Mode": "3"}]
        self.dbus.sub_configs["app_rules"] = kate
        self.window.typing.rows["SpellCheck"].set_active(False)
        self.undo()
        self.assertEqual(self.dbus.sub_configs["app_rules"], kate)
        self.window.macros._remove("kg")
        self.undo()
        self.assertEqual(self.dbus.sub_configs["app_rules"], kate)

    def test_no_undo_when_fcitx5_could_not_be_read(self):
        with mock.patch.object(window.settings_snapshot, "take", return_value=None):
            self.window.typing.rows["SpellCheck"].set_active(False)
        self.assertIsNone(self.window.toast.get_button_label())


class RefusesAppRules(FakeDBusHandler):
    def set_sub_config_list(self, path, root_key, data_list):
        return path != "app_rules" and super().set_sub_config_list(path, root_key, data_list)


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkDefaultsTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        self.local = isolate(self)
        self.dbus = self.fcitx()
        self.window = window.Window(None, self.dbus)
        self.addCleanup(self.window.destroy)

    def fcitx(self, cls=FakeDBusHandler):
        return cls(
            {"SpellCheck": "False", "EnableCustomKeymap": "True"},
            sub_configs={
                "ngosen-macro": [{"Key": "kg", "Value": "khô"}],
                "custom_keymap": [{"Key": "s", "Value": "DauSac"}],
                "app_rules": [{"App": "firefox", "Mode": "5"}],
            },
        )

    def changed(self):
        return (
            self.dbus.values["SpellCheck"],
            self.dbus.values["EnableCustomKeymap"],
            [len(self.dbus.sub_configs[k]) for k in ("ngosen-macro", "custom_keymap", "app_rules")],
        )

    def local_words(self):
        with open(self.local, encoding="utf-8") as f:
            return sorted(f.read().split())

    def test_defaults_clear_everything_and_undo_brings_it_back(self):
        self.window.dictionary.word_row.set_text("ngosen")
        self.window.dictionary.add_button.emit("clicked")
        self.window.restore_defaults()
        self.assertEqual(self.changed(), ("True", "False", [0, 0, 0]))
        self.assertFalse(os.path.exists(self.local))
        self.assertTrue(self.window.typing.rows["SpellCheck"].get_active())
        self.assertEqual(self.window.macros.table.entries, [])
        self.assertEqual(self.window.toast.get_title(), "Defaults restored.")
        self.window.toast.emit("button-clicked")
        self.assertEqual(self.changed(), ("False", "True", [1, 1, 1]))
        self.assertEqual(self.local_words(), ["hello", "ngosen", "world"])

    def test_only_confirming_the_dialog_restores(self):
        dialog = self.window.confirm_defaults()
        dialog.emit("response", "cancel")
        self.assertEqual(self.changed(), ("False", "True", [1, 1, 1]))
        dialog = self.window.confirm_defaults()
        dialog.emit("response", "reset")
        self.assertEqual(self.changed(), ("True", "False", [0, 0, 0]))

    def test_a_dictionary_read_in_part_is_left_as_it_is(self):
        os.makedirs(os.path.dirname(self.local), exist_ok=True)
        with open(self.local, "wb") as f:
            f.write(b"ok\n\xff\n")
        self.window.restore_defaults()
        with open(self.local, "rb") as f:
            self.assertEqual(f.read(), b"ok\n\xff\n")
        self.assertEqual(self.window.toast.get_timeout(), 0)

    def test_undo_brings_back_what_fcitx5_remembered_after_the_window_opened(self):
        self.dbus.sub_configs["app_rules"].append({"App": "kate", "Mode": "3"})
        self.window.restore_defaults()
        self.window.toast.emit("button-clicked")
        self.assertEqual(len(self.dbus.sub_configs["app_rules"]), 2)

    def test_nothing_is_reset_when_it_could_not_be_undone(self):
        with mock.patch.object(window.settings_snapshot, "take", return_value=None):
            self.window.restore_defaults()
        self.assertEqual(self.changed(), ("False", "True", [1, 1, 1]))
        self.assertIsNone(self.window.toast.get_button_label())

    def test_a_partial_reset_says_so_and_can_be_undone(self):
        self.dbus = self.fcitx(RefusesAppRules)
        self.window = window.Window(None, self.dbus)
        self.addCleanup(self.window.destroy)
        self.window.restore_defaults()
        self.assertEqual(self.changed(), ("True", "False", [0, 0, 1]))
        self.assertTrue(self.window.typing.rows["SpellCheck"].get_active())
        self.assertEqual(self.window.toast.get_timeout(), 0)
        self.window.toast.emit("button-clicked")
        self.assertEqual(self.changed(), ("False", "True", [1, 1, 1]))


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkBackupTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.path = os.path.join(os.environ["XDG_DATA_HOME"], "b.json")
        self.dbus = FakeDBusHandler(
            {"SpellCheck": "True"}, sub_configs={"ngosen-macro": [{"Key": "kg", "Value": "khô"}]}
        )
        self.window = window.Window(None, self.dbus)
        self.addCleanup(self.window.destroy)

    def test_restoring_a_backup_puts_it_back_and_can_be_undone(self):
        self.window.export_backup(self.path)
        self.assertTrue(self.window.toast.get_title().startswith("Backed up to "))
        self.window.typing.rows["SpellCheck"].set_active(False)
        self.window.macros._remove("kg")
        self.window.restore_backup(self.path)
        self.assertEqual(self.window.toast.get_title(), "Restored from b.json.")
        self.assertEqual(self.dbus.values["SpellCheck"], "True")
        self.assertTrue(self.window.typing.rows["SpellCheck"].get_active())
        self.assertEqual([e["Key"] for e in self.window.macros.table.entries], ["kg"])
        self.window.toast.emit("button-clicked")
        self.assertEqual(self.dbus.values["SpellCheck"], "False")
        self.assertEqual(self.dbus.sub_configs["ngosen-macro"], [])

    def test_a_file_that_is_not_a_backup_changes_nothing(self):
        with open(self.path, "w") as f:
            f.write("[]")
        self.window.restore_backup(self.path)
        self.assertTrue(self.window.toast.get_title().startswith("Failed to open backup file:"))
        self.assertEqual(self.dbus.sub_configs["ngosen-macro"], [{"Key": "kg", "Value": "khô"}])

    def test_a_backup_that_cannot_be_written_says_so(self):
        self.window.export_backup(os.path.join(self.path, "missing", "b.json"))
        self.assertTrue(self.window.toast.get_title().startswith("Failed to export backup:"))

    def test_the_menu_offers_backup_and_restore(self):
        for name in ("backup", "restore-backup", "restore-defaults"):
            self.assertIsNotNone(self.window.lookup_action(name), name)

    def test_a_cancelled_file_dialog_does_nothing(self):
        def cancelled(_result):
            raise GLib.Error("dismissed")

        use = mock.Mock()
        self.window._on_file(None, None, cancelled, use)
        use.assert_not_called()
        picked = mock.Mock(get_path=mock.Mock(return_value=self.path))
        self.window._on_file(None, None, lambda _r: picked, use)
        use.assert_called_once_with(self.path)


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkCheckDialogTest(unittest.TestCase):
    NO_IM = {"XDG_SESSION_TYPE": "x11", "XDG_CURRENT_DESKTOP": "XFCE"}

    def setUp(self):
        Adw.init()
        # The fcitx5 and IBus of the machine running the tests must not add a row.
        for name in ("runs_replaced_addon", "_process_running"):
            patcher = mock.patch.object(system_check, name, return_value=False)
            patcher.start()
            self.addCleanup(patcher.stop)
        with mock.patch.dict(os.environ, self.NO_IM, clear=True):
            self.check = check_dialog.CheckDialog(FakeDBusHandler(), "1.4.2")

    def clipboard(self):
        text = []
        clipboard = Gdk.Display.get_default().get_clipboard()
        clipboard.read_text_async(None, lambda c, result: text.append(c.read_text_finish(result)))
        GLib.timeout_add_seconds(5, lambda: text.append(None))
        while not text:
            GLib.MainContext.default().iteration(True)
        return text[0] or ""

    def levels(self):
        return [row.level for row in self.check.rows]

    def test_lists_findings_again_and_copies_them(self):
        self.assertIn(system_check.FAIL, self.levels())
        self.assertIn("kate (wayland)", self.check.apps.get_text())
        fixed = dict(
            self.NO_IM, GTK_IM_MODULE="fcitx", QT_IM_MODULE="fcitx", XMODIFIERS="@im=fcitx"
        )
        old = self.check.rows
        with mock.patch.dict(os.environ, fixed, clear=True):
            self.check.run()
        self.assertEqual(self.levels(), [system_check.OK])
        self.assertEqual([row.get_parent() for row in old], [None] * len(old))
        with mock.patch.dict(os.environ, self.NO_IM, clear=True):
            self.check.run()
        self.check.copy_btn.emit("clicked")
        self.assertIn("[X] GTK apps have no input method", self.clipboard())
        self.assertEqual(self.check.status.get_text(), "Copied the result.")

    def test_messaging_copies_the_result_and_opens_the_chat(self):
        Gdk.Display.get_default().get_clipboard().set("")
        with mock.patch.object(check_dialog, "open_uri") as open_uri:
            self.check.message_btn.emit("clicked")
        self.assertEqual(open_uri.call_args[0][1], MESSENGER_URL)
        self.assertIn("Ngó Sen 1.4.2", self.clipboard())
        self.assertIn("Paste", self.check.status.get_text())

    def test_reporting_opens_an_issue_with_the_result(self):
        with mock.patch.object(check_dialog, "open_uri") as open_uri:
            self.check.report_btn.emit("clicked")
        self.assertIn("1.4.2", unquote(open_uri.call_args[0][1]))

    def test_user_answers_when_the_app_hides_its_text(self):
        test = typing_log.AppTest(path="log", program="edge", frontend="xim")
        with mock.patch.object(typing_log, "read_latest", return_value=test):
            self.check.read_typing_log()
        self.assertTrue(self.check.wrong_btn.get_visible())
        self.assertIn("edge (xim)", self.check.app_result.get_text())
        self.check.wrong_btn.emit("clicked")
        self.assertFalse(self.check.wrong_btn.get_visible())
        self.assertIn("result: wrong", self.check.report_text())

    def test_the_about_page_opens_one_dialog_and_checks_again(self):
        win = window.Window(None, FakeDBusHandler())
        self.addCleanup(win.destroy)
        with mock.patch.object(Adw.Dialog, "present"):
            win.open_system_check()
            first = win.check
            with mock.patch.object(first, "run") as run:
                win.open_system_check()
        self.assertIs(win.check, first)
        run.assert_called_once()


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkSearchTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.window = window.Window(None, FakeDBusHandler())
        self.addCleanup(self.window.destroy)

    def search(self, text):
        self.window.search_entry.set_text(text)
        self.window._on_search(self.window.search_entry)
        return [r for r in _children(self.window.results) if hasattr(r, "found")]

    def test_a_found_setting_opens_on_its_page_with_the_focus(self):
        results = self.search("spell check")
        self.assertIn(self.window.typing.rows["SpellCheck"], [r.found[0] for r in results])
        self.window.show_page("about")
        self.window.search_entry.emit("activate")
        self.assertEqual(self.window.stack.get_visible_child_name(), "typing")
        self.assertIs(self.window.get_focus(), results[0].found[0])
        self.assertEqual(self.window.side_stack.get_visible_child_name(), "pages")

    def test_enter_right_after_typing_opens_what_was_typed(self):
        self.search("about")
        self.window.search_entry.set_text("spell check")
        self.window.search_entry.emit("activate")
        self.assertIs(self.window.get_focus(), self.window.typing.rows["SpellCheck"])

    def test_find_shows_the_sidebar_of_a_narrow_window(self):
        self.window.split.set_collapsed(True)
        self.window.show_page("about")
        self.window.focus_search()
        self.assertFalse(self.window.split.get_show_content())
        self.assertIs(
            self.window.get_focus().get_ancestor(Gtk.SearchEntry), self.window.search_entry
        )

    def test_a_setting_in_a_tab_opens_that_tab(self):
        (result,) = self.search("custom keymap")
        self.window.open_result(result)
        self.assertEqual(self.window.stack.get_visible_child_name(), "keys")
        self.assertEqual(self.window.tabs["keymap"].get_visible_child_name(), "keymap")

    def test_the_users_own_entries_and_whats_new_are_not_settings(self):
        for page in (self.window.macros, self.window.keymap):
            page.list.set_filter_func(None)
            page.list.append(Adw.ActionRow(title="zzqq"))
        self.assertEqual(self.search("zzqq"), [])
        self.assertEqual(self.search("Cứu chữ"), [])

    def test_nothing_found_says_so(self):
        self.assertEqual(self.search("no such setting"), [])
        self.assertEqual(self.window.side_stack.get_visible_child_name(), "results")
        self.assertIsNotNone(self.window.results.get_row_at_index(0))
        self.window.search_entry.emit("activate")
        self.assertEqual(self.window.stack.get_visible_child_name(), "typing")
        self.assertEqual(self.search(""), [])
        self.assertEqual(self.window.side_stack.get_visible_child_name(), "pages")


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkWhatsNewPageTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.window = window.Window(None, FakeDBusHandler())
        self.addCleanup(self.window.destroy)

    def test_every_button_leads_somewhere_in_this_window(self):
        w = self.window
        buttons = [b for b in _children(w.whats_new.page) if hasattr(b, "target")]
        items = [*whats_new_content.FEATURES, *whats_new_content.TIPS]
        self.assertEqual(len(buttons), sum(item["target"] is not None for item in items))
        for button in buttons:
            kind, name = button.target
            w.show_page("whats-new")
            w.set_focus(None)
            button.emit("clicked")
            if kind == "setting":
                controls = [p.control_for(name) for p in (w.typing, w.shortcuts, w.appearance)]
                self.assertIn(w.get_focus(), [c for c in controls if c is not None], name)
            else:
                self.assertEqual(w.stack.get_visible_child_name(), name)

    def assert_scrolled_to(self, open_it):
        """open_it(window) focuses a row low on a page never shown; the page must scroll to it."""
        w = window.Window(None, FakeDBusHandler())
        self.addCleanup(w.destroy)
        w.set_default_size(900, 500)
        w.present()
        row = open_it(w)
        adjustment = row.get_ancestor(Gtk.ScrolledWindow).get_vadjustment()
        context = GLib.MainContext.default()
        for _ in range(300):
            if adjustment.get_value() > 0:
                break
            context.iteration(False)
            time.sleep(0.01)
        if w.get_frame_clock().get_frame_counter() == 0 and not REQUIRED:
            self.skipTest("the window was never drawn, as on a locked desktop")
        self.assertGreater(adjustment.get_value(), 0)
        self.assertIs(w.get_focus(), row)

    def test_a_row_low_on_a_page_never_shown_is_scrolled_to(self):
        def open_setting(w):
            w.open_setting("SettingsToolkit")
            return w.appearance.control_for("SettingsToolkit")

        self.assert_scrolled_to(open_setting)

    def test_a_search_result_on_a_page_never_shown_is_scrolled_to(self):
        def open_found(w):
            result = Gtk.ListBoxRow()
            result.found = (w.appearance.control_for("SettingsToolkit"), "appearance")
            w.open_result(result)
            return result.found[0]

        self.assert_scrolled_to(open_found)

    def test_the_tips_are_scrolled_to_on_a_fresh_window(self):
        def open_tips(w):
            w.show_tips()
            return w.whats_new.tip_rows[0]

        self.assert_scrolled_to(open_tips)

    def test_open_setting_shows_the_tab_and_focuses_the_row(self):
        self.assertTrue(self.window.open_setting("ModeMenuKey"))
        self.assertEqual(self.window.stack.get_visible_child_name(), "keys")
        self.assertEqual(self.window.tabs["shortcuts"].get_visible_child_name(), "shortcuts")
        self.assertIs(self.window.get_focus(), self.window.shortcuts.hotkeys["ModeMenuKey"])

    def test_the_tip_on_the_typing_page_opens_the_tips(self):
        self.window.typing.tip_row.emit("activated")
        self.assertEqual(self.window.stack.get_visible_child_name(), "whats-new")

    def test_the_backup_tip_points_at_the_menu(self):
        backup = whats_new_content.TIPS[-1]
        with mock.patch.object(window, "random_tip", return_value=backup):
            other = window.Window(None, FakeDBusHandler())
        self.addCleanup(other.destroy)
        shown = [other.typing.tip_row.get_title(), other.whats_new.tip_rows[-1].get_title()]
        for title in shown:
            self.assertIn("menu", title)
            self.assertNotIn("below", title)

    def test_without_fcitx5_open_setting_does_nothing(self):
        other = window.Window(None, NoFcitx())
        self.addCleanup(other.destroy)
        self.assertFalse(other.open_setting("RetypeAfterSwitch"))


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class HotkeyButtonTest(unittest.TestCase):
    def setUp(self):
        self.button = HotkeyButton("F12")
        self.recorded = []
        self.button.connect("recorded", lambda _b, key: self.recorded.append(key))
        self.button.set_active(True)

    def press(self, keyval, state=0):
        self.assertTrue(self.button._on_press(None, keyval, 0, Gdk.ModifierType(state)))

    def release(self, keyval):
        self.button._on_release(None, keyval, 0, Gdk.ModifierType(0))

    def test_ctrl_shift_pressed_and_let_go_is_recorded(self):
        self.press(Gdk.KEY_Control_L)
        self.press(Gdk.KEY_Shift_L, Gdk.ModifierType.CONTROL_MASK)
        self.release(Gdk.KEY_Shift_L)
        self.assertEqual(self.recorded, ["Control+Shift_L"])
        self.assertFalse(self.button.get_active())

    def test_a_key_with_modifiers_is_recorded_as_fcitx5_names_it(self):
        self.press(Gdk.KEY_Shift_L)
        self.press(Gdk.KEY_ISO_Left_Tab, Gdk.ModifierType.SHIFT_MASK)
        self.assertEqual(self.recorded, ["Shift+Tab"])

    def test_a_mode_key_shows_as_the_qt_window_shows_it(self):
        button = HotkeyButton("plus", single=True)
        labels = [w.get_label() for w in _children(button) if isinstance(w, Gtk.Label)]
        self.assertEqual(labels, ["+"])

    def test_escape_cancels_and_keeps_the_key(self):
        self.press(Gdk.KEY_Escape)
        self.assertEqual(self.recorded, [])
        self.assertFalse(self.button.get_active())
        self.assertEqual(self.button.key, "F12")


def _children(widget):
    child = widget.get_first_child()
    while child is not None:
        yield child
        yield from _children(child)
        child = child.get_next_sibling()


if __name__ == "__main__":
    unittest.main()
