#!/usr/bin/env python3
"""Write comparison cases to stdout, one per line:

    id <TAB> input method <TAB> modern <TAB> free marking <TAB> w2u <TAB> keys

In keys, '<' is a backspace and '!' restores the word to the keys typed.
"""

import argparse
import random
import re
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DICT = ROOT / "data/dictionaries/vietnamese.cm.dict"
GO_TESTS = ROOT / "bamboo/bamboo-core/bamboo_test.go"

TONES = {"́": 0, "̀": 1, "̉": 2, "̃": 3, "̣": 4}
TONE_KEYS = {"Telex": "sfrxj", "VNI": "12345"}
MARKS = {
    "Telex": {"â": "aa", "ê": "ee", "ô": "oo", "ă": "aw", "ơ": "ow", "ư": "uw", "đ": "dd"},
    "VNI": {"â": "a6", "ê": "e6", "ô": "o6", "ă": "a8", "ơ": "o7", "ư": "u7", "đ": "d9"},
}
VOWELS = set("aăâeêioôơuưy")
ENGLISH = """windows facebook github chrome python linux ubuntu fedora google youtube
file test email server online offline update download password string return
class import export default status window vscode docker thanks hello""".split()


def split_tone(word):
    """Return (letters with marks but no tone, tone index or None)."""
    tone = None
    letters = []
    for ch in word:
        parts = unicodedata.normalize("NFD", ch)
        base = parts[0]
        rest = ""
        for c in parts[1:]:
            if c in TONES:
                tone = TONES[c]
            else:
                rest += c
        letters.append(unicodedata.normalize("NFC", base + rest))
    return letters, tone


def keys_for(word, im, order):
    """Keys that type word: marks inline or at the end, tone at the end or after the vowels."""
    letters, tone = split_tone(word)
    marks = MARKS[im]
    tone_key = TONE_KEYS[im][tone] if tone is not None else ""
    out = []
    tail = []
    last_vowel = -1
    for ch in letters:
        if order == "marks-last" and ch in marks and ch not in "âêôđ":
            out.append(marks[ch][0])
            tail.append(marks[ch][1])
        else:
            out.append(marks.get(ch, ch))
        if ch in VOWELS:
            last_vowel = len(out)
    if order == "tone-after-vowel" and tone_key and last_vowel > 0:
        out.insert(last_vowel, tone_key)
        tone_key = ""
    seq = "".join(out) + "".join(dict.fromkeys(tail)) + tone_key
    return seq


def dictionary_words():
    words = []
    for line in DICT.read_text(encoding="utf-8").splitlines():
        w = line.strip().lower()
        if w and not w.startswith("#"):
            words.append(w)
    return words


def go_test_inputs():
    text = GO_TESTS.read_text(encoding="utf-8")
    return sorted(set(re.findall(r'ProcessString\("([^"\\]+)"', text)))


def configs(args):
    for im in ("Telex", "VNI"):
        for modern in (1, 0):
            for free in (1, 0):
                yield im, modern, free, args.w2u


def with_backspaces(seq, rng):
    """Delete and retype one key, so the word should come out unchanged."""
    if len(seq) < 2:
        return seq
    i = rng.randrange(1, len(seq) + 1)
    return seq[:i] + "<" + seq[i - 1 : i] + seq[i:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w2u", type=int, default=0)
    ap.add_argument("--random", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    out = sys.stdout
    words = dictionary_words()
    n = 0

    def emit(kind, im, modern, free, w2u, keys):
        nonlocal n
        n += 1
        out.write(f"{kind}-{n}\t{im}\t{modern}\t{free}\t{w2u}\t{keys}\n")

    for im, modern, free, w2u in configs(args):
        cfg = (im, modern, free, w2u)
        for w in words:
            for order in ("inline", "tone-after-vowel", "marks-last"):
                emit(f"dict.{order}", *cfg, keys_for(w, im, order))
            seq = keys_for(w, im, "inline")
            emit("dict.capital", *cfg, seq[:1].upper() + seq[1:])
            emit("dict.upper", *cfg, seq.upper())
            emit("dict.backspace", *cfg, with_backspaces(seq, rng))
            emit("dict.restore", *cfg, seq + "!")
        for w in ENGLISH:
            emit("english", *cfg, w)
            emit("english.restore", *cfg, w + "!")
        if im == "Telex":
            for s in go_test_inputs():
                emit("go-test", *cfg, s)
        letters = "abcdeghiklmnopqrstuvxy"
        extra = TONE_KEYS[im] + ("wz" if im == "Telex" else "06789")
        for _ in range(args.random // 8):
            length = rng.randint(1, 12)
            keys = "".join(
                rng.choice(letters + extra + "<")
                if rng.random() > 0.1
                else rng.choice(letters).upper()
                for _ in range(length)
            )
            emit("random", *cfg, keys)


if __name__ == "__main__":
    main()
