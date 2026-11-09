# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

# isort: off
from support import FakeDBusHandler, app
from core.catalog import DEPENDS, locked_by
from core.settings_model import SettingsModel
from qtpy.QtCore import Qt
from qtpy.QtTest import QTest
from qtpy.QtWidgets import QCheckBox
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory
from ui.pages.macro_editor import MacroEditorPage

# isort: on

ALL_LOCKED = {
    controller: "False" if needed == "True" else "True" for controller, needed in DEPENDS.values()
}


class LockedByTest(unittest.TestCase):
    def test_a_setting_without_a_dependency_is_never_locked(self):
        self.assertIsNone(locked_by("W2U", lambda _key: "False"))

    def test_locked_only_while_the_controller_holds_another_value(self):
        self.assertEqual(
            locked_by("SpellCheck", {"AutoNonVnRestore": "False"}.get), "AutoNonVnRestore"
        )
        self.assertIsNone(locked_by("SpellCheck", {"AutoNonVnRestore": "True"}.get))
        self.assertEqual(locked_by("IconTheme", {"UseLotusIcons": "True"}.get), "UseLotusIcons")
        self.assertIsNone(locked_by("IconTheme", {"UseLotusIcons": "False"}.get))

    def test_the_macro_table_reads_its_switches_as_booleans(self):
        self.assertEqual(locked_by("TimeFormat", {"EnableMacro": False}.get), "EnableMacro")
        self.assertIsNone(locked_by("TimeFormat", {"EnableMacro": True}.get))

    def test_a_controller_fcitx5_does_not_report_locks_nothing(self):
        self.assertIsNone(locked_by("SpellCheck", {}.get))

    def test_every_key_is_a_setting_fcitx5_reports(self):
        model = SettingsModel(FakeDBusHandler())
        model.load()
        for key, (controller, _needed) in DEPENDS.items():
            self.assertIn(key, model.metadata)
            self.assertIn(controller, model.metadata)

    def test_each_setting_is_one_step_from_its_controller(self):
        for controller, _needed in DEPENDS.values():
            self.assertNotIn(controller, DEPENDS)


class QtWindowLocksTest(unittest.TestCase):
    def window(self, values):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler(values))
        window.show()
        app.processEvents()
        self.addCleanup(window.close)
        window._build_all_pages()
        return window

    @staticmethod
    def page_with(window, key):
        return next(
            p for p in window._pages if getattr(p, "control_for", lambda _k: None)(key) is not None
        )

    def test_every_locked_setting_is_disabled_and_opens_what_unlocks_it(self):
        window = self.window(ALL_LOCKED)
        for key, (controller, _needed) in DEPENDS.items():
            page = self.page_with(window, key)
            self.assertFalse(page.control_for(key).isEnabled(), key)
            self.assertTrue(window.open_setting(key), key)
            self.assertIs(window.focusWidget(), page.control_for(controller), key)

    def test_every_setting_is_enabled_and_opens_itself_when_unlocked(self):
        values = {c: needed for c, needed in DEPENDS.values()}
        window = self.window(values)
        for key in DEPENDS:
            page = self.page_with(window, key)
            self.assertTrue(page.control_for(key).isEnabled(), key)
            self.assertTrue(window.open_setting(key), key)
            self.assertIs(window.focusWidget(), page.control_for(key), key)

    def test_a_search_result_for_a_locked_setting_opens_what_unlocks_it(self):
        window = self.window(ALL_LOCKED)
        box = window.search_results
        for key, (controller, _needed) in DEPENDS.items():
            page = self.page_with(window, key)
            window.search_field.setText(self.shown_name(page, key))
            found = [
                box.item(r) for r in range(box.count()) if box.item(r).data(Qt.UserRole)[2] == key
            ]
            self.assertTrue(found, key)
            window.open_search_result(found[0])
            self.assertIs(window.focusWidget(), page.control_for(controller), key)

    @staticmethod
    def shown_name(page, key):
        control = page.control_for(key)
        if isinstance(control, QCheckBox):
            return control.text()
        if isinstance(page, MacroEditorPage):
            label = page.time_label if key == "TimeFormat" else page.date_label
        else:
            label = page.shortcut_labels.get(key) or page.form.labelForField(page._fields[key])
        return label.text().rstrip(":")

    def test_a_search_result_whose_setting_is_gone_opens_its_label(self):
        window = self.window(ALL_LOCKED)
        window.search_field.setText("time format")
        item = window.search_results.item(0)
        self.page_with(window, "TimeFormat").controls.clear()
        window.open_search_result(item)
        self.assertEqual(window.search_field.text(), "")

    def test_a_form_label_finds_the_setting_inside_its_wrapper(self):
        window = self.window(ALL_LOCKED)
        page = self.page_with(window, "IconTheme")
        label = page.form.labelForField(page._fields["IconTheme"])
        window.search_field.setText(label.text().rstrip(":"))
        window.open_search_result(window.search_results.item(0))
        self.assertIs(window.focusWidget(), page.control_for("UseLotusIcons"))


