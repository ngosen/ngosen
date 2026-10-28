# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Shows what is wrong with this machine's fcitx5 setup and where to send the result."""

from core.brand import MESSENGER_URL
from gi.repository import Adw, Gdk, Gtk
from i18n import _

from core import system_check, typing_log

ICONS = {
    system_check.OK: ("emblem-ok-symbolic", "success"),
    system_check.WARN: ("dialog-warning-symbolic", "warning"),
    system_check.FAIL: ("dialog-error-symbolic", "error"),
}


def open_uri(widget, uri):
    Gtk.UriLauncher(uri=uri).launch(widget.get_root(), None, None)


def _text(css=None):
    label = Gtk.Label(wrap=True, xalign=0)
    if css:
        label.add_css_class(css)
    return label


def _button(label, on_click, *css):
    button = Gtk.Button(label=label, css_classes=list(css))
    button.connect("clicked", lambda _b: on_click())
    return button


class CheckDialog:
    def __init__(self, dbus, ngosen_version=""):
        self.dbus = dbus
        self.ngosen_version = ngosen_version
        self.app_test = None
        self.rows = []
        page = Adw.PreferencesPage()
        self.results = Adw.PreferencesGroup(
            description=_(
                "How fcitx5 is set up on this machine. Nothing is sent until you send a bug "
                "report, and it lists the apps below, so read it before sending."
            )
        )
        self.apps = _text("dim-label")
        self.apps.set_margin_top(12)
        self.results.add(self.apps)
        page.add(self.results)
        page.add(self._app_test_group())
        page.add(self._send_group())

        header = Adw.HeaderBar()
        again = Gtk.Button(icon_name="view-refresh-symbolic", tooltip_text=_("Check again"))
        again.connect("clicked", lambda _b: self.run())
        header.pack_start(again)
        view = Adw.ToolbarView(content=page)
        view.add_top_bar(header)
        self.dialog = Adw.Dialog(
            title=_("Check my machine"), child=view, content_width=560, content_height=640
        )
        self.run()

    def _app_test_group(self):
        group = Adw.PreferencesGroup(
            title=_("Test an app"),
            description=_(
                "Open the app, switch to Ngó Sen and type “{sample}” ({telex} in Telex). Then "
                "choose “Save typing log” in the Ngó Sen menu and read the log here."
            ).format(sample=typing_log.SAMPLE, telex=typing_log.SAMPLE_TELEX),
        )
        row = Gtk.Box(spacing=12)
        row.append(_button(_("Read typing log"), self.read_typing_log))
        self.right_btn = _button(_("It typed right"), lambda: self._set_verdict(typing_log.RIGHT))
        self.wrong_btn = _button(_("It typed wrong"), lambda: self._set_verdict(typing_log.WRONG))
        for button in (self.right_btn, self.wrong_btn):
            button.set_visible(False)
            row.append(button)
        group.add(row)
        self.app_result = _text()
        self.app_result.set_margin_top(12)
        self.app_result.set_visible(False)
        group.add(self.app_result)
        return group

    def _send_group(self):
        group = Adw.PreferencesGroup(title=_("Send the result"))
        row = Gtk.Box(spacing=12)
        self.message_btn = _button(_("Message on Facebook…"), self.open_chat, "suggested-action")
        self.report_btn = _button(_("Report on GitHub…"), self.open_report)
        self.copy_btn = _button(_("Copy result"), self.copy_report)
        for button in (self.message_btn, self.report_btn, self.copy_btn):
            row.append(button)
        group.add(row)
        self.status = _text("dim-label")
        self.status.set_margin_top(12)
        group.add(self.status)
        return group

    def run(self):
        self.facts = system_check.collect(self.dbus, self.ngosen_version)
        self.findings = system_check.evaluate(self.facts)
        for row in self.rows:
            self.results.remove(row)
        findings = self.findings or [
            system_check.Finding(system_check.OK, _("No setup problem found"))
        ]
        self.rows = [self._row(finding) for finding in findings]
        for row in self.rows:
            self.results.add(row)
        # Below the rows, as in the Qt window.
        self.results.remove(self.apps)
        self.results.add(self.apps)
        names = ", ".join(f"{p} ({fe})" for p, fe in self.facts.apps)
        self.apps.set_text(_("Apps connected to fcitx5: {}").format(names) if names else "")
        self.apps.set_visible(bool(names))

    def _row(self, finding):
        row = Adw.ActionRow(title=finding.title, subtitle=finding.fix or "", use_markup=False)
        icon, css = ICONS[finding.level]
        row.add_prefix(Gtk.Image(icon_name=icon, css_classes=[css]))
        row.level = finding.level
        return row

    def read_typing_log(self):
        self.app_test = typing_log.read_latest()
        asking = self.app_test is not None and self.app_test.verdict == typing_log.UNKNOWN
        self.right_btn.set_visible(asking)
        self.wrong_btn.set_visible(asking)
        self._show_app_test()

    def _set_verdict(self, verdict):
        self.app_test.verdict = verdict
        self.right_btn.set_visible(False)
        self.wrong_btn.set_visible(False)
        self._show_app_test()

    def _show_app_test(self):
        self.app_result.set_text(typing_log.describe(self.app_test))
        self.app_result.set_visible(True)

    def report_text(self):
        return system_check.report(self.facts, self.findings, self.app_test)

    def copy_report(self):
        Gdk.Display.get_default().get_clipboard().set(self.report_text())
        self.status.set_text(_("Copied the result."))

    def open_chat(self):
        # Messenger cannot be handed the text, so it waits on the clipboard.
        Gdk.Display.get_default().get_clipboard().set(self.report_text())
        self.status.set_text(
            _("Copied the result. Paste it into the message and say what went wrong.")
        )
        open_uri(self.dialog, MESSENGER_URL)

    def open_report(self):
        open_uri(self.dialog, system_check.issue_url(self.facts, self.report_text()))
