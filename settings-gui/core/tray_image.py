# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copies a picked tray image into the user's icon theme, where every tray can find it."""

import base64
import hashlib
import os
import re
import struct
from pathlib import Path

# Icon themes only load these.
SUFFIXES = (".svg", ".png")
# Plasma, Cinnamon, COSMIC and Sway look the icon up by name, so it lives in hicolor.
FOLDERS = {".svg": "icons/hicolor/scalable/apps", ".png": "icons/hicolor/256x256/apps"}


def _number(value):
    match = re.fullmatch(r"\s*([0-9.]+)\s*(px)?\s*", value or "")
    return float(match.group(1)) if match else None


def _square_svg(content):
    """The SVG with its view widened to a centred square, or unchanged when its size is unknown."""
    text = content.decode("utf-8")
    root = re.search(r"<svg\b[^>]*>", text)
    if not root:
        return content
    tag = root.group(0)
    attrs = dict(re.findall(r'\s([\w:-]+)\s*=\s*["\']([^"\']*)["\']', tag))
    box = (
        [float(v) for v in re.split(r"[\s,]+", attrs["viewBox"].strip())]
        if "viewBox" in attrs
        else None
    )
    if not box:
        width, height = _number(attrs.get("width")), _number(attrs.get("height"))
        if not width or not height:
            return content
        box = [0.0, 0.0, width, height]
    x, y, width, height = box
    if width == height:
        return content
    side = max(width, height)
    view = f"{x - (side - width) / 2:g} {y - (side - height) / 2:g} {side:g} {side:g}"
    kept = re.sub(r'\s(viewBox|width|height)\s*=\s*["\'][^"\']*["\']', "", tag)
    new_tag = kept.replace("<svg", f'<svg viewBox="{view}" width="{side:g}" height="{side:g}"', 1)
    return (text[: root.start()] + new_tag + text[root.end() :]).encode("utf-8")


def _square_png(content):
    """An SVG holding the PNG centred in a square, or None when the PNG already is square."""
    if len(content) < 24 or content[12:16] != b"IHDR":
        return None
    width, height = struct.unpack(">II", content[16:24])
    if width == height:
        return None
    side = max(width, height)
    data = base64.b64encode(content).decode("ascii")
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
        f'viewBox="0 0 {side} {side}" width="{side}" height="{side}">'
        f'<image x="{(side - width) // 2}" y="{(side - height) // 2}" width="{width}" height="{height}" '
        f'xlink:href="data:image/png;base64,{data}"/></svg>'
    ).encode("ascii")


def _data_home():
    return Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share")


def install(source, language, keep=(), data_home=None):
    """Copies source and returns the copy's path; raises ValueError for a file type trays cannot show.

    The name changes with the content, so trays that cache icons by name show a new image. Older
    copies for the language are removed, except those in keep (the saved setting). The copy is
    square, since GNOME's AppIndicator tray leaves a wider image drawn behind later icons.
    """
    suffix = Path(source).suffix.lower()
    if suffix not in SUFFIXES:
        raise ValueError(suffix)
    content = Path(source).read_bytes()
    if suffix == ".svg":
        content = _square_svg(content)
    else:
        wrapped = _square_png(content)
        if wrapped:
            content, suffix = wrapped, ".svg"
    name = f"ngosen-tray-{language}-{hashlib.sha256(content).hexdigest()[:12]}{suffix}"
    home = Path(data_home) if data_home else _data_home()
    target = home / FOLDERS[suffix] / name
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        target.write_bytes(content)
    for folder in FOLDERS.values():
        for old in (home / folder).glob(f"ngosen-tray-{language}-*"):
            if old != target and str(old) not in keep:
                old.unlink()
    return str(target)
