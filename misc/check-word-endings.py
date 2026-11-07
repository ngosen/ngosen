#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Measures what words commit when a space ends them, to check a change to how words end.

    check-word-endings.py run OUT_DIR       type every corpus, write OUT_DIR/<corpus>.tsv
    check-word-endings.py compare BASE NEW  list what changed between two runs

A corpus line is the keys, then optionally a tab and the word they should give. Corpora with an
expected word report how many give it; the others report only what changed against BASE.
"""

import os
import re
import subprocess
import sys
import tempfile
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CASES = ROOT / "misc/word-endings"
DICTIONARY = ROOT / "data/dictionaries/vietnamese.cm.dict"
HUNSPELL = Path("/usr/share/hunspell/en_US.dic")
WORD_LIST = Path("/usr/share/dict/words")

MARKS = {"â": "aa", "ă": "aw", "ê": "ee", "ô": "oo", "ơ": "ow", "ư": "uw", "đ": "dd"}
TONES = {"́": "s", "̀": "f", "̉": "r", "̃": "x", "̣": "j"}
FINALS = ("ch", "c", "p", "t")


def telex(word):
    """Telex keys for a Vietnamese word as letters and its tone key, or None."""
    letters, tone = [], ""
    for ch in word:
        parts = unicodedata.normalize("NFD", ch)
        tone = next((TONES[c] for c in parts if c in TONES), tone)
        base = unicodedata.normalize("NFC", "".join(c for c in parts if c not in TONES))
        letters.append(MARKS.get(base, base))
    if not all(p.isascii() and p.isalpha() for p in letters):
        return None
    return letters, tone


def vietnamese():
    """Dictionary words typed with the tone last, and right after the last vowel."""
    last, after_vowel, retone = [], [], []
    for line in DICTIONARY.read_text(encoding="utf-8").splitlines():
        word = line.strip().lower()
        typed = telex(word) if word and " " not in word else None
        if not typed:
            continue
        letters, tone = typed
        keys = "".join(letters)
        last.append(f"{keys}{tone}\t{word}")
        vowels = [i for i, p in enumerate(letters) if p[0] in "aeiouy"]
        if tone and vowels:
            i = vowels[-1] + 1
            after_vowel.append(f"{''.join(letters[:i])}{tone}{''.join(letters[i:])}\t{word}")
            # A tone the final consonant does not take, then the right one, in both orders.
            final = next((f for f in FINALS if keys.endswith(f)), None)
            if final and tone in "sj":
                stem = "".join(letters[:i])
                rest = "".join(letters[i:])
                for wrong in "xf":
                    retone.append(f"{stem}{wrong}{rest}{tone}\t{word}")
                    retone.append(f"{stem}{rest}{wrong}{tone}\t{word}")
        else:
            after_vowel.append(f"{keys}{tone}\t{word}")
    return {
        "vietnamese-tone-last": last,
        "vietnamese-tone-after-vowel": after_vowel,
        "retone-generated": retone,
    }


def english():
    words = [
        w
        for w in WORD_LIST.read_text(errors="ignore").splitlines()
        if re.fullmatch(r"[A-Za-z]{2,}", w)
    ]
    names = set()
    for line in HUNSPELL.read_text(errors="ignore").splitlines()[1:]:
        name, _, flags = line.partition("/")
        if re.fullmatch(r"[A-Z][a-z]+", name):
            names.add(name)
            if "S" in flags or "M" in flags:
                names.add(name + "s")
    listed = (ROOT / "bamboo-rs/src/engine/english-words.txt").read_text().splitlines()
    technical = (CASES / "technical.txt").read_text().splitlines()
    return {
        "word-list": words,
        "names": sorted(names),
        "english-words": listed,
        "technical": technical,
    }


def corpora():
    result = {"telex": (CASES / "telex.tsv").read_text(encoding="utf-8").splitlines()}
    result.update(vietnamese())
    result.update(english())
    return result


def committed(lines, method):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", encoding="utf-8") as file:
        file.write("\n".join(lines) + "\n")
        file.flush()
        result = subprocess.run(
            [
                "cargo",
                "test",
                "--release",
                "--lib",
                "print_committed_words",
                "--",
                "--ignored",
                "--nocapture",
            ],
            cwd=ROOT / "bamboo-rs",
            env={**os.environ, "NGOSEN_WORDS": file.name, "NGOSEN_INPUT_METHOD": method},
            text=True,
            capture_output=True,
            check=True,
        )
    # Single-threaded libtest prints its own status in front of the first line.
    rows = [line.partition("COMMITTED\t")[2].split("\t") for line in result.stdout.splitlines()]
    return [(keys, shown(text)) for keys, text in (row for row in rows if len(row) == 2)]


def shown(text):
    """The committed word without the one space that ended it, or the whole text if that differs."""
    value = text[1:-1]
    if value.endswith(" ") and not value.endswith("  "):
        return value[:-1]
    return text


def same_word(a, b):
    """Whether two words differ at most in where the tone sits (hòa, hoà)."""

    def key(word):
        parts = unicodedata.normalize("NFD", word)
        tones = sorted(c for c in parts if c in TONES)
        return "".join(c for c in parts if c not in TONES), tones

    return key(a) == key(b)


def run(out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    jobs = {name: (lines, "Telex") for name, lines in corpora().items()}
    jobs["vni"] = ((CASES / "vni.tsv").read_text(encoding="utf-8").splitlines(), "VNI")
    for name, (lines, method) in jobs.items():
        lines = [line for line in lines if line.partition("\t")[0]]
        expected = [line.partition("\t")[2] for line in lines]
        got = committed(lines, method)
        if len(got) != len(expected):
            sys.exit(f"{name}: typed {len(expected)} lines, got {len(got)} results")
        rows = [f"{keys}\t{want}\t{shown}" for (keys, shown), want in zip(got, expected)]
        (out_dir / f"{name}.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")
        print(f"{name}: {len(rows)}")


def load(path):
    rows = [line.split("\t") for line in path.read_text(encoding="utf-8").splitlines() if line]
    return {keys: (want, got) for keys, want, got in rows}


def compare(base_dir, new_dir, limit=20):
    for path in sorted(new_dir.glob("*.tsv")):
        new = load(path)
        base_path = base_dir / path.name
        base = load(base_path) if base_path.exists() else {}
        changed = [k for k in new if k in base and base[k][1] != new[k][1]]
        added = len(new.keys() - base.keys())
        removed = len(base.keys() - new.keys())
        line = f"{path.stem}: {len(new)} words, {len(changed)} changed"
        if added or removed:
            line += f", {added} added, {removed} removed"
        wrong = [k for k in new if new[k][0] and not same_word(new[k][0], new[k][1])]
        expected = sum(1 for k in new if new[k][0])
        if expected:
            line += f", {expected - len(wrong)}/{expected} as expected"
        print(line)
        for keys in changed[:limit]:
            print(f"  {keys}\t{base[keys][1]} -> {new[keys][1]}")
        for keys in wrong[:limit]:
            print(f"  WRONG {keys}\t{new[keys][1]} (want {new[keys][0]})")
    for path in sorted(base_dir.glob("*.tsv")):
        if not (new_dir / path.name).exists():
            print(f"{path.stem}: missing from {new_dir}")


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "run":
        run(Path(sys.argv[2]))
    elif len(sys.argv) == 4 and sys.argv[1] == "compare":
        compare(Path(sys.argv[2]), Path(sys.argv[3]))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
