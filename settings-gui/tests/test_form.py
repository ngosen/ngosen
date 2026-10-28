# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import time
import unittest
from unittest import mock

from qtpy.QtCore import QEvent
from qtpy.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFormLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QMessageBox,
)
from support import FakeDBusHandler, app
from ui import toolkit_switch
from ui.components import SingleKeyCaptureWidget
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dynamic_settings import (
    HINTS,
    MENU_ENTRIES,
    SETTINGS_MAP,
    DynamicSettingsPage,
    SettingsCategory,
)


def form_of(page):
    app.sendPostedEvents(None, QEvent.DeferredDelete)
    return page.findChild(QFormLayout)


def row_labels(form):
    labels = []
    for row in range(form.rowCount()):
        item = form.itemAt(row, QFormLayout.LabelRole)
        if item and isinstance(item.widget(), QLabel) and item.widget().text():
            labels.append(item.widget().text())
    return labels


class TypingPageTest(unittest.TestCase):
    def setUp(self):
        self.page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.TYPING)

    def test_common_settings_come_first(self):
        self.assertEqual(
            row_labels(form_of(self.page))[:3], ["Mode:", "Input Method:", "Output Charset:"]
        )

    def test_options_are_grouped(self):
        labels = row_labels(form_of(self.page))
        self.assertIn("Tone marks:", labels)
        self.assertIn("Quick typing:", labels)

    def test_hint_sits_under_its_setting(self):
        cb = next(
            c for c in self.page.findChildren(QCheckBox) if c.text() == "Auto Restore Invalid Words"
        )
        hints = [w.text() for w in cb.parentWidget().findChildren(QLabel)]
        self.assertIn("Typing windows still gives windows.", hints)

    def test_lock_key_sits_under_tone_marks(self):
        labels = row_labels(form_of(self.page))
        self.assertLess(labels.index("Tone marks:"), labels.index("Key to Keep the Word as Typed:"))

    def test_hints_take_one_line(self):
        self.page.resize(760, 900)
        self.page.show()
        app.processEvents()
        hints = [
            label
            for label in self.page.findChildren(QLabel)
            if label.text()
            in (
                "Typing windows still gives windows.",
                "Words that are not Vietnamese get no tone marks.",
            )
        ]
        self.assertEqual(len(hints), 2)
        for hint in hints:
            self.assertLess(hint.height(), 2 * hint.fontMetrics().lineSpacing(), hint.text())

    def test_long_hint_wraps_instead_of_scrolling(self):
        # Narrow enough to wrap whether the labels sit beside or above the fields.
        self.page.resize(600, 900)
        self.page.show()
        app.processEvents()
        hint = next(
            label
            for label in self.page.findChildren(QLabel)
            if label.text().startswith("z in Telex (0 in VNI)")
        )
        self.assertGreaterEqual(hint.height(), 2 * hint.fontMetrics().lineSpacing())
        # The page may still scroll for other reasons; the hint must not be what forces it.
        one_line = hint.fontMetrics().boundingRect(hint.text()).width()
        self.assertLess(hint.minimumSizeHint().width(), one_line)

    def test_has_a_field_to_try_typing(self):
        form = form_of(self.page)
        last = form.itemAt(form.rowCount() - 1, QFormLayout.FieldRole).widget()
        self.assertIsInstance(last, QLineEdit)

    def test_labels_go_above_fields_in_a_narrow_window(self):
        self.page.resize(500, 600)
        self.page.show()
        app.processEvents()
        self.assertEqual(form_of(self.page).rowWrapPolicy(), QFormLayout.WrapAllRows)
        self.page.resize(1400, 600)
        app.processEvents()
        self.assertEqual(form_of(self.page).rowWrapPolicy(), QFormLayout.DontWrapRows)

    def test_labels_go_above_fields_when_larger_text_does_not_fit(self):
        font = self.page.font()
        font.setPointSizeF(font.pointSizeF() * 1.4)
        self.page.setFont(font)
        self.page.resize(760, 600)
        self.page.show()
        app.processEvents()
        self.assertEqual(form_of(self.page).rowWrapPolicy(), QFormLayout.WrapAllRows)