class QtPageLocksTest(unittest.TestCase):
    def test_a_controller_locks_and_unlocks_at_once(self):
        fake = FakeDBusHandler({"AutoNonVnRestore": "True"})
        page = DynamicSettingsPage(fake, category=SettingsCategory.TYPING)
        controller, spell = page.control_for("AutoNonVnRestore"), page.control_for("SpellCheck")
        self.assertTrue(spell.isEnabled())
        controller.setChecked(False)
        self.assertFalse(spell.isEnabled())
        controller.setChecked(True)
        self.assertTrue(spell.isEnabled())

    def test_a_locked_setting_keeps_its_value(self):
        fake = FakeDBusHandler({"AutoNonVnRestore": "True", "SpellCheck": "True"})
        page = DynamicSettingsPage(fake, category=SettingsCategory.TYPING)
        page.control_for("AutoNonVnRestore").setChecked(False)
        self.assertTrue(page.control_for("SpellCheck").isChecked())
        self.assertNotIn("SpellCheck", page.modified_values)

    def test_locks_follow_a_reload_and_the_defaults(self):
        fake = FakeDBusHandler({"AutoNonVnRestore": "False"})
        page = DynamicSettingsPage(fake, category=SettingsCategory.TYPING)
        self.assertFalse(page.control_for("SpellCheck").isEnabled())
        page.restore_defaults()
        self.assertTrue(page.control_for("SpellCheck").isEnabled())
        page.load_data()
        self.assertFalse(page.control_for("SpellCheck").isEnabled())

    def test_a_mode_key_and_its_label_lock_with_the_mode(self):
        fake = FakeDBusHandler({"ShowModeEmoji": "True"})
        page = DynamicSettingsPage(fake, category=SettingsCategory.SHORTCUTS)
        page.control_for("ShowModeEmoji").setChecked(False)
        self.assertFalse(page.control_for("ShortcutEmoji").isEnabled())
        self.assertFalse(page.shortcut_labels["ShortcutEmoji"].isEnabled())
        self.assertTrue(page.control_for("ShowModeEmoji").isEnabled())

    def test_hiding_a_mode_ends_its_key_conflict(self):
        fake = FakeDBusHandler({"ShortcutPreedit": "w", "ShortcutEmoji": "w"})
        page = DynamicSettingsPage(fake, category=SettingsCategory.SHORTCUTS)
        self.assertTrue(page.has_validation_errors())
        page.control_for("ShowModeEmoji").setChecked(False)
        self.assertFalse(page.has_validation_errors())

    def test_tab_skips_a_locked_setting(self):
        fake = FakeDBusHandler({"AutoNonVnRestore": "False"})
        page = DynamicSettingsPage(fake, category=SettingsCategory.TYPING)
        page.resize(750, 500)
        page.show()
        self.addCleanup(page.close)
        app.processEvents()
        controller = page.control_for("AutoNonVnRestore")
        controller.setFocus(Qt.OtherFocusReason)
        QTest.keyClick(controller, Qt.Key_Tab)
        self.assertIs(page.focusWidget(), page.control_for("RetypeAfterSwitch"))

    def test_the_macro_switch_locks_what_depends_on_it(self):
        page = MacroEditorPage(FakeDBusHandler({"EnableMacro": "True"}))
        locked = (
            page.cb_capitalize,
            page.cb_enable_off_mode,
            page.input_time_format,
            page.input_date_format,
            page.time_label,
            page.date_label,
        )
        self.assertTrue(all(w.isEnabled() for w in locked))
        page.cb_enable.setChecked(False)
        self.assertFalse(any(w.isEnabled() for w in locked))
        self.assertTrue(page.cb_enable.isEnabled())
        self.assertTrue(page.input_key.isEnabled())
        page.cb_enable.setChecked(True)
        self.assertTrue(all(w.isEnabled() for w in locked))

    def test_the_macro_locks_follow_a_reload_and_the_defaults(self):
        fake = FakeDBusHandler({"EnableMacro": "False"})
        page = MacroEditorPage(fake)
        self.assertFalse(page.input_time_format.isEnabled())
        page.restore_defaults()
        self.assertTrue(page.input_time_format.isEnabled())
        page.load_data()
        self.assertFalse(page.input_time_format.isEnabled())

    def test_the_formats_sit_right_under_the_macro_switches(self):
        page = MacroEditorPage(FakeDBusHandler())
        page.resize(750, 900)
        page.show()
        self.addCleanup(page.close)
        app.processEvents()
        self.assertLess(
            page.input_time_format.mapToGlobal(page.rect().topLeft()).y(),
            page.table.mapToGlobal(page.rect().topLeft()).y(),
        )


if __name__ == "__main__":
    unittest.main()
