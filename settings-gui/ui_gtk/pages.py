# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The pages of the GTK window, drawn from the same catalog as the Qt window."""

from core.catalog import (
    HINTS,
    MODE_BUTTON_ORDER,
    SETTINGS_MAP,
    TYPING_EXAMPLES,
    SettingsCategory,
)
from gi.repository import Adw, Gtk
from i18n import _

from core import system_check
from ui_gtk import brand

REPO_URL = "https://github.com/ngosen/ngosen"


def _hinted(row, key):
    row.set_use_markup(False)
    if key in HINTS:
        row.set_subtitle(_(HINTS[key]))
    return row


class SettingsPage:
    """One category of the catalog as rows; each change goes to on_change(key, value) at once."""

    def __init__(self, model, on_change, category, title, icon):
        self.model = model
        self.on_change = on_change
        self.rows = {}
        self.page = Adw.PreferencesPage(title=title, icon_name=icon)
        self._add_header()
        for title, keys in SETTINGS_MAP[category].items():
            group = Adw.PreferencesGroup(title=_(title))
            for key in keys:
                item = model.metadata.get(key)
                if item:
                    for row in self._rows_for(item):
                        group.add(row)
            self._add_extras(title, group)
            self.page.add(group)

    def _add_header(self):
        """Rows a page shows above its settings."""

    def _add_extras(self, title, group):
        """Rows a page adds after a group's settings."""

    def control_for(self, key):
        """The row that edits a setting, for the window to show and focus."""
        return self.rows.get(key)

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
            return [row, self._example_row(row)] if key == "InputMethod" else [row]
        if kind == "Boolean":
            return [self._switch_row(key, label)]
        return []

    def _mode_row(self, key, label, annotations):
        row = _hinted(Adw.ActionRow(title=_(label)), key)
        box = Gtk.Box(css_classes=["linked"], valign=Gtk.Align.CENTER)
        modes = set(annotations.get("Enum", {}).values())
        first = None
        for mode in MODE_BUTTON_ORDER:
            if mode not in modes:
                continue
            button = Gtk.ToggleButton(label=_(mode), css_classes=["mode-button"])
            button.set_group(first)
            first = first or button
            button.set_active(mode == self.value(key))
            button.connect("toggled", self._on_mode, key, mode)
            box.append(button)
        row.add_suffix(box)
        self.rows[key] = row
        return row

    def _on_mode(self, button, key, mode):
        if button.get_active():
            self.on_change(key, mode)

    def _combo_row(self, key, label, enum):
        values = [
            str(enum[k]) for k in sorted(enum, key=lambda k: int(k) if str(k).isdigit() else k)
        ]
        row = _hinted(Adw.ComboRow(title=_(label)), key)
        row.set_model(Gtk.StringList.new([self._shown(key, v) for v in values]))
        if self.value(key) in values:
            row.set_selected(values.index(self.value(key)))
        row.values = values
        row.connect("notify::selected", self._on_combo, key)
        self.rows[key] = row
        return row

    def _on_combo(self, row, _param, key):
        self.on_change(key, row.values[row.get_selected()])

    def _example_row(self, combo):
        row = Adw.ActionRow(css_classes=["typed"])
        row.set_use_markup(False)

        def show(*_args):
            text = TYPING_EXAMPLES.get(combo.values[combo.get_selected()], "")
            row.set_title(text)
            row.set_visible(bool(text))

        combo.connect("notify::selected", show)
        show()
        return row

    def _switch_row(self, key, label):
        row = _hinted(Adw.SwitchRow(title=self._switch_title(key, label)), key)
        row.set_active(self.value(key).lower() == "true")
        row.connect("notify::active", self._on_switch, key)
        self.rows[key] = row
        return row

    def _on_switch(self, row, _param, key):
        self.on_change(key, "True" if row.get_active() else "False")


class TypingPage(SettingsPage):
    """tip, if given, shows above the settings; on_tip(…) runs when it is clicked."""

    def __init__(self, model, on_change, tip=None, on_tip=None):
        self.tip = tip
        self.on_tip = on_tip
        super().__init__(
            model, on_change, SettingsCategory.TYPING, _("Typing"), "input-keyboard-symbolic"
        )
        try_it = Adw.PreferencesGroup()
        entry = Adw.EntryRow(title=_("Type here to test"))
        entry.add_css_class("typed")
        try_it.add(entry)
        self.page.add(try_it)

    def _add_header(self):
        if self.tip is None:
            return
        group = Adw.PreferencesGroup()
        row = Adw.ActionRow(
            title=_("Tip: {}").format(_(self.tip["text"])), use_markup=False, activatable=True
        )
        row.add_suffix(Gtk.Image(icon_name="go-next-symbolic"))
        if self.on_tip:
            row.connect("activated", lambda _r: self.on_tip())
        self.tip_row = row
        group.add(row)
        self.page.add(group)


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
    button = Gtk.Button(label=label, css_classes=["pill"])
    button.connect("clicked", lambda b: Gtk.UriLauncher(uri=uri).launch(b.get_root(), None, None))
    return button


def about_page(version):
    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
    box.set_margin_top(24)
    box.set_margin_bottom(24)
    box.append(Gtk.Image(icon_name=brand.LOGO, pixel_size=80))
    box.append(_label(brand.WORDMARK, "wordmark"))
    box.append(_label(_("Version {}").format(version), "dim-label"))
    if system_check.runs_replaced_addon():
        box.append(
            _label(
                _(
                    "Typing still uses the Ngó Sen that ran before the update. "
                    "Log out and back in to start version {}."
                ).format(version),
                "error",
            )
        )
    box.append(
        _label(_("Vietnamese input method for fcitx5 on Linux that types like UniKey on Windows."))
    )
    box.append(_label(f'<a href="{REPO_URL}">{REPO_URL}</a>', markup=True))
    buttons = Gtk.Box(spacing=12, halign=Gtk.Align.CENTER)
    buttons.append(_link_button(_("Report Bug"), f"{REPO_URL}/issues/new?template=bug_report.yml"))
    buttons.append(
        _link_button(_("Request Feature"), f"{REPO_URL}/issues/new?template=feature_request.yml")
    )
    box.append(buttons)
    box.append(Gtk.Separator())
    box.append(_label(_("Developed by Nguyen Phi")))
    box.append(Gtk.Separator())
    box.append(_label(_("Licensed under the GNU General Public License v3.0"), "dim-label"))
    clamp = Adw.Clamp(maximum_size=600, child=box)
    return Gtk.ScrolledWindow(child=clamp, hscrollbar_policy=Gtk.PolicyType.NEVER)
