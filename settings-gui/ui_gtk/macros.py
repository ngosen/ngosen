# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Macros page: its switches and formats, an editor and the list, each saved at once."""

from core.catalog import HINTS, locked_by
from gi.repository import Adw, Gtk
from i18n import N_, _

from core import chat_shorthand, macros, page_status
from ui_gtk.status_row import show_status, status_row

SWITCHES = {
    "EnableMacro": N_("Enable Macro"),
    "CapitalizeMacro": N_("Capitalize Macro"),
    "EnableMacroInOffMode": N_("Macro in Off Mode"),
}
FORMAT_TITLES = {"TimeFormat": N_("Time Format:"), "DateFormat": N_("Date Format:")}
INVALID_KEY = N_("Macro keys cannot contain spaces or special characters.")


class MacroPage:
    """on_save(write, label) runs every save, so the window can offer to undo it.
    show_message(text, undo) shows a toast."""

    def __init__(self, table, on_save, show_message=None):
        self.table = table
        self.on_save = on_save
        self.show_message = show_message
        self.chat = chat_shorthand.ChatShorthand(table.dbus)
        self._syncing = False
        self.page = Adw.PreferencesPage(
            title=_("Macros"), icon_name="accessories-text-editor-symbolic"
        )
        self.page.add(self._options())
        self.page.add(self._chat())
        self.page.add(self._editor())
        self.page.add(self._list())
        self.refresh()
        self.refresh_status()

    def _options(self):
        group = Adw.PreferencesGroup(
            title=_("Macros"),
            description=_("Macros support dynamic placeholders: $TIME and $DATE."),
        )
        self.status = status_row()
        group.add(self.status)
        self.switches = {}
        for key, title in SWITCHES.items():
            group.add(self._switch(key, _(title)))
        self.formats = {}
        for key, title in FORMAT_TITLES.items():
            row = Adw.ComboRow(title=_(title).rstrip(":"), use_markup=False)
            row.set_model(Gtk.StringList.new([_(macros.EXAMPLES[f]) for f in macros.FORMATS[key]]))
            row.set_selected(macros.FORMATS[key].index(self.table.options[key]))
            row.connect("notify::selected", self._on_format, key)
            row.setting_key = key
            self.formats[key] = row
            group.add(row)
        return group

    def _switch(self, key, title):
        row = Adw.SwitchRow(title=title, use_markup=False)
        if key in HINTS:
            row.set_subtitle(_(HINTS[key]))
        row.set_active(self.table.options[key])
        row.connect("notify::active", self._on_switch, key)
        row.setting_key = key
        self.switches[key] = row
        return row

    def _chat(self):
        group = Adw.PreferencesGroup(title=_("Chat abbreviations"))
        group.add(self._switch("ChatShorthand", _("Expand chat abbreviations")))
        self.chat_list = Adw.ExpanderRow(title=_("The list"), use_markup=False)
        # The built-in entries are not settings to find.
        self.chat_list.searchable = False
        self.chat_entries = None
        self.chat_rows = []
        group.add(self.chat_list)
        return group

    def _show_chat(self):
        entries, why = self.chat.entries()
        self.chat_list.set_subtitle(why or chat_shorthand.kept_line(entries))
        self.chat_list.set_enable_expansion(bool(entries))
        if entries == self.chat_entries:
            return
        self.chat_entries = entries
        for row in self.chat_rows:
            self.chat_list.remove(row)
        self.chat_rows = [self._chat_row(entry) for entry in entries or []]
        for row in self.chat_rows:
            self.chat_list.add_row(row)

    def _chat_row(self, entry):
        row = Adw.ActionRow(title=entry.key, subtitle=entry.text, use_markup=False)
        if entry.kept:
            button = Gtk.Button(label=_("Expand again"), valign=Gtk.Align.CENTER)
            button.update_property(
                [Gtk.AccessibleProperty.LABEL], [_("Expand {} again").format(entry.key)]
            )
            button.connect("clicked", lambda _b: self.expand_again(entry.key))
            row.add_suffix(button)
        return row

    def expand_again(self, key):
        _done, line = self.chat.expand_again(key)
        if self.show_message:
            self.show_message(line, None)
        self._show_chat()

    def control_for(self, key):
        """The row that edits a setting, for the window to show and focus."""
        return self.switches.get(key) or self.formats.get(key)

    def value_of(self, key):
        return self.table.options.get(key)

    def _apply_locks(self):
        for key, row in (*self.switches.items(), *self.formats.items()):
            row.set_sensitive(locked_by(key, self.value_of) is None)

    def refresh_status(self):
        """Shows why this page is not in use now, without rebuilding it."""
        model = page_status.read(self.table.dbus)
        if model and not self.table.is_modified():
            self.table.follow(model.value_of)
            self._syncing = True
            for key, row in self.switches.items():
                row.set_active(self.table.options[key])
            self._syncing = False
            self._apply_locks()
        show_status(self.status, page_status.macro_line(model) if model else "")
        self._show_chat()

    def _on_switch(self, row, _param, key):
        if not self._syncing:
            self._change(
                lambda: self.table.options.update({key: row.get_active()}), row.get_title()
            )

    def _on_format(self, row, _param, key):
        if not self._syncing:
            value = macros.FORMATS[key][row.get_selected()]
            self._change(lambda: self.table.options.update({key: value}), row.get_title())

    def _editor(self):
        group = Adw.PreferencesGroup()
        self.key_row = Adw.EntryRow(title=_("Abbreviation (e.g. kg)"))
        self.value_row = Adw.EntryRow(title=_("Full text (e.g. khô gà)"))
        self.value_row.add_css_class("typed-input")
        self.add_button = Gtk.Button(label=_("Add"), valign=Gtk.Align.CENTER)
        self.add_button.add_css_class("suggested-action")
        self.add_button.connect("clicked", lambda _b: self._add())
        self.value_row.add_suffix(self.add_button)
        for row in (self.key_row, self.value_row):
            row.connect("changed", lambda _r: self._check())
            row.connect("entry-activated", lambda _r: self._add())
            group.add(row)
        return group

    def _list(self):
        group = Adw.PreferencesGroup()
        self.search = Gtk.SearchEntry(placeholder_text=_("Search macros..."))
        self.search.connect("search-changed", lambda _s: self.list.invalidate_filter())
        group.set_header_suffix(self.search)
        self.list = Gtk.ListBox(selection_mode=Gtk.SelectionMode.NONE)
        self.list.add_css_class("boxed-list")
        self.list.set_filter_func(self._matches)
        # The user's own entries are not settings to find.
        self.list.searchable = False
        group.add(self.list)
        return group

    def _index(self, key):
        # The engine ignores case in macro keys.
        return next(
            (i for i, e in enumerate(self.table.entries) if e["Key"].lower() == key.lower()), None
        )

    def _check(self):
        key = self.key_row.get_text()
        bad = macros.is_invalid_key(key)
        if bad:
            self.key_row.add_css_class("error")
        else:
            self.key_row.remove_css_class("error")
        self.key_row.set_tooltip_text(_(INVALID_KEY) if bad else None)
        self.add_button.set_label(_("Update") if self._index(key) is not None else _("Add"))
        self.add_button.set_sensitive(bool(key) and not bad and bool(self.value_row.get_text()))

    def _add(self):
        if not self.add_button.get_sensitive():
            return
        key, value = self.key_row.get_text(), self.value_row.get_text()
        index = self._index(key)

        def put():
            if index is None:
                self.table.entries.append({"Key": key, "Value": value})
            else:
                self.table.entries[index] = {"Key": key, "Value": value}

        if self._change(put, _("Macros")):
            self.key_row.set_text("")
            self.value_row.set_text("")
            self.key_row.grab_focus()

    def _remove(self, key):
        index = self._index(key)
        if index is not None:
            self._change(lambda: self.table.entries.pop(index), _("Macros"))

    def _edit(self, entry):
        self.key_row.set_text(entry["Key"])
        self.value_row.set_text(entry["Value"])
        self.value_row.grab_focus()

    def _change(self, change, label):
        """Applies and saves a change, or puts the page back as it was if fcitx5 refuses it."""
        # Not reloaded on failure: an fcitx5 that is restarting reads as an empty table, and the
        # next save would write that over the user's macros.
        options, entries = dict(self.table.options), [dict(e) for e in self.table.entries]
        change()
        ok = self.on_save(self.table.save, label)
        if not ok:
            self.table.options, self.table.entries = options, entries
        self.refresh()
        return ok

    def _matches(self, row):
        needle = self.search.get_text().casefold()
        key, value = row.entry["Key"], row.entry["Value"]
        return not needle or needle in key.casefold() or needle in value.casefold()

    def refresh(self):
        self._syncing = True
        for key, row in self.switches.items():
            row.set_active(self.table.options[key])
        for key, row in self.formats.items():
            row.set_selected(macros.FORMATS[key].index(self.table.options[key]))
        self._syncing = False
        self._apply_locks()
        self.list.remove_all()
        for entry in self.table.entries:
            self.list.append(self._row(entry))
        self._check()

    def _row(self, entry):
        row = Adw.ActionRow(use_markup=False, activatable=True)
        row.set_title(entry["Key"])
        row.set_subtitle(entry["Value"])
        row.entry = entry
        if macros.is_invalid_key(entry["Key"]):
            row.add_css_class("error")
            row.set_tooltip_text(_(INVALID_KEY))
        row.connect("activated", lambda _r: self._edit(entry))
        remove = Gtk.Button(
            icon_name="user-trash-symbolic", valign=Gtk.Align.CENTER, tooltip_text=_("Remove")
        )
        remove.add_css_class("flat")
        remove.connect("clicked", lambda _b: self._remove(entry["Key"]))
        row.add_suffix(remove)
        return row
