# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""What the "What's new & tips" page says: the one file to update before a feature release.

A target names what the "Open setting" button shows: ("setting", key) for a setting on the
Typing, Keys or Appearance page, ("page", name) for a whole page, or None.
"""

import random

from i18n import N_

# Newest release first. A release with no entry gets no section.
FEATURES = [
    {
        "version": "1.6",
        "title": N_("Cứu chữ"),
        "how": N_(
            "Forgot to switch Vietnamese on? Press the English/Vietnamese key and the word is "
            "fixed; Tab or Esc gives back what you typed."
        ),
        "example": "tieengs vieetj → tiếng việt",
        "target": ("setting", "RetypeAfterSwitch"),
    },
    {
        "version": "1.6",
        "title": N_("Cứu dấu"),
        "how": N_(
            "Select a word, or click right after it, and type a tone key to change its tone "
            "mark; Tab or Esc gives it back."
        ),
        "example": "nhanh + s → nhánh",
        "target": ("setting", "RetoneSelection"),
    },
    {
        "version": "1.6",
        "title": N_("English/Vietnamese key"),
        "how": N_(
            "A new install switches with Ctrl+Shift, an older one keeps Ctrl+Space; Ngó Sen "
            "remembers the mode across logouts."
        ),
        "example": "",
        "target": ("setting", "ToggleKey"),
    },
]

TIPS = [
    {
        "text": N_("Press ` to pick Sen, Preedit, Emoji Picker or OFF for the app in use."),
        "target": ("setting", "ModeMenuKey"),
    },
    {
        "text": N_("Tap Ctrl alone to keep the word as typed: tunx, Ctrl, Space keeps tũn."),
        "target": ("setting", "LockWordModifier"),
    },
    {
        "text": N_("A macro ko → không also types Không when you type Ko."),
        "target": ("page", "macros"),
    },
    {
        "text": N_("] types ư and [ types ơ, so ch]a gives chưa. Off by default."),
        "target": ("setting", "BracketTransform"),
    },
    {
        "text": N_("Space twice types a period. Off by default."),
        "target": ("setting", "DoubleSpaceToPeriod"),
    },
    {
        "text": N_("-- types an em dash (—). Off by default."),
        "target": ("setting", "DoubleHyphenToEmDash"),
    },
    {
        "text": N_(
            "A capital letter after . ! ? and Enter. Off by default; the tray menu's Auto "
            "Capitalize pauses it."
        ),
        "target": ("setting", "AutoCapitalizeAfterPunctuation"),
    },
    {
        "text": N_("hoà or hòa? Pick with “Use oà, uý (Instead Of òa, úy)”."),
        "target": ("setting", "ModernStyle"),
    },
    {
        "text": N_("$DATE and $TIME in a macro type today's date and time: hn → Hôm nay $DATE."),
        "target": ("page", "macros"),
    },
    {
        "text": N_("Typing wrong in some app? “Check my machine…” on the About page says why."),
        "target": ("page", "about"),
    },
    {
        "text": N_(
            "“Save typing log” in the tray menu records what you type, to attach to a bug report."
        ),
        "target": None,
    },
    {
        "text": N_("“Back Up…” and “Restore…” below keep your settings, macros and dictionary."),
        "target": None,
    },
]


def random_tip():
    """A different tip each time the window opens."""
    return random.choice(TIPS)
