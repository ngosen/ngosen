# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The pages of the GTK window, drawn from the same catalog as the Qt window."""

from core.brand import FEATURE_REQUEST_URL, REPO_URL
from core.catalog import (
    DEPENDS,
    HINTS,
    LEARNING_GROUP,
    MODE_BUTTON_ORDER,
    MODE_HINTS,
    SETTINGS_MAP,
    TYPING_EXAMPLES,
    TYPING_GROUP,
    SettingsCategory,
    shown_mode,
)
from core.learned import Learned, how_it_learns
from core.teencode_apps import TeencodeApps, toggle_hint
from gi.repository import Adw, Gtk
from i18n import _

from core import page_status
from ui_gtk import brand
from ui_gtk.check_page import open_uri
from ui_gtk.status_row import older_fcitx_group, show_status, status_row
from ui_gtk.whats_new import tip_text


def _hinted(row, key):
    row.set_use_markup(False)
    if key in HINTS:
        row.set_subtitle(_(HINTS[key]))
    return row


def _show_value_whole(row):
    # Adwaita cuts the value short when a long hint shares the row.
    whole = Gtk.SignalListItemFactory()
    whole.connect("setup", lambda _f, item: item.set_child(Gtk.Label(xalign=0)))
    whole.connect("bind", lambda _f, item: item.get_child().set_label(item.get_item().get_string()))
    # The list keeps its own look, with the mark on the chosen value.
    row.set_list_factory(row.get_factory())
    row.set_factory(whole)


