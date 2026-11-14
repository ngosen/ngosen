#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Regenerates data/keep-words.txt: words whose keys must not be cut back to a Vietnamese word
when a key breaks it, like `axit` or `photoshop`, and written words Space must not cut short,
like `lênin`.

The list is SCOWL en_US-large and misc/keep-words-written.txt, minus data/english-common.txt,
which the engine already checks. Pass --zip to read a downloaded copy of the SCOWL archive.
"""

import hashlib
import io
import re
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCOWL_URL = (
    "https://github.com/en-wl/wordlist/releases/download/rel-2026.02.25/"
    "hunspell-en_US-large-2026.02.25.zip"
)
SCOWL_SHA256 = "06ab5a2a12c29033f100988d3b0a5e53dcad40bf2473ccb78270619d1da99321"


def scowl_archive():
    if "--zip" in sys.argv:
        data = Path(sys.argv[sys.argv.index("--zip") + 1]).read_bytes()
    else:
        with urllib.request.urlopen(SCOWL_URL) as response:
            data = response.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SCOWL_SHA256:
        sys.exit(f"SCOWL archive has sha256 {digest}, expected {SCOWL_SHA256}")
    return zipfile.ZipFile(io.BytesIO(data))


def affix_rules(aff):
    """{flag: (kind, combines, entries)}; `combines` says whether the flag's affix may go on a
    word that has an affix of the other kind."""
    rules = {}
    for line in aff.splitlines():
        parts = line.split()
        if len(parts) < 4 or parts[0] not in ("PFX", "SFX"):
            continue
        kind, flag = parts[0], parts[1]
        if len(parts) == 4 and parts[2] in ("Y", "N"):
            rules.setdefault(flag, (kind, parts[2] == "Y", []))
            continue
        strip = "" if parts[2] == "0" else parts[2]
        add = "" if parts[3] == "0" else parts[3]
        condition = parts[4] if len(parts) > 4 else "."
        pattern = re.compile("^" + condition if kind == "PFX" else condition + "$")
        rules[flag][2].append((strip, add, pattern))
    return rules


def with_affix(word, kind, entries):
    for strip, add, pattern in entries:
        if not pattern.search(word):
            continue
        if kind == "SFX" and word.endswith(strip):
            yield word[: len(word) - len(strip)] + add
        elif kind == "PFX" and word.startswith(strip):
            yield add + word[len(strip) :]


def scowl_words():
    """Every form the dictionary spells, lower case, letters only."""
    archive = scowl_archive()
    rules = affix_rules(archive.read("en_US-large.aff").decode())
    words = set()
    for line in archive.read("en_US-large.dic").decode().splitlines()[1:]:
        stem, _, flags = line.partition("/")
        found = [rules[f] for f in flags.split("\t")[0] if f in rules]
        forms = {stem}
        crossing = set()
        for kind, combines, entries in found:
            if kind == "SFX":
                added = set(with_affix(stem, kind, entries))
                forms |= added
                if combines:
                    crossing |= added
        for kind, combines, entries in found:
            if kind == "PFX":
                for form in {stem} | (crossing if combines else set()):
                    forms.update(with_affix(form, kind, entries))
        words |= {w.lower() for w in forms if w.isascii() and w.isalpha()}
    return words


def main():
    written = (ROOT / "misc/keep-words-written.txt").read_text().split()
    common = set((ROOT / "data/english-common.txt").read_text().split())
    words = (scowl_words() | set(written)) - common
    (ROOT / "data/keep-words.txt").write_text("".join(f"{w}\n" for w in sorted(words)))
    print(f"{len(words)} words")


if __name__ == "__main__":
    main()
