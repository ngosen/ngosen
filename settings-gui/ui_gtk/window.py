# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The GTK window: pages in a sidebar, as in GNOME Settings."""

import argparse

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
gi.require_version("PangoCairo", "1.0")

from core.dbus_handler import NgoSenDBusHandler  # noqa: E402
from core.settings_model import SettingsModel  # noqa: E402
from gi.repository import Adw, Gio, Gtk  # noqa: E402
from i18n import _  # noqa: E402

from ui_gtk import brand, pages  # noqa: E402

APP_ID = "io.github.ngosen.NgoSen.Settings"

try:
    from version import __version__
except ImportError:
    __version__ = "dev version"


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="ngosen-settings")
    parser.add_argument("--page", choices=["whats-new"])
    args, _unknown = parser.parse_known_args(argv)
    return args


class Window(Adw.ApplicationWindow):
    def __init__(self, app, dbus=None):
        super().__init__(
            application=app, title=_("Ngó Sen Settings"), default_width=900, default_height=680
        )
        self.model = SettingsModel(dbus or NgoSenDBusHandler())
        self.toast = None
        self.toasts = Adw.ToastOverlay()
        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.CROSSFADE)
        self.toasts.set_child(self.stack)
        self.pages = []

        if self.model.load():
            self.typing = pages.TypingPage(self.model, self.save)
            typing = self.typing.page
        else:
            self.typing = None
            typing = Adw.StatusPage(
                icon_name="dialog-error-symbolic",
                title=_("fcitx5 is not running"),
                description=_("Cannot reach fcitx5, so changes will not be saved."),
            )
        self._add_page("typing", _("Typing"), "input-keyboard-symbolic", typing)
        self._add_page("about", _("About"), "help-about-symbolic", pages.about_page(__version__))

        self.split = Adw.NavigationSplitView(min_sidebar_width=220)
        self.split.set_sidebar(Adw.NavigationPage(title="Ngó Sen", child=self._sidebar()))
        content = Adw.ToolbarView(content=self.toasts)
        content.add_top_bar(Adw.HeaderBar())
        self.content = Adw.NavigationPage(title=self.pages[0][1], child=content)
        self.split.set_content(self.content)
        self.set_content(self.split)

        narrow = Adw.Breakpoint.new(Adw.BreakpointCondition.parse("max-width: 600sp"))
        narrow.add_setter(self.split, "collapsed", True)
        self.add_breakpoint(narrow)

    def _add_page(self, name, title, icon, widget):
        self.stack.add_named(widget, name)
        self.pages.append((name, title, icon))

    def _sidebar(self):
        header = Adw.HeaderBar(show_title=False)
        mark = Gtk.Box(spacing=8)
        mark.append(Gtk.Image(icon_name=brand.LOGO, pixel_size=28))
        mark.append(Gtk.Label(label=brand.WORDMARK, css_classes=["wordmark"]))
        header.pack_start(mark)
        self.sidebar = Gtk.ListBox(css_classes=["navigation-sidebar"])
        for _name, title, icon in self.pages:
            row = Gtk.Box(spacing=12)
            row.append(Gtk.Image(icon_name=icon))
            row.append(Gtk.Label(label=title, xalign=0))
            self.sidebar.append(row)
        self.sidebar.connect("row-activated", self._on_row)
        self.sidebar.select_row(self.sidebar.get_row_at_index(0))
        view = Adw.ToolbarView(content=Gtk.ScrolledWindow(child=self.sidebar))
        view.add_top_bar(header)
        return view

    def _on_row(self, _listbox, row):
        name, title, _icon = self.pages[row.get_index()]
        self.stack.set_visible_child_name(name)
        self.content.set_title(title)
        self.split.set_show_content(True)

    def show_page(self, name):
        """Shows the named page; a page this window does not have yet is ignored."""
        for index, (page, _title, _icon) in enumerate(self.pages):
            if page == name:
                row = self.sidebar.get_row_at_index(index)
                self.sidebar.select_row(row)
                self._on_row(self.sidebar, row)

    def save(self, key, value):
        self.model.set(key, value)
        if self.model.save():
            self._show(Adw.Toast(title=_("Saved “{}”.").format(self.model.label(key) or key)))
        else:
            self._show(
                Adw.Toast(title=_("Could not save. Check that fcitx5 is running."), timeout=0)
            )

    def _show(self, toast):
        if self.toast is not None:
            self.toast.dismiss()
        self.toast = toast
        toast.set_use_markup(False)
        self.toasts.add_toast(toast)


class App(Adw.Application):
    def __init__(self, dbus=None):
        super().__init__(application_id=APP_ID, flags=Gio.ApplicationFlags.HANDLES_COMMAND_LINE)
        self.dbus = dbus

    def do_startup(self):
        Adw.Application.do_startup(self)
        brand.load_font()
        brand.install()

    def do_command_line(self, command_line):
        # A second launch, such as the What's new notification, lands here in the running window.
        args = parse_args(command_line.get_arguments()[1:])
        window = self.get_active_window() or Window(self, self.dbus)
        if args.page:
            window.show_page(args.page)
        window.present()
        return 0


def run(argv):
    return App().run(argv)
