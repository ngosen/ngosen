# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Appearance page: the window language, the tray icon and what the tray menu shows."""

import os

from core.catalog import (
    HINTS,
    LANGUAGE_NAMES,
    MENU_ENTRIES,
    MENU_ICON_FALLBACKS,
    TRAY_ICON,
    TRAY_IMAGE_BUTTONS,
    TRAY_IMAGE_KEYS,
    TRAY_KEYS,
    SettingsCategory,
)
from gi.repository import Adw, Gdk, Gio, GLib, Gtk
from i18n import _

from core import tray_image
from ui_gtk.pages import SettingsPage
from ui_gtk.toolkit_switch import toolkit_row


class AppearancePage(SettingsPage):
    def __init__(self, model, on_change):
        self.preview = None
        self.image_shown = {}

        def changed(key, value):
            on_change(key, value)
            if key in TRAY_KEYS:
                self.update_preview()

        super().__init__(
            model,
            changed,
            SettingsCategory.APPEARANCE,
            _("Appearance"),
            "preferences-desktop-appearance-symbolic",
        )
        Adw.StyleManager.get_default().connect("notify::dark", lambda *_a: self.update_preview())
        self.update_preview()

    def _shown(self, key, value):
        # Each language is named in itself, so it can be found whichever one the window is in.
        return LANGUAGE_NAMES.get(value, value) if key == "UiLanguage" else _(value)

    def _rows_for(self, item):
        if item[0] == "SettingsToolkit":
            self.rows[item[0]] = toolkit_row(self.model, HINTS[item[0]])
            return [self.rows[item[0]]]
        if item[0] in TRAY_IMAGE_KEYS:
            return [self._image_row(item[0], item[2])]
        return super()._rows_for(item)

    def _image_row(self, key, label):
        row = Adw.ActionRow(title=_(label), use_markup=False)
        if key in HINTS:
            row.set_subtitle(_(HINTS[key]))
        shown = Gtk.Box(valign=Gtk.Align.CENTER)
        # Icon buttons keep the row inside the narrowest window.
        pick = Gtk.Button(
            icon_name="document-open-symbolic",
            tooltip_text=_("Choose…"),
            valign=Gtk.Align.CENTER,
            css_classes=["flat"],
        )
        clear = Gtk.Button(
            icon_name="edit-clear-symbolic",
            tooltip_text=_("Remove"),
            valign=Gtk.Align.CENTER,
            css_classes=["flat"],
        )
        pick.connect("clicked", lambda _b: self._choose_image(key, label))
        clear.connect("clicked", lambda _b: self._changed(key, ""))
        for button, name in zip((pick, clear), TRAY_IMAGE_BUTTONS[key]):
            button.update_property([Gtk.AccessibleProperty.LABEL], [_(name)])
        for widget in (shown, pick, clear):
            row.add_suffix(widget)
        self.image_shown[key] = (shown, clear)
        self.rows[key] = row
        return row

    def _choose_image(self, key, label):
        images = Gtk.FileFilter(name=_("Images"))
        images.add_suffix("svg")
        images.add_suffix("png")
        filters = Gio.ListStore.new(Gtk.FileFilter)
        filters.append(images)
        dialog = Gtk.FileDialog(title=_(label), modal=True, filters=filters, default_filter=images)

        def done(_dialog, result):
            try:
                file = dialog.open_finish(result)
            except GLib.Error:
                return  # Cancelled.
            if file is not None and file.get_path():
                self.use_image(key, file.get_path())

        dialog.open(self.page.get_root(), None, done)

    def use_image(self, key, path):
        """Copies the picked image where trays find it and points the setting at the copy."""
        try:
            saved = str(self.model.saved.get(key, ""))
            copy = tray_image.install(path, TRAY_IMAGE_KEYS[key], keep={saved})
        except (OSError, ValueError) as e:
            self.rows[key].set_subtitle(_("Could not use this image: {}").format(e))
            return
        self._changed(key, copy)

    def _show_images(self):
        for key, (shown, clear) in self.image_shown.items():
            path = str(self.model.value_of(key) or "")
            while child := shown.get_first_child():
                shown.remove(child)
            if path:
                shown.append(_tray_icon(path))
            else:
                shown.append(Gtk.Label(label=_("Default"), css_classes=["dim-label"]))
            clear.set_sensitive(bool(path))

    def _show_values(self):
        super()._show_values()
        self.update_preview()

    def _switch_title(self, key, label):
        return _(MENU_ENTRIES[key][0]) if key in MENU_ENTRIES else _(label)

    def _switch_row(self, key, label):
        row = super()._switch_row(key, label)
        if key in MENU_ENTRIES:
            row.add_prefix(_menu_icon(MENU_ENTRIES[key][1]))
        return row

    def _add_extras(self, title, group):
        if title != TRAY_ICON:
            return
        self.preview = Gtk.Box(spacing=8, css_classes=["tray-preview"], valign=Gtk.Align.CENTER)
        self.preview.update_property(
            [Gtk.AccessibleProperty.LABEL], [_("Tray icons for Vietnamese and English")]
        )
        row = Adw.ActionRow(
            title=_("In the tray:").rstrip(":"), subtitle=_("Vietnamese, English"), use_markup=False
        )
        row.add_suffix(self.preview)
        group.add(row)

    def update_preview(self):
        self._show_images()
        if self.preview is None:
            return
        names, dark = self.model.tray_icons(Adw.StyleManager.get_default().get_dark())
        while child := self.preview.get_first_child():
            self.preview.remove(child)
        for name in names:
            self.preview.append(_tray_icon(name))
        self.preview.remove_css_class("tray-light" if dark else "tray-dark")
        self.preview.add_css_class("tray-dark" if dark else "tray-light")


def _theme():
    return Gtk.IconTheme.get_for_display(Gdk.Display.get_default())


def _menu_icon(name):
    # Themes like Adwaita ship only the symbolic variant.
    for candidate in (name, f"{name}-symbolic", MENU_ICON_FALLBACKS.get(name)):
        if candidate and _theme().has_icon(candidate):
            return Gtk.Image(icon_name=candidate)
    # Keeps the text in line with the entries that have an icon.
    blank = Gtk.Image()
    blank.set_size_request(16, 16)
    return blank


def _tray_icon(name, size=22):
    """The theme's icon, or the installed hicolor file if the theme lacks it."""
    if os.path.isabs(name):
        return Gtk.Image(file=name, pixel_size=size)
    # Checked first: a missing name would be shortened, and themes with an "fcitx" icon show that.
    if _theme().has_icon(name):
        return Gtk.Image(icon_name=name, pixel_size=size)
    for folder in [GLib.get_user_data_dir(), *GLib.get_system_data_dirs()]:
        path = os.path.join(folder, "icons/hicolor/scalable/apps", f"{name}.svg")
        if os.path.exists(path):
            return Gtk.Image(file=path, pixel_size=size)
    return Gtk.Image(pixel_size=size)