class ShortcutsPageTest(unittest.TestCase):
    def test_mode_keys_fit_in_the_list(self):
        page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.SHORTCUTS)
        page.resize(800, 600)
        page.show()
        for _ in range(3):
            app.processEvents()
        view = page.findChild(QListWidget).viewport()
        for button in page.findChildren(SingleKeyCaptureWidget):
            self.assertLessEqual(button.mapTo(view, button.rect().bottomRight()).x(), view.width())


def wait_until(condition, seconds=5):
    """Runs the event loop until condition() holds, for work handed back from a thread."""
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            raise AssertionError("timed out")
        app.processEvents()
        time.sleep(0.01)


class AppearancePageTest(unittest.TestCase):
    def setUp(self):
        self.fake = FakeDBusHandler({"ShowMenuCharset": "True"})
        self.page = DynamicSettingsPage(self.fake, category=SettingsCategory.APPEARANCE)

    def menu_checkbox(self, text):
        return next(c for c in self.page.findChildren(QCheckBox) if c.text() == text)

    def test_switching_reopens_in_gtk_when_it_is_installed(self):
        button = self.page.control_for("SettingsToolkit")
        self.assertEqual(button.text(), "Switch to GTK…")
        offer = mock.Mock(ready=True, target="gtk")
        with (
            mock.patch("ui.toolkit_switch.Offer", return_value=offer),
            mock.patch("ui.toolkit_switch.switch", return_value=True) as switch,
            mock.patch.object(self.page, "close") as close,
        ):
            button.click()
            self.assertFalse(button.isEnabled())
            wait_until(lambda: close.called)
        switch.assert_called_once_with(self.page.model, "gtk")
        self.assertTrue(button.isEnabled())

    def test_a_window_that_still_cannot_load_after_installing_is_reported(self):
        offer = mock.Mock(install_argv=["true"], command="sudo true")
        offer.name = "GTK"
        offer.installed_now.return_value = False
        button = self.page.control_for("SettingsToolkit")
        with (
            mock.patch("ui.toolkit_switch.switch") as switch,
            mock.patch("ui.toolkit_switch._failed") as failed,
        ):
            toolkit_switch._install(button, self.page.model, offer)
            wait_until(lambda: failed.called)
        switch.assert_not_called()

    def test_a_refused_password_prompt_changes_nothing(self):
        offer = mock.Mock(install_argv=["sh", "-c", "exit 126"])
        offer.name = "GTK"
        button = self.page.control_for("SettingsToolkit")
        with mock.patch("ui.toolkit_switch._failed") as failed:
            toolkit_switch._install(button, self.page.model, offer)
            wait_until(lambda: button.isEnabled())
        failed.assert_not_called()
        offer.installed_now.assert_not_called()

    def test_a_missing_window_lists_its_packages_first(self):
        offer = mock.Mock(ready=False, installable=True, runnable=True, packages=("a", "b"))
        offer.name = "GTK"
        offer.download_size.return_value = None
        with (
            mock.patch("ui.toolkit_switch.Offer", return_value=offer),
            mock.patch("ui.toolkit_switch.switch") as switch,
        ):
            self.page.control_for("SettingsToolkit").click()
            wait_until(lambda: self.page.findChild(QMessageBox))
        box = self.page.findChild(QMessageBox)
        self.assertIn("a, b", box.informativeText())
        self.assertIn("Install", [b.text() for b in box.buttons()])
        switch.assert_not_called()
        box.close()

    def test_menu_entries_follow_the_tray_icon(self):
        labels = row_labels(form_of(self.page))
        self.assertLess(labels.index("Tray icon:"), labels.index("Show in the tray menu:"))

    def test_checkboxes_name_the_menu_entries(self):
        texts = [c.text() for c in self.page.findChildren(QCheckBox)]
        for name in ("Input Method", "Charset", "Custom Dictionary"):
            self.assertIn(name, texts)
        self.assertNotIn("Show Charset in the tray menu", texts)

    def test_tray_preview_follows_the_icon_settings(self):
        preview = self.page.findChild(QLabel, "TrayIconPreview")
        self.assertIsNotNone(preview)
        self.assertEqual(self.page._tray_icon_names()[0], ("fcitx-ngosen", "fcitx-ngosen-off"))
        self.assertFalse(preview.pixmap().isNull())
        drawn = preview.pixmap().cacheKey()
        self.page.update_config("UseLotusIcons", "False")
        self.assertNotEqual(preview.pixmap().cacheKey(), drawn)
        self.page.update_config("IconTheme", "Light")
        self.assertEqual(
            self.page._tray_icon_names(),
            (("fcitx-ngosen-default-black", "fcitx-ngosen-off-default-black"), False),
        )
        self.page.update_config("IconTheme", "Dark")
        self.assertEqual(
            self.page._tray_icon_names(),
            (("fcitx-ngosen-default", "fcitx-ngosen-off-default"), True),
        )

    def test_language_comes_first_and_names_itself(self):
        form = form_of(self.page)
        self.assertEqual(row_labels(form)[0], "Interface Language:")
        field = form.itemAt(0, QFormLayout.FieldRole).widget()
        combo = field if isinstance(field, QComboBox) else field.findChild(QComboBox)
        self.assertEqual(
            [combo.itemText(i) for i in range(combo.count())], ["Tiếng Việt", "English"]
        )
        self.assertEqual(combo.currentData(), "Vietnamese")
        combo.setCurrentIndex(1)
        self.assertEqual(self.page.modified_values["UiLanguage"], "English")

    def test_unticking_hides_the_entry(self):
        self.menu_checkbox("Charset").click()
        self.assertEqual(self.page.modified_values["ShowMenuCharset"], "False")
        self.assertEqual(self.page.change_label(), "Show Charset in the tray menu")


