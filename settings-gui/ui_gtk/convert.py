# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Convert page: the hotkey that converts selected text, its charsets, and a box to convert here."""

from core.catalog import CONVERT_CHARSETS, HINTS, SettingsCategory
from gi.repository import Adw, Gdk, GLib, Gtk
from i18n import _

from core import convert
from ui_gtk.hotkey_button import HotkeyButton
from ui_gtk.pages import SettingsPage

# Long enough to skip the calls in the middle of a fast paste or typing burst.
DELAY_MS = 150
BOX_HEIGHT = 110
COPIED_MS = 1500


def _text_view(editable=True):
    view = Gtk.TextView(
        wrap_mode=Gtk.WrapMode.WORD_CHAR,
        editable=editable,
        cursor_visible=editable,
        accepts_tab=False,
        top_margin=10,
        bottom_margin=10,
        left_margin=12,
        right_margin=12,
        css_classes=["typed"],
    )
    scroll = Gtk.ScrolledWindow(
        child=view,
        min_content_height=BOX_HEIGHT,
        hscrollbar_policy=Gtk.PolicyType.NEVER,
        css_classes=["card"],
    )
    return view, scroll


class ConvertPage(SettingsPage):
    def __init__(self, model, on_change, dbus):
        self.dbus = dbus
        self.hotkey = None
        self._pending = 0
        super().__init__(
            model,
            on_change,
            SettingsCategory.CONVERT,
            _("Convert"),
            "ngosen-charset",
        )
        self.page.add(self._convert_here())

    def _rows_for(self, item):
        if item[0] == "ConvertKey":
            return [self._hotkey_row(item)]
        return super()._rows_for(item)

    def control_for(self, key):
        return self.hotkey if key == "ConvertKey" else super().control_for(key)

    def _hotkey_row(self, item):
        key = item[0]
        row = Adw.ActionRow(title=_(item[2]), subtitle=_(HINTS[key]), use_markup=False)
        self.hotkey = HotkeyButton(next(iter(self.model.hotkeys(key)), ""))
        self.hotkey.connect("recorded", lambda _b, text: self._record(key, text))
        clear = Gtk.Button(
            icon_name="edit-clear-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Clear hotkey"),
            css_classes=["flat"],
        )
        clear.connect("clicked", lambda _b: self._record(key, ""))
        row.add_suffix(self.hotkey)
        row.add_suffix(clear)
        return row

    def _record(self, key, text):
        self.model.set_first_hotkey(key, text)
        self.hotkey.set_key(next(iter(self.model.hotkeys(key)), ""))
        self.on_change(key, self.model.current[key])

    def _add_extras(self, title, group):
        if title != CONVERT_CHARSETS:
            return
        swap = Gtk.Button(
            label=_("S_wap"),
            use_underline=True,
            tooltip_text=_("Swap the charsets"),
            css_classes=["flat"],
        )
        swap.connect("clicked", lambda _b: self._swap())
        group.set_header_suffix(swap)

    def _swap(self):
        source, target = self.rows.get("ConvertFrom"), self.rows.get("ConvertTo")
        if not source or not target:
            return
        first, second = source.get_selected(), target.get_selected()
        self._following = True
        source.set_selected(second)
        target.set_selected(first)
        self._following = False
        # One save, so Undo puts both back.
        self.model.set("ConvertFrom", source.values[second])
        self._changed("ConvertTo", target.values[first])

    def _changed(self, key, value):
        super()._changed(key, value)
        if key in convert.SETTING_KEYS:
            self.refresh()

    def _convert_here(self):
        group = Adw.PreferencesGroup(
            title=_("Convert here"),
            description=_("Paste text in the first box; the second shows it converted."),
        )
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        self.source, source_box = _text_view()
        self.source.update_property(
            [Gtk.AccessibleProperty.LABEL], [_("Paste the text to convert")]
        )
        self.result, result_box = _text_view(editable=False)
        self.result.update_property(
            [Gtk.AccessibleProperty.LABEL], [_("The converted text shows here")]
        )
        self.source.get_buffer().connect("changed", lambda _b: self._later())
        box.append(source_box)
        box.append(result_box)

        bottom = Gtk.Box(spacing=8)
        self.error = Gtk.Label(wrap=True, xalign=0, use_markup=False)
        self.error_icon = Gtk.Image(icon_name="dialog-warning-symbolic", valign=Gtk.Align.START)
        self.copy = Gtk.Button(
            label=_("_Copy result"), use_underline=True, halign=Gtk.Align.END, hexpand=True
        )
        self.copy.connect("clicked", lambda _b: self._copy())
        bottom.append(self.error_icon)
        bottom.append(self.error)
        bottom.append(self.copy)
        box.append(bottom)
        group.add(box)
        self._show("", None)
        return group

    def _later(self):
        if self._pending:
            GLib.source_remove(self._pending)
        self._pending = GLib.timeout_add(DELAY_MS, self._on_time)

    def _on_time(self):
        self._pending = 0
        self.refresh()
        return GLib.SOURCE_REMOVE

    def refresh(self):
        buffer = self.source.get_buffer()
        text = buffer.get_text(buffer.get_start_iter(), buffer.get_end_iter(), False)
        self._show(*convert.convert(self.dbus, self.model, text))

    def _show(self, text, error):
        self.result.get_buffer().set_text(text or "")
        self.error.set_text(error or "")
        self.error.set_visible(bool(error))
        self.error_icon.set_visible(bool(error))
        self.copy.set_sensitive(bool(text))

    def _copy(self):
        buffer = self.result.get_buffer()
        text = buffer.get_text(buffer.get_start_iter(), buffer.get_end_iter(), False)
        Gdk.Display.get_default().get_clipboard().set(text)
        self.copy.set_label(_("Copied"))
        GLib.timeout_add(COPIED_MS, self._copied_done)

    def _copied_done(self):
        self.copy.set_label(_("_Copy result"))
        return GLib.SOURCE_REMOVE
