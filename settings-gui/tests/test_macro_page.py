# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import re
import unittest

# isort: off
from support import FakeDBusHandler, app
from qtpy.QtCore import QPoint, QPointF, Qt
from qtpy.QtGui import QWheelEvent
from qtpy.QtWidgets import (
    QAbstractButton,
    QComboBox,
    QLabel,
    QLineEdit,
    QScrollArea,
    QStyleFactory,
)
from ui.main_window import NgoSenSettingsWindow
from ui.pages.macro_editor import MacroEditorPage

from core import macros

# isort: on


class WordsTest(unittest.TestCase):
    def setUp(self):
        self.page = MacroEditorPage(FakeDBusHandler())

    def test_the_formats_show_what_they_produce(self):
        for combo, key in (
            (self.page.input_time_format, "TimeFormat"),
            (self.page.input_date_format, "DateFormat"),
        ):
            shown = [combo.itemText(i) for i in range(combo.count())]
            self.assertEqual(shown, [macros.EXAMPLES[f] for f in macros.FORMATS[key]])
            self.assertFalse([text for text in shown if "%" in text])

    def test_each_format_saves_its_own_value(self):
        fake = FakeDBusHandler()
        page = MacroEditorPage(fake)
        for name, key in (("input_time_format", "TimeFormat"), ("input_date_format", "DateFormat")):
            combo = getattr(page, name)
            for value in macros.FORMATS[key]:
                combo.setCurrentIndex(combo.findData(value))
                self.assertTrue(page.save_data())
                self.assertEqual(fake.values[key], value)
                self.assertEqual(getattr(MacroEditorPage(fake), name).currentData(), value)

    def test_the_editor_labels_match_the_table(self):
        texts = [label.text() for label in self.page.findChildren(QLabel)]
        self.assertIn("Abbreviation:", texts)
        self.assertIn("Expanded Text:", texts)
        self.assertNotIn("Key:", texts)

    def test_the_editor_labels_lead_to_their_boxes(self):
        buddies = {label.text(): label.buddy() for label in self.page.findChildren(QLabel)}
        self.assertIs(buddies["Abbreviation:"], self.page.input_key)
        self.assertIs(buddies["Expanded Text:"], self.page.input_val)

    def test_the_hints_dim_with_their_switch(self):
        hints = [
            label
            for label in self.page.findChildren(QLabel)
            if label.text().startswith(("kg gives", "Abbreviations also", "Macros support"))
        ]
        self.assertEqual(len(hints), 3)
        self.page.cb_enable.setChecked(False)
        self.assertEqual([hint.isEnabled() for hint in hints], [False] * 3)
        self.page.cb_enable.setChecked(True)
        self.assertEqual([hint.isEnabled() for hint in hints], [True] * 3)

    def test_new_texts_have_a_vietnamese_text(self):
        po = os.path.join(os.path.dirname(__file__), "..", "..", "po", "vi.po")
        with open(po, encoding="utf-8") as f:
            text = f.read()
        for msgid in (
            "kg gives khô gà, Kg gives Khô gà, KG gives KHÔ GÀ.",
            "Abbreviations also expand while the mode is Off.",
            "Abbreviation:",
            "Expanded Text:",
        ):
            self.assertRegex(text, f'msgid "{re.escape(msgid)}"\\nmsgstr "[^"]+"')


def wheel_down(widget):
    center = QPointF(widget.rect().center())
    event = QWheelEvent(
        center,
        QPointF(widget.mapToGlobal(widget.rect().center())),
        QPoint(0, 0),
        QPoint(0, -120),
        Qt.NoButton,
        Qt.NoModifier,
        Qt.NoScrollPhase,
        False,
    )
    app.sendEvent(widget, event)
    return event


class WheelTest(unittest.TestCase):
    def test_the_wheel_changes_no_combo_box_in_the_window(self):
        window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.addCleanup(window.close)
        combos = [c for c in window.findChildren(QComboBox) if c.count() > 1 and c.isEnabled()]
        self.assertGreater(len(combos), 3)
        for combo in combos:
            combo.setCurrentIndex(0)
            wheel_down(combo)
            self.assertEqual(combo.currentIndex(), 0, combo)

    def test_a_format_box_leaves_the_wheel_to_the_page(self):
        page = MacroEditorPage(FakeDBusHandler())
        self.assertFalse(wheel_down(page.input_time_format).isAccepted())


class ScrollTest(unittest.TestCase):
    def test_the_format_boxes_take_a_row_each(self):
        page = MacroEditorPage(FakeDBusHandler())
        page.resize(550, 400)
        page.show()
        self.addCleanup(page.close)
        app.processEvents()
        self.assertGreater(page.input_date_format.y(), page.input_time_format.y())

    def test_every_control_can_be_reached_at_the_smallest_window(self):
        self.assert_every_control_can_be_reached()

    @unittest.skipUnless("Breeze" in QStyleFactory.keys(), "Breeze style is not installed")
    def test_every_control_can_be_reached_on_the_wider_kde_style(self):
        previous_font = app.font()
        previous_style = app.style().objectName()
        self.addCleanup(app.setFont, previous_font)
        self.addCleanup(app.setStyle, previous_style)
        app.setStyle("Breeze")
        font = app.font()
        font.setPointSize(10)
        app.setFont(font)
        self.assert_every_control_can_be_reached()

    def assert_every_control_can_be_reached(self):
        page = MacroEditorPage(FakeDBusHandler())
        # The window's minimum size less the sidebar and the bottom bar.
        page.resize(550, 400)
        page.show()
        self.addCleanup(page.close)
        app.processEvents()
        scroll = page.findChild(QScrollArea)
        self.assertGreater(scroll.widget().height(), scroll.viewport().height())
        self.assertFalse(scroll.horizontalScrollBar().isVisible())
        for kind in (QAbstractButton, QComboBox, QLineEdit):
            for widget in page.findChildren(kind):
                if widget.isVisibleTo(page):
                    scroll.ensureWidgetVisible(widget)
                    app.processEvents()
                    self.assertFalse(widget.visibleRegion().isEmpty(), widget)


if __name__ == "__main__":
    unittest.main()