class SettingsPage:
    """One category of the catalog as rows; each change goes to on_change(key, value) at once."""

    def __init__(self, model, on_change, category, title, icon):
        self.model = model
        self.on_change = on_change
        self.rows = {}
        self._following = False
        self.page = Adw.PreferencesPage(title=title, icon_name=icon)
        older, self.older_note = older_fcitx_group(model, category)
        self.page.add(older)
        self._add_header()
        for title, keys in SETTINGS_MAP[category].items():
            # The page title already says this one.
            group = Adw.PreferencesGroup(title="" if title == TYPING_GROUP else _(title))
            for key in keys:
                item = model.metadata.get(key)
                if item:
                    for row in self._rows_for(item):
                        group.add(row)
            self._add_extras(title, group)
            self.page.add(group)
        for key, row in self.rows.items():
            row.setting_key = key
        self._apply_locks()

    def _add_header(self):
        """Rows a page shows above its settings."""

    def _add_extras(self, title, group):
        """Rows a page adds after a group's settings."""

    def control_for(self, key):
        """The row that edits a setting, for the window to show and focus."""
        return self.rows.get(key)

    def value_of(self, key):
        return self.model.value_of(key)

    def refresh_status(self):
        """Follows what fcitx5 holds, for a change made elsewhere while the window was open."""
        model = page_status.read(self.model.dbus)
        if model and not self.model.modified:
            self.model.follow(model)
            self._show_values()

    def _apply_locks(self):
        for key in DEPENDS.keys() & self.rows.keys():
            self.rows[key].set_sensitive(self.model.locked_by(key) is None)

    def _changed(self, key, value):
        if self._following:
            return
        self.on_change(key, value)
        self._apply_locks()

    def _show_values(self):
        """Moves the rows to the model's values, as nothing the user changed."""
        self._following = True
        for key, row in self.rows.items():
            if isinstance(row, Adw.SwitchRow):
                row.set_active(self.value(key).lower() == "true")
            elif isinstance(row, Adw.ComboRow) and self.value(key) in row.values:
                row.set_selected(row.values.index(self.value(key)))
        if "Mode" in self.rows and shown_mode(self.value("Mode")) in self.mode_buttons:
            self.mode_buttons[shown_mode(self.value("Mode"))].set_active(True)
        self._following = False
        self._apply_locks()

    def _shown(self, key, value):
        return _(value)

    def _switch_title(self, key, label):
        return _(label)

    def value(self, key):
        return str(self.model.current.get(key, self.model.default(key)))

    def _rows_for(self, item):
        key, kind, label, _default, annotations = item
        if key == "Mode":
            return [self._mode_row(key, label, annotations)]
        if "Enum" in annotations:
            row = self._combo_row(key, label, annotations["Enum"])
            if key == "InputMethod":
                self._show_example(row)
            return [row]
        if kind == "Boolean":
            return [self._switch_row(key, label)]
        return []

    def _mode_row(self, key, label, annotations):
        row = _hinted(Adw.ActionRow(title=_(label)), key)
        box = Gtk.Box(css_classes=["linked"], valign=Gtk.Align.CENTER)
        modes = set(annotations.get("Enum", {}).values())
        first = None
        self.mode_buttons = {}
        for mode in MODE_BUTTON_ORDER:
            if mode not in modes:
                continue
            button = Gtk.ToggleButton(label=_(mode), css_classes=["mode-button"])
            button.set_group(first)
            first = first or button
            button.set_active(mode == shown_mode(self.value(key)))
            button.connect("toggled", self._on_mode, key, mode)
            box.append(button)
            self.mode_buttons[mode] = button
        row.add_suffix(box)
        self.rows[key] = row
        self._show_mode_hint(shown_mode(self.value(key)))
        return row

    def _show_mode_hint(self, mode):
        self.rows["Mode"].set_subtitle(_(MODE_HINTS[mode]) if mode in MODE_HINTS else "")

    def _on_mode(self, button, key, mode):
        if button.get_active():
            self._show_mode_hint(mode)
            self._changed(key, mode)

    def _combo_row(self, key, label, enum):
        values = [
            str(enum[k]) for k in sorted(enum, key=lambda k: int(k) if str(k).isdigit() else k)
        ]
        row = _hinted(Adw.ComboRow(title=_(label)), key)
        _show_value_whole(row)
        row.set_model(Gtk.StringList.new([self._shown(key, v) for v in values]))
        if self.value(key) in values:
            row.set_selected(values.index(self.value(key)))
        row.values = values
        row.connect("notify::selected", self._on_combo, key)
        self.rows[key] = row
        return row

    def _on_combo(self, row, _param, key):
        self._changed(key, row.values[row.get_selected()])

    def _show_example(self, combo):
        self.example = Gtk.Label(css_classes=["typed"], valign=Gtk.Align.CENTER)

        def show(*_args):
            self.example.set_text(TYPING_EXAMPLES.get(combo.values[combo.get_selected()], ""))

        combo.connect("notify::selected", show)
        show()
        combo.add_suffix(self.example)

    def _switch_row(self, key, label):
        row = _hinted(Adw.SwitchRow(title=self._switch_title(key, label)), key)
        row.set_active(self.value(key).lower() == "true")
        row.connect("notify::active", self._on_switch, key)
        self.rows[key] = row
        return row

    def _on_switch(self, row, _param, key):
        self._changed(key, "True" if row.get_active() else "False")


