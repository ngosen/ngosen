# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

# isort: off
from support import FakeDBusHandler, app
from qtpy.QtCore import QPoint, QRect, Qt
from qtpy.QtGui import QColor, QPalette
from qtpy.QtTest import QTest
from qtpy.QtWidgets import (
    QAbstractButton,
    QAbstractItemView,
    QApplication,
    QCheckBox,
    QComboBox,
    QLineEdit,
    QScrollArea,
    QStyleFactory,
    QTabWidget,
)
from ui import brand
from ui.main_window import NgoSenSettingsWindow

# isort: on


def on_screen(widget):
    return QRect(widget.mapToGlobal(QPoint(0, 0)), widget.size())


def controls_of(page):
    found = []
    for kind in (QAbstractButton, QComboBox, QLineEdit, QAbstractItemView):
        found += [widget for widget in page.findChildren(kind) if widget.isVisible()]
    return found


class WindowCase(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)
        self.window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.addCleanup(self.window.close)
        self.window.resize(750, 500)
        self.window.show()
        self.window.activateWindow()
        QTest.qWaitForWindowActive(self.window)
        app.processEvents()

    def every_page(self):
        window = self.window
        for row in range(window.sidebar.count()):
            window.sidebar.setCurrentRow(row)
            app.processEvents()
            current = window.content_stack.currentWidget()
            if isinstance(current, QTabWidget):
                for tab in range(current.count()):
                    current.setCurrentIndex(tab)
                    app.processEvents()
                    yield current.widget(tab)
            else:
                yield current


class StaysPutTest(WindowCase):
    def test_showing_a_message_moves_nothing(self):
        for page in self.every_page():
            # A page settles a moment after it first opens, with or without a message.
            app.processEvents()
            self.window.message_bar.hide()
            app.processEvents()
            before = [on_screen(widget) for widget in controls_of(page)]
            self.window._show_message("Saved.", undo=True)
            app.processEvents()
            self.assertEqual([on_screen(widget) for widget in controls_of(page)], before, page)


class PlaceTest(WindowCase):
    def test_it_sits_at_the_bottom_of_the_pages_through_a_resize(self):
        self.window._show_message("Saved.", undo=True)
        for size in ((750, 500), (1100, 800), (750, 500)):
            self.window.resize(*size)
            app.processEvents()
            stack = on_screen(self.window.content_stack)
            bar = on_screen(self.window.message_bar)
            self.assertTrue(stack.contains(bar), size)
            self.assertLess(stack.bottom() - bar.bottom(), 20, size)
            self.assertLess(self.window.message_bar.height(), 60, size)

    def test_a_long_saved_text_stays_on_one_line_with_all_of_it_in_the_tooltip(self):
        self.window._show_message("Saved.")
        app.processEvents()
        height = self.window.message_bar.height()
        text = "Backed up to /home/someone/" + "a-long-folder/" * 20 + "backup.json."
        self.window._show_message(text)
        app.processEvents()
        self.assertEqual(self.window.message_bar.height(), height)
        self.assertEqual(self.window.message_label.toolTip(), text)
        self.assertEqual(self.window.message_label.text(), text)
        self.assertLessEqual(
            on_screen(self.window.message_label).right(), on_screen(self.window.message_bar).right()
        )

    def test_a_long_error_wraps_and_may_grow(self):
        self.window._show_message("Saved.")
        app.processEvents()
        height = self.window.message_bar.height()
        self.window._show_message("One line\nanother\nand another", error=True)
        app.processEvents()
        self.assertGreater(self.window.message_bar.height(), height)


