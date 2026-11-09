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
# hunspell -G drops the forms that are not words, so guessing too many costs only time.
MORE_SUFFIXES = SUFFIXES + [
    "est",
    "ment",
    "ments",
    "ness",
    "ion",
    "ions",
    "ation",
    "ations",
    "ings",
    "ive",
    "al",
]
WORD_LIST = Path("/usr/share/dict/words")
# Typed on purpose: `sus` takes a tone, so `suss` is how people get it.
LEFT_OUT = {"suss"}
# `pas` is in the dictionary, but nobody types `pass` to get it.
ALWAYS_IN = {"pass"}


def hunspell_candidates(pattern=r"[a-z]{3,}", suffixes=SUFFIXES):
    stems = set()
    for line in DICTIONARY.read_text(errors="ignore").split("\n")[1:]:
        word = line.split("/")[0].split("\t")[0]
        if re.fullmatch(pattern, word):
            stems.add(word)
    words = set(stems)
    for stem in stems:
        for suffix in suffixes:
            words.add(stem + suffix)
            if stem[-1] in "ey":
                words.add(stem[:-1] + suffix)
    return words


def candidates():
    words = hunspell_candidates()
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


def printed_by_test(test, tag, words, release=False):
    """The tab-separated rows an ignored test in bamboo-rs prints for `words`."""
    with tempfile.NamedTemporaryFile("w", suffix=".txt") as file:
        file.write("\n".join(words) + "\n")
        file.flush()
        result = subprocess.run(
            ["cargo", "test", *(["--release"] if release else []), "--lib", test]
            + ["--", "--ignored", "--nocapture"],
            cwd=ROOT / "bamboo-rs",
            env={**os.environ, "NGOSEN_WORDS": file.name},
            text=True,
            capture_output=True,
            check=True,
        )
    # Single-threaded libtest prints its own status in front of the first row.
    tag += "\t"
    return [line.partition(tag)[2].split("\t") for line in result.stdout.split("\n") if tag in line]


def shown_by_engine(words):
    return printed_by_test("print_shown_words", "SHOWN", words)


def retone_lookalikes():
    """English words a space would turn into a Vietnamese word with its tone changed, like `parts`
    into pát. Only hunspell is read, so /usr/share/dict/words can still measure what is missed."""
    words = sorted(english(w for w in hunspell_candidates() if w[-1] in "sfrxj"))
    # Typing each word several times over is slow without optimizations.
    found = printed_by_test("print_retone_lookalikes", "LOOKALIKE", words, release=True)
    path = ROOT / "bamboo-rs/src/engine/english-retone-lookalikes.txt"
    path.write_text("".join(f"{word}\n" for word, _ in found))
    for word, retoned in found:
        print(f"  {word}\t{retoned}")
    print(f"{len(found)} words typed like a retoned Vietnamese word")


def common_words():
    """Every English word the dictionary knows, names in lower case, for data/english-common.txt.
    The engine keeps a Backspace on a word shown as typed when the keys start one of these."""
    words = english(hunspell_candidates(r"[a-z]{2,}", MORE_SUFFIXES))
    words |= {w.lower() for w in english(hunspell_candidates(r"[A-Z][a-z]+", MORE_SUFFIXES))}
    # The word list has the forms the guesses miss, like `admirable`; hunspell still decides.
    listed = set(WORD_LIST.read_text(errors="ignore").split("\n"))
    words |= english(w for w in listed if re.fullmatch(r"[a-z]{2,}", w))
    words |= {w.lower() for w in english(w for w in listed if re.fullmatch(r"[A-Z][a-z]+", w))}
    path = ROOT / "data/english-common.txt"
    path.write_text("".join(f"{word}\n" for word in sorted(words)))
    print(f"{len(words)} common words")


def main():
    common_words()
    words = sorted(english(candidates()))
    changed = [(w, s) for w, s in shown_by_engine(words) if w != s and s.isascii()]
    shown_english = english([s for _, s in changed])
    kept = [
        word
        for word, shown in changed
        if word in ALWAYS_IN
        or (
            word not in LEFT_OUT
            and len(word) > 3
            and (shown not in shown_english or not re.search(r"(.)\1", word))
        )
    ]
    shown_of = dict(changed)
    names = english([shown_of[word].capitalize() for word in kept])
    shorter = [
        word
        for word in kept
        if word not in ALWAYS_IN
        and re.search(r"(.)\1", word)
        and shown_of[word].capitalize() in names
    ]
    engine = ROOT / "bamboo-rs/src/engine"
    (engine / "english-words.txt").write_text("\n".join(kept) + "\n")
    (engine / "english-words-shorter-capitalized.txt").write_text("\n".join(shorter) + "\n")
    print(f"{len(kept)} words, {len(shorter)} shorter when capitalized")
    retone_lookalikes()


if __name__ == "__main__":
    main()
