# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The row that reopens the settings in the Qt window, installing it first when needed."""

import threading

from core.toolkit_install import Offer, format_size, switch
from gi.repository import Adw, Gdk, Gio, GLib, Gtk
from i18n import _


def toolkit_row(model, hint):
    row = Adw.ActionRow(
        title=_("This window uses {}").format("GTK"), subtitle=_(hint), use_markup=False
    )
    button = Gtk.Button(label=_("Switch to {}…").format("Qt"), valign=Gtk.Align.CENTER)
    button.connect("clicked", lambda _b: _offer(button, model, Offer("gtk")))
    row.add_suffix(button)
    return row


def _offer(button, model, offer):
    if offer.ready:
        _switch(button, model, offer)
        return
    dialog = Adw.AlertDialog(heading=_("Switch to {}").format(offer.name))
    dialog.add_response("cancel", _("&Cancel").replace("&", "_"))
    if not offer.installable:
        dialog.set_body(_("Install the {} window with your package manager.").format(offer.name))
        dialog.present(button.get_root())
        return
    needs = _("The {} window needs these packages: {}.").format(
        offer.name, ", ".join(offer.packages)
    )
    dialog.set_body(needs + "\n" + _("Checking the download size…"))
    dialog.add_response("copy", _("Copy Command"))
    if offer.runnable:
        dialog.add_response("install", _("Install"))
        dialog.set_response_appearance("install", Adw.ResponseAppearance.SUGGESTED)
        dialog.set_default_response("install")
    dialog.connect("response", _on_response, button, model, offer)
    dialog.present(button.get_root())

    def show_size(size):
        if size is None:
            dialog.set_body(needs)
        else:
            dialog.set_body(needs + "\n" + _("Download: about {}.").format(format_size(size)))

    threading.Thread(
        target=lambda: GLib.idle_add(show_size, offer.download_size()), daemon=True
    ).start()


def _on_response(_dialog, response, button, model, offer):
    if response == "copy":
        Gdk.Display.get_default().get_clipboard().set(offer.command)
    elif response == "install":
        _install(button, model, offer)


def _install(button, model, offer):
    button.set_sensitive(False)
    button.set_label(_("Installing…"))
    process = Gio.Subprocess.new(offer.install_argv, Gio.SubprocessFlags.NONE)

    def done(process, result):
        process.wait_finish(result)
        button.set_sensitive(True)
        button.set_label(_("Switch to {}…").format(offer.name))
        if process.get_successful() and offer.installed_now():
            _switch(button, model, offer)
        # 126 and 127: the password prompt was closed or refused.
        elif process.get_exit_status() not in (126, 127):
            _failed(button, offer)

    process.wait_async(None, done)


def _failed(button, offer):
    dialog = Adw.AlertDialog(
        heading=_("Could not install"),
        body=_("Run this command in a terminal:") + "\n" + offer.command,
    )
    dialog.add_response("close", _("&OK").replace("&", "_"))
    dialog.add_response("copy", _("Copy Command"))
    dialog.connect(
        "response",
        lambda _d, response: (
            response == "copy" and Gdk.Display.get_default().get_clipboard().set(offer.command)
        ),
    )
    dialog.present(button.get_root())


def _switch(button, model, offer):
    if switch(model, offer.target):
        Gio.Application.get_default().quit()
        return
    dialog = Adw.AlertDialog(heading=_("Could not switch"), body=_("Check that fcitx5 is running."))
    dialog.add_response("close", _("&OK").replace("&", "_"))
    dialog.present(button.get_root())
