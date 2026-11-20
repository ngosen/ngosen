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

if CAN_RUN:
    from gi.repository import Adw, Gtk
    from test_gtk_emoji import _children, _rows, _settle
    from test_gtk_window import isolate
    from ui_gtk import window


@unittest.skipUnless(CAN_RUN, "needs GTK 4, libadwaita 1.5 and a display")
class GtkTeencodeAppsTest(unittest.TestCase):
    def setUp(self):
        Adw.init()
        isolate(self)
        self.dbus = FakeDBusHandler(sub_configs={"teencode_apps": [{"App": "discord"}]})
        self.window = window.Window(None, self.dbus)
        self.addCleanup(self.window.destroy)
        self.window.present()
        _settle()
        self.page = self.window.typing

    def titles(self):
        return [r.get_title() for r in _rows(self.page.teencode_apps)]

    def test_turn_off_from_the_list(self):
        self.assertEqual(self.titles(), ["discord"])
        row = next(_rows(self.page.teencode_apps))
        button = next(c for c in _children(row) if isinstance(c, Gtk.Button))
        button.emit("clicked")
        self.assertEqual(self.dbus.sub_configs["teencode_apps"], [])
        self.assertEqual(self.titles(), [window._("Not on in any app.")])

    def test_apps_turned_on_from_the_keyboard_show_up(self):
        self.dbus.sub_configs["teencode_apps"].append({"App": "zalo"})
        self.page.refresh_status()
        self.assertEqual(self.titles(), ["discord", "zalo"])


if __name__ == "__main__":
    unittest.main()
