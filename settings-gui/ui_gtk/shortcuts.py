# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Shortcuts page: the switch key, the mode keys and the order of the mode menu."""

from core.catalog import (
    HINTS,
    MODE_KEY_TO_INTERNAL_NAME,
    MODE_SHORTCUT_TO_VISIBILITY,
    MODE_SWITCHING,
    SETTINGS_MAP,
    SettingsCategory,
)
from gi.repository import Adw, Gdk, GLib, GObject, Gtk
from i18n import _

from ui_gtk.hotkey_button import HotkeyButton

DEFAULT_ORDER = "Sen,Preedit,Emoji,Off,Default"


class ShortcutsPage:
    """on_change(key, value) saves each change at once; other_methods says fcitx5 has more."""

    def __init__(self, model, on_change, other_methods=False):
        self.model = model
        self.on_change = on_change
        self.hotkeys = {}
        self.page = Adw.PreferencesPage(
            title=_("Shortcuts"), icon_name="preferences-desktop-keyboard-shortcuts-symbolic"
        )
        groups = SETTINGS_MAP[SettingsCategory.SHORTCUTS]
        for title, keys in groups.items():
            if title != MODE_SWITCHING:
                self.page.add(self._hotkeys(keys, other_methods))
        self.page.add(self._modes())
        self.show_conflicts()

    def _hotkeys(self, keys, other_methods):
        # Untitled: the tab above already says Shortcuts.
        group = Adw.PreferencesGroup()
        for key in keys:
            item = self.model.metadata.get(key)
            if not item:
                continue
            toggle = key == "ToggleKey"
            shown = self.model.shown_toggle_key(key) if toggle else self._first(key)
            row = Adw.ActionRow(title=_(item[2]), use_markup=False)
            if key in HINTS:
                row.set_subtitle(_(HINTS[key]))
            button = HotkeyButton(shown)
            button.connect("recorded", lambda _b, text, k=key: self._record(k, text))
            clear = Gtk.Button(
                icon_name="edit-clear-symbolic",
                valign=Gtk.Align.CENTER,
                tooltip_text=_("Clear hotkey"),
                css_classes=["flat"],
            )
            clear.connect("clicked", lambda _b, k=key: self._record(k, ""))
            row.add_suffix(button)
            row.add_suffix(clear)
            self.hotkeys[key] = button
            group.add(row)
            if toggle:
                group.add(self._offer(key))
                if other_methods:
                    note = _(
                        "fcitx5 has other input methods too; its own key still switches to them."
                    )
                    group.add(
                        Adw.ActionRow(title=note, use_markup=False, css_classes=["dim-label"])
                    )
        return group

    def _offer(self, key):
        # Upgrades kept Ctrl+Space; UniKey's key is one click away.
        self.offer = Adw.ActionRow(
            title=_("Switch with Ctrl+Shift, as in UniKey?"), use_markup=False
        )
        use = Gtk.Button(label=_("Use Ctrl+Shift"), valign=Gtk.Align.CENTER)
        use.connect("clicked", lambda _b: self._use_ctrl_shift(key))
        self.offer.add_suffix(use)
        self.offer.set_visible(not self.model.toggle_is_ctrl_shift(key))
        return self.offer

    def _first(self, key):
        return next(iter(self.model.hotkeys(key)), "")

    def _record(self, key, text):
        if key == "ToggleKey":
            self.model.set_toggle_key(text, key)
            self.hotkeys[key].set_key(self.model.shown_toggle_key(key))
            self.offer.set_visible(not self.model.toggle_is_ctrl_shift(key))
        else:
            self.model.set_first_hotkey(key, text)
            self.hotkeys[key].set_key(self._first(key))
        self._changed(key, self.model.current[key])

    def _changed(self, key, value):
        self.on_change(key, value)
        self.show_conflicts()

    def _use_ctrl_shift(self, key):
        self.hotkeys[key].set_active(False)
        self._record(key, "")

    def _modes(self):
        group = Adw.PreferencesGroup(
            title=_(MODE_SWITCHING),
            description=_(
                "Drag the handle on the left to reorder modes in the menu. Use checkboxes to toggle visibility, and click the buttons to reassign shortcuts."
            ),
        )
        self.modes = Gtk.ListBox(selection_mode=Gtk.SelectionMode.NONE, css_classes=["boxed-list"])
        self.mode_rows = {}
        group.add(self.modes)
        hint = Gtk.Label(
            label=_(HINTS["ModeOrder"]),
            wrap=True,
            xalign=0,
            margin_top=12,
            css_classes=["dim-label"],
        )
        group.add(hint)
        self._fill_modes()
        return group

    def order(self):
        saved = str(self.model.current.get("ModeOrder", DEFAULT_ORDER))
        # The addon renames old mode names before it hands the config out.
        order = list(dict.fromkeys(saved.split(",")))
        return order + [n for n in MODE_KEY_TO_INTERNAL_NAME.values() if n not in order]

    def _fill_modes(self):
        self.modes.remove_all()
        self.mode_rows.clear()
        keys = {name: key for key, name in MODE_KEY_TO_INTERNAL_NAME.items()}
        for name in self.order():
            key = keys.get(name)
            if key and key in self.model.metadata:
                self.modes.append(self._mode_row(key, name))

    def _mode_row(self, key, name):
        row = Adw.ActionRow(title=_(self.model.metadata[key][2]), use_markup=False)
        row.add_prefix(Gtk.Image(icon_name="list-drag-handle-symbolic"))
        visible_key = MODE_SHORTCUT_TO_VISIBILITY[key]
        shown = Gtk.CheckButton(valign=Gtk.Align.CENTER)
        shown.set_active(str(self.model.current.get(visible_key, "True")).lower() == "true")
        shown.connect(
            "toggled", lambda b: self._changed(visible_key, "True" if b.get_active() else "False")
        )
        row.add_prefix(shown)
        button = HotkeyButton(
            str(self.model.current.get(key, self.model.default(key))), single=True
        )
        button.connect("recorded", lambda _b, text: self._changed(key, text))
        row.add_suffix(button)

        source = Gtk.DragSource(actions=Gdk.DragAction.MOVE)
        source.connect("prepare", lambda *_a: Gdk.ContentProvider.new_for_value(name))
        row.add_controller(source)
        target = Gtk.DropTarget.new(GObject.TYPE_STRING, Gdk.DragAction.MOVE)
        target.connect("drop", lambda _t, value, _x, _y: self.move(value, name))
        row.add_controller(target)
        self.mode_rows[key] = row
        return row

    def move(self, name, onto):
        """Puts mode name where onto is, as a drop does."""
        order = self.order()
        if name == onto or name not in order or onto not in order:
            return False
        # Dragged down it lands below the row, dragged up above it.
        below = order.index(onto) > order.index(name)
        order.remove(name)
        order.insert(order.index(onto) + below, name)
        self._changed("ModeOrder", ",".join(order))
        # The drop is still running on a row that rebuilding would destroy.
        GLib.idle_add(self._rebuild_modes)
        return True

    def _rebuild_modes(self):
        self._fill_modes()
        self.show_conflicts()
        return GLib.SOURCE_REMOVE

    def show_conflicts(self):
        """Marks modes that share a key; nothing is saved until they no longer do."""
        conflicts = {
            key: message for message, keys in self.model.shortcut_conflicts() for key in keys
        }
        for key, row in self.mode_rows.items():
            message = conflicts.get(key)
            row.set_subtitle(message or "")
            if message:
                row.add_css_class("error")
            else:
                row.remove_css_class("error")
        return bool(conflicts)