class CoverTest(WindowCase):
    def assert_no_control_under_the_bar(self):
        for page in self.every_page():
            scroll = page.findChild(QScrollArea)
            self.assertIsInstance(scroll, QScrollArea, page)
            scroll.verticalScrollBar().setValue(scroll.verticalScrollBar().maximum())
            self.window._show_message("Saved.", undo=True)
            app.processEvents()
            bar = on_screen(self.window.message_bar)
            for widget in controls_of(page):
                self.assertFalse(on_screen(widget).intersects(bar), (page, widget))

    def test_at_the_end_of_every_page_no_control_is_under_the_bar(self):
        self.assert_no_control_under_the_bar()

    @unittest.skipUnless("Breeze" in QStyleFactory.keys(), "Breeze style is not installed")
    def test_at_the_end_of_every_page_no_control_is_under_the_bar_on_breeze(self):
        font, style = app.font(), app.style().objectName()
        self.addCleanup(app.setFont, font)
        self.addCleanup(app.setStyle, style)
        app.setStyle("Breeze")
        bigger = app.font()
        bigger.setPointSize(10)
        app.setFont(bigger)
        # The room a page keeps is measured when it is built.
        self.window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.addCleanup(self.window.close)
        self.window.resize(750, 500)
        self.window.show()
        app.processEvents()
        self.assert_no_control_under_the_bar()

    def test_a_control_at_the_edge_scrolls_clear_of_the_bar(self):
        self.window.activateWindow()
        QTest.qWaitForWindowActive(self.window)
        page = next(self.every_page())
        scroll = page.findChild(QScrollArea)
        viewport = on_screen(scroll.viewport())
        under = [w for w in controls_of(page) if on_screen(w).bottom() > viewport.bottom()]
        target = next(w for w in under if w.focusPolicy() & Qt.TabFocus)
        scroll.ensureWidgetVisible(target, 0, 0)
        target.setFocus()
        app.processEvents()
        self.assertIs(QApplication.focusWidget(), target)
        self.window._show_message("Saved.", undo=True)
        app.processEvents()
        self.assertFalse(on_screen(target).intersects(on_screen(self.window.message_bar)))
        self.assertTrue(on_screen(scroll.viewport()).contains(on_screen(target)))


class ClickTest(WindowCase):
    def test_a_click_on_a_control_near_the_bar_lands(self):
        page = next(self.every_page())
        self.window._show_message("Saved.", undo=True)
        app.processEvents()
        scroll = page.findChild(QScrollArea)
        box = next(b for b in page.findChildren(QCheckBox) if b.isVisible())
        bar_top = on_screen(self.window.message_bar).top()
        # Half the box above the bar, so giving it focus would scroll the page.
        overlap = on_screen(box).bottom() - bar_top - box.height() // 2
        scroll.verticalScrollBar().setValue(scroll.verticalScrollBar().value() + overlap)
        app.processEvents()
        self.assertTrue(on_screen(box).intersects(on_screen(self.window.message_bar)))
        self.window.sidebar.setFocus()
        before = box.isChecked()
        # The pointer stays still: if the page scrolls, the release lands elsewhere on the box.
        pointer = box.mapToGlobal(QPoint(8, 3))
        QTest.mousePress(box, Qt.LeftButton, Qt.NoModifier, QPoint(8, 3))
        app.processEvents()
        QTest.mouseRelease(box, Qt.LeftButton, Qt.NoModifier, box.mapFromGlobal(pointer))
        self.assertNotEqual(box.isChecked(), before)