class TypingPage(SettingsPage):
    """tip, if given, shows above the settings; on_tip(…) runs when it is clicked.

    show_message(text, undo) shows a toast.
    """

    def __init__(self, model, on_change, tip=None, on_tip=None, show_message=None):
        self.tip = tip
        self.on_tip = on_tip
        self.show_message = show_message
        self.custom_note = status_row()
        self.retone_note = status_row()
        super().__init__(
            model, on_change, SettingsCategory.TYPING, _("Typing"), "input-keyboard-symbolic"
        )
        self.teencode = TeencodeApps(model.dbus)
        self.teencode_group = Adw.PreferencesGroup(title=_("Teencode"))
        self.teencode_apps = Gtk.ListBox(
            selection_mode=Gtk.SelectionMode.NONE, css_classes=["boxed-list"]
        )
        # App names are the user's own entries, not settings to find.
        self.teencode_apps.searchable = False
        self.teencode_group.add(self.teencode_apps)
        self.page.add(self.teencode_group)
        try_it = Adw.PreferencesGroup()
        entry = Adw.EntryRow(title=_("Type here to test"))
        entry.add_css_class("typed")
        try_it.add(entry)
        self.page.add(try_it)
        self.refresh_status()

    def _rows_for(self, item):
        rows = super()._rows_for(item)
        if item[0] == "InputMethod":
            rows.append(self.custom_note)
        elif item[0] == "RetoneSelection":
            rows.append(self.retone_note)
        return rows

    def refresh_status(self):
        """Shows the notes that depend on what fcitx5 holds, without rebuilding the page."""
        model = page_status.read(self.model.dbus)
        if model and not self.model.modified:
            self.model.follow(model)
            self._show_values()
        show_status(
            self.custom_note,
            page_status.custom_without_keys_line(model, self.model.dbus) if model else "",
        )
        show_status(self.retone_note, page_status.retone_line(model) if model else "")
        # The keyboard may have turned teencode on in an app since the page was built.
        self.teencode.apps = self.teencode.read()
        self._show_teencode()

    def _show_teencode(self):
        self.teencode_group.set_description(
            toggle_hint(self.value_of("ModeMenuKey"), self.value_of("ShortcutTeencode"))
        )
        self.teencode_apps.remove_all()
        for app in self.teencode.apps:
            row = Adw.ActionRow(title=app, use_markup=False)
            button = Gtk.Button(label=_("Turn off"), valign=Gtk.Align.CENTER)
            button.update_property(
                [Gtk.AccessibleProperty.LABEL], [_("Turn teencode off in {}").format(app)]
            )
            button.connect("clicked", lambda _b, app=app: self.teencode_off(app))
            row.add_suffix(button)
            self.teencode_apps.append(row)
        if not self.teencode.apps:
            empty = Adw.ActionRow(title=_("Not on in any app."), css_classes=["dim-label"])
            empty.set_activatable(False)
            self.teencode_apps.append(empty)

    def teencode_off(self, app):
        if not self.teencode.turn_off(app) and self.show_message:
            self.show_message(_("Could not save. Check that fcitx5 is running."), None)
        self._show_teencode()

    def _add_header(self):
        if self.tip is None:
            return
        group = Adw.PreferencesGroup()
        row = Adw.ActionRow(
            title=_("Tip: {}").format(tip_text(self.tip)), use_markup=False, activatable=True
        )
        row.add_suffix(Gtk.Image(icon_name="go-next-symbolic"))
        if self.on_tip:
            row.connect("activated", lambda _r: self.on_tip())
        self.tip_row = row
        group.add(row)
        self.page.add(group)


