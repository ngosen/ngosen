# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The command line both windows accept."""

import argparse


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="fcitx5-ngosen-settings")
    # The What's new notification opens the window on its page.
    parser.add_argument("--page", choices=["whats-new"])
    # Qt and GTK read their own options from argv, so the unknown ones stay.
    args, _unknown = parser.parse_known_args(argv)
    return args
