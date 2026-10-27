# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Dictionary page: its switch, an editor and the word list, each saved at once."""

from gi.repository import Adw, Gtk
from i18n import N_, _

INVALID_WORD = N_("Warning: Dictionary words should not contain spaces.")


class DictionaryPage:
    """on_saved(ok, label) hears about every save, to show a toast."""

    def __init__(self, dictionary, on_saved):
        self.dictionary = dictionary
        self.on_saved = on_saved
        self._syncing = False
        error = dictionary.load()
        self.page = Adw.PreferencesPage(
            title=_("Dictionary"), icon_name="accessories-dictionary-symbolic"
        )
        self.page.add(self._options(error))
        self.page.add(self._editor())
        self.page.add(self._list())
        # A file that was not read in full must not be written back.
        for group in (self.editor, self.list_group):
            group.set_sensitive(not dictionary.load_failed)
        self.switch.set_sensitive(not dictionary.load_failed)
        self.refresh()

    def _options(self, error):
        group = Adw.PreferencesGroup()
        if error:
            message = _(
                "Failed to load dictionary completely: {}\nSaving is disabled to prevent data loss."
            ).format(error)
            row = Adw.ActionRow(title=message, use_markup=False, css_classes=["error"])
            group.add(row)
        self.switch = Adw.SwitchRow(title=_("Custom Dictionary"), use_markup=False)
        self.switch.connect("notify::active", self._on_switch)
        group.add(self.switch)
        return group

    def _on_switch(self, row, _param):
        if not self._syncing:
            self._change(lambda: setattr(self.dictionary, "enabled", row.get_active()))

    def _editor(self):
        self.editor = Adw.PreferencesGroup()
        self.word_row = Adw.EntryRow(title=_("Word (e.g. khongdau)"))
        self.word_row.add_css_class("typed-input")
        self.add_button = Gtk.Button(label=_("Add"), valign=Gtk.Align.CENTER)
        self.add_button.add_css_class("suggested-action")
        self.add_button.connect("clicked", lambda _b: self._add())
        self.word_row.add_suffix(self.add_button)
        self.word_row.connect("changed", lambda _r: self._check())
        self.word_row.connect("entry-activated", lambda _r: self._add())
        self.editor.add(self.word_row)
        return self.editor

    def _list(self):
        self.list_group = Adw.PreferencesGroup()
        self.search = Gtk.SearchEntry(placeholder_text=_("Search words..."))
        self.search.connect(
            "search-changed", lambda _s: self.filter.changed(Gtk.FilterChange.DIFFERENT)
        )
        self.list_group.set_header_suffix(self.search)
        # The bundled list has thousands of words; a list view draws only the rows on screen.
        self.words = Gtk.StringList()
        self.filter = Gtk.CustomFilter.new(self._matches)
        filtered = Gtk.FilterListModel(model=self.words, filter=self.filter)
        factory = Gtk.SignalListItemFactory()
        factory.connect("setup", self._setup_row)
        factory.connect("bind", self._bind_row)
        view = Gtk.ListView(
            model=Gtk.NoSelection(model=filtered), factory=factory, css_classes=["boxed-list"]
        )
        scroll = Gtk.ScrolledWindow(child=view, height_request=320)
        scroll.add_css_class("card")
        self.list_group.add(scroll)
        return self.list_group

    def _setup_row(self, _factory, item):
        box = Gtk.Box(spacing=6, margin_start=12, margin_end=6, margin_top=2, margin_bottom=2)
        label = Gtk.Label(xalign=0, hexpand=True, css_classes=["typed"])
        remove = Gtk.Button(
            icon_name="user-trash-symbolic", tooltip_text=_("Remove"), css_classes=["flat"]
        )
        remove.connect("clicked", lambda _b: self._remove(label.get_label()))
        box.append(label)
        box.append(remove)
        item.set_child(box)

    def _bind_row(self, _factory, item):
        item.get_child().get_first_child().set_label(item.get_item().get_string())

    def _matches(self, item):
        needle = self.search.get_text().casefold()
        return not needle or needle in item.get_string().casefold()

    def _check(self):
        word = self.word_row.get_text().strip()
        bad = " " in word
        if bad:
            self.word_row.add_css_class("error")
        else:
            self.word_row.remove_css_class("error")
        self.word_row.set_tooltip_text(_(INVALID_WORD) if bad else None)
        exists = word in self.dictionary.words
        self.add_button.set_label(_("Exists") if exists else _("Add"))
        self.add_button.set_sensitive(bool(word) and not bad and not exists)

    def _add(self):
        if not self.add_button.get_sensitive():
            return
        word = self.word_row.get_text().strip()
        if self._change(lambda: self.dictionary.words.append(word)):
            self.word_row.set_text("")
            self.word_row.grab_focus()

    def _remove(self, word):
        if word in self.dictionary.words:
            self._change(lambda: self.dictionary.words.remove(word))

    def _change(self, change):
        """Applies and saves a change, or puts the page back as it was if it cannot be saved."""
        enabled, words = self.dictionary.enabled, list(self.dictionary.words)
        change()
        try:
            ok = self.dictionary.save()
        except OSError:
            ok = False
        if not ok:
            self.dictionary.enabled, self.dictionary.words = enabled, words
        self.on_saved(ok, _("Custom Dictionary"))
        self.refresh()
        return ok

    def refresh(self):
        self._syncing = True
        self.switch.set_active(self.dictionary.enabled)
        self._syncing = False
        words = sorted(self.dictionary.words)
        self.words.splice(0, self.words.get_n_items(), words)
        self._check()
