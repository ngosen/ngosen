# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Keymap page: its switch, presets, an editor and the key list, each saved at once."""

from gi.repository import Adw, Gtk
from i18n import _

from core import keymap

ACTION_CODES = [code for code, _name in keymap.BAMBOO_ACTIONS]
ACTION_NAMES = [name for _code, name in keymap.BAMBOO_ACTIONS]


class KeymapPage:
    """on_saved(ok, label) hears about every save, to show a toast."""

    def __init__(self, table, on_saved):
        self.table = table
        self.on_saved = on_saved
        self._syncing = False
        self.page = Adw.PreferencesPage(
            title=_("Keymap"), icon_name="preferences-desktop-keyboard-symbolic"
        )
        self.page.add(self._options())
        self.page.add(self._editor())
        self.page.add(self._list())
        self.refresh()

    def _options(self):
        group = Adw.PreferencesGroup()
        self.switch = Adw.SwitchRow(title=_("Custom Keymap"), use_markup=False)
        self.switch.connect("notify::active", self._on_switch)
        group.add(self.switch)
        self.preset = Adw.ComboRow(title=_("Original Input Method:").rstrip(":"), use_markup=False)
        self.preset.set_model(Gtk.StringList.new(list(keymap.PRESETS)))
        apply = Gtk.Button(label=_("Apply Preset"), valign=Gtk.Align.CENTER)
        apply.connect("clicked", lambda _b: self._confirm_preset())
        self.preset.add_suffix(apply)
        group.add(self.preset)
        return group

    def _on_switch(self, row, _param):
        if not self._syncing:
            self._change(lambda: setattr(self.table, "enabled", row.get_active()))

    def _confirm_preset(self):
        name = list(keymap.PRESETS)[self.preset.get_selected()]
        dialog = Adw.AlertDialog(
            heading=_("Confirm"),
            body=_("This operation will replace all existing keys with ")
            + name
            + _(". Are you sure?"),
        )
        dialog.add_response("cancel", _("&Cancel").replace("&", "_"))
        dialog.add_response("apply", _("Apply Preset"))
        dialog.set_response_appearance("apply", Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.connect("response", lambda _d, response: response == "apply" and self.apply(name))
        dialog.present(self.page)

    def apply(self, name):
        entries = [{"Key": k, "Value": v} for k, v in keymap.PRESETS[name]]
        self._change(lambda: setattr(self.table, "entries", entries))

    def _editor(self):
        group = Adw.PreferencesGroup()
        self.key_row = Adw.EntryRow(title=_("Key (Example: s)"))
        self.key_row.add_css_class("typed-input")
        self.add_button = Gtk.Button(label=_("Add"), valign=Gtk.Align.CENTER)
        self.add_button.add_css_class("suggested-action")
        self.add_button.connect("clicked", lambda _b: self._add())
        self.key_row.add_suffix(self.add_button)
        self.key_row.connect("changed", lambda _r: self._check())
        self.key_row.connect("entry-activated", lambda _r: self._add())
        group.add(self.key_row)
        self.action = Adw.ComboRow(title=_("Action"), use_markup=False)
        self.action.set_model(Gtk.StringList.new(ACTION_NAMES))
        group.add(self.action)
        return group

    def _list(self):
        group = Adw.PreferencesGroup()
        self.list = Gtk.ListBox(selection_mode=Gtk.SelectionMode.NONE)
        self.list.add_css_class("boxed-list")
        group.add(self.list)
        return group

    def _index(self, key):
        return next((i for i, e in enumerate(self.table.entries) if e["Key"] == key), None)

    def _check(self):
        key = self.key_row.get_text().strip()
        self.add_button.set_label(_("Update") if self._index(key) is not None else _("Add"))
        self.add_button.set_sensitive(bool(key))

    def _add(self):
        if not self.add_button.get_sensitive():
            return
        key = self.key_row.get_text().strip()
        action = ACTION_CODES[self.action.get_selected()]
        index = self._index(key)

        def put():
            if index is None:
                self.table.entries.append({"Key": key, "Value": action})
            else:
                self.table.entries[index] = {"Key": key, "Value": action}

        if self._change(put):
            self.key_row.set_text("")
            self.key_row.grab_focus()

    def _set_action(self, key, position):
        index = self._index(key)
        action = ACTION_CODES[position]
        if index is not None and self.table.entries[index]["Value"] != action:
            self._change(lambda: self.table.entries[index].update({"Value": action}))

    def _remove(self, key):
        index = self._index(key)
        if index is not None:
            self._change(lambda: self.table.entries.pop(index))

    def _change(self, change):
        """Applies and saves a change, or puts the page back as it was if fcitx5 refuses it."""
        # Not reloaded on failure, for the reason given in MacroPage._change.
        enabled, entries = self.table.enabled, [dict(e) for e in self.table.entries]
        change()
        ok = self.table.save()
        if not ok:
            self.table.enabled, self.table.entries = enabled, entries
        self.on_saved(ok, _("Keymap"))
        self.refresh()
        return ok

    def refresh(self):
        self._syncing = True
        self.switch.set_active(self.table.enabled)
        self._syncing = False
        self.list.remove_all()
        for entry in self.table.entries:
            self.list.append(self._row(entry))
        self._check()

    def _row(self, entry):
        row = Adw.ActionRow()
        row.add_prefix(Gtk.Label(label=entry["Key"], width_chars=3, css_classes=["typed"]))
        actions = Gtk.DropDown(model=Gtk.StringList.new(ACTION_NAMES), valign=Gtk.Align.CENTER)
        actions.set_selected(ACTION_CODES.index(entry["Value"]))
        actions.connect(
            "notify::selected", lambda d, _p: self._set_action(entry["Key"], d.get_selected())
        )
        row.add_suffix(actions)
        remove = Gtk.Button(
            icon_name="user-trash-symbolic", valign=Gtk.Align.CENTER, tooltip_text=_("Remove")
        )
        remove.add_css_class("flat")
        remove.connect("clicked", lambda _b: self._remove(entry["Key"]))
        row.add_suffix(remove)
        return row
