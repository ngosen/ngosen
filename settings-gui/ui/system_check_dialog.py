# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Shows what is wrong with this machine's fcitx5 setup and opens a pre-filled bug report."""

from core.brand import MESSENGER_URL
from i18n import _
from qtpy.QtCore import QTimer, QUrl
from qtpy.QtGui import QDesktopServices, QGuiApplication
from qtpy.QtWidgets import (
    QDialog,
    QDialogButtonBox,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QStyle,
    QVBoxLayout,
    QWidget,
)

from core import system_check, typing_log
from ui.typography import mute

ICONS = {
    system_check.OK: QStyle.SP_DialogApplyButton,
    system_check.WARN: QStyle.SP_MessageBoxWarning,
    system_check.FAIL: QStyle.SP_MessageBoxCritical,
}


class SystemCheckDialog(QDialog):
    def __init__(self, dbus_handler, ngosen_version="", parent=None):
        super().__init__(parent)
        self.dbus_handler = dbus_handler
        self.ngosen_version = ngosen_version
        self.setWindowTitle(_("Check my machine"))
        self.setMinimumWidth(520)

        layout = QVBoxLayout(self)
        intro = QLabel(
            _(
                "How fcitx5 is set up on this machine. Nothing is sent until you send a bug "
                "report, and it lists the apps below, so read it before sending."
            )
        )
        intro.setWordWrap(True)
        mute(intro)
        layout.addWidget(intro)

        self.results = QVBoxLayout()
        self.results.setSpacing(12)
        layout.addLayout(self.results)

        self.apps = QLabel()
        self.apps.setObjectName("CheckApps")
        self.apps.setWordWrap(True)
        mute(self.apps)
        layout.addWidget(self.apps)
        layout.addWidget(self._app_test_box())
        layout.addStretch()

        # Its own row: five buttons do not fit the dialog, and these two are where the result goes.
        send = QHBoxLayout()
        self.message_btn = QPushButton(_("Message on Facebook…"))
        self.report_btn = QPushButton(_("Report on GitHub…"))
        send.addWidget(self.message_btn)
        send.addWidget(self.report_btn)
        send.addStretch()
        layout.addLayout(send)
        self.status = QLabel()
        self.status.setWordWrap(True)
        mute(self.status)
        layout.addWidget(self.status)

        buttons = QDialogButtonBox(QDialogButtonBox.Close)
        self.again_btn = buttons.addButton(_("Check again"), QDialogButtonBox.ActionRole)
        self.copy_btn = buttons.addButton(_("Copy result"), QDialogButtonBox.ActionRole)
        self.again_btn.clicked.connect(self.run)
        self.copy_btn.clicked.connect(self.copy_report)
        self.message_btn.clicked.connect(self.open_chat)
        self.report_btn.clicked.connect(self.open_report)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.run()

    def run(self):
        self.facts = system_check.collect(self.dbus_handler, self.ngosen_version)
        self.findings = system_check.evaluate(self.facts)
        self._show_findings()
        if self.facts.apps:
            names = ", ".join(f"{p} ({fe})" for p, fe in self.facts.apps)
            self.apps.setText(_("Apps connected to fcitx5: {}").format(names))
        else:
            self.apps.setText("")
        # The layout picks up the new rows only on the next event loop pass; resizing now would
        # use the old height and clip the rows.
        QTimer.singleShot(0, self._fit)

    def _app_test_box(self):
        self.app_test = None
        box = QGroupBox(_("Test an app"))
        box_layout = QVBoxLayout(box)
        steps = QLabel(
            _(
                "Open the app, switch to Ngó Sen and type “{sample}” ({telex} in Telex). Then "
                "choose “Save typing log” in the Ngó Sen menu and read the log here."
            ).format(sample=typing_log.SAMPLE, telex=typing_log.SAMPLE_TELEX)
        )
        steps.setWordWrap(True)
        box_layout.addWidget(steps)

        row = QHBoxLayout()
        self.read_log_btn = QPushButton(_("Read typing log"))
        self.read_log_btn.clicked.connect(self.read_typing_log)
        row.addWidget(self.read_log_btn)
        self.right_btn = QPushButton(_("It typed right"))
        self.wrong_btn = QPushButton(_("It typed wrong"))
        self.right_btn.clicked.connect(lambda: self._set_verdict(typing_log.RIGHT))
        self.wrong_btn.clicked.connect(lambda: self._set_verdict(typing_log.WRONG))
        for btn in (self.right_btn, self.wrong_btn):
            btn.hide()
            row.addWidget(btn)
        row.addStretch()
        box_layout.addLayout(row)

        self.app_result = QLabel()
        self.app_result.setObjectName("AppTestResult")
        self.app_result.setWordWrap(True)
        self.app_result.hide()
        box_layout.addWidget(self.app_result)
        return box

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
        QTimer.singleShot(0, self._fit)

    def _show_findings(self):
        while self.results.count():
            item = self.results.takeAt(0)
            row = item.widget()
            if row:
                # Detach now: deleteLater alone leaves the old row painted under the new one.
                row.setParent(None)
                row.deleteLater()
        findings = self.findings or [
            system_check.Finding(system_check.OK, _("No setup problem found"))
        ]
        for finding in findings:
            self.results.addWidget(self._row(finding))

    def _row(self, finding):
        row = QWidget()
        row.setObjectName(f"CheckRow-{finding.level}")
        box = QHBoxLayout(row)
        box.setContentsMargins(0, 0, 0, 0)
        icon = QLabel()
        size = self.style().pixelMetric(QStyle.PM_SmallIconSize)
        icon.setPixmap(self.style().standardIcon(ICONS[finding.level]).pixmap(size, size))
        box.addWidget(icon, 0)
        text = QVBoxLayout()
        text.setSpacing(2)
        text.addWidget(QLabel(finding.title))
        if finding.fix:
            text.addWidget(mute(QLabel(finding.fix)))
        box.addLayout(text, 1)
        return row

    def _fit(self):
        self.adjustSize()
        # The size hint wraps text at a narrower width than the dialog has, leaving a gap.
        self.resize(self.width(), self.layout().totalHeightForWidth(self.width()))

    def report_text(self):
        return system_check.report(self.facts, self.findings, self.app_test)

    def copy_report(self):
        QGuiApplication.clipboard().setText(self.report_text())
        self.status.setText(_("Copied the result."))

    def open_chat(self):
        # Messenger cannot be handed the text, so it waits on the clipboard.
        QGuiApplication.clipboard().setText(self.report_text())
        self.status.setText(
            _("Copied the result. Paste it into the message and say what went wrong.")
        )
        QDesktopServices.openUrl(QUrl(MESSENGER_URL))

    def open_report(self):
        QDesktopServices.openUrl(QUrl(system_check.issue_url(self.facts, self.report_text())))
