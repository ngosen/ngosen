# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs the settings window against a fake fcitx5, with no display and no D-Bus."""

import os

from fake_fcitx import DEFAULTS, METADATA, FakeDBusHandler  # noqa: F401

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from qtpy.QtWidgets import QApplication  # noqa: E402

app = QApplication.instance() or QApplication([])
