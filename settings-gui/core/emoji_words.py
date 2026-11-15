# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Emoji page's lists: which words offer which emoji, and the apps that offer none.

fcitx5 keeps only the user's changes to the default words, so words added to a later version
still reach the user.
"""

import unicodedata

from core.emoji_table import EMOJI, GROUPS

WORDS = ("emoji_words", "Words")
OFF_APPS = ("emoji_off_apps", "Apps")
_BY_EMOJI = {row[0]: row for row in EMOJI}


def clean(word):
    """A word as the engine compares it: lower case, composed, single spaces."""
    return " ".join(unicodedata.normalize("NFC", word).lower().split())


class EmojiWords:
    def __init__(self, dbus):
        self.dbus = dbus
        self.load()

    def load(self):
        self.edits = {}
        for item in self.dbus.get_sub_config_list(*WORDS) or []:
            key = (item.get("Emoji", ""), clean(item.get("Keyword", "")))
            if all(key):
                self.edits[key] = str(item.get("On", "True")).lower() == "true"
        self._saved = dict(self.edits)
        self.off_apps = [
            item.get("App", "") for item in self.dbus.get_sub_config_list(*OFF_APPS) or []
        ]
        self.off_apps = [app for app in self.off_apps if app]

    # The words of an emoji, as (word, on): its defaults, then the user's own, then its name's.
    def chips(self, emoji):
        _e, _group, _name, defaults, others = _BY_EMOJI.get(emoji, (emoji, 0, "", (), ()))
        known = (*defaults, *others)
        added = [w for (e, w) in self.edits if e == emoji and w not in known]
        return [(w, self.is_on(emoji, w)) for w in (*defaults, *added, *others)]

    def is_on(self, emoji, word):
        return self.edits.get((emoji, word), word in self._defaults(emoji))

    def _defaults(self, emoji):
        return _BY_EMOJI[emoji][3] if emoji in _BY_EMOJI else ()

    def toggle(self, emoji, word):
        on = not self.is_on(emoji, word)
        row = _BY_EMOJI.get(emoji)
        known = row is not None and word in (*row[3], *row[4])
        # A word of the user's own keeps its entry, so it stays on the page when turned off.
        if known and on == (word in row[3]):
            self.edits.pop((emoji, word), None)
        else:
            self.edits[(emoji, word)] = on

    def add(self, emoji, word):
        """Turns a word on for the emoji; False when it is empty or already there."""
        word = clean(word)
        if not word or any(w == word for w, _on in self.chips(emoji)):
            return False
        self.edits[(emoji, word)] = True
        return True

    def changed(self, emoji):
        return any(e == emoji for e, _w in self.edits)

    def reset(self, emoji):
        self.edits = {key: on for key, on in self.edits.items() if key[0] != emoji}

    def rows(self, query="", group=None, only_on=False):
        """The table's rows that match, as in EMOJI."""
        needle = clean(query)
        shown = []
        for row in EMOJI:
            if group and GROUPS[row[1]] != group:
                continue
            chips = self.chips(row[0])
            if only_on and not any(on for _w, on in chips):
                continue
            if needle and needle != row[0] and not any(needle in w for w, _on in chips):
                continue
            shown.append(row)
        return shown

    def enable_app(self, app):
        """Offers emoji in the app again; returns an undo that puts it back, or None if not saved."""
        before = list(self.off_apps)
        self.off_apps = [a for a in self.off_apps if a != app]
        if not self.save_apps():
            self.off_apps = before
            return None

        def undo():
            self.off_apps = before
            return self.save_apps()

        return undo

    def save_apps(self):
        return self.dbus.set_sub_config_list(*OFF_APPS, [{"App": app} for app in self.off_apps])

    def is_modified(self):
        return self.edits != self._saved

    def is_modified_from_default(self):
        return bool(self.edits or self.off_apps)

    def restore_defaults(self):
        self.edits = {}
        self.off_apps = []

    def save(self):
        items = [
            {"Keyword": word, "Emoji": emoji, "On": "True" if on else "False"}
            for (emoji, word), on in self.edits.items()
        ]
        if not self.dbus.set_sub_config_list(*WORDS, items):
            return False
        self._saved = dict(self.edits)
        return True
