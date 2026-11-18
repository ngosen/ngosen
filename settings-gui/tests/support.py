# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs the settings window against a fake fcitx5, with no display and no D-Bus."""

import os

from fake_fcitx import DEFAULTS, METADATA, FakeDBusHandler  # noqa: F401

from core import system_check

_fcitx_version = system_check._fcitx_version


def _fcitx_version_with_home():
    # Tests clear the environment to stand in for other desktops, and fcitx5 aborts without HOME.
    return _fcitx_version() if os.environ.get("HOME") else ""


system_check._fcitx_version = _fcitx_version_with_home

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from qtpy.QtWidgets import QApplication  # noqa: E402

app = QApplication.instance() or QApplication([])
