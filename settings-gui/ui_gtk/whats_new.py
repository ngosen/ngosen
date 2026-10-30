# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The "What's new & tips" page: recent features and typing tips, each with a way to its setting."""

from core.whats_new_content import FEATURES, TIPS
from gi.repository import Adw, Gtk
from i18n import _

from ui_gtk.focus import focus_when_laid_out

# Sidebar pages the tips point at, named as they are in the window.
PAGE_TITLES = {"macros": _("Macros"), "check": _("Check my machine")}


def tip_text(tip):
    return _(tip.get("gtk_text", tip["text"]))


class WhatsNewPage:
    """open_setting(key) and show_page(name) take the buttons to what they name."""

    def __init__(self, open_setting, show_page):
        self.open_setting = open_setting
        self.show_page = show_page
        self.page = Adw.PreferencesPage(
            title=_("What's new & tips"), icon_name="dialog-information-symbolic"
        )
        groups = {}
        for feature in FEATURES:
            version = feature["version"]
            if version not in groups:
                groups[version] = Adw.PreferencesGroup(title=_("New in {}").format(version))
                self.page.add(groups[version])
            for row in self._feature_rows(feature):
                groups[version].add(row)
        tips = Adw.PreferencesGroup(title=_("Tips"))
        self.tip_rows = []
        for tip in TIPS:
            row = Adw.ActionRow(title=tip_text(tip), use_markup=False)
            self._add_button(row, tip["target"])
            self.tip_rows.append(row)
            tips.add(row)
        self.page.add(tips)

    def _feature_rows(self, feature):
        row = Adw.ActionRow(title=_(feature["title"]), subtitle=_(feature["how"]), use_markup=False)
        self._add_button(row, feature["target"])
        if not feature["example"]:
            return [row]
        example = Adw.ActionRow(title=feature["example"], use_markup=False, css_classes=["typed"])
        return [row, example]

    def _add_button(self, row, target):
        if target is None:
            return
        kind, name = target
        if kind == "setting":
            button = Gtk.Button(label=_("Open setting"), valign=Gtk.Align.CENTER)
            button.connect("clicked", lambda _b: self.open_setting(name))
        else:
            label = _("Open the {} page").format(PAGE_TITLES[name])
            button = Gtk.Button(label=label, valign=Gtk.Align.CENTER)
            button.connect("clicked", lambda _b: self.show_page(name))
        button.target = target
        row.add_suffix(button)

    def show_tips(self):
        # Scrolling follows the focus, so focusing the first tip brings the list into view.
        focus_when_laid_out(self.tip_rows[0])
