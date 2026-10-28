# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The GTK window: pages in a sidebar, as in GNOME Settings."""

import os
from datetime import datetime

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
gi.require_version("PangoCairo", "1.0")

from core.brand import APP_ID  # noqa: E402
from core.catalog import MODE_SHORTCUT_TO_VISIBILITY  # noqa: E402
from core.cli import parse_args  # noqa: E402
from core.dbus_handler import NgoSenDBusHandler  # noqa: E402
from core.dictionary import Dictionary  # noqa: E402
from core.keymap import Keymap  # noqa: E402
from core.macros import MacroTable  # noqa: E402
from core.settings_model import SettingsModel  # noqa: E402
from core.whats_new_content import random_tip  # noqa: E402
from gi.repository import Adw, Gio, GLib, Gtk  # noqa: E402
from i18n import _  # noqa: E402

from core import backup, defaults, settings_snapshot  # noqa: E402
from ui_gtk import brand, pages, search  # noqa: E402
from ui_gtk.appearance import AppearancePage  # noqa: E402
from ui_gtk.check_dialog import CheckDialog  # noqa: E402
from ui_gtk.dictionary import DictionaryPage  # noqa: E402
from ui_gtk.focus import focus_when_laid_out  # noqa: E402
from ui_gtk.keymap import KeymapPage  # noqa: E402
from ui_gtk.macros import MacroPage  # noqa: E402
from ui_gtk.shortcuts import ShortcutsPage  # noqa: E402
from ui_gtk.whats_new import WhatsNewPage  # noqa: E402

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
        self.dbus = dbus or NgoSenDBusHandler()
        self.toast = None
        self.toasts = Adw.ToastOverlay()
        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.CROSSFADE)
        self.toasts.set_child(self.stack)
        self.pages = []
        self.search_pages = []
        self.page_titles = {}
        self._undo_to = None

        for name, title, icon, widget, searchable in self._data_pages():
            self._add_page(name, title, icon, widget, searchable)
        self.whats_new = WhatsNewPage(self.open_setting, self.show_page)
        self._add_page(
            "whats-new",
            _("What's new & tips"),
            "dialog-information-symbolic",
            self.whats_new.page,
            searchable=False,
        )
        self.check = None
        self._add_page(
            "about",
            _("About"),
            "help-about-symbolic",
            pages.about_page(__version__, self.open_system_check),
        )

        self.split = Adw.NavigationSplitView(min_sidebar_width=220)
        self.split.set_sidebar(Adw.NavigationPage(title="Ngó Sen", child=self._sidebar()))
        content = Adw.ToolbarView(content=self.toasts)
        content.add_top_bar(self._header())
        self.content = Adw.NavigationPage(title=self.pages[0][1], child=content)
        self.split.set_content(self.content)
        self.set_content(self.split)

        narrow = Adw.Breakpoint.new(Adw.BreakpointCondition.parse("max-width: 600sp"))
        narrow.add_setter(self.split, "collapsed", True)
        self.add_breakpoint(narrow)

    def _data_pages(self):
        """Builds the pages that show saved settings, as (name, title, icon, widget, searchable)."""
        self.model = SettingsModel(self.dbus)
        self.typing = self.macros = self.dictionary = self.keymap = self.shortcuts = None
        self.appearance = None
        self.tabs = {}
        self.tab_parents = {}
        if self.model.load():
            self.typing = pages.TypingPage(
                self.model, self.save, tip=random_tip(), on_tip=self.show_tips
            )
            self.appearance = AppearancePage(self.model, self.save)
            others = [n for n in (self.dbus.input_method_names() or []) if n != "ngosen"]
            self.shortcuts = ShortcutsPage(self.model, self.save, bool(others))
            table = MacroTable(self.dbus)
            table.load()
            self.macros = MacroPage(table, self.save_with_undo)
            self.dictionary = DictionaryPage(Dictionary(self.dbus), self.save_with_undo)
            keys = Keymap(self.dbus)
            keys.load()
            self.keymap = KeymapPage(keys, self.save_with_undo)
        return [
            ("typing", _("Typing"), "input-keyboard-symbolic", _widget(self.typing), True),
            (
                "macros",
                _("Macros & Dictionary"),
                "accessories-text-editor-symbolic",
                self._tabs("macros", macros=self.macros, dictionary=self.dictionary),
                False,
            ),
            (
                "keys",
                _("Keys"),
                "preferences-desktop-keyboard-symbolic",
                self._tabs("keys", shortcuts=self.shortcuts, keymap=self.keymap),
                False,
            ),
            (
                "appearance",
                _("Appearance"),
                "preferences-desktop-appearance-symbolic",
                _widget(self.appearance),
                True,
            ),
        ]

    def reload(self):
        """Builds the settings pages again from what fcitx5 holds, keeping the page and tab shown."""
        shown = self.stack.get_visible_child_name()
        tabs = [name for name, stack in self.tabs.items() if stack.get_visible_child_name() == name]
        static = [entry for entry in self.search_pages if entry[0] in ("whats-new", "about")]
        self.search_pages = []
        for name, title, _icon, widget, searchable in self._data_pages():
            self.stack.remove(self.stack.get_child_by_name(name))
            self.stack.add_named(widget, name)
            if searchable:
                self.search_pages.append((name, widget))
                self.page_titles[name] = title
        self.search_pages += static
        for name in tabs:
            if name in self.tabs:
                self.tabs[name].set_visible_child_name(name)
        self.stack.set_visible_child_name(shown)

    def undo(self):
        """Puts back the settings from before the last save."""
        if self._undo_to is None:
            return
        if not settings_snapshot.restore(self.dbus, self._undo_to):
            self._show(
                Adw.Toast(title=_("Could not undo. Check that fcitx5 is running."), timeout=0)
            )
            return
        self._undo_to = None
        self.reload()
        self._show(Adw.Toast(title=_("Change undone.")))

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
            self.search_pages.append((name, page.page))
            self.page_titles[name] = page.page.get_title()
        switcher = Adw.ViewSwitcher(stack=stack, policy=Adw.ViewSwitcherPolicy.WIDE)
        switcher.set_halign(Gtk.Align.CENTER)
        switcher.set_margin_top(6)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        box.append(switcher)
        stack.set_vexpand(True)
        box.append(stack)
        return box

    def _add_page(self, name, title, icon, widget, searchable=True):
        self.stack.add_named(widget, name)
        self.pages.append((name, title, icon))
        if searchable:
            self.search_pages.append((name, widget))
            self.page_titles[name] = title

    def _header(self):
        menu = Gio.Menu()
        for label, name, run in (
            (_("Back &Up…"), "backup", self.choose_backup_file),
            (_("Res&tore…"), "restore-backup", self.choose_restore_file),
            (_("Restore defaults…"), "restore-defaults", self.confirm_defaults),
        ):
            menu.append(label.replace("&", "_"), f"win.{name}")
            action = Gio.SimpleAction.new(name, None)
            action.connect("activate", lambda *_args, run=run: run())
            self.add_action(action)
        header = Adw.HeaderBar()
        header.pack_end(
            Gtk.MenuButton(icon_name="open-menu-symbolic", menu_model=menu, tooltip_text=_("Menu"))
        )
        return header

    def confirm_defaults(self):
        dialog = Adw.AlertDialog(
            heading=_("Restore all settings to defaults?"),
            body=_(
                "Macros, words added to the dictionary, custom keys and the modes remembered"
                " for each app are cleared too. You can undo this right after."
            ),
        )
        dialog.add_response("cancel", _("&Cancel").replace("&", "_"))
        dialog.add_response("reset", _("&Defaults").replace("&", "_"))
        dialog.set_response_appearance("reset", Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.set_default_response("cancel")
        dialog.set_close_response("cancel")
        dialog.connect(
            "response", lambda _d, response: response == "reset" and self.restore_defaults()
        )
        dialog.present(self)
        return dialog

    def restore_defaults(self):
        self._replace_all(
            lambda: defaults.restore(self.dbus),
            _("Defaults restored."),
            _("Could not restore every default. Check that fcitx5 is running."),
        )

    def _replace_all(self, write, done, failed):
        """Runs a write that changes every page, with Undo back to what fcitx5 held just before."""
        # Taken now, since fcitx5 remembers app modes while the window is open.
        before = settings_snapshot.take(self.dbus)
        if before is None:
            self._show(Adw.Toast(title=failed, timeout=0))
            return
        ok = write()
        # Even a partial write changed fcitx5, so the pages show what it holds and undo stays.
        self.reload()
        self._undo_to = before
        toast = Adw.Toast(title=done) if ok else Adw.Toast(title=failed, timeout=0)
        self._show(self._with_undo(toast))

    def choose_backup_file(self):
        dialog = self._file_dialog(_("Export Backup"))
        dialog.set_initial_name(f"ngosen-backup-{datetime.now():%Y%m%d-%H%M%S}.json")
        dialog.save(self, None, self._on_file, dialog.save_finish, self.export_backup)

    def choose_restore_file(self):
        dialog = self._file_dialog(_("Select Backup File"))
        dialog.open(self, None, self._on_file, dialog.open_finish, self.restore_backup)

    def _file_dialog(self, title):
        json_files = Gtk.FileFilter(name=_("Ngó Sen backup"))
        json_files.add_suffix("json")
        filters = Gio.ListStore.new(Gtk.FileFilter)
        filters.append(json_files)
        return Gtk.FileDialog(title=title, modal=True, filters=filters, default_filter=json_files)

    def _on_file(self, _dialog, result, finish, use):
        try:
            file = finish(result)
        except GLib.Error:
            return  # Cancelled.
        if file is not None and file.get_path():
            use(file.get_path())

    def export_backup(self, path):
        try:
            backup.export(self.dbus, path)
        except (OSError, ValueError) as e:
            self._show(Adw.Toast(title=_("Failed to export backup:\n").strip() + " " + str(e)))
            return
        self._show(Adw.Toast(title=_("Backed up to {}.").format(path)))

    def restore_backup(self, path):
        try:
            data = backup.load(path)
        except (OSError, ValueError) as e:
            self._show(Adw.Toast(title=_("Failed to open backup file:\n").strip() + " " + str(e)))
            return
        self._replace_all(
            lambda: backup.apply(self.dbus, data),
            _("Restored from {}.").format(os.path.basename(path)),
            _("Could not restore. Check that fcitx5 is running."),
        )

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
        self.results = Gtk.ListBox(css_classes=["navigation-sidebar"])
        self.results.connect("row-activated", lambda _l, row: self.open_result(row))
        self.side_stack = Gtk.Stack()
        self.side_stack.add_named(Gtk.ScrolledWindow(child=self.sidebar), "pages")
        self.side_stack.add_named(Gtk.ScrolledWindow(child=self.results), "results")
        self.search_entry = Gtk.SearchEntry(
            placeholder_text=_("Search settings"), margin_start=8, margin_end=8, margin_bottom=6
        )
        self.search_entry.connect("search-changed", self._on_search)
        self.search_entry.connect("activate", self._open_first_result)
        find = Gtk.ShortcutController(scope=Gtk.ShortcutScope.MANAGED)
        find.add_shortcut(
            Gtk.Shortcut(
                trigger=Gtk.ShortcutTrigger.parse_string("<Control>f"),
                action=Gtk.CallbackAction.new(lambda *_a: self.focus_search()),
            )
        )
        self.add_controller(find)
        view = Adw.ToolbarView(content=self.side_stack)
        view.add_top_bar(header)
        view.add_top_bar(self.search_entry)
        return view

    def _on_search(self, entry):
        self.results.remove_all()
        text = entry.get_text()
        self.side_stack.set_visible_child_name("results" if text.strip() else "pages")
        if not text.strip():
            return
        for title, row, name in search.find(self.search_pages, text):
            result = Gtk.ListBoxRow()
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, margin_top=4, margin_bottom=4)
            box.append(Gtk.Label(label=title, xalign=0, wrap=True))
            page = Gtk.Label(label=self.page_titles[name], xalign=0, css_classes=["dim-label"])
            box.append(page)
            result.set_child(box)
            result.found = (row, name)
            self.results.append(result)
        if self.results.get_row_at_index(0) is None:
            empty = Gtk.ListBoxRow(activatable=False, selectable=False)
            empty.set_child(Gtk.Label(label=_("No settings found"), css_classes=["dim-label"]))
            self.results.append(empty)

    def focus_search(self):
        # A narrow window shows the page alone, so the search field has to be brought back.
        self.split.set_show_content(False)
        self.search_entry.grab_focus()
        return True

    def _open_first_result(self, entry):
        # Results follow typing after a short delay; Enter must not open the previous query's.
        self._on_search(entry)
        first = self.results.get_row_at_index(0)
        if first is not None and hasattr(first, "found"):
            self.open_result(first)

    def open_result(self, result):
        """Shows the page and tab holding a found row and puts the focus on it."""
        if not hasattr(result, "found"):
            return
        row, name = result.found
        self.search_entry.set_text("")
        self.side_stack.set_visible_child_name("pages")
        self.show_page(name)
        focus_when_laid_out(row)

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
                focus_when_laid_out(control)
                return True
        return False

    def open_system_check(self):
        if self.check is None:
            self.check = CheckDialog(self.dbus, __version__)
        else:
            self.check.run()
        self.check.dialog.present(self)

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
        before = settings_snapshot.take(self.dbus)
        ok = self.model.save()
        self.model.modified.update(held)
        if key in held:
            self._show(Adw.Toast(title=conflicts[0][0], timeout=0))
        else:
            self._report(ok, self.model.label(key) or key, before)

    def save_with_undo(self, write, label):
        """Runs a page's write, with Undo back to what fcitx5 held just before it."""
        # Taken now, since fcitx5 remembers app modes while the window is open.
        before = settings_snapshot.take(self.dbus)
        ok = write()
        self._report(ok, label, before)
        return ok

    def _report(self, ok, label, before):
        if ok:
            self._undo_to = before
            self._show(self._with_undo(Adw.Toast(title=_("Saved “{}”.").format(label))))
        else:
            self._show(
                Adw.Toast(title=_("Could not save. Check that fcitx5 is running."), timeout=0)
            )

    def _with_undo(self, toast):
        if self._undo_to is not None:
            toast.set_button_label(_("&Undo").replace("&", "_"))
            toast.connect("button-clicked", lambda _t: self.undo())
        return toast

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
