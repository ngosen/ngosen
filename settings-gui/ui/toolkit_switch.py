# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The button that reopens the settings in the GTK window, installing it first when needed."""

import threading

from core.toolkit_install import Offer, format_size, switch
from i18n import _
from qtpy.QtCore import QObject, QProcess, Signal
from qtpy.QtWidgets import QApplication, QHBoxLayout, QLabel, QMessageBox, QPushButton, QWidget


class _Results(QObject):
    """Hands what a thread found to the main thread."""

    ready = Signal(object)

    def __init__(self):
        super().__init__()
        self.ready.connect(lambda done: done[0](done[1]))


_results = []


def _in_background(work, then):
    # Made on the main thread and kept for the app's life, so no thread deletes a Qt object.
    if not _results:
        _results.append(_Results())
    results = _results[0]
    threading.Thread(target=lambda: results.ready.emit((then, work())), daemon=True).start()


def toolkit_field(model):
    field = QWidget()
    layout = QHBoxLayout(field)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.addWidget(QLabel(_("This window uses {}").format("Qt")))
    button = QPushButton(_("Switch to {}…").format("GTK"))
    button.clicked.connect(lambda: _check(button, model))
    layout.addWidget(button)
    layout.addStretch()
    return field, button


def _check(button, model):
    button.setEnabled(False)

    def checked(offer):
        try:
            button.setEnabled(True)
        except RuntimeError:
            return  # The window closed meanwhile.
        _offer(button, model, offer)

    _in_background(lambda: Offer("qt"), checked)


def _offer(button, model, offer):
    if offer.ready:
        _switch(button, model, offer)
        return
    box = QMessageBox(button.window())
    box.setWindowTitle(_("Switch to {}").format(offer.name))
    box.setText(_("Switch to {}").format(offer.name))
    if not offer.installable:
        box.setInformativeText(
            _("Install the {} window with your package manager.").format(offer.name)
        )
        box.addButton(QMessageBox.Close)
        box.open()
        return
    needs = _("The {} window needs these packages: {}.").format(
        offer.name, ", ".join(offer.packages)
    )
    box.setInformativeText(needs + "\n" + _("Checking the download size…"))
    install = box.addButton(_("Install"), QMessageBox.AcceptRole) if offer.runnable else None
    copy = box.addButton(_("Copy Command"), QMessageBox.ActionRole)
    box.addButton(QMessageBox.Cancel)
    if install:
        box.setDefaultButton(install)

    def show_size(size):
        text = (
            needs
            if size is None
            else needs + "\n" + _("Download: about {}.").format(format_size(size))
        )
        try:
            box.setInformativeText(text)
        except RuntimeError:
            pass  # The window closed before the package manager answered.

    def clicked(chosen):
        if chosen is copy:
            QApplication.clipboard().setText(offer.command)
        elif chosen is install:
            _install(button, model, offer)

    _in_background(offer.download_size, show_size)
    box.buttonClicked.connect(clicked)
    box.open()


def _install(button, model, offer):
    button.setEnabled(False)
    button.setText(_("Installing…"))
    process = QProcess(button)

    def finished(code):
        button.setEnabled(True)
        button.setText(_("Switch to {}…").format(offer.name))
        # 126 and 127: the password prompt was closed or refused.
        if code not in (0, 126, 127):
            _failed(button, offer)
        elif code == 0:
            _in_background(offer.installed_now, lambda ok: _installed(ok, button, model, offer))

    process.finished.connect(lambda code, _status: finished(code))
    process.errorOccurred.connect(lambda error: error == QProcess.FailedToStart and finished(-1))
    process.start(offer.install_argv[0], offer.install_argv[1:])


def _installed(ok, button, model, offer):
    if ok:
        _switch(button, model, offer)
    else:
        _failed(button, offer)


def _failed(button, offer):
    box = QMessageBox(button.window())
    box.setIcon(QMessageBox.Warning)
    box.setWindowTitle(_("Could not install"))
    box.setText(_("Could not install"))
    box.setInformativeText(_("Run this command in a terminal:") + "\n" + offer.command)
    copy = box.addButton(_("Copy Command"), QMessageBox.ActionRole)
    box.addButton(QMessageBox.Close)
    box.buttonClicked.connect(
        lambda chosen: chosen is copy and QApplication.clipboard().setText(offer.command)
    )
    box.open()


def _switch(button, model, offer):
    if switch(model, offer.target):
        # Closing saves any change still waiting, then ends the main loop.
        button.window().close()
        return
    QMessageBox.warning(button.window(), _("Could not switch"), _("Check that fcitx5 is running."))
