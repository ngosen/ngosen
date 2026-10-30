# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
from unittest import mock

# isort: off
from support import DEFAULTS, FakeDBusHandler, app
from core import whats_new_content
from core.whats_new_content import FEATURES, TIPS
from qtpy.QtWidgets import QCheckBox, QLabel, QPushButton
from ui.main_window import NgoSenSettingsWindow
from ui.pages.dynamic_settings import TipLine
from ui.pages.whats_new import PAGE_TITLES, WhatsNewPage

# isort: on


def targets():
    return [entry["target"] for entry in FEATURES + TIPS if entry["target"]]


class ContentTest(unittest.TestCase):
    def test_every_target_exists(self):
        for kind, name in targets():
            if kind == "setting":
                self.assertIn(name, DEFAULTS)
            else:
                self.assertIn(name, PAGE_TITLES)

    def test_newest_release_comes_first(self):
        versions = [tuple(int(n) for n in f["version"].split(".")) for f in FEATURES]
        self.assertEqual(versions, sorted(versions, reverse=True))


class PageTest(unittest.TestCase):
    def setUp(self):
        self.window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.window.resize(900, 700)
        self.window.show()
        app.processEvents()
        self.addCleanup(self.window.close)

    def test_sits_above_about(self):
        titles = [self.window.sidebar.item(r).text() for r in range(self.window.sidebar.count())]
        self.assertEqual(titles[-3:], ["What's new & tips", "Check my machine", "About"])

    def test_page_option_opens_it(self):
        page = self.window.show_page("whats-new")
        self.assertIsInstance(page, WhatsNewPage)
        self.assertEqual(self.window.sidebar.currentItem().text(), "What's new & tips")

    def test_a_switched_window_can_open_on_appearance(self):
        self.window.show_page("appearance")
        self.assertEqual(self.window.sidebar.currentItem().text(), "Appearance")

    def test_every_setting_target_has_a_control(self):
        for kind, name in targets():
            if kind == "setting":
                self.assertTrue(self.window.open_setting(name), name)
        self.assertFalse(self.window.open_setting("NoSuchSetting"))

    def test_open_setting_focuses_its_control(self):
        self.window.open_setting("RetoneSelection")
        self.assertEqual(self.window.sidebar.currentItem().text(), "Typing")
        focused = app.focusWidget()
        self.assertIsInstance(focused, QCheckBox)
        self.assertEqual(focused.text(), "Change the tone of a selected word")

    def test_card_button_opens_its_setting(self):
        page = self.window.show_page("whats-new")
        button = next(
            b
            for b in page.findChildren(QPushButton)
            if b.property("target") == ("setting", "RetoneSelection")
        )
        button.click()
        self.assertEqual(self.window.sidebar.currentItem().text(), "Typing")
        self.assertIsInstance(app.focusWidget(), QCheckBox)

    def test_gtk_card_links_to_pictures_of_the_gtk_window(self):
        page = self.window.show_page("whats-new")
        link = next(b for b in page.findChildren(QPushButton) if b.property("url"))
        self.assertEqual(link.text(), "See the GTK look")
        with mock.patch("ui.pages.whats_new.QDesktopServices.openUrl") as open_url:
            link.click()
        self.assertEqual(
            open_url.call_args.args[0].toString(),
            "https://ngosen.github.io/huong-dan/cua-so-cai-dat/",
        )

    def test_tip_button_opens_its_page(self):
        page = self.window.show_page("whats-new")
        button = next(
            b for b in page.findChildren(QPushButton) if b.property("target") == ("page", "macros")
        )
        button.click()
        self.assertEqual(self.window.sidebar.currentItem().text(), "Macros & Dictionary")
        tabs = self.window.content_stack.currentWidget()
        self.assertEqual(tabs.tabText(tabs.currentIndex()), "Macros")

    def test_show_page_selects_a_second_tab(self):
        page = self.window.show_page("dictionary")
        tabs = self.window.content_stack.currentWidget()
        self.assertEqual(tabs.tabText(tabs.currentIndex()), "Dictionary")
        self.assertIs(tabs.currentWidget(), page)

    def test_search_skips_the_page(self):
        self.window.search_field.setText("Cứu dấu")
        box = self.window.search_results
        results = [box.item(r).text() for r in range(box.count())]
        self.assertFalse(any("What's new" in r for r in results), results)

    def test_text_fits_a_narrow_window(self):
        page = self.window.show_page("whats-new")
        self.window.resize(600, 500)
        app.processEvents()
        for label in page.findChildren(QLabel):
            if label.wordWrap():
                self.assertGreaterEqual(label.width(), label.minimumSizeHint().width())


class TipLineTest(unittest.TestCase):
    def test_typing_page_shows_the_picked_tip(self):
        with mock.patch.object(whats_new_content.random, "choice", side_effect=lambda t: t[3]):
            window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.addCleanup(window.close)
        window.resize(900, 700)
        window.show()
        app.processEvents()
        line = window.findChild(TipLine)
        self.assertTrue(line.toolTip().startswith("Tip: ] types ư"))
        line.click()
        self.assertEqual(window.sidebar.currentItem().text(), "What's new & tips")

    def test_a_long_tip_is_cut_to_the_line(self):
        line = TipLine("word " * 80)
        line.resize(300, 30)
        line.show()
        app.processEvents()
        self.addCleanup(line.close)
        self.assertTrue(line.text().endswith("…"))
        self.assertLess(line.fontMetrics().horizontalAdvance(line.text()), 300)


if __name__ == "__main__":
    unittest.main()
