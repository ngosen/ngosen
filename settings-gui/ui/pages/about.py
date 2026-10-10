# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

from i18n import _
from qtpy.QtCore import Qt, QUrl
from qtpy.QtGui import QDesktopServices, QIcon
from qtpy.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QScrollArea,
    QVBoxLayout,
    QWidget,
)

try:
    from version import __version__
except ImportError:
    __version__ = "dev version"  # Fallback for local development


class AboutPage(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._setup_ui()

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
        layout.setAlignment(Qt.AlignTop | Qt.AlignHCenter)

        # Logo/Icon
        try:
            pixmap = QIcon.fromTheme("fcitx-ngosen").pixmap(80, 80)
            if pixmap.isNull():
                logo = QLabel("🪷")
                logo.setStyleSheet("font-size: 64px; margin-bottom: 5px;")
            else:
                logo = QLabel()
                logo.setPixmap(pixmap)
                logo.setStyleSheet("margin-bottom: 5px;")
        except Exception:
            logo = QLabel("🪷")
            logo.setStyleSheet("font-size: 64px; margin-bottom: 5px;")

        layout.addWidget(logo, alignment=Qt.AlignCenter)

        title = QLabel("Ngó Sen")
        title.setObjectName("AboutTitle")
        layout.addWidget(title, alignment=Qt.AlignCenter)

        version = QLabel(_("Version {}").format(__version__))
        version.setObjectName("VersionTag")
        version.setAlignment(Qt.AlignCenter)
        version.setStyleSheet("""
            QLabel#VersionTag {
                background-color: palette(highlight);
                color: palette(highlighted-text);
                border-radius: 10px;
                padding: 2px 10px;
                font-size: 11px;
                font-weight: bold;
            }
        """)
        layout.addWidget(version, alignment=Qt.AlignCenter)

        desc = QLabel(_("Modern, fast, and stable Vietnamese input method for Linux."))
        desc.setWordWrap(True)
        desc.setAlignment(Qt.AlignCenter)
        desc.setObjectName("AboutDescription")
        desc.setMinimumHeight(60)
        layout.addWidget(desc, alignment=Qt.AlignCenter)

        # GitHub Project Link
        github_link = QLabel(
            '<a href="https://github.com/ngosen/ngosen" style="text-decoration: none;">https://github.com/ngosen/ngosen</a>'
        )
        github_link.setOpenExternalLinks(True)
        layout.addWidget(github_link, alignment=Qt.AlignCenter)

        # Support Buttons Row
        support_layout = QHBoxLayout()
        support_layout.setSpacing(15)
        support_layout.setAlignment(Qt.AlignCenter)

        btn_bug = QPushButton(_("Report Bug"))
        btn_bug.setObjectName("BugReport")
        btn_bug.setFixedWidth(200)
        btn_bug.clicked.connect(
            lambda: QDesktopServices.openUrl(
                QUrl("https://github.com/ngosen/ngosen/issues/new?template=bug_report.yml")
            )
        )

        btn_feature = QPushButton(_("Request Feature"))
        btn_feature.setObjectName("FeatureRequest")
        btn_feature.setFixedWidth(200)
        btn_feature.clicked.connect(
            lambda: QDesktopServices.openUrl(
                QUrl("https://github.com/ngosen/ngosen/issues/new?template=feature_request.yml")
            )
        )

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

        # One credit line; the authors are named in the copyright notices.
        credits = QLabel(
            _("Based on the code of {lotus} and {vmk}.").format(
                lotus='<a href="https://github.com/LotusInputMethod/fcitx5-lotus" style="text-decoration: none;">fcitx5-lotus</a>',
                vmk='<a href="https://github.com/thanhpy2009/VMK" style="text-decoration: none;">VMK</a>',
            )
        )
        credits.setOpenExternalLinks(True)
        credits.setObjectName("CreditsLine")
        credits.setStyleSheet("font-size: 11px;")
        layout.addWidget(credits, alignment=Qt.AlignCenter)
        layout.addStretch()

        # Footer
        footer_line = QFrame()
        footer_line.setFrameShape(QFrame.HLine)
        footer_line.setObjectName("AboutLine")
        layout.addWidget(footer_line)

        license_info = QLabel(_("Licensed under the GNU General Public License v3.0"))
        license_info.setObjectName("LicenseInfo")
        layout.addWidget(license_info, alignment=Qt.AlignCenter)

        scroll.setWidget(content_widget)
        root_layout.addWidget(scroll)
