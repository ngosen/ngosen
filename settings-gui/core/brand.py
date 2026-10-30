# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The marks and links both windows share."""

import os

FONT_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "fonts", "XanhMono-Regular.ttf"
)
APP_ID = "io.github.ngosen.NgoSen.Settings"
# The icon is installed under the app id, as desktop files expect.
LOGO = APP_ID

# (light, dark) pairs; the dark shade keeps light text readable on it.
ENAMEL = ("#1c6b5f", "#247b6c")
ON_ENAMEL = "#f0f5ee"

REPO_URL = "https://github.com/ngosen/ngosen"
ISSUE_URL = f"{REPO_URL}/issues/new"
FEATURE_REQUEST_URL = f"{ISSUE_URL}?template=feature_request.yml"
# A chat with the Ngó Sen Facebook page, for people without a GitHub account.
MESSENGER_URL = "https://m.me/61595753582618"
