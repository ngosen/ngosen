#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that packaging/debian/copyright records each file under the license the file states.

A file states its license with an SPDX-License-Identifier line near its top, or in a <file>.license
next to it. Word lists the engine loads must state one, since most are derived from outside data.

Usage: check-license-records.py [RECORD]   (default packaging/debian/copyright; needs python3-debian)
"""

import re
import subprocess
import sys
from pathlib import Path

from debian.copyright import Copyright

ROOT = Path(__file__).resolve().parent.parent
WORD_LISTS = re.compile(
    r"(data/(?!CMakeLists\.txt)[^/]+\.(txt|tsv)|bamboo-rs/src/engine/[^/]+\.txt)"
)
SKIP = ("bamboo-rs/vendor/", "LICENSES/")
DEBIAN_TO_SPDX = {"Expat": "MIT", "GPL-3": "GPL-3.0-only", "SCOWL": "LicenseRef-SCOWL"}
SPDX_LINE = re.compile(r"SPDX-License-Identifier:\s*(.+?)\s*(\*/|-->)?\s*$")


def terms(expression, names=None):
    names = names or {}
    return frozenset(names.get(t, t) for t in re.split(r"\s+(?:and|AND)\s+", expression.strip()))


def spdx_line(path, max_lines):
    try:
        with open(ROOT / path, encoding="utf-8", errors="replace") as f:
            for _, line in zip(range(max_lines), f):
                if m := SPDX_LINE.search(line):
                    return m.group(1)
    except (IsADirectoryError, FileNotFoundError):
        pass
    return None


def stated_license(path, tracked):
    if f"{path}.license" in tracked:
        return spdx_line(f"{path}.license", 100) or ""
    return spdx_line(path, 10)


def main():
    record_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "packaging/debian/copyright"
    with open(record_path, encoding="utf-8") as f:
        record = Copyright(f, strict=True)
    listed = subprocess.run(
        ["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout
    tracked = set(listed.split("\0")) - {""}

    errors = []
    for path in sorted(tracked):
        if path.startswith(SKIP) or path.endswith(".license"):
            continue
        stated = stated_license(path, tracked)
        if stated == "":
            errors.append(f"{path}.license: no SPDX-License-Identifier line")
            continue
        if stated is None:
            if WORD_LISTS.fullmatch(path):
                errors.append(f"{path}: states no license; add {path}.license")
            continue
        paragraph = record.find_files_paragraph(path)
        recorded = paragraph.license.synopsis if paragraph else "nothing"
        if terms(stated) != terms(recorded, DEBIAN_TO_SPDX):
            errors.append(f"{path}: states {stated}, {record_path.name} records {recorded}")

    for error in errors:
        print(error)
    if errors:
        print(f"{len(errors)} files disagree with {record_path}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