class GuessingPage(SettingsPage):
    """show_message(text, undo) shows a toast."""

    def __init__(self, model, on_change, show_message=None):
        self.show_message = show_message
        self.learned = Learned(model.dbus)
        super().__init__(
            model,
            on_change,
            SettingsCategory.GUESSING,
            _("Word guessing"),
            "tools-check-spelling-symbolic",
        )
        # Text typed in this window since, such as on the Typing page, is learned too.
        self.page.connect("map", lambda _page: self._show_learned())
        self.refresh_status()

    def _add_header(self):
        self.table_note = status_row()
        group = Adw.PreferencesGroup()
        group.add(self.table_note)
        self.page.add(group)

    def _add_extras(self, title, group):
        if title != LEARNING_GROUP:
            return
        # Not in DEPENDS: what was learned can be seen and forgotten with guessing off.
        self.learned_row = Adw.ActionRow(title=_("Learned"), use_markup=False)
        self.view_button = Gtk.Button(label=_("View"), valign=Gtk.Align.CENTER)
        self.view_button.connect("clicked", lambda _b: self.toggle_view())
        forget = Gtk.Button(label=_("Forget all…"), valign=Gtk.Align.CENTER)
        forget.connect("clicked", lambda _b: self.confirm_forget())
        self.learned_row.add_suffix(self.view_button)
        self.learned_row.add_suffix(forget)
        group.add(self.learned_row)
        self.learned_text = Gtk.TextView(
            editable=False,
            cursor_visible=False,
            monospace=True,
            top_margin=8,
            bottom_margin=8,
            left_margin=8,
            right_margin=8,
        )
        self.learned_text.update_property(
            [Gtk.AccessibleProperty.LABEL], [_("What Đoán dấu learned")]
        )
        self.learned_view = Gtk.ScrolledWindow(
            child=self.learned_text,
            min_content_height=160,
            max_content_height=320,
            propagate_natural_height=True,
            css_classes=["card"],
            margin_top=12,
            visible=False,
        )
        self.learned_view.searchable = False
        group.add(self.learned_view)
        self.how_it_learns = Adw.ExpanderRow(
            title=_("How it learns and where it is kept"), use_markup=False
        )
        self.how_it_learns_lines = []
        for text in how_it_learns():
            label = Gtk.Label(
                label=text, wrap=True, xalign=0, selectable=True, margin_top=8, margin_bottom=8
            )
            label.set_margin_start(12)
            label.set_margin_end(12)
            self.how_it_learns.add_row(label)
            self.how_it_learns_lines.append(label)
        group.add(self.how_it_learns)

    def refresh_status(self):
        super().refresh_status()
        self._show_learned()

    def _show_learned(self):
        status = self.learned.status()
        self.learned_row.set_subtitle(status.line)
        show_status(
            self.table_note,
            _("The guessing table is not installed, so Đoán dấu cannot add marks yet.")
            if status.table_missing
            else "",
        )
        if self.learned_view.get_visible():
            self._fill_view()

    def _fill_view(self):
        text, failed = self.learned.view()
        if failed:
            self._hide_view()
            self._message(failed)
            return
        self.learned_text.get_buffer().set_text(text)
        self.learned_view.set_visible(True)
        self.view_button.set_label(_("Hide"))

    def _hide_view(self):
        self.learned_view.set_visible(False)
        self.learned_text.get_buffer().set_text("")
        self.view_button.set_label(_("View"))

    def toggle_view(self):
        if self.learned_view.get_visible():
            self._hide_view()
        else:
            self._fill_view()

    def confirm_forget(self):
        dialog = Adw.AlertDialog(
            heading=_("Forget what Đoán dấu learned?"),
            body=_(
                "Đoán dấu will guess from its table alone, as on a new machine."
                " This cannot be undone."
            ),
        )
        dialog.add_response("cancel", _("&Cancel").replace("&", "_"))
        dialog.add_response("forget", _("Forget all"))
        dialog.set_response_appearance("forget", Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.set_default_response("cancel")
        dialog.set_close_response("cancel")
        dialog.connect("response", lambda _d, response: response == "forget" and self.forget())
        dialog.present(self.page.get_root())
        return dialog

    def forget(self):
        _forgot, line = self.learned.forget()
        self._message(line)
        self._show_learned()

    def _message(self, text):
        if self.show_message:
            self.show_message(text, None)


def _label(text, *css, markup=False):
    label = Gtk.Label(wrap=True, justify=Gtk.Justification.CENTER, max_width_chars=50)
    if markup:
        label.set_markup(text)
    else:
        label.set_text(text)
    for name in css:
        label.add_css_class(name)
    return label


def _link_button(label, uri):
    button = Gtk.Button(label=label, css_classes=["pill"], halign=Gtk.Align.CENTER)
    button.connect("clicked", lambda b: open_uri(b, uri))
    return button


def about_page(version):
    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
    box.set_margin_top(24)
    box.set_margin_bottom(24)
    box.append(Gtk.Image(icon_name=brand.LOGO, pixel_size=80))
    box.append(_label("Ngó Sen", "wordmark"))
    box.append(_label(_("Version {}").format(version), "dim-label"))
    box.append(
        _label(_("Vietnamese input method for fcitx5 on Linux that types like UniKey on Windows."))
    )
    box.append(_label(f'<a href="{REPO_URL}">{REPO_URL}</a>', markup=True))
    box.append(_link_button(_("Request Feature"), FEATURE_REQUEST_URL))
    box.append(Gtk.Separator())
    box.append(_label(_("Developed by Nguyen Phi")))
    box.append(Gtk.Separator())
    box.append(_label(_("Licensed under the GNU General Public License v3.0"), "dim-label"))
    clamp = Adw.Clamp(maximum_size=600, child=box)
    return Gtk.ScrolledWindow(child=clamp, hscrollbar_policy=Gtk.PolicyType.NEVER)
