# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copies a picked tray image into the user's icon theme, where every tray can find it."""

import hashlib
import os
import shutil
from pathlib import Path

# Icon themes only load these.
SUFFIXES = (".svg", ".png")
# Plasma, Cinnamon, COSMIC and Sway look the icon up by name, so it lives in hicolor.
FOLDERS = {".svg": "icons/hicolor/scalable/apps", ".png": "icons/hicolor/256x256/apps"}


def _data_home():
    return Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share")


def install(source, language, keep=(), data_home=None):
    """Copies source and returns the copy's path; raises ValueError for a file type trays cannot show.

    The name changes with the content, so trays that cache icons by name show a new image. Older
    copies for the language are removed, except those in keep (the saved setting).
    """
    suffix = Path(source).suffix.lower()
    if suffix not in SUFFIXES:
        raise ValueError(suffix)
    content = Path(source).read_bytes()
    name = f"ngosen-tray-{language}-{hashlib.sha256(content).hexdigest()[:12]}{suffix}"
    home = Path(data_home) if data_home else _data_home()
    target = home / FOLDERS[suffix] / name
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        shutil.copyfile(source, target)
    for folder in FOLDERS.values():
        for old in (home / folder).glob(f"ngosen-tray-{language}-*"):
            if old != target and str(old) not in keep:
                old.unlink()
    return str(target)
