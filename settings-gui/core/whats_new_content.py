# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""What the "What's new & tips" page says: the one file to update before a feature release.

An optional link is a (label, url) pair opened in the browser.
A target names what the "Open setting" button shows: ("setting", key) for a setting on the
Typing, Keys or Appearance page, ("page", name) for a whole page, or None.
"""

import random

from i18n import N_

from core.brand import AI_HELP_URL

# Newest release first. A release with no entry gets no section.
FEATURES = [
    {
        "version": "1.15",
        "title": N_("Type without marks"),
        "how": N_(
            "Type toi di hoc and get tôi đi học. Turn on Đoán dấu on the Word guessing page; it can "
            "also fix a syllable typed wrong. It learns from what you type, so its guesses follow how "
            "you write: View shows what it learned, Forget all erases it. In terminals it stays off "
            "unless you turn on Guess in terminals too."
        ),
        "example": "toi di hoc → tôi đi học",
        "target": ("setting", "GuessDiacritics"),
    },
    {
        "version": "1.14",
        "title": N_("Emoji in the apps you choose"),
        "how": N_(
            "Emoji suggestions are off until you turn them on for an app: press ` and then w in it. "
            "Press Tab to number the emoji, then a number to pick one; before Tab, numbers type as "
            "usual. Left alone for 4 seconds, the list hides."
        ),
        "example": "",
        "target": ("page", "emoji"),
    },
    {
        "version": "1.14",
        "title": N_("Pick what the tray shows"),
        "how": N_(
            "The tray can show vi / en instead of the icon, or a picture of your own for each "
            "language. Pick one under Tray Icon Shows. If you had turned the Ngó Sen status icon "
            "off, pick V / E again."
        ),
        "example": "",
        "target": ("setting", "TrayIconKind"),
    },
    {
        "version": "1.14",
        "title": N_("Change a tone in Preedit"),
        "how": N_(
            "In Preedit, select a typed word and press a tone key: the word comes back underlined "
            "with the new tone, as in Sen. Press the key again to keep it as typed. This works in "
            "GTK 4 apps and Chromium."
        ),
        "example": "tiềng + s → tiếng",
        "target": ("setting", "RetoneSelection"),
    },
    {
        "version": "1.14",
        "title": N_("Help from an AI assistant"),
        "how": N_(
            "When typing goes wrong, send an AI assistant the guide below with what you see. It "
            "reads Check my machine and the typing log with you and leaves the fixes to you. The "
            "ngosen-check command prints Check my machine in a terminal."
        ),
        "example": "",
        "target": ("page", "check"),
        "link": (N_("Guide for AI assistants"), AI_HELP_URL),
    },
    {
        "version": "1.13",
        "title": N_("One switch for every app"),
        "how": N_(
            "New installs start in Vietnamese, and the switch key now changes all apps at once, "
            "as in UniKey. Apps set to OFF stay in English; the switch key there opens the ` menu "
            "to pick Sen or Preedit. For the old behavior, choose Each app under Switch Applies "
            "To on the Shortcuts page."
        ),
        "example": "",
        "target": ("setting", "ToggleScope"),
    },
    {
        "version": "1.13",
        "title": N_("Emoji at the end of a sentence"),
        "how": N_(
            "Emoji now wait for the end of a sentence: type . ! ? or pause, then press Tab and a "
            "number to pick one. On GNOME without the Input Method Panel extension they "
            "show right in the field. English words such as cat or thumbs up count too, and the Emoji "
            "page can export and import your words."
        ),
        "example": "",
        "target": ("page", "emoji"),
    },
    {
        "version": "1.13",
        "title": N_("Drawn-out letters"),
        "how": N_(
            "Repeat the last letter of a marked word to draw it out: the word keeps its marks and "
            "the repeated letters have none, as in UniKey, instead of bringing back the keys you "
            "typed. English words such as besttt stay as typed."
        ),
        "example": "afaaa → àaaa, ddusngggg → đúngggg",
        "target": None,
    },
    {
        "version": "1.13",
        "title": N_("Teencode for chosen apps"),
        "how": N_(
            "Turn teencode on for one app with the teencode key in the ` menu. Words like zậy, đag "
            "and nhưg then keep their marks. It is off by default."
        ),
        "example": "nhuwgx → nhữg",
        "target": ("setting", "ShortcutTeencode"),
    },
    {
        "version": "1.13",
        "title": N_("A command for your own shortcut"),
        "how": N_(
            "Bind the ngosen-toggle command to a desktop shortcut to switch between English and "
            "Vietnamese, even when no text field has focus."
        ),
        "example": "",
        "target": None,
    },
    {
        "version": "1.12",
        "title": N_("Emoji while typing"),
        "how": N_(
            "After a word such as cười, con mèo or haha, emoji show next to the cursor. Press Tab "
            "to number them, then 1 to 5 to pick one; Tab, Space or Enter takes the first. The "
            "Emoji page lists every emoji with its words, which you can change."
        ),
        "example": "",
        "target": ("page", "emoji"),
    },
    {
        "version": "1.12",
        "title": N_("Check my machine opens by itself"),
        "how": N_(
            "When a step that stops Vietnamese typing is left, the settings window opens on Check "
            "my machine, then reminds you less and less often until it is fixed. After a first "
            "install it opens once to show what is left."
        ),
        "example": "",
        "target": ("page", "check"),
    },
    {
        "version": "1.11",
        "title": N_("Convert selected text"),
        "how": N_(
            "Select text typed in TCVN3, VNI or another charset and press the convert hotkey to "
            "change it to the charset you chose. Set the hotkey on the Convert page, which also "
            "converts text pasted into it."
        ),
        "example": "",
        "target": ("page", "convert"),
    },
    {
        "version": "1.11",
        "title": N_("Extra keys after a word"),
        "how": N_(
            "A key that does not fit the word no longer undoes its marks. Space then drops the "
            "extra keys, or splits them off when they make a word; BackSpace right after brings "
            "back what you typed."
        ),
        "example": "dduowjcd → được, coongty → công ty",
        "target": None,
    },
    {
        "version": "1.11",
        "title": N_("g for ng"),
        "how": N_(
            "After a word that already has its marks, a final g becomes ng when you press Space."
        ),
        "example": "nhuwg → nhưng, khoog → không",
        "target": None,
    },
    {
        "version": "1.11",
        "title": N_("Vietnamese in the tray menu"),
        "how": N_(
            "The tray menu starts with an item that turns Vietnamese on or off, for when "
            "Ctrl + Shift does not work."
        ),
        "example": "",
        "target": None,
    },
    {
        "version": "1.10",
        "title": N_("Vim mode"),
        "how": N_(
            "Turn on Vim mode for an app from the ` menu: Vim commands after Esc go through as "
            "typed, and the first word after entering insert mode gets its marks. For nvim, "
            "Claude Code and the Vim plugins of Obsidian and JetBrains IDEs."
        ),
        "example": "",
        "target": ("setting", "ShortcutVim"),
    },
    {
        "version": "1.10",
        "title": N_("Commands and paths"),
        "how": N_(
            "A word that starts with / at the start of a line or after a space stays as typed, "
            "for chat commands and paths in a terminal."
        ),
        "example": "/cost, cd /usr/share",
        "target": ("setting", "RawWordAfterSlash"),
    },
    {
        "version": "1.10",
        "title": N_("Forgotten circumflex or horn"),
        "how": N_(
            "In Telex, a word typed without its circumflex or horn still comes out right where "
            "the vowel always takes one."
        ),
        "example": "tienf → tiền, Nguyenx → Nguyễn",
        "target": None,
    },
    {
        "version": "1.10",
        "title": N_("More place and people names"),
        "how": N_(
            "Names such as plông, Mnông and Đakrông now type instead of turning back into the "
            "keys you typed."
        ),
        "example": "ddakroong → đakrông",
        "target": None,
    },
    {
        "version": "1.9",
        "title": N_("GNOME without the extension"),
        "how": N_(
            "On GNOME, typing modes and per-app settings now work with the Ngó Sen extension off, "
            "for apps that have a .desktop file. Check my machine says when the extension is still "
            "needed."
        ),
        "example": "",
        "target": ("page", "check"),
    },
    {
        "version": "1.8",
        "title": N_("Check my machine"),
        "how": N_(
            "Shows what keeps Vietnamese typing from working on this machine, with a button to fix "
            "each item. Test typing in an app and send a bug report from the same page."
        ),
        "example": "",
        "target": ("page", "check"),
    },
    {
        "version": "1.7",
        "title": N_("GTK look for GNOME"),
        "how": N_(
            "A new GTK look that fits GNOME. Prefer the Qt look? Switch on the Appearance page."
        ),
        "example": "",
        "target": ("setting", "SettingsToolkit"),
        # Pictures of the GTK look for people still in the Qt one; the GTK page leaves it out.
        "link": (N_("See the GTK look"), "https://ngosen.github.io/huong-dan/cua-so-cai-dat/"),
    },
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
        "text": N_(
            "Press ` to pick Sen, Preedit or OFF for the app in use, or to turn emoji or Vim keys on or off for it."
        ),
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
        "text": N_("Typing wrong in some app? The Check my machine page says why."),
        "target": ("page", "check"),
    },
    {
        "text": N_(
            "“Save typing log” in the tray menu records what you type, to attach to a bug report."
        ),
        "target": None,
    },
    {
        "text": N_(
            "“Back Up…” and “Restore…” at the bottom of the settings pages keep your settings, macros and dictionary."
        ),
        # The GTK window has these in its header menu, not along the bottom.
        "gtk_text": N_(
            "“Back Up…” and “Restore…” in the window menu keep your settings, macros and dictionary."
        ),
        "target": None,
    },
]


def random_tip():
    """A different tip each time the window opens."""
    return random.choice(TIPS)
