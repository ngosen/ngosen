#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks data/chat-shorthand.tsv, the chat abbreviations expanded for everyone.

A key that is also a word would change text typed on purpose, so a key must be two letters or more,
no Vietnamese syllable and no English word the engine knows. Whether a key has one meaning in chat
text is read by hand and not checked here.

Usage: check-chat-shorthand.py [--self-test]
"""

import sys
import unicodedata
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / "data"
# The word lists hold these, yet nobody writes them as words: "dc" is there as the abbreviation DC,
# and "đt" is no syllable.
LISTED_BUT_NO_WORD = {"dc", "đt"}


def words(path):
    with open(path, encoding="utf-8") as f:
        return {unicodedata.normalize("NFC", line.strip().lower()) for line in f}


def problems(lines, syllables, english):
    found, keys = [], []
    for number, line in enumerate(lines, 1):
        where = f"line {number}"
        if line != unicodedata.normalize("NFC", line):
            found.append(f"{where}: not NFC")
        fields = line.split("\t")
        if len(fields) != 2 or not all(f and f == f.strip() for f in fields):
            found.append(f"{where}: expected KEY<TAB>EXPANSION")
            continue
        key, expansion = fields
        if len(key) < 2 or not key.isalpha() or key != key.lower():
            found.append(f"{where}: key {key!r} must be two or more lower-case letters")
        if key in syllables - LISTED_BUT_NO_WORD:
            found.append(f"{where}: key {key!r} is a Vietnamese syllable")
        if key in english - LISTED_BUT_NO_WORD:
            found.append(f"{where}: key {key!r} is an English word")
        if expansion.lower() == key:
            found.append(f"{where}: {key!r} expands to itself")
        keys.append(key)
    if keys != sorted(set(keys)):
        found.append("keys must be sorted and unique")
    return found


def self_test():
    syllables, english = {"ban", "đt"}, {"vs", "dc"}
    cases = [
        (["ko\tkhông"], []),
        (["dc\tđược"], []),
        (["đt\tđiện thoại"], []),
        (["ae\tanh em", "ko\tkhông"], []),
        (["k\tkhông"], ["two or more"]),
        (["Ko\tkhông"], ["two or more"]),
        (["ban\tbạn"], ["Vietnamese syllable"]),
        (["vs\tvới"], ["English word"]),
        (["ko không"], ["expected KEY"]),
        ([" ko\tkhông"], ["expected KEY"]),
        (["ko\tkhông "], ["expected KEY"]),
        (["ko\tko"], ["expands to itself"]),
        ([unicodedata.normalize("NFD", "ko\tkhông")], ["not NFC"]),
        (["ko\tkhông", "ae\tanh em"], ["sorted and unique"]),
        (["ko\tkhông", "ko\tkhông"], ["sorted and unique"]),
    ]
    failed = False
    for lines, expected in cases:
        got = problems(lines, syllables, english)
        if len(got) != len(expected) or not all(e in g for e, g in zip(expected, got)):
            print(f"self-test: {lines!r} gave {got}, expected {expected}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


def main():
    if sys.argv[1:] == ["--self-test"]:
        return self_test()
    with open(DATA / "chat-shorthand.tsv", encoding="utf-8") as f:
        lines = f.read().splitlines()
    found = problems(
        lines, words(DATA / "dictionaries/vietnamese.cm.dict"), words(DATA / "english-common.txt")
    )
    for problem in found:
        print(f"data/chat-shorthand.tsv: {problem}", file=sys.stderr)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
