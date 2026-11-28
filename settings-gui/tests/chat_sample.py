# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""A fake fcitx5 whose Ngó Sen holds chat abbreviations, as the engine lists them."""

from fake_fcitx import FakeDBusHandler

TABLE = {"ko": "không", "ae": "anh em", "dc": "được"}


class ChatEngine(FakeDBusHandler):
    def __init__(self, kept=(), refuses=""):
        super().__init__()
        self.kept = set(kept)
        self.refuses = refuses
        self.asked = []

    def call_ngosen(self, method, *args):
        self.asked.append((method, *args))
        if method == "ChatShorthandList":
            # In the order the engine gives them, which is not the order shown.
            return "\n".join(
                f"{key}\t{text}\t{'kept' if key in self.kept else ''}"
                for key, text in sorted(TABLE.items())
            )
        if method == "ExpandShorthandAgain":
            if not self.refuses:
                self.kept.discard(args[0])
            return (not self.refuses, self.refuses)
        return super().call_ngosen(method, *args)