class LifeTest(WindowCase):
    def expire(self):
        self.window.message_bar.timer.timeout.emit()
        app.processEvents()

    def test_a_saved_message_goes_after_a_while(self):
        self.window._show_message("Saved.", undo=True)
        self.assertTrue(self.window.message_bar.timer.isActive())
        self.assertEqual(self.window.message_bar.timer.interval(), 5000)
        self.expire()
        self.assertTrue(self.window.message_bar.isHidden())

    def test_it_stays_while_the_pointer_is_on_it(self):
        self.window._show_message("Saved.", undo=True)
        self.window.message_bar.setAttribute(Qt.WA_UnderMouse, True)
        self.expire()
        self.assertFalse(self.window.message_bar.isHidden())
        self.assertTrue(self.window.message_bar.timer.isActive())
        self.window.message_bar.setAttribute(Qt.WA_UnderMouse, False)
        self.expire()
        self.assertTrue(self.window.message_bar.isHidden())

    def test_it_stays_while_focus_is_inside(self):
        self.window.activateWindow()
        QTest.qWaitForWindowActive(self.window)
        self.window._show_message("Saved.", undo=True)
        self.window.btn_undo.setFocus()
        app.processEvents()
        self.assertIs(QApplication.focusWidget(), self.window.btn_undo)
        self.expire()
        self.assertFalse(self.window.message_bar.isHidden())

    def test_focus_left_on_close_after_undo_does_not_hold_it(self):
        self.window._show_message("Saved.", undo=True)
        self.window.message_bar.close_button.setFocus()
        app.processEvents()
        self.assertIs(QApplication.focusWidget(), self.window.message_bar.close_button)
        self.expire()
        self.assertTrue(self.window.message_bar.isHidden())

    def test_it_stays_while_the_window_is_in_the_background(self):
        self.window._show_message("Paste it into the message.")
        with mock.patch.object(self.window.message_bar, "isActiveWindow", return_value=False):
            self.expire()
        self.assertFalse(self.window.message_bar.isHidden())

    def test_a_fix_with_undo_stays_and_shows_all_its_text(self):
        self.window.show_check_message("Fixed one. Fixed another.", lambda: True)
        self.assertFalse(self.window.message_bar.timer.isActive())
        self.assertTrue(self.window.message_label.wordWrap())
        self.window.show_check_message("Paste it.", None)
        self.assertTrue(self.window.message_bar.timer.isActive())

    def test_colours_follow_the_window_palette(self):
        self.window._show_message("Saved.")
        palette = QPalette(app.palette())
        self.addCleanup(app.setPalette, QPalette(app.palette()))
        palette.setColor(QPalette.Window, QColor("#222222"))
        palette.setColor(QPalette.WindowText, QColor("#eeeeee"))
        app.setPalette(palette)
        app.processEvents()
        self.window._show_message("Saved.")
        fill = brand.pick(self.window, brand.SAVED_FILL)
        self.assertEqual(fill, brand.SAVED_FILL[1])
        self.assertIn(fill, self.window.message_bar.styleSheet())

    def test_errors_and_replace_all_messages_stay_until_closed(self):
        for kwargs in ({"error": True}, {"keep": True}):
            self.window._show_message("Message.", **kwargs)
            self.assertFalse(self.window.message_bar.timer.isActive(), kwargs)
            self.window.message_bar.close_button.click()
            self.assertTrue(self.window.message_bar.isHidden(), kwargs)

    def test_defaults_stay_until_closed(self):
        self.window._reset_pending = True
        with mock.patch.object(self.window, "_modified_pages", return_value=[]):
            self.window.save_pending()
        self.window._reset_pending = True
        page = mock.Mock()
        page.save_data.return_value = True
        with mock.patch.object(self.window, "_modified_pages", return_value=[page]):
            self.window.save_pending()
        self.assertEqual(self.window.message_label.text(), "Defaults restored.")
        self.assertFalse(self.window.message_bar.timer.isActive())

    def test_a_conflict_message_goes_once_the_conflict_is_gone(self):
        window = self.window
        with (
            mock.patch.object(window, "has_validation_errors", return_value=True),
            mock.patch.object(window, "validation_message", return_value="Same key twice."),
        ):
            window.on_changed()
        self.assertFalse(window.message_bar.isHidden())
        self.assertFalse(window.message_bar.timer.isActive())
        window.on_changed()
        self.assertTrue(window.message_bar.isHidden())

    def conflict(self):
        window = self.window
        return (
            mock.patch.object(window, "has_validation_errors", return_value=True),
            mock.patch.object(window, "validation_message", return_value="Same key twice."),
        )

    def test_a_conflict_cannot_be_closed(self):
        errors, message = self.conflict()
        with errors, message:
            self.window.on_changed()
        self.assertTrue(self.window.message_bar.close_button.isHidden())
        self.window._show_message("Saved.")
        self.assertFalse(self.window.message_bar.close_button.isHidden())

    def test_a_conflict_comes_back_after_another_message(self):
        errors, message = self.conflict()
        with errors, message:
            self.window.on_changed()
            self.window._show_message("Backed up.")
            self.expire()
        self.assertEqual(self.window.message_label.text(), "Same key twice.")
        self.assertFalse(self.window.message_bar.isHidden())

    def test_a_huge_error_stays_inside_the_pages(self):
        self.window._show_message("\n".join(["A line"] * 60), error=True)
        app.processEvents()
        stack = on_screen(self.window.content_stack)
        self.assertTrue(stack.contains(on_screen(self.window.message_bar)))

    def test_another_error_stays_after_a_later_change(self):
        self.window._show_message("Could not save.", error=True)
        self.window.on_changed()
        self.assertFalse(self.window.message_bar.isHidden())


class KeyboardTest(WindowCase):
    def setUp(self):
        super().setUp()
        self.window._build_all_pages()
        self.window.activateWindow()
        QTest.qWaitForWindowActive(self.window)
        self.window._show_message("Saved.", undo=True)
        app.processEvents()

    def test_tab_reaches_undo_with_every_page_built(self):
        self.window.sidebar.setFocus()
        for _step in range(400):
            QTest.keyClick(QApplication.focusWidget(), Qt.Key_Tab)
            if QApplication.focusWidget() is self.window.btn_undo:
                return
        self.fail("Tab never reached Undo")

    def test_the_undo_mnemonic_works(self):
        clicks = []
        self.window.btn_undo.clicked.connect(lambda: clicks.append(True))
        QTest.keyClick(self.window, Qt.Key_U, Qt.AltModifier)
        # A mnemonic presses the button for a moment before it clicks.
        QTest.qWait(300)
        self.assertEqual(clicks, [True])


if __name__ == "__main__":
    unittest.main()