class SidebarTest(unittest.TestCase):
    def test_general_is_merged_into_typing(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        titles = [window.sidebar.item(r).text() for r in range(window.sidebar.count())]
        self.assertNotIn("General", titles)
        self.assertEqual(titles[0], "Typing")


def vietnamese_catalog():
    """msgid → msgstr from po/vi.po, joining continued lines."""
    path = os.path.join(os.path.dirname(__file__), "..", "..", "po", "vi.po")
    entries, field = {}, None
    msgid = msgstr = ""
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("msgid "):
                if msgid:
                    entries[msgid] = msgstr
                field, msgid, msgstr = "id", line[7:-1], ""
            elif line.startswith("msgstr "):
                field, msgstr = "str", line[8:-1]
            elif line.startswith('"') and field == "id":
                msgid += line[1:-1]
            elif line.startswith('"') and field == "str":
                msgstr += line[1:-1]
    if msgid:
        entries[msgid] = msgstr
    return {k.replace('\\"', '"'): v for k, v in entries.items()}


class HintTest(unittest.TestCase):
    # The input method shows a typing example, the mode keys share one line under their list, and
    # the menu entries are named by their checkboxes.
    NO_HINT = {"InputMethod", *MENU_ENTRIES}

    def test_every_option_explains_itself(self):
        for category in (
            SettingsCategory.TYPING,
            SettingsCategory.SHORTCUTS,
            SettingsCategory.APPEARANCE,
        ):
            page = DynamicSettingsPage(FakeDBusHandler(), category=category)
            shown = {label.text() for label in page.findChildren(QLabel)}
            for keys in SETTINGS_MAP[category].values():
                for key in keys:
                    if key in self.NO_HINT or key.startswith("Shortcut"):
                        continue
                    with self.subTest(key=key):
                        self.assertIn(HINTS.get(key), shown)

    def test_mode_keys_say_when_they_work(self):
        page = DynamicSettingsPage(FakeDBusHandler(), category=SettingsCategory.SHORTCUTS)
        shown = {label.text() for label in page.findChildren(QLabel)}
        self.assertIn(HINTS["ModeOrder"], shown)

    def test_every_hint_has_a_vietnamese_line(self):
        catalog = vietnamese_catalog()
        for key, text in HINTS.items():
            with self.subTest(key=key):
                self.assertTrue(catalog.get(text), text)


if __name__ == "__main__":
    unittest.main()
