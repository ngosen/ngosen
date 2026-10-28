# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

from core.brand import BUG_REPORT_URL, FEATURE_REQUEST_URL, REPO_URL
from i18n import _
from qtpy.QtCore import Qt, QUrl
from qtpy.QtGui import QColor, QDesktopServices, QPalette
from qtpy.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QScrollArea,
    QVBoxLayout,
    QWidget,
)

from core import system_check
from ui import brand
from ui.typography import mute

try:
    from version import __version__
except ImportError:
    __version__ = "dev version"  # Fallback for local development


def _color_warning(label):
    palette = label.palette()
    palette.setColor(QPalette.WindowText, QColor(brand.pick(label, brand.ERROR_BORDER)))
    label.setPalette(palette)


class AboutPage(QWidget):
    def __init__(self, dbus_handler=None, parent=None):
        super().__init__(parent)
        self.dbus_handler = dbus_handler
        self.check_dialog = None
        self._setup_ui()

    def open_system_check(self):
        from ui.system_check_dialog import SystemCheckDialog

        if self.check_dialog is None:
            self.check_dialog = SystemCheckDialog(self.dbus_handler, __version__, self)
        else:
            self.check_dialog.run()
        self.check_dialog.show()
        self.check_dialog.raise_()

    def _setup_ui(self):
        # Root layout for this widget
        root_layout = QVBoxLayout(self)
        root_layout.setContentsMargins(0, 0, 0, 0)

        # Scroll Area to handle overcrowding
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        scroll.setAttribute(Qt.WA_TranslucentBackground)

        content_widget = QWidget()
        content_widget.setObjectName("AboutContent")

        layout = QVBoxLayout(content_widget)
        layout.setContentsMargins(40, 30, 40, 40)
        layout.setSpacing(20)
        # Centred per widget: a centred layout would squeeze the wrapping description.
        layout.setAlignment(Qt.AlignTop)

        pixmap = brand.logo_icon().pixmap(80, 80)
        if not pixmap.isNull():
            logo = QLabel()
            logo.setPixmap(pixmap)
            layout.addWidget(logo, alignment=Qt.AlignCenter)

        title = QLabel("Ngó Sen")
        title.setObjectName("Wordmark")
        title.setFont(brand.wordmark_font(30))
        layout.addWidget(title, alignment=Qt.AlignCenter)

        version = QLabel(_("Version {}").format(__version__))
        version.setObjectName("VersionTag")
        version.setAlignment(Qt.AlignCenter)
        mute(version)
        layout.addWidget(version, alignment=Qt.AlignCenter)

        if system_check.runs_replaced_addon():
            replaced = QLabel(
                _(
                    "Typing still uses the Ngó Sen that ran before the update. "
                    "Log out and back in to start version {}."
                ).format(__version__)
            )
            replaced.setObjectName("AddonReplaced")
            replaced.setWordWrap(True)
            replaced.setAlignment(Qt.AlignCenter)
            layout.addWidget(replaced)
            brand.follow_theme(replaced, _color_warning)

        desc = QLabel(
            _("Vietnamese input method for fcitx5 on Linux that types like UniKey on Windows.")
        )
        desc.setWordWrap(True)
        desc.setAlignment(Qt.AlignCenter)
        desc.setObjectName("AboutDescription")
        layout.addWidget(desc)

        # GitHub Project Link
        github_link = QLabel(f'<a href="{REPO_URL}" style="text-decoration: none;">{REPO_URL}</a>')
        github_link.setOpenExternalLinks(True)
        layout.addWidget(github_link, alignment=Qt.AlignCenter)

        # Support Buttons Row
        support_layout = QHBoxLayout()
        support_layout.setSpacing(15)
        support_layout.setAlignment(Qt.AlignCenter)

        btn_bug = QPushButton(_("Report Bug"))
        btn_bug.setObjectName("BugReport")
        btn_bug.setFixedWidth(200)
        btn_bug.clicked.connect(lambda: QDesktopServices.openUrl(QUrl(BUG_REPORT_URL)))

        btn_feature = QPushButton(_("Request Feature"))
        btn_feature.setObjectName("FeatureRequest")
        btn_feature.setFixedWidth(200)
        btn_feature.clicked.connect(lambda: QDesktopServices.openUrl(QUrl(FEATURE_REQUEST_URL)))

        # Its own row: three buttons do not fit the narrowest window, and checking comes before reporting.
        btn_check = QPushButton(_("Check my machine…"))
        btn_check.setObjectName("SystemCheck")
        btn_check.setFixedWidth(200)
        btn_check.setEnabled(self.dbus_handler is not None)
        btn_check.clicked.connect(self.open_system_check)
        layout.addWidget(btn_check, alignment=Qt.AlignCenter)

        support_layout.addWidget(btn_bug)
        support_layout.addWidget(btn_feature)
        layout.addLayout(support_layout)

        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setObjectName("AboutLine")
        layout.addWidget(line)

        developer = QLabel(_("Developed by Nguyen Phi"))
        developer.setObjectName("DeveloperLine")
        layout.addWidget(developer, alignment=Qt.AlignCenter)

        layout.addStretch()

        # Footer
        footer_line = QFrame()
        footer_line.setFrameShape(QFrame.HLine)
        footer_line.setObjectName("AboutLine")
        layout.addWidget(footer_line)

        license_info = QLabel(_("Licensed under the GNU General Public License v3.0"))
        license_info.setObjectName("LicenseInfo")
        mute(license_info)
        layout.addWidget(license_info, alignment=Qt.AlignCenter)

        scroll.setWidget(content_widget)
        root_layout.addWidget(scroll)
