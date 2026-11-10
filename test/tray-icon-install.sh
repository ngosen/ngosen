#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Every tray icon name the engine can ask for must be installed where trays look: hicolor for the
# path and name lookups, Breeze for Plasma, which takes the icon from its theme by name.
set -u
build=${1:?usage: tray-icon-install.sh <build dir>}
dest=$(mktemp -d)
trap 'rm -rf "$dest"' EXIT
DESTDIR=$dest cmake --install "$build" >/dev/null || exit 1
icons=$(find "$dest" -type d -path '*/share/icons' | head -1)
[ -n "$icons" ] || {
    echo "no icons installed" >&2
    exit 1
}
fail=0
need() {
    [ -f "$icons/$1" ] || {
        echo "not installed: $1" >&2
        fail=1
    }
}
for base in fcitx-ngosen fcitx-ngosen-off fcitx-ngosen-emoji; do
    for name in "$base" "$base-large" "$base-default" "$base-default-black"; do
        need "hicolor/scalable/status/$name.svg"
        need "hicolor/scalable/apps/$name.svg"
        need "hicolor/22x22/status/$name.png"
        need "hicolor/24x24/status/$name.png"
    done
    for name in "$base" "$base-default" "$base-default-black"; do
        need "hicolor/scalable/status/$name-gnome.svg"
    done
    for theme in breeze breeze-dark; do
        for size in 22 24; do
            need "$theme/status/$size/$base.svg"
            need "$theme/status/$size/$base-large.svg"
        done
    done
done
exit $fail
