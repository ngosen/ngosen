# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest

from qtpy.QtCore import Qt
from qtpy.QtTest import QTest
from qtpy.QtWidgets import QComboBox
from support import FakeDBusHandler, app
from ui.main_window import NgoSenSettingsWindow


class KeyboardTest(unittest.TestCase):
    def setUp(self):
        self.window = NgoSenSettingsWindow(dbus_handler=FakeDBusHandler())
        self.window.show()
        self.window.activateWindow()
        app.processEvents()
        self.addCleanup(self.window.close)

    def focus_chain(self):
        chain = [self.window.search_field]
        widget = chain[0].nextInFocusChain()
        while widget is not chain[0]:
            chain.append(widget)
            widget = widget.nextInFocusChain()
        return chain

    def test_tab_reaches_the_page_before_the_bottom_bar(self):
        for row in (1, 0):
            self.window.sidebar.setCurrentRow(row)
            app.processEvents()
        chain = self.focus_chain()
        combo = next(w for w in chain if isinstance(w, QComboBox) and w.isVisible())
        self.assertLess(chain.index(self.window.sidebar), chain.index(combo))
        self.assertLess(chain.index(combo), chain.index(self.window.btn_reset))

    def test_ctrl_f_goes_to_search(self):
        self.window.sidebar.setFocus()
        QTest.keyClick(self.window.sidebar, Qt.Key_F, Qt.ControlModifier)
        app.processEvents()
        self.assertIs(app.focusWidget(), self.window.search_field)


if __name__ == "__main__":
    unittest.main()
