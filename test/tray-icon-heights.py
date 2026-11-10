#!/usr/bin/env python3
"""Check the tray icon sizes: plain glyphs as tall as the tile, large tiles 1.25 times the
normal one, and no icon clipped.

usage: tray-icon-heights.py RSVG_CONVERT SOURCE_ICON_DIR GNOME_ICON_DIR LARGE_ICON_DIR
"""

import os
import struct
import subprocess
import sys
import tempfile
import zlib

SIZES = (16, 22, 24, 32)
TILES = ("fcitx-ngosen", "fcitx-ngosen-off", "fcitx-ngosen-emoji")
PLAIN = (
    "fcitx-ngosen-default",
    "fcitx-ngosen-default-black",
    "fcitx-ngosen-off-default",
    "fcitx-ngosen-off-default-black",
    "fcitx-ngosen-emoji-default",
    "fcitx-ngosen-emoji-default-black",
)


def read_alpha(png):
    """Return rows of alpha values from an 8-bit RGBA, non-interlaced PNG."""
    data = open(png, "rb").read()
    pos, idat, width, height = 8, b"", 0, 0
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos : pos + 8])
        body = data[pos + 8 : pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", body[:10])
            if depth != 8 or colour != 6 or body[12] != 0:
                sys.exit(f"{png}: expected 8-bit RGBA, not interlaced")
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    raw, stride, rows, prev = zlib.decompress(idat), width * 4, [], bytearray(width * 4)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1 : (y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if kind == 1:
                line[i] = (line[i] + a) & 255
            elif kind == 2:
                line[i] = (line[i] + b) & 255
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line[3::4])
        prev = line
    return rows


def glyph_rows(rsvg, svg, size, tmp):
    png = os.path.join(tmp, "icon.png")
    subprocess.run([rsvg, "-w", str(size), "-h", str(size), svg, "-o", png], check=True)
    # Pixels at half opacity or more count as part of the glyph; fainter ones are edge smoothing.
    return [[a >= 128 for a in row] for row in read_alpha(png)]


def height(rows):
    filled = [y for y, row in enumerate(rows) if any(row)]
    return filled[-1] - filled[0] + 1 if filled else 0


def middle(rows):
    filled = [y for y, row in enumerate(rows) if any(row)]
    return (filled[0] + filled[-1]) / 2 if filled else 0


def touches_edge(rows):
    return any(rows[0]) or any(rows[-1]) or any(row[0] or row[-1] for row in rows)


def main():
    rsvg, source_dir, gnome_dir, large_dir = sys.argv[1:5]
    sets = (
        (lambda n: os.path.join(source_dir, n + ".svg"), ""),
        (lambda n: os.path.join(gnome_dir, n + "-gnome.svg"), "-gnome"),
    )
    failures = []
    with tempfile.TemporaryDirectory() as tmp:
        for path, suffix in sets:
            for size in SIZES:
                tile_rows = glyph_rows(rsvg, path("fcitx-ngosen"), size, tmp)
                tile = height(tile_rows)
                for name in TILES + PLAIN:
                    rows = glyph_rows(rsvg, path(name), size, tmp)
                    if touches_edge(rows):
                        failures.append(f"{name}{suffix} at {size} px is clipped at the edge")
                    if name in PLAIN and abs(height(rows) - tile) > 1:
                        failures.append(
                            f"{name}{suffix} at {size} px: glyph {height(rows)} px, tile {tile} px"
                        )
                    if name in PLAIN and abs(middle(rows) - middle(tile_rows)) > 1:
                        failures.append(f"{name}{suffix} at {size} px is not centred like the tile")
        for size in SIZES:
            tile = height(glyph_rows(rsvg, os.path.join(source_dir, "fcitx-ngosen.svg"), size, tmp))
            for name in TILES:
                rows = glyph_rows(rsvg, os.path.join(large_dir, name + "-large.svg"), size, tmp)
                if touches_edge(rows):
                    failures.append(f"{name}-large at {size} px is clipped at the edge")
                if abs(height(rows) - tile * 1.25) > 1:
                    failures.append(
                        f"{name}-large at {size} px: tile {height(rows)} px, normal {tile} px"
                    )
    for line in failures:
        print(line)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
