#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Regenerates bamboo-rs/src/engine/english-words.txt from the hunspell en_US dictionary.

A word goes in when Telex keys turn it into plain letters that differ from the keys. People also
double a key on purpose to undo a tone (`off` for "of"), so a word is left out when it has a doubled
letter and what Telex shows is English too.
"""

import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DICTIONARY = Path("/usr/share/hunspell/en_US.dic")
SUFFIXES = ["s", "es", "ed", "d", "ing", "er", "ers", "ly", "y", "ies", "ier", "iest"]
WORD_LIST = Path("/usr/share/dict/words")
# Typed on purpose: `sus` takes a tone, so `suss` is how people get it.
LEFT_OUT = {"suss"}
# `pas` is in the dictionary, but nobody types `pass` to get it.
ALWAYS_IN = {"pass"}


def candidates():
    stems = set()
    for line in DICTIONARY.read_text(errors="ignore").split("\n")[1:]:
        word = line.split("/")[0].split("\t")[0]
        if re.fullmatch(r"[a-z]{3,}", word):
            stems.add(word)
    words = set(stems)
    for stem in stems:
        for suffix in SUFFIXES:
            words.add(stem + suffix)
            if stem[-1] in "ey":
                words.add(stem[:-1] + suffix)
    # The dictionary's stems miss irregular forms and words it builds from parts, like `disaster`.
    for line in WORD_LIST.read_text(errors="ignore").split("\n"):
        if re.fullmatch(r"[a-z]{3,}", line):
            words.add(line)
    return sorted(words)


def english(words):
    result = subprocess.run(
        ["hunspell", "-d", "en_US", "-G"],
        input="\n".join(words) + "\n",
        text=True,
        capture_output=True,
        check=True,
    )
    return set(result.stdout.split())


def shown_by_engine(words):
    with tempfile.NamedTemporaryFile("w", suffix=".txt") as file:
        file.write("\n".join(words) + "\n")
        file.flush()
        result = subprocess.run(
            ["cargo", "test", "--lib", "print_shown_words", "--", "--ignored", "--nocapture"],
            cwd=ROOT / "bamboo-rs",
            env={**os.environ, "NGOSEN_WORDS": file.name},
            text=True,
            capture_output=True,
            check=True,
        )
    return [
        line.split("\t")[1:] for line in result.stdout.split("\n") if line.startswith("SHOWN\t")
    ]


def main():
    words = sorted(english(candidates()))
    changed = [(w, s) for w, s in shown_by_engine(words) if w != s and s.isascii()]
    shown_english = english([s for _, s in changed])
    kept = [
        word
        for word, shown in changed
        if word in ALWAYS_IN
        or (word not in LEFT_OUT and (shown not in shown_english or not re.search(r"(.)\1", word)))
    ]
    (ROOT / "bamboo-rs/src/engine/english-words.txt").write_text("\n".join(kept) + "\n")
    print(f"{len(kept)} words")


if __name__ == "__main__":
    main()
