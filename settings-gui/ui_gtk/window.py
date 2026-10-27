# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The GTK window: pages in a sidebar, as in GNOME Settings."""

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
gi.require_version("PangoCairo", "1.0")

from core.catalog import MODE_SHORTCUT_TO_VISIBILITY  # noqa: E402
from core.cli import parse_args  # noqa: E402
from core.dbus_handler import NgoSenDBusHandler  # noqa: E402
from core.dictionary import Dictionary  # noqa: E402
from core.keymap import Keymap  # noqa: E402
from core.macros import MacroTable  # noqa: E402
from core.settings_model import SettingsModel  # noqa: E402
from core.whats_new_content import random_tip  # noqa: E402
from gi.repository import Adw, Gio, Gtk  # noqa: E402
from i18n import _  # noqa: E402

from ui_gtk import brand, pages  # noqa: E402
from ui_gtk.appearance import AppearancePage  # noqa: E402
from ui_gtk.dictionary import DictionaryPage  # noqa: E402
from ui_gtk.keymap import KeymapPage  # noqa: E402
from ui_gtk.macros import MacroPage  # noqa: E402
from ui_gtk.shortcuts import ShortcutsPage  # noqa: E402
from ui_gtk.whats_new import WhatsNewPage  # noqa: E402

APP_ID = "io.github.ngosen.NgoSen.Settings"
MODE_KEYS = [*MODE_SHORTCUT_TO_VISIBILITY, *MODE_SHORTCUT_TO_VISIBILITY.values()]

try:
    from version import __version__
except ImportError:
    __version__ = "dev version"


class Window(Adw.ApplicationWindow):
    def __init__(self, app, dbus=None):
        super().__init__(
            application=app, title=_("Ngó Sen Settings"), default_width=900, default_height=680
        )
        dbus = dbus or NgoSenDBusHandler()
        self.model = SettingsModel(dbus)
        self.toast = None
        self.toasts = Adw.ToastOverlay()
        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.CROSSFADE)
        self.toasts.set_child(self.stack)
        self.pages = []

        self.typing = self.macros = self.dictionary = self.keymap = self.shortcuts = None
        self.appearance = None
        self.tabs = {}
        self.tab_parents = {}
        if self.model.load():
            self.typing = pages.TypingPage(
                self.model, self.save, tip=random_tip(), on_tip=self.show_tips
            )
            self.appearance = AppearancePage(self.model, self.save)
            others = [n for n in (dbus.input_method_names() or []) if n != "ngosen"]
            self.shortcuts = ShortcutsPage(self.model, self.save, bool(others))
            table = MacroTable(dbus)
            table.load()
            self.macros = MacroPage(table, self.report)
            self.dictionary = DictionaryPage(Dictionary(dbus), self.report)
            keys = Keymap(dbus)
            keys.load()
            self.keymap = KeymapPage(keys, self.report)
        self._add_page("typing", _("Typing"), "input-keyboard-symbolic", _widget(self.typing))
        self._add_page(
            "macros",
            _("Macros & Dictionary"),
            "accessories-text-editor-symbolic",
            self._tabs("macros", macros=self.macros, dictionary=self.dictionary),
        )
        self._add_page(
            "keys",
            _("Keys"),
            "preferences-desktop-keyboard-symbolic",
            self._tabs("keys", shortcuts=self.shortcuts, keymap=self.keymap),
        )
        self._add_page(
            "appearance",
            _("Appearance"),
            "preferences-desktop-appearance-symbolic",
            _widget(self.appearance),
        )
        self.whats_new = WhatsNewPage(self.open_setting, self.show_page)
        self._add_page(
            "whats-new",
            _("What's new & tips"),
            "dialog-information-symbolic",
            self.whats_new.page,
        )
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

    def _tabs(self, parent, **pages):
        """Pages that share a sidebar entry, as tabs like the Qt window's."""
        if None in pages.values():
            return _widget(None)
        stack = Adw.ViewStack()
        for name, page in pages.items():
            stack.add_titled_with_icon(
                page.page, name, page.page.get_title(), page.page.get_icon_name()
            )
            self.tabs[name] = stack
            self.tab_parents[name] = parent
        switcher = Adw.ViewSwitcher(stack=stack, policy=Adw.ViewSwitcherPolicy.WIDE)
        switcher.set_halign(Gtk.Align.CENTER)
        switcher.set_margin_top(6)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        box.append(switcher)
        stack.set_vexpand(True)
        box.append(stack)
        return box

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
        """Shows the named page or tab; a page this window does not have yet is ignored."""
        if name in self.tabs:
            self.tabs[name].set_visible_child_name(name)
            name = self.tab_parents[name]
        for index, (page, _title, _icon) in enumerate(self.pages):
            if page == name:
                row = self.sidebar.get_row_at_index(index)
                self.sidebar.select_row(row)
                self._on_row(self.sidebar, row)

    def open_setting(self, key):
        """Shows and focuses the row of a setting; False for a key no page shows."""
        for name, page in (
            ("typing", self.typing),
            ("shortcuts", self.shortcuts),
            ("appearance", self.appearance),
        ):
            control = page.control_for(key) if page else None
            if control is not None:
                self.show_page(name)
                control.grab_focus()
                return True
        return False

    def show_tips(self):
        self.show_page("whats-new")
        self.whats_new.show_tips()

    def save(self, key, value):
        self.model.set(key, value)
        conflicts = self.model.shortcut_conflicts()
        # Mode keys wait while two modes share one, so swapping two keys never saves the halfway
        # state; every other change still saves.
        held = {
            k: self.model.modified.pop(k)
            for k in MODE_KEYS
            if conflicts and k in self.model.modified
        }
        ok = self.model.save()
        self.model.modified.update(held)
        if key in held:
            self._show(Adw.Toast(title=conflicts[0][0], timeout=0))
        else:
            self.report(ok, self.model.label(key) or key)

    def report(self, ok, label):
        if ok:
            self._show(Adw.Toast(title=_("Saved “{}”.").format(label)))
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


def _widget(page):
    """The page's widget, or a notice when fcitx5 could not be reached to build it."""
    if page is not None:
        return page.page
    return Adw.StatusPage(
        icon_name="dialog-error-symbolic",
        title=_("fcitx5 is not running"),
        description=_("Cannot reach fcitx5, so changes will not be saved."),
    )


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
