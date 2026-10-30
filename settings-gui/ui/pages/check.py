# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Check my machine page: each setup step, with a fix where the window can do it."""

from core.brand import MESSENGER_URL
from i18n import _
from qtpy.QtCore import Qt, QTimer, QUrl
from qtpy.QtGui import QDesktopServices, QGuiApplication
from qtpy.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QProgressBar,
    QPushButton,
    QScrollArea,
    QStyle,
    QVBoxLayout,
    QWidget,
)

from core import system_check, typing_log
from ui import brand
from ui.pages.dynamic_settings import CardWidget
from ui.typography import emphasize, mute

ICONS = {
    system_check.OK: QStyle.SP_DialogApplyButton,
    system_check.WARN: QStyle.SP_MessageBoxWarning,
    system_check.FAIL: QStyle.SP_MessageBoxCritical,
}


def _wrapped(text="", parent_layout=None):
    label = QLabel(text)
    label.setWordWrap(True)
    if parent_layout is not None:
        parent_layout.addWidget(label)
    return label


def _button(text, on_click):
    button = QPushButton(text)
    button.clicked.connect(on_click)
    return button


class CheckPage(QWidget):
    """Read-only, so the window neither saves it nor searches it for settings."""

    searchable = False

    def __init__(self, dbus_handler, ngosen_version, notify, on_count, parent=None):
        """notify(text, undo) shows a message; on_count(n) gets the number of steps left."""
        super().__init__(parent)
        self.dbus_handler = dbus_handler
        self.ngosen_version = ngosen_version
        self.notify = notify
        self.on_count = on_count
        self.app_test = None
        self.fixes = system_check.Fixes(dbus_handler)
        self.rows = []
        self._setup_ui()
        self.run()

    def _setup_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setContentsMargins(30, 20, 30, 20)
        layout.setSpacing(16)

        self.heading = emphasize(_wrapped(), 1.3)
        self.heading.setObjectName("CategoryTitle")
        layout.addWidget(self.heading)
        self.summary = mute(_wrapped("", layout))
        buttons = QHBoxLayout()
        self.fix_all_btn = _button("", self.fix_all)
        self.fix_all_btn.setDefault(True)
        self.again_btn = _button(_("Check again"), self.run)
        buttons.addWidget(self.fix_all_btn)
        buttons.addWidget(self.again_btn)
        buttons.addStretch()
        layout.addLayout(buttons)

        layout.addWidget(self._heading(_("Steps to type Vietnamese")))
        self.steps = CardWidget()
        layout.addWidget(self.steps)

        layout.addWidget(self._heading(_("Try typing here")))
        layout.addWidget(mute(_wrapped(_("In Telex, vieetj becomes việt."))))
        try_it = QLineEdit()
        try_it.setPlaceholderText(_("Type here to test"))
        try_it.setFont(brand.typed_font(17))
        layout.addWidget(try_it)

        layout.addWidget(self._heading(_("Test an app")))
        layout.addWidget(self._app_test_card())

        layout.addWidget(self._heading(_("Apps connected to fcitx5")))
        layout.addWidget(
            mute(
                _wrapped(
                    _(
                        "An app missing here has not connected to the input method: open it "
                        "again, or send the result to Ngó Sen."
                    )
                )
            )
        )
        self.apps = _wrapped()
        self.apps.setObjectName("CheckApps")
        layout.addWidget(self.apps)

        layout.addWidget(self._heading(_("Send the result to Ngó Sen")))
        layout.addWidget(
            mute(
                _wrapped(
                    _(
                        "The result lists the steps above, the apps connected and the typing log "
                        "you read. Nothing is sent until you press a button below."
                    )
                )
            )
        )
        send = QHBoxLayout()
        self.message_btn = _button(_("Message on Facebook…"), self.open_chat)
        self.report_btn = _button(_("Report on GitHub…"), self.open_report)
        self.copy_btn = _button(_("Copy result"), self.copy_report)
        for button in (self.message_btn, self.report_btn, self.copy_btn):
            send.addWidget(button)
        send.addStretch()
        layout.addLayout(send)
        layout.addStretch()
        scroll.setWidget(content)
        root.addWidget(scroll)

    @staticmethod
    def _heading(text):
        return emphasize(QLabel(text), 1.15)

    def _app_test_card(self):
        card = CardWidget()
        _wrapped(
            _(
                "Open the app, switch to Ngó Sen and type “{sample}” ({telex} in Telex). Then "
                "choose “Save typing log” in the Ngó Sen menu and read the log here."
            ).format(sample=typing_log.SAMPLE, telex=typing_log.SAMPLE_TELEX),
            card.content_layout,
        )
        row = QHBoxLayout()
        self.read_log_btn = _button(_("Read typing log"), self.read_typing_log)
        self.right_btn = _button(_("It typed right"), lambda: self._set_verdict(typing_log.RIGHT))
        self.wrong_btn = _button(_("It typed wrong"), lambda: self._set_verdict(typing_log.WRONG))
        row.addWidget(self.read_log_btn)
        for button in (self.right_btn, self.wrong_btn):
            button.hide()
            row.addWidget(button)
        row.addStretch()
        card.content_layout.addLayout(row)
        self.app_result = _wrapped("", card.content_layout)
        self.app_result.setObjectName("AppTestResult")
        self.app_result.hide()
        return card

    def run(self):
        self.facts = system_check.collect(self.dbus_handler, self.ngosen_version)
        self.found = system_check.steps(self.facts)
        self.findings = [f for f in self.found if f.level != system_check.OK]
        self._show_steps()
        names = [f"{program} ({frontend})" for program, frontend in self.facts.apps]
        self.apps.setText(", ".join(names) or _("No app is connected yet"))

    def _show_steps(self):
        for row in self.rows:
            # Detach now: deleteLater alone leaves the old row painted under the new one.
            row.setParent(None)
            row.deleteLater()
        self.rows = [self._row(finding) for finding in self.found]
        for row in self.rows:
            self.steps.content_layout.addWidget(row)
        title, text = system_check.summary(self.found, self.fixes.failed)
        self.heading.setText(_("Fixing…") if self.fixes.busy else title)
        self.summary.setText(text)
        fixable = self.fixes.fixable(self.findings)
        self.fix_all_btn.setText(_("Fix all ({})").format(len(fixable)))
        self.fix_all_btn.setVisible(len(fixable) > 1)
        self.fix_all_btn.setEnabled(not self.fixes.busy)
        self.on_count(len(self.findings))

    def _row(self, finding):
        row = QWidget()
        row.setObjectName(f"CheckRow-{finding.level}")
        row.level = finding.level
        row.title = finding.title
        row.button = None
        box = QHBoxLayout(row)
        box.setContentsMargins(0, 0, 0, 0)
        icon = QLabel()
        size = self.style().pixelMetric(QStyle.PM_SmallIconSize)
        icon.setPixmap(self.style().standardIcon(ICONS[finding.level]).pixmap(size, size))
        box.addWidget(icon, 0, Qt.AlignTop)
        text = QVBoxLayout()
        text.setSpacing(2)
        text.addWidget(_wrapped(finding.title))
        # A step with a button needs no instructions unless the button fails.
        subtitle = "" if finding.action else finding.fix
        trailing = None
        if finding.action in self.fixes.busy:
            subtitle = _("Fixing…")
            trailing = QProgressBar()
            trailing.setRange(0, 0)
            trailing.setFixedWidth(80)
            trailing.setTextVisible(False)
        elif finding.action in self.fixes.failed:
            subtitle = system_check.failed_text(finding)
            if finding.command:
                trailing = _button(_("Copy command"), lambda: self.copy_command(finding.command))
        elif finding.action:
            trailing = _button(
                system_check.fix_label(finding.action), lambda: self.fix([finding.action])
            )
        if subtitle:
            text.addWidget(mute(_wrapped(subtitle)))
        box.addLayout(text, 1)
        if trailing is not None:
            box.addWidget(trailing)
        if isinstance(trailing, QPushButton):
            row.button = trailing
        return row

    def fix_all(self):
        self.fix(self.fixes.fixable(self.findings))

    def fix(self, actions):
        if self.fixes.busy or not actions:
            return
        self.fixes.busy = set(actions)
        self._show_steps()
        # Let the busy row paint before the fix holds the event loop.
        QTimer.singleShot(50, lambda: self._run_fixes(list(actions)))

    def _run_fixes(self, actions):
        done, undos, settle = self.fixes.run(actions)
        QTimer.singleShot(settle, lambda: self._fixed(done, undos))

    def _fixed(self, done, undos):
        self.run()
        wait = self.fixes.settle_again(done, self.findings)
        if wait:
            QTimer.singleShot(wait, lambda: self._fixed(done, undos))
            return
        text, undo = self.fixes.finish(done, undos, self.findings)
        self._show_steps()
        if text:
            self.notify(text, self._then_check(undo) if undo else None)

    def _then_check(self, undo):
        def undo_and_check():
            ok = undo()
            self.run()
            return ok

        return undo_and_check

    def copy_command(self, command):
        QGuiApplication.clipboard().setText(command)
        self.notify(_("Copied: {}").format(command), None)

    def read_typing_log(self):
        self.app_test = typing_log.read_latest()
        asking = self.app_test is not None and self.app_test.verdict == typing_log.UNKNOWN
        self.right_btn.setVisible(asking)
        self.wrong_btn.setVisible(asking)
        self._show_app_test()

    def _set_verdict(self, verdict):
        self.app_test.verdict = verdict
        self.right_btn.hide()
        self.wrong_btn.hide()
        self._show_app_test()

    def _show_app_test(self):
        self.app_result.setText(typing_log.describe(self.app_test))
        self.app_result.show()

    def report_text(self):
        return system_check.report(self.facts, self.findings, self.app_test)

    def copy_report(self):
        QGuiApplication.clipboard().setText(self.report_text())
        self.notify(_("Copied the result."), None)

    def open_chat(self):
        # Messenger cannot be handed the text, so it waits on the clipboard.
        QGuiApplication.clipboard().setText(self.report_text())
        self.notify(
            _("Copied the result. Paste it into the message and say what went wrong."), None
        )
        QDesktopServices.openUrl(QUrl(MESSENGER_URL))

    def open_report(self):
        QDesktopServices.openUrl(QUrl(system_check.issue_url(self.facts, self.report_text())))
